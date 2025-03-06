from .cpp_types import *
from .cpp_util import *


class Struct:
    name: str
    doc: str
    kind: str
    members: list[ParameterType]
    has_release: bool

    def append_forward_declaration(self, b: SourceBuilder):
        b.append(f"struct {self.name};\n")

    def append_definition(self, b: SourceBuilder):
        c_type = f"WGPU{self.name}"
        no_discard = '[[nodiscard]] ' if self.has_release else ''

        b.append(doc_comment(self.doc))
        b.append(f"struct {no_discard}{self.name} {{\n")

        b.append(indent(1, f"operator {c_type}() {{ return *reinterpret_cast<{c_type}*>(this); }}\n"))

        if self.has_release:
            c_release_func = f"wgpu{self.name}FreeMembers"
            b.append(indent(1, f"void release() {{ {c_release_func}(*this); }}\n"))

        b.append('\n')
        if self.kind in ['base_in', 'base_out', 'base_in_or_out']:
            b.append(indent(1, 'ChainedStruct* next = nullptr;\n'))
            b.append('\n')
        elif self.kind in ['extension_in', 'extension_out', 'extension_in_or_out']:
            b.append(indent(1, f"ChainedStruct chain {{ .sType = SType::{self.name} }};\n"))
            b.append('\n')

        [m.append_struct_field(b) for m in self.members]
        b.append('};\n\n')


class Function:
    name: str
    doc: str
    args: list[ParameterType]
    return_type: Type

    def append_forward_declaration(self, b: SourceBuilder, l: int = 0):
        f_args = ', '.join([p.cpp_function_parameter() for p in self.args])

        b.append(indent(l, doc_comment(self.doc)))
        b.append(indent(l, f"{self.return_type.cpp_type()} {self.name}({f_args});\n"))

    def append_definition(self, b: SourceBuilder, object_name: str | None = None):
        f_args = ', '.join([p.cpp_function_parameter() for p in self.args])
        func_name = f"{object_name}::{self.name}" if object_name is not None else self.name

        capitalized_name = capitalize_first_letter(self.name)
        c_func_name = f"wgpu{object_name}{capitalized_name}" if object_name is not None else f"wgpu{capitalized_name}"

        # FIXME: Pointers to structs can't be implicitly converted, so we have to be explicit about it.
        def should_reinterpret(type_: Type):
            return isinstance(type_, PointerType) and isinstance(type_.inner, NamedType)

        c_args: list[str] = []
        if object_name is not None:
            c_args.insert(0, 'm_ptr')
        for p in self.args:
            if should_reinterpret(p.type_):
                # noinspection PyUnresolvedReferences
                n = '&' if p.type_.reference else ''
                target_type = p.type_.as_c_header_type()
                c_args.append(f"reinterpret_cast<{target_type.cpp_type()}>({n}{p.name})")
            elif isinstance(p.type_, ArrayType):
                c_args.append(f"{p.name}.count")

                data_type = PointerType(p.type_.inner)
                if should_reinterpret(data_type):
                    target_type = data_type.as_c_header_type()
                    c_args.append(f"reinterpret_cast<{target_type.cpp_type()}>({p.name}.data)")
                else:
                    c_args.append(f"{p.name}.data")
            elif isinstance(p.type_, NamedType) and p.type_.kind == 'enum':
                target_type = p.type_.as_c_header_type()
                c_args.append(f"static_cast<{target_type.cpp_type()}>({p.name})")
            else:
                c_args.append(p.name)

        func_call = f"{c_func_name}({', '.join(c_args)})"

        b.append(doc_comment(self.doc))
        b.append(f"inline {self.return_type.cpp_type()} {func_name}({f_args}) {{\n")

        if isinstance(self.return_type, VoidType):
            b.append(indent(1, f"{func_call};\n"))
        elif isinstance(self.return_type, NamedType) and self.return_type.kind == 'enum':
            b.append(indent(1, f"return static_cast<{self.return_type.cpp_type()}>({func_call});\n"))
        elif isinstance(self.return_type, NamedType) and self.return_type.kind == 'struct':
            b.append(indent(1, f"auto result = {func_call};\n"))
            b.append(indent(1, f"return *reinterpret_cast<{self.return_type.cpp_type()}*>(&result);\n"))
        else:
            b.append(indent(1, f"return {func_call};\n"))

        b.append('}\n')


class ObjectClass:
    name: str
    doc: str
    methods: list[Function]

    def append_forward_declaration(self, b: SourceBuilder):
        b.append(f"class {self.name};\n")

    def append_definition(self, b: SourceBuilder):
        c_type = f"WGPU{self.name}"

        b.append(doc_comment(self.doc))
        b.append(f"class [[nodiscard]] {self.name} {{\n")
        b.append('public:\n')
        b.append(indent(1, f"constexpr {self.name}() = default;\n"))
        b.append(indent(1, f"constexpr {self.name}({c_type} ptr) : m_ptr(ptr) {{ }}\n"))
        b.append(indent(1, f"constexpr {self.name}({self.name} const&) = default;\n"))
        b.append(indent(1, f"constexpr {self.name}({self.name}&&) = default;\n"))
        b.append(indent(1, f"constexpr {self.name}& operator=({self.name} const&) = default;\n"))
        b.append(indent(1, f"constexpr {self.name}& operator=({self.name}&& rhs) = default;\n"))

        b.append('\n')
        b.append(indent(1, 'void addRef();\n'))
        b.append(indent(1, 'void release();\n'))

        b.append('\n')
        b.append(indent(1, 'constexpr operator bool() const { return !!m_ptr; }\n'))
        b.append(indent(1, f"constexpr operator {c_type}() const {{ return m_ptr; }}\n"))

        b.append('\n')
        for method in self.methods:
            method.append_forward_declaration(b, l=1)
            b.append('\n')

        b.append('private:\n')
        b.append(indent(1, f"{c_type}Impl* m_ptr {{}};\n"))
        b.append('};\n')

    def append_builtin_method_definitions(self, b: SourceBuilder):
        b.append(f"inline void {self.name}::addRef() {{\n")
        b.append(indent(1, f"wgpu{self.name}AddRef(m_ptr);\n"))
        b.append('}\n\n')

        b.append(f"inline void {self.name}::release() {{\n")
        b.append(indent(1, f"wgpu{self.name}Release(m_ptr);\n"))
        b.append('}\n')


class Enum:
    class Variant:
        name: str
        doc: str
        value: Value

        def append_definition(self, b: SourceBuilder):
            b.append(indent(1, doc_comment(self.doc)))
            b.append(indent(1, f"{self.name} = {self.value.cpp_value()},\n"))

    name: str
    doc: str
    variants: list[Variant]
    base_type: Type

    def append_definition(self, b: SourceBuilder):
        b.append(doc_comment(self.doc))
        b.append(f"enum class {self.name} : {self.base_type.cpp_type()} {{\n")
        [v.append_definition(b) for v in self.variants]
        b.append("};\n")

    def append_bitflag_definitions(self, b: SourceBuilder):
        all_flags = ' | '.join([f"{self.name}::{v.name}" for v in self.variants])

        self.append_definition(b)
        b.append('\n')
        b.append('template <>\n')
        b.append(f"struct FlagTraits<{self.name}> {{\n")
        b.append(indent(1, 'constexpr static bool valid = true;\n'))
        b.append(indent(1, f"constexpr static Flags<{self.name}> allFlags = {all_flags};\n"))
        b.append('};\n')


class Callback:
    name: str
    doc: str
    args: list[Type]
    has_mode: bool

    def append_definition(self, b: SourceBuilder):
        c_type = f"WGPU{self.name}"
        args = ', '.join([a.cpp_type() for a in self.args] + ['void*', 'void*'])

        b.append(doc_comment(self.doc))
        b.append(f"struct {self.name} {{\n")

        b.append(indent(1, f"operator {c_type}() {{ return *reinterpret_cast<{c_type}*>(this); }}\n"))

        b.append('\n')
        b.append(indent(1, 'ChainedStruct* next {};\n'))

        b.append('\n')
        if self.has_mode:
            b.append(indent(1, 'CallbackMode mode = CallbackMode::AllowSpontaneous;\n'))

        b.append(indent(1, f"void (*callback)({args}) {{}};\n"))
        b.append(indent(1, 'void* userdata1 {};\n'))
        b.append(indent(1, 'void* userdata2 {};\n'))

        b.append('};\n')


def struct_from_spec(spec: any):
    out = Struct()
    out.name = pascal_case(spec['name'])
    out.doc = spec['doc']
    out.kind = spec['type']

    out.members = []
    for member in spec['members']:
        out.members.append(parameter_type_from_spec(member))

    out.has_release = 'free_members' in spec and spec['free_members']

    return out


def function_from_spec(spec: any):
    out = Function()
    out.name = camel_case(spec['name'])
    out.doc = spec['doc']

    out.return_type = VoidType()
    if 'returns' in spec:
        out.return_type = parameter_type_from_spec(spec['returns']).type_

    out.args = []
    if 'args' in spec:
        for arg in spec['args']:
            out.args.append(parameter_type_from_spec(arg))
    if 'callback' in spec:
        name = pascal_case(spec['callback'].removeprefix('callback.') + '_callback_info')
        parameter_type = ParameterType(NamedType(name, 'callback'))
        parameter_type.name = 'callbackInfo'
        out.args.append(parameter_type)

        assert isinstance(out.return_type, VoidType)
        out.return_type = NamedType("Future", "struct")

    return out


def object_class_from_spec(spec: any):
    out = ObjectClass()
    out.name = pascal_case(spec['name'])
    out.doc = spec['doc']

    out.methods = []
    for method in spec['methods']:
        out.methods.append(function_from_spec(method))

    return out


def enum_from_spec(spec: any, enum_prefix: int, bitflag: bool = False):
    out = Enum()
    out.name = pascal_case(spec['name'])
    out.doc = spec['doc']

    out.base_type = PrimitiveType('WGPUFlags') if bitflag else PrimitiveType('uint32_t')

    out.variants = []
    for i, entry in enumerate(spec['entries']):
        if entry is None:
            continue

        variant = Enum.Variant()
        variant.name = pascal_case(entry['name'])
        variant.doc = entry['doc']

        if variant.name[0].isdigit():
            variant.name = '_' + variant.name

        # Reference webgpu.h for enum values
        # variant.value = ConstantValue(f"WGPU{pascal_case(spec['name'])}_{pascal_case(entry['name'])}")

        # Get the enum values from the spec, which is more readable
        if 'value_combination' in entry:
            variant.value = CombinationValue([pascal_case(e) for e in entry['value_combination']])
        elif 'value' in entry:
            variant.value = value16_from_spec(entry['value'])
        elif bitflag:
            if i == 0: variant.value = IntValue(enum_prefix)
            else: variant.value = IntValue(enum_prefix | (1 << (i - 1)))
        else:
            variant.value = IntValue(enum_prefix | i)

        out.variants.append(variant)

    return out


def callback_from_spec(spec: any):
    out = Callback()
    out.name = f"{pascal_case(spec['name'])}CallbackInfo"
    out.doc = spec['doc']

    out.args = []
    for arg in spec['args']:
        out.args.append(parameter_type_from_spec(arg).type_)

    out.has_mode = spec['style'] == 'callback_mode'

    return out
