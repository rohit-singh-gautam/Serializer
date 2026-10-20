# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Reject stale/partial qualification reports and mismatched provider byte claims."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('package_reproducibility_qualification', ROOT / 'qualification/package_reproducibility.py')
QUALIFICATION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(QUALIFICATION)


def report():
    """Build a complete declared process inventory without coupling guard checks to a ZIP implementation."""
    manifest = QUALIFICATION.oracle.load_manifest()
    observations = [dict(profile=profile, force_zip64=forced, permutation=permutation, repetition=repetition,
                         path='fixture', inspection={'archive_sha256': 'same', 'projection_sha256': 'semantic',
                             'metadata_methods': [manifest['backends'][language]['metadata_method']] if profile.startswith('opc-') else []})
                    for language in ('cpp',) for profile in manifest['profiles'] for forced in manifest['force_zip64']
                    for permutation in range(len(manifest['permutations'])) for repetition in range(manifest['process_repetitions'])]
    languages = {language: {'status': 'passed', 'provider_fingerprint_sha256': 'provider',
                           'provider_cross_host_identity': None, 'observations': copy.deepcopy(observations)}
                 for language in QUALIFICATION.LANGUAGES}
    return {'status': 'passed', 'full_inventory': True, 'manifest_sha256': 'manifest', 'languages': languages,
            'qualification_sources_sha256': {'schema': 'source'},
            'source_verification': {'changed': [], 'compiler_unchanged': True}}


class qualification_guards(unittest.TestCase):
    """Exercise the evidence admission boundary independently of actual SDK/provider availability."""

    def setUp(self):
        """Give each failure test independent complete baseline reports."""
        self.current, self.previous = report(), report()

    def compare(self):
        """Apply the actual maintained cross-host verifier to the current candidate evidence."""
        return QUALIFICATION.compare_hosts(self.current, self.previous)

    def test_identical_complete_evidence(self):
        """Complete matching reports qualify all declared languages."""
        self.assertEqual(set(self.compare()), set(QUALIFICATION.LANGUAGES))

    def test_changed_source_snapshot(self):
        """Identical archives cannot qualify different maintained source contracts."""
        self.previous['qualification_sources_sha256']['schema'] = 'different'
        with self.assertRaises(AssertionError):
            self.compare()

    def test_portable_source_path_identity(self):
        """Historical Windows separators identify the same maintained input on Linux."""
        self.current['qualification_sources_sha256'] = {'test/schema': 'source'}
        self.previous['qualification_sources_sha256'] = {'test\\schema': 'source'}
        self.assertEqual(set(self.compare()), set(QUALIFICATION.LANGUAGES))

    def test_duplicate_source_alias(self):
        """Two spellings of one source cannot hide contradictory snapshot entries."""
        self.current['qualification_sources_sha256'] = {'test/schema': 'source', 'test\\schema': 'source'}
        with self.assertRaises(AssertionError):
            self.compare()

    def test_source_changed_during_run(self):
        """A previously observed source mutation invalidates an otherwise matching report."""
        self.current['source_verification']['changed'] = ['schema']
        with self.assertRaises(AssertionError):
            self.compare()

    def test_compiler_changed_during_run(self):
        """Changing the generating compiler cannot retain a successful evidence status."""
        self.current['source_verification']['compiler_unchanged'] = False
        with self.assertRaises(AssertionError):
            self.compare()

    def test_missing_language(self):
        """A hand-edited passed marker cannot hide a missing SDK."""
        del self.previous['languages']['swift']
        with self.assertRaises(AssertionError):
            self.compare()

    def test_missing_process_decision(self):
        """A passed row still needs every required permutation and fresh process."""
        self.current['languages']['cpp']['observations'].pop()
        with self.assertRaises(AssertionError):
            self.compare()

    def test_duplicate_process_decision(self):
        """Duplicating one decision cannot substitute for another required process."""
        row = self.current['languages']['cpp']['observations']
        row[-1] = copy.deepcopy(row[0])
        with self.assertRaises(AssertionError):
            self.compare()

    def differing_compressed_pair(self):
        """Select a DEFLATE decision while keeping independent semantic metadata equal."""
        first = next(item for item in self.current['languages']['cpp']['observations'] if item['profile'] == 'zip-deflate')
        first['inspection']['archive_sha256'] = 'different-compressed-bytes'

    def test_same_provider_requires_compressed_equality(self):
        """An exact provider pin makes differing compressed output a failure."""
        self.differing_compressed_pair()
        with self.assertRaises(AssertionError):
            self.compare()

    def test_different_provider_records_ineligibility(self):
        """Equivalent semantics with different providers do not acquire a byte equality claim."""
        self.differing_compressed_pair()
        self.previous['languages']['cpp']['provider_fingerprint_sha256'] = 'other-provider'
        values = self.compare()['cpp']['comparisons']
        self.assertTrue(any(not row['byte_identical'] and not row['compressed_equality_eligible'] for row in values))

    def test_source_bound_codec_requires_equality(self):
        """Matching self-contained implementations still require matching compressed bytes."""
        self.differing_compressed_pair()
        self.previous['languages']['cpp']['provider_fingerprint_sha256'] = 'other-provider'
        self.previous['languages']['cpp']['provider_cross_host_identity'] = 'same-encoder'
        self.current['languages']['cpp']['provider_cross_host_identity'] = 'same-encoder'
        with self.assertRaises(AssertionError):
            self.compare()

    def test_stored_always_requires_byte_equality(self):
        """Provider differences cannot excuse changed uncompressed ZIP wire bytes."""
        self.current['languages']['cpp']['observations'][0]['inspection']['archive_sha256'] = 'different'
        self.previous['languages']['cpp']['provider_fingerprint_sha256'] = 'other-provider'
        with self.assertRaises(AssertionError):
            self.compare()

    def test_projection_difference_always_rejects(self):
        """Different compressors cannot hide changed logical metadata or payload bytes."""
        self.current['languages']['cpp']['observations'][0]['inspection']['projection_sha256'] = 'different'
        with self.assertRaises(AssertionError):
            self.compare()

    def test_external_inputs_are_watched(self):
        """Mutating each maintained imported input changes the observed source contract."""
        dependencies = ('test/resources/package_relocation.serializer', 'test/resources/opc_profile.ini',
                        'test/package_node_dependencies.py', 'test/resources/xml_shapes/typescript/package.json',
                        'test/resources/xml_shapes/typescript/package-lock.json', 'example/xml/rust/Cargo.toml', 'example/xml/rust/Cargo.lock')
        with tempfile.TemporaryDirectory(prefix='serializer-reproducibility-guards-') as temporary:
            root = Path(temporary)
            adapter = root / 'qualification/package_reproducibility'
            adapter.mkdir(parents=True)
            driver = adapter.with_suffix('.py')
            driver.write_text('driver', encoding='utf-8')
            (adapter / 'manifest.json').write_text('inventory', encoding='utf-8')
            for name in dependencies:
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('baseline', encoding='utf-8')
            with mock.patch.multiple(QUALIFICATION, ROOT=root, ADAPTERS=adapter, __file__=str(driver)):
                baseline = QUALIFICATION.source_snapshot()
                for name in dependencies:
                    target = root / name
                    target.write_text('changed', encoding='utf-8')
                    self.assertNotEqual(baseline[name], QUALIFICATION.source_snapshot()[name])
                    target.write_text('baseline', encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
