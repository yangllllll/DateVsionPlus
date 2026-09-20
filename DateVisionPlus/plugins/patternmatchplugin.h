#ifndef PATTERNMATCHPLUGIN_H
#define PATTERNMATCHPLUGIN_H

#include "../core/pluginbase.h"

#include <QRect>

#include <opencv2/core.hpp>

namespace OVP {

/** 模板匹配：ROI 框选训练模板并匹配（模板随项目保存） */
class PatternMatchPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit PatternMatchPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("pattern_match"); }
    QString name() const override { return QStringLiteral("模板匹配"); }
    QString category() const override { return QStringLiteral("检测定位"); }
    QString description() const override
    {
        return QStringLiteral("在图像中框选ROI训练模板并匹配");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    /** 从 ROI 提取模板（灰度），失败返回空 Mat */
    cv::Mat trainTemplate(const cv::Mat &image, const QRect &roi);

    cv::Mat templateImage() const { return m_templateImage; }
    QRect templateRoi() const { return m_templateRoi; }

    QVariantMap extraData() const override;
    void setExtraData(const QVariantMap &data) override;

private:
    QRect m_templateRoi;
    cv::Mat m_templateImage;
};

} // namespace OVP

#endif // PATTERNMATCHPLUGIN_H
