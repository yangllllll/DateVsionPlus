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

    QJsonObject config() const;
    void setConfig(const QJsonObject &config);

public slots:
    void setResponseData(const QString &data);
    void appendLog(const QString &message, const QString &color = QStringLiteral("#cccccc"));

signals:
    /** 收到远程控制字，请求主窗口执行流程图 */
    void executeRequested();

private slots:
    void onStartClicked();
    void onStopClicked();
    void onDelimiterChanged(const QString &text);

private:
    Ui::CommunicationPanel *ui = nullptr;
    TcpServer *m_server = nullptr;
};

#endif // COMMUNICATIONPANEL_H
