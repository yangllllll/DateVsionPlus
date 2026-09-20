#ifndef LINEFINDERPLUGIN_H
#define LINEFINDERPLUGIN_H

#include "../core/pluginbase.h"

#include <QList>
#include <QRect>
#include <QVector>
#include <QVariantMap>

namespace OVP {

/** 线查找：基于亮度梯度在 ROI 内检测直线，支持自动学习色差 */
class LineFinderPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit LineFinderPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("line_finder"); }
    QString name() const override { return QStringLiteral("线查找"); }
    QString category() const override { return QStringLiteral("检测定位"); }
    QString description() const override
    {
        return QStringLiteral("基于亮度色阶在ROI区域内检测直线，支持自动学习色差，返回单条最优线段");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- ROI 与学习 ----
    QList<QRect> rois() const { return m_rois; }
    void setRois(const QList<QRect> &rois) { m_rois = rois; }

    QList<QVector<double>> detectedLines() const { return m_detectedLines; }

    /** 自动学习，返回 {x1, y1, x2, y2, edgeT1, edgeT2}；ok 为 false 表示失败 */
    QVector<double> autoLearn(const cv::Mat &image, const QRect &roi, bool *ok = nullptr);

    QVector<double> learnedLine() const { return m_learnedLine; }
    QVector<int> learnedThresholds() const { return m_learnedThresholds; }

    QVariantMap extraData() const override;
    void setExtraData(const QVariantMap &data) override;

private:
    QList<QRect> m_rois;
    QList<QVector<double>> m_detectedLines;
    QVector<double> m_learnedLine;
    QVector<int> m_learnedThresholds;
};

} // namespace OVP

#endif // LINEFINDERPLUGIN_H
