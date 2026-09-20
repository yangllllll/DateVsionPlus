#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "pluginbase.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

class QPluginLoader;

namespace OVP {

/**
 * @brief 插件管理器（单例）
 *
 * - 内置插件：由 plugins/builtinregistry.cpp 静态注册
 * - 用户插件：Qt Plugin 动态库（Windows 下为 .dll），由 QPluginLoader 从插件目录加载，
 *             动态库必须实现 OpenVisionPluginProvider 接口（见 core/plugininterface.h）
 */
class PluginManager
{
public:
    using CreateFunction = OVP::PluginBase *(*)(QObject *);

    struct Info
    {
        QString id;
        QString name;
        QString category;
        QString description;
        CreateFunction create = nullptr;
    };

    /** 单个用户插件动态库的加载结果 */
    struct UserPluginRecord
    {
        QString filePath;      ///< 动态库路径
        bool loaded = false;   ///< 是否成功作为 OpenVisionPlus 插件加载
        QString message;       ///< 成功为摘要，失败为错误原因
        QStringList pluginIds; ///< 该库中注册的工具 ID
    };

    static PluginManager &instance();

    /** 注册一个插件工厂（内部会创建一个探针实例读取元信息） */
    void registerFactory(CreateFunction fn);
    void registerInfo(const Info &info);

    /** 重新加载内置插件 + 用户插件动态库 */
    void reloadAll();

    // ---- 用户插件（DLL）相关 ----
    /** 默认的动态库搜索目录：程序运行目录/plugins、当前工作目录/plugins */
    static QStringList defaultPluginDirs();
    void setPluginDirs(const QStringList &dirs);
    QStringList pluginDirs() const { return m_pluginDirs; }

    /** 最近一次 reloadAll 的加载明细（供 UI 显示日志） */
    QList<UserPluginRecord> userPluginRecords() const { return m_userRecords; }

    QStringList categories() const;
    QStringList pluginsInCategory(const QString &category) const;
    QList<Info> allPlugins() const;
    Info info(const QString &id) const;
    bool contains(const QString &id) const;

    /** 创建插件实例，调用方（通常是 NodeItem）负责释放 */
    PluginBase *create(const QString &id, QObject *parent = nullptr) const;

private:
    PluginManager();
    ~PluginManager();
    PluginManager(const PluginManager &) = delete;
    PluginManager &operator=(const PluginManager &) = delete;

    void loadBuiltin();
    void loadUserPlugins();
    bool loadUserPluginFile(const QString &filePath, UserPluginRecord &record);

    QMap<QString, Info> m_plugins;
    QStringList m_categoryOrder;

    QStringList m_pluginDirs;
    QList<UserPluginRecord> m_userRecords;

    /** 必须长期持有：QPluginLoader 析构会卸载动态库，导致已创建的插件实例失效 */
    QMap<QString, QPluginLoader *> m_loaders;
    QSet<QString> m_scannedFiles;
};

/** 便捷注册用工厂函数 */
template <typename T>
PluginBase *createPluginInstance(QObject *parent)
{
    return new T(parent);
}

} // namespace OVP

#endif // PLUGINMANAGER_H
