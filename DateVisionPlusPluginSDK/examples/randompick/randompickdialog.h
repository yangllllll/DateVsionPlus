#ifndef RANDOM_PICKDIALOG_H
#define RANDOM_PICKDIALOG_H

#include "randompickplugin.h"

#include <QDialog>
#include <QPixmap>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;

/**
 * @brief 无序抓取设置对话框
 *
 * 选择 N 点标定文件、抓取序号与过滤条件，可用上游图像 + 已收到的 YOLO 结果做一次转换测试。
 */
class RandomPickDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RandomPickDialog(OVP::RandomPickPlugin *plugin, const cv::Mat &image,
                              QWidget *parent = nullptr);

private slots:
    void onBrowseCalib();
    void onTest();
    void onAccept();

private:
    void setupUi();
    void loadParams();
    void applyParams();
    void setStatus(const QString &text, bool isError);
    void showImage(const cv::Mat &image);

    OVP::RandomPickPlugin *m_plugin = nullptr;
    cv::Mat m_image;

    QLineEdit *m_calibEdit = nullptr;
    QSpinBox *m_indexSpin = nullptr;
    QDoubleSpinBox *m_scoreSpin = nullptr;
    QLineEdit *m_classFilterEdit = nullptr;
    QCheckBox *m_drawCheck = nullptr;
    QSpinBox *m_thicknessSpin = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_previewLabel = nullptr;
};

#endif // RANDOM_PICKDIALOG_H
