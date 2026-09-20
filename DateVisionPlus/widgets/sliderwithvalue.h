#ifndef SLIDERWITHVALUE_H
#define SLIDERWITHVALUE_H

#include <QWidget>

class QLabel;
class QSlider;

/** 滑条 + 数值标签的组合控件（可在 Qt Designer 中作为自定义控件使用） */
class SliderWithValue : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit SliderWithValue(QWidget *parent = nullptr);

    int minimum() const;
    void setMinimum(int value);
    int maximum() const;
    void setMaximum(int value);
    void setRange(int min, int max);
    int value() const;

public slots:
    void setValue(int value);

signals:
    void valueChanged(int value);

private:
    void syncLabel();

    QSlider *m_slider = nullptr;
    QLabel *m_label = nullptr;
};

#endif // SLIDERWITHVALUE_H
