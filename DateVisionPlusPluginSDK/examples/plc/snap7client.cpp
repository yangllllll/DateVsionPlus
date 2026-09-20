#include "snap7client.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#if defined(Q_OS_WIN)
static const char *const kLibraryName = "snap7.dll";
#else
static const char *const kLibraryName = "libsnap7.so";
#endif

namespace Snap7 {

namespace {

/** 用户填写的路径：允许填 DLL 全路径，也允许只填所在目录 */
QStringList expandCandidate(const QString &path)
{
    if (path.isEmpty())
        return QStringList();

    const QFileInfo info(path);
    const bool looksLikeLibrary =
        path.endsWith(QLatin1String(".dll"), Qt::CaseInsensitive) ||
        path.endsWith(QLatin1String(".so"), Qt::CaseInsensitive);

    if (looksLikeLibrary && !info.isDir())
        return QStringList(path);

    const QString withName = QDir(path).filePath(QLatin1String(kLibraryName));
    if (info.isDir())
        return QStringList(withName);

    return QStringList{withName, path};
}

void appendUnique(QStringList &list, const QString &value)
{
    if (!value.isEmpty() && !list.contains(value))
        list.append(value);
}

} // namespace

// ---------------------------------------------------------------- Snap7Api

Snap7Api::Snap7Api() = default;

Snap7Api::~Snap7Api()
{
    if (m_library) {
        m_library->unload();
        delete m_library;
        m_library = nullptr;
    }
    m_loaded = false;
}

Snap7Api *Snap7Api::instance()
{
    static Snap7Api api;
    return &api;
}

QStringList Snap7Api::libraryCandidates(const QString &preferredPath)
{
    QStringList candidates;

    for (const QString &path : expandCandidate(preferredPath))
        appendUnique(candidates, path);

    const QString envPath = QString::fromLocal8Bit(qgetenv("SNAP7_PATH"));
    for (const QString &path : expandCandidate(envPath))
        appendUnique(candidates, path);

    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        appendUnique(candidates, QDir(appDir).filePath(QLatin1String(kLibraryName)));
        appendUnique(candidates, QDir(appDir).filePath(QStringLiteral("plugins/") +
                                                       QLatin1String(kLibraryName)));
        appendUnique(candidates, QDir(appDir).filePath(QStringLiteral("snap7/") +
                                                       QLatin1String(kLibraryName)));
    }

    // 最后交给系统 PATH / ld.so.conf 去找
    appendUnique(candidates, QLatin1String(kLibraryName));

    return candidates;
}

bool Snap7Api::resolveSymbols(QLibrary *library, QString *error)
{
    struct Required {
        const char *name;
        void **target;
    };

    const Required required[] = {
        {"Cli_Create", reinterpret_cast<void **>(&create)},
        {"Cli_Destroy", reinterpret_cast<void **>(&destroy)},
        {"Cli_ConnectTo", reinterpret_cast<void **>(&connectTo)},
        {"Cli_Disconnect", reinterpret_cast<void **>(&disconnect)},
        {"Cli_GetConnected", reinterpret_cast<void **>(&getConnected)},
        {"Cli_WriteArea", reinterpret_cast<void **>(&writeArea)},
        {"Cli_ErrorText", reinterpret_cast<void **>(&errorText)},
    };

    for (const Required &item : required) {
        QFunctionPointer symbol = library->resolve(item.name);
        if (!symbol) {
            m_lastError = QStringLiteral("snap7 动态库缺少导出函数 %1：%2")
                              .arg(QLatin1String(item.name), library->fileName());
            if (error)
                *error = m_lastError;
            return false;
        }
        *item.target = reinterpret_cast<void *>(symbol);
    }

    // ---- 可选符号 ----
    connect = reinterpret_cast<FnCliConnect>(library->resolve("Cli_Connect"));
    readArea = reinterpret_cast<FnCliReadArea>(library->resolve("Cli_ReadArea"));
    getLastError = reinterpret_cast<FnCliGetLastError>(library->resolve("Cli_GetLastError"));

    return true;
}

bool Snap7Api::load(const QString &preferredPath, QString *error)
{
    if (m_loaded)
        return true;

    const QStringList candidates = libraryCandidates(preferredPath);
    QStringList tried;

    for (const QString &path : candidates) {
        QLibrary *library = new QLibrary(path);
        library->setLoadHints(QLibrary::PreventUnloadHint);
        if (!library->load()) {
            tried.append(QStringLiteral("%1 (%2)").arg(path, library->errorString()));
            delete library;
            continue;
        }

        if (!resolveSymbols(library, error)) {
            tried.append(path);
            library->unload();
            delete library;
            continue;
        }

        if (m_library) {
            m_library->unload();
            delete m_library;
        }
        m_library = library;
        m_libraryPath = path;
        m_loaded = true;
        m_lastError.clear();
        return true;
    }

    m_lastError = QStringLiteral("未能加载 %1。已尝试：\n  %2\n"
                                 "请把 snap7.dll（snap7-full-1.4.2/release/Windows/Win64/）"
                                 "放到主程序目录或其 plugins 目录，或设置环境变量 SNAP7_PATH。")
                      .arg(QLatin1String(kLibraryName), tried.join(QStringLiteral("\n  ")));
    if (error)
        *error = m_lastError;
    return false;
}

// ------------------------------------------------------------- Snap7Client

Snap7Client::Snap7Client() = default;

Snap7Client::~Snap7Client()
{
    disconnect();

    Snap7Api *api = Snap7Api::instance();
    if (m_created && api->destroy) {
        S7Object client = m_client;
        api->destroy(&client);
        m_client = 0;
    }
    m_created = false;
}

bool Snap7Client::ensureCreated(QString *error)
{
    Snap7Api *api = Snap7Api::instance();
    if (!api->isLoaded() && !api->load(QString(), error))
        return false;

    if (m_created)
        return true;

    m_client = api->create();
    m_created = true;
    if (m_client == 0)
        return fail(QStringLiteral("snap7 Cli_Create 返回空句柄"), error);
    return true;
}

bool Snap7Client::isConnected() const
{
    Snap7Api *api = Snap7Api::instance();
    if (!m_created || !api->isLoaded() || !api->getConnected)
        return false;

    int connected = 0;
    api->getConnected(m_client, &connected);
    return connected != 0;
}

bool Snap7Client::connectTo(const QString &address, int rack, int slot, QString *error)
{
    if (!ensureCreated(error))
        return false;

    // 已连接且地址未变则直接复用（Python: if not get_connected(): connect(...)）
    if (isConnected() && m_address == address)
        return true;

    if (isConnected())
        disconnect();

    Snap7Api *api = Snap7Api::instance();
    const QByteArray ipBytes = address.toUtf8();
    const int code = api->connectTo(m_client, ipBytes.constData(), rack, slot);
    if (code != 0) {
        fail(describe(code, QStringLiteral("连接 %1 (rack=%2, slot=%3)").arg(address).arg(rack).arg(slot)),
             error);
        return false;
    }

    m_address = address;
    m_lastError.clear();
    return true;
}

void Snap7Client::disconnect()
{
    Snap7Api *api = Snap7Api::instance();
    if (m_created && api->isLoaded() && api->disconnect)
        api->disconnect(m_client);
    m_address.clear();
}

bool Snap7Client::writeArea(int area, int dbNumber, int start, int wordLen, const QByteArray &data,
                            QString *error)
{
    Snap7Api *api = Snap7Api::instance();
    if (!api->isLoaded() || !m_created)
        return fail(QStringLiteral("PLC 未连接"), error);

    const int code = api->writeArea(m_client, area, dbNumber, start, data.size(), wordLen,
                                    const_cast<char *>(data.constData()));
    if (code != 0) {
        fail(describe(code, QStringLiteral("写区域 area=0x%1 db=%2 start=%3")
                                .arg(area, 2, 16, QLatin1Char('0'))
                                .arg(dbNumber)
                                .arg(start)),
             error);
        return false;
    }
    return true;
}

bool Snap7Client::writeByte(int area, int dbNumber, int start, quint8 value, QString *error)
{
    return writeArea(area, dbNumber, start, WLByte,
                     QByteArray(1, static_cast<char>(value)), error);
}

bool Snap7Client::readArea(int area, int dbNumber, int start, int amount, int wordLen,
                           QByteArray *out, QString *error)
{
    Snap7Api *api = Snap7Api::instance();
    if (!api->readArea)
        return fail(QStringLiteral("当前 snap7.dll 未提供 Cli_ReadArea，无法读回数据"), error);
    if (!api->isLoaded() || !m_created)
        return fail(QStringLiteral("PLC 未连接"), error);

    QByteArray buffer(amount, '\0');
    const int code = api->readArea(m_client, area, dbNumber, start, amount, wordLen,
                                   buffer.data());
    if (code != 0) {
        fail(describe(code, QStringLiteral("读区域 area=0x%1 db=%2 start=%3")
                                .arg(area, 2, 16, QLatin1Char('0'))
                                .arg(dbNumber)
                                .arg(start)),
             error);
        return false;
    }
    if (out)
        *out = buffer;
    return true;
}

bool Snap7Client::readByte(int area, int dbNumber, int start, quint8 *value, QString *error)
{
    QByteArray buffer;
    if (!readArea(area, dbNumber, start, 1, WLByte, &buffer, error))
        return false;
    if (value && !buffer.isEmpty())
        *value = static_cast<quint8>(buffer.at(0));
    return true;
}

QString Snap7Client::errorText(int code)
{
    Snap7Api *api = Snap7Api::instance();
    if (!api->isLoaded() || !api->errorText)
        return QString::number(code);

    char text[1024] = {0};
    api->errorText(code, text, static_cast<int>(sizeof(text)));
    return QString::fromUtf8(text).trimmed();
}

QString Snap7Client::describe(int code, const QString &context) const
{
    return QStringLiteral("%1 失败: %2 (0x%3)")
        .arg(context, errorText(code))
        .arg(static_cast<quint32>(code), 8, 16, QLatin1Char('0'));
}

bool Snap7Client::fail(const QString &message, QString *error)
{
    m_lastError = message;
    if (error)
        *error = message;
    return false;
}

int areaFromName(const QString &name)
{
    const QString key = name.trimmed().toUpper();
    if (key == QLatin1String("DB"))
        return AreaDB;
    if (key == QLatin1String("PA") || key == QLatin1String("Q"))
        return AreaPA;
    if (key == QLatin1String("PE") || key == QLatin1String("I"))
        return AreaPE;
    if (key == QLatin1String("CT"))
        return AreaCT;
    if (key == QLatin1String("TM"))
        return AreaTM;
    return AreaMK; // 默认 M 区（Python 版固定使用 Area.MK）
}

} // namespace Snap7
