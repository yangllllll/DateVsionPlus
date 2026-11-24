#include "detectionlist.h"

#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace DETECT {
namespace {

bool readCenter(const QVariantMap &map, QPointF &center)
{
    if (map.contains(QStringLiteral("center_x")) && map.contains(QStringLiteral("center_y"))) {
        center = QPointF(map.value(QStringLiteral("center_x")).toDouble(),
                         map.value(QStringLiteral("center_y")).toDouble());
        return true;
    }

    if (map.contains(QStringLiteral("center"))) {
        const QVariantList value = map.value(QStringLiteral("center")).toList();
        if (value.size() >= 2) {
            center = QPointF(value.at(0).toDouble(), value.at(1).toDouble());
            return true;
        }
    }

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

    if (!hasBox)
        return false;

    center = QPointF((x1 + x2) * 0.5, (y1 + y2) * 0.5);
    return true;
}

} // namespace

bool parse(const QVariant &value, QList<Item> &items, QString *error)
{
    items.clear();

    if (!value.canConvert<QVariantList>()) {
        if (error)
            *error = QStringLiteral("detections 输入不是检测列表，请连接 YOLO目标识别的 detections 输出");
        return false;
    }

    const QVariantList list = value.toList();
    int parsed = 0;

    for (const QVariant &entry : list) {
        const QVariantMap map = entry.toMap();
        if (map.isEmpty())
            continue;

        Item item;
        item.classId = map.value(QStringLiteral("class_id"), -1).toInt();
        item.className = map.value(QStringLiteral("class_name")).toString();
        item.confidence = map.value(QStringLiteral("confidence"), 0.0).toFloat();

        if (!readCenter(map, item.center))
            continue;

        ++parsed;
        items.append(item);
    }

    if (!list.isEmpty() && parsed == 0) {
        if (error)
            *error = QStringLiteral("检测结果格式不正确：需要包含 center/center_x/xyxy 字段");
        return false;
    }

    if (error)
        error->clear();
    return true;
}

bool matchesFilter(const Item &item, const QString &filterText)
{
    QString text = filterText.trimmed();
    if (text.isEmpty())
        return true;
    text.replace(QStringLiteral("，"), QStringLiteral(","));

    const QStringList tokens = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString token : tokens) {
        token = token.trimmed();
        if (token.isEmpty())
            continue;

        bool ok = false;
        const int id = token.toInt(&ok);
        if (ok) {
            if (item.classId == id)
                return true;
        } else if (item.className.compare(token, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace DETECT
