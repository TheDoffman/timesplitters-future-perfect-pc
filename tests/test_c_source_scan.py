"""Original lexical examples for the next translation-support stage."""
from pathlib import Path
import sys
import os
import subprocess
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from c_source_scan import mask_noncode, strip_disabled, observations, scan


def test_comments_literals_and_prototypes_cannot_define_functions():
    source = '''
// void sub_00000001(void) {}
char *text = "void sub_00000002(void) {}";
/* void sub_00000003(void) {} */
void sub_00000004(void)
;
static void sub_00000005 ( void ) /* body */ { sub_00000004(); }
extern void sub_00000005_gen(void);
'''
    names, wrapped, referenced = observations(source)
    assert names == {'sub_00000005'}
    assert wrapped == {5} and referenced == {4, 5}


def test_false_branch_else_and_nested_unknown_macro():
    source = '''
#if (0)
#ifdef SOMETHING
void sub_00000001(void) {}
#endif
#elif 0U
void sub_00000002(void) {}
#else
#if UNKNOWN
void sub_00000003(void) {}
#else
void sub_00000004(void) {}
#endif
#endif
'''
    assert observations(source)[0] == {'sub_00000003', 'sub_00000004'}


def test_true_branch_suppresses_later_unknown_and_else():
    source = '''
#if 1
void sub_00000001(void) {}
#elif UNKNOWN
void sub_00000002(void) {}
#else
void sub_00000003(void) {}
#endif
'''
    assert observations(source)[0] == {'sub_00000001'}


def test_comment_directives_and_escaped_quotes_are_inert():
    source = '/*\n#if 0\n*/\nchar *s="escaped \\" quote";\nvoid sub_00000001(void) {}\n'
    assert observations(source)[0] == {'sub_00000001'}
    masked = mask_noncode(source)
    assert len(masked) == len(source)
    assert masked.count('\n') == source.count('\n')


def test_macros_supply_references_but_no_definitions():
    source = '#define SAMPLE void sub_00000001(void) {}\n#define REF sub_00000002\n'
    assert observations(source) == (set(), set(), {1, 2})


def test_unclosed_condition_fails_and_missing_file_is_empty(tmp_path):
    with pytest.raises(ValueError, match='Unterminated'):
        strip_disabled('#if 0\nvoid sub_00000001(void) {}\n')
    assert scan(tmp_path / 'absent.c') == (set(), set(), set())


def test_spliced_line_comments_and_macro_bodies_stay_out_of_definitions():
    source = ('// continued comment \\\nvoid sub_00000001(void) {}\n'
              '#define SAMPLE \\\nvoid sub_00000002(void) {}\n'
              'void sub_00000003(void) {}\n')
    assert observations(source) == ({'sub_00000003'}, set(), {2, 3})


def test_unknown_condition_retains_each_possible_branch_until_known_true():
    source = ('#if UNDECIDED\nvoid sub_00000001(void) {}\n'
              '#elif 1\nvoid sub_00000002(void) {}\n'
              '#else\nvoid sub_00000003(void) {}\n#endif\n')
    assert observations(source)[0] == {'sub_00000001', 'sub_00000002'}


def test_production_importlib_entry_uses_project_scanner(tmp_path):
    root = Path(__file__).resolve().parents[1]
    runtime = root / 'external/ts3FPrecomp'
    if not (runtime / 'tools/recomp/manual_scan.py').is_file():
        pytest.skip('Pinned runtime unavailable')
    code = '''import importlib.util,sys
from pathlib import Path
sys.path.insert(0,str(Path(sys.argv[1]).parents[2]))
from tools.recomp.translator import write_if_changed
assert write_if_changed.__module__ == 'generation_output'
spec=importlib.util.spec_from_file_location('standalone_manual',sys.argv[1])
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
assert module.scan.__module__ == 'c_source_scan'
assert module.definition_names.__module__ == 'c_source_scan'
'''
    result = subprocess.run([sys.executable, '-c', code, str(runtime / 'tools/recomp/manual_scan.py')],
                            cwd=tmp_path, env=dict(os.environ, TSFP_PROJECT_ROOT=str(root)),
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
