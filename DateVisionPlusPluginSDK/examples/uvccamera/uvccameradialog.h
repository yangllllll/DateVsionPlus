#ifndef UVCCAMERADIALOG_H
#define UVCCAMERADIALOG_H

#include "uvccameraplugin.h"

#include <QDialog>

class QCloseEvent;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;

/**
 * @brief UVC 摄像头设置对话框
 *
 * 打开摄像头后由后台线程持续取图，本对话框只负责预览；
 * 关闭对话框不会关闭摄像头，方便流程直接复用最新帧。
 */
class UvcCameraDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UvcCameraDialog(OVP::UvcCameraPlugin *plugin, QWidget *parent = nullptr);
    ~UvcCameraDialog() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onRefreshClicked();
    void onConnectClicked();
    void onOkClicked();
    void onPreviewTick();

private:
    void setupUi();
    void refreshCameraList();
    void setConnectedState(bool connected);
    void stopPreview();
    /** 把界面上的分辨率 / 帧率 / 后端写回插件参数 */
    void applyUiToParams();

    OVP::UvcCameraPlugin *m_plugin = nullptr;
    QComboBox *m_cameraCombo = nullptr;
    QComboBox *m_backendCombo = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QSpinBox *m_fpsSpin = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_previewTimer = nullptr;
    int m_missedFrames = 0;
};

#endif // UVCCAMERADIALOG_H
