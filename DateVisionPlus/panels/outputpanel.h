#ifndef OUTPUTPANEL_H
#define OUTPUTPANEL_H

#include <QColor>
#include <QMap>
#include <QWidget>

#include <QVariant>

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

private:
    Ui::OutputPanel *ui = nullptr;
};

#endif // OUTPUTPANEL_H
