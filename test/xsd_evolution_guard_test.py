# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Reject incomplete, source-mismatched or semantically changed XSD evolution evidence."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('xsd_evolution_qualification', ROOT / 'qualification/xsd_evolution.py')
QUALIFICATION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(QUALIFICATION)


def report():
    """Construct a declared complete inventory for testing the evidence admission boundary."""
    def row(variant):
        """Use declared literal evidence without coupling guard controls to an actual SDK."""
        literal = QUALIFICATION.literal_emission(variant)
        return {'status': 'passed', 'emission': {'native_sha256': literal['native_sha256'],
                                                'xml': {'projection': literal['xml_projection'], 'sha256': 'xml'}}}
    return {'status': 'passed', 'full_inventory': True, 'qualification_sources_sha256': {'schema': 'same'},
            'source_verification': {'changed': [], 'compiler_unchanged': True, 'changed_artifacts': []},
            'decisions': {'emissions': 22, 'native_exchanges': 484, 'xml_exchanges': 242, 'rejected_xml': 44},
            'languages': {language: {variant: row(variant) for variant in QUALIFICATION.VARIANTS}
                          for language in QUALIFICATION.LANGUAGES}}


class evolution_guards(unittest.TestCase):
    """Verify that nominal passed markers cannot substitute for the required proof."""

    def setUp(self):
        """Start each rejection test with independent complete reports."""
        self.current, self.previous = report(), report()

    def compare(self):
        """Apply the actual maintained cross-host evidence admission function."""
        return QUALIFICATION.compare_hosts(self.current, self.previous)

    def test_complete_reports(self):
        """Every required language and both separately generated variants are admitted."""
        self.assertEqual(len(self.compare()), 22)

    def test_missing_sdk(self):
        """A passed marker cannot hide a missing actual SDK."""
        del self.current['languages']['swift']
        with self.assertRaises(AssertionError):
            self.compare()

    def test_missing_variant(self):
        """One generated model cannot stand in for the second XSD contract."""
        del self.previous['languages']['python']['v2']
        with self.assertRaises(AssertionError):
            self.compare()

    def test_subset_diagnostic(self):
        """A diagnostic run never closes the full inventory gate."""
        self.current['full_inventory'] = False
        with self.assertRaises(AssertionError):
            self.compare()

    def test_missing_exchange(self):
        """Successful builds alone cannot replace the full actual consumer matrix."""
        self.current['decisions']['xml_exchanges'] -= 1
        with self.assertRaises(AssertionError):
            self.compare()

    def test_source_mismatch(self):
        """The same bytes cannot qualify different maintained source contracts."""
        self.current['qualification_sources_sha256']['schema'] = 'different'
        with self.assertRaises(AssertionError):
            self.compare()

    def test_mutated_artifact(self):
        """A provider or consumer mutation invalidates otherwise matching evidence."""
        self.current['source_verification']['changed_artifacts'] = ['runtime']
        with self.assertRaises(AssertionError):
            self.compare()

    def test_conflicting_artifact_observations(self):
        """Later captures cannot hide a provider change between separate variant builds."""
        captures = [{str(ROOT / 'provider'): 'first'}, {str(ROOT / 'provider'): 'second'}]
        artifacts, conflicts = QUALIFICATION.merge_artifacts(captures)
        self.assertEqual(len(conflicts), 1)
        self.assertEqual(set(artifacts.values()), {'first'})

    def test_native_bytes_changed(self):
        """XML binding changes cannot silently alter the native wire contract."""
        self.current['languages']['c']['v2']['emission']['native_sha256']['authored.bin'] = 'different'
        with self.assertRaises(AssertionError):
            self.compare()

    def test_xml_identity_changed(self):
        """Lexical variation cannot excuse a changed expanded name or value projection."""
        self.current['languages']['rust']['v1']['emission']['xml']['projection'] = ['different']
        with self.assertRaises(AssertionError):
            self.compare()

    def test_matching_empty_native_inventory(self):
        """Equality of two absent native inventories cannot qualify native wire stability."""
        for value in (self.current, self.previous):
            value['languages']['cpp']['v1']['emission']['native_sha256'] = {}
        with self.assertRaises(AssertionError):
            self.compare()

    def test_matching_empty_xml_projection(self):
        """Two omitted XML projections cannot replace checked expanded names and values."""
        for value in (self.current, self.previous):
            value['languages']['python']['v2']['emission']['xml']['projection'] = []
        with self.assertRaises(AssertionError):
            self.compare()

    def test_matching_wrong_native_identity(self):
        """The same changed integer-key bytes cannot silently alter native field IDs."""
        for value in (self.current, self.previous):
            value['languages']['c']['v1']['emission']['native_sha256']['authored-integer.bin'] = 'changed'
        with self.assertRaises(AssertionError):
            self.compare()

    def test_rebound_schema_snapshot_uses_content_and_relative_identity(self):
        """Relocated rebound inputs compare equally, while any changed XSD byte invalidates the proof."""
        with tempfile.TemporaryDirectory(prefix='serializer-xsd-rebound-guards-') as temporary:
            first, second = Path(temporary) / 'first', Path(temporary) / 'second'
            for directory in (first, second):
                (directory / 'v1').mkdir(parents=True)
                (directory / 'v1/order.serializer').write_text('authored native identities', encoding='utf-8')
                (directory / 'v1/order.xsd').write_text('fixed XML contract', encoding='utf-8')
            baseline = QUALIFICATION.source_snapshot(first)
            self.assertEqual(baseline, QUALIFICATION.source_snapshot(second))
            self.assertIn('schema_override/v1/order.xsd', baseline)
            (second / 'v1/order.xsd').write_text('changed XML contract', encoding='utf-8')
            self.assertNotEqual(baseline, QUALIFICATION.source_snapshot(second))


if __name__ == '__main__':
    unittest.main()
