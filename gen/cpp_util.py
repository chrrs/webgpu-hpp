from .cpp_values import *

class SourceBuilder:
    string: str = ''

    def append(self, other: str):
        self.string += other
    
    def __str__(self):
        return self.string

def doc_comment(doc: str) -> str:
    doc = doc.removeprefix('TODO').strip()
    if doc == '':
        return ''
    
    lines = [f" * {line}\n" for line in doc.split('\n')]
    return '/**\n' + ''.join(lines) + ' **/\n'

def capitalize_first_letter(s: str):
    return s[0].upper() + s[1:] if len(s) > 0 else ''

def indent(level: int, code: str) -> str:
    lines = [(('    ' * level) + line) if len(line) > 0 else '' for line in code.split('\n')]
    return '\n'.join(lines)

def camel_case(ident: str) -> str:
    ident = ident.split("_")
    return ident[0] + ''.join(capitalize_first_letter(word) for word in ident[1:])

def pascal_case(ident: str) -> str:
    return ''.join([capitalize_first_letter(word) for word in ident.split('_')])