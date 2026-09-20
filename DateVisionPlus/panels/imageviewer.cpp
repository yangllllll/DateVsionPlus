#include "imageviewer.h"

#include "../core/cvutils.h"

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>

ImageViewer::ImageViewer(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 150);
    setStyleSheet(QStringLiteral("background: #1e1e1e;"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ImageViewer::setImage(const cv::Mat &image)
{
    m_pixmap = image.empty() ? QPixmap() : OVP::matToQPixmap(image);
    m_fit = true;
    update();
}

void ImageViewer::clearImage()
{
    m_pixmap = QPixmap();
    update();
}

void ImageViewer::fitToWindow()
{
    m_fit = true;
    update();
}

void ImageViewer::zoomActualSize()
{
    m_fit = false;
    m_zoom = 1.0;
    update();
}

void ImageViewer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));

    if (m_pixmap.isNull()) {
        painter.setPen(QColor(100, 100, 100));
        painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 11));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("无图像"));
        return;
    }

    QPixmap scaled;
    if (m_fit) {
        scaled = m_pixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        scaled = m_pixmap.scaled(static_cast<int>(m_pixmap.width() * m_zoom),
                                 static_cast<int>(m_pixmap.height() * m_zoom),
                                 Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const int x = (width() - scaled.width()) / 2;
    const int y = (height() - scaled.height()) / 2;
    painter.drawPixmap(x, y, scaled);
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    if (m_pixmap.isNull())
        return;

    m_fit = false;
    if (event->angleDelta().y() > 0)
        m_zoom = qMin(10.0, m_zoom * 1.15);
    else
        m_zoom = qMax(0.05, m_zoom / 1.15);
    update();
}
