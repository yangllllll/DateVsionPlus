#ifndef MVCAMERADIALOG_H
#define MVCAMERADIALOG_H

#include "mvcameraplugin.h"

#include <QDialog>

class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTimer;

/**
 * @brief 大华相机设置对话框（对应 Python 版 MVCameraDialog）
 *
 * 相机枚举 + 连接/断开 + 实时预览，构造函数签名与主程序约定一致。
 */
class MVCameraDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MVCameraDialog(OVP::MVCameraPlugin *plugin, QWidget *parent = nullptr);
    ~MVCameraDialog() override;

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
    /** 已连接时不枚举相机，直接显示当前相机 */
    void populateCurrentCamera();
    void setConnectedState(bool connected);
    void stopPreview();

    OVP::MVCameraPlugin *m_plugin = nullptr;
    QComboBox *m_cameraCombo = nullptr;
    QDoubleSpinBox *m_fpsSpin = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_previewTimer = nullptr;
    int m_missedFrames = 0;
    /** 已显示的帧序号，避免重复转换同一帧 */
    quint64 m_lastShownSeq = 0;
};

#endif // MVCAMERADIALOG_H
