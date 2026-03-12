/****************************************************************************
** Meta object code from reading C++ file 'QmlBridge.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../../vsgQt-master/examples/vsgqtviewer/QmlBridge.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'QmlBridge.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9QmlBridgeE_t {};
} // unnamed namespace

template <> constexpr inline auto QmlBridge::qt_create_metaobjectdata<qt_meta_tag_ZN9QmlBridgeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "QmlBridge",
        "modelPlacementRequested",
        "",
        "fileName",
        "fullPath",
        "focusEntityRequested",
        "id",
        "targetUntethered",
        "cameraChanged",
        "perfChanged",
        "simTimeChanged",
        "simRangeChanged",
        "simAnimatingChanged",
        "simMultiplierChanged",
        "acmiStatsChanged",
        "maxBufferedProgressChanged",
        "requestAddModel",
        "modelType",
        "selectModel",
        "category",
        "focusEntity",
        "removeEntity",
        "cancelPlacement",
        "untether",
        "seekToProgress",
        "p",
        "timeStringAt",
        "setSimAnimating",
        "animating",
        "setSimMultiplier",
        "multiplier",
        "stopSim",
        "adjustTimelineCapacity",
        "delta",
        "longitude",
        "latitude",
        "altitude",
        "fov",
        "pitch",
        "heading",
        "viewHeight",
        "scaleText",
        "frameMs",
        "fps",
        "simProgress",
        "simCurrentTime",
        "simTotalSeconds",
        "simStartTime",
        "simStopTime",
        "simAnimating",
        "simMultiplier",
        "acmiPacketCount",
        "acmiBufferMode",
        "maxBufferedProgress"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'modelPlacementRequested'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::QString, 4 },
        }}),
        // Signal 'focusEntityRequested'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'targetUntethered'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'cameraChanged'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'perfChanged'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'simTimeChanged'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'simRangeChanged'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'simAnimatingChanged'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'simMultiplierChanged'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'acmiStatsChanged'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'maxBufferedProgressChanged'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'requestAddModel'
        QtMocHelpers::MethodData<void(const QString &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
        // Method 'selectModel'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::QString, 19 }, { QMetaType::QString, 4 },
        }}),
        // Method 'focusEntity'
        QtMocHelpers::MethodData<void(const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Method 'removeEntity'
        QtMocHelpers::MethodData<void(const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Method 'cancelPlacement'
        QtMocHelpers::MethodData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'untether'
        QtMocHelpers::MethodData<void()>(23, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'seekToProgress'
        QtMocHelpers::MethodData<void(double)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 25 },
        }}),
        // Method 'timeStringAt'
        QtMocHelpers::MethodData<QString(double) const>(26, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::Double, 25 },
        }}),
        // Method 'setSimAnimating'
        QtMocHelpers::MethodData<void(bool)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 28 },
        }}),
        // Method 'setSimMultiplier'
        QtMocHelpers::MethodData<void(double)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 30 },
        }}),
        // Method 'stopSim'
        QtMocHelpers::MethodData<void()>(31, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'adjustTimelineCapacity'
        QtMocHelpers::MethodData<void(double)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 33 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'longitude'
        QtMocHelpers::PropertyData<double>(34, QMetaType::Double, QMC::DefaultPropertyFlags, 3),
        // property 'latitude'
        QtMocHelpers::PropertyData<double>(35, QMetaType::Double, QMC::DefaultPropertyFlags, 3),
        // property 'altitude'
        QtMocHelpers::PropertyData<double>(36, QMetaType::Double, QMC::DefaultPropertyFlags, 3),
        // property 'fov'
        QtMocHelpers::PropertyData<double>(37, QMetaType::Double, QMC::DefaultPropertyFlags, 3),
        // property 'pitch'
        QtMocHelpers::PropertyData<double>(38, QMetaType::Double, QMC::DefaultPropertyFlags, 3),
        // property 'heading'
        QtMocHelpers::PropertyData<double>(39, QMetaType::Double, QMC::DefaultPropertyFlags, 3),
        // property 'viewHeight'
        QtMocHelpers::PropertyData<double>(40, QMetaType::Double, QMC::DefaultPropertyFlags, 3),
        // property 'scaleText'
        QtMocHelpers::PropertyData<QString>(41, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'frameMs'
        QtMocHelpers::PropertyData<double>(42, QMetaType::Double, QMC::DefaultPropertyFlags, 4),
        // property 'fps'
        QtMocHelpers::PropertyData<int>(43, QMetaType::Int, QMC::DefaultPropertyFlags, 4),
        // property 'simProgress'
        QtMocHelpers::PropertyData<double>(44, QMetaType::Double, QMC::DefaultPropertyFlags, 5),
        // property 'simCurrentTime'
        QtMocHelpers::PropertyData<QString>(45, QMetaType::QString, QMC::DefaultPropertyFlags, 5),
        // property 'simTotalSeconds'
        QtMocHelpers::PropertyData<double>(46, QMetaType::Double, QMC::DefaultPropertyFlags, 6),
        // property 'simStartTime'
        QtMocHelpers::PropertyData<QString>(47, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'simStopTime'
        QtMocHelpers::PropertyData<QString>(48, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'simAnimating'
        QtMocHelpers::PropertyData<bool>(49, QMetaType::Bool, QMC::DefaultPropertyFlags, 7),
        // property 'simMultiplier'
        QtMocHelpers::PropertyData<double>(50, QMetaType::Double, QMC::DefaultPropertyFlags, 8),
        // property 'acmiPacketCount'
        QtMocHelpers::PropertyData<int>(51, QMetaType::Int, QMC::DefaultPropertyFlags, 9),
        // property 'acmiBufferMode'
        QtMocHelpers::PropertyData<bool>(52, QMetaType::Bool, QMC::DefaultPropertyFlags, 9),
        // property 'maxBufferedProgress'
        QtMocHelpers::PropertyData<double>(53, QMetaType::Double, QMC::DefaultPropertyFlags, 10),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<QmlBridge, qt_meta_tag_ZN9QmlBridgeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject QmlBridge::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9QmlBridgeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9QmlBridgeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9QmlBridgeE_t>.metaTypes,
    nullptr
} };

void QmlBridge::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QmlBridge *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->modelPlacementRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->focusEntityRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->targetUntethered(); break;
        case 3: _t->cameraChanged(); break;
        case 4: _t->perfChanged(); break;
        case 5: _t->simTimeChanged(); break;
        case 6: _t->simRangeChanged(); break;
        case 7: _t->simAnimatingChanged(); break;
        case 8: _t->simMultiplierChanged(); break;
        case 9: _t->acmiStatsChanged(); break;
        case 10: _t->maxBufferedProgressChanged(); break;
        case 11: _t->requestAddModel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->selectModel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 13: _t->focusEntity((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->removeEntity((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: _t->cancelPlacement(); break;
        case 16: _t->untether(); break;
        case 17: _t->seekToProgress((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 18: { QString _r = _t->timeStringAt((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 19: _t->setSimAnimating((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 20: _t->setSimMultiplier((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 21: _t->stopSim(); break;
        case 22: _t->adjustTimelineCapacity((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)(const QString & , const QString & )>(_a, &QmlBridge::modelPlacementRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)(const QString & )>(_a, &QmlBridge::focusEntityRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)()>(_a, &QmlBridge::targetUntethered, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)()>(_a, &QmlBridge::cameraChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)()>(_a, &QmlBridge::perfChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)()>(_a, &QmlBridge::simTimeChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)()>(_a, &QmlBridge::simRangeChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)()>(_a, &QmlBridge::simAnimatingChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)()>(_a, &QmlBridge::simMultiplierChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)()>(_a, &QmlBridge::acmiStatsChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (QmlBridge::*)()>(_a, &QmlBridge::maxBufferedProgressChanged, 10))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<double*>(_v) = _t->longitude(); break;
        case 1: *reinterpret_cast<double*>(_v) = _t->latitude(); break;
        case 2: *reinterpret_cast<double*>(_v) = _t->altitude(); break;
        case 3: *reinterpret_cast<double*>(_v) = _t->fov(); break;
        case 4: *reinterpret_cast<double*>(_v) = _t->pitch(); break;
        case 5: *reinterpret_cast<double*>(_v) = _t->heading(); break;
        case 6: *reinterpret_cast<double*>(_v) = _t->viewHeight(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->scaleText(); break;
        case 8: *reinterpret_cast<double*>(_v) = _t->frameMs(); break;
        case 9: *reinterpret_cast<int*>(_v) = _t->fps(); break;
        case 10: *reinterpret_cast<double*>(_v) = _t->simProgress(); break;
        case 11: *reinterpret_cast<QString*>(_v) = _t->simCurrentTime(); break;
        case 12: *reinterpret_cast<double*>(_v) = _t->simTotalSeconds(); break;
        case 13: *reinterpret_cast<QString*>(_v) = _t->simStartTime(); break;
        case 14: *reinterpret_cast<QString*>(_v) = _t->simStopTime(); break;
        case 15: *reinterpret_cast<bool*>(_v) = _t->simAnimating(); break;
        case 16: *reinterpret_cast<double*>(_v) = _t->simMultiplier(); break;
        case 17: *reinterpret_cast<int*>(_v) = _t->acmiPacketCount(); break;
        case 18: *reinterpret_cast<bool*>(_v) = _t->acmiBufferMode(); break;
        case 19: *reinterpret_cast<double*>(_v) = _t->maxBufferedProgress(); break;
        default: break;
        }
    }
}

const QMetaObject *QmlBridge::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QmlBridge::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9QmlBridgeE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QmlBridge::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 23)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 23;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 23)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 23;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 20;
    }
    return _id;
}

// SIGNAL 0
void QmlBridge::modelPlacementRequested(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void QmlBridge::focusEntityRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void QmlBridge::targetUntethered()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void QmlBridge::cameraChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void QmlBridge::perfChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void QmlBridge::simTimeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void QmlBridge::simRangeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void QmlBridge::simAnimatingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void QmlBridge::simMultiplierChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void QmlBridge::acmiStatsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void QmlBridge::maxBufferedProgressChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}
QT_WARNING_POP
