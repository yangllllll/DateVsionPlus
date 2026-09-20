#ifndef PLCDIALOG_H
#define PLCDIALOG_H

/**
 * @brief PLC 调试对话框（Python 版没有，此处额外提供）
 *
 * 用于排查通讯：查看连接状态、手动写 1 / 写 0、读回当前字节值。
 */

#include <QDialog>

class QLabel;

namespace OVP {
class PLCSendPlugin;
}

class PLCDialog : public QDialog
{
public:
    explicit PLCDialog(OVP::PLCSendPlugin *plugin, QWidget *parent = nullptr);

private:
    void refresh();
    void showResult(const QString &text, bool ok);
    bool ensureConnected();

    OVP::PLCSendPlugin *m_plugin = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_resultLabel = nullptr;
};

#endif // PLCDIALOG_H
