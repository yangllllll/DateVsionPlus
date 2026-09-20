#ifndef PLUGININTERFACE_H
#define PLUGININTERFACE_H

#include "pluginbase.h"

#include <QtPlugin>

/**
 * @brief 用户扩展插件（Qt 插件，动态库）需要实现的提供者接口
 *
 * 编译为一个 Qt Plugin（.dll / .so），放到程序目录下的 plugins/ 文件夹，
 * 启动时会被 PluginManager 自动扫描加载。
 *
 * 用法：
 * @code
 * class MyProvider : public QObject, public OpenVisionPluginProvider
 * {
 *     Q_OBJECT
 *     Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
 *     Q_INTERFACES(OpenVisionPluginProvider)
 * public:
 *     QList<Entry> availablePlugins() const override;
 * };
 * @endcode
 */
class OpenVisionPluginProvider
{
public:
    using CreateFunction = OVP::PluginBase *(*)(QObject *);

    struct Entry
    {
        CreateFunction create = nullptr;
    };

    virtual ~OpenVisionPluginProvider() = default;
    virtual QList<Entry> availablePlugins() const = 0;
};

#define OpenVisionPluginProvider_iid "com.datekj.OpenVisionPlus.PluginProvider/1.0"
Q_DECLARE_INTERFACE(OpenVisionPluginProvider, OpenVisionPluginProvider_iid)

#endif // PLUGININTERFACE_H
