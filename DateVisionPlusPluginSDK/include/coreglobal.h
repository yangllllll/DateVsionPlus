#ifndef OVPCORE_GLOBAL_H
#define OVPCORE_GLOBAL_H

#include <QtGlobal>

/**
 * OpenVisionPlus 核心库符号导出宏
 *
 * 主程序编译时定义 OPENVISIONCORE_LIBRARY，符号为 export；
 * 用户插件 DLL 不定义该宏，符号为 import（MSVC 需链接主程序生成的导入库）。
 */
#if defined(OPENVISIONCORE_STATIC)
#  define OPVCORE_EXPORT
#elif defined(OPENVISIONCORE_LIBRARY)
#  define OPVCORE_EXPORT Q_DECL_EXPORT
#else
#  define OPVCORE_EXPORT Q_DECL_IMPORT
#endif

#endif // OVPCORE_GLOBAL_H
