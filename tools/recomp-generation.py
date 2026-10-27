"""Track which generator sources produced the local, ignored game C files."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def fingerprint(runtime):
    paths = [runtime / 'scripts/recompile.py',
             runtime / 'templates/runtime/recomp_types.h',
             runtime / 'titles/futureperfect/src/recomp_manual.c']
    paths.extend(p for p in (runtime / 'tools/recomp').rglob('*')
                 if p.is_file() and p.suffix in ('.py', '.json', '.txt')
                 and '__pycache__' not in p.parts and not p.name.startswith('test_'))
    paths.extend((runtime / 'config/extra_symbols').glob('*.json'))
    paths.extend(p for p in (runtime / 'tools/xdk_symbols').glob('*.py')
                 if not p.name.startswith('test_'))
    paths.extend(p for p in (runtime / 'tools/disasm').glob('*.py')
                 if not p.name.startswith('test_'))
    paths.extend(p for p in (runtime / 'tools/func_id').glob('*.py')
                 if not p.name.startswith('test_'))
    result = {p.relative_to(runtime).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(paths)}
    project = Path(__file__).resolve().parents[1]
    for name in ('xbe_image.py', 'xbe_kernel_exports.def', 'recomp_image.py', 'x86_decode.py', 'x86_flow.py', 'x86_recovery.py', 'x86_functions.py', 'recomp_symbols.py', 'analysis_cache.py', 'analysis_config.py', 'analysis_output.py', 'analysis_runner.py', 'xdk_catalog.py', 'compile_pipeline.py', 'function_evidence.py', 'function_categories.py', 'function_catalog.py', 'c_source_scan.py', 'generation_output.py', 'translation_decode.py', 'translation_layout.py', 'translation_hooks.py', 'branch_feedback.py', 'unresolved_report.py', 'translation_operands.py', 'translation_moves.py', 'translation_integer.py', 'translation_bits.py', 'translation_product.py', 'translation_bitops.py', 'translation_exchange.py', 'translation_compare.py', 'translation_strings.py', 'translation_conditions.py', 'translation_sse.py', 'translation_mmx.py', 'translation_x87.py', 'translation_control.py', 'translation_lifting.py', 'translation_context.py', 'translation_helpers.py', 'translation_names.py', 'translation_host_symbols.txt', 'translation_host_symbols.json', 'translation_batch.py', 'translation_discovery.py', 'translation_function.py', 'translation_cli.py'):
        result['project/tools/' + name] = hashlib.sha256(
            (project / 'tools' / name).read_bytes()).hexdigest()
    parser_bridge = runtime / 'tools/xbe_parser/xbe_parser.py'
    if parser_bridge.is_file():
        result['tools/xbe_parser/xbe_parser.py'] = hashlib.sha256(parser_bridge.read_bytes()).hexdigest()
    # The lift scans these declarations to emit wrappers and original-body
    # pointers. Function-body edits only require compiling, but changing the
    # exported/imported names requires regenerating the guest C first.
    declarations = set()
    pattern = re.compile(r"^\s*(HLE_EXPORT|HLE_ORIGINAL|HLE_IMPORT_VAR)\(\s*([A-Za-z_]\w*)\s*\)", re.M)
    for source in (runtime / 'src/hle').rglob('*.c'):
        declarations.update(pattern.findall(source.read_text(encoding='utf-8')))
    result['hle-declarations'] = hashlib.sha256(
        json.dumps(sorted(declarations)).encode('utf-8')).hexdigest()
    return result


def marker(runtime):
    return runtime / 'titles/futureperfect/src/recomp/gen/tsfp-generator.json'


def current(runtime):
    try:
        return json.loads(marker(runtime).read_text(encoding='utf-8')) == fingerprint(runtime)
    except (OSError, ValueError):
        return False


def record(runtime):
    path = marker(runtime)
    if not (path.parent / 'recomp_funcs.h').is_file():
        raise RuntimeError('Generate the game sources before recording their generator.')
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(fingerprint(runtime), indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    if args.record:
        record(runtime)
    else:
        raise SystemExit(0 if current(runtime) else 1)
