"""Original indirect-branch ledger fixtures and production consumer contracts."""

from project_paths import source_file as repo_file

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import branch_feedback as feedback


def test_dump_union_ignores_truncated_invalid_and_unobserved_records(tmp_path):
    path = tmp_path/'targets.dump'
    path.write_text('# header\n00002000 1\n00002000 2\n00003000 0\n100000000 1\n-1 2\n00004000 8\n0000')
    assert feedback.parse_dump(path) == {0x2000: 3}


def test_sites_union_saturation_persists_across_shorter_runs(tmp_path):
    path, db = tmp_path/'sites.dump', tmp_path/'sites.json'
    path.write_text('00001000 00002000 +\n00001000 00003000\n00004000 +\n00000000 00002000\n0000')
    assert feedback.merge_sites(db,[path])[1] == 2
    path.write_text('00001000 00002000\n')
    feedback.merge_sites(db,[path])
    assert feedback.load_sites(db) == {0x1000: ({0x2000,0x3000},True), 0x4000: (set(),True)}


def test_duplicate_ledger_entries_accumulate_flags(tmp_path):
    path = tmp_path/'targets.json'
    path.write_text(json.dumps([dict(start='0x2000',flags=1),dict(start='0x2000',flags=2),0x3000]))
    assert feedback.load_db(path) == {0x2000:3,0x3000:1}


@pytest.mark.parametrize('record', [dict(start='-1',flags=1),dict(start='0x2000',flags=0),
    dict(start='0x2000',flags=True),dict(start='0x100000000',flags=2),True])
def test_invalid_ledger_records_fail_without_rewriting(tmp_path, record):
    path = tmp_path/'targets.json'
    original = json.dumps([record]); path.write_text(original)
    with pytest.raises(ValueError):
        feedback.load_db(path)
    assert path.read_text() == original


def test_atomic_publication_preserves_previous_ledger_on_replace_error(tmp_path, monkeypatch):
    path = tmp_path/'targets.json'
    feedback.save_db(path,{0x2000:1}); previous = path.read_bytes()
    def fail(*args):
        raise OSError('simulated publication failure')
    monkeypatch.setattr(Path,'replace',fail)
    with pytest.raises(OSError):
        feedback.save_db(path,{0x2000:3,0x3000:2})
    assert path.read_bytes() == previous
    assert not list(tmp_path.glob('.generated-*'))


def test_overlap_lookup_has_no_fixed_window_and_keeps_known_entries():
    bodies = [(0x1000,0x5000)] + [(0x2000+16*n,0x2008+16*n) for n in range(20)]
    index = feedback.BodyIndex(bodies)
    assert index.interior(0x4000) == 0x1000
    assert index.interior(0x2000) is None
    assert index.interior(0x5000) is None


def test_explicit_end_ranges_and_zero_sizes_are_handled(tmp_path):
    path = tmp_path/'functions.json'
    path.write_text(json.dumps([dict(start='0x2000',end='0x2100'),dict(start='0x3000',size=0)]))
    assert feedback.load_function_bodies(path) == [(0x2000,0x2100)]


def test_seed_filter_is_separate_and_idempotent(tmp_path):
    ledger, functions, output = (tmp_path/name for name in ('ledger.json','functions.json','seeds.json'))
    feedback.save_db(ledger,{0x2000:1,0x2020:2,0x3003:2,0x4000:2})
    original = ledger.read_bytes()
    functions.write_text(json.dumps([dict(start='0x2000',end='0x2100')]))
    command=['--db',str(ledger),'--functions',str(functions),'seeds','--out',str(output)]
    assert feedback.main(command) == 0
    assert feedback.load_db(output) == {0x2000:1,0x4000:2}
    before = output.read_bytes(); assert feedback.main(command) == 0
    assert output.read_bytes()==before and ledger.read_bytes()==original
    assert feedback.main(command[:-1]+[str(ledger)])==1
    assert ledger.read_bytes()==original


def test_seed_export_does_not_implicitly_use_another_titles_function_bounds(tmp_path, monkeypatch):
    functions=tmp_path/'other-title.json'
    functions.write_text(json.dumps([dict(start='0x1000',end='0x5000')]))
    monkeypatch.setattr(feedback,'FUNCTIONS_PATH',str(functions))
    ledger,output=tmp_path/'targets.json',tmp_path/'seeds.json'
    feedback.save_db(ledger,{0x2000:2})
    assert feedback.main(['--db',str(ledger),'seeds','--out',str(output)])==0
    assert feedback.load_db(output)=={0x2000:2}


def test_zero_size_known_alias_inside_another_body_remains_seedable(tmp_path):
    functions,ledger,output=(tmp_path/name for name in ('functions.json','ledger.json','seeds.json'))
    functions.write_text(json.dumps([dict(start='0x2000',end='0x2100'),
                                    dict(start='0x2020',size=0)]))
    feedback.save_db(ledger,{0x2020:2,0x2030:2})
    assert feedback.main(['--db',str(ledger),'--functions',str(functions),
                          'seeds','--out',str(output)])==0
    assert feedback.load_db(output)=={0x2020:2}


def test_merge_validates_site_ledger_before_changing_targets(tmp_path):
    dump, ledger, sites = (tmp_path/name for name in ('a.dump','targets.json','sites.json'))
    dump.write_text('00002000 2\n')
    feedback.save_db(ledger,{0x2000:1}); before = ledger.read_bytes()
    sites.write_text(json.dumps({'0x1000':dict(targets=[],saturated='false')}))
    assert feedback.main(['--db',str(ledger),'merge',str(dump),'--sites-db',str(sites)])==1
    assert ledger.read_bytes()==before
    assert feedback.main(['--db',str(dump),'merge',str(dump)])==1
    assert dump.read_text()=='00002000 2\n'


def test_own_xbe_probe_accepts_unaligned_body_and_rejects_data(tmp_path):
    from test_xbe_image import fixture
    raw=fixture(); raw[0x1011:0x1014]=bytes.fromhex('31c0c3')
    path=tmp_path/'sample.xbe'; path.write_bytes(raw)
    probe=feedback._decode_probe(path)
    assert probe(0x11011)
    assert not probe(0x5000)


def test_copied_cli_merges_and_exports_without_toolkit(tmp_path):
    for name in ('branch_feedback.py','generation_output.py'):
        shutil.copyfile(repo_file(ROOT, 'tools', name),tmp_path/name)
    dump=tmp_path/'a.dump';dump.write_text('00002000 2\n00003003 1\n')
    db,sites,out=(tmp_path/name for name in ('targets.json','sites.json','seeds.json'))
    for args in (['merge',str(dump),'--sites-db',str(sites)],['seeds','--out',str(out)]):
        result=subprocess.run([sys.executable,str(tmp_path/'branch_feedback.py'),'--db',str(db),*args],
                              cwd=tmp_path,capture_output=True,text=True,timeout=30)
        assert result.returncode==0,result.stdout+result.stderr
    assert feedback.load_db(db)=={0x2000:2,0x3003:1}
    assert feedback.load_db(out)=={0x2000:2}


def test_sites_survive_missing_target_dump_and_repeat_merge_is_unchanged(tmp_path):
    dump, db, sites = (tmp_path/name for name in ('run.dump','targets.json','sites.json'))
    Path(feedback.sites_dump_beside(dump)).write_text('00001000 00002000 +\n')
    args=['--db',str(db),'merge',str(dump),'--sites-db',str(sites)]
    assert feedback.main(args)==0
    assert not db.exists()
    assert feedback.load_sites(sites)=={0x1000:({0x2000},True)}
    before=sites.stat().st_mtime_ns
    assert feedback.main(args)==0
    assert sites.stat().st_mtime_ns==before


def test_production_feedback_bridge_and_existing_contracts():
    runtime=ROOT/'external/ts3FPrecomp'
    if not (runtime/'tools/recomp/icall_feedback.py').is_file():
        pytest.skip('Pinned translator consumer unavailable')
    code='''from tools.recomp import icall_feedback
import branch_feedback
assert icall_feedback is branch_feedback
import pytest
raise SystemExit(pytest.main(['tools/recomp/test_icall_feedback.py',
    'tools/recomp/test_icall_sites_merge.py','-q','--tb=short']))
'''
    result=subprocess.run([sys.executable,'-c',code],cwd=runtime,
                          env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),
                          capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
