#include "pluginmanager.h"

#include "../plugins/builtinregistry.h"
#include "plugininterface.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QPluginLoader>

namespace OVP {

PluginManager::PluginManager()
    : m_pluginDirs(defaultPluginDirs())
{
}

PluginManager::~PluginManager()
{
    // 进程退出时才卸载动态库，此时所有插件实例均已销毁
    for (QPluginLoader *loader : m_loaders)
        delete loader;
    m_loaders.clear();
}

PluginManager &PluginManager::instance()
{
    static PluginManager manager;
    return manager;
}

void PluginManager::registerFactory(CreateFunction fn)
{
    if (!fn)
        return;

    PluginBase *probe = fn(nullptr);
    if (!probe)
        return;

    Info info;
    info.id = probe->id();
    info.name = probe->name();
    info.category = probe->category();
    info.description = probe->description();
    info.create = fn;
    delete probe;

    registerInfo(info);
}

void PluginManager::registerInfo(const Info &info)
{
    if (info.id.isEmpty() || !info.create)
        return;

    if (!m_categoryOrder.contains(info.category))
        m_categoryOrder.append(info.category);

    m_plugins.insert(info.id, info);
}

void PluginManager::reloadAll()
{
    m_plugins.clear();
    m_categoryOrder.clear();

    loadBuiltin();
    loadUserPlugins();
}

void PluginManager::loadBuiltin()
{
    registerBuiltinPlugins(*this);
}

QStringList PluginManager::defaultPluginDirs()
{
    return {QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"),
            QDir::currentPath() + QStringLiteral("/plugins")};
}

void PluginManager::setPluginDirs(const QStringList &dirs)
{
    m_pluginDirs = dirs;
}

void PluginManager::loadUserPlugins()
{
    m_userRecords.clear();
    m_scannedFiles.clear();

    // 各平台动态库后缀
    QStringList nameFilters;
#if defined(Q_OS_WIN)
    nameFilters << QStringLiteral("*.dll");
#elif defined(Q_OS_MACOS)
    nameFilters << QStringLiteral("*.dylib") << QStringLiteral("*.so");
#else
    nameFilters << QStringLiteral("*.so");
#endif

    for (const QString &dirPath : m_pluginDirs) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        QDirIterator it(dirPath, nameFilters, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = QFileInfo(it.next()).canonicalFilePath();
            if (filePath.isEmpty() || m_scannedFiles.contains(filePath))
                continue;
            m_scannedFiles.insert(filePath);

            UserPluginRecord record;
            loadUserPluginFile(filePath, record);
            m_userRecords.append(record);
        }
    }
}

bool PluginManager::loadUserPluginFile(const QString &filePath, UserPluginRecord &record)
{
    record.filePath = filePath;

    // 复用已有的 loader，避免重复加载同一动态库
    QPluginLoader *loader = m_loaders.value(filePath, nullptr);
    if (!loader) {
        loader = new QPluginLoader(filePath);
        m_loaders.insert(filePath, loader);
    }

    QObject *instance = loader->instance();
    if (!instance) {
        record.loaded = false;
        record.message = loader->errorString().isEmpty()
                             ? QStringLiteral("无法加载（可能不是 Qt 插件库或编译器套件不匹配）")
                             : loader->errorString();
        return false;
    }

    auto *provider = qobject_cast<OpenVisionPluginProvider *>(instance);
    if (!provider) {
        record.loaded = false;
        record.message = QStringLiteral("动态库未实现 OpenVisionPluginProvider 接口");
        return false;
    }

    const QList<OpenVisionPluginProvider::Entry> entries = provider->availablePlugins();
    for (const OpenVisionPluginProvider::Entry &entry : entries) {
        if (!entry.create)
            continue;

        // 先探测 ID，便于记录日志；失败则不注册
        PluginBase *probe = entry.create(nullptr);
        if (!probe)
            continue;
        const QString id = probe->id();
        delete probe;

        registerFactory(entry.create);
        record.pluginIds.append(id);
    }

    record.loaded = true;
    record.message = QStringLiteral("已注册 %1 个工具").arg(record.pluginIds.size());
    return true;
}

QStringList PluginManager::categories() const
{
    return m_categoryOrder;
}

QStringList PluginManager::pluginsInCategory(const QString &category) const
{
    QStringList result;
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        if (it.value().category == category)
            result.append(it.key());
    }
    return result;
}

QList<PluginManager::Info> PluginManager::allPlugins() const
{
    return m_plugins.values();
}

PluginManager::Info PluginManager::info(const QString &id) const
{
    return m_plugins.value(id);
}

bool PluginManager::contains(const QString &id) const
{
    return m_plugins.contains(id);
}

PluginBase *PluginManager::create(const QString &id, QObject *parent) const
{
    auto it = m_plugins.constFind(id);
    if (it == m_plugins.constEnd() || !it.value().create)
        return nullptr;
    return it.value().create(parent);
}

} // namespace OVP
