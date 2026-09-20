#include "pluginbase.h"

namespace OVP {

PluginBase::PluginBase(QObject *parent)
    : QObject(parent)
{
}

PluginBase::~PluginBase() = default;

void PluginBase::setParam(const QString &name, const QVariant &value)
{
    m_params.insert(name, value);
}

QVariant PluginBase::param(const QString &name) const
{
    auto it = m_params.constFind(name);
    if (it != m_params.constEnd())
        return it.value();

    for (const ParamDef &def : inputParams()) {
        if (def.name == name)
            return def.defaultValue;
    }
    return QVariant();
}

int PluginBase::paramInt(const QString &name, int defaultValue) const
{
    const QVariant v = param(name);
    return v.isValid() ? v.toInt() : defaultValue;
}

double PluginBase::paramDouble(const QString &name, double defaultValue) const
{
    const QVariant v = param(name);
    return v.isValid() ? v.toDouble() : defaultValue;
}

QString PluginBase::paramString(const QString &name, const QString &defaultValue) const
{
    const QVariant v = param(name);
    return v.isValid() ? v.toString() : defaultValue;
}

bool PluginBase::paramBool(const QString &name, bool defaultValue) const
{
    const QVariant v = param(name);
    return v.isValid() ? v.toBool() : defaultValue;
}

QVariantMap PluginBase::params() const
{
    QVariantMap result;
    for (const ParamDef &def : inputParams())
        result.insert(def.name, param(def.name));
    for (auto it = m_params.constBegin(); it != m_params.constEnd(); ++it)
        result.insert(it.key(), it.value());
    return result;
}

void PluginBase::setParams(const QVariantMap &values)
{
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
        m_params.insert(it.key(), it.value());
}

void PluginBase::setInput(const QString &port, const QVariant &value)
{
    m_inputs.insert(port, value);
}

QVariant PluginBase::input(const QString &port) const
{
    return m_inputs.value(port);
}

cv::Mat PluginBase::inputImage(const QString &port) const
{
    const QVariant v = m_inputs.value(port);
    if (!v.isValid() || !v.canConvert<cv::Mat>())
        return cv::Mat();
    return v.value<cv::Mat>();
}

void PluginBase::setOutput(const QString &port, const QVariant &value)
{
    m_outputs.insert(port, value);
}

QVariant PluginBase::output(const QString &port) const
{
    return m_outputs.value(port);
}

QVariantMap PluginBase::outputs() const
{
    return m_outputs;
}

void PluginBase::reset()
{
    m_inputs.clear();
    m_outputs.clear();
}

QDialog *PluginBase::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    Q_UNUSED(inputImage);
    Q_UNUSED(parent);
    return nullptr;
}

} // namespace OVP
