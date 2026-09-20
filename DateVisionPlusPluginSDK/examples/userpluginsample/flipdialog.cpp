#include "flipdialog.h"
#include "sampleplugins.h"
#include "ui_flipdialog.h"

#include <QStringList>

FlipDialog::FlipDialog(OVP::FlipPlugin *plugin, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FlipDialog)
    , m_plugin(plugin)
    , m_modes(OVP::FlipPlugin::flipModes())
{
    ui->setupUi(this);

    // 用插件当前参数初始化
    const QString current = m_plugin ? m_plugin->paramString(QStringLiteral("flip_mode"),
                                                             m_modes.value(0))
                                     : m_modes.value(0);
    const int index = qMax(0, m_modes.indexOf(current));
    ui->sliderMode->setValue(index);
    ui->lblMode->setText(m_modes.value(index));

    connect(ui->sliderMode, &QSlider::valueChanged, this, &FlipDialog::onModeChanged);
    connect(ui->btnOk, &QPushButton::clicked, this, &FlipDialog::onOkClicked);
}

FlipDialog::~FlipDialog()
{
    delete ui;
}

void FlipDialog::onModeChanged(int value)
{
    if (value >= 0 && value < m_modes.size())
        ui->lblMode->setText(m_modes.at(value));
}

void FlipDialog::onOkClicked()
{
    const int value = ui->sliderMode->value();
    if (m_plugin && value >= 0 && value < m_modes.size())
        m_plugin->setParam(QStringLiteral("flip_mode"), m_modes.at(value));
    accept();
}
