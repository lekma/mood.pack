#include "pack.h"


/* -------------------------------------------------------------------------- */

PyObject *Registry = NULL;

_Py_Identifier PyId___module__ = _Py_static_string_init("__module__");
_Py_Identifier PyId___qualname__ = _Py_static_string_init("__qualname__");
_Py_Identifier PyId___reduce__ = _Py_static_string_init("__reduce__");


/* pack.pack(object) -------------------------------------------------------- */

/* returns the packed representation of *object* (*message*)
   as a bytearray object */

static inline PyObject *
__pack_pack__(PyObject *obj)
{
    PyObject *msg = NULL;

    if ((msg = new_msg()) && pack_object(msg, obj)) {
        Py_CLEAR(msg);
    }
    return msg;
}


static PyObject *
pack_pack(PyObject *module, PyObject *obj)
{
    return __pack_pack__(obj);
}


/* pack.register(*items) ---------------------------------------------------- */

/* adds each *item* to the *registry*. *item* must be a class or a singleton
  (instance whose ``__reduce__`` method returns a string) */

static PyObject *
pack_register(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    Py_ssize_t i;

    for (i = 0; i < nargs; ++i) {
        if (register_object(args[i])) {
            return NULL;
        }
    }
    Py_RETURN_NONE;
}


/* pack.unpack(message) ----------------------------------------------------- */

/* reads a packed object hierarchy from a `bytes-like` *message* and returns
   the reconstituted object hierarchy specified therein */

static PyObject *
pack_unpack(PyObject *module, PyObject *args)
{
    PyObject *result = NULL;
    Py_buffer msg;
    Py_ssize_t offset = 0;

    if (PyArg_ParseTuple(args, "y*:unpack", &msg)) {
        result = unpack_msg(&msg, &offset);
        PyBuffer_Release(&msg);
    }
    return result;
}


/* pack.message(object) ----------------------------------------------------- */

/* same as pack.pack(object) except it returns
   the packed representation of *object* (*message*) preceded by
   the packed representation of the length of *message*. */

static PyObject *
pack_message(PyObject *module, PyObject *obj)
{
    PyObject *result = NULL, *msg = NULL;

    if ((msg = __pack_pack__(obj))) {
        result = __pack_message__(msg);
        Py_DECREF(msg);
    }
    return result;
}


/* pack.size(message) ------------------------------------------------------- */

/* returns the size of a *message* produced by pack.message(object)
   example: pack.unpack(read(pack.size(read(read(1)[0])))) */

static PyObject *
pack_size(PyObject *module, PyObject *args)
{
    PyObject *result = NULL;
    Py_buffer msg;

    if (PyArg_ParseTuple(args, "y*:size", &msg)) {
        result = __unpack_size__(&msg);
        PyBuffer_Release(&msg);
    }
    return result;
}


/* pack.packable(callable) -------------------------------------------------- */

/* decorator, useful when 6th item returned by the ``__reduce__`` method
   needs to be packable */

static PyObject *
__pack_reduce__(PyObject *self)
{
    PyObject *module = NULL, *qualname = NULL, *result = NULL;

    if (
        (module = _PyObject_GetAttrId(self, &PyId___module__)) &&
        (qualname = _PyObject_GetAttrId(self, &PyId___qualname__))
    ) {
        if (PyUnicode_CheckExact(module) && PyUnicode_CheckExact(qualname)) {
            result = PyUnicode_FromFormat("%U.%U", module, qualname);
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
    return result;
}


static PyMethodDef __pack_reduce_method__ = {
    "__reduce__", (PyCFunction)__pack_reduce__, METH_NOARGS, NULL
};


static PyObject *
pack_packable(PyObject *module, PyObject *obj)
{
    PyObject *_reduce_ = NULL, *result = NULL;

    if (PyCallable_Check(obj)) {
        _reduce_ = PyCFunction_New(&__pack_reduce_method__, obj);
        if (_reduce_ && !_PyObject_SetAttrId(obj, &PyId___reduce__, _reduce_)) {
            result = Py_NewRef(obj);
        }
        Py_XDECREF(_reduce_);
    }
    else {
        PyErr_Format(
            PyExc_TypeError,
            "expected a callable, got %.200s",
            Py_TYPE(obj)->tp_name
        );
    }
    return result;
}


/* --------------------------------------------------------------------------
    module
   -------------------------------------------------------------------------- */

/* pack_def.m_methods */
static PyMethodDef pack_m_methods[] = {
    {"pack", (PyCFunction)pack_pack, METH_O, "pack(object) -> message"},
    {"register", _PyCFunction_CAST(pack_register), METH_FASTCALL, "register(*items)"},
    {"unpack", (PyCFunction)pack_unpack, METH_VARARGS, "unpack(message) -> object"},
    {"message", (PyCFunction)pack_message, METH_O, "message(object) -> message"},
    {"size", (PyCFunction)pack_size, METH_VARARGS, "size(message) -> int"},
    {"packable", (PyCFunction)pack_packable, METH_O, "packable(callable) -> callable"},
    {NULL} /* Sentinel */
};


/* pack_def.m_traverse */
static int
pack_m_traverse(PyObject *module, visitproc visit, void *arg)
{
    Py_VISIT(Registry);
    return 0;
}


/* pack_def.m_clear */
static int
pack_m_clear(PyObject *module)
{
    Py_CLEAR(Registry);
    return 0;
}


/* pack_def.m_free */
static void
pack_m_free(PyObject *module)
{
    pack_m_clear(module);
}


/* pack_def */
static PyModuleDef pack_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "pack",
    .m_doc = "mood pack module",
    .m_size = -1,
    .m_methods = pack_m_methods,
    .m_traverse = (traverseproc)pack_m_traverse,
    .m_clear = (inquiry)pack_m_clear,
    .m_free = (freefunc)pack_m_free,
};


/* module initialization */
static inline int
__module_init__(PyObject *module)
{
    if (
        !(Registry = PyDict_New()) ||
        register_object(Py_NotImplemented) ||
        register_object(Py_Ellipsis) ||
        PyModule_AddStringConstant(module, "__version__", PKG_VERSION)
    ) {
        Py_CLEAR(Registry);
        return -1;
    }
    return 0;
}

PyMODINIT_FUNC
PyInit_pack(void)
{
    PyObject *module = NULL;

    if ((module = PyState_FindModule(&pack_def))) {
        Py_INCREF(module);
    }
    else if (
        (module = PyModule_Create(&pack_def)) && __module_init__(module)
    ) {
        Py_CLEAR(module);
    }
    return module;
}
