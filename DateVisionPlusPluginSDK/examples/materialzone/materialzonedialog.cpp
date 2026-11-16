#include "materialzonedialog.h"

#include "cvutils.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const char *const kGroupStyle =
    "QGroupBox { color: #aaa; border: 1px solid #3e3e42; margin-top: 8px; } "
    "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }";
const char *const kInputStyle =
    "background: #333; color: #ccc; border: 1px solid #555; padding: 2px;";
const char *const kButtonStyle =
    "QPushButton { background: #3e3e42; color: #ddd; border: 1px solid #555; padding: 4px 10px; } "
    "QPushButton:hover { background: #505050; }";

} // namespace

// ----------------------------------------------------------------- ZoneCanvas

ZoneCanvas::ZoneCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
}

void ZoneCanvas::setImage(const cv::Mat &image)
{
    m_image = image;
    if (image.empty()) {
        m_pixmap = QPixmap();
    } else {
        m_pixmap = QPixmap::fromImage(OVP::matToQImage(image));
    }
    update();
}

void ZoneCanvas::setZones(const QList<OVP::Zone> &zones)
{
    m_zones = zones;
    std::sort(m_zones.begin(), m_zones.end(),
              [](const OVP::Zone &a, const OVP::Zone &b) { return a.id < b.id; });
    update();
}

void ZoneCanvas::setSelectedId(int id)
{
    if (m_selectedId == id)
        return;
    m_selectedId = id;
    update();
}

QRect ZoneCanvas::imageRect() const
{
    if (m_pixmap.isNull())
        return QRect();

    QSize scaled = m_pixmap.size();
    scaled.scale(size(), Qt::KeepAspectRatio);
    return QRect((width() - scaled.width()) / 2, (height() - scaled.height()) / 2,
                 scaled.width(), scaled.height());
}

QRect ZoneCanvas::toWidgetRect(const cv::Rect2f &zone) const
{
    const QRect ir = imageRect();
    if (ir.isEmpty())
        return QRect();
    return QRect(ir.x() + qRound(zone.x * ir.width()),
                 ir.y() + qRound(zone.y * ir.height()),
                 qRound(zone.width * ir.width()),
                 qRound(zone.height * ir.height()));
}

cv::Rect2f ZoneCanvas::toZoneRect(const QRect &widgetRect) const
{
    const QRect ir = imageRect();
    if (ir.isEmpty())
        return cv::Rect2f();

    const QRect r = widgetRect.normalized().intersected(ir);
    if (r.width() < 2 || r.height() < 2)
        return cv::Rect2f();

    return cv::Rect2f(static_cast<float>(r.x() - ir.x()) / ir.width(),
                      static_cast<float>(r.y() - ir.y()) / ir.height(),
                      static_cast<float>(r.width()) / ir.width(),
                      static_cast<float>(r.height()) / ir.height());
}

int ZoneCanvas::zoneAt(const QPoint &pos) const
{
    for (int i = m_zones.size() - 1; i >= 0; --i) {
        if (toWidgetRect(m_zones.at(i).rect).contains(pos))
            return m_zones.at(i).id;
    }
    return -1;
}

int ZoneCanvas::nextZoneId() const
{
    int maxId = 0;
    for (const OVP::Zone &zone : m_zones)
        maxId = std::max(maxId, zone.id);
    return maxId + 1;
}

void ZoneCanvas::paintEvent(QPaintEvent *event)
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

    const QRect target = imageRect();
    painter.drawPixmap(target, m_pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QFontMetrics metrics(font());
    for (const OVP::Zone &zone : m_zones) {
        const QRect r = toWidgetRect(zone.rect);
        if (r.isEmpty())
            continue;

        const bool selected = (zone.id == m_selectedId);
        const QColor color = selected ? QColor(255, 200, 0) : QColor(0, 160, 255);

        painter.setPen(QPen(color, selected ? 3 : 2));
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 36));
        painter.drawRect(r);

        const QString label = QStringLiteral("Z%1").arg(zone.id);
        QRect labelRect = metrics.boundingRect(label).adjusted(0, 0, 8, 4);
        labelRect.moveTopLeft(QPoint(r.left() + 2, r.top() + 2));
        if (labelRect.bottom() > r.bottom())
            labelRect.moveBottom(r.bottom() - 2);

        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRect(labelRect);
        painter.setPen(Qt::black);
        painter.drawText(labelRect, Qt::AlignCenter, label);
    }

    if (m_dragging && !m_rubberBand.isNull()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 235, 59), 1, Qt::DashLine));
        painter.drawRect(m_rubberBand);
    }
}

void ZoneCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !hasImage()
        || !imageRect().contains(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    setFocus();
    m_pressed = true;
    m_dragging = false;
    m_start = event->pos();
    m_rubberBand = QRect(m_start, m_start);
}

void ZoneCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (!m_dragging
        && (event->pos() - m_start).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    m_dragging = true;
    m_rubberBand = QRect(m_start, event->pos()).normalized().intersected(imageRect());
    update();
}

void ZoneCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_pressed) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_pressed = false;

    if (m_dragging) {
        m_dragging = false;
        const QRect dragRect =
            QRect(m_start, event->pos()).normalized().intersected(imageRect());
        m_rubberBand = QRect();

        if (dragRect.width() >= 4 && dragRect.height() >= 4) {
            OVP::Zone zone;
            zone.id = nextZoneId();
            zone.rect = toZoneRect(dragRect);
            if (zone.rect.width > 0.0f && zone.rect.height > 0.0f) {
                m_zones.append(zone);
                m_selectedId = zone.id;
                update();
                emit zonesChanged();
                emit selectionChanged(zone.id);
                return;
            }
        }
        update();
        return;
    }

    // 单击：选中（空白处取消选中）
    m_selectedId = zoneAt(event->pos());
    update();
    emit selectionChanged(m_selectedId);
}

void ZoneCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_selectedId >= 0) {
            for (int i = 0; i < m_zones.size(); ++i) {
                if (m_zones.at(i).id == m_selectedId) {
                    m_zones.removeAt(i);
                    m_selectedId = -1;
                    update();
                    emit zonesChanged();
                    emit selectionChanged(-1);
                    return;
                }
            }
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

// ------------------------------------------------------- MaterialZoneDialog

MaterialZoneDialog::MaterialZoneDialog(OVP::MaterialZonePlugin *plugin, const cv::Mat &image,
                                       QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
    , m_image(image)
{
    setWindowTitle(QStringLiteral("物料识别 - 检测区设置"));
    resize(1100, 700);
    setMinimumSize(820, 520);

    setupUi();
    loadParams();

    m_canvas->setImage(m_image);
    if (m_plugin)
        m_canvas->setZones(m_plugin->zones());
    refreshTable();
}

void MaterialZoneDialog::setupUi()
{
    auto *mainLayout = new QHBoxLayout(this);

    // ---- 左侧：检测区列表 + 参数 ----
    auto *left = new QWidget(this);
    left->setFixedWidth(350);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 6, 0);
    leftLayout->setSpacing(6);

    auto *zoneGroup = new QGroupBox(QStringLiteral("检测区"), left);
    zoneGroup->setStyleSheet(QString::fromLatin1(kGroupStyle));
    auto *zoneLayout = new QVBoxLayout(zoneGroup);
    zoneLayout->setSpacing(6);

    m_table = new QTableWidget(0, 5, zoneGroup);
    m_table->setHorizontalHeaderLabels({QStringLiteral("编号"), QStringLiteral("X"),
                                        QStringLiteral("Y"), QStringLiteral("宽"),
                                        QStringLiteral("高")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_table, &QTableWidget::itemSelectionChanged, this,
            &MaterialZoneDialog::onTableSelectionChanged);
    zoneLayout->addWidget(m_table, 1);

    auto *zoneButtons = new QHBoxLayout();
    auto *deleteButton = new QPushButton(QStringLiteral("删除选中"), zoneGroup);
    deleteButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(deleteButton, &QPushButton::clicked, this, &MaterialZoneDialog::onDeleteSelected);
    auto *clearButton = new QPushButton(QStringLiteral("清空"), zoneGroup);
    clearButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(clearButton, &QPushButton::clicked, this, &MaterialZoneDialog::onClearZones);
    zoneButtons->addWidget(deleteButton);
    zoneButtons->addWidget(clearButton);
    zoneLayout->addLayout(zoneButtons);

    auto *hint = new QLabel(
        QStringLiteral("在右侧图像上按住左键拖拽即可新建检测区；单击选中，Delete 删除。\n"
                       "检测区按编号从小到大判断，输出编号最小的命中区。"),
        zoneGroup);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    zoneLayout->addWidget(hint);

    leftLayout->addWidget(zoneGroup, 1);

    auto *paramGroup = new QGroupBox(QStringLiteral("参数"), left);
    paramGroup->setStyleSheet(QString::fromLatin1(kGroupStyle));
    auto *paramForm = new QFormLayout(paramGroup);
    paramForm->setSpacing(6);

    m_scoreSpin = new QDoubleSpinBox(paramGroup);
    m_scoreSpin->setRange(0.0, 1.0);
    m_scoreSpin->setSingleStep(0.01);
    m_scoreSpin->setDecimals(2);
    m_scoreSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    paramForm->addRow(QStringLiteral("最小置信度"), m_scoreSpin);

    m_classFilterEdit = new QLineEdit(paramGroup);
    m_classFilterEdit->setStyleSheet(QString::fromLatin1(kInputStyle));
    m_classFilterEdit->setPlaceholderText(QStringLiteral("如 0,2 或 person,car（留空=全部）"));
    paramForm->addRow(QStringLiteral("类别过滤"), m_classFilterEdit);

    m_thicknessSpin = new QSpinBox(paramGroup);
    m_thicknessSpin->setRange(1, 5);
    m_thicknessSpin->setStyleSheet(QString::fromLatin1(kInputStyle));
    paramForm->addRow(QStringLiteral("线宽"), m_thicknessSpin);

    m_drawCheck = new QCheckBox(QStringLiteral("绘制检测区"), paramGroup);
    paramForm->addRow(QString(), m_drawCheck);

    m_emptyNgCheck = new QCheckBox(QStringLiteral("无物料判NG"), paramGroup);
    paramForm->addRow(QString(), m_emptyNgCheck);

    leftLayout->addWidget(paramGroup);

    m_statusLabel = new QLabel(QStringLiteral("就绪"), left);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    leftLayout->addWidget(m_statusLabel);

    auto *dialogButtons = new QHBoxLayout();
    dialogButtons->addStretch();
    auto *okButton = new QPushButton(QStringLiteral("确定"), left);
    okButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #0d7377; color: #fff; border: 1px solid #0a5c5f; "
        "padding: 4px 10px; } QPushButton:hover { background: #0a5c5f; }"));
    connect(okButton, &QPushButton::clicked, this, &MaterialZoneDialog::onAccept);
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), left);
    cancelButton->setStyleSheet(QString::fromLatin1(kButtonStyle));
    connect(cancelButton, &QPushButton::clicked, this, &MaterialZoneDialog::reject);
    dialogButtons->addWidget(okButton);
    dialogButtons->addWidget(cancelButton);
    leftLayout->addLayout(dialogButtons);

    mainLayout->addWidget(left);

    // ---- 右侧：ROI 绘制画布 ----
    auto *rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    auto *caption = new QLabel(QStringLiteral("检测区 ROI（按住左键拖拽绘制）"), this);
    caption->setStyleSheet(QStringLiteral("color: #aaa;"));
    rightLayout->addWidget(caption);

    m_canvas = new ZoneCanvas(this);
    connect(m_canvas, &ZoneCanvas::zonesChanged, this, &MaterialZoneDialog::onZonesChanged);
    connect(m_canvas, &ZoneCanvas::selectionChanged, this, &MaterialZoneDialog::onSelectionChanged);
    rightLayout->addWidget(m_canvas, 1);

    mainLayout->addLayout(rightLayout, 1);
}

// --------------------------------------------------------------- 参数读写

void MaterialZoneDialog::loadParams()
{
    if (!m_plugin)
        return;

    m_scoreSpin->setValue(m_plugin->paramDouble(QStringLiteral("min_score"), 0.0));
    m_classFilterEdit->setText(m_plugin->paramString(QStringLiteral("class_filter")));
    m_thicknessSpin->setValue(m_plugin->paramInt(QStringLiteral("line_thickness"), 2));
    m_drawCheck->setChecked(m_plugin->paramBool(QStringLiteral("draw"), true));
    m_emptyNgCheck->setChecked(m_plugin->paramBool(QStringLiteral("empty_as_ng"), false));
}

void MaterialZoneDialog::applyParams()
{
    if (!m_plugin)
        return;

    m_plugin->setParam(QStringLiteral("min_score"), m_scoreSpin->value());
    m_plugin->setParam(QStringLiteral("class_filter"), m_classFilterEdit->text().trimmed());
    m_plugin->setParam(QStringLiteral("line_thickness"), m_thicknessSpin->value());
    m_plugin->setParam(QStringLiteral("draw"), m_drawCheck->isChecked());
    m_plugin->setParam(QStringLiteral("empty_as_ng"), m_emptyNgCheck->isChecked());
}

// ------------------------------------------------------------------ 列表

void MaterialZoneDialog::refreshTable()
{
    m_updatingTable = true;

    const QList<OVP::Zone> zones = m_canvas->zones();
    const bool hasImage = m_canvas->hasImage() && !m_image.empty();

    m_table->setRowCount(zones.size());
    for (int row = 0; row < zones.size(); ++row) {
        const OVP::Zone &zone = zones.at(row);

        auto *idItem = new QTableWidgetItem(QString::number(zone.id));
        idItem->setData(Qt::UserRole, zone.id);
        idItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, idItem);

        QString values[4];
        if (hasImage) {
            values[0] = QString::number(qRound(zone.rect.x * m_image.cols));
            values[1] = QString::number(qRound(zone.rect.y * m_image.rows));
            values[2] = QString::number(qRound(zone.rect.width * m_image.cols));
            values[3] = QString::number(qRound(zone.rect.height * m_image.rows));
        } else {
            values[0] = QStringLiteral("%1%").arg(zone.rect.x * 100.0f, 0, 'f', 1);
            values[1] = QStringLiteral("%1%").arg(zone.rect.y * 100.0f, 0, 'f', 1);
            values[2] = QStringLiteral("%1%").arg(zone.rect.width * 100.0f, 0, 'f', 1);
            values[3] = QStringLiteral("%1%").arg(zone.rect.height * 100.0f, 0, 'f', 1);
        }

        for (int column = 0; column < 4; ++column) {
            auto *item = new QTableWidgetItem(values[column]);
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(row, column + 1, item);
        }
    }

    m_updatingTable = false;

    m_statusLabel->setText(QStringLiteral("共 %1 个检测区 | 坐标单位：%2")
                               .arg(zones.size())
                               .arg(hasImage ? QStringLiteral("像素") : QStringLiteral("%")));
    selectRowById(m_canvas->selectedId());
}

void MaterialZoneDialog::selectRowById(int id)
{
    if (id < 0) {
        m_table->clearSelection();
        return;
    }

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto *item = m_table->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == id) {
            m_table->selectRow(row);
            return;
        }
    }
}

void MaterialZoneDialog::onZonesChanged()
{
    refreshTable();
}

void MaterialZoneDialog::onSelectionChanged(int id)
{
    selectRowById(id);
}

void MaterialZoneDialog::onTableSelectionChanged()
{
    if (m_updatingTable)
        return;

    const int row = m_table->currentRow();
    if (row < 0)
        return;

    auto *item = m_table->item(row, 0);
    if (!item)
        return;

    m_canvas->setSelectedId(item->data(Qt::UserRole).toInt());
}

void MaterialZoneDialog::onDeleteSelected()
{
    const int id = m_canvas->selectedId();
    if (id < 0)
        return;

    QList<OVP::Zone> zones = m_canvas->zones();
    for (int i = 0; i < zones.size(); ++i) {
        if (zones.at(i).id == id) {
            zones.removeAt(i);
            break;
        }
    }

    m_canvas->setZones(zones);
    m_canvas->setSelectedId(-1);
    refreshTable();
}

void MaterialZoneDialog::onClearZones()
{
    m_canvas->setZones({});
    m_canvas->setSelectedId(-1);
    refreshTable();
}

void MaterialZoneDialog::onAccept()
{
    applyParams();
    if (m_plugin)
        m_plugin->setZones(m_canvas->zones());
    accept();
}
