#ifndef FLOWVIEW_H
#define FLOWVIEW_H

#include <QGraphicsView>

namespace OVP {
class FlowScene;
}

/**
 * @brief 流程图视图：支持缩放与工具箱拖拽放置
 *
 * 注意：本类会被 Qt Designer 的 .ui 文件"提升"使用，uic 生成的代码不带命名空间限定，
 * 因此 FlowView 必须位于全局命名空间（内部持有的 FlowScene 仍在 OVP 命名空间）。
 */
class FlowView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit FlowView(QWidget *parent = nullptr);
    explicit FlowView(OVP::FlowScene *scene, QWidget *parent = nullptr);
    ~FlowView() override;

    OVP::FlowScene *flowScene() const { return m_scene; }

    void zoomIn();
    void zoomOut();
    void zoomFit();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void applySettings();

    OVP::FlowScene *m_scene = nullptr;
    qreal m_zoom = 1.0;
};

#endif // FLOWVIEW_H
