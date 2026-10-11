#include "flowview.h"
#include "flowscene.h"

#include "../core/plugintypes.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QVarLengthArray>
#include <QWheelEvent>

#include <cmath>

using namespace OVP;

FlowView::FlowView(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new FlowScene(this))
{
    setScene(m_scene);
    applySettings();
}

FlowView::FlowView(FlowScene *scene, QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(scene)
{
    setScene(m_scene);
    applySettings();
}

FlowView::~FlowView() = default;

void FlowView::applySettings()
{
    setRenderHint(QPainter::Antialiasing);
    // 只重绘脏区：大流程图下 FullViewportUpdate 会让任何局部改动都触发整屏重绘
    setViewportUpdateMode(SmartViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setDragMode(RubberBandDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QColor(14, 19, 24));
}

void FlowView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, QColor(14, 19, 24));

    // Keep the drafting dots readable at every zoom level without drawing a
    // dense mesh when a large flow is fitted into the viewport.
    const qreal zoom = qMax(qAbs(painter->worldTransform().m11()), 0.001);
    qreal spacing = 24.0;
    while (spacing * zoom < 14.0)
        spacing *= 2.0;

    const qreal left = std::floor(rect.left() / spacing) * spacing;
    const qreal top = std::floor(rect.top() / spacing) * spacing;
    QVarLengthArray<QPointF, 1024> points;
    for (qreal x = left; x <= rect.right(); x += spacing) {
        for (qreal y = top; y <= rect.bottom(); y += spacing)
            points.append(QPointF(x, y));
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    QPen dotPen(QColor(48, 62, 73));
    dotPen.setCosmetic(true);
    dotPen.setWidthF(1.0);
    painter->setPen(dotPen);
    painter->drawPoints(points.constData(), points.size());
    painter->restore();
}

void FlowView::zoomIn()
{
    if (m_zoom < 3.0) {
        scale(1.15, 1.15);
        m_zoom *= 1.15;
    }
}

void FlowView::zoomOut()
{
    if (m_zoom > 0.2) {
        scale(1.0 / 1.15, 1.0 / 1.15);
        m_zoom /= 1.15;
    }
}

void FlowView::zoomFit()
{
    if (m_scene)
        fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

void FlowView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0)
        zoomIn();
    else
        zoomOut();
    event->accept();
}

void FlowView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(pluginIdMimeType()))
        event->acceptProposedAction();
    else
        event->ignore();
}

void FlowView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(pluginIdMimeType()))
        event->acceptProposedAction();
    else
        event->ignore();
}

void FlowView::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(pluginIdMimeType())) {
        const QString pluginId = QString::fromUtf8(event->mimeData()->data(pluginIdMimeType()));
        if (m_scene)
            m_scene->addPluginNode(pluginId, mapToScene(event->position().toPoint()));
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}
