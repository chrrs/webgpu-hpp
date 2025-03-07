from .cpp_structs import *
from .cpp_types import *


def generate_webgpu_hpp(spec: any):
    enum_prefix = spec['enum_prefix']
    enum_prefix = int(enum_prefix[2:], base=16) if enum_prefix.startswith('0x') else int(enum_prefix)

    # Parsing the spec
    enums = [enum_from_spec(e, enum_prefix) for e in spec['enums']]
    bitflags = [enum_from_spec(e, enum_prefix, bitflag=True) for e in spec['bitflags']]
    callbacks = [callback_from_spec(c) for c in spec['callbacks']]
    objects = [object_class_from_spec(o) for o in spec['objects']]
    structs = [struct_from_spec(s) for s in spec['structs']]
    functions = [function_from_spec(f) for f in spec['functions']]

    # Sort the structs so every member type is defined, while still
    # keeping them in mostly alphabetical order
    sorted_structs: list[Struct] = []
    for s in reversed(structs):
        dependencies = [m.type_.name
                        for m in s.members
                        if isinstance(m.type_, NamedType) and m.type_.kind == 'struct']
        i = len(sorted_structs) - 1
        while i >= 0:
            if sorted_structs[i].name in dependencies:
                break
            i -= 1

        sorted_structs.insert(i + 1, s)

    # Generating C++ code
    forward_declarations = SourceBuilder()
    for s in sorted_structs:
        s.append_forward_declaration(forward_declarations)
    forward_declarations.append('\n')
    for o in objects:
        o.append_forward_declaration(forward_declarations)

    enum_definitions = SourceBuilder()
    for i, e in enumerate(enums):
        if i != 0: enum_definitions.append('\n')
        e.append_definition(enum_definitions)

    bitflag_definitions = SourceBuilder()
    for i, e in enumerate(bitflags):
        if i != 0: bitflag_definitions.append('\n')
        e.append_bitflag_definitions(bitflag_definitions)

    callback_definitions = SourceBuilder()
    for i, c in enumerate(callbacks):
        if i != 0: callback_definitions.append('\n')
        c.append_definition(callback_definitions)

    object_definitions = SourceBuilder()
    for i, o in enumerate(objects):
        if i != 0: object_definitions.append('\n')
        o.append_definition(object_definitions)

    struct_definitions = SourceBuilder()
    for i, s in enumerate(sorted_structs):
        if i != 0: struct_definitions.append('\n')
        s.append_definition(struct_definitions)

    function_definitions = SourceBuilder()
    for i, f in enumerate(functions):
        if i != 0: function_definitions.append('\n')
        f.append_definition(function_definitions)

    method_definitions = SourceBuilder()
    for i, o in enumerate(objects):
        if i != 0: method_definitions.append('\n')

        method_definitions.append(f"// - {o.name}\n")
        o.append_builtin_method_definitions(method_definitions)

        for f in o.methods:
            method_definitions.append('\n')
            f.append_definition(method_definitions, object_name=o.name)

    # C++ template assembly
    disabled_lints = '*-explicit-constructor, *-explicit-constructor, *-noexcept-*'
    final_src = f"""#pragma once

{doc_comment(spec['copyright'])}
/**
 * !! THIS FILE IS AUTOMATICALLY GENERATED !!
 * 
 * Do not edit this file directly.
 * See `gen/__init__.py` for the code that generates this file.
 **/

#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// ReSharper disable CppSpecialFunctionWithoutNoexceptSpecification
// NOLINTBEGIN({disabled_lints})

namespace wgpu {{

typedef WGPUBool Bool;

// -- FORWARD DECLARATIONS --
{forward_declarations}

// -- ENUMS --
{enum_definitions}

// -- WRAPPER DECLARATIONS --
struct ChainedStruct {{
    ChainedStruct* next;
    SType sType;
}};

template <class T>
struct Array {{
    constexpr Array() = default;
    constexpr Array(size_t count, T* data) : count(count), data(data) {{ }}
    
    constexpr Array(std::span<T> span) : count(span.size()), data(span.data()) {{ }}
    constexpr Array(std::vector<T>& vector) : count(vector.size()), data(vector.data()) {{ }}
    
    template <size_t N>
    constexpr Array(std::array<T, N>& array) : count(N), data(array.data()) {{ }}

    constexpr T& operator[](size_t index) {{ return data[index]; }}
    constexpr T const& operator[](size_t index) const {{ return data[index]; }}
    
    size_t count {{}};
    T* data {{}};
}};

struct StringView {{
    constexpr StringView() = default;
    constexpr StringView(size_t length, const char* data) : data(data), length(length) {{ }}
    
    constexpr StringView(const char* str) : data(str), length(std::char_traits<char>::length(str)) {{ }}
    constexpr StringView(const std::string& str) : data(str.data()), length(str.size()) {{ }}
    constexpr StringView(std::string_view str) : data(str.data()), length(str.size()) {{ }}

    constexpr operator WGPUStringView() const {{ return {{ data, length }}; }}
    constexpr explicit operator std::string() const {{ return {{ data, length }}; }}
    constexpr operator std::string_view() const {{ return {{ data, length }}; }}
    
    const char* data {{}};
    size_t length {{}};
}};


// -- BITFLAG HELPERS --
template <class T>
struct FlagTraits {{
    constexpr static bool valid = false;
}};

template <class T>
struct Flags {{
    using BitType = std::underlying_type_t<T>;

    constexpr Flags() = default;
    constexpr Flags(T bit) : m_bits(static_cast<BitType>(bit)) {{ }}
    constexpr Flags(BitType bits) : m_bits(bits) {{ }}

    constexpr auto operator<=>(Flags const& rhs) const = default;

    constexpr Flags operator&(Flags const& rhs) const {{ return Flags(m_bits & rhs.m_bits); }}
    constexpr Flags operator|(Flags const& rhs) const {{ return Flags(m_bits | rhs.m_bits); }}
    constexpr Flags operator^(Flags const& rhs) const {{ return Flags(m_bits ^ rhs.m_bits); }}

    constexpr Flags operator~() const {{ return Flags(m_bits ^ FlagTraits<T>::allFlags.m_mask); }}
    constexpr bool operator!() const {{ return !m_bits; }}

    constexpr Flags& operator&=(Flags const& rhs) {{ m_bits = m_bits & rhs.m_bits; return *this; }}
    constexpr Flags& operator|=(Flags const& rhs) {{ m_bits = m_bits | rhs.m_bits; return *this; }}
    constexpr Flags& operator^=(Flags const& rhs) {{ m_bits = m_bits ^ rhs.m_bits; return *this; }}

    constexpr operator bool() const {{ return !!m_bits; }}
    constexpr operator BitType() const {{ return m_bits; }}

private:
    BitType m_bits {{}};
}};

template <class T>
constexpr bool operator<(T bit, Flags<T> const& flags) {{ return flags.operator>(bit); }}
template <class T>
constexpr bool operator>(T bit, Flags<T> const& flags) {{ return flags.operator<(bit); }}
template <class T>
constexpr bool operator<=(T bit, Flags<T> const& flags) {{ return flags.operator>=(bit); }}
template <class T>
constexpr bool operator>=(T bit, Flags<T> const& flags) {{ return flags.operator<=(bit); }}
template <class T>
constexpr bool operator==(T bit, Flags<T> const& flags) {{ return flags.operator==(bit); }}
template <class T>
constexpr bool operator!=(T bit, Flags<T> const& flags) {{ return flags.operator!=(bit); }}

template <class T>
constexpr bool operator&(T bit, Flags<T> const& flags) {{ return flags.operator&(bit); }}
template <class T>
constexpr bool operator|(T bit, Flags<T> const& flags) {{ return flags.operator|(bit); }}
template <class T>
constexpr bool operator^(T bit, Flags<T> const& flags) {{ return flags.operator^(bit); }}

template <class T, std::enable_if_t<FlagTraits<T>::valid, bool> = true>
constexpr Flags<T> operator&(T lhs, T rhs) {{ return Flags(lhs) & rhs; }}
template <class T, std::enable_if_t<FlagTraits<T>::valid, bool> = true>
constexpr Flags<T> operator|(T lhs, T rhs) {{ return Flags(lhs) | rhs; }}
template <class T, std::enable_if_t<FlagTraits<T>::valid, bool> = true>
constexpr Flags<T> operator^(T lhs, T rhs) {{ return Flags(lhs) ^ rhs; }}
template <class T, std::enable_if_t<FlagTraits<T>::valid, bool> = true>
constexpr Flags<T> operator~(T bit) {{ return ~Flags(bit); }}

// -- BITFLAGS --
{bitflag_definitions}

// -- CALLBACKS --
{callback_definitions}

// -- OBJECTS --
{object_definitions}

// -- STRUCTS --
{struct_definitions}

// -- FUNCTIONS --
{function_definitions}

// -- METHODS --
{method_definitions}
}}

// NOLINTEND({disabled_lints})
"""

    return final_src
