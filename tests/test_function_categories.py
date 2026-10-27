"""Original source-identifier and graph propagation examples."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from function_categories import source_identifiers, direct_source_labels, propagate_categories


def test_source_ids_preserve_paths_and_derive_categories():
    records = [dict(address='0x100', string='@@(#)$Id: //RenderWare/RW36Active/rwsdk/src/plcore/sample.c#12 $'),
               dict(address='0x200', string='//RenderWare/another/rwsdk/world/pipe/p2/xbox/sample.c#2 $'),
               dict(address='0x300', string='//RenderWare/version/rwsdk/../escape.c#1 $')]
    identifiers = source_identifiers(records)
    assert [(r['path'], r['category']) for r in identifiers] == [
        ('src/plcore/sample.c', 'rw_plcore'), ('world/pipe/p2/xbox/sample.c', 'rw_world_pipe_xbox')]
    labels = direct_source_labels(identifiers, {0x100: [0x1000], 0x200: [0x2000]})
    assert labels[0x1000]['source_file'] != labels[0x2000]['source_file']
    assert labels[0x1000]['module'] == labels[0x2000]['module'] == 'sample.c'


def nodes(edges):
    return [dict(start=hex(start), calls_to=[hex(target) for target in targets]) for start, targets in edges.items()]


def test_graph_consensus_uses_snapshot_and_reduces_confidence():
    functions = nodes({1: [3, 4], 2: [3, 4], 3: [], 4: [], 5: [3, 4]})
    seeds = {1: dict(category='rw_core', confidence=1.0), 2: dict(category='rw_core', confidence=0.9)}
    # Node 5 is initially unknown, preventing the two callees from gaining a
    # unanimous caller label. Removing that unclassified caller allows progress.
    assert not propagate_categories(functions, seeds)
    functions = nodes({1: [3, 4], 2: [3, 4], 3: [], 4: []})
    result = propagate_categories(functions, seeds)
    assert set(result) == {3, 4}
    assert result[3]['confidence'] == 0.9 * 0.8
    assert result == propagate_categories(list(reversed(functions)), seeds)
    assert set(seeds) == {1, 2}


def test_cycles_conflicts_and_duplicate_edges_do_not_create_certainty():
    functions = nodes({1: [2, 2], 2: [1], 3: [1, 2]})
    assert not propagate_categories(functions, {})
    seeds = {1: dict(category='rw_core', confidence=1.0), 2: dict(category='game_audio', confidence=1.0)}
    assert not propagate_categories(functions, seeds)
    assert not propagate_categories(nodes({1: [], 2: [1, 1]}), {1: seeds[1]})


def test_round_budget_prevents_same_pass_cascades():
    functions = nodes({1: [], 2: [], 3: [1, 2], 4: [1, 2], 5: [3, 4]})
    seeds = {1: dict(category='rw_core', confidence=1.0), 2: dict(category='rw_core', confidence=1.0)}
    assert set(propagate_categories(functions, seeds, max_rounds=1)) == {3, 4}
    result = propagate_categories(functions, seeds, max_rounds=2)
    assert set(result) == {3, 4, 5}
    assert result[5]['confidence'] == 0.8 * 0.8
    assert not propagate_categories(functions, seeds, max_rounds=0)
