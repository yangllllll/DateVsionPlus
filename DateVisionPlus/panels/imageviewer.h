#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QPixmap>
#include <QSize>
#include <QWidget>

#include <opencv2/core.hpp>

/** 可缩放的图像查看器 */
class ImageViewer : public QWidget
{
    Q_OBJECT

public:
    explicit ImageViewer(QWidget *parent = nullptr);

    void setImage(const cv::Mat &image);
    void clearImage();
    bool hasImage() const { return !m_pixmap.isNull(); }

public slots:
    void fitToWindow();
    void zoomActualSize();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    /** 缩放结果按 (视口尺寸 + 缩放参数) 缓存，避免每次重绘都做一次 SmoothTransformation */
    const QPixmap &scaledPixmap();

    QPixmap m_pixmap;
    qreal m_zoom = 1.0;
    bool m_fit = true;

    QPixmap m_scaled;
    QSize m_scaledViewport;
    qreal m_scaledZoom = -1.0;
    bool m_scaledFit = false;
};

#endif // IMAGEVIEWER_H
