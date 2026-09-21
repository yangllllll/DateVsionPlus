#include "propertiespanel.h"
#include "ui_propertiespanel.h"

#include "../core/pluginbase.h"
#include "../flowchart/nodeitem.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

namespace {
const char *kEditorStyle =
    "font-family: 'Microsoft YaHei'; font-size: 11px; background: #333333; color: #cccccc; "
    "border: 1px solid #555555; padding: 2px;";

/**
 * @brief 弹出列表宽度自适应的下拉框
 *
 * QComboBox 默认把弹出列表的宽度限制为自身宽度，属性面板只有 240px 宽，
 * 选项文字稍长就会被省略成 “...”。此类在每次弹出前按最长选项重新计算宽度。
 */
class AutoWidthComboBox : public QComboBox
{
public:
    explicit AutoWidthComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
        // 控件本身不按内容撑开（否则面板会被挤爆），只填充表单可用宽度
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        setMinimumContentsLength(6);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void showPopup() override
    {
        const int desired = desiredPopupWidth();
        if (QAbstractItemView *popup = view(); popup && desired > 0) {
            // 不再把长文本省略成 “...”
            popup->setTextElideMode(Qt::ElideNone);
            popup->setMinimumWidth(desired);
        }

        QComboBox::showPopup();

        // 兜底：个别样式下容器宽度仍受控件宽度限制，弹出后再拉宽并做屏幕边界校正
        QWidget *container = view() ? view()->window() : nullptr;
        if (!container || desired <= 0 || container->width() >= desired)
            return;

        const QRect available = screen() ? screen()->availableGeometry() : QRect();
        const int popupWidth = available.isValid() ? qMin(desired, available.width() - 16) : desired;
        int x = container->x();
        if (available.isValid() && x + popupWidth > available.right())
            x = qMax(available.left(), available.right() - popupWidth);
        container->setGeometry(x, container->y(), popupWidth, container->height());
    }

private:
    /** 期望的弹出列表宽度：按最长选项文本计算，且不小于控件自身宽度 */
    int desiredPopupWidth() const
    {
        const QFontMetrics fm(font());
        int maxTextWidth = 0;
        for (int i = 0; i < count(); ++i)
            maxTextWidth = qMax(maxTextWidth, fm.horizontalAdvance(itemText(i)));
        if (maxTextWidth <= 0)
            return 0;

        // frameWidth() / style() 属于 QFrame，必须拿到 QAbstractItemView 本身的类型
        QAbstractItemView *popup = view();
        const int frame = popup ? 2 * popup->frameWidth() : 0;
        const int scrollBar = popup ? popup->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, popup) : 0;

        // 预留：左右边框 + 可能的垂直滚动条 + 文本内边距
        return qMax(width(), maxTextWidth + frame + scrollBar + 12);
    }
};
}

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PropertiesPanel)
{
    ui->setupUi(this);
    m_form = ui->formLayout;

    auto *hint = new QLabel(QStringLiteral("  (未选中节点)"), this);
    hint->setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    hint->setStyleSheet(QStringLiteral("color: #888888; padding: 16px;"));
    m_form->addRow(hint);
}

PropertiesPanel::~PropertiesPanel()
{
    delete ui;
}

void PropertiesPanel::clearForm()
{
    m_widgets.clear();
    while (m_form->count() > 0) {
        QLayoutItem *item = m_form->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void PropertiesPanel::clear()
{
    setNode(nullptr);
}

void PropertiesPanel::setNode(OVP::NodeItem *node)
{
    m_node = node;
    clearForm();

    if (!node) {
        auto *hint = new QLabel(QStringLiteral("  (未选中节点)"), this);
        hint->setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
        hint->setStyleSheet(QStringLiteral("color: #888888; padding: 16px;"));
        m_form->addRow(hint);
        return;
    }

    OVP::PluginBase *plugin = node->plugin();
    const QList<OVP::ParamDef> params = plugin ? plugin->inputParams() : QList<OVP::ParamDef>();

    if (params.isEmpty()) {
        auto *label = new QLabel(QStringLiteral("  工具: %1\n  无参数").arg(node->pluginName()), this);
        label->setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
        label->setStyleSheet(QStringLiteral("color: #aaaaaa; padding: 8px;"));
        m_form->addRow(label);
        return;
    }

    auto *nameLabel = new QLabel(QStringLiteral("  %1").arg(node->pluginName()), this);
    nameLabel->setFont(QFont(QStringLiteral("Microsoft YaHei"), 10, QFont::Bold));
    nameLabel->setStyleSheet(QStringLiteral("color: #4fc3f7; padding: 4px 0 8px 0;"));
    m_form->addRow(nameLabel);

    for (const OVP::ParamDef &def : params)
        addParamWidget(def);
}

void PropertiesPanel::addParamWidget(const OVP::ParamDef &def)
{
    if (!m_node || !m_node->plugin())
        return;

    QVariant current = m_node->plugin()->param(def.name);
    if (!current.isValid())
        current = def.defaultValue;

    QWidget *editor = nullptr;

    switch (def.type) {
    case OVP::ParamType::Int: {
        auto *spin = new QSpinBox(this);
        if (def.hasRange) {
            spin->setRange(static_cast<int>(def.minValue), static_cast<int>(def.maxValue));
        } else {
            spin->setRange(-1000000, 1000000);
        }
        spin->setSingleStep(def.step > 0 ? static_cast<int>(def.step) : 1);
        spin->setValue(current.toInt());
        connect(spin, &QSpinBox::valueChanged, this,
                [this, def](int v) { onValueChanged(def.name, v); });
        editor = spin;
        break;
    }
    case OVP::ParamType::Float: {
        auto *spin = new QDoubleSpinBox(this);
        spin->setDecimals(3);
        if (def.hasRange)
            spin->setRange(def.minValue, def.maxValue);
        else
            spin->setRange(-99999.0, 99999.0);
        spin->setSingleStep(def.step > 0 ? def.step : 0.1);
        spin->setValue(current.toDouble());
        connect(spin, &QDoubleSpinBox::valueChanged, this,
                [this, def](double v) { onValueChanged(def.name, v); });
        editor = spin;
        break;
    }
    case OVP::ParamType::Slider: {
        auto *container = new QWidget(this);
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *slider = new QSlider(Qt::Horizontal, container);
        slider->setRange(static_cast<int>(def.minValue), static_cast<int>(def.maxValue));
        slider->setValue(current.toInt());
        auto *valueLabel = new QLabel(QString::number(slider->value()), container);
        valueLabel->setStyleSheet(QStringLiteral("color: #cccccc; min-width: 30px;"));
        valueLabel->setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
        connect(slider, &QSlider::valueChanged, this, [valueLabel](int v) {
            valueLabel->setText(QString::number(v));
        });
        connect(slider, &QSlider::valueChanged, this,
                [this, def](int v) { onValueChanged(def.name, v); });
        layout->addWidget(slider, 1);
        layout->addWidget(valueLabel, 0);
        m_widgets.insert(def.name + QStringLiteral("_slider"), slider);
        editor = container;
        break;
    }
    case OVP::ParamType::Bool: {
        auto *check = new QCheckBox(this);
        check->setChecked(current.toBool());
        connect(check, &QCheckBox::toggled, this,
                [this, def](bool v) { onValueChanged(def.name, v); });
        editor = check;
        break;
    }
    case OVP::ParamType::Choice: {
        auto *combo = new AutoWidthComboBox(this);
        combo->addItems(def.choices);
        combo->setCurrentText(current.toString());
        combo->setToolTip(def.description.isEmpty() ? def.displayName : def.description);
        connect(combo, &QComboBox::currentTextChanged, this,
                [this, def](const QString &v) { onValueChanged(def.name, v); });
        editor = combo;
        break;
    }
    case OVP::ParamType::File: {
        auto *container = new QWidget(this);
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *lineEdit = new QLineEdit(container);
        lineEdit->setReadOnly(true);
        lineEdit->setStyleSheet(
            QStringLiteral("background: #333333; color: #cccccc; border: 1px solid #555555; padding: 2px;"));
        lineEdit->setText(current.toString());
        auto *button = new QPushButton(QStringLiteral("..."), container);
        button->setMaximumWidth(30);
        button->setStyleSheet(
            QStringLiteral("QPushButton { background: #3e3e42; color: #cccccc; border: 1px solid #555555; }"
                           "QPushButton:hover { background: #505050; }"));
        connect(button, &QPushButton::clicked, this,
                [this, lineEdit, def]() { browseFile(lineEdit, def.name); });
        layout->addWidget(lineEdit, 1);
        layout->addWidget(button, 0);
        m_widgets.insert(def.name + QStringLiteral("_lineedit"), lineEdit);
        editor = container;
        break;
    }
    case OVP::ParamType::String: {
        auto *lineEdit = new QLineEdit(this);
        lineEdit->setText(current.toString());
        connect(lineEdit, &QLineEdit::textChanged, this,
                [this, def](const QString &v) { onValueChanged(def.name, v); });
        editor = lineEdit;
        break;
    }
    }

    if (!editor)
        return;

    editor->setStyleSheet(editor->styleSheet() + QString::fromUtf8(kEditorStyle));

    auto *label = new QLabel(def.displayName, this);
    label->setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    label->setStyleSheet(QStringLiteral("color: #aaaaaa;"));
    if (!def.description.isEmpty())
        label->setToolTip(def.description);

    m_form->addRow(label, editor);
    m_widgets.insert(def.name, editor);
}

void PropertiesPanel::browseFile(QLineEdit *lineEdit, const QString &paramName)
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择文件"), QString(),
        QStringLiteral("图片文件 (*.png *.jpg *.bmp *.tiff *.tif);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    lineEdit->setText(path);
    onValueChanged(paramName, path);
}

void PropertiesPanel::onValueChanged(const QString &paramName, const QVariant &value)
{
    if (!m_node || !m_node->plugin())
        return;
    m_node->plugin()->setParam(paramName, value);
    emit paramChanged(m_node->nodeId(), paramName, value);
}
