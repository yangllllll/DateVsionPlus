#include "plcplugin.h"

#include "plcdialog.h"

#include <QStringList>

namespace OVP {

static PluginBase *createPLCSendPlugin(QObject *parent)
{
    return new PLCSendPlugin(parent);
}

PLCSendPlugin::PLCSendPlugin(QObject *parent)
    : PluginBase(parent)
{
}

PLCSendPlugin::~PLCSendPlugin() = default;

QList<PortDef> PLCSendPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Bool, QStringLiteral("输入布尔值"))};
}

QList<PortDef> PLCSendPlugin::outputPorts() const
{
    return {port(QStringLiteral("status"), PortType::Bool, QStringLiteral("输出状态"))};
}

QList<ParamDef> PLCSendPlugin::inputParams() const
{
    return {
        stringParam(QStringLiteral("ip"), QStringLiteral("PLC地址"), QStringLiteral("192.168.8.40"),
                    QStringLiteral("PLC IP 地址")),
        intParam(QStringLiteral("rack"), QStringLiteral("机架号"), 0, 0, 7, 1,
                 QStringLiteral("Rack（Python 版固定为 0）")),
        intParam(QStringLiteral("slot"), QStringLiteral("插槽号"), 1, 0, 31, 1,
                 QStringLiteral("Slot（Python 版固定为 1）")),
        choiceParam(QStringLiteral("area"), QStringLiteral("区域"), QStringLiteral("MK"),
                    QStringList{QStringLiteral("MK"), QStringLiteral("DB"), QStringLiteral("PA"),
                                QStringLiteral("PE")},
                    QStringLiteral("写入的 PLC 存储区，对应 snap7.Area（MK=M, DB=数据块）")),
        intParam(QStringLiteral("db"), QStringLiteral("DB块号"), 0, 0, 65535, 1,
                 QStringLiteral("区域为 DB 时有效（Python 版固定为 0）")),
        intParam(QStringLiteral("address"), QStringLiteral("字节地址"), 100, 0, 65535, 1,
                 QStringLiteral("写入的起始字节地址（Python 版固定为 100）")),
        boolParam(QStringLiteral("disconnect_after_write"), QStringLiteral("执行后断开"), true,
                  QStringLiteral("与 Python 版一致：写完立即断开；取消勾选可复用连接以提升速度"))
    };
}

bool PLCSendPlugin::writeResult(quint8 value, QString *error)
{
    if (!m_plc.connectTo(ip(), rack(), slot(), error))
        return false;

    return m_plc.writeByte(areaCode(), dbNumber(), startByte(), value, error);
}

bool PLCSendPlugin::execute()
{
    clearError();

    // Python: vision = self._inputs.get("input"); if vision == True: 写 1 else 写 0
    const QVariant visionValue = input(QStringLiteral("input"));
    const bool vision = visionValue.isValid() && visionValue.toBool();

    QString error;
    const bool connected = m_plc.connectTo(ip(), rack(), slot(), &error);

    // Python: 连接成功写 status=True，否则 False
    setOutput(QStringLiteral("status"), connected);

    if (!connected) {
        setError(error.isEmpty() ? QStringLiteral("PLC 连接失败") : error);
        return false;
    }

    const bool written = m_plc.writeByte(areaCode(), dbNumber(), startByte(), vision ? 1 : 0, &error);
    if (!written)
        setError(error);

    if (disconnectAfterWrite())
        m_plc.disconnect();

    return written;
}

QDialog *PLCSendPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    Q_UNUSED(inputImage);
    return new PLCDialog(this, parent);
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> PLCPluginProvider::availablePlugins() const
{
    return {OpenVisionPluginProvider::Entry{OVP::createPLCSendPlugin}};
}
