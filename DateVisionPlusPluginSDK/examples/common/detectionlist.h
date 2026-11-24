#ifndef DETECTIONLIST_H
#define DETECTIONLIST_H

/**
 * @brief YOLO detections 输入的解析工具（N点标定 / 无序抓取共用）
 *
 * 兼容字段：
 *   center_x / center_y、center[2]、x1..y2、xyxy[4]
 * 其中任一组合能算出中心点即可。
 */

#include <QList>
#include <QPointF>
#include <QString>
#include <QVariant>

namespace DETECT {

struct Item
{
    int classId = -1;
    QString className;
    float confidence = 0.0f;
    QPointF center;   //!< 像素坐标
};

/** 解析 QVariantList<QVariantMap>；格式不对时返回 false 并写入 error */
bool parse(const QVariant &value, QList<Item> &items, QString *error = nullptr);

/** 类别过滤：数字按 class_id，其它按 class_name（不区分大小写，逗号分隔，空 = 全部） */
bool matchesFilter(const Item &item, const QString &filterText);

} // namespace DETECT

#endif // DETECTIONLIST_H
