import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('hybrid', Path(__file__).resolve().parents[1] / 'tools/texture-hybrid-plan.py')
hybrid = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hybrid)


class HybridPlanTests(unittest.TestCase):
    def test_unknown_and_sensitive_assets_never_become_bulk_candidates(self):
        for category in ['character', 'weapon', 'prop', 'effect', 'interface', 'reflection', 'material_uncertain']:
            self.assertNotEqual(hybrid.lane({'status': 'needs_art'}, {'visual_category': category}), 'batch_candidate')
        self.assertEqual(hybrid.lane({'status': 'needs_art'}, None), 'visual_triage')

    def test_installed_and_held_take_precedence_over_category(self):
        record = {'visual_category': 'environment'}
        self.assertEqual(hybrid.lane({'status': 'installed'}, record), 'installed')
        self.assertEqual(hybrid.lane({'status': 'draft_requires_review'}, record), 'correction')
        self.assertEqual(hybrid.lane({'status': 'retained_original'}, record), 'retained_original')

    def test_new_material_label_joins_same_family_batches(self):
        worklist = dict(pack_bytes=0, textures=[
            dict(id='old', status='needs_art', original_size=[128, 128]),
            dict(id='new', status='needs_art', original_size=[256, 128])])
        classification = dict(records=[
            dict(key='old', visual_category='environment', description='wood boards'),
            dict(key='new', visual_category='environment_material', description='wood boards')])
        report = hybrid.build(worklist, classification)
        self.assertEqual(report['counts'], {'batch_candidate': 2})
        self.assertEqual(len(report['candidate_batches']), 1)
        self.assertEqual(set(report['candidate_batches'][0]['keys']), {'old', 'new'})
        self.assertFalse(report['complete'])

    def test_mixed_sensitive_labels_keep_specialist_review(self):
        for category in ['prop_or_interface', 'interface_or_mechanical_atlas',
                         'effect_or_reflection', 'signage_or_environment',
                         'artwork_or_identity', 'character_or_material_uncertain',
                         'environment_alpha', 'environment_cutout',
                         'environment_sky', 'foliage', 'decal_or_environment',
                         'prop_or_utility_atlas', 'interface_font']:
            with self.subTest(category=category):
                record = {'visual_category': category}
                self.assertEqual(hybrid.lane({'status': 'needs_art'}, record), 'specialist_review')
                self.assertEqual(hybrid.lane({'status': 'installed'}, record), 'installed')
                self.assertEqual(hybrid.lane({'status': 'draft_requires_review'}, record), 'correction')
                self.assertEqual(hybrid.lane({'status': 'unsupported'}, record), 'format_research')

    def test_unknown_and_atlas_labels_are_not_promoted_to_bulk(self):
        for category in ['environment_custom', 'prop_or_architectural_atlas',
                         'character_atlas', 'weapon_or_prop', 'new_unknown_label']:
            with self.subTest(category=category):
                self.assertEqual(hybrid.lane({'status': 'needs_art'}, {'visual_category': category}),
                                 'individual_review')

    def test_batches_are_bounded_and_deterministic(self):
        textures = [dict(id=str(i), status='needs_art', original_size=[128, 128], occurrences=i) for i in range(19)]
        records = [dict(key=str(i), visual_category='environment', description='steel plate') for i in range(19)]
        report = hybrid.build(dict(pack_bytes=100, textures=textures), dict(records=records))
        self.assertEqual([len(b['keys']) for b in report['candidate_batches']], [8, 8, 3])
        self.assertEqual(report['candidate_batches'][0]['keys'][0], '18')
        self.assertEqual(report['pack_headroom_bytes'], hybrid.PACK_BUDGET - 100)
        self.assertEqual(len(report['textures']), 19)

    def test_rejects_budget_overflow_and_duplicate_keys(self):
        with self.assertRaises(ValueError):
            hybrid.build(dict(pack_bytes=hybrid.PACK_BUDGET + 1, textures=[]), dict(records=[]))
        row = dict(id='a', status='needs_art', original_size=[128, 128])
        with self.assertRaises(ValueError):
            hybrid.build(dict(pack_bytes=0, textures=[row, row]), dict(records=[]))

    def test_invalid_batch_sizes(self):
        for size in [0, 17]:
            with self.assertRaises(ValueError):
                hybrid.build(dict(pack_bytes=0, textures=[]), dict(records=[]), size)


if __name__ == '__main__':
    unittest.main()
