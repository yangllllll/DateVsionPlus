/****************************************************************************
** Meta object code from reading C++ file 'plcplugin.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "plcplugin.h"
#include <QtCore/qmetatype.h>
#include <QtCore/qplugin.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'plcplugin.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN3OVP13PLCSendPluginE_t {};
} // unnamed namespace

template <> constexpr inline auto OVP::PLCSendPlugin::qt_create_metaobjectdata<qt_meta_tag_ZN3OVP13PLCSendPluginE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "OVP::PLCSendPlugin"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PLCSendPlugin, qt_meta_tag_ZN3OVP13PLCSendPluginE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject OVP::PLCSendPlugin::staticMetaObject = { {
    QMetaObject::SuperData::link<PluginBase::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3OVP13PLCSendPluginE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3OVP13PLCSendPluginE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN3OVP13PLCSendPluginE_t>.metaTypes,
    nullptr
} };

void OVP::PLCSendPlugin::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PLCSendPlugin *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *OVP::PLCSendPlugin::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OVP::PLCSendPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3OVP13PLCSendPluginE_t>.strings))
        return static_cast<void*>(this);
    return PluginBase::qt_metacast(_clname);
}

int OVP::PLCSendPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = PluginBase::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN17PLCPluginProviderE_t {};
} // unnamed namespace

template <> constexpr inline auto PLCPluginProvider::qt_create_metaobjectdata<qt_meta_tag_ZN17PLCPluginProviderE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "PLCPluginProvider"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PLCPluginProvider, qt_meta_tag_ZN17PLCPluginProviderE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject PLCPluginProvider::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17PLCPluginProviderE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17PLCPluginProviderE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17PLCPluginProviderE_t>.metaTypes,
    nullptr
} };

void PLCPluginProvider::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PLCPluginProvider *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *PLCPluginProvider::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PLCPluginProvider::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17PLCPluginProviderE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "OpenVisionPluginProvider"))
        return static_cast< OpenVisionPluginProvider*>(this);
    if (!strcmp(_clname, "com.datekj.OpenVisionPlus.PluginProvider/1.0"))
        return static_cast< OpenVisionPluginProvider*>(this);
    return QObject::qt_metacast(_clname);
}

int PLCPluginProvider::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    return _id;
}

#ifdef QT_MOC_EXPORT_PLUGIN_V2
static constexpr unsigned char qt_pluginMetaDataV2_PLCPluginProvider[] = {
    0xbf, 
    // "IID"
    0x02,  0x78,  0x2c,  'c',  'o',  'm',  '.',  'd', 
    'a',  't',  'e',  'k',  'j',  '.',  'O',  'p', 
    'e',  'n',  'V',  'i',  's',  'i',  'o',  'n', 
    'P',  'l',  'u',  's',  '.',  'P',  'l',  'u', 
    'g',  'i',  'n',  'P',  'r',  'o',  'v',  'i', 
    'd',  'e',  'r',  '/',  '1',  '.',  '0', 
    // "className"
    0x03,  0x71,  'P',  'L',  'C',  'P',  'l',  'u', 
    'g',  'i',  'n',  'P',  'r',  'o',  'v',  'i', 
    'd',  'e',  'r', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN_V2(PLCPluginProvider, PLCPluginProvider, qt_pluginMetaDataV2_PLCPluginProvider)
#else
QT_PLUGIN_METADATA_SECTION
Q_CONSTINIT static constexpr unsigned char qt_pluginMetaData_PLCPluginProvider[] = {
    'Q', 'T', 'M', 'E', 'T', 'A', 'D', 'A', 'T', 'A', ' ', '!',
    // metadata version, Qt version, architectural requirements
    0, QT_VERSION_MAJOR, QT_VERSION_MINOR, qPluginArchRequirements(),
    0xbf, 
    // "IID"
    0x02,  0x78,  0x2c,  'c',  'o',  'm',  '.',  'd', 
    'a',  't',  'e',  'k',  'j',  '.',  'O',  'p', 
    'e',  'n',  'V',  'i',  's',  'i',  'o',  'n', 
    'P',  'l',  'u',  's',  '.',  'P',  'l',  'u', 
    'g',  'i',  'n',  'P',  'r',  'o',  'v',  'i', 
    'd',  'e',  'r',  '/',  '1',  '.',  '0', 
    // "className"
    0x03,  0x71,  'P',  'L',  'C',  'P',  'l',  'u', 
    'g',  'i',  'n',  'P',  'r',  'o',  'v',  'i', 
    'd',  'e',  'r', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN(PLCPluginProvider, PLCPluginProvider)
#endif  // QT_MOC_EXPORT_PLUGIN_V2

QT_WARNING_POP
