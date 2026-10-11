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
    ui->header->setProperty("uiRole", "panelToolbar");
    ui->headerLayout->setContentsMargins(10, 6, 10, 6);
    ui->lblTitle->setText(QStringLiteral("SESSION LOG"));
    ui->lblTitle->setProperty("uiRole", "caption");
    ui->btnClear->setProperty("uiRole", "compact");
    ui->btnClear->setText(QStringLiteral("清空记录"));
    ui->tabs->setTabText(0, QStringLiteral("运行日志"));
    ui->tabs->setTabText(1, QStringLiteral("输出结果"));
    ui->txtLog->setPlaceholderText(QStringLiteral("运行流程后，这里将显示执行日志。"));
    ui->txtLog->document()->setDocumentMargin(12);
    ui->tblResults->setAlternatingRowColors(true);
    ui->tblResults->verticalHeader()->hide();
    ui->tblResults->verticalHeader()->setDefaultSectionSize(30);
    ui->tblResults->setSelectionBehavior(QAbstractItemView::SelectRows);
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
    log(QStringLiteral("[信息] ") + message, QStringLiteral("#b7d2df"));
}

void OutputPanel::logSuccess(const QString &message)
{
    log(QStringLiteral("[成功] ") + message, QStringLiteral("#89b9a3"));
}

void OutputPanel::logWarning(const QString &message)
{
    log(QStringLiteral("[警告] ") + message, QStringLiteral("#d2b17c"));
}

void OutputPanel::logError(const QString &message)
{
    log(QStringLiteral("[错误] ") + message, QStringLiteral("#d28a8a"));
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
