from enum import IntEnum, IntFlag, unique
from struct import pack as __pack__
from sys import maxsize


# ------------------------------------------------------------------------------

@unique
class Types(IntFlag):
    TYPE_INVALID   = 0x00
    TYPE_INT1      = 0x01
    TYPE_INT2      = 0x02
    TYPE_INT4      = 0x04
    TYPE_INT8      = 0x08
    TYPE_UINT      = 0x11
    TYPE_FLOAT     = 0x12
    TYPE_COMPLEX   = 0x13
    TYPE_NONE      = 0x21
    TYPE_TRUE      = 0x22
    TYPE_FALSE     = 0x23
    TYPE_STR       = 0x30
    TYPE_BYTES     = 0x40
    TYPE_BYTEARRAY = 0x50
    TYPE_TUPLE     = 0x60
    TYPE_LIST      = 0x70
    TYPE_DICT      = 0x80
    TYPE_SET       = 0x90
    TYPE_FROZENSET = 0xa0
    TYPE_CLASS     = 0xd0
    TYPE_SINGLETON = 0xe0
    TYPE_INSTANCE  = 0xf0


# ------------------------------------------------------------------------------

@unique
class Limits(IntEnum):
    UINT_MAX = (1 << 64)    # 18446744073709551616
    INT8_MAX = (1 << 63)    # 9223372036854775808
    INT8_MIN = -INT8_MAX    # -9223372036854775808
    INT4_MAX = (1 << 31)    # 2147483648
    INT4_MIN = -INT4_MAX    # -2147483648
    INT2_MAX = (1 << 15)    # 32768
    INT2_MIN = -INT2_MAX    # -32768
    INT1_MAX = (1 << 7)     # 128
    INT1_MIN = -INT1_MAX    # -128


# ------------------------------------------------------------------------------

_obj_too_big = "{0.__name__} object too big to pack"
_cannot_pack = "cannot pack {0.__name__} objects"


def error(_type, msg, o):
    return _type(msg.format(type(o)))


# ------------------------------------------------------------------------------

def _pack_int1_(o):
    return __pack__("=Bb", Types.TYPE_INT1, o)

def _pack_int2_(o):
    return __pack__("=Bh", Types.TYPE_INT2, o)

def _pack_int4_(o):
    return __pack__("=Bi", Types.TYPE_INT4, o)

def _pack_int8_(o):
    return __pack__("=Bq", Types.TYPE_INT8, o)

def _pack_uint_(o):
    return __pack__("=BQ", Types.TYPE_UINT, o)

def _size_(_len):
    if _len < Limits.INT1_MAX:
        return (1, "b")
    if _len < Limits.INT2_MAX:
        return (2, "h")
    if _len < Limits.INT4_MAX:
        return (4, "i")
    return (8, "q")

def _pack_len_(_type, o):
    if ((_len := len(o)) < maxsize):
        _size, _fmt = _size_(_len)
        return __pack__(f"=B{_fmt}", (_type | _size), _len)
    raise error(OverflowError, _obj_too_big, o)

def _pack_data_(_type, o):
    if ((_len := len(o)) < maxsize):
        _size, _fmt = _size_(_len)
        return __pack__(f"=B{_fmt}{_len}s", (_type | _size), _len, o)
    raise error(OverflowError, _obj_too_big, o)

def _pack_sequence_(_type, o):
    return b"".join((_pack_len_(_type, o), *(pack(v) for v in o)))


# ------------------------------------------------------------------------------

def _pack_int_(o):
    if o >= Limits.INT8_MIN:
        if o < Limits.INT4_MIN:
            return _pack_int8_(o)
        if o < Limits.INT2_MIN:
            return _pack_int4_(o)
        if o < Limits.INT1_MIN:
            return _pack_int2_(o)
        if o < Limits.INT1_MAX:
            return _pack_int1_(o)
        if o < Limits.INT2_MAX:
            return _pack_int2_(o)
        if o < Limits.INT4_MAX:
            return _pack_int4_(o)
        if o < Limits.INT8_MAX:
            return _pack_int8_(o)
        if o < Limits.UINT_MAX:
            return _pack_uint_(o)
    raise error(OverflowError, _obj_too_big, o)

def _pack_float_(o):
    return __pack__("=Bd", Types.TYPE_FLOAT, o)

def _pack_complex_(o):
    return __pack__("=Bdd", Types.TYPE_COMPLEX, o.real, o.imag)

def _pack_none_():
    return __pack__("=B", Types.TYPE_NONE)

def _pack_true_():
    return __pack__("=B", Types.TYPE_TRUE)

def _pack_false_():
    return __pack__("=B", Types.TYPE_FALSE)

def _pack_str_(o):
    return _pack_data_(Types.TYPE_STR, o.encode("utf-8"))

def _pack_bytes_(o):
    return _pack_data_(Types.TYPE_BYTES, o)

def _pack_bytearray_(o):
    return _pack_data_(Types.TYPE_BYTEARRAY, o)

def _pack_tuple_(o):
    return _pack_sequence_(Types.TYPE_TUPLE, o)

def _pack_list_(o):
    return _pack_sequence_(Types.TYPE_LIST, o)

def _pack_dict_(o):
    return b"".join(
        (
            _pack_len_(Types.TYPE_DICT, o),
            *(b"".join((pack(k), pack(v))) for k, v in o.items())
        )
    )

def _pack_set_(o):
    return _pack_sequence_(Types.TYPE_SET, o)

def _pack_frozenset_(o):
    return _pack_sequence_(Types.TYPE_FROZENSET, o)

def _pack_class_(o):
    return _pack_data_(
        Types.TYPE_CLASS,
        b"".join((_pack_str_(v) for v in (o.__module__, o.__qualname__)))
    )

def _pack_object_(o):
    try:
        _reduce = o.__reduce__()
    except AttributeError:
        raise error(TypeError, _cannot_pack, o) from None
    if isinstance(_reduce, str):
        return _pack_data_(Types.TYPE_SINGLETON, _pack_str_(_reduce))
    elif isinstance(_reduce, tuple):
        return _pack_data_(Types.TYPE_INSTANCE, _pack_tuple_(_reduce))
    raise TypeError("__reduce__() must return a str or a tuple")


# ------------------------------------------------------------------------------

_pack_types = {
    int: _pack_int_,
    float: _pack_float_,
    complex: _pack_complex_,
    type(None): lambda o: _pack_none_(),
    bool: lambda o: _pack_true_() if o else _pack_false_(),
    str: _pack_str_,
    bytes: _pack_bytes_,
    bytearray: _pack_bytearray_,
    tuple: _pack_tuple_,
    list: _pack_list_,
    dict: _pack_dict_,
    set: _pack_set_,
    frozenset: _pack_frozenset_,
    type: _pack_class_,
}

def pack(o):
    return _pack_types.get(type(o), _pack_object_)(o)
