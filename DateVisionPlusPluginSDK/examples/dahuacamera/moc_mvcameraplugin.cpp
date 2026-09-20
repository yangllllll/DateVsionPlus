/****************************************************************************
** Meta object code from reading C++ file 'mvcameraplugin.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "mvcameraplugin.h"
#include <QtCore/qmetatype.h>
#include <QtCore/qplugin.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mvcameraplugin.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN3OVP14MVCameraPluginE_t {};
} // unnamed namespace

template <> constexpr inline auto OVP::MVCameraPlugin::qt_create_metaobjectdata<qt_meta_tag_ZN3OVP14MVCameraPluginE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "OVP::MVCameraPlugin"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MVCameraPlugin, qt_meta_tag_ZN3OVP14MVCameraPluginE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject OVP::MVCameraPlugin::staticMetaObject = { {
    QMetaObject::SuperData::link<PluginBase::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3OVP14MVCameraPluginE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3OVP14MVCameraPluginE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN3OVP14MVCameraPluginE_t>.metaTypes,
    nullptr
} };

void OVP::MVCameraPlugin::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MVCameraPlugin *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *OVP::MVCameraPlugin::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OVP::MVCameraPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3OVP14MVCameraPluginE_t>.strings))
        return static_cast<void*>(this);
    return PluginBase::qt_metacast(_clname);
}

int OVP::MVCameraPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = PluginBase::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN25DahuaCameraPluginProviderE_t {};
} // unnamed namespace

template <> constexpr inline auto DahuaCameraPluginProvider::qt_create_metaobjectdata<qt_meta_tag_ZN25DahuaCameraPluginProviderE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DahuaCameraPluginProvider"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DahuaCameraPluginProvider, qt_meta_tag_ZN25DahuaCameraPluginProviderE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DahuaCameraPluginProvider::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN25DahuaCameraPluginProviderE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN25DahuaCameraPluginProviderE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN25DahuaCameraPluginProviderE_t>.metaTypes,
    nullptr
} };

void DahuaCameraPluginProvider::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DahuaCameraPluginProvider *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *DahuaCameraPluginProvider::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DahuaCameraPluginProvider::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN25DahuaCameraPluginProviderE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "OpenVisionPluginProvider"))
        return static_cast< OpenVisionPluginProvider*>(this);
    if (!strcmp(_clname, "com.datekj.OpenVisionPlus.PluginProvider/1.0"))
        return static_cast< OpenVisionPluginProvider*>(this);
    return QObject::qt_metacast(_clname);
}

int DahuaCameraPluginProvider::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    return _id;
}

#ifdef QT_MOC_EXPORT_PLUGIN_V2
static constexpr unsigned char qt_pluginMetaDataV2_DahuaCameraPluginProvider[] = {
    0xbf, 
    // "IID"
    0x02,  0x78,  0x2c,  'c',  'o',  'm',  '.',  'd', 
    'a',  't',  'e',  'k',  'j',  '.',  'O',  'p', 
    'e',  'n',  'V',  'i',  's',  'i',  'o',  'n', 
    'P',  'l',  'u',  's',  '.',  'P',  'l',  'u', 
    'g',  'i',  'n',  'P',  'r',  'o',  'v',  'i', 
    'd',  'e',  'r',  '/',  '1',  '.',  '0', 
    // "className"
    0x03,  0x78,  0x19,  'D',  'a',  'h',  'u',  'a', 
    'C',  'a',  'm',  'e',  'r',  'a',  'P',  'l', 
    'u',  'g',  'i',  'n',  'P',  'r',  'o',  'v', 
    'i',  'd',  'e',  'r', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN_V2(DahuaCameraPluginProvider, DahuaCameraPluginProvider, qt_pluginMetaDataV2_DahuaCameraPluginProvider)
#else
QT_PLUGIN_METADATA_SECTION
Q_CONSTINIT static constexpr unsigned char qt_pluginMetaData_DahuaCameraPluginProvider[] = {
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
    0x03,  0x78,  0x19,  'D',  'a',  'h',  'u',  'a', 
    'C',  'a',  'm',  'e',  'r',  'a',  'P',  'l', 
    'u',  'g',  'i',  'n',  'P',  'r',  'o',  'v', 
    'i',  'd',  'e',  'r', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN(DahuaCameraPluginProvider, DahuaCameraPluginProvider)
#endif  // QT_MOC_EXPORT_PLUGIN_V2

QT_WARNING_POP
