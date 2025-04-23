#include "pack.h"


#define WhilePacking(n) __While__(packing, n)


#define INT4_MAX (1LL << 31)
#define INT4_MIN -INT4_MAX


#define INT2_MAX (1LL << 15)
#define INT2_MIN -INT2_MAX


#define INT1_MAX (1LL << 7)
#define INT1_MIN -INT1_MAX


/* -------------------------------------------------------------------------- */

static inline PyByteArrayObject *
__msg_alloc__(Py_ssize_t alloc)
{
    PyByteArrayObject *self = NULL;

    if ((self = PyObject_New(PyByteArrayObject, &PyByteArray_Type))) {
        if ((self->ob_bytes = PyObject_Malloc(alloc))) {
            self->ob_start = self->ob_bytes;
            self->ob_alloc = alloc;
            self->ob_exports = 0;
            Py_SIZE(self) = 0;
            self->ob_bytes[0] = '\0';
        }
        else {
            Py_CLEAR(self);
            PyErr_NoMemory();
        }
    }
    return self;
}


static inline int
__msg_realloc__(PyByteArrayObject *self, Py_ssize_t nalloc)
{
    Py_ssize_t alloc = 0;
    void *bytes = NULL;

    if (self->ob_alloc < nalloc) {
        alloc = Py_MAX(nalloc, (self->ob_alloc << 1));
        if (!(bytes = PyObject_Realloc(self->ob_bytes, alloc))) {
            return -1;
        }
        self->ob_start = self->ob_bytes = bytes;
        self->ob_alloc = alloc;
    }
    return 0;
}


#define __WRITE_BEGIN__ \
    size_t start = Py_SIZE(self), nsize = start + size; \
    if ((nsize >= PY_SSIZE_T_MAX) || __msg_realloc__(self, (nsize + 1))) { \
        PyErr_NoMemory(); \
        return -1; \
    }


#define __WRITE_END__ \
    Py_SIZE(self) = nsize; \
    self->ob_bytes[nsize] = '\0'; \
    return 0;


static inline int
__write_type__(
    PyByteArrayObject *self, uint8_t type
)
{
    size_t size = 1;

    __WRITE_BEGIN__

    self->ob_bytes[start] = type;

    __WRITE_END__
}


static inline int
__write_buffer__(
    PyByteArrayObject *self, uint8_t type,
    const void *buffer, size_t len
)
{
    size_t size = 1 + len;

    __WRITE_BEGIN__

    self->ob_bytes[start++] = type;
    memcpy((self->ob_bytes + start), buffer, len);

    __WRITE_END__
}


static inline int
__write_buffers__(
    PyByteArrayObject *self, uint8_t type,
    const void *buffer1, size_t len1,
    const void *buffer2, size_t len2
)
{
    size_t size = 1 + len1 + len2;

    __WRITE_BEGIN__

    self->ob_bytes[start++] = type;
    memcpy((self->ob_bytes + start), buffer1, len1);
    start += len1;
    memcpy((self->ob_bytes + start), buffer2, len2);

    __WRITE_END__
}


/* new_msg ------------------------------------------------------------------ */

PyObject *
__msg_new__(Py_ssize_t alloc)
{
    return _PyObject_CAST(__msg_alloc__(((alloc + 7) & ~7)));
}


PyObject *
new_msg(void)
{
    return __msg_new__(16);
}


/* -------------------------------------------------------------------------- */

static int
__store_type__(
    PyObject *msg, uint8_t type
)
{
    return __write_type__(
        (PyByteArrayObject *)msg, type
    );
}


static int
__store_buffer__(
    PyObject *msg, uint8_t type,
    const void *buffer, size_t len
)
{
    return __write_buffer__(
        (PyByteArrayObject *)msg, type, buffer, len
    );
}


static int
__store_buffers__(
    PyObject *msg, uint8_t type,
    const void *buffer1, size_t len1,
    const void *buffer2, size_t len2
)
{
    return __write_buffers__(
        (PyByteArrayObject *)msg, type, buffer1, len1, buffer2, len2
    );
}


/* -------------------------------------------------------------------------- */

static inline uint8_t
__positive_int_size__(int64_t v)
{
    return (v < INT2_MAX) ? ((v < INT1_MAX) ? 1 : 2) : ((v < INT4_MAX) ? 4 : 8);
}


static inline uint8_t
__negative_int_size__(int64_t v)
{
    return (v < INT2_MIN) ? ((v < INT4_MIN) ? 8 : 4) : ((v < INT1_MIN) ? 2 : 1);
}


#define __int_size__(v) \
    (((v) < 0) ? __negative_int_size__((v)) : __positive_int_size__((v)))


#define __len_size__ __positive_int_size__


/* -------------------------------------------------------------------------- */

static inline int
__store_len__(PyObject *msg, uint8_t type, Py_ssize_t len)
{
    uint8_t size = __len_size__(len);

    return __store_buffer__(msg, (type | size), &len, size);
}


static inline int
__store_data__(PyObject *msg, uint8_t type, const void *data, Py_ssize_t len)
{
    uint8_t size = __len_size__(len);

    return __store_buffers__(msg, (type | size), &len, size, data, len);
}


/* TYPE_INT / TYPE_UINT ----------------------------------------------------- */

static inline int
__store_int__(PyObject *msg, int64_t value)
{
    if ((value == -1) && PyErr_Occurred()) {
        return -1;
    }
    // for TYPE_INT size is type
    uint8_t size = __int_size__(value);
    return __store_buffer__(msg, size, &value, size);
}


static inline int
__store_uint__(PyObject *msg, uint64_t value)
{
    if ((value == (uint64_t)-1) && PyErr_Occurred()) {
        return -1;
    }
    return __store_buffer__(msg, TYPE_UINT, &value, 8);
}


static inline int
__pack_long__(PyObject *msg, PyObject *obj)
{
    int overflow = 0;
    int64_t value = PyLong_AsLongLongAndOverflow(obj, &overflow);

    if (overflow) {
        if (overflow < 0) {
            PyErr_SetString(PyExc_OverflowError, "int too big to convert");
            return -1;
        }
        return __store_uint__(msg, PyLong_AsUnsignedLongLong(obj));
    }
    return __store_int__(msg, value);
}


/* TYPE_FLOAT --------------------------------------------------------------- */

#define __store_float__(m, v) \
    __store_buffer__(m, TYPE_FLOAT, &v, 8)


static inline int
__pack_float__(PyObject *msg, PyObject *obj)
{
    float64_t fvalue = { .f = PyFloat_AS_DOUBLE(obj) };

    return __store_float__(msg, fvalue.i);
}


/* TYPE_COMPLEX ------------------------------------------------------------- */

#define __store_complex__(m, r, i) \
    __store_buffers__(m, TYPE_COMPLEX, &r, 8, &i, 8)


static inline int
__pack_complex__(PyObject *msg, PyObject *obj)
{
    Py_complex complex = ((PyComplexObject *)obj)->cval;
    float64_t freal = { .f = complex.real}, fimag = { .f = complex.imag};

    return __store_complex__(msg, freal.i, fimag.i);
}


/* TYPE_NONE / TYPE_TRUE / TYPE_FALSE --------------------------------------- */

#define __pack_none__(m) \
    __store_type__(m, TYPE_NONE)


#define __pack_true__(m) \
    __store_type__(m, TYPE_TRUE)


#define __pack_false__(m) \
    __store_type__(m, TYPE_FALSE)


/* TYPE_STR ----------------------------------------------------------------- */

static inline int
__pack_str__(PyObject *msg, PyObject *obj)
{
    const char *bytes = NULL;
    Py_ssize_t len = 0;

    if (!(bytes = PyUnicode_AsUTF8AndSize(obj, &len))) {
        return -1;
    }
    return __store_data__(msg, TYPE_STR, bytes, len);
}


/* TYPE_BYTES / TYPE_BYTEARRAY ---------------------------------------------- */

#define __store_bytes__(T, m, t, o) \
    __store_data__(m, t, T##_AS_STRING(o), T##_GET_SIZE(o))


#define __pack_bytes__(m, o) \
    __store_bytes__(PyBytes, m, TYPE_BYTES, o)


#define __pack_bytearray__(m, o) \
    __store_bytes__(PyByteArray, m, TYPE_BYTEARRAY, o)


/* TYPE_TUPLE / TYPE_LIST --------------------------------------------------- */

static int
__store_sequence__(
    PyObject *msg,
    uint8_t type,
    PyObject **items,
    Py_ssize_t len,
    const char *where
)
{
    Py_ssize_t i;
    int res = -1;

    if (!Py_EnterRecursiveCall(where)) {
        if (!__store_len__(msg, type, len)) {
            for (res = 0, i = 0; i < len; ++i) {
                if ((res = pack_object(msg, items[i]))) {
                    break;
                }
            }
        }
        Py_LeaveRecursiveCall();
    }
    return res;
}


#define __pack_sequence__(T, m, t, o, n) \
    __store_sequence__(m, t, _##T##_ITEMS(o), T##_GET_SIZE(o), WhilePacking(n))


#define __pack_tuple__(m, o) \
    __pack_sequence__(PyTuple, m, TYPE_TUPLE, o, "tuple")


#define __pack_list__(m, o) \
    __pack_sequence__(PyList, m, TYPE_LIST, o, "list")


/* TYPE_DICT ---------------------------------------------------------------- */

static int
__pack_dict__(PyObject *msg, PyObject *obj)
{
    Py_ssize_t pos = 0;
    PyObject *key = NULL, *val = NULL;
    int res = -1;

    if (!Py_EnterRecursiveCall(WhilePacking("dict"))) {
        if (!__store_len__(msg, TYPE_DICT, PyDict_GET_SIZE(obj))) {
            while ((res = PyDict_Next(obj, &pos, &key, &val))) {
                if (
                    (res = pack_object(msg, key)) ||
                    (res = pack_object(msg, val))
                ) {
                    break;
                }
            }
        }
        Py_LeaveRecursiveCall();
    }
    return res;
}


/* TYPE_SET / TYPE_FROZENSET ------------------------------------------------ */

static int
__store_anyset__(PyObject *msg, uint8_t type, PyObject *obj, const char *where)
{
    Py_ssize_t pos = 0;
    PyObject *item = NULL;
    Py_hash_t hash;
    int res = -1;

    if (!Py_EnterRecursiveCall(where)) {
        if (!__store_len__(msg, type, PySet_GET_SIZE(obj))) {
            while ((res = _PySet_NextEntry(obj, &pos, &item, &hash))) {
                if ((res = pack_object(msg, item))) {
                    break;
                }
            }
        }
        Py_LeaveRecursiveCall();
    }
    return res;
}


#define __pack_anyset__(m, t, o, n) \
    __store_anyset__(m, t, o, WhilePacking(n))


#define __pack_set__(m, o) \
    __pack_anyset__(m, TYPE_SET, o, "set")


#define __pack_frozenset__(m, o) \
    __pack_anyset__(m, TYPE_FROZENSET, o, "frozenset")


/* -------------------------------------------------------------------------- */

static inline int
__store_class_id__(PyObject *msg, PyObject *obj)
{
    PyObject *module = NULL, *qualname = NULL;
    int res = -1;

    if (
        (module = _PyObject_GetAttrId(obj, &PyId___module__)) &&
        (qualname = _PyObject_GetAttrId(obj, &PyId___qualname__))
    ) {
        if (PyUnicode_CheckExact(module) && PyUnicode_CheckExact(qualname)) {
            res = __pack_str__(msg, module) ? -1 : __pack_str__(msg, qualname);
        }
        else {
            PyErr_Format(
                PyExc_TypeError,
                "expected strings, got: __module__: %.200s, __qualname__: %.200s",
                Py_TYPE(module)->tp_name,
                Py_TYPE(qualname)->tp_name
            );
        }
    }
    Py_XDECREF(qualname);
    Py_XDECREF(module);
    return res;
}


static inline int
__store_singleton_id__(PyObject *msg, PyObject *obj)
{
    PyObject *reduce = NULL;
    int res = -1;

    if ((reduce = _PyObject_CallMethodId(obj, &PyId___reduce__, NULL))) {
        if (PyUnicode_CheckExact(reduce)) {
            res = __pack_str__(msg, reduce);
        }
        else {
            PyErr_SetString(PyExc_TypeError, "__reduce__() must return a str");
        }
        Py_DECREF(reduce);
    }
    return res;
}


#define __store_id__(m, o) \
    (PyType_Check(o) ? __store_class_id__(m, o) : __store_singleton_id__(m, o))


/* TYPE_CLASS --------------------------------------------------------------- */

#define __store_class__(m, d, o) \
    ((__store_class_id__(d, o)) ? -1 : __store_bytes__(PyByteArray, m, TYPE_CLASS, d))


static inline int
__pack_class__(PyObject *msg, PyObject *obj)
{
    PyObject *data = NULL;
    int res = -1;

    if ((data = new_msg())) {
        res = __store_class__(msg, data, obj);
        Py_DECREF(data);
    }
    return res;
}


/* TYPE_SINGLETON / TYPE_INSTANCE ------------------------------------------- */

#define __store_reduce_singleton__(m, o) \
    (__pack_str__(m, o) ? TYPE_INVALID : TYPE_SINGLETON)


#define __store_reduce_instance__(m, o) \
    (__pack_tuple__(m, o) ? TYPE_INVALID : TYPE_INSTANCE)


#define __store_instance__(m, t, o) \
    __store_bytes__(PyByteArray, m, t, o)


static inline int
__pack_instance__(PyObject *msg, PyObject *obj, const char *name)
{
    PyObject *reduce = NULL, *data = NULL;
    uint8_t type = TYPE_INVALID; // 0
    int res = -1;

    if ((reduce = _PyObject_CallMethodId(obj, &PyId___reduce__, NULL))) {
        if ((data = new_msg())) {
            if (PyUnicode_CheckExact(reduce)) {
                type = __store_reduce_singleton__(data, reduce);
            }
            else if (PyTuple_CheckExact(reduce)) {
                type = __store_reduce_instance__(data, reduce);
            }
            else {
                PyErr_SetString(
                    PyExc_TypeError, "__reduce__() must return a str or a tuple"
                );
            }
            if (type) {
                res = __store_instance__(msg, type, data);
            }
            Py_DECREF(data);
        }
        Py_DECREF(reduce);
    }
    else if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
        PyErr_Clear();
        PyErr_Format(PyExc_TypeError, "cannot pack '%.200s' objects", name);
    }
    return res;
}


/* pack_object -------------------------------------------------------------- */

static inline int
__pack_object__(PyObject *msg, PyObject *obj, PyTypeObject *type)
{
    int res = -1;

    if (type == &PyLong_Type) {
        res = __pack_long__(msg, obj);
    }
    else if (type == &PyFloat_Type) {
        res = __pack_float__(msg, obj);
    }
    else if (type == &PyComplex_Type) {
        res = __pack_complex__(msg, obj);
    }
    else if (type == &PyUnicode_Type) {
        res = __pack_str__(msg, obj);
    }
    else if (type == &PyBytes_Type) {
        res = __pack_bytes__(msg, obj);
    }
    else if (type == &PyByteArray_Type) {
        res = __pack_bytearray__(msg, obj);
    }
    else if (type == &PyTuple_Type) {
        res = __pack_tuple__(msg, obj);
    }
    else if (type == &PyList_Type) {
        res = __pack_list__(msg, obj);
    }
    else if (type == &PyDict_Type) {
        res = __pack_dict__(msg, obj);
    }
    else if (type == &PySet_Type) {
        res = __pack_set__(msg, obj);
    }
    else if (type == &PyFrozenSet_Type) {
        res = __pack_frozenset__(msg, obj);
    }
    else if (type == &PyType_Type) {
        res = __pack_class__(msg, obj);
    }
    else {
        res = __pack_instance__(msg, obj, type->tp_name);
    }
    return res;
}


int
pack_object(PyObject *msg, PyObject *obj)
{
    int res = -1;

    if (obj == Py_None) {
        res = __pack_none__(msg);
    }
    else if (obj == Py_True) {
        res = __pack_true__(msg);
    }
    else if (obj == Py_False) {
        res = __pack_false__(msg);
    }
    else {
        res = __pack_object__(msg, obj, Py_TYPE(obj));
    }
    return res;
}


/* register_object ---------------------------------------------------------- */

/* because bytearrays, being mutable, cannot be used as dict keys */
#define _PyBytes_FromPyByteArray(o) \
    PyBytes_FromStringAndSize(PyByteArray_AS_STRING(o), PyByteArray_GET_SIZE(o))


int
register_object(PyObject *obj)
{
    PyObject *msg = NULL, *key = NULL;
    int res = -1;

    if ((msg = new_msg())) {
        if (
            !__store_id__(msg, obj) &&
            (key = _PyBytes_FromPyByteArray(msg))
        ) {
            res = PyDict_SetItem(Registry, key, obj);
            Py_DECREF(key);
        }
        Py_DECREF(msg);
    }
    return res;
}


/* __pack_message__ --------------------------------------------------------- */

PyObject *
__pack_message__(PyObject *msg)
{
    PyObject *result = NULL;
    Py_ssize_t len = PyByteArray_GET_SIZE(msg);
    uint8_t size = __len_size__(len);

    if (
        (result = __msg_new__(2 + size + len)) &&
        __store_buffers__(
            result, size, &len, size, PyByteArray_AS_STRING(msg), len
        )
    ) {
        Py_CLEAR(result);
    }
    return result;
}
