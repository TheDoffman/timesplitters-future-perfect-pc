"""The build must regenerate guest wrappers when runtime hooks change."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('generation', ROOT / 'tools/recomp-generation.py')
generation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generation)


def test_runtime_declarations_invalidate_generated_wrappers(tmp_path):
    for name in ('scripts/recompile.py', 'templates/runtime/recomp_types.h',
                 'titles/futureperfect/src/recomp_manual.c',
                 'titles/futureperfect/src/recomp/gen/recomp_funcs.h',
                 'src/hle/stream.c'):
        p = tmp_path / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('/* original */')
    source = tmp_path / 'src/hle/stream.c'
    generation.record(tmp_path)
    for declaration in ('HLE_EXPORT(Release)', 'HLE_ORIGINAL(Release)', 'HLE_IMPORT_VAR(Clock)'):
        source.write_text(source.read_text() + '\n' + declaration + '\n')
        assert not generation.current(tmp_path)
        generation.record(tmp_path)
        assert generation.current(tmp_path)
    source.write_text(source.read_text() + '\n/* body-only edit */\n')
    assert generation.current(tmp_path)
    source.unlink()
    assert not generation.current(tmp_path)
    generation.record(tmp_path)
    symbols = tmp_path / 'config/extra_symbols/45410066.json'
    symbols.parent.mkdir(parents=True)
    symbols.write_text('{"symbols": []}')
    assert not generation.current(tmp_path)
    generation.record(tmp_path)
    symbols.write_text('{"symbols": [{"name": "XMVPlaybackStop"}]}')
    assert not generation.current(tmp_path)


def test_parser_sources_and_bridge_invalidate_generation(tmp_path, monkeypatch):
    project = tmp_path / 'project'
    runtime = tmp_path / 'runtime'
    for name in ('scripts/recompile.py', 'templates/runtime/recomp_types.h',
                 'titles/futureperfect/src/recomp_manual.c',
                 'titles/futureperfect/src/recomp/gen/recomp_funcs.h',
                 'tools/xbe_parser/xbe_parser.py', 'tools/disasm/engine.py', 'tools/xdk_symbols/__main__.py', 'tools/func_id/__main__.py'):
        path = runtime / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('original')
    for name in ('xbe_image.py', 'xbe_kernel_exports.def', 'recomp_image.py', 'x86_decode.py', 'x86_flow.py', 'x86_recovery.py', 'x86_functions.py', 'recomp_symbols.py', 'analysis_cache.py', 'analysis_config.py', 'analysis_output.py', 'analysis_runner.py', 'xdk_catalog.py', 'compile_pipeline.py', 'function_evidence.py', 'function_categories.py', 'function_catalog.py', 'c_source_scan.py', 'generation_output.py', 'translation_decode.py', 'translation_layout.py', 'translation_hooks.py', 'branch_feedback.py', 'unresolved_report.py', 'translation_operands.py', 'translation_moves.py', 'translation_integer.py', 'translation_bits.py', 'translation_product.py', 'translation_bitops.py', 'translation_exchange.py', 'translation_compare.py', 'translation_strings.py', 'translation_conditions.py', 'translation_sse.py', 'translation_mmx.py', 'translation_x87.py', 'translation_control.py', 'translation_lifting.py', 'translation_context.py', 'translation_helpers.py', 'translation_names.py', 'translation_host_symbols.txt', 'translation_host_symbols.json', 'translation_batch.py', 'translation_discovery.py', 'translation_function.py', 'translation_cli.py'):
        path = project / 'tools' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('original')
    monkeypatch.setattr(generation, '__file__', str(project / 'tools/recomp-generation.py'))
    for path in (project / 'tools/xbe_image.py', project / 'tools/xbe_kernel_exports.def',
                 project / 'tools/recomp_image.py', project / 'tools/x86_decode.py', project / 'tools/x86_flow.py', project / 'tools/x86_recovery.py', project / 'tools/x86_functions.py', project / 'tools/recomp_symbols.py',
                 project / 'tools/analysis_cache.py', project / 'tools/analysis_config.py', project / 'tools/analysis_output.py', project / 'tools/analysis_runner.py',
                 project / 'tools/xdk_catalog.py', project / 'tools/compile_pipeline.py',
                 project / 'tools/function_evidence.py', project / 'tools/function_categories.py', project / 'tools/function_catalog.py', project / 'tools/c_source_scan.py', project / 'tools/generation_output.py', project / 'tools/translation_decode.py', project / 'tools/translation_layout.py', project / 'tools/translation_hooks.py', project / 'tools/branch_feedback.py', project / 'tools/unresolved_report.py', project / 'tools/translation_operands.py', project / 'tools/translation_moves.py', project / 'tools/translation_integer.py', project / 'tools/translation_bits.py', project / 'tools/translation_product.py',
                 runtime / 'tools/xbe_parser/xbe_parser.py', runtime / 'tools/disasm/engine.py', runtime / 'tools/xdk_symbols/__main__.py', runtime / 'tools/func_id/__main__.py'):
        generation.record(runtime)
        assert generation.current(runtime)
        path.write_text('changed')
        assert not generation.current(runtime)
