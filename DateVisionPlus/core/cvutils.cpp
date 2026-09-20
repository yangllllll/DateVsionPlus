#include "cvutils.h"

#include "opencvcompat.h"

namespace OVP {

QImage matToQImage(const cv::Mat &mat)
{
    if (mat.empty())
        return QImage();

    cv::Mat src = mat;
    if (src.depth() != CV_8U) {
        cv::Mat tmp;
        cv::normalize(src, tmp, 0, 255, cv::NORM_MINMAX);
        tmp.convertTo(src, CV_8U);
    }

    switch (src.channels()) {
    case 1: {
        QImage image(src.data, src.cols, src.rows, static_cast<int>(src.step),
                     QImage::Format_Grayscale8);
        return image.copy();
    }
    case 3: {
        // 直接用 BGR888 承载，省掉一次全图 cvtColor（Qt 会按需自行转换）
        QImage image(src.data, src.cols, src.rows, static_cast<int>(src.step),
                     QImage::Format_BGR888);
        return image.copy();
    }
    case 4: {
        cv::Mat rgba;
        cv::cvtColor(src, rgba, cv::COLOR_BGRA2RGBA);
        QImage image(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step),
                     QImage::Format_RGBA8888);
        return image.copy();
    }
    default:
        break;
    }
    return QImage();
}

QPixmap matToQPixmap(const cv::Mat &mat)
{
    return QPixmap::fromImage(matToQImage(mat));
}

cv::Scalar bgrColor(const QString &colorName)
{
    if (colorName == QStringLiteral("红色")) return cv::Scalar(0, 0, 255);
    if (colorName == QStringLiteral("蓝色")) return cv::Scalar(255, 0, 0);
    if (colorName == QStringLiteral("黄色")) return cv::Scalar(0, 255, 255);
    if (colorName == QStringLiteral("青色")) return cv::Scalar(255, 255, 0);
    if (colorName == QStringLiteral("白色")) return cv::Scalar(255, 255, 255);
    if (colorName == QStringLiteral("黑色")) return cv::Scalar(0, 0, 0);
    return cv::Scalar(0, 255, 0); // 绿色
}

QColor qtColor(const QString &colorName)
{
    if (colorName == QStringLiteral("红色")) return QColor(244, 67, 54);
    if (colorName == QStringLiteral("蓝色")) return QColor(33, 150, 243);
    if (colorName == QStringLiteral("黄色")) return QColor(255, 235, 59);
    if (colorName == QStringLiteral("青色")) return QColor(0, 188, 212);
    if (colorName == QStringLiteral("白色")) return QColor(255, 255, 255);
    if (colorName == QStringLiteral("黑色")) return QColor(0, 0, 0);
    return QColor(76, 175, 80); // 绿色
}

cv::Mat ensureGray(const cv::Mat &src)
{
    if (src.empty())
        return src;
    if (src.channels() == 1)
        return src;
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat ensureBgr(const cv::Mat &src)
{
    if (src.empty())
        return src;
    if (src.channels() == 3)
        return src.clone();
    if (src.channels() == 4) {
        cv::Mat bgr;
        cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
        return bgr;
    }
    cv::Mat bgr;
    cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
    return bgr;
}

int oddKernelSize(int size)
{
    if (size < 1)
        size = 1;
    if (size % 2 == 0)
        size += 1;
    return size;
}

} // namespace OVP
