from .cpp_util import *
from typing import Self

class Type:
    def cpp_type(self) -> str:
        raise NotImplementedError

    def as_c_header_type(self) -> Self:
        raise NotImplementedError

class VoidType(Type):
    def cpp_type(self) -> str:
        return 'void'

    def as_c_header_type(self) -> Self:
        return self

class PrimitiveType(Type):
    name: str

    def __init__(self, name: str):
        self.name = name

    def cpp_type(self):
        return self.name

    def as_c_header_type(self) -> Self:
        return self

class NamedType(Type):
    name: str
    kind: str | None

    def __init__(self, name: str, kind: str | None = None):
        self.name = name
        self.kind = kind

    def cpp_type(self):
        return self.name

    def as_c_header_type(self) -> Self:
        return NamedType(f"WGPU{self.name}")

class FlagsType(Type):
    name: str

    def __init__(self, name: str):
        self.name = name
    
    def cpp_type(self):
        return f"Flags<{self.name}>"

    def as_c_header_type(self) -> NamedType:
        return NamedType(f"WGPU{self.name}")

class PointerType(Type):
    inner: Type
    mutable: bool
    reference: bool

    def __init__(self, inner: Type, mutable: bool = False, reference: bool = False):
        self.inner = inner
        self.mutable = mutable
        self.reference = reference

    def cpp_type(self):
        symbol = '&' if self.reference else '*'
        if not self.mutable:
            return f"{self.inner.cpp_type()} const{symbol}"
        return f"{self.inner.cpp_type()}{symbol}"

    def as_c_header_type(self) -> Self:
        return PointerType(self.inner.as_c_header_type(), mutable=self.mutable, reference=False)

class ArrayType(Type):
    inner: Type

    def __init__(self, inner: Type):
        self.inner = inner

    def cpp_type(self):
        return f"Array<{self.inner.cpp_type()}>"

    def as_c_header_type(self) -> Self:
        raise AssertionError('ArrayType does not have a direct C-header equivalent')


class ParameterType:
    type_: Type
    name: str | None
    doc: str
    default_value: Value | None

    def __init__(self, type_: Type, default_value: Value | None = None):
        self.type_ = type_
        self.default_value = default_value
    
    def cpp_function_parameter(self):
        out = self.type_.cpp_type()

        if self.name is not None:
            out += ' ' + self.name

        return out
    
    def append_struct_field(self, b: SourceBuilder):
        b.append(indent(1, doc_comment(self.doc)))
        b.append(indent(1, f"{self.type_.cpp_type()} {self.name}"))

        if self.default_value is None or isinstance(self.default_value, ZeroValue):
            # OptionalBool undefined value is not 0, so we need to handle that separately.
            if isinstance(self.type_, NamedType) and self.type_.name == "OptionalBool":
                b.append(' = OptionalBool::Undefined;\n')
            else:
                b.append(' {};\n')
        else:
            b.append(f" = {self.default_value.cpp_value()};\n")

def type_from_spec(type_: str):
    # Primitive types
    primitives = {
        'bool': 'Bool',
        'nullable_string': 'StringView',
        'string_with_default_empty': 'StringView',
        'out_string': 'StringView',
        'uint16': 'uint16_t',
        'uint32': 'uint32_t',
        'uint64': 'uint64_t',
        'usize': 'size_t',
        'int16': 'int16_t',
        'int32': 'int32_t',
        'float32': 'float',
        'nullable_float32': 'float',
        'float64': 'double',
        'float64_supertype': 'double',
    }

    if type_ == 'c_void':
        return VoidType()
    if type_ in primitives:
        return PrimitiveType(primitives[type_])
    if type_.startswith('array<'):
        return ArrayType(type_from_spec(type_[6:-1]))

    # Complex types
    kind, name = type_.split('.', 2)
    if kind == 'bitflag':
        return FlagsType(pascal_case(name))
    elif kind == 'callback':
        name += '_callback_info'
    
    return NamedType(pascal_case(name), kind)

def default_value_from_spec(spec: str, type_: Type):
    if isinstance(spec, bool):
        return ConstantValue('true' if spec else 'false')
    elif isinstance(spec, int):
        return IntValue(spec)
    elif isinstance(spec, float):
        return FloatValue(spec)
    elif spec.startswith('0x'):
        return IntValue(int(spec[2:], base=16), hex=True)
    elif spec.startswith('constant.'):
        return ConstantValue("WGPU_" + spec[9:].upper())
    elif spec == 'zero':
        return ZeroValue()
    elif isinstance(type_, FlagsType):
        return ConstantValue(f"{type_.name}::{pascal_case(spec)}")
    
    assert isinstance(type_, NamedType) and type_.kind == 'enum'
    return ConstantValue(f"{type_.cpp_type()}::{pascal_case(spec)}")

def parameter_type_from_spec(spec: any):
    type_ = type_from_spec(spec['type'])

    if 'pointer' in spec and not isinstance(type_, ArrayType):
        mutable = spec['pointer'] == 'mutable'
        reference = not ('optional' in spec and spec['optional'])

        # We want to keep void pointers as pointers.
        if isinstance(type_, VoidType):
            reference = False

        type_ = PointerType(type_, mutable, reference)

    # Any type that's marked as passed with ownership should be an object.
    # FIXME: transform structs with 'free_members' set.
    # if 'ownership' in spec and spec['ownership'] == 'with':
    #     assert isinstance(type_, NamedType)
    #     assert type_.kind == 'object'
    
    parameter_type = ParameterType(type_)
    if 'name' in spec:
        parameter_type.name = camel_case(spec['name'])
    if 'default' in spec:
        value = default_value_from_spec(spec['default'], type_)
        parameter_type.default_value = value

    parameter_type.doc = spec['doc']
    return parameter_type
