#ifndef COMMUNICATIONPANEL_H
#define COMMUNICATIONPANEL_H

#include <QJsonObject>
#include <QWidget>

namespace Ui {
class CommunicationPanel;
}

class TcpServer;

/** 通信面板：TCP 服务端配置、输出格式与通信日志 */
class CommunicationPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CommunicationPanel(QWidget *parent = nullptr);
    ~CommunicationPanel() override;

    QString outputFormat() const;
    QString delimiter() const;
    /** 结果发送延迟（毫秒），用于兼容响应太快会丢失的客户端 */
    int sendDelayMs() const;
    /** 服务运行中且存在客户端时才需要回发结果 */
    bool canSendResponse() const;
    /** 忽略当前触发请求：不再回包，也不再等待超时 */
    void discardPendingTrigger();

    QJsonObject config() const;
    void setConfig(const QJsonObject &config);

public slots:
    void setResponseData(const QString &data);
    void appendLog(const QString &message, const QString &color = QStringLiteral("#cccccc"));

signals:
    /** 收到远程控制字，请求主窗口执行流程图 */
    void executeRequested();
    /** 本次触发等待结果超时，主窗口应作废该请求（不再补发结果） */
    void responseTimedOut();

private slots:
    void onStartClicked();
    void onStopClicked();
    void onDelimiterChanged(const QString &text);

private:
    Ui::CommunicationPanel *ui = nullptr;
    TcpServer *m_server = nullptr;
};

#endif // COMMUNICATIONPANEL_H
