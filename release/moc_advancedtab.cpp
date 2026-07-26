/****************************************************************************
** Meta object code from reading C++ file 'advancedtab.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.6.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../advancedtab.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'advancedtab.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.6.1. It"
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

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSAdvancedTabENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSAdvancedTabENDCLASS = QtMocHelpers::stringData(
    "AdvancedTab",
    "on_imgHOffsetSpinbox_valueChanged",
    "",
    "arg1",
    "on_rectWidthSlider_valueChanged",
    "value",
    "on_rectWidthSpinbox_valueChanged",
    "on_rectWidthSlider_sliderReleased"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSAdvancedTabENDCLASS_t {
    uint offsetsAndSizes[16];
    char stringdata0[12];
    char stringdata1[34];
    char stringdata2[1];
    char stringdata3[5];
    char stringdata4[32];
    char stringdata5[6];
    char stringdata6[33];
    char stringdata7[34];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSAdvancedTabENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSAdvancedTabENDCLASS_t qt_meta_stringdata_CLASSAdvancedTabENDCLASS = {
    {
        QT_MOC_LITERAL(0, 11),  // "AdvancedTab"
        QT_MOC_LITERAL(12, 33),  // "on_imgHOffsetSpinbox_valueCha..."
        QT_MOC_LITERAL(46, 0),  // ""
        QT_MOC_LITERAL(47, 4),  // "arg1"
        QT_MOC_LITERAL(52, 31),  // "on_rectWidthSlider_valueChanged"
        QT_MOC_LITERAL(84, 5),  // "value"
        QT_MOC_LITERAL(90, 32),  // "on_rectWidthSpinbox_valueChanged"
        QT_MOC_LITERAL(123, 33)   // "on_rectWidthSlider_sliderRele..."
    },
    "AdvancedTab",
    "on_imgHOffsetSpinbox_valueChanged",
    "",
    "arg1",
    "on_rectWidthSlider_valueChanged",
    "value",
    "on_rectWidthSpinbox_valueChanged",
    "on_rectWidthSlider_sliderReleased"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSAdvancedTabENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   38,    2, 0x08,    1 /* Private */,
       4,    1,   41,    2, 0x08,    3 /* Private */,
       6,    1,   44,    2, 0x08,    5 /* Private */,
       7,    0,   47,    2, 0x08,    7 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject AdvancedTab::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_CLASSAdvancedTabENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSAdvancedTabENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSAdvancedTabENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AdvancedTab, std::true_type>,
        // method 'on_imgHOffsetSpinbox_valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'on_rectWidthSlider_valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'on_rectWidthSpinbox_valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'on_rectWidthSlider_sliderReleased'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void AdvancedTab::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AdvancedTab *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->on_imgHOffsetSpinbox_valueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->on_rectWidthSlider_valueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->on_rectWidthSpinbox_valueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->on_rectWidthSlider_sliderReleased(); break;
        default: ;
        }
    }
}

const QMetaObject *AdvancedTab::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AdvancedTab::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSAdvancedTabENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int AdvancedTab::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}
QT_WARNING_POP
