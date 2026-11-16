#ifndef YOLODETECTDIALOG_H
#define YOLODETECTDIALOG_H

#include "yolodetectplugin.h"

#include <QDialog>
#include <QPixmap>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QSpinBox;

/**
 * @brief YOLO 识别参数对话框（对应 Python 版 YoloDialog）
 *
 * 左侧预览（上游图像 / 识别结果），右侧模型与参数设置，可「测试识别」。
 */
class YoloDetectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit YoloDetectDialog(OVP::YoloDetectPlugin *plugin, const cv::Mat &image,
                              QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onBrowseModel();
    void onBrowseNames();
    void onBrowseOrt();
    void onTest();
    void onSave();

private:
    void setupUi();
    void loadParams();
    void applyParams();
    void setStatus(const QString &text, bool isError);
    void updatePreview();

    OVP::YoloDetectPlugin *m_plugin = nullptr;
    cv::Mat m_inputImage;
    cv::Mat m_previewImage;
    QPixmap m_previewPixmap;

    QLabel *m_previewLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    QLineEdit *m_modelEdit = nullptr;
    QLineEdit *m_namesEdit = nullptr;
    QLineEdit *m_ortPathEdit = nullptr;
    QComboBox *m_deviceCombo = nullptr;
    QComboBox *m_backendCombo = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QSpinBox *m_imgszSpin = nullptr;
    QSpinBox *m_maxDetSpin = nullptr;
    QSpinBox *m_thicknessSpin = nullptr;
    QDoubleSpinBox *m_confSpin = nullptr;
    QDoubleSpinBox *m_iouSpin = nullptr;
    QLineEdit *m_classFilterEdit = nullptr;
    QCheckBox *m_halfCheck = nullptr;
    QCheckBox *m_emptyNgCheck = nullptr;
    QCheckBox *m_drawCheck = nullptr;
    QCheckBox *m_labelCheck = nullptr;
};

#endif // YOLODETECTDIALOG_H
