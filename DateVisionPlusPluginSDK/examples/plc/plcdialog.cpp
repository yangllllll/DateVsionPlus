#include "plcdialog.h"

#include "plcplugin.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

PLCDialog::PLCDialog(OVP::PLCSendPlugin *plugin, QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
{
    setWindowTitle(QStringLiteral("PLC 传值 - 调试"));
    resize(360, 200);

    auto *layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *writeLayout = new QHBoxLayout();
    auto *writeOne = new QPushButton(QStringLiteral("写 1"), this);
    auto *writeZero = new QPushButton(QStringLiteral("写 0"), this);
    writeLayout->addWidget(writeOne);
    writeLayout->addWidget(writeZero);
    layout->addLayout(writeLayout);

    auto *otherLayout = new QHBoxLayout();
    auto *connectButton = new QPushButton(QStringLiteral("连接"), this);
    auto *disconnectButton = new QPushButton(QStringLiteral("断开"), this);
    auto *readButton = new QPushButton(QStringLiteral("读取当前值"), this);
    otherLayout->addWidget(connectButton);
    otherLayout->addWidget(disconnectButton);
    otherLayout->addWidget(readButton);
    layout->addLayout(otherLayout);

    m_resultLabel = new QLabel(QStringLiteral("就绪"), this);
    m_resultLabel->setWordWrap(true);
    layout->addWidget(m_resultLabel);

    auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);
    layout->addWidget(closeButton);

    connect(connectButton, &QPushButton::clicked, this, [this]() {
        QString error;
        if (m_plugin->client()->connectTo(m_plugin->ip(), m_plugin->rack(), m_plugin->slot(), &error))
            showResult(QStringLiteral("已连接到 %1").arg(m_plugin->ip()), true);
        else
            showResult(error, false);
        refresh();
    });

    connect(disconnectButton, &QPushButton::clicked, this, [this]() {
        m_plugin->client()->disconnect();
        showResult(QStringLiteral("已断开"), true);
        refresh();
    });

    connect(writeOne, &QPushButton::clicked, this, [this]() {
        if (!ensureConnected())
            return;
        QString error;
        if (m_plugin->client()->writeByte(m_plugin->areaCode(), m_plugin->dbNumber(),
                                          m_plugin->startByte(), 1, &error))
            showResult(QStringLiteral("写入 1 成功"), true);
        else
            showResult(error, false);
    });

    connect(writeZero, &QPushButton::clicked, this, [this]() {
        if (!ensureConnected())
            return;
        QString error;
        if (m_plugin->client()->writeByte(m_plugin->areaCode(), m_plugin->dbNumber(),
                                          m_plugin->startByte(), 0, &error))
            showResult(QStringLiteral("写入 0 成功"), true);
        else
            showResult(error, false);
    });

    connect(readButton, &QPushButton::clicked, this, [this]() {
        if (!ensureConnected())
            return;
        quint8 value = 0;
        QString error;
        if (m_plugin->client()->readByte(m_plugin->areaCode(), m_plugin->dbNumber(),
                                         m_plugin->startByte(), &value, &error))
            showResult(QStringLiteral("当前值 = %1").arg(value), true);
        else
            showResult(error, false);
    });

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    refresh();
}

void PLCDialog::refresh()
{
    const bool connected = m_plugin->client()->isConnected();
    m_statusLabel->setText(QStringLiteral("PLC 地址: %1 (rack=%2, slot=%3)\n区域: %4  DB号: %5  "
                                          "字节地址: %6\n状态: %7")
                               .arg(m_plugin->ip())
                               .arg(m_plugin->rack())
                               .arg(m_plugin->slot())
                               .arg(m_plugin->areaCode(), 2, 16, QLatin1Char('0'))
                               .arg(m_plugin->dbNumber())
                               .arg(m_plugin->startByte())
                               .arg(connected ? QStringLiteral("已连接") : QStringLiteral("未连接")));
}

void PLCDialog::showResult(const QString &text, bool ok)
{
    m_resultLabel->setText(text);
    m_resultLabel->setStyleSheet(ok ? QStringLiteral("color: #2e7d32;")
                                    : QStringLiteral("color: #c62828;"));
}

bool PLCDialog::ensureConnected()
{
    if (m_plugin->client()->isConnected())
        return true;

    QString error;
    if (m_plugin->client()->connectTo(m_plugin->ip(), m_plugin->rack(), m_plugin->slot(), &error)) {
        refresh();
        return true;
    }

    showResult(error, false);
    return false;
}
