#ifndef EXPRESSIONEVALUATOR_H
#define EXPRESSIONEVALUATOR_H

#include "coreglobal.h"

#include <QString>
#include <functional>

namespace OVP {

/**
 * @brief 安全的四则运算求值器（仅支持 + - * / ( ) 与数字）
 * @param expression 表达式字符串
 * @param result 求值结果
 * @return 表达式合法且求值成功返回 true
 */
OPVCORE_EXPORT bool evaluateArithmetic(const QString &expression, double &result);

/**
 * @brief 把形如 "{节点ID.端口名}" 的引用替换为实际值
 * @param line 待处理的一行文本
 * @param resolve 回调：输入 "节点ID.端口名"，返回替换字符串
 */
OPVCORE_EXPORT QString substituteReferences(const QString &line,
                                            const std::function<QString(const QString &)> &resolve);

} // namespace OVP

#endif // EXPRESSIONEVALUATOR_H
