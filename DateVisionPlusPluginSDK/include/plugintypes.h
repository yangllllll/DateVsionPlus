#ifndef PLUGINTYPES_H
#define PLUGINTYPES_H

#include "coreglobal.h"

#include <QColor>
#include <QMetaType>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVector>
#include <QList>

#include <opencv2/core.hpp>

Q_DECLARE_METATYPE(cv::Mat)

namespace OVP {

/** 端口数据类型 */
enum class PortType {
    Image,
    Region,
    Number,
    String,
    Point,
    Matrix,
    Bool,
    Any
};

/** 端口定义 */
struct PortDef
{
    QString name;
    PortType type = PortType::Any;
    QString description;
};

OPVCORE_EXPORT PortDef port(const QString &name, PortType type,
                            const QString &description = QString());

OPVCORE_EXPORT QString portTypeKey(PortType type);
OPVCORE_EXPORT QColor portTypeColor(PortType type);

/** 参数控件类型 */
enum class ParamType {
    Int,
    Float,
    String,
    Bool,
    Choice,
    Slider,
    File
};

/** 参数定义 */
struct ParamDef
{
    QString name;
    QString displayName;
    ParamType type = ParamType::String;
    QVariant defaultValue;
    double minValue = 0.0;
    double maxValue = 100.0;
    double step = 1.0;
    bool hasRange = false;
    QStringList choices;
    QString description;
};

OPVCORE_EXPORT ParamDef intParam(const QString &name, const QString &display, int def, int min,
                                 int max, int step = 1, const QString &desc = QString());
OPVCORE_EXPORT ParamDef floatParam(const QString &name, const QString &display, double def,
                                   double min, double max, double step = 0.1,
                                   const QString &desc = QString());
OPVCORE_EXPORT ParamDef sliderParam(const QString &name, const QString &display, int def, int min,
                                    int max, int step = 1, const QString &desc = QString());
OPVCORE_EXPORT ParamDef boolParam(const QString &name, const QString &display, bool def,
                                  const QString &desc = QString());
OPVCORE_EXPORT ParamDef choiceParam(const QString &name, const QString &display, const QString &def,
                                    const QStringList &choices, const QString &desc = QString());
OPVCORE_EXPORT ParamDef stringParam(const QString &name, const QString &display, const QString &def,
                                    const QString &desc = QString());
OPVCORE_EXPORT ParamDef fileParam(const QString &name, const QString &display, const QString &def,
                                  const QString &desc = QString());

OPVCORE_EXPORT QString paramTypeKey(ParamType type);

/** 工具箱拖拽到画布使用的 MIME 类型 */
OPVCORE_EXPORT QString pluginIdMimeType();

/** cv::Mat <-> QVariant */
OPVCORE_EXPORT QVariant imageValue(const cv::Mat &mat);
OPVCORE_EXPORT cv::Mat toMat(const QVariant &value);

/** 线段集合 [{x1,y1,x2,y2}, ...] 与 QVariant 互转 */
OPVCORE_EXPORT QVariant packLines(const QList<QVector<double>> &lines);
OPVCORE_EXPORT QList<QVector<double>> unpackLines(const QVariant &value);

/** 点集合 [{x,y}, ...] 与 QVariant 互转 */
OPVCORE_EXPORT QVariant packPoints(const QList<QPointF> &points);
OPVCORE_EXPORT QList<QPointF> unpackPoints(const QVariant &value);

/** 端口值的显示字符串 */
OPVCORE_EXPORT QString portValueToString(const QVariant &value);

} // namespace OVP

#endif // PLUGINTYPES_H
