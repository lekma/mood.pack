#ifndef Py_MOOD_PACK_H
#define Py_MOOD_PACK_H


#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "structmember.h"

#include "helpers/helpers.h"


#ifdef __cplusplus
extern "C" {
#endif


#if !PY_LITTLE_ENDIAN
#error "mood.pack requires a little endian CPU host"
#endif /* PY_LITTLE_ENDIAN */


#if !defined(HAVE_LONG_LONG)
#error "mood.pack requires a 64 bits integer type"
#endif /* HAVE_LONG_LONG */


/* -------------------------------------------------------------------------- */

/* for float conversion */
typedef union {
    double f;
    uint64_t i;
} float64_t;


/* for use with Py_EnterRecursiveCall */
#define __While__(a, n) " while " #a " a " n


/* -------------------------------------------------------------------------- */

extern PyObject *Registry;

extern _Py_Identifier PyId___module__;
extern _Py_Identifier PyId___qualname__;
extern _Py_Identifier PyId___reduce__;


/* -------------------------------------------------------------------------- */

PyObject *new_msg(void);

int pack_object(PyObject *, PyObject *);
int register_object(PyObject *);
PyObject *__pack_message__(PyObject *);

PyObject *__instance_new__(PyObject *);
PyObject *unpack_msg(Py_buffer *, Py_ssize_t *);
PyObject *__unpack_size__(Py_buffer *);


/* types -------------------------------------------------------------------- */

enum {
    TYPE_INVALID   = 0x00,

    TYPE_INT1      = 0x01,
    TYPE_INT2      = 0x02,
    TYPE_INT4      = 0x04,
    TYPE_INT8      = 0x08,

    TYPE_UINT      = 0x11,
    TYPE_FLOAT     = 0x12,
    TYPE_COMPLEX   = 0x13,

    TYPE_NONE      = 0x21,
    TYPE_TRUE      = 0x22,
    TYPE_FALSE     = 0x23,

    TYPE_STR       = 0x30,
    TYPE_BYTES     = 0x40,
    TYPE_BYTEARRAY = 0x50,

    TYPE_TUPLE     = 0x60,
    TYPE_LIST      = 0x70,

    TYPE_DICT      = 0x80,

    TYPE_SET       = 0x90,
    TYPE_FROZENSET = 0xa0,

    TYPE_CLASS     = 0xd0,
    TYPE_SINGLETON = 0xe0,
    TYPE_INSTANCE  = 0xf0,
};


#ifdef __cplusplus
}
#endif


#endif // !Py_MOOD_PACK_H
