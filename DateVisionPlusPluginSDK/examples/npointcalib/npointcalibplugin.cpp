#include "npointcalibplugin.h"
#include "npointcalibdialog.h"

#include <QVariantList>

namespace OVP {

static PluginBase *createNPointCalibPlugin(QObject *parent)
{
    return new NPointCalibPlugin(parent);
}

NPointCalibPlugin::NPointCalibPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> NPointCalibPlugin::inputPorts() const
{
    return {
        port(QStringLiteral("input"), PortType::Image, QStringLiteral("图像（可选，用于界面显示）")),
        port(QStringLiteral("detections"), PortType::Any,
             QStringLiteral("YOLO 检测结果（对话框取点用）")),
    };
}

QList<PortDef> NPointCalibPlugin::outputPorts() const
{
    return {
        port(QStringLiteral("calib_ok"), PortType::Bool, QStringLiteral("标定是否有效")),
        port(QStringLiteral("point_count"), PortType::Number, QStringLiteral("标定点数量")),
        port(QStringLiteral("rms"), PortType::Number,
             QStringLiteral("拟合残差 RMS（机器人坐标单位）")),
        port(QStringLiteral("matrix"), PortType::Any,
             QStringLiteral("变换矩阵 [a,b,c,d,e,f]：x=a*u+b*v+c, y=d*u+e*v+f")),
    };
}

QList<ParamDef> NPointCalibPlugin::inputParams() const
{
    return {
        fileParam(QStringLiteral("calib_file"), QStringLiteral("标定文件"), QString(),
                  QStringLiteral("上次保存/加载的 .ncal 标定文件路径（对话框中选择）")),
    };
}

bool NPointCalibPlugin::execute()
{
    clearError();

    QVariantList matrix;
    matrix.reserve(6);
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 3; ++column)
            matrix.append(m_calib.matrix(row, column));
    }

    setOutput(QStringLiteral("calib_ok"), m_calib.valid);
    setOutput(QStringLiteral("point_count"), static_cast<int>(m_calib.points.size()));
    setOutput(QStringLiteral("rms"), m_calib.rms);
    setOutput(QStringLiteral("matrix"), matrix);

    if (!m_calib.valid) {
        setError(QStringLiteral("标定尚未完成：至少需要 3 组有效的图像/机器人对应点，"
                                "请在对话框中计算并保存"));
        return false;
    }

    return true;
}

QDialog *NPointCalibPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    return new NPointCalibDialog(this, inputImage, parent);
}

QVariantMap NPointCalibPlugin::extraData() const
{
    return m_calib.toVariantMap();
}

void NPointCalibPlugin::setExtraData(const QVariantMap &data)
{
    CALIB::CalibData calib;
    if (calib.fromVariantMap(data)) {
        m_calib = calib;
    } else {
        m_calib = CALIB::CalibData();
    }
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> NPointCalibPluginProvider::availablePlugins() const
{
    return {OpenVisionPluginProvider::Entry{OVP::createNPointCalibPlugin}};
}
