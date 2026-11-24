#ifndef CALIBDATA_H
#define CALIBDATA_H

/**
 * @brief N 点标定数据（图像坐标 -> 机器人坐标的二维仿射变换）
 *
 * 模型： x = a*u + b*v + c
 *        y = d*u + e*v + f
 * 至少 3 组点（u,v -> x,y），用最小二乘求解；支持保存/加载 JSON 标定文件。
 *
 * 被「N点标定」与「无序抓取」两个插件共用。
 */

#include <QList>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QVariantMap>

#include <opencv2/core.hpp>

#include <memory>

namespace CALIB {

/** 一组标定点：图像像素坐标 + 对应机器人坐标 */
struct CalibPoint
{
    QPointF image;
    QPointF robot;
};

struct CalibData
{
    QList<CalibPoint> points;
    cv::Matx23d matrix = cv::Matx23d(1, 0, 0, 0, 1, 0);   //!< [a b c; d e f]
    bool valid = false;
    double rms = 0.0;          //!< 拟合残差均方根（机器人坐标单位）
    QSize imageSize;           //!< 标定时使用的图像尺寸（仅供参考）
    QString sourceFile;

    /** 用 points 最小二乘拟合 matrix；失败时 valid = false */
    bool compute(QString *error = nullptr);

    /** 图像坐标 -> 机器人坐标 */
    QPointF apply(const QPointF &imagePoint) const;

    /** 与项目 extraData 互转 */
    QVariantMap toVariantMap() const;
    bool fromVariantMap(const QVariantMap &map, QString *error = nullptr);

    /** 保存 / 加载 .ncal（JSON 文本） */
    bool save(const QString &path, QString *error = nullptr) const;
    static bool load(const QString &path, CalibData &data, QString *error = nullptr);
};

/**
 * @brief 按路径缓存标定文件
 *
 * 文件修改时间变化时自动重新加载，适合流程每帧调用。
 */
std::shared_ptr<const CalibData> loadCached(const QString &path, QString *error = nullptr);

/** 文件对话框过滤器 */
QString fileFilter();

} // namespace CALIB

#endif // CALIBDATA_H
