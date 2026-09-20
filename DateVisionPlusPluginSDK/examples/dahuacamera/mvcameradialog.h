#ifndef MVCAMERADIALOG_H
#define MVCAMERADIALOG_H

#include "mvcameraplugin.h"

#include <QDialog>

class QCloseEvent;
class QComboBox;
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
    void setConnectedState(bool connected);
    void stopPreview();

    OVP::MVCameraPlugin *m_plugin = nullptr;
    QComboBox *m_cameraCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_previewTimer = nullptr;
    int m_consecutiveFailures = 0;
};

#endif // MVCAMERADIALOG_H
