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
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ImageViewer::setImage(const cv::Mat &image)
{
    m_pixmap = image.empty() ? QPixmap() : OVP::matToQPixmap(image);
    m_fit = true;
    m_scaled = QPixmap();
    update();
}

void ImageViewer::clearImage()
{
    m_pixmap = QPixmap();
    m_scaled = QPixmap();
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

const QPixmap &ImageViewer::scaledPixmap()
{
    const bool stale = m_scaled.isNull() || m_scaledViewport != size()
                       || m_scaledFit != m_fit
                       || (!m_fit && qAbs(m_scaledZoom - m_zoom) > 1e-6);
    if (!stale)
        return m_scaled;

    if (m_fit) {
        m_scaled = m_pixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        m_scaled = m_pixmap.scaled(static_cast<int>(m_pixmap.width() * m_zoom),
                                   static_cast<int>(m_pixmap.height() * m_zoom),
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    m_scaledViewport = size();
    m_scaledFit = m_fit;
    m_scaledZoom = m_zoom;
    return m_scaled;
}

void ImageViewer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#101519")));

    if (m_pixmap.isNull()) {
        painter.setPen(QColor(QStringLiteral("#25313a")));
        for (int y = 16; y < height(); y += 24)
            for (int x = 16; x < width(); x += 24)
                painter.drawPoint(x, y);

        const QRect frame = rect().adjusted(18, 18, -19, -19);
        painter.setPen(QColor(QStringLiteral("#293740")));
        painter.drawRect(frame);
        painter.setPen(QPen(QColor(QStringLiteral("#7895a5")), 1.4));
        const int corner = 12;
        for (const QPoint &point : {frame.topLeft(), frame.topRight(), frame.bottomLeft(), frame.bottomRight()}) {
            const int dx = point.x() == frame.left() ? corner : -corner;
            const int dy = point.y() == frame.top() ? corner : -corner;
            painter.drawLine(point, point + QPoint(dx, 0));
            painter.drawLine(point, point + QPoint(0, dy));
        }

        const int cx = width() / 2;
        const int cy = height() / 2 - 20;
        const QRect imageRect(cx - 21, cy - 19, 42, 32);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(QStringLiteral("#7895a5")), 1.4));
        painter.drawRect(imageRect);
        painter.drawLine(cx - 14, cy + 6, cx - 3, cy - 5);
        painter.drawLine(cx - 3, cy - 5, cx + 4, cy + 2);
        painter.drawLine(cx + 4, cy + 2, cx + 10, cy - 3);
        painter.drawLine(cx + 10, cy - 3, cx + 16, cy + 6);
        painter.drawEllipse(QPointF(cx + 11, cy - 11), 2.0, 2.0);

        QFont titleFont = font();
        titleFont.setPixelSize(13);
        painter.setFont(titleFont);
        painter.setPen(QColor(QStringLiteral("#b7c9d3")));
        painter.drawText(QRect(20, cy + 27, width() - 40, 20), Qt::AlignCenter,
                         QStringLiteral("等待图像输入"));
        titleFont.setPixelSize(11);
        painter.setFont(titleFont);
        painter.setPen(QColor(QStringLiteral("#738691")));
        painter.drawText(QRect(20, cy + 50, width() - 40, 18), Qt::AlignCenter,
                         QStringLiteral("运行流程后显示节点图像"));
        return;
    }

    const QPixmap &scaled = scaledPixmap();
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
