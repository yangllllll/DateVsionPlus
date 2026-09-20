#include "sliderwithvalue.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

SliderWithValue::SliderWithValue(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setStyleSheet(
        QStringLiteral("QSlider::groove:horizontal { height: 4px; background: #333; }"
                       "QSlider::handle:horizontal { width: 12px; margin: -4px 0; "
                       "background: #0d7377; border-radius: 6px; }"));

    m_label = new QLabel(QStringLiteral("0"), this);
    m_label->setStyleSheet(QStringLiteral("color: #ccc; min-width: 30px; font-size: 11px;"));
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(m_slider, 1);
    layout->addWidget(m_label, 0);

    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        syncLabel();
        emit valueChanged(v);
    });
}

int SliderWithValue::minimum() const
{
    return m_slider->minimum();
}

void SliderWithValue::setMinimum(int value)
{
    m_slider->setMinimum(value);
    syncLabel();
}

int SliderWithValue::maximum() const
{
    return m_slider->maximum();
}

void SliderWithValue::setMaximum(int value)
{
    m_slider->setMaximum(value);
    syncLabel();
}

void SliderWithValue::setRange(int min, int max)
{
    m_slider->setRange(min, max);
    syncLabel();
}

int SliderWithValue::value() const
{
    return m_slider->value();
}

void SliderWithValue::setValue(int value)
{
    if (m_slider->value() == value)
        return;
    m_slider->setValue(value);
    syncLabel();
}

void SliderWithValue::syncLabel()
{
    m_label->setText(QString::number(m_slider->value()));
}
