#ifndef UVCCAPTURE_H
#define UVCCAPTURE_H

/**
 * @brief 标准 UVC 摄像头采集（OpenCV VideoCapture + 独立后台取图线程）
 *
 * 设计要点：
 *   1) UvcGrabber 继承 QThread，start 之后在线程里死循环 cap.read()，
 *      每拿到一帧就写入内部变量 m_latest（带帧序号 m_seq）。
 *   2) 外部（流程执行 / UI 预览）只做两件事：
 *        - lastFrame()：直接取 m_latest（零等待，最快）
 *        - waitFrame(afterSeq)：等一帧序号 > afterSeq 的新图，保证不过时
 *      因此执行节点时不再有「开流 / 关流」的开销，也不会拿到旧帧。
 *   3) 取图线程打开失败或连续取帧失败会记录错误，UI 与 execute() 都能读到。
 */

#include <QList>
#include <QMutex>
#include <QString>
#include <QtGlobal>
#include <QThread>
#include <QWaitCondition>

#include <atomic>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

namespace UVC {

/** 采集后端（对应 cv::VideoCaptureAPI） */
enum class Backend {
    Auto = 0,       //!< 按平台依次尝试：Windows MSMF->DSHOW->ANY，Linux V4L2->ANY
    MSMF,           //!< Windows Media Foundation
    DirectShow,     //!< Windows DirectShow
    V4L2,           //!< Linux Video4Linux2
    AVFoundation,   //!< macOS
    Any             //!< 交给 OpenCV 自动选择
};

/** 采集参数 */
struct CameraConfig
{
    int index = 0;        //!< 摄像头序号
    int width = 0;        //!< 0 = 使用摄像头默认分辨率
    int height = 0;
    double fps = 0.0;     //!< 0 = 使用摄像头默认帧率
    Backend backend = Backend::Auto;
};

/** 枚举到的摄像头信息 */
struct CameraInfo
{
    int index = -1;
    int width = 0;
    int height = 0;
    QString name;

    /** 下拉框显示文本，如 "[0] 摄像头 0 (1920x1080)" */
    QString displayName() const;
};

/**
 * @brief 后台连续取图线程
 *
 * 线程安全：m_latest / m_seq / m_error 等均由 m_mutex 保护，
 * 取帧线程写、UI 线程与流程线程读。
 */
class UvcGrabber : public QThread
{
public:
    explicit UvcGrabber(QObject *parent = nullptr);
    ~UvcGrabber() override;

    void setConfig(const CameraConfig &config);
    CameraConfig config() const;

    /** 启动取图线程；内部会等待相机真正打开（最长 5s），失败时 error 给出原因 */
    bool startGrab(QString *error = nullptr);
    /** 停止取图线程并等待其退出 */
    void stopGrab();
    bool isGrabbing() const;

    /** 最新一帧（拷贝出来）；一帧都没有时返回 false */
    bool lastFrame(cv::Mat &out, quint64 *seq = nullptr) const;

    /**
     * @brief 等待一帧序号大于 afterSeq 的新图
     * @param timeoutMs 最长等待时间；<= 0 表示不等待，立即返回当前帧
     */
    bool waitFrame(cv::Mat &out, int timeoutMs, quint64 afterSeq = 0, QString *error = nullptr);

    quint64 frameSeq() const;
    /** 取图线程实测帧率 */
    double measuredFps() const;
    int frameWidth() const;
    int frameHeight() const;

    /** 取图线程的最后一次错误（取帧失败 / 相机断开等） */
    QString error() const;

    /** 扫描可用摄像头；连续 2 个序号打不开就停止扫描 */
    static QList<CameraInfo> enumCameras(int maxIndex = 10, QString *error = nullptr);

protected:
    void run() override;

private:
    mutable QMutex m_mutex;
    QWaitCondition m_cond;

    CameraConfig m_config;
    cv::Mat m_latest;
    quint64 m_seq = 0;
    double m_fps = 0.0;
    int m_width = 0;
    int m_height = 0;
    bool m_started = false;
    QString m_error;

    std::atomic<bool> m_stop{false};
};

} // namespace UVC

#endif // UVCCAPTURE_H
