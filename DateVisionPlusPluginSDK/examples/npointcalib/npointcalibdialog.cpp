#include "npointcalibdialog.h"

#include "cvutils.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

const char *const kGroupStyle =
    "QGroupBox { color: #aaa; border: 1px solid #3e3e42; margin-top: 8px; } "
    "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }";
const char *const kButtonStyle =
    "QPushButton { background: #3e3e42; color: #ddd; border: 1px solid #555; padding: 4px 10px; } "
    "QPushButton:hover { background: #505050; }";
const char *const kOkStyle =
    "QPushButton { background: #0d7377; color: #fff; border: 1px solid #0a5c5f; padding: 4px 10px; } "
    "QPushButton:hover { background: #0a5c5f; }";

} // namespace

// ----------------------------------------------------------------- CalibCanvas

CalibCanvas::CalibCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(520, 380);
}

void CalibCanvas::setImage(const cv::Mat &image)
{
    if (image.empty()) {
        m_pixmap = QPixmap();
        m_imageSize = QSize();
    } else {
        m_pixmap = QPixmap::fromImage(OVP::matToQImage(image));
        m_imageSize = QSize(image.cols, image.rows);
    }
    update();
}

void CalibCanvas::setPoints(const QList<CALIB::CalibPoint> &points, const QList<int> &labels)
{
    m_points = points;
    m_labels = labels;
    update();
}

void CalibCanvas::setDetections(const QList<QPointF> &centers)
{
    m_detections = centers;
    update();
}

QRect CalibCanvas::imageRect() const
{
    if (m_pixmap.isNull())
        return QRect();

    QSize scaled = m_pixmap.size();
    scaled.scale(size(), Qt::KeepAspectRatio);
    return QRect((width() - scaled.width()) / 2, (height() - scaled.height()) / 2,
                 scaled.width(), scaled.height());
}

QPoint CalibCanvas::toWidget(const QPointF &imagePoint) const
{
    const QRect r = imageRect();
    if (r.isEmpty() || m_imageSize.isEmpty())
        return QPoint();

    const double scaleX = static_cast<double>(r.width()) / m_imageSize.width();
    const double scaleY = static_cast<double>(r.height()) / m_imageSize.height();
    return QPoint(r.x() + qRound(imagePoint.x() * scaleX),
                  r.y() + qRound(imagePoint.y() * scaleY));
}

void CalibCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));

    if (m_pixmap.isNull()) {
        painter.setPen(QColor(140, 140, 140));
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("无图像：请先连接上游图像节点"));
        return;
    }

    painter.drawPixmap(imageRect(), m_pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // YOLO 检测中心
    painter.setPen(QPen(QColor(110, 190, 255), 1));
    for (const QPointF &center : m_detections) {
        const QPoint p = toWidget(center);
        painter.drawEllipse(p, 6, 6);
        painter.drawLine(p + QPoint(-8, 0), p + QPoint(8, 0));
        painter.drawLine(p + QPoint(0, -8), p + QPoint(0, 8));
    }

    // 已录入的标定点
    for (int i = 0; i < m_points.size(); ++i) {
        const QPoint p = toWidget(m_points.at(i).image);
        painter.setPen(QPen(QColor(255, 80, 80), 2));
        painter.setBrush(QColor(255, 220, 0));
        painter.drawEllipse(p, 5, 5);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(255, 255, 120));

        const int label = (i < m_labels.size()) ? m_labels.at(i) : (i + 1);
        painter.drawText(p + QPoint(8, -8), QString::number(label));
    }
}

// ------------------------------------------------------------- NPointCalibDialog

NPointCalibDialog::NPointCalibDialog(OVP::NPointCalibPlugin *plugin, const cv::Mat &image,
                                     QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
    , m_image(image)
{
    setWindowTitle(QStringLiteral("N点标定 - 设置"));
    resize(1150, 720);
    setMinimumSize(900, 560);

    setupUi();

    m_canvas->setImage(m_image);
    m_calib = m_plugin ? m_plugin->calibration() : CALIB::CalibData();
    fillTable(m_calib.points);
    if (m_calib.valid) {
        m_resultLabel->setText(QStringLiteral("点数 %1 | RMS %2")
                                   .arg(m_calib.points.size())
                                   .arg(m_calib.rms, 0, 'f', 3));
    }

    // 表格为空时，自动用当前 YOLO 检测结果填充图像坐标
    refreshDetections(m_calib.points.isEmpty());
}

void NPointCalibDialog::setupUi()
{
    auto *mainLayout = new QHBoxLayout(this);

    // ---- 左侧：标定点录入 ----
    auto *left = new QWidget(this);
    left->setFixedWidth(470);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 6, 0);
    leftLayout->setSpacing(6);

    auto *pointGroup = new QGroupBox(QStringLiteral("标定点（图像坐标 -> 机器人坐标）"), left);
    pointGroup->setStyleSheet(QString::fromLatin1(kGroupStyle));
    auto *pointLayout = new QVBoxLayout(pointGroup);
    pointLayout->setSpacing(6);

    m_table = new QTableWidget(0, 5, pointGroup);
    m_table->setHorizontalHeaderLabels({QStringLiteral("序号"), QStringLiteral("图像 X"),
                                        QStringLiteral("图像 Y"), QStringLiteral("机器人 X"),
                                        QStringLiteral("机器人 Y")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_table, &QTableWidget::itemChanged, this, &NPointCalibDialog::onTableChanged);
    pointLayout->addWidget(m_table, 1);

    auto *pointButtons = new QHBoxLayout();
    auto *addButton = new QPushButton(QStringLiteral("添加行"), pointGroup);
    addButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(addButton, &QPushButton::clicked, this, &NPointCalibDialog::onAddRow);
    auto *deleteButton = new QPushButton(QStringLiteral("删除行"), pointGroup);
    deleteButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(deleteButton, &QPushButton::clicked, this, &NPointCalibDialog::onDeleteRow);
    auto *clearButton = new QPushButton(QStringLiteral("清空"), pointGroup);
    clearButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(clearButton, &QPushButton::clicked, this, &NPointCalibDialog::onClearRows);
    pointButtons->addWidget(addButton);
    pointButtons->addWidget(deleteButton);
    pointButtons->addWidget(clearButton);
    pointLayout->addLayout(pointButtons);

    leftLayout->addWidget(pointGroup, 1);

    // ---- 自动取点 ----
    auto *yoloGroup = new QGroupBox(QStringLiteral("YOLO 自动取点"), left);
    yoloGroup->setStyleSheet(QString::fromLatin1(kGroupStyle));
    auto *yoloLayout = new QVBoxLayout(yoloGroup);
    yoloLayout->setSpacing(6);

    auto *fillButton = new QPushButton(QStringLiteral("自动取点（使用当前检测结果）"), yoloGroup);
    fillButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(fillButton, &QPushButton::clicked, this, &NPointCalibDialog::onAutoFill);
    yoloLayout->addWidget(fillButton);

    auto *yoloHint = new QLabel(
        QStringLiteral("先运行一次流程让 YOLO 输出结果。打开窗口时会自动把每个检测框中心\n"
                       "按序号填入图像坐标，并在右侧图像上标注点序号；\n"
                       "检测变化后可点「自动取点」重建列表（已填的机器人坐标按行保留）。"),
        yoloGroup);
    yoloHint->setWordWrap(true);
    yoloHint->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    yoloLayout->addWidget(yoloHint);

    leftLayout->addWidget(yoloGroup);

    m_resultLabel = new QLabel(QStringLiteral("尚未标定"), left);
    m_resultLabel->setStyleSheet(QStringLiteral("color: #7fd18b; font-weight: bold;"));
    leftLayout->addWidget(m_resultLabel);

    m_statusLabel = new QLabel(QStringLiteral("至少需要 3 组点；建议分散到画面四周"), left);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    leftLayout->addWidget(m_statusLabel);

    auto *calcRow = new QHBoxLayout();
    auto *computeButton = new QPushButton(QStringLiteral("计算标定"), left);
    computeButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(computeButton, &QPushButton::clicked, this, &NPointCalibDialog::onCompute);
    auto *saveButton = new QPushButton(QStringLiteral("保存文件"), left);
    saveButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(saveButton, &QPushButton::clicked, this, &NPointCalibDialog::onSaveFile);
    auto *loadButton = new QPushButton(QStringLiteral("加载文件"), left);
    loadButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(loadButton, &QPushButton::clicked, this, &NPointCalibDialog::onLoadFile);
    calcRow->addWidget(computeButton);
    calcRow->addWidget(saveButton);
    calcRow->addWidget(loadButton);
    leftLayout->addLayout(calcRow);

    auto *dialogButtons = new QHBoxLayout();
    dialogButtons->addStretch();
    auto *okButton = new QPushButton(QStringLiteral("确定"), left);
    okButton->setStyleSheet(QString::fromLatin1(kOkStyle));
    connect(okButton, &QPushButton::clicked, this, &NPointCalibDialog::onAccept);
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), left);
    cancelButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(cancelButton, &QPushButton::clicked, this, &NPointCalibDialog::reject);
    dialogButtons->addWidget(okButton);
    dialogButtons->addWidget(cancelButton);
    leftLayout->addLayout(dialogButtons);

    mainLayout->addWidget(left);

    // ---- 右侧预览 ----
    auto *rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    auto *caption = new QLabel(QStringLiteral("图像预览（黄点 = 已录入标定点，蓝圈 = YOLO 中心）"), this);
    caption->setStyleSheet(QStringLiteral("color: #aaa;"));
    rightLayout->addWidget(caption);

    m_canvas = new CalibCanvas(this);
    rightLayout->addWidget(m_canvas, 1);

    mainLayout->addLayout(rightLayout, 1);
}

// ------------------------------------------------------------------- 表格

void NPointCalibDialog::fillTable(const QList<CALIB::CalibPoint> &points)
{
    m_updatingTable = true;
    m_table->setRowCount(0);
    m_table->setRowCount(points.size());

    for (int row = 0; row < points.size(); ++row) {
        auto *number = new QTableWidgetItem(QString::number(row + 1));
        number->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        number->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, number);

        setPointCell(row, 1, points.at(row).image.x());
        setPointCell(row, 2, points.at(row).image.y());
        setPointCell(row, 3, points.at(row).robot.x());
        setPointCell(row, 4, points.at(row).robot.y());
    }

    m_updatingTable = false;
    updatePreview();
}

void NPointCalibDialog::renumberRows()
{
    m_updatingTable = true;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto *item = m_table->item(row, 0);
        if (!item) {
            item = new QTableWidgetItem;
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(row, 0, item);
        }
        item->setText(QString::number(row + 1));
    }
    m_updatingTable = false;
}

void NPointCalibDialog::setPointCell(int row, int column, double value)
{
    auto *item = m_table->item(row, column);
    if (!item) {
        item = new QTableWidgetItem;
        m_table->setItem(row, column, item);
    }
    item->setText(QString::number(value, 'f', 2));
}

bool NPointCalibDialog::collectPoints(QList<CALIB::CalibPoint> &points, QString *error) const
{
    points.clear();

    for (int row = 0; row < m_table->rowCount(); ++row) {
        QString cells[4];
        bool allEmpty = true;
        for (int column = 0; column < 4; ++column) {
            auto *item = m_table->item(row, column + 1);
            cells[column] = item ? item->text().trimmed() : QString();
            if (!cells[column].isEmpty())
                allEmpty = false;
        }

        if (allEmpty)
            continue;

        double values[4] = {0.0, 0.0, 0.0, 0.0};
        for (int column = 0; column < 4; ++column) {
            bool ok = false;
            values[column] = cells[column].toDouble(&ok);
            if (!ok) {
                if (error) {
                    *error = QStringLiteral("第 %1 行「%2」不是有效数字")
                                 .arg(row + 1)
                                 .arg(QStringList{cells[0], cells[1], cells[2], cells[3]}
                                          .value(column));
                }
                return false;
            }
        }

        CALIB::CalibPoint point;
        point.image = QPointF(values[0], values[1]);
        point.robot = QPointF(values[2], values[3]);
        points.append(point);
    }

    if (points.size() < 3) {
        if (error)
            *error = QStringLiteral("至少需要 3 组完整标定点，当前 %1 组").arg(points.size());
        return false;
    }

    return true;
}

// ------------------------------------------------------------------- 交互

void NPointCalibDialog::onAddRow()
{
    m_updatingTable = true;
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *number = new QTableWidgetItem(QString::number(row + 1));
    number->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    number->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 0, number);

    for (int column = 1; column <= 4; ++column)
        m_table->setItem(row, column, new QTableWidgetItem);

    m_updatingTable = false;
    m_table->setCurrentCell(row, 1);
    updatePreview();
}

void NPointCalibDialog::onDeleteRow()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;

    m_updatingTable = true;
    m_table->removeRow(row);
    m_updatingTable = false;

    renumberRows();
    updatePreview();
}

void NPointCalibDialog::onClearRows()
{
    m_updatingTable = true;
    m_table->setRowCount(0);
    m_updatingTable = false;

    m_calib = CALIB::CalibData();
    m_resultLabel->setText(QStringLiteral("尚未标定"));
    updatePreview();
}

void NPointCalibDialog::onAutoFill()
{
    if (m_table->rowCount() > 0) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("自动取点"),
            QStringLiteral("将用当前 YOLO 检测结果重建标定点列表。\n"
                           "已填写的机器人坐标会按行号保留，继续吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer != QMessageBox::Yes)
            return;
    }

    refreshDetections(true);
}

void NPointCalibDialog::onCompute()
{
    QList<CALIB::CalibPoint> points;
    QString error;
    if (!collectPoints(points, &error)) {
        setStatus(error, true);
        return;
    }

    m_calib.points = points;
    if (!m_image.empty())
        m_calib.imageSize = QSize(m_image.cols, m_image.rows);

    if (!m_calib.compute(&error)) {
        setStatus(error, true);
        m_resultLabel->setText(QStringLiteral("标定失败"));
        return;
    }

    m_resultLabel->setText(QStringLiteral("点数 %1 | RMS %2")
                               .arg(points.size())
                               .arg(m_calib.rms, 0, 'f', 3));
    setStatus(QStringLiteral("标定完成，可保存为标定文件"), false);
    updatePreview();
}

void NPointCalibDialog::onSaveFile()
{
    QList<CALIB::CalibPoint> points;
    QString error;
    if (!collectPoints(points, &error)) {
        setStatus(error, true);
        return;
    }

    m_calib.points = points;
    if (!m_image.empty())
        m_calib.imageSize = QSize(m_image.cols, m_image.rows);
    if (!m_calib.compute(&error)) {
        setStatus(error, true);
        return;
    }

    QString suggested = m_plugin ? m_plugin->paramString(QStringLiteral("calib_file")) : QString();
    if (suggested.isEmpty())
        suggested = QStringLiteral("calib.ncal");

    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存标定文件"), suggested,
                                                CALIB::fileFilter());
    if (path.isEmpty())
        return;

    if (!path.endsWith(QStringLiteral(".ncal"), Qt::CaseInsensitive)
        && !path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".ncal");
    }

    if (!m_calib.save(path, &error)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        return;
    }

    if (m_plugin)
        m_plugin->setParam(QStringLiteral("calib_file"), path);

    m_resultLabel->setText(QStringLiteral("点数 %1 | RMS %2")
                               .arg(points.size())
                               .arg(m_calib.rms, 0, 'f', 3));
    setStatus(QStringLiteral("已保存：%1").arg(path), false);
}

void NPointCalibDialog::onLoadFile()
{
    QString suggested = m_plugin ? m_plugin->paramString(QStringLiteral("calib_file")) : QString();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("加载标定文件"),
                                                      suggested, CALIB::fileFilter());
    if (path.isEmpty())
        return;

    CALIB::CalibData data;
    QString error;
    if (!CALIB::CalibData::load(path, data, &error)) {
        QMessageBox::critical(this, QStringLiteral("加载失败"), error);
        return;
    }

    m_calib = data;
    fillTable(data.points);

    m_resultLabel->setText(QStringLiteral("点数 %1 | RMS %2")
                               .arg(data.points.size())
                               .arg(data.rms, 0, 'f', 3));
    setStatus(QStringLiteral("已加载：%1").arg(path), false);

    if (m_plugin)
        m_plugin->setParam(QStringLiteral("calib_file"), path);
}

void NPointCalibDialog::onTableChanged()
{
    if (m_updatingTable)
        return;
    updatePreview();
}

void NPointCalibDialog::onAccept()
{
    QList<CALIB::CalibPoint> points;
    QString error;
    if (!collectPoints(points, &error)) {
        QMessageBox::warning(this, QStringLiteral("无法保存"), error);
        return;
    }

    m_calib.points = points;
    if (!m_image.empty())
        m_calib.imageSize = QSize(m_image.cols, m_image.rows);

    if (!m_calib.compute(&error)) {
        QMessageBox::warning(this, QStringLiteral("标定失败"), error);
        return;
    }

    if (m_plugin)
        m_plugin->setCalibration(m_calib);
    accept();
}

void NPointCalibDialog::updatePreview()
{
    // 只要图像坐标有效就画出来（机器人坐标可以稍后填写），标注行号
    QList<CALIB::CalibPoint> points;
    QList<int> labels;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        bool okX = false;
        bool okY = false;
        auto *itemX = m_table->item(row, 1);
        auto *itemY = m_table->item(row, 2);
        const double u = itemX ? itemX->text().trimmed().toDouble(&okX) : 0.0;
        const double v = itemY ? itemY->text().trimmed().toDouble(&okY) : 0.0;
        if (!okX || !okY)
            continue;

        bool okRobotX = false;
        bool okRobotY = false;
        auto *robotX = m_table->item(row, 3);
        auto *robotY = m_table->item(row, 4);
        const double x = robotX ? robotX->text().trimmed().toDouble(&okRobotX) : 0.0;
        const double y = robotY ? robotY->text().trimmed().toDouble(&okRobotY) : 0.0;

        CALIB::CalibPoint point;
        point.image = QPointF(u, v);
        point.robot = (okRobotX && okRobotY) ? QPointF(x, y) : QPointF(0.0, 0.0);
        points.append(point);
        labels.append(row + 1);
    }

    if (m_canvas)
        m_canvas->setPoints(points, labels);
}

void NPointCalibDialog::setStatus(const QString &text, bool isError)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError ? QStringLiteral("color: #ff6b6b; font-size: 11px;")
                                         : QStringLiteral("color: #7fd18b; font-size: 11px;"));
}

void NPointCalibDialog::refreshDetections(bool fillTable)
{
    m_detections.clear();

    const QVariant value = m_plugin ? m_plugin->input(QStringLiteral("detections")) : QVariant();
    if (!value.isValid()) {
        if (m_canvas)
            m_canvas->setDetections({});
        if (fillTable)
            setStatus(QStringLiteral("未收到检测结果：请先运行一次流程让 YOLO 输出，再点「自动取点」"), true);
        return;
    }

    QString error;
    if (!DETECT::parse(value, m_detections, &error)) {
        if (m_canvas)
            m_canvas->setDetections({});
        setStatus(error, true);
        return;
    }

    QList<QPointF> centers;
    centers.reserve(m_detections.size());
    for (const DETECT::Item &item : m_detections)
        centers.append(item.center);

    if (m_canvas)
        m_canvas->setDetections(centers);

    if (!fillTable)
        return;

    if (m_detections.isEmpty()) {
        setStatus(QStringLiteral("当前没有检测到目标，无法自动取点"), true);
        return;
    }

    // 保留已填写的机器人坐标（按行号对应）
    QList<bool> hasRobot;
    QList<QPointF> robotValues;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        bool okX = false;
        bool okY = false;
        auto *itemX = m_table->item(row, 3);
        auto *itemY = m_table->item(row, 4);
        const double x = itemX ? itemX->text().trimmed().toDouble(&okX) : 0.0;
        const double y = itemY ? itemY->text().trimmed().toDouble(&okY) : 0.0;
        hasRobot.append(okX && okY);
        robotValues.append(QPointF(x, y));
    }

    // 用检测框中心重建图像坐标，列表顺序 = 点序号
    m_updatingTable = true;
    m_table->setRowCount(m_detections.size());
    for (int row = 0; row < m_detections.size(); ++row) {
        auto *number = new QTableWidgetItem(QString::number(row + 1));
        number->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        number->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, number);

        const QPointF center = m_detections.at(row).center;
        setPointCell(row, 1, center.x());
        setPointCell(row, 2, center.y());

        if (row < hasRobot.size() && hasRobot.at(row)) {
            setPointCell(row, 3, robotValues.at(row).x());
            setPointCell(row, 4, robotValues.at(row).y());
        } else {
            m_table->setItem(row, 3, new QTableWidgetItem);
            m_table->setItem(row, 4, new QTableWidgetItem);
        }
    }
    m_updatingTable = false;

    m_calib.valid = false;
    m_resultLabel->setText(QStringLiteral("尚未标定"));
    updatePreview();

    setStatus(QStringLiteral("已自动添加 %1 个点，请按图中序号依次填写机器人坐标")
                  .arg(m_detections.size()),
              false);
}
