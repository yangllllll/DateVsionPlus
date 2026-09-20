#ifndef OPENCVCOMPAT_H
#define OPENCVCOMPAT_H

/**
 * @brief OpenCV 版本兼容头
 *
 * OpenCV 5 将轮廓 / 几何度量函数从 imgproc 迁移到了新的 geometry 模块：
 *   contourArea / arcLength / moments / fitLine / minAreaRect /
 *   minEnclosingCircle / boundingRect / isContourConvex
 * 而 drawContours / findContours / Canny / 形态学 / 滤波等仍在 imgproc。
 *
 * 业务代码只需 #include 本头文件即可同时兼容 OpenCV 4.x 与 5.x。
 */

#include <opencv2/core.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/imgproc.hpp>

#if CV_VERSION_MAJOR >= 5
#  include <opencv2/geometry/2d.hpp>
#endif

#endif // OPENCVCOMPAT_H
