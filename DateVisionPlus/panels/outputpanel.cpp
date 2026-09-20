#include "outputpanel.h"
#include "ui_outputpanel.h"

#include "../core/plugintypes.h"

#include <QHeaderView>
#include <QMetaType>
#include <QTableWidgetItem>
#include <QTextCursor>

OutputPanel::OutputPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OutputPanel)
{
    ui->setupUi(this);
    ui->tblResults->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    connect(ui->btnClear, &QPushButton::clicked, this, &OutputPanel::clearLog);
}

OutputPanel::~OutputPanel()
{
    delete ui;
}

void OutputPanel::log(const QString &message, const QString &color)
{
    if (ui->txtLog->document()->blockCount() > 200)
        ui->txtLog->clear();

    ui->txtLog->moveCursor(QTextCursor::End);
    ui->txtLog->setTextColor(QColor(color));
    ui->txtLog->insertPlainText(message + QLatin1Char('\n'));
    ui->txtLog->moveCursor(QTextCursor::End);
}

void OutputPanel::logInfo(const QString &message)
{
    log(QStringLiteral("[信息] ") + message, QStringLiteral("#2196f3"));
}

void OutputPanel::logSuccess(const QString &message)
{
    log(QStringLiteral("[成功] ") + message, QStringLiteral("#4caf50"));
}

void OutputPanel::logWarning(const QString &message)
{
    log(QStringLiteral("[警告] ") + message, QStringLiteral("#ff9800"));
}

void OutputPanel::logError(const QString &message)
{
    log(QStringLiteral("[错误] ") + message, QStringLiteral("#f44336"));
}

void OutputPanel::updateResults(const QMap<QString, QVariant> &results)
{
    ui->tblResults->setRowCount(0);

    int row = 0;
    for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
        const QString nodeId = it.key();
        if (nodeId.startsWith(QLatin1Char('_')))
            continue;
        if (it.value().metaType().id() != QMetaType::QVariantMap)
            continue;

        const QVariantMap outputs = it.value().toMap();
        for (auto o = outputs.constBegin(); o != outputs.constEnd(); ++o) {
            ui->tblResults->insertRow(row);
            ui->tblResults->setItem(row, 0, new QTableWidgetItem(nodeId));
            ui->tblResults->setItem(row, 1, new QTableWidgetItem(o.key()));
            ui->tblResults->setItem(row, 2,
                                    new QTableWidgetItem(OVP::portValueToString(o.value())));
            ++row;
        }
    }
}

void OutputPanel::clearLog()
{
    ui->txtLog->clear();
    ui->tblResults->setRowCount(0);
}
