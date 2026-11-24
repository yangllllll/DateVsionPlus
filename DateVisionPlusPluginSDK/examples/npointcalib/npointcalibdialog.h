#ifndef NPOINTCALIBDIALOG_H
#define NPOINTCALIBDIALOG_H

#include "npointcalibplugin.h"
#include "detectionlist.h"

#include <QDialog>
#include <QPixmap>
#include <QWidget>

class QLabel;
class QTableWidget;

/** 标定点预览画布：图像 + 标定点（带序号标注） */
class CalibCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit CalibCanvas(QWidget *parent = nullptr);

    void setImage(const cv::Mat &image);
    /** labels 为空时按 1..N 标注；可与 points 一一对应 */
    void setPoints(const QList<CALIB::CalibPoint> &points,
                   const QList<int> &labels = QList<int>());
    void setDetections(const QList<QPointF> &centers);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRect imageRect() const;
    QPoint toWidget(const QPointF &imagePoint) const;

    QPixmap m_pixmap;
    QSize m_imageSize;
    QList<CALIB::CalibPoint> m_points;
    QList<int> m_labels;
    QList<QPointF> m_detections;
};

/**
 * @brief N 点标定对话框
 *
 * 左侧录入标定点（图像坐标可用 YOLO 中心填充，机器人坐标手动输入），
 * 右侧预览图像与标定点位置；支持计算、保存、加载 .ncal 标定文件。
 */
class NPointCalibDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NPointCalibDialog(OVP::NPointCalibPlugin *plugin, const cv::Mat &image,
                               QWidget *parent = nullptr);

private slots:
    void onAddRow();
    void onDeleteRow();
    void onClearRows();
    /** 用当前 YOLO 检测结果重建标定点（保留已填写的机器人坐标） */
    void onAutoFill();
    void onCompute();
    void onSaveFile();
    void onLoadFile();
    void onTableChanged();
    void onAccept();

private:
    void setupUi();
    /** 读取 detections；fillTable 为 true 时按检测结果重建行 */
    void refreshDetections(bool fillTable);
    void fillTable(const QList<CALIB::CalibPoint> &points);
    void renumberRows();
    bool collectPoints(QList<CALIB::CalibPoint> &points, QString *error) const;
    void updatePreview();
    void setStatus(const QString &text, bool isError);
    void setPointCell(int row, int column, double value);

    OVP::NPointCalibPlugin *m_plugin = nullptr;
    cv::Mat m_image;
    CALIB::CalibData m_calib;
    QList<DETECT::Item> m_detections;
    bool m_updatingTable = false;

    QTableWidget *m_table = nullptr;
    QLabel *m_resultLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    CalibCanvas *m_canvas = nullptr;
};

#endif // NPOINTCALIBDIALOG_H
