#include "roigraphics.h"

#include "../core/cvutils.h"

#include <QBrush>
#include <QCursor>
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPointF>

// ---------------------------------------------------------------- RoiRectItem

RoiRectItem::RoiRectItem(qreal x, qreal y, qreal w, qreal h, int index, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_rect(x, y, w, h)
    , m_index(index)
{
    setFlag(ItemIsMovable, true);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemSendsGeometryChanges, true);
    setZValue(10.0);
    setAcceptHoverEvents(true);
    setCursor(Qt::SizeAllCursor);
}

QRectF RoiRectItem::boundingRect() const
{
    return m_rect.adjusted(-HandleSize, -HandleSize, HandleSize, HandleSize);
}

void RoiRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);

    painter->setPen(QPen(QColor(0, 200, 255), 2.0));
    painter->setBrush(QBrush(QColor(0, 200, 255, 30)));
    painter->drawRect(m_rect);

    painter->setPen(QPen(QColor(0, 200, 255), 1.0));
    painter->setBrush(QBrush(QColor(0, 200, 255)));
    for (const QRectF &handle : handleRects())
        painter->drawRect(handle);

    painter->setPen(Qt::white);
    painter->setFont(QFont(QStringLiteral("Consolas"), 10, QFont::Bold));
    painter->drawText(m_rect.adjusted(4, 4, -4, -4), Qt::AlignLeft | Qt::AlignTop,
                      QString::number(m_index + 1));
}

QList<QRectF> RoiRectItem::handleRects() const
{
    const qreal hs = HandleSize;
    return {
        QRectF(m_rect.left() - hs / 2, m_rect.top() - hs / 2, hs, hs),
        QRectF(m_rect.right() - hs / 2, m_rect.top() - hs / 2, hs, hs),
        QRectF(m_rect.left() - hs / 2, m_rect.bottom() - hs / 2, hs, hs),
        QRectF(m_rect.right() - hs / 2, m_rect.bottom() - hs / 2, hs, hs)
    };
}

QRect RoiRectItem::roiRect() const
{
    const QPointF pos = this->pos();
    return QRect(static_cast<int>(pos.x() + m_rect.x()), static_cast<int>(pos.y() + m_rect.y()),
                 static_cast<int>(m_rect.width()), static_cast<int>(m_rect.height()));
}

void RoiRectItem::setRoiIndex(int index)
{
    m_index = index;
    update();
}

void RoiRectItem::setRectSize(qreal width, qreal height)
{
    prepareGeometryChange();
    m_rect.setWidth(width);
    m_rect.setHeight(height);
    update();
}

void RoiRectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QPointF pos = event->pos();
        const QList<QRectF> handles = handleRects();
        for (int i = 0; i < handles.size(); ++i) {
            if (handles.at(i).contains(pos)) {
                m_draggingHandle = i;
                setCursor((i == 0 || i == 3) ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
                event->accept();
                return;
            }
        }
        m_draggingHandle = -1;
        setCursor(Qt::SizeAllCursor);
    }
    QGraphicsObject::mousePressEvent(event);
}

void RoiRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_draggingHandle >= 0) {
        prepareGeometryChange();
        QRectF updated = m_rect;
        switch (m_draggingHandle) {
        case 0: updated.setTopLeft(event->pos()); break;
        case 1: updated.setTopRight(event->pos()); break;
        case 2: updated.setBottomLeft(event->pos()); break;
        case 3: updated.setBottomRight(event->pos()); break;
        default: break;
        }
        if (updated.width() >= 10 && updated.height() >= 10)
            m_rect = updated.normalized();
        update();
        event->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(event);
}

void RoiRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    m_draggingHandle = -1;
    setCursor(Qt::SizeAllCursor);
    QGraphicsObject::mouseReleaseEvent(event);
}

// ------------------------------------------------------------- RoiImageScene

RoiImageScene::RoiImageScene(QObject *parent)
    : QGraphicsScene(parent)
{
}

void RoiImageScene::setImage(const cv::Mat &image)
{
    clear();
    m_roiItems.clear();
    m_imageItem = nullptr;

    if (image.empty())
        return;

    const QPixmap pixmap = OVP::matToQPixmap(OVP::ensureBgr(image));
    m_imageItem = addPixmap(pixmap);
    m_imageItem->setZValue(-1.0);
    setSceneRect(0, 0, image.cols, image.rows);
}

void RoiImageScene::updateBackground(const cv::Mat &image)
{
    if (m_imageItem) {
        removeItem(m_imageItem);
        delete m_imageItem;
        m_imageItem = nullptr;
    }

    if (image.empty())
        return;

    const QPixmap pixmap = OVP::matToQPixmap(OVP::ensureBgr(image));
    m_imageItem = addPixmap(pixmap);
    m_imageItem->setZValue(-1.0);
}

QList<QRect> RoiImageScene::rois() const
{
    QList<QRect> result;
    for (RoiRectItem *item : m_roiItems)
        result.append(item->roiRect());
    return result;
}

void RoiImageScene::addRoi(const QRect &roi)
{
    auto *item = new RoiRectItem(0, 0, roi.width(), roi.height(), m_roiItems.size());
    item->setPos(roi.x(), roi.y());
    addItem(item);
    m_roiItems.append(item);
    emit roiChanged();
}

void RoiImageScene::removeSelectedRoi()
{
    bool changed = false;
    for (QGraphicsItem *item : selectedItems()) {
        auto *roiItem = dynamic_cast<RoiRectItem *>(item);
        if (!roiItem)
            continue;
        m_roiItems.removeAll(roiItem);
        removeItem(roiItem);
        delete roiItem;
        changed = true;
    }
    if (changed) {
        updateIndices();
        emit roiChanged();
    }
}

void RoiImageScene::clearRois()
{
    for (RoiRectItem *item : m_roiItems) {
        removeItem(item);
        delete item;
    }
    m_roiItems.clear();
    emit roiChanged();
}

RoiRectItem *RoiImageScene::roiItemAt(int index) const
{
    if (index < 0 || index >= m_roiItems.size())
        return nullptr;
    return m_roiItems.at(index);
}

void RoiImageScene::updateIndices()
{
    for (int i = 0; i < m_roiItems.size(); ++i)
        m_roiItems.at(i)->setRoiIndex(i);
}

void RoiImageScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
        if (!item || item == m_imageItem) {
            m_drawing = true;
            m_drawStart = event->scenePos();
            m_drawItem = new RoiRectItem(0, 0, 0, 0, m_roiItems.size());
            m_drawItem->setPos(m_drawStart);
            addItem(m_drawItem);
            event->accept();
            return;
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void RoiImageScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_drawing && m_drawItem) {
        const QPointF start = m_drawStart;
        const QPointF end = event->scenePos();
        const qreal x = qMin(start.x(), end.x());
        const qreal y = qMin(start.y(), end.y());
        const qreal w = qAbs(end.x() - start.x());
        const qreal h = qAbs(end.y() - start.y());
        m_drawItem->setPos(x, y);
        m_drawItem->setRectSize(w, h);
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void RoiImageScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_drawing && m_drawItem) {
        m_drawing = false;
        if (m_drawItem->rectSize().width() >= 10 && m_drawItem->rectSize().height() >= 10) {
            m_roiItems.append(m_drawItem);
            m_drawItem->setSelected(true);
            m_drawItem = nullptr;
            emit roiChanged();
        } else {
            removeItem(m_drawItem);
            delete m_drawItem;
            m_drawItem = nullptr;
        }
        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}
