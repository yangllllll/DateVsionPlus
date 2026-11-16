#ifndef MATERIALZONEDIALOG_H
#define MATERIALZONEDIALOG_H

#include "materialzoneplugin.h"

#include <QDialog>
#include <QPixmap>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

/**
 * @brief 检测区绘制画布
 *
 * 左键拖拽新建检测区，单击选中，Delete 删除。
 * 区域坐标以归一化 (0~1) 保存，图像分辨率变化也能复用。
 */
class ZoneCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit ZoneCanvas(QWidget *parent = nullptr);

    void setImage(const cv::Mat &image);
    void setZones(const QList<OVP::Zone> &zones);
    QList<OVP::Zone> zones() const { return m_zones; }

    void setSelectedId(int id);
    int selectedId() const { return m_selectedId; }
    bool hasImage() const { return !m_pixmap.isNull(); }

signals:
    void zonesChanged();
    void selectionChanged(int id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRect imageRect() const;
    QRect toWidgetRect(const cv::Rect2f &zone) const;
    cv::Rect2f toZoneRect(const QRect &widgetRect) const;
    int zoneAt(const QPoint &pos) const;
    int nextZoneId() const;

    cv::Mat m_image;
    QPixmap m_pixmap;
    QList<OVP::Zone> m_zones;
    int m_selectedId = -1;

    bool m_pressed = false;
    bool m_dragging = false;
    QPoint m_start;
    QRect m_rubberBand;
};

/**
 * @brief 物料识别（检测区）设置对话框
 *
 * 左侧：检测区列表 + 参数；右侧：图像画布（拖拽绘制 ROI）。
 */
class MaterialZoneDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MaterialZoneDialog(OVP::MaterialZonePlugin *plugin, const cv::Mat &image,
                                QWidget *parent = nullptr);

private slots:
    void onZonesChanged();
    void onSelectionChanged(int id);
    void onTableSelectionChanged();
    void onDeleteSelected();
    void onClearZones();
    void onAccept();

private:
    void setupUi();
    void loadParams();
    void applyParams();
    void refreshTable();
    void selectRowById(int id);

    OVP::MaterialZonePlugin *m_plugin = nullptr;
    cv::Mat m_image;

    ZoneCanvas *m_canvas = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QDoubleSpinBox *m_scoreSpin = nullptr;
    QLineEdit *m_classFilterEdit = nullptr;
    QCheckBox *m_drawCheck = nullptr;
    QCheckBox *m_emptyNgCheck = nullptr;
    QSpinBox *m_thicknessSpin = nullptr;
    bool m_updatingTable = false;
};

#endif // MATERIALZONEDIALOG_H
