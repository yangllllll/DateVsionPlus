#ifndef ROIGRAPHICS_H
#define ROIGRAPHICS_H

#include <QGraphicsObject>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QList>
#include <QRectF>

#include <opencv2/core.hpp>

/** 可拖拽 / 可缩放的 ROI 矩形 */
class RoiRectItem : public QGraphicsObject
{
    Q_OBJECT

public:
    static constexpr qreal HandleSize = 8.0;

    RoiRectItem(qreal x, qreal y, qreal w, qreal h, int index, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    /** 返回 ROI 的 (x, y, w, h) */
    QRect roiRect() const;
    int roiIndex() const { return m_index; }
    void setRoiIndex(int index);

    void setRectSize(qreal width, qreal height);
    QRectF rectSize() const { return m_rect; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QList<QRectF> handleRects() const;

    QRectF m_rect;
    int m_index = 0;
    int m_draggingHandle = -1; // -1 无, 0 左上, 1 右上, 2 左下, 3 右下
};

/** 带背景图像的场景，支持鼠标绘制 ROI */
class RoiImageScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit RoiImageScene(QObject *parent = nullptr);

    void setImage(const cv::Mat &image);
    /** 更新背景图像但保留已有 ROI */
    void updateBackground(const cv::Mat &image);

    QList<QRect> rois() const;
    void addRoi(const QRect &roi);
    void removeSelectedRoi();
    void clearRois();
    int roiCount() const { return m_roiItems.size(); }
    RoiRectItem *roiItemAt(int index) const;

signals:
    void roiChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void updateIndices();

    QList<RoiRectItem *> m_roiItems;
    QGraphicsPixmapItem *m_imageItem = nullptr;
    bool m_drawing = false;
    QPointF m_drawStart;
    RoiRectItem *m_drawItem = nullptr;
};

#endif // ROIGRAPHICS_H
