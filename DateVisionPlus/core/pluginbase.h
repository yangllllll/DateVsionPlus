#ifndef PLUGINBASE_H
#define PLUGINBASE_H

#include "coreglobal.h"
#include "plugintypes.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

class QDialog;
class QWidget;

namespace OVP {

/**
 * @brief 视觉工具插件基类
 *
 * 所有内置工具 / 用户扩展插件都必须继承此类，并实现 execute()。
 * 端口数据通过 QVariant 传递，图像端口使用 QVariant::fromValue(cv::Mat)。
 */
class OPVCORE_EXPORT PluginBase : public QObject
{
    Q_OBJECT

public:
    explicit PluginBase(QObject *parent = nullptr);
    ~PluginBase() override;

    /** 唯一标识（保存项目时使用） */
    virtual QString id() const = 0;
    /** 显示名称 */
    virtual QString name() const = 0;
    /** 工具箱分类 */
    virtual QString category() const = 0;
    virtual QString description() const { return QString(); }
    virtual QString version() const { return QStringLiteral("1.0.0"); }

    virtual QList<PortDef> inputPorts() const { return {}; }
    virtual QList<PortDef> outputPorts() const { return {}; }
    virtual QList<ParamDef> inputParams() const { return {}; }

    // ---- 参数 ----
    void setParam(const QString &name, const QVariant &value);
    QVariant param(const QString &name) const;
    int paramInt(const QString &name, int defaultValue = 0) const;
    double paramDouble(const QString &name, double defaultValue = 0.0) const;
    QString paramString(const QString &name, const QString &defaultValue = QString()) const;
    bool paramBool(const QString &name, bool defaultValue = false) const;

    /** 所有声明参数的当前值（含默认值） */
    QVariantMap params() const;
    void setParams(const QVariantMap &values);

    // ---- 端口数据 ----
    void setInput(const QString &port, const QVariant &value);
    QVariant input(const QString &port) const;
    cv::Mat inputImage(const QString &port = QStringLiteral("input")) const;

    void setOutput(const QString &port, const QVariant &value);
    QVariant output(const QString &port) const;
    QVariantMap outputs() const;

    // ---- 执行 ----
    virtual bool execute() = 0;
    virtual void reset();

    /** 插件私有数据序列化（ROI、模板等），用于项目保存 / 加载 */
    virtual QVariantMap extraData() const { return {}; }
    virtual void setExtraData(const QVariantMap &data) { Q_UNUSED(data); }

    QString lastError() const { return m_lastError; }
    void clearError() { m_lastError.clear(); }

    /**
     * @brief 双击节点时创建专用编辑对话框
     * @param inputImage 上游节点产出的输入图像（可能为空）
     * @param parent 父窗口
     * @return 返回 nullptr 表示该工具没有专用对话框（内置与用户插件均适用）
     */
    virtual QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent);

protected:
    void setError(const QString &message) { m_lastError = message; }

    QVariantMap m_params;
    QVariantMap m_inputs;
    QVariantMap m_outputs;
    QString m_lastError;
};

} // namespace OVP

#endif // PLUGINBASE_H
