#include "plugintypes.h"

namespace OVP {

PortDef port(const QString &name, PortType type, const QString &description)
{
    return PortDef{name, type, description};
}

QString portTypeKey(PortType type)
{
    switch (type) {
    case PortType::Image: return QStringLiteral("image");
    case PortType::Region: return QStringLiteral("region");
    case PortType::Number: return QStringLiteral("number");
    case PortType::String: return QStringLiteral("string");
    case PortType::Point: return QStringLiteral("point");
    case PortType::Matrix: return QStringLiteral("matrix");
    case PortType::Bool: return QStringLiteral("bool");
    case PortType::Any: return QStringLiteral("any");
    }
    return QStringLiteral("any");
}

QColor portTypeColor(PortType type)
{
    switch (type) {
    case PortType::Image: return QColor(0, 200, 80);
    case PortType::Region: return QColor(0, 140, 255);
    case PortType::Number: return QColor(255, 160, 0);
    case PortType::String: return QColor(255, 220, 0);
    case PortType::Point: return QColor(0, 200, 200);
    case PortType::Matrix: return QColor(255, 60, 60);
    case PortType::Bool: return QColor(200, 60, 200);
    case PortType::Any: return QColor(160, 160, 160);
    }
    return QColor(128, 128, 128);
}

ParamDef intParam(const QString &name, const QString &display, int def, int min, int max,
                  int step, const QString &desc)
{
    ParamDef p;
    p.name = name;
    p.displayName = display;
    p.type = ParamType::Int;
    p.defaultValue = def;
    p.minValue = min;
    p.maxValue = max;
    p.step = step;
    p.hasRange = true;
    p.description = desc;
    return p;
}

ParamDef floatParam(const QString &name, const QString &display, double def, double min,
                    double max, double step, const QString &desc)
{
    ParamDef p;
    p.name = name;
    p.displayName = display;
    p.type = ParamType::Float;
    p.defaultValue = def;
    p.minValue = min;
    p.maxValue = max;
    p.step = step;
    p.hasRange = true;
    p.description = desc;
    return p;
}

ParamDef sliderParam(const QString &name, const QString &display, int def, int min, int max,
                     int step, const QString &desc)
{
    ParamDef p;
    p.name = name;
    p.displayName = display;
    p.type = ParamType::Slider;
    p.defaultValue = def;
    p.minValue = min;
    p.maxValue = max;
    p.step = step;
    p.hasRange = true;
    p.description = desc;
    return p;
}

ParamDef boolParam(const QString &name, const QString &display, bool def, const QString &desc)
{
    ParamDef p;
    p.name = name;
    p.displayName = display;
    p.type = ParamType::Bool;
    p.defaultValue = def;
    p.description = desc;
    return p;
}

ParamDef choiceParam(const QString &name, const QString &display, const QString &def,
                     const QStringList &choices, const QString &desc)
{
    ParamDef p;
    p.name = name;
    p.displayName = display;
    p.type = ParamType::Choice;
    p.defaultValue = def;
    p.choices = choices;
    p.description = desc;
    return p;
}

ParamDef stringParam(const QString &name, const QString &display, const QString &def,
                     const QString &desc)
{
    ParamDef p;
    p.name = name;
    p.displayName = display;
    p.type = ParamType::String;
    p.defaultValue = def;
    p.description = desc;
    return p;
}

ParamDef fileParam(const QString &name, const QString &display, const QString &def,
                   const QString &desc)
{
    ParamDef p;
    p.name = name;
    p.displayName = display;
    p.type = ParamType::File;
    p.defaultValue = def;
    p.description = desc;
    return p;
}

QString paramTypeKey(ParamType type)
{
    switch (type) {
    case ParamType::Int: return QStringLiteral("int");
    case ParamType::Float: return QStringLiteral("float");
    case ParamType::String: return QStringLiteral("str");
    case ParamType::Bool: return QStringLiteral("bool");
    case ParamType::Choice: return QStringLiteral("choice");
    case ParamType::Slider: return QStringLiteral("slider");
    case ParamType::File: return QStringLiteral("file");
    }
    return QStringLiteral("str");
}

QString pluginIdMimeType()
{
    return QStringLiteral("application/x-openvision-plugin-id");
}

QVariant imageValue(const cv::Mat &mat)
{
    return QVariant::fromValue(mat);
}

cv::Mat toMat(const QVariant &value)
{
    if (!value.isValid())
        return cv::Mat();
    if (value.canConvert<cv::Mat>())
        return value.value<cv::Mat>();
    return cv::Mat();
}

QVariant packLines(const QList<QVector<double>> &lines)
{
    QVariantList result;
    for (const QVector<double> &line : lines) {
        QVariantList inner;
        for (double v : line)
            inner.append(v);
        result.append(QVariant(inner));
    }
    return result;
}

QList<QVector<double>> unpackLines(const QVariant &value)
{
    QList<QVector<double>> lines;
    if (!value.isValid() || value.metaType().id() != QMetaType::QVariantList)
        return lines;

    const QVariantList list = value.toList();
    for (const QVariant &item : list) {
        if (item.metaType().id() != QMetaType::QVariantList)
            continue;
        const QVariantList inner = item.toList();
        if (inner.size() < 4)
            continue;
        QVector<double> line(4);
        for (int i = 0; i < 4; ++i)
            line[i] = inner.at(i).toDouble();
        lines.append(line);
    }
    return lines;
}

QVariant packPoints(const QList<QPointF> &points)
{
    QVariantList result;
    for (const QPointF &p : points) {
        QVariantList inner;
        inner.append(p.x());
        inner.append(p.y());
        result.append(QVariant(inner));
    }
    return result;
}

QList<QPointF> unpackPoints(const QVariant &value)
{
    QList<QPointF> points;
    if (!value.isValid() || value.metaType().id() != QMetaType::QVariantList)
        return points;

    const QVariantList list = value.toList();
    for (const QVariant &item : list) {
        if (item.metaType().id() != QMetaType::QVariantList)
            continue;
        const QVariantList inner = item.toList();
        if (inner.size() < 2)
            continue;
        points.append(QPointF(inner.at(0).toDouble(), inner.at(1).toDouble()));
    }
    return points;
}

QString portValueToString(const QVariant &value)
{
    if (!value.isValid())
        return QStringLiteral("None");

    if (value.canConvert<cv::Mat>()) {
        const cv::Mat m = value.value<cv::Mat>();
        if (m.empty())
            return QStringLiteral("<空图像>");
        return QStringLiteral("Mat %1x%2").arg(m.cols).arg(m.rows);
    }

    switch (value.metaType().id()) {
    case QMetaType::Bool:
        return value.toBool() ? QStringLiteral("True") : QStringLiteral("False");
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return QString::number(value.toLongLong());
    case QMetaType::Double:
    case QMetaType::Float:
        return QString::number(value.toDouble(), 'f', 4);
    case QMetaType::QString:
        return value.toString();
    case QMetaType::QVariantList: {
        const QVariantList list = value.toList();
        if (list.isEmpty())
            return QStringLiteral("0");
        QStringList parts;
        for (const QVariant &item : list) {
            if (item.metaType().id() == QMetaType::QVariantList) {
                QStringList nums;
                for (const QVariant &n : item.toList())
                    nums.append(QString::number(n.toDouble(), 'f', 1));
                parts.append(QLatin1Char('(') + nums.join(QLatin1Char(',')) + QLatin1Char(')'));
            } else {
                parts.append(item.toString());
            }
        }
        return parts.join(QLatin1Char(' '));
    }
    default:
        return value.toString();
    }
}

} // namespace OVP
