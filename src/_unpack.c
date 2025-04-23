#include "pack.h"


#define WhileUnpacking(n) __While__(unpacking, n)


/* -------------------------------------------------------------------------- */

#define __read_int1__(b) (*((int8_t *)b))
#define __read_int2__(b) (*((int16_t *)b))
#define __read_int4__(b) (*((int32_t *)b))
#define __read_int8__(b) (*((int64_t *)b))


#define __read_uint8__(b) (*((uint64_t *)b))


static inline double
__read_float8__(const char *buffer)
{
    float64_t value = { .i = __read_uint8__(buffer) };

    return value.f;
}


/* -------------------------------------------------------------------------- */

static inline const char *
__read_buffer__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    Py_ssize_t orig_offset = *offset, new_offset = orig_offset + size;

    if (new_offset > msg->len) {
        PyErr_SetString(PyExc_EOFError, "Ran out of input");
        return NULL;
    }
    *offset = new_offset;
    return ((msg->buf) + orig_offset);
}


static inline uint8_t
__read_type__(Py_buffer *msg, Py_ssize_t *offset)
{
    const char *buffer = NULL;

    if ((buffer = __read_buffer__(msg, offset, 1))) {
        return (*((uint8_t *)buffer));
    }
    return TYPE_INVALID;
}


/* -------------------------------------------------------------------------- */

#define __unwrap_int__(b, s) \
    PyLong_FromLongLong(__read_int##s##__(b))


#define __unwrap_uint__(b, s) \
    PyLong_FromUnsignedLongLong(__read_uint##s##__(b))


#define __unwrap_float__(b, s) \
    PyFloat_FromDouble(__read_float##s##__(b))


#define __unwrap_complex__(b, s) \
    PyComplex_FromDoubles(__read_float8__(b), __read_float8__((b + 8)))


#define __unwrap_str__(b, s) \
    PyUnicode_FromStringAndSize(b, s)


#define __unwrap_bytes__(b, s) \
    PyBytes_FromStringAndSize(b, s)


#define __unwrap_bytearray__(b, s) \
    PyByteArray_FromStringAndSize(b, s)


static inline int
__unwrap_sequence__(
    Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size, PyObject **items
)
{
    PyObject *item = NULL;
    Py_ssize_t i;
    int res = 0;

    for (i = 0; i < size; ++i) {
        if (
            (
                res = (
                    (
                        item = unpack_msg(msg, offset)
                    ) ? 0 : -1
                )
            )
        ) {
            break;
        }
        items[i] = item; // steals ref
    }
    return res;
}


static inline int
__unwrap_dict__(
    Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size, PyObject *items
)
{
    PyObject *key = NULL, *value = NULL;
    Py_ssize_t i;
    int res = 0;

    for (i = 0; i < size; ++i) {
        if (
            (
                res = (
                    (
                        (key = unpack_msg(msg, offset)) &&
                        (value = unpack_msg(msg, offset))
                    ) ? PyDict_SetItem(items, key, value) : -1
                )
            )
        ) {
            Py_XDECREF(value);
            Py_XDECREF(key);
            break;
        }
        Py_DECREF(value);
        Py_DECREF(key);
    }
    return res;
}


static inline int
__unwrap_anyset__(
    Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size, PyObject *items
)
{
    PyObject *item = NULL;
    Py_ssize_t i;
    int res = 0;

    for (i = 0; i < size; ++i) {
        if (
            (
                res = (
                    (
                        item = unpack_msg(msg, offset)
                    ) ? PySet_Add(items, item) : -1
                )
            )
        ) {
            Py_XDECREF(item);
            break;
        }
        Py_DECREF(item);
    }
    return res;
}


/* -------------------------------------------------------------------------- */

#define __unpack_object__(t, m, o, s) \
    ((buffer = __read_buffer__(m, o, s)) ? __unwrap_##t##__(buffer, s) : NULL)


#define __unpack_int__(m, o, s) __unpack_object__(int, m, o, s)


#define __unpack_uint__(m, o, s) __unpack_object__(uint, m, o, s)


#define __unpack_float__(m, o, s) __unpack_object__(float, m, o, s)


#define __unpack_complex__(m, o, s) __unpack_object__(complex, m, o, s)


#define __unpack_str__(m, o, s) __unpack_object__(str, m, o, s)


#define __unpack_bytes__(m, o, s) __unpack_object__(bytes, m, o, s)


#define __unpack_bytearray__(m, o, s) __unpack_object__(bytearray, m, o, s)


static PyObject *
__unpack_tuple__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    PyObject *result = NULL;

    if (!Py_EnterRecursiveCall(WhileUnpacking("tuple"))) {
        if (
            (result = PyTuple_New(size)) &&
            __unwrap_sequence__(msg, offset, size, _PyTuple_ITEMS(result))
        ) {
            Py_CLEAR(result);
        }
        Py_LeaveRecursiveCall();
    }
    return result;
}


static PyObject *
__unpack_list__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    PyObject *result = NULL;

    if (!Py_EnterRecursiveCall(WhileUnpacking("list"))) {
        if (
            (result = PyList_New(size)) &&
            __unwrap_sequence__(msg, offset, size, _PyList_ITEMS(result))
        ) {
            Py_CLEAR(result);
        }
        Py_LeaveRecursiveCall();
    }
    return result;
}


static PyObject *
__unpack_dict__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    PyObject *result = NULL;

    if (!Py_EnterRecursiveCall(WhileUnpacking("dict"))) {
        if (
            (result = PyDict_New()) &&
            __unwrap_dict__(msg, offset, size, result)
        ) {
            Py_CLEAR(result);
        }
        Py_LeaveRecursiveCall();
    }
    return result;
}


static PyObject *
__unpack_set__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    PyObject *result = NULL;

    if (!Py_EnterRecursiveCall(WhileUnpacking("set"))) {
        if (
            (result = PySet_New(NULL)) &&
            __unwrap_anyset__(msg, offset, size, result)
        ) {
            Py_CLEAR(result);
        }
        Py_LeaveRecursiveCall();
    }
    return result;
}


static PyObject *
__unpack_frozenset__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    PyObject *result = NULL;

    if (!Py_EnterRecursiveCall(WhileUnpacking("frozenset"))) {
        if (
            (result = PyFrozenSet_New(NULL)) &&
            __unwrap_anyset__(msg, offset, size, result)
        ) {
            Py_CLEAR(result);
        }
        Py_LeaveRecursiveCall();
    }
    return result;
}


/* -------------------------------------------------------------------------- */

static PyObject *
__unpack_registered__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    const char *buffer = NULL;
    PyObject *result = NULL, *key = NULL;

    if (
        (buffer = __read_buffer__(msg, offset, size)) &&
        (key = PyBytes_FromStringAndSize(buffer, size))
    ) {
        if ((result = PyDict_GetItem(Registry, key))) { // borrowed
            Py_INCREF(result);
        }
        Py_DECREF(key);
    }
    return result;
}


/* -------------------------------------------------------------------------- */

static inline void
__unpack_class_error__(Py_buffer *msg, Py_ssize_t *offset)
{
    _Py_IDENTIFIER(builtins);
    PyObject *module = NULL, *qualname = NULL;

    if (
        (module = unpack_msg(msg, offset)) &&
        (qualname = unpack_msg(msg, offset))
    ) {
        if (!_PyUnicode_EqualToASCIIId(module, &PyId_builtins)) {
            PyErr_Format(
                PyExc_TypeError,
                "cannot unpack <class '%U.%U'>",
                module,
                qualname
            );
        }
        else {
            PyErr_Format(
                PyExc_TypeError, "cannot unpack <class '%U'>", qualname
            );
        }
    }
    Py_XDECREF(qualname);
    Py_XDECREF(module);
}


static PyObject *
__unpack_class__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    // keep the original offset in case of error
    Py_ssize_t orig_offset = *offset;
    PyObject *result = NULL;

    if (
        !(result = __unpack_registered__(msg, offset, size)) &&
        !PyErr_Occurred()
    ) {
        __unpack_class_error__(msg, &orig_offset);
    }
    return result;
}


/* -------------------------------------------------------------------------- */

static inline void
__unpack_singleton_error__(Py_buffer *msg, Py_ssize_t *offset)
{
    PyObject *name = NULL;

    if ((name = unpack_msg(msg, offset))) {
        PyErr_Format(PyExc_TypeError, "cannot unpack '%U'", name);
        Py_DECREF(name);
    }
}


static PyObject *
__unpack_singleton__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    // keep the original offset in case of error
    Py_ssize_t orig_offset = *offset;
    PyObject *result = NULL;

    if (
        !(result = __unpack_registered__(msg, offset, size)) &&
        !PyErr_Occurred()
    ) {
        __unpack_singleton_error__(msg, &orig_offset);
    }
    return result;
}


/* -------------------------------------------------------------------------- */

static PyObject *
__unpack_instance__(Py_buffer *msg, Py_ssize_t *offset, Py_ssize_t size)
{
    PyObject *result = NULL, *reduce = NULL;

    if ((reduce = unpack_msg(msg, offset))) {
        result = __instance_new__(reduce);
        Py_DECREF(reduce);
    }
    return result;
}


/* unpack_msg --------------------------------------------------------------- */

#define __read_size__(m, o, s) \
    ((buffer = __read_buffer__(m, o, s)) ? __read_int##s##__(buffer) : -1)


#define __unpack_sized__(t, m, o, s) \
    (((size = __read_size__(m, o, s)) < 0) ? NULL : __unpack_##t##__(m, o, size))


#define __SIZED_CASE__(T, t, s) \
    case (T | s): \
        result = __unpack_sized__(t, msg, offset, s); \
        break;

#define SIZED_CASE(T, t) \
    __SIZED_CASE__(T, t, 1) \
    __SIZED_CASE__(T, t, 2) \
    __SIZED_CASE__(T, t, 4) \
    __SIZED_CASE__(T, t, 8)


PyObject *
unpack_msg(Py_buffer *msg, Py_ssize_t *offset)
{
    uint8_t type = TYPE_INVALID;
    const char *buffer = NULL;
    Py_ssize_t size = -1;
    PyObject *result = NULL;

    switch ((type = __read_type__(msg, offset))) {
        case TYPE_INVALID:
            if (!PyErr_Occurred()) {
                PyErr_Format(PyExc_TypeError, "invalid type: '0x%02x'", type);
            }
            break;
        case TYPE_INT1:
            result = __unpack_int__(msg, offset, 1);
            break;
        case TYPE_INT2:
            result = __unpack_int__(msg, offset, 2);
            break;
        case TYPE_INT4:
            result = __unpack_int__(msg, offset, 4);
            break;
        case TYPE_INT8:
            result = __unpack_int__(msg, offset, 8);
            break;
        case TYPE_UINT:
            result = __unpack_uint__(msg, offset, 8);
            break;
        case TYPE_FLOAT:
            result = __unpack_float__(msg, offset, 8);
            break;
        case TYPE_COMPLEX:
            result = __unpack_complex__(msg, offset, 16);
            break;
        case TYPE_NONE:
            result = Py_NewRef(Py_None);
            break;
        case TYPE_TRUE:
            result = Py_NewRef(Py_True);
            break;
        case TYPE_FALSE:
            result = Py_NewRef(Py_False);
            break;
        SIZED_CASE(TYPE_STR, str)
        SIZED_CASE(TYPE_BYTES, bytes)
        SIZED_CASE(TYPE_BYTEARRAY, bytearray)
        SIZED_CASE(TYPE_TUPLE, tuple)
        SIZED_CASE(TYPE_LIST, list)
        SIZED_CASE(TYPE_DICT, dict)
        SIZED_CASE(TYPE_SET, set)
        SIZED_CASE(TYPE_FROZENSET, frozenset)
        SIZED_CASE(TYPE_CLASS, class)
        SIZED_CASE(TYPE_SINGLETON, singleton)
        SIZED_CASE(TYPE_INSTANCE, instance)
        default:
            PyErr_Format(PyExc_TypeError, "unknown type: '0x%02x'", type);
            break;
    }
    return result;
}


/* __unpack_size__ ---------------------------------------------------------- */

PyObject *
__unpack_size__(Py_buffer *msg)
{
    Py_ssize_t size = -1, len = msg->len;
    const char *buf = msg->buf;

    switch (len) {
        case 1:
            size = __read_int1__(buf);
            break;
        case 2:
            size = __read_int2__(buf);
            break;
        case 4:
            size = __read_int4__(buf);
            break;
        case 8:
            size = __read_int8__(buf);
            break;
        default:
            return PyErr_Format(
                PyExc_ValueError, "invalid buffer len: %zd", len
            );
    }
    return PyLong_FromSsize_t(size);
}
