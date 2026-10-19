# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify the complete XML fixture inventory and hard SDK-failure boundary."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location('xml_fixture_manifest', ROOT / 'example/xml/fixture_manifest.py')
fixtures = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fixtures)
MANIFEST = ROOT / 'test/resources/xml_exchange_manifest.json'


class fixture_manifest_test(unittest.TestCase):
    """Keep every producer/consumer and its independent oracle mandatory."""

    def setUp(self):
        """Own isolated mutation fixtures outside tracked source directories."""
        self.directory = tempfile.TemporaryDirectory(prefix='serializer-xml-manifest-')
        self.addCleanup(self.directory.cleanup)
        self.source = Path(self.directory.name) / 'manifest.json'
        self.value = json.loads(MANIFEST.read_text(encoding='utf-8'))

    def load(self, value):
        """Read a mutated inventory through the production loader."""
        self.source.write_text(json.dumps(value), encoding='utf-8')
        return fixtures.load_manifest(self.source, ROOT)

    def test_all_eleven_languages_and_shared_input(self):
        """Require complete prefixes, existing consumer files and the common semantic oracle."""
        manifest = self.load(self.value)
        self.assertEqual({entry['id'] for entry in manifest['languages']}, set(fixtures.LANGUAGE_PREFIXES))
        self.assertEqual(len(manifest['languages']) ** 2, 121)

    def test_missing_duplicate_unknown_and_wrong_prefix_reject(self):
        """Prevent inventory mistakes from silently reducing qualification coverage."""
        for mutation in ('missing', 'duplicate', 'unknown', 'prefix'):
            value = copy.deepcopy(self.value)
            if mutation == 'missing':
                value['languages'].pop()
            elif mutation == 'duplicate':
                value['languages'].append(value['languages'][0])
            elif mutation == 'unknown':
                value['languages'][0]['id'] = 'not_a_backend'
            else:
                value['languages'][0]['prefix'] = 'PY'
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.load(value)

    def test_unsafe_and_missing_paths_reject(self):
        """Keep fixture paths within source inventory rather than generated or outside files."""
        for spelling in ('../outside.xml', '/outside.xml', 'C:/outside.xml', './example/xml/order.xml',
                         'example\\xml\\order.xml', 'example/xml/no_such_fixture.xml'):
            value = copy.deepcopy(self.value)
            value['input'] = spelling
            with self.subTest(path=spelling), self.assertRaises(ValueError):
                self.load(value)

    def test_versions_duplicate_keys_and_partial_exchange_reject(self):
        """Reject unrecognized format and weakened exchange contracts before SDK work."""
        for key, replacement in (('version', 2), ('version', True), ('exchange', 'one_pair'),
                                 ('expected', {}), ('unknown', True)):
            value = copy.deepcopy(self.value)
            value[key] = replacement
            with self.subTest(key=key, replacement=replacement), self.assertRaises(ValueError):
                self.load(value)
        self.source.write_text('{"version":1,"version":1}', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            fixtures.load_manifest(self.source, ROOT)

    def test_declared_source_must_be_the_actual_sdk_consumer(self):
        """Reject an existing language file which the SDK would never compile or execute."""
        value = copy.deepcopy(self.value)
        java = next(entry for entry in value['languages'] if entry['id'] == 'java')
        java['source'] = 'example/xml/java/README.md'
        with self.assertRaisesRegex(ValueError, 'maintained SDK entry point'):
            self.load(value)

    def test_oracle_rejects_reordered_and_extra_xml_content(self):
        """Require expanded names, values and ordered simple content in every exchange."""
        raw = fixtures.source_path(ROOT, self.value['input']).read_text(encoding='utf-8')
        seed = Path(self.directory.name) / 'document.xml'
        for changed in (raw.replace('R&amp;D', 'incorrect'), raw.replace('<tag>A</tag>', '<tag extra="1">A</tag>'),
                        raw.replace('<count>3</count>', '<count><nested/></count>')):
            self.assertNotEqual(changed, raw)
            seed.write_text(changed, encoding='utf-8')
            with self.assertRaises(AssertionError):
                fixtures.check_document(seed, self.value['expected'])

    def test_missing_or_failing_selected_sdk_is_a_hard_failure(self):
        """Actually launch a nonexistent tool and a failing tool; neither becomes a skip."""
        class missing_runner:
            """Resolve to a deliberately absent executable for a selected backend."""
            def command(self, language, executable, *arguments):
                """Produce a missing path independent of the host's installed SDK inventory."""
                return [str(Path(self_directory) / 'serializer_deliberately_missing_sdk')]
        self_directory = self.directory.name
        with self.assertRaises(FileNotFoundError):
            fixtures.require_sdks(self.value, ['java'], missing_runner(),
                                  lambda command: subprocess.run(command, check=True, capture_output=True))
        class failing_runner:
            """Use the current Python host only to produce an actual nonzero process status."""
            def command(self, language, executable, *arguments):
                """Return a process that exists and explicitly fails SDK qualification."""
                import sys
                return [sys.executable, '-c', 'raise SystemExit(7)']
        with self.assertRaises(subprocess.CalledProcessError):
            fixtures.require_sdks(self.value, ['java'], failing_runner(),
                                  lambda command: subprocess.run(command, check=True, capture_output=True))


if __name__ == '__main__':
    unittest.main()
