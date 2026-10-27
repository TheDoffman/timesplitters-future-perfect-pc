"""Lexical C-source observations for generated/manual function coordination.

This is not a C preprocessor. Literal conditional branches are resolved; every
possible branch of an unknown macro condition remains visible. Callers that
need configuration-specific definitions must provide preprocessed source.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

from pathlib import Path
import re


def mask_noncode(source):
    """Blank comments and quoted literals, retaining offsets and line breaks."""
    result = list(source)
    cursor = 0
    def blank(start, end):
        for index in range(start, end):
            if source[index] not in '\r\n':
                result[index] = ' '
    while cursor < len(source):
        start = cursor
        if source.startswith('//', cursor):
            end = source.find('\n', cursor + 2)
            cursor = len(source) if end < 0 else end
            blank(start, cursor)
        elif source.startswith('/*', cursor):
            end = source.find('*/', cursor + 2)
            cursor = len(source) if end < 0 else end + 2
            blank(start, cursor)
        elif source[cursor] in ('"', "'"):
            quote = source[cursor]
            cursor += 1
            while cursor < len(source):
                character = source[cursor]
                cursor += 1
                if character == '\\':
                    cursor = min(len(source), cursor + 1)
                elif character == quote:
                    break
            blank(start, cursor)
        else:
            cursor += 1
    return ''.join(result)


def literal_condition(expression):
    expression = expression.strip()
    while expression.startswith('(') and expression.endswith(')'):
        expression = expression[1:-1].strip()
    if re.fullmatch(r'(?:0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*', expression):
        token = expression.rstrip('uUlL')
        return int(token, 16 if token.lower().startswith('0x') else 10) != 0
    return None


def strip_disabled(source):
    """Remove certainly inactive branches; retain unknown configuration paths."""
    source = source.replace('\\\r\n', '').replace('\\\n', '')
    cleaned = mask_noncode(source)
    stack, active, output = [], True, []
    for original, code in zip(source.splitlines(keepends=True), cleaned.splitlines(keepends=True)):
        directive = re.match(r'\s*#\s*(\w+)\b(.*)', code)
        if directive:
            name, expression = directive.groups()
            if name in ('if', 'ifdef', 'ifndef'):
                condition = literal_condition(expression) if name == 'if' else None
                stack.append([active, condition is not True])
                active = active and condition is not False
            elif name in ('elif', 'else') and stack:
                parent, remaining = stack[-1]
                condition = literal_condition(expression) if name == 'elif' else True
                active = parent and remaining and condition is not False
                stack[-1][1] = remaining and condition is not True
            elif name == 'endif' and stack:
                active = stack.pop()[0]
            elif active:
                output.append(original)
        elif active:
            output.append(original)
    if stack:
        raise ValueError('Unterminated source conditional')
    return ''.join(output)


def observations(source):
    live = mask_noncode(strip_disabled(source))
    # Retain macro-body symbol references but never treat macro text as a C
    # definition. The complete compiler remains authoritative for expansion.
    references = {int(value, 16) for value in re.findall(r'\bsub_([0-9a-fA-F]{8})\b', live)}
    declarations = '\n'.join(line for line in live.splitlines() if not line.lstrip().startswith('#'))
    tokens = re.findall(r'[A-Za-z_]\w*|[^\s]', declarations)
    defined, wrapped, depth = set(), set(), 0
    for index, token in enumerate(tokens):
        if depth == 0 and token == 'void' and index + 5 < len(tokens):
            name = tokens[index + 1]
            match = re.fullmatch(r'sub_([0-9a-fA-F]{8})(_gen)?', name)
            if match and tokens[index + 2:index + 5] == ['(', 'void', ')']:
                tail = tokens[index + 5]
                if tail == '{' and not match[2]:
                    defined.add(name)
                if tail == ';' and match[2]:
                    wrapped.add(int(match[1], 16))
        if token == '{':
            depth += 1
        elif token == '}':
            depth = max(0, depth - 1)
    return defined, wrapped, references


def definition_names(path):
    path = Path(path)
    return observations(path.read_text(encoding='utf-8'))[0] if path.is_file() else set()


def scan(path):
    path = Path(path)
    if not path.is_file():
        return set(), set(), set()
    defined, wrapped, references = observations(path.read_text(encoding='utf-8'))
    return {int(name[4:], 16) for name in defined}, wrapped, references
