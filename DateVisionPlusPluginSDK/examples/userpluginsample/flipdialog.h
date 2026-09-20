#ifndef FLIPDIALOG_H
#define FLIPDIALOG_H

#include <QDialog>
#include <QStringList>

namespace OVP {
class FlipPlugin;
}

namespace Ui {
class FlipDialog;
}

/** 图像翻转插件的专用对话框（对应 Python 版 FlipDialog） */
class FlipDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FlipDialog(OVP::FlipPlugin *plugin, QWidget *parent = nullptr);
    ~FlipDialog() override;

private slots:
    void onOkClicked();
    void onModeChanged(int value);

private:
    Ui::FlipDialog *ui = nullptr;
    OVP::FlipPlugin *m_plugin = nullptr;
    QStringList m_modes;
};

#endif // FLIPDIALOG_H
