#include "outputpanel.h"
#include "ui_outputpanel.h"

#include "../core/plugintypes.h"

#include <QHeaderView>
#include <QMetaType>
#include <QTableWidgetItem>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

namespace {
constexpr int kMaxLogBlocks = 300;
constexpr int kFlushIntervalMs = 80;
}

OutputPanel::OutputPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OutputPanel)
{
    ui->setupUi(this);
    ui->tblResults->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 由 Qt 自行裁剪历史日志，比每次写入前统计 blockCount() 便宜得多
    ui->txtLog->setUndoRedoEnabled(false);
    ui->txtLog->document()->setMaximumBlockCount(kMaxLogBlocks);

    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(kFlushIntervalMs);
    connect(m_flushTimer, &QTimer::timeout, this, &OutputPanel::flushLog);

    connect(ui->btnClear, &QPushButton::clicked, this, &OutputPanel::clearLog);
}

OutputPanel::~OutputPanel()
{
    delete ui;
}

void OutputPanel::log(const QString &message, const QString &color)
{
    m_pending.append({message, color});

    // 缓冲量过大时立即落盘，避免连续运行模式下内存无上限增长
    if (m_pending.size() >= 64)
        flushLog();
    else
        m_flushTimer->start();
}

void OutputPanel::flushLog()
{
    m_flushTimer->stop();
    if (m_pending.isEmpty())
        return;

    ui->txtLog->setUpdatesEnabled(false);

    QTextCursor cursor(ui->txtLog->document());
    cursor.movePosition(QTextCursor::End);
    for (const LogEntry &entry : m_pending) {
        QTextCharFormat format;
        format.setForeground(QColor(entry.color));
        cursor.setCharFormat(format);
        cursor.insertText(entry.message + QLatin1Char('\n'));
    }
    m_pending.clear();

    ui->txtLog->setTextCursor(cursor);
    ui->txtLog->setUpdatesEnabled(true);
    ui->txtLog->ensureCursorVisible();
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
    int total = 0;
    for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
        if (it.key().startsWith(QLatin1Char('_')))
            continue;
        if (it.value().metaType().id() != QMetaType::QVariantMap)
            continue;
        total += it.value().toMap().size();
    }

    // 一次性定好行数再填值：逐行 insertRow 会让表格反复重算布局
    ui->tblResults->setUpdatesEnabled(false);
    ui->tblResults->clearContents();
    ui->tblResults->setRowCount(total);

    int row = 0;
    for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
        const QString nodeId = it.key();
        if (nodeId.startsWith(QLatin1Char('_')))
            continue;
        if (it.value().metaType().id() != QMetaType::QVariantMap)
            continue;

        const QVariantMap outputs = it.value().toMap();
        for (auto o = outputs.constBegin(); o != outputs.constEnd(); ++o) {
            ui->tblResults->setItem(row, 0, new QTableWidgetItem(nodeId));
            ui->tblResults->setItem(row, 1, new QTableWidgetItem(o.key()));
            ui->tblResults->setItem(row, 2,
                                    new QTableWidgetItem(OVP::portValueToString(o.value())));
            ++row;
        }
    }

    ui->tblResults->setUpdatesEnabled(true);
}

void OutputPanel::clearLog()
{
    m_pending.clear();
    ui->txtLog->clear();
    ui->tblResults->setRowCount(0);
}
