#ifndef SAMPLEPLUGINS_H
#define SAMPLEPLUGINS_H

// ---- 主程序 SDK ----
#include "pluginbase.h"
#include "plugininterface.h"

#include <QDialog>
#include <QList>
#include <QObject>
#include <QString>

namespace OVP {

/**
 * @brief 图像翻转（带专用对话框，对应 Python 的 FlipPlugin）
 *
 * 重写 createDialog()，双击流程图节点即可打开翻转模式设置对话框。
 */
class FlipPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit FlipPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("user_flip"); }
    QString name() const override { return QStringLiteral("图像翻转"); }
    QString category() const override { return QStringLiteral("用户插件"); }
    QString description() const override
    {
        return QStringLiteral("对图像进行水平/垂直翻转（双击节点打开对话框）");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    /** 翻转模式字符串 -> cv::flip 的 flipCode */
    static int flipCode(const QString &mode);
    static QStringList flipModes();

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;
};

/** 图像反色（对应 Python 的 InvertPlugin） */
class InvertPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit InvertPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("user_invert"); }
    QString name() const override { return QStringLiteral("图像反色"); }
    QString category() const override { return QStringLiteral("用户插件"); }
    QString description() const override { return QStringLiteral("对图像进行反色处理"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;

    bool execute() override;
};

/** 数字比较（对应 Python 的 NumberPlugin） */
class NumberPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit NumberPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("user_number"); }
    QString name() const override { return QStringLiteral("比较"); }
    QString category() const override { return QStringLiteral("数学运算"); }
    QString description() const override { return QStringLiteral("对数字进行比较"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;
};

/** 逻辑与（对应 Python 的 AndPlugin） */
class AndPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit AndPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("user_and"); }
    QString name() const override { return QStringLiteral("逻辑与"); }
    QString category() const override { return QStringLiteral("数学运算"); }
    QString description() const override { return QStringLiteral("对两个布尔值进行逻辑与操作"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;

    bool execute() override;
};

} // namespace OVP

/**
 * @brief 插件提供者：一个 DLL 导出多个工具
 *
 * 关键点：
 *   - 必须 Q_OBJECT + Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
 *   - 必须 Q_INTERFACES(OpenVisionPluginProvider)
 *   - availablePlugins() 返回每个工具的创建函数（函数指针，跨 DLL 安全）
 */
class SamplePluginProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)

public:
    QList<Entry> availablePlugins() const override;
};

#endif // SAMPLEPLUGINS_H
