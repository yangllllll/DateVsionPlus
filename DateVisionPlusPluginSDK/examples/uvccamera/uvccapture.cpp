#include "uvccapture.h"

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QtGlobal>

#include <memory>

namespace UVC {
namespace {

// 直接用数值，避免不同 OpenCV 版本枚举名缺失导致编译失败
constexpr int kApiAny = 0;
constexpr int kApiDSHOW = 700;
constexpr int kApiMSMF = 1400;
constexpr int kApiV4L2 = 200;
constexpr int kApiAVFoundation = 1200;

constexpr int kOpenWaitMs = 5000;          //!< startGrab 等待相机打开的最长时间
constexpr int kMaxConsecutiveFailures = 60; //!< 连续取帧失败次数，超过则认为相机已断开
constexpr int kRetrySleepMs = 5;

QList<int> backendCandidates(Backend backend)
{
    switch (backend) {
    case Backend::MSMF:         return {kApiMSMF};
    case Backend::DirectShow:   return {kApiDSHOW};
    case Backend::V4L2:         return {kApiV4L2};
    case Backend::AVFoundation: return {kApiAVFoundation};
    case Backend::Any:          return {kApiAny};
    case Backend::Auto:         break;
    }

#if defined(Q_OS_WIN)
    return {kApiMSMF, kApiDSHOW, kApiAny};
#elif defined(Q_OS_LINUX)
    return {kApiV4L2, kApiAny};
#elif defined(Q_OS_MACOS)
    return {kApiAVFoundation, kApiAny};
#else
    return {kApiAny};
#endif
}

} // namespace

// ------------------------------------------------------------------ CameraInfo

QString CameraInfo::displayName() const
{
    if (width > 0 && height > 0)
        return QStringLiteral("[%1] %2 (%3x%4)").arg(index).arg(name).arg(width).arg(height);
    return QStringLiteral("[%1] %2").arg(index).arg(name);
}

// ------------------------------------------------------------------- UvcGrabber

UvcGrabber::UvcGrabber(QObject *parent)
    : QThread(parent)
{
}

UvcGrabber::~UvcGrabber()
{
    stopGrab();
}

void UvcGrabber::setConfig(const CameraConfig &config)
{
    QMutexLocker locker(&m_mutex);
    m_config = config;
}

CameraConfig UvcGrabber::config() const
{
    QMutexLocker locker(&m_mutex);
    return m_config;
}

bool UvcGrabber::startGrab(QString *error)
{
    if (isRunning() && !m_stop.load())
        return true;

    if (isRunning())
        stopGrab();

    {
        QMutexLocker locker(&m_mutex);
        m_error.clear();
        m_started = false;
        m_seq = 0;
        m_fps = 0.0;
        m_latest = cv::Mat();
    }

    m_stop.store(false);
    start();

    // 等相机真正打开，让调用方能立刻拿到明确的错误信息
    QDeadlineTimer deadline(kOpenWaitMs);
    QMutexLocker locker(&m_mutex);
    while (!m_started) {
        if (!m_cond.wait(&m_mutex, deadline))
            break;
    }

    if (!m_error.isEmpty()) {
        if (error)
            *error = m_error;
        return false;
    }
    if (!m_started) {
        if (error)
            *error = QStringLiteral("打开摄像头超时");
        return false;
    }
    return true;
}

void UvcGrabber::stopGrab()
{
    if (!isRunning())
        return;

    m_stop.store(true);
    {
        QMutexLocker locker(&m_mutex);
        m_cond.wakeAll();
    }
    wait(5000);
}

bool UvcGrabber::isGrabbing() const
{
    return isRunning() && !m_stop.load();
}

bool UvcGrabber::lastFrame(cv::Mat &out, quint64 *seq) const
{
    QMutexLocker locker(&m_mutex);
    if (m_latest.empty())
        return false;

    out = m_latest.clone();
    if (seq)
        *seq = m_seq;
    return true;
}

bool UvcGrabber::waitFrame(cv::Mat &out, int timeoutMs, quint64 afterSeq, QString *error)
{
    QDeadlineTimer deadline(timeoutMs);
    QMutexLocker locker(&m_mutex);

    while (m_seq <= afterSeq && m_error.isEmpty()) {
        if (timeoutMs <= 0)
            break;
        if (!m_cond.wait(&m_mutex, deadline))
            break;
    }

    if (!m_error.isEmpty() && m_seq == 0) {
        if (error)
            *error = m_error;
        return false;
    }
    if (m_seq == 0) {
        if (error)
            *error = QStringLiteral("摄像头未输出图像");
        return false;
    }
    if (m_seq <= afterSeq) {
        if (error)
            *error = QStringLiteral("等待新帧超时（%1 ms）").arg(timeoutMs);
        return false;
    }

    out = m_latest.clone();
    return true;
}

quint64 UvcGrabber::frameSeq() const
{
    QMutexLocker locker(&m_mutex);
    return m_seq;
}

double UvcGrabber::measuredFps() const
{
    QMutexLocker locker(&m_mutex);
    return m_fps;
}

int UvcGrabber::frameWidth() const
{
    QMutexLocker locker(&m_mutex);
    return m_width;
}

int UvcGrabber::frameHeight() const
{
    QMutexLocker locker(&m_mutex);
    return m_height;
}

QString UvcGrabber::error() const
{
    QMutexLocker locker(&m_mutex);
    return m_error;
}

QList<CameraInfo> UvcGrabber::enumCameras(int maxIndex, QString *error)
{
    QList<CameraInfo> result;
    int consecutiveMiss = 0;

    for (int index = 0; index < maxIndex; ++index) {
        bool found = false;
        for (int api : backendCandidates(Backend::Auto)) {
            cv::VideoCapture cap(index, api);
            if (!cap.isOpened())
                continue;

            cv::Mat probe;
            cap.read(probe); // 部分设备必须读一帧才能拿到真实分辨率

            CameraInfo info;
            info.index = index;
            info.width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
            info.height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
            info.name = QStringLiteral("摄像头 %1").arg(index);
            result.append(info);
            found = true;
            break;
        }

        consecutiveMiss = found ? 0 : consecutiveMiss + 1;
        if (consecutiveMiss >= 2)
            break;
    }

    if (result.isEmpty() && error)
        *error = QStringLiteral("未检测到可用摄像头");

    return result;
}

void UvcGrabber::run()
{
    const CameraConfig cfg = config();

    auto cap = std::make_unique<cv::VideoCapture>();
    bool opened = false;
    for (int api : backendCandidates(cfg.backend)) {
        if (cap->open(cfg.index, api)) {
            opened = true;
            break;
        }
    }

    if (!opened) {
        QMutexLocker locker(&m_mutex);
        m_error = QStringLiteral("无法打开摄像头 %1").arg(cfg.index);
        m_started = true;
        m_cond.wakeAll();
        return;
    }

    if (cfg.width > 0)
        cap->set(cv::CAP_PROP_FRAME_WIDTH, cfg.width);
    if (cfg.height > 0)
        cap->set(cv::CAP_PROP_FRAME_HEIGHT, cfg.height);
    if (cfg.fps > 0)
        cap->set(cv::CAP_PROP_FPS, cfg.fps);

    {
        QMutexLocker locker(&m_mutex);
        m_started = true;
        m_error.clear();
        m_cond.wakeAll();
    }

    cv::Mat frame;
    int failures = 0;
    quint64 counted = 0;
    QElapsedTimer fpsTimer;
    fpsTimer.start();

    while (!m_stop.load(std::memory_order_relaxed)) {
        if (!cap->read(frame) || frame.empty()) {
            if (++failures >= kMaxConsecutiveFailures) {
                QMutexLocker locker(&m_mutex);
                m_error = QStringLiteral("连续取帧失败，摄像头可能已断开");
                m_cond.wakeAll();
                break;
            }
            msleep(kRetrySleepMs);
            continue;
        }
        failures = 0;

        {
            QMutexLocker locker(&m_mutex);
            frame.copyTo(m_latest);
            ++m_seq;
            m_width = frame.cols;
            m_height = frame.rows;
            m_cond.wakeAll();
        }

        if (++counted >= 30) {
            const double elapsed = fpsTimer.elapsed() / 1000.0;
            if (elapsed > 0.0) {
                QMutexLocker locker(&m_mutex);
                m_fps = counted / elapsed;
            }
            counted = 0;
            fpsTimer.restart();
        }
    }

    cap->release();
}

} // namespace UVC
