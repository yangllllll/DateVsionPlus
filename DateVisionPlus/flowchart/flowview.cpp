#include "flowview.h"
#include "flowscene.h"

#include "../core/plugintypes.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScrollBar>
#include <QWheelEvent>

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
