#ifndef OUTPUTPANEL_H
#define OUTPUTPANEL_H

#include <QColor>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <QVariant>

class QTimer;

namespace Ui {
class OutputPanel;
}

/** 输出面板：执行日志 + 结果表格 */
class OutputPanel : public QWidget
{
    Q_OBJECT

public:
    explicit OutputPanel(QWidget *parent = nullptr);
    ~OutputPanel() override;

public slots:
    void log(const QString &message, const QString &color = QStringLiteral("#cccccc"));
    void logInfo(const QString &message);
    void logSuccess(const QString &message);
    void logWarning(const QString &message);
    void logError(const QString &message);
    void updateResults(const QMap<QString, QVariant> &results);
    void clearLog();

private slots:
    /** 把缓冲的日志一次性写入控件，避免每条日志都触发一次富文本重排 */
    void flushLog();

private:
    struct LogEntry
    {
        QString message;
        QString color;
    };

    Ui::OutputPanel *ui = nullptr;
    QTimer *m_flushTimer = nullptr;
    QVector<LogEntry> m_pending;
};

#endif // OUTPUTPANEL_H
