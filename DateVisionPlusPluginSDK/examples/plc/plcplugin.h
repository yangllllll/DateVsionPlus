#ifndef PLCPLUGIN_H
#define PLCPLUGIN_H

// ---- 主程序 SDK ----
#include "pluginbase.h"
#include "plugininterface.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

// ---- snap7 封装 ----
#include "snap7client.h"

class QDialog;
class QWidget;

namespace OVP {

/**
 * @brief PLC 传值插件（对应 Python 版 user_plugins/plc.py 的 PLC_SendPlugin）
 *
 * 行为与 Python 版一致：
 *   - 输入布尔值 input：True -> 向存储区写 1，False（或空）-> 写 0；
 *   - 输出布尔值 status：PLC 连接状态；
 *   - 默认写入 M 区第 100 个字节（Python: write_area(Area.MK, 0, 100, ...)，rack=0, slot=1）；
 *   - 参数「执行后断开」默认开启，与 Python 版每次写完即 disconnect 的行为一致，
 *     取消勾选可复用连接，显著提升连续运行时的速度。
 *
 * 在 Python 版基础上补充：区域 / DB 块号 / 字节地址 / 机架号 / 插槽号可配置，
 * 并提供一个调试对话框（连接、写 0/1、读回当前字节值）。
 */
class PLCSendPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit PLCSendPlugin(QObject *parent = nullptr);
    ~PLCSendPlugin() override;

    QString id() const override { return QStringLiteral("plc_send"); }
    QString name() const override { return QStringLiteral("PLC传值"); }
    QString category() const override { return QStringLiteral("PLC控制"); }
    QString description() const override { return QStringLiteral("将检测结果发送到PLC"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- 供调试对话框使用 ----
    Snap7::Snap7Client *client() { return &m_plc; }
    QString ip() const { return paramString(QStringLiteral("ip"), QStringLiteral("192.168.8.40")); }
    int rack() const { return paramInt(QStringLiteral("rack"), 0); }
    int slot() const { return paramInt(QStringLiteral("slot"), 1); }
    int areaCode() const { return Snap7::areaFromName(paramString(QStringLiteral("area"), QStringLiteral("MK"))); }
    int dbNumber() const { return paramInt(QStringLiteral("db"), 0); }
    int startByte() const { return paramInt(QStringLiteral("address"), 100); }
    bool disconnectAfterWrite() const
    {
        return paramBool(QStringLiteral("disconnect_after_write"), true);
    }

private:
    /** 连接并把 value 写入目标地址 */
    bool writeResult(quint8 value, QString *error = nullptr);

    Snap7::Snap7Client m_plc;
};

} // namespace OVP

/**
 * @brief 插件提供者：一个 DLL 可导出多个工具
 */
class PLCPluginProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)

public:
    QList<Entry> availablePlugins() const override;
};

#endif // PLCPLUGIN_H
