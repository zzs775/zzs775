/****************************************************************************
** Meta object code from reading C++ file 'SimDataManager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../../vsgQt-master/examples/vsgqtviewer/SimDataManager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SimDataManager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.1. It"
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
struct qt_meta_tag_ZN14SimDataManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto SimDataManager::qt_create_metaobjectdata<qt_meta_tag_ZN14SimDataManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SimDataManager",
        "maxReceivedTimeChanged",
        "",
        "time",
        "sceneResetRequested",
        "entityAdded",
        "id",
        "entityRemoved",
        "entityUpdated",
        "addEntity",
        "ModelType",
        "type",
        "lat",
        "lon",
        "alt",
        "addEntityWithName",
        "name",
        "removeEntity",
        "updateEntityPosition",
        "yaw",
        "pitch",
        "roll",
        "getEntity",
        "QVariantMap",
        "count",
        "clearAll",
        "getEntitiesByType",
        "QVariantList",
        "typeFilter",
        "maxReceivedTime"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'maxReceivedTimeChanged'
        QtMocHelpers::SignalData<void(double)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 3 },
        }}),
        // Signal 'sceneResetRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'entityAdded'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'entityRemoved'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'entityUpdated'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Method 'addEntity'
        QtMocHelpers::MethodData<void(ModelType, double, double, double)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 }, { QMetaType::Double, 12 }, { QMetaType::Double, 13 }, { QMetaType::Double, 14 },
        }}),
        // Method 'addEntityWithName'
        QtMocHelpers::MethodData<void(const QString &, ModelType, double, double, double)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 }, { 0x80000000 | 10, 11 }, { QMetaType::Double, 12 }, { QMetaType::Double, 13 },
            { QMetaType::Double, 14 },
        }}),
        // Method 'removeEntity'
        QtMocHelpers::MethodData<bool(const QString &)>(17, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 6 },
        }}),
        // Method 'updateEntityPosition'
        QtMocHelpers::MethodData<bool(const QString &, double, double, double, double, double, double)>(18, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 6 }, { QMetaType::Double, 12 }, { QMetaType::Double, 13 }, { QMetaType::Double, 14 },
            { QMetaType::Double, 19 }, { QMetaType::Double, 20 }, { QMetaType::Double, 21 },
        }}),
        // Method 'updateEntityPosition'
        QtMocHelpers::MethodData<bool(const QString &, double, double, double, double, double)>(18, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::QString, 6 }, { QMetaType::Double, 12 }, { QMetaType::Double, 13 }, { QMetaType::Double, 14 },
            { QMetaType::Double, 19 }, { QMetaType::Double, 20 },
        }}),
        // Method 'updateEntityPosition'
        QtMocHelpers::MethodData<bool(const QString &, double, double, double, double)>(18, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::QString, 6 }, { QMetaType::Double, 12 }, { QMetaType::Double, 13 }, { QMetaType::Double, 14 },
            { QMetaType::Double, 19 },
        }}),
        // Method 'updateEntityPosition'
        QtMocHelpers::MethodData<bool(const QString &, double, double, double)>(18, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::QString, 6 }, { QMetaType::Double, 12 }, { QMetaType::Double, 13 }, { QMetaType::Double, 14 },
        }}),
        // Method 'getEntity'
        QtMocHelpers::MethodData<QVariantMap(const QString &) const>(22, 2, QMC::AccessPublic, 0x80000000 | 23, {{
            { QMetaType::QString, 6 },
        }}),
        // Method 'count'
        QtMocHelpers::MethodData<int() const>(24, 2, QMC::AccessPublic, QMetaType::Int),
        // Method 'clearAll'
        QtMocHelpers::MethodData<void()>(25, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'getEntitiesByType'
        QtMocHelpers::MethodData<QVariantList(const QString &) const>(26, 2, QMC::AccessPublic, 0x80000000 | 27, {{
            { QMetaType::QString, 28 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'maxReceivedTime'
        QtMocHelpers::PropertyData<double>(29, QMetaType::Double, QMC::DefaultPropertyFlags, 0),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SimDataManager, qt_meta_tag_ZN14SimDataManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SimDataManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractListModel::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14SimDataManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14SimDataManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14SimDataManagerE_t>.metaTypes,
    nullptr
} };

void SimDataManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SimDataManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->maxReceivedTimeChanged((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 1: _t->sceneResetRequested(); break;
        case 2: _t->entityAdded((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->entityRemoved((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->entityUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->addEntity((*reinterpret_cast<std::add_pointer_t<ModelType>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4]))); break;
        case 6: _t->addEntityWithName((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<ModelType>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[5]))); break;
        case 7: { bool _r = _t->removeEntity((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 8: { bool _r = _t->updateEntityPosition((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[7])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 9: { bool _r = _t->updateEntityPosition((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[6])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 10: { bool _r = _t->updateEntityPosition((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[5])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 11: { bool _r = _t->updateEntityPosition((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 12: { QVariantMap _r = _t->getEntity((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 13: { int _r = _t->count();
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 14: _t->clearAll(); break;
        case 15: { QVariantList _r = _t->getEntitiesByType((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SimDataManager::*)(double )>(_a, &SimDataManager::maxReceivedTimeChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SimDataManager::*)()>(_a, &SimDataManager::sceneResetRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SimDataManager::*)(const QString & )>(_a, &SimDataManager::entityAdded, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SimDataManager::*)(const QString & )>(_a, &SimDataManager::entityRemoved, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SimDataManager::*)(const QString & )>(_a, &SimDataManager::entityUpdated, 4))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<double*>(_v) = _t->maxReceivedTime(); break;
        default: break;
        }
    }
}

const QMetaObject *SimDataManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SimDataManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14SimDataManagerE_t>.strings))
        return static_cast<void*>(this);
    return QAbstractListModel::qt_metacast(_clname);
}

int SimDataManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractListModel::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 16;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void SimDataManager::maxReceivedTimeChanged(double _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void SimDataManager::sceneResetRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void SimDataManager::entityAdded(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void SimDataManager::entityRemoved(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void SimDataManager::entityUpdated(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
