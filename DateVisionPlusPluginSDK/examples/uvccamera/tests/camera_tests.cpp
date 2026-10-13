#include "uvccapture.h"
#include "dahuaimvsdk.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QtTest>

#include <atomic>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace UVC {

class UvcGrabberTest : public QObject
{
    Q_OBJECT
private slots:
    void snapshotsAreIndependent()
    {
        UvcGrabber grabber;
        cv::Mat output(1, 1, CV_8UC1, cv::Scalar(99));
        quint64 seq = 99;
        QVERIFY(!grabber.lastFrame(output, &seq));
        QCOMPARE(output.at<uchar>(0), uchar(99));
        QCOMPARE(seq, quint64(99));

        cv::Mat input(16, 32, CV_8UC1, cv::Scalar(7));
        const auto *sourceData = input.data;
        auto buffer = std::make_shared<cv::Mat>(std::move(input));
        grabber.publishFrame(buffer);
        QVERIFY(input.empty());
        QCOMPARE(grabber.m_latest->data, sourceData);
        QVERIFY(grabber.lastFrame(output, &seq));
        QCOMPARE(seq, quint64(1));
        output.setTo(99);

        cv::Mat unchanged;
        QVERIFY(grabber.lastFrame(unchanged));
        QCOMPARE(cv::countNonZero(unchanged != 7), 0);
        buffer = std::make_shared<cv::Mat>(16, 32, CV_8UC1, cv::Scalar(8));
        grabber.publishFrame(buffer);
        QCOMPARE(cv::countNonZero(unchanged != 7), 0);
        QVERIFY(grabber.waitFrame(output, 0, 1, nullptr, &seq));
        QCOMPARE(seq, quint64(2));
        QCOMPARE(cv::countNonZero(output != 8), 0);
        QVERIFY(!grabber.waitFrame(output, 5, seq));
    }

    void concurrentFrameAndSequenceStayMatched()
    {
        UvcGrabber grabber;
        std::atomic<bool> done{false};
        std::thread producer([&] {
            std::shared_ptr<cv::Mat> buffer;
            for (int i = 1; i <= 1500; ++i) {
                grabber.prepareFrame(buffer);
                buffer->create(120, 160, CV_32SC1);
                buffer->setTo(i);
                grabber.publishFrame(buffer);
            }
            done.store(true);
        });
        int reads = 0;
        bool valid = true;
        do {
            cv::Mat output;
            quint64 seq = 0;
            if (grabber.waitFrame(output, 100, 0, nullptr, &seq)) {
                valid = valid && cv::countNonZero(output != static_cast<int>(seq)) == 0;
                output.setTo(-1);
                ++reads;
            }
        } while (!done.load());
        producer.join();
        QVERIFY(valid);
        QVERIFY(reads > 0);
        cv::Mat output;
        quint64 seq;
        QVERIFY(grabber.lastFrame(output, &seq));
        QCOMPARE(seq, quint64(1500));
        QCOMPARE(cv::countNonZero(output != 1500), 0);
    }

    void slowReaderKeepsRetiredBufferImmutable()
    {
        UvcGrabber grabber;
        auto buffer = std::make_shared<cv::Mat>(240, 320, CV_8UC1, cv::Scalar(7));
        grabber.publishFrame(buffer);
        const std::shared_ptr<const cv::Mat> slowSnapshot = grabber.m_latest;
        buffer = std::make_shared<cv::Mat>(240, 320, CV_8UC1, cv::Scalar(8));
        grabber.publishFrame(buffer);
        const auto *retiredData = buffer->data;
        grabber.prepareFrame(buffer);
        QVERIFY(buffer->data != retiredData);

        std::atomic<bool> done{false};
        std::thread producer([&] {
            for (int i = 0; i < 1500; ++i) {
                grabber.prepareFrame(buffer);
                buffer->create(240, 320, CV_8UC1);
                buffer->setTo(i % 255);
                grabber.publishFrame(buffer);
            }
            done.store(true);
        });
        bool valid = true;
        do {
            const cv::Mat copy = slowSnapshot->clone();
            valid = valid && cv::countNonZero(copy != 7) == 0;
            QThread::msleep(1);
        } while (!done.load());
        producer.join();
        QVERIFY(valid);
        QCOMPARE(cv::countNonZero(*slowSnapshot != 7), 0);

        // 已退休且没有读者的缓冲区继续复用，不需要每帧重新分配。
        const auto *reusableData = buffer->data;
        grabber.prepareFrame(buffer);
        QCOMPARE(buffer->data, reusableData);
    }

    void stopWakesFrameWaiter()
    {
        UvcGrabber grabber;
        std::thread stopper([&] {
            QThread::msleep(20);
            QMutexLocker locker(&grabber.m_mutex);
            grabber.m_stop.store(true);
            grabber.m_cond.wakeAll();
        });
        QElapsedTimer elapsed;
        elapsed.start();
        cv::Mat output;
        const bool gotFrame = grabber.waitFrame(output, 1500);
        stopper.join();
        QVERIFY(!gotFrame);
        QVERIFY(elapsed.elapsed() < 1000);
    }

    void syntheticPublicationCost()
    {
        constexpr int iterations = 80;
        const cv::Mat decoded(1080, 1920, CV_8UC3, cv::Scalar(1, 2, 3));
        cv::Mat previousRead, previousCache;
        QMutex previousMutex;
        UvcGrabber grabber;
        std::shared_ptr<cv::Mat> buffer;
        const auto oldPath = [&] {
            QElapsedTimer elapsed;
            elapsed.start();
            for (int i = 0; i < iterations; ++i) {
                decoded.copyTo(previousRead);
                QMutexLocker locker(&previousMutex);
                previousRead.copyTo(previousCache);
            }
            return elapsed.nsecsElapsed();
        };
        const auto newPath = [&] {
            QElapsedTimer elapsed;
            elapsed.start();
            for (int i = 0; i < iterations; ++i) {
                grabber.prepareFrame(buffer);
                decoded.copyTo(*buffer);
                grabber.publishFrame(buffer);
            }
            return elapsed.nsecsElapsed();
        };
        oldPath();
        newPath();
        std::array<qint64, 7> oldTimes, newTimes;
        for (size_t i = 0; i < oldTimes.size(); ++i) {
            if (i % 2 == 0) {
                oldTimes[i] = oldPath();
                newTimes[i] = newPath();
            } else {
                newTimes[i] = newPath();
                oldTimes[i] = oldPath();
            }
        }
        std::sort(oldTimes.begin(), oldTimes.end());
        std::sort(newTimes.begin(), newTimes.end());
        qInfo().nospace() << "1080p synthetic read-buffer + publish, " << iterations
            << " frames (7 alternating rounds, median): old=" << oldTimes[3] / 1e6
            << " ms, new=" << newTimes[3] / 1e6
            << " ms. Includes capture-buffer preparation; excludes device/decode and consumer clone.";
        QVERIFY(cv::norm(previousCache, *grabber.m_latest, cv::NORM_INF) == 0);
    }
};

} // namespace UVC

namespace DahuaMV {
namespace {
std::vector<unsigned char> pixels;
IMV_FrameInfo frameInfo{};
int startCount, stopCount, releaseCount, getCount;
int startResult, getResult, convertResult;
bool throwOnConvert;
unsigned char *convertedInto;

int DAHUA_IMV_CALL fakeStart(void *) { ++startCount; return startResult; }
int DAHUA_IMV_CALL fakeStop(void *) { ++stopCount; return IMV_OK; }
int DAHUA_IMV_CALL fakeGet(void *, IMV_Frame *frame, unsigned int)
{
    ++getCount;
    frame->pData = pixels.data();
    frame->frameInfo = frameInfo;
    return getResult;
}
int DAHUA_IMV_CALL fakeRelease(void *, IMV_Frame *) { ++releaseCount; return IMV_OK; }
int DAHUA_IMV_CALL fakeConvert(void *, IMV_PixelConvertParam *param)
{
    if (throwOnConvert)
        throw std::runtime_error("synthetic conversion failure");
    convertedInto = param->pDstBuf;
    if (convertResult != IMV_OK)
        return convertResult;
    std::memcpy(param->pDstBuf, pixels.data(), param->nDstBufSize);
    return IMV_OK;
}
} // namespace

class DahuaCameraTest : public QObject
{
    Q_OBJECT
private slots:
    void init()
    {
        auto *api = ImvApi::instance();
        api->m_loaded = true;
        api->startGrabbing = fakeStart;
        api->stopGrabbing = fakeStop;
        api->getFrame = fakeGet;
        api->releaseFrame = fakeRelease;
        api->pixelConvert = fakeConvert;
        startCount = stopCount = releaseCount = getCount = 0;
        startResult = getResult = convertResult = IMV_OK;
        throwOnConvert = false;
        convertedInto = nullptr;
        frameInfo = {};
        frameInfo.width = 3;
        frameInfo.height = 2;
        frameInfo.pixelFormat = gvspPixelBGR8;
        pixels = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
        frameInfo.size = static_cast<unsigned int>(pixels.size());
    }

    void convertsDirectlyAndPreservesContinuousStream()
    {
        DahuaCamera camera;
        camera.m_handle = &camera;
        camera.m_open = camera.m_grabbing = true;
        cv::Mat output;
        QVERIFY(camera.grab(output, 1));
        QCOMPARE(output.data, convertedInto);
        QCOMPARE(output.rows, 2);
        QCOMPARE(output.cols, 3);
        QCOMPARE(std::memcmp(output.data, pixels.data(), pixels.size()), 0);
        QCOMPARE(releaseCount, 1);
        QCOMPARE(startCount, 0);
        QCOMPARE(stopCount, 0);
        QVERIFY(camera.isGrabbing());
        pixels.assign(pixels.size(), 0);
        QCOMPARE(output.at<cv::Vec3b>(0, 0)[0], uchar(1));
    }

    void temporaryStreamStopsOnSuccessFailureAndException()
    {
        DahuaCamera camera;
        camera.m_handle = &camera;
        camera.m_open = true;
        cv::Mat output;
        QVERIFY(camera.grab(output, 1));
        QCOMPARE(startCount, 1);
        QCOMPARE(stopCount, 1);
        QVERIFY(!camera.isGrabbing());
        convertResult = IMV_ERROR;
        const auto *previous = output.data;
        QVERIFY(!camera.grab(output, 1));
        QCOMPARE(output.data, previous);
        QCOMPARE(releaseCount, 2);
        QCOMPARE(stopCount, 2);
        throwOnConvert = true;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, camera.grab(output, 1));
        QCOMPARE(releaseCount, 3);
        QCOMPARE(stopCount, 3);
        QVERIFY(!camera.isGrabbing());
    }

    void failedStartDoesNotReadOrStop()
    {
        DahuaCamera camera;
        camera.m_open = true;
        startResult = IMV_ERROR;
        cv::Mat output;
        QVERIFY(!camera.grab(output, 1));
        QCOMPARE(startCount, 1);
        QCOMPARE(getCount, 0);
        QCOMPARE(stopCount, 0);
        QCOMPARE(releaseCount, 0);
    }

    void grayscaleRespectsPaddingAndOutputAliasing()
    {
        DahuaCamera camera;
        camera.m_open = camera.m_grabbing = true;
        frameInfo.pixelFormat = gvspPixelBayGB8;
        frameInfo.paddingX = 1;
        pixels = {1, 2, 3, 99, 4, 5, 6, 99};
        frameInfo.size = static_cast<unsigned int>(pixels.size());
        cv::Mat output(2, 3, CV_8UC1, cv::Scalar(0));
        const cv::Mat alias = output;
        QVERIFY(camera.getFrame(output, 1));
        QCOMPARE(cv::countNonZero(alias), 0);
        QCOMPARE(output.at<uchar>(1, 0), uchar(4));
        QCOMPARE(output.at<uchar>(1, 2), uchar(6));
        QCOMPARE(releaseCount, 1);
        frameInfo.size = 5;
        QVERIFY(!camera.getFrame(output, 1));
        QCOMPARE(releaseCount, 2);
        QCOMPARE(output.at<uchar>(1, 2), uchar(6));
    }

    void timeoutDoesNotReleaseAnUnacquiredFrame()
    {
        DahuaCamera camera;
        camera.m_open = camera.m_grabbing = true;
        getResult = IMV_TIMEOUT;
        cv::Mat output;
        QVERIFY(!camera.getFrame(output, 1));
        QCOMPARE(getCount, 3);
        QCOMPARE(releaseCount, 0);
    }
};
} // namespace DahuaMV

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    UVC::UvcGrabberTest uvcTests;
    DahuaMV::DahuaCameraTest dahuaTests;
    const QString suite = qEnvironmentVariable("CAMERA_TEST_SUITE");
    if (suite == QStringLiteral("uvc"))
        return QTest::qExec(&uvcTests, argc, argv);
    if (suite == QStringLiteral("dahua"))
        return QTest::qExec(&dahuaTests, argc, argv);
    return QTest::qExec(&uvcTests, argc, argv) | QTest::qExec(&dahuaTests, argc, argv);
}

#include "camera_tests.moc"
