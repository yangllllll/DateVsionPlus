#include "materialzoneplugin.h"
#include "materialzonedialog.h"

#include <QSet>
#include <QVariantList>

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace OVP {
namespace {

/** 解析出的单个检测目标 */
struct DetectionItem
{
    int classId = -1;
    QString className;
    float confidence = 0.0f;
    QPointF center;
};

/** 类别过滤：数字按 class_id，其它按 class_name（不区分大小写） */
struct ClassFilter
{
    bool enabled = false;
    QSet<int> ids;
    QSet<QString> names;

    bool matches(const DetectionItem &item) const
    {
        if (!enabled)
            return true;
        if (item.classId >= 0 && ids.contains(item.classId))
            return true;
        return !item.className.isEmpty() && names.contains(item.className.toLower());
    }
};

ClassFilter buildClassFilter(const QString &text)
{
    ClassFilter filter;
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return filter;
    trimmed.replace(QStringLiteral("，"), QStringLiteral(","));

    const QStringList tokens = trimmed.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString token : tokens) {
        token = token.trimmed();
        if (token.isEmpty())
            continue;
        bool ok = false;
        const int id = token.toInt(&ok);
        if (ok)
            filter.ids.insert(id);
        else
            filter.names.insert(token.toLower());
    }
    filter.enabled = !filter.ids.isEmpty() || !filter.names.isEmpty();
    return filter;
}

/**
 * @brief 解析 YOLO 的 detections 输出
 *
 * 支持 QVariantList<QVariantMap>，字段兼容本项目 YOLO 插件与 Python 版：
 *   center_x/center_y、center[2]、x1..y2、xyxy[4]
 */
bool readDetections(const QVariant &value, QList<DetectionItem> &items, QString *error)
{
    items.clear();

    if (!value.canConvert<QVariantList>()) {
        if (error)
            *error = QStringLiteral("detections 输入不是检测列表，请连接 YOLO目标识别的 detections 输出");
        return false;
    }

    const QVariantList list = value.toList();
    int withCenter = 0;

    for (const QVariant &entry : list) {
        const QVariantMap map = entry.toMap();
        if (map.isEmpty())
            continue;

        DetectionItem item;
        item.classId = map.value(QStringLiteral("class_id"), -1).toInt();
        item.className = map.value(QStringLiteral("class_name")).toString();
        item.confidence = map.value(QStringLiteral("confidence"), 0.0).toFloat();

        bool hasCenter = false;
        if (map.contains(QStringLiteral("center_x")) && map.contains(QStringLiteral("center_y"))) {
            item.center = QPointF(map.value(QStringLiteral("center_x")).toDouble(),
                                  map.value(QStringLiteral("center_y")).toDouble());
            hasCenter = true;
        } else if (map.contains(QStringLiteral("center"))) {
            const QVariantList center = map.value(QStringLiteral("center")).toList();
            if (center.size() >= 2) {
                item.center = QPointF(center.at(0).toDouble(), center.at(1).toDouble());
                hasCenter = true;
            }
        }

        if (!hasCenter) {
            double x1 = 0.0;
            double y1 = 0.0;
            double x2 = 0.0;
            double y2 = 0.0;
            bool hasBox = false;

            if (map.contains(QStringLiteral("x1")) && map.contains(QStringLiteral("y1"))
                && map.contains(QStringLiteral("x2")) && map.contains(QStringLiteral("y2"))) {
                x1 = map.value(QStringLiteral("x1")).toDouble();
                y1 = map.value(QStringLiteral("y1")).toDouble();
                x2 = map.value(QStringLiteral("x2")).toDouble();
                y2 = map.value(QStringLiteral("y2")).toDouble();
                hasBox = true;
            } else if (map.contains(QStringLiteral("xyxy"))) {
                const QVariantList box = map.value(QStringLiteral("xyxy")).toList();
                if (box.size() >= 4) {
                    x1 = box.at(0).toDouble();
                    y1 = box.at(1).toDouble();
                    x2 = box.at(2).toDouble();
                    y2 = box.at(3).toDouble();
                    hasBox = true;
                }
            }

            if (hasBox) {
                item.center = QPointF((x1 + x2) * 0.5, (y1 + y2) * 0.5);
                hasCenter = true;
            }
        }

        if (!hasCenter)
            continue;

        ++withCenter;
        items.append(item);
    }

    // 列表非空却一个都解析不出来，说明格式不对
    if (!list.isEmpty() && withCenter == 0) {
        if (error) {
            *error = QStringLiteral("检测结果格式不正确：需要包含 center/center_x/xyxy 字段");
        }
        return false;
    }

    return true;
}

cv::Rect zonePixelRect(const Zone &zone, const cv::Size &size)
{
    const int x = std::max(0, std::min(size.width - 1, cvRound(zone.rect.x * size.width)));
    const int y = std::max(0, std::min(size.height - 1, cvRound(zone.rect.y * size.height)));
    const int w = std::max(1, cvRound(zone.rect.width * size.width));
    const int h = std::max(1, cvRound(zone.rect.height * size.height));
    return cv::Rect(x, y, std::min(w, size.width - x), std::min(h, size.height - y));
}

PluginBase *createMaterialZonePlugin(QObject *parent)
{
    return new MaterialZonePlugin(parent);
}

} // namespace

// -------------------------------------------------------- MaterialZonePlugin

MaterialZonePlugin::MaterialZonePlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> MaterialZonePlugin::inputPorts() const
{
    return {
        port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像（用于坐标换算与显示）")),
        port(QStringLiteral("detections"), PortType::Any, QStringLiteral("YOLO 检测结果")),
    };
}

QList<PortDef> MaterialZonePlugin::outputPorts() const
{
    return {
        port(QStringLiteral("output"), PortType::Image, QStringLiteral("标注图像")),
        port(QStringLiteral("zone_id"), PortType::Number,
             QStringLiteral("存在物料的检测区编号中最小值（-1 = 无）")),
        port(QStringLiteral("present"), PortType::Bool, QStringLiteral("检测区内是否有物料")),
        port(QStringLiteral("object_count"), PortType::Number, QStringLiteral("落在检测区内的物体数量")),
        port(QStringLiteral("zone_hits"), PortType::Any,
             QStringLiteral("各检测区命中数量 [{zone_id,count},...]")),
    };
}

QList<ParamDef> MaterialZonePlugin::inputParams() const
{
    return {
        floatParam(QStringLiteral("min_score"), QStringLiteral("最小置信度"), 0.0, 0.0, 1.0, 0.01,
                   QStringLiteral("低于该置信度的检测结果不参与判断")),
        stringParam(QStringLiteral("class_filter"), QStringLiteral("类别过滤"), QString(),
                    QStringLiteral("只统计指定类别，填类别 id 或名称，逗号分隔；留空表示全部")),
        boolParam(QStringLiteral("draw"), QStringLiteral("绘制检测区"), true,
                  QStringLiteral("在输出图像上绘制检测区、命中的物体中心")),
        intParam(QStringLiteral("line_thickness"), QStringLiteral("线宽"), 2, 1, 5, 1,
                 QStringLiteral("检测区线宽")),
        boolParam(QStringLiteral("empty_as_ng"), QStringLiteral("无物料判NG"), false,
                  QStringLiteral("勾选后，没有任何检测区命中时节点返回失败(NG)")),
    };
}

bool MaterialZonePlugin::execute()
{
    clearError();

    const QVariant detValue = input(QStringLiteral("detections"));
    if (!detValue.isValid()) {
        setError(QStringLiteral("没有检测结果输入，请连接 YOLO目标识别的 detections 输出"));
        return false;
    }

    QList<DetectionItem> detections;
    QString parseError;
    if (!readDetections(detValue, detections, &parseError)) {
        setError(parseError);
        return false;
    }

    const float minScore = static_cast<float>(paramDouble(QStringLiteral("min_score"), 0.0));
    const ClassFilter filter = buildClassFilter(paramString(QStringLiteral("class_filter")));

    QList<DetectionItem> filtered;
    filtered.reserve(detections.size());
    for (const DetectionItem &item : detections) {
        if (item.confidence < minScore)
            continue;
        if (!filter.matches(item))
            continue;
        filtered.append(item);
    }

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (!m_zones.isEmpty() && image.empty()) {
        setError(QStringLiteral("没有输入图像，无法把检测区坐标换算成像素（请连接图像输入）"));
        return false;
    }

    // ---- 逐个检测区统计命中 ----
    QList<int> counts;
    QList<bool> hitFlags(filtered.size(), false);
    counts.reserve(m_zones.size());

    int bestZoneId = -1;
    for (const Zone &zone : m_zones) {
        const cv::Rect rect = zonePixelRect(zone, image.size());
        int count = 0;
        for (int i = 0; i < filtered.size(); ++i) {
            const QPointF &center = filtered.at(i).center;
            if (rect.contains(cv::Point(cvRound(center.x()), cvRound(center.y())))) {
                ++count;
                hitFlags[i] = true;
            }
        }
        counts.append(count);
        if (count > 0 && (bestZoneId < 0 || zone.id < bestZoneId))
            bestZoneId = zone.id;
    }

    int objectCount = 0;
    for (bool hit : hitFlags) {
        if (hit)
            ++objectCount;
    }

    QVariantList hits;
    hits.reserve(m_zones.size());
    for (int i = 0; i < m_zones.size(); ++i) {
        QVariantMap entry;
        entry.insert(QStringLiteral("zone_id"), m_zones.at(i).id);
        entry.insert(QStringLiteral("count"), counts.at(i));
        hits.append(entry);
    }

    // ---- 标注图像 ----
    if (!image.empty()) {
        cv::Mat annotated = image.clone();
        if (paramBool(QStringLiteral("draw"), true)) {
            const int thickness = paramInt(QStringLiteral("line_thickness"), 2);
            for (int i = 0; i < m_zones.size(); ++i) {
                const Zone &zone = m_zones.at(i);
                const cv::Rect rect = zonePixelRect(zone, image.size());
                const bool isBest = (zone.id == bestZoneId);
                const cv::Scalar color = isBest ? cv::Scalar(0, 255, 0)
                                                : (counts.at(i) > 0 ? cv::Scalar(0, 180, 0)
                                                                    : cv::Scalar(255, 160, 0));
                cv::rectangle(annotated, rect, color, isBest ? thickness + 1 : thickness,
                              cv::LINE_AA);

                const std::string label = QStringLiteral("Z%1").arg(zone.id).toStdString();
                cv::putText(annotated, label, cv::Point(rect.x + 4, rect.y + 18),
                            cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2, cv::LINE_AA);
            }

            for (int i = 0; i < filtered.size(); ++i) {
                const cv::Point center(cvRound(filtered.at(i).center.x()),
                                       cvRound(filtered.at(i).center.y()));
                cv::circle(annotated, center, 4,
                           hitFlags.at(i) ? cv::Scalar(0, 255, 0) : cv::Scalar(160, 160, 160),
                           cv::FILLED, cv::LINE_AA);
            }
        }
        setOutput(QStringLiteral("output"), imageValue(annotated));
    }

    setOutput(QStringLiteral("zone_id"), bestZoneId);
    setOutput(QStringLiteral("present"), bestZoneId >= 0);
    setOutput(QStringLiteral("object_count"), objectCount);
    setOutput(QStringLiteral("zone_hits"), hits);

    if (bestZoneId < 0 && paramBool(QStringLiteral("empty_as_ng"), false)) {
        setError(QStringLiteral("检测区内没有物料"));
        return false;
    }

    return true;
}

QDialog *MaterialZonePlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    return new MaterialZoneDialog(this, inputImage, parent);
}

void MaterialZonePlugin::setZones(const QList<Zone> &zones)
{
    m_zones.clear();
    for (Zone zone : zones) {
        zone.rect.x = std::max(0.0f, std::min(1.0f, zone.rect.x));
        zone.rect.y = std::max(0.0f, std::min(1.0f, zone.rect.y));
        zone.rect.width = std::max(0.0f, std::min(1.0f - zone.rect.x, zone.rect.width));
        zone.rect.height = std::max(0.0f, std::min(1.0f - zone.rect.y, zone.rect.height));
        if (zone.rect.width <= 0.0f || zone.rect.height <= 0.0f)
            continue;
        m_zones.append(zone);
    }

    std::sort(m_zones.begin(), m_zones.end(),
              [](const Zone &a, const Zone &b) { return a.id < b.id; });
}

QVariantMap MaterialZonePlugin::extraData() const
{
    QVariantList list;
    list.reserve(m_zones.size());
    for (const Zone &zone : m_zones) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), zone.id);
        map.insert(QStringLiteral("x"), zone.rect.x);
        map.insert(QStringLiteral("y"), zone.rect.y);
        map.insert(QStringLiteral("w"), zone.rect.width);
        map.insert(QStringLiteral("h"), zone.rect.height);
        list.append(map);
    }

    QVariantMap data;
    data.insert(QStringLiteral("zones"), list);
    return data;
}

void MaterialZonePlugin::setExtraData(const QVariantMap &data)
{
    const QVariantList list = data.value(QStringLiteral("zones")).toList();
    QList<Zone> zones;
    zones.reserve(list.size());
    for (const QVariant &entry : list) {
        const QVariantMap map = entry.toMap();
        Zone zone;
        zone.id = map.value(QStringLiteral("id"), 0).toInt();
        zone.rect = cv::Rect2f(map.value(QStringLiteral("x")).toFloat(),
                               map.value(QStringLiteral("y")).toFloat(),
                               map.value(QStringLiteral("w")).toFloat(),
                               map.value(QStringLiteral("h")).toFloat());
        zones.append(zone);
    }
    setZones(zones);
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> MaterialZonePluginProvider::availablePlugins() const
{
    return {OpenVisionPluginProvider::Entry{OVP::createMaterialZonePlugin}};
}
