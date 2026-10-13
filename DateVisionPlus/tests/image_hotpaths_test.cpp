#include "core/cvutils.h"
#include "core/opencvcompat.h"
#include "plugins/blobanalysisplugin.h"
#include "plugins/measureplugin.h"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// The previous conversion is retained as a pixel and timing baseline.
QImage referenceImage(const cv::Mat &input)
{
    cv::Mat src = input;
    if (src.depth() != CV_8U) {
        cv::Mat normalized;
        cv::normalize(src, normalized, 0, 255, cv::NORM_MINMAX);
        normalized.convertTo(src, CV_8U);
    }
    if (src.channels() == 4) {
        cv::Mat rgba;
        cv::cvtColor(src, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step),
                      QImage::Format_RGBA8888).copy();
    }
    return QImage(src.data, src.cols, src.rows, static_cast<int>(src.step),
                  src.channels() == 1 ? QImage::Format_Grayscale8 : QImage::Format_BGR888).copy();
}

void testImageConversion()
{
    require(OVP::matToQImage({}).isNull(), "empty Mat must produce a null image");
    const int types[] = {CV_8UC1, CV_8UC3, CV_8UC4, CV_16UC4, CV_32FC4};
    cv::RNG random(12345);
    for (int type : types) {
        for (bool useRoi : {false, true}) {
            cv::Mat allocation(37, 67, type);
            random.fill(allocation, cv::RNG::UNIFORM, 0, 240);
            cv::Mat src = useRoi ? allocation(cv::Rect(3, 2, 53, 31)) : allocation;
            const cv::Mat before = src.clone();
            const QImage expected = referenceImage(src);
            QImage actual = OVP::matToQImage(src);
            require(actual == expected, "image pixels, alpha or ROI stride changed");
            require(cv::norm(src, before, cv::NORM_INF) == 0, "conversion modified its input");
            src.setTo(cv::Scalar::all(0));
            src.release();
            allocation.release();
            require(actual == expected, "QImage did not retain independent pixel ownership");
            QImage shared = actual;
            actual.bits()[0] ^= 0xff;
            require(shared == expected, "QImage implicit sharing was broken");
        }
    }

    // External memory and an unaligned row stride must also be supported.
    std::vector<uchar> pixels(19 * 87, 127);
    cv::Mat external(19, 21, CV_8UC4, pixels.data(), 87);
    const QImage expected = referenceImage(external);
    const QImage actual = OVP::matToQImage(external);
    std::fill(pixels.begin(), pixels.end(), 0);
    require(actual == expected, "external buffer lifetime or stride changed");
}

// Preserve the original blob path to verify exact contours and drawing pixels.
cv::Mat referenceBlob(const cv::Mat &input, bool fill, int *count)
{
    cv::Mat gray = OVP::ensureGray(input).clone();
    if (gray.depth() != CV_8U)
        gray.convertTo(gray, CV_8U);
    double minimum = 0;
    double maximum = 0;
    cv::minMaxLoc(gray, &minimum, &maximum);
    if (maximum > minimum)
        cv::threshold(gray, gray, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(gray, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    cv::Mat result = OVP::ensureBgr(input);
    *count = 0;
    for (const auto &contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < 100 || area > 100000)
            continue;
        ++*count;
        cv::drawContours(result, std::vector<std::vector<cv::Point>>{contour}, -1,
                         cv::Scalar(0, 255, 0), fill ? -1 : 2);
        const cv::Moments moments = cv::moments(contour);
        if (std::abs(moments.m00) > 1e-9) {
            cv::circle(result,
                       cv::Point(static_cast<int>(moments.m10 / moments.m00),
                                 static_cast<int>(moments.m01 / moments.m00)),
                       3, cv::Scalar(0, 0, 255), -1);
        }
    }
    return result;
}

cv::Mat fixture()
{
    cv::Mat image(320, 480, CV_8UC1, cv::Scalar(37));
    cv::circle(image, cv::Point(80, 110), 37, cv::Scalar(210), -1);
    cv::rectangle(image, cv::Rect(230, 40, 85, 70), cv::Scalar(180), -1);
    cv::circle(image, cv::Point(350, 225), 45, cv::Scalar(230), -1);
    cv::circle(image, cv::Point(350, 225), 20, cv::Scalar(37), -1);
    cv::circle(image, cv::Point(10, 10), 2, cv::Scalar(255), -1);
    return image;
}

void testBlob()
{
    std::vector<cv::Mat> images;
    images.push_back(fixture());
    images.push_back(OVP::ensureBgr(images.front()));
    cv::Mat highDepth;
    images.front().convertTo(highDepth, CV_16U);
    images.push_back(highDepth);
    images.push_back(cv::Mat(320, 480, CV_8UC1, cv::Scalar(73)));
    images.push_back(cv::Mat::zeros(320, 480, CV_8UC1));
    images.push_back(images.front()(cv::Rect(13, 9, 421, 281)));
    for (const cv::Mat &input : images) {
        for (bool fill : {false, true}) {
            const cv::Mat before = input.clone();
            int expectedCount = 0;
            const cv::Mat expected = referenceBlob(input, fill, &expectedCount);
            OVP::BlobAnalysisPlugin plugin;
            plugin.setParam(QStringLiteral("fill_contours"), fill);
            plugin.setInput(QStringLiteral("input"), OVP::imageValue(input));
            require(plugin.execute(), "blob execution failed");
            require(plugin.output(QStringLiteral("blob_count")).toInt() == expectedCount,
                    "blob count changed");
            const cv::Mat actual = plugin.output(QStringLiteral("output")).value<cv::Mat>();
            require(cv::norm(actual, expected, cv::NORM_INF) == 0, "blob drawing pixels changed");
            require(cv::norm(input, before, cv::NORM_INF) == 0, "blob modified upstream pixels");
        }
    }
}

void testMeasureInputIsolation()
{
    const cv::Mat source = fixture();
    for (const cv::Mat &input : {source, source(cv::Rect(13, 9, 421, 281)),
                                cv::Mat(320, 480, CV_8UC1, cv::Scalar(73))}) {
        const cv::Mat before = input.clone();
        OVP::MeasurePlugin plugin;
        plugin.setInput(QStringLiteral("input"), OVP::imageValue(input));
        require(plugin.execute(), "measure execution failed");
        require(cv::norm(input, before, cv::NORM_INF) == 0,
                "measure thresholding modified a shared upstream image");
        require(plugin.execute(), "repeated measure execution failed");
        require(cv::norm(input, before, cv::NORM_INF) == 0,
                "repeated measure execution modified upstream pixels");
    }
}

void benchmarkConversion()
{
    cv::setNumThreads(1);
    cv::Mat frame(2160, 3840, CV_8UC4);
    cv::randu(frame, 0, 255);
    std::vector<double> original;
    std::vector<double> optimized;
    volatile unsigned checksum = 0;
    auto measure = [&](bool reference) {
        QElapsedTimer timer;
        timer.start();
        const QImage result = reference ? referenceImage(frame) : OVP::matToQImage(frame);
        const double milliseconds = timer.nsecsElapsed() / 1000000.0;
        checksum += result.constBits()[0];
        return milliseconds;
    };
    for (int i = 0; i < 36; ++i) {
        // Alternate order and omit warm-up so allocator/cache effects are shared.
        const double first = measure(i % 2 == 0);
        const double second = measure(i % 2 != 0);
        if (i >= 5) {
            original.push_back(i % 2 == 0 ? first : second);
            optimized.push_back(i % 2 == 0 ? second : first);
        }
    }
    std::sort(original.begin(), original.end());
    std::sort(optimized.begin(), optimized.end());
    std::cout << "4K BGRA conversion median (31 samples, one OpenCV thread): before="
              << original[original.size() / 2] << " ms, after="
              << optimized[optimized.size() / 2] << " ms, checksum=" << checksum << '\n';
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        testImageConversion();
        testBlob();
        testMeasureInputIsolation();
        std::cout << "PASS: image conversion, blob equivalence, and upstream input isolation\n";
        if (app.arguments().contains(QStringLiteral("--benchmark")))
            benchmarkConversion();
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
