class Value:
    def cpp_value(self) -> str:
        raise NotImplementedError

class ZeroValue(Value):
    def cpp_value(self):
        return '{}'

class ConstantValue(Value):
    const: str

    def __init__(self, const: str):
        self.const = const

    def cpp_value(self):
        return self.const

class IntValue(Value):
    value: int
    hex: bool

    def __init__(self, value: int, hex: bool = False):
        self.value = value
        self.hex = hex

    def cpp_value(self) -> str:
        return f"0x{self.value:x}" if self.hex else str(self.value)

class FloatValue(Value):
    value: float

    def __init__(self, value: float):
        self.value = value

    def cpp_value(self) -> str:
        return f"{self.value}f"

class CombinationValue(Value):
    variants: list[str]

    def __init__(self, variants: list[str]):
        self.variants = variants
    
    def cpp_value(self):
        return ' | '.join(self.variants)


def value16_from_spec(spec: any):
    return IntValue(spec)

def value64_from_spec(spec: any):
    mappings = {
        'usize_max': 'SIZE_MAX',
        'uint32_max': 'UINT32_MAX',
        'uint64_max': 'UINT64_MAX',
        'nan': 'NAN',
    }

    if spec in mappings:
        return ConstantValue(mappings[spec])
    return IntValue(spec)