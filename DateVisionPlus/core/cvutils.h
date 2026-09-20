#ifndef CVUTILS_H
#define CVUTILS_H

#include "coreglobal.h"

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QString>

#include <opencv2/core.hpp>

namespace OVP {

/** cv::Mat -> QImage（支持 CV_8UC1 / CV_8UC3 / CV_8UC4，其余自动转换） */
OPVCORE_EXPORT QImage matToQImage(const cv::Mat &mat);

/** cv::Mat -> QPixmap */
OPVCORE_EXPORT QPixmap matToQPixmap(const cv::Mat &mat);

/** 中文颜色名 -> OpenCV BGR 标量 */
OPVCORE_EXPORT cv::Scalar bgrColor(const QString &colorName);

/** 中文颜色名 -> Qt 颜色 */
OPVCORE_EXPORT QColor qtColor(const QString &colorName);

/** 统一转为灰度图（已是单通道则直接返回） */
OPVCORE_EXPORT cv::Mat ensureGray(const cv::Mat &src);

/** 统一转为三通道 BGR 图 */
OPVCORE_EXPORT cv::Mat ensureBgr(const cv::Mat &src);

/** 确保核大小为奇数且 >= 1 */
OPVCORE_EXPORT int oddKernelSize(int size);

} // namespace OVP

#endif // CVUTILS_H
