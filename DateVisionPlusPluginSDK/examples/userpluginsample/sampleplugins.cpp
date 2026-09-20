#include "sampleplugins.h"

#include "cvutils.h"
#include "flipdialog.h"

#include "opencvcompat.h"

#include <QtGlobal>

namespace OVP {

// ------------------------------------------------------ 各工具的创建函数（工厂）

static PluginBase *createFlipPlugin(QObject *parent)
{
    return new FlipPlugin(parent);
}

static PluginBase *createInvertPlugin(QObject *parent)
{
    return new InvertPlugin(parent);
}

static PluginBase *createNumberPlugin(QObject *parent)
{
    return new NumberPlugin(parent);
}

static PluginBase *createAndPlugin(QObject *parent)
{
    return new AndPlugin(parent);
}

// ---------------------------------------------------------------- FlipPlugin

QStringList FlipPlugin::flipModes()
{
    return {QStringLiteral("水平翻转"), QStringLiteral("垂直翻转"), QStringLiteral("水平+垂直翻转")};
}

int FlipPlugin::flipCode(const QString &mode)
{
    // 对应 Python: {"水平翻转": 1, "垂直翻转": 0, "水平+垂直翻转": -1}
    if (mode == QStringLiteral("垂直翻转"))
        return 0;
    if (mode == QStringLiteral("水平+垂直翻转"))
        return -1;
    return 1;
}

FlipPlugin::FlipPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> FlipPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> FlipPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("翻转结果"))};
}

QList<ParamDef> FlipPlugin::inputParams() const
{
    return {choiceParam(QStringLiteral("flip_mode"), QStringLiteral("翻转模式"),
                        QStringLiteral("水平翻转"), flipModes(), QStringLiteral("选择翻转方向"))};
}

bool FlipPlugin::execute()
{
    clearError();

    const cv::Mat source = inputImage(QStringLiteral("input"));
    if (source.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    const QString mode = paramString(QStringLiteral("flip_mode"), QStringLiteral("水平翻转"));

    cv::Mat result;
    cv::flip(source, result, flipCode(mode));
    if (result.empty()) {
        setError(QStringLiteral("翻转失败"));
        return false;
    }

    setOutput(QStringLiteral("output"), imageValue(result));
    return true;
}

QDialog *FlipPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    Q_UNUSED(inputImage);
    // 对话框定义在本 DLL 内（flipdialog.ui / flipdialog.h）
    return new FlipDialog(this, parent);
}

// -------------------------------------------------------------- InvertPlugin

InvertPlugin::InvertPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> InvertPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> InvertPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("反色结果"))};
}

bool InvertPlugin::execute()
{
    clearError();

    const cv::Mat source = inputImage(QStringLiteral("input"));
    if (source.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    cv::Mat result;
    cv::bitwise_not(source, result);

    setOutput(QStringLiteral("output"), imageValue(result));
    return true;
}

// -------------------------------------------------------------- NumberPlugin

NumberPlugin::NumberPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> NumberPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Number, QStringLiteral("输入数字"))};
}

QList<PortDef> NumberPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Bool, QStringLiteral("比较结果"))};
}

QList<ParamDef> NumberPlugin::inputParams() const
{
    return {choiceParam(QStringLiteral("compare_mode"), QStringLiteral("比较模式"),
                        QStringLiteral("大于"),
                        {QStringLiteral("大于"), QStringLiteral("小于"), QStringLiteral("等于")},
                        QStringLiteral("选择比较模式")),
            floatParam(QStringLiteral("NUM"), QStringLiteral("数字"), 0.0, -99999.0, 99999.0, 1.0,
                       QStringLiteral("输入数字"))};
}

bool NumberPlugin::execute()
{
    clearError();

    const QVariant inputValue = input(QStringLiteral("input"));
    if (!inputValue.isValid()) {
        setError(QStringLiteral("缺少输入数字"));
        return false;
    }

    const double num1 = inputValue.toDouble();
    const double num2 = paramDouble(QStringLiteral("NUM"), 0.0);
    const QString mode = paramString(QStringLiteral("compare_mode"), QStringLiteral("大于"));

    bool result = false;
    if (mode == QStringLiteral("小于")) {
        result = num1 < num2;
    } else if (mode == QStringLiteral("等于")) {
        result = qFuzzyCompare(num1, num2) || num1 == num2;
    } else {
        result = num1 > num2;
    }

    setOutput(QStringLiteral("output"), result);
    return true;
}

// ----------------------------------------------------------------- AndPlugin

AndPlugin::AndPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> AndPlugin::inputPorts() const
{
    return {port(QStringLiteral("input1"), PortType::Bool, QStringLiteral("输入布尔值1")),
            port(QStringLiteral("input2"), PortType::Bool, QStringLiteral("输入布尔值2"))};
}

QList<PortDef> AndPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Bool, QStringLiteral("逻辑与结果"))};
}

bool AndPlugin::execute()
{
    clearError();

    const QVariant v1 = input(QStringLiteral("input1"));
    const QVariant v2 = input(QStringLiteral("input2"));

    if (!v1.isValid() || !v2.isValid()) {
        setOutput(QStringLiteral("output"), false);
        setError(QStringLiteral("缺少布尔输入"));
        return false;
    }

    const bool result = v1.toBool() && v2.toBool();
    setOutput(QStringLiteral("output"), result);

    // 与 Python 版保持一致：任一输入为 False 时判定为不通过
    if (!result)
        setError(QStringLiteral("逻辑与结果为 False"));
    return result;
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> SamplePluginProvider::availablePlugins() const
{
    return {Entry{OVP::createFlipPlugin},
            Entry{OVP::createInvertPlugin},
            Entry{OVP::createNumberPlugin},
            Entry{OVP::createAndPlugin}};
}
