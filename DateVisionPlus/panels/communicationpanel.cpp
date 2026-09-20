#include "communicationpanel.h"
#include "ui_communicationpanel.h"

#include "../communication/tcpserver.h"

#include <QColor>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QTextCursor>

namespace {

QString localIpAddress()
{
    for (const QHostAddress &address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol
            && address != QHostAddress::LocalHost) {
            return address.toString();
        }
    }
    return QStringLiteral("127.0.0.1");
}

} // namespace

CommunicationPanel::CommunicationPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommunicationPanel)
{
    ui->setupUi(this);

    m_server = new TcpServer(this);
    ui->lblLocalIp->setText(localIpAddress());

    connect(ui->btnStart, &QPushButton::clicked, this, &CommunicationPanel::onStartClicked);
    connect(ui->btnStop, &QPushButton::clicked, this, &CommunicationPanel::onStopClicked);
    connect(ui->comboDelimiter, &QComboBox::currentTextChanged, this,
            &CommunicationPanel::onDelimiterChanged);

    connect(m_server, &TcpServer::logMessage, this, &CommunicationPanel::appendLog);
    connect(m_server, &TcpServer::triggerReceived, this, &CommunicationPanel::executeRequested);
    connect(m_server, &TcpServer::clientCountChanged, this, [this](int count) {
        ui->lblClientCount->setText(QStringLiteral("连接: %1").arg(count));
    });

    ui->btnStop->setEnabled(false);
    ui->editCustomDelimiter->setVisible(false);
}

CommunicationPanel::~CommunicationPanel()
{
    delete ui;
}

QString CommunicationPanel::outputFormat() const
{
    return ui->editOutputFormat->toPlainText();
}

QString CommunicationPanel::delimiter() const
{
    const QString text = ui->comboDelimiter->currentText();
    if (text == QStringLiteral("自定义"))
        return ui->editCustomDelimiter->text();
    if (text == QStringLiteral("\\t"))
        return QStringLiteral("\t");
    if (text == QStringLiteral("\\n"))
        return QStringLiteral("\n");
    return text;
}

void CommunicationPanel::setResponseData(const QString &data)
{
    m_server->setResponse(data);
}

void CommunicationPanel::appendLog(const QString &message, const QString &color)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    ui->txtCommLog->moveCursor(QTextCursor::End);
    ui->txtCommLog->setTextColor(QColor(color));
    ui->txtCommLog->insertPlainText(QStringLiteral("[%1] %2\n").arg(timestamp, message));
    ui->txtCommLog->moveCursor(QTextCursor::End);
}

void CommunicationPanel::onStartClicked()
{
    const int port = ui->spinPort->value();
    const QString controlWord = ui->editControlWord->text().trimmed();
    if (controlWord.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置错误"), QStringLiteral("请输入检测控制字"));
        return;
    }

    m_server->configure(port, controlWord);
    if (!m_server->startServer())
        return;

    ui->btnStart->setEnabled(false);
    ui->btnStop->setEnabled(true);
    ui->spinPort->setEnabled(false);
    ui->lblStatus->setText(QStringLiteral("● 运行中"));
    ui->lblStatus->setStyleSheet(QStringLiteral("color: #4caf50; font-size: 12px;"));
}

void CommunicationPanel::onStopClicked()
{
    m_server->stopServer();
    ui->btnStart->setEnabled(true);
    ui->btnStop->setEnabled(false);
    ui->spinPort->setEnabled(true);
    ui->lblStatus->setText(QStringLiteral("● 已停止"));
    ui->lblStatus->setStyleSheet(QStringLiteral("color: #888888; font-size: 12px;"));
}

void CommunicationPanel::onDelimiterChanged(const QString &text)
{
    ui->editCustomDelimiter->setVisible(text == QStringLiteral("自定义"));
}

QJsonObject CommunicationPanel::config() const
{
    QJsonObject object;
    object.insert(QStringLiteral("port"), ui->spinPort->value());
    object.insert(QStringLiteral("control_word"), ui->editControlWord->text());
    object.insert(QStringLiteral("output_format"), ui->editOutputFormat->toPlainText());
    object.insert(QStringLiteral("delimiter"), ui->comboDelimiter->currentText());
    object.insert(QStringLiteral("custom_delimiter"), ui->editCustomDelimiter->text());
    return object;
}

void CommunicationPanel::setConfig(const QJsonObject &config)
{
    if (config.contains(QStringLiteral("port")))
        ui->spinPort->setValue(config.value(QStringLiteral("port")).toInt(8080));
    if (config.contains(QStringLiteral("control_word")))
        ui->editControlWord->setText(config.value(QStringLiteral("control_word")).toString());
    if (config.contains(QStringLiteral("output_format")))
        ui->editOutputFormat->setPlainText(config.value(QStringLiteral("output_format")).toString());
    if (config.contains(QStringLiteral("delimiter"))) {
        const int index = ui->comboDelimiter->findText(config.value(QStringLiteral("delimiter")).toString());
        if (index >= 0)
            ui->comboDelimiter->setCurrentIndex(index);
    }
    if (config.contains(QStringLiteral("custom_delimiter")))
        ui->editCustomDelimiter->setText(config.value(QStringLiteral("custom_delimiter")).toString());
}
