# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify unchanged compiler artifact timestamps, protected preflight and actual Ninja restat."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parent.parent
LANGUAGES = {'cpp': 'Model.hpp', 'java': 'Model.java', 'js': 'Model.mjs',
             'typescript': 'Model.d.mts', 'go': 'Model.go', 'csharp': 'Model.cs',
             'rust': 'Model.rs', 'python': 'Model.py', 'swift': 'Model.swift',
             'kotlin': 'Model.kt', 'c': 'Model.h'}
OPTIONS = None


def command(directory, arguments, label, success=True):
    """Retain complete diagnostics and never treat failed generation as an unchanged successful artifact."""
    result = subprocess.run([str(OPTIONS.compiler), *map(str, arguments)], cwd=directory,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (directory / (label + '.log')).write_bytes(result.stdout)
    if success and result.returncode:
        raise AssertionError(result.stdout.decode('utf-8', errors='replace'))
    if not success and not result.returncode:
        raise AssertionError('Rejected generation unexpectedly succeeded')
    return result.stdout


def snapshot(paths):
    """Compare exact bytes and native timestamps, including files whose length remains unchanged."""
    return {str(path): (hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_mtime_ns) for path in paths}


def age(paths):
    """Make unconditional rewrites fail deterministically without timing-resolution sleeps."""
    earlier = time.time_ns() - 20_000_000_000
    for path in paths:
        os.utime(path, ns=(earlier, earlier))


class PublicationTests(unittest.TestCase):
    """Exercise the actual compiler across every backend and its existing include/depfile safeguards."""

    def setUp(self):
        """Give each decision a fresh owned scratch tree, retained with its final diagnostics."""
        self.directory = Path(tempfile.mkdtemp(prefix=self._testMethodName + '-', dir=OPTIONS.build))
        (self.directory / 'shared').mkdir()
        self.child = self.directory / 'shared/common.serializer'
        self.child.write_text('serializer version 1; namespace demo { class record { public uint32 count (1); } }')
        self.schema = self.directory / 'model.serializer'
        self.schema.write_text('serializer version 1; include shared/common; namespace demo { class envelope { public record value; } }')
        self.generated = self.directory / 'generated'
        self.generated.mkdir()
        self.depfile = self.generated / 'model.d'

    def arguments(self, destinations=None, depfile=None):
        """Keep output spelling stable while allowing isolated protected-alias negative controls."""
        selected = {language: self.generated / name for language, name in LANGUAGES.items()}
        selected.update(destinations or {})
        result = ['--input', self.schema, '--language', ','.join(LANGUAGES), '--cpp.format', 'false',
                  '--depfile', depfile or self.depfile]
        for language, path in selected.items():
            result += ['--' + language + '.output', path]
        return result

    def first(self):
        """Generate all artifacts, including implicit C++ child headers, then pin old output timestamps."""
        output = command(self.directory, self.arguments(), 'initial')
        paths = sorted(path for path in self.generated.rglob('*') if path.is_file())
        self.assertGreater(len(paths), len(LANGUAGES) + 1)
        age(paths)
        return paths, snapshot(paths), output

    def test_repeat_all_languages(self):
        """Identical eleven-language generation preserves artifacts/depfile bytes, mtimes and diagnostics."""
        paths, previous, output = self.first()
        again = command(self.directory, self.arguments(), 'identical')
        self.assertEqual(output, again)
        self.assertEqual(previous, snapshot(paths))

    def test_one_language_changes(self):
        """An isolated Go presentation change updates only its file while equal batch siblings stay untouched."""
        paths, previous, _ = self.first()
        command(self.directory, self.arguments() + ['--go.package', 'alternate_models'], 'one-language')
        current = snapshot(paths)
        changed = str(self.generated / LANGUAGES['go'])
        self.assertNotEqual(previous[changed][0], current[changed][0])
        self.assertNotEqual(previous[changed][1], current[changed][1])
        for path in previous.keys() - {changed}:
            self.assertEqual(previous[path], current[path], path)

    def test_transitive_change_and_missing_file(self):
        """Changed bytes still publish; deleting one output recreates it without touching equal siblings."""
        paths, previous, _ = self.first()
        self.child.write_text(self.child.read_text().replace('uint32', 'uint64'))
        command(self.directory, self.arguments(), 'transitive-change')
        current = snapshot(paths)
        changed = [path for path in previous if previous[path][0] != current[path][0]]
        self.assertTrue(changed)
        for path in previous:
            if path in changed:
                self.assertNotEqual(previous[path][1], current[path][1])
            else:
                self.assertEqual(previous[path], current[path])
        missing = self.generated / LANGUAGES['java']
        missing.unlink()
        command(self.directory, self.arguments(), 'missing-output')
        restored = snapshot(paths)
        self.assertEqual(current[str(missing)][0], restored[str(missing)][0])
        for path in current.keys() - {str(missing)}:
            self.assertEqual(current[path], restored[path], path)

    def test_preflight_and_formatter_preservation(self):
        """Source aliases, depfile/output aliases and failed formatting reject before any equal artifact shortcut."""
        paths, previous, _ = self.first()
        protected = paths + [self.schema, self.child]
        prior = snapshot(protected)
        formatting = self.arguments()
        formatting[formatting.index('--cpp.format') + 1] = 'true'
        formatting += ['--cpp.clang_format', self.directory / 'absent-formatter']
        for label, arguments in (
                ('source-alias', self.arguments({'cpp': self.schema})),
                ('child-alias', self.arguments({'cpp': self.child})),
                ('depfile-alias', self.arguments(depfile=self.generated / LANGUAGES['java'])),
                ('formatter-failure', formatting)):
            output = command(self.directory, arguments, label, success=False)
            if label == 'formatter-failure':
                self.assertIn(b'clang-format', output)
            self.assertEqual(prior, snapshot(protected), label)
        alias = self.directory / 'hard-link.hpp'
        os.link(self.child, alias)
        command(self.directory, self.arguments({'cpp': alias}), 'hard-link-source', success=False)
        self.assertEqual(prior, snapshot(protected))
        self.assertEqual(previous, snapshot(paths))

    def test_import_depfile(self):
        """Protected XSD reruns preserve source/depfile timestamps and still reject differing imports."""
        if not OPTIONS.with_xml:
            self.skipTest('Selected compiler profile has no XML provider')
        xsd = self.directory / 'model.xsd'
        xsd.write_text('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">'
                       '<xs:element name="record"><xs:complexType><xs:attribute name="count" '
                       'type="xs:unsignedInt" use="required"/></xs:complexType></xs:element></xs:schema>')
        output, depfile = self.directory / 'imported.serializer', self.directory / 'imported.d'
        arguments = ['--import-xsd', xsd, '--output', output, '--depfile', depfile]
        command(self.directory, arguments, 'import-initial')
        age([output, depfile])
        previous = snapshot([output, depfile])
        command(self.directory, arguments, 'import-identical')
        self.assertEqual(previous, snapshot([output, depfile]))
        xsd.write_text(xsd.read_text().replace('unsignedInt', 'unsignedLong'))
        command(self.directory, arguments, 'import-protected', success=False)
        self.assertEqual(previous, snapshot([output, depfile]))

    def test_ninja_restat(self):
        """A changed compiler dependency regenerates once and prunes unchanged downstream compilation."""
        directory = self.directory / 'ninja'
        directory.mkdir()
        schema = directory / 'model.serializer'
        schema.write_text('serializer version 1; class record { public uint32 count (1); }')
        marker = directory / 'compiler.stamp'
        marker.write_text('initial')
        action = ('"' + sys.executable + '" "' + str(Path(__file__).resolve()) + '" --action ')
        common = ' --compiler "' + str(OPTIONS.compiler) + '" --build "' + str(directory) + '"'
        (directory / 'build.ninja').write_text('rule generate\n  command = ' + action + 'generate' + common +
            '\n  restat = 1\nrule consume\n  command = ' + action + 'consume' + common +
            '\nbuild model.hpp: generate compiler.stamp model.serializer\n'
            'build consumer.bin: consume model.hpp\ndefault consumer.bin\n')
        for label, expected in (('initial', ['generate', 'consume']), ('compiler-change', ['generate']),
                                ('unchanged', []), ('schema-change', ['generate', 'consume'])):
            if label == 'compiler-change':
                time.sleep(0.05)
                marker.write_text('new compiler with unchanged C++ bytes')
            elif label == 'schema-change':
                time.sleep(0.05)
                schema.write_text(schema.read_text().replace('uint32', 'uint64'))
            calls = directory / 'calls.txt'
            before = calls.read_text().splitlines() if calls.exists() else []
            result = subprocess.run([str(OPTIONS.ninja), '-v'], cwd=directory,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (directory / (label + '.log')).write_bytes(result.stdout)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertEqual(calls.read_text().splitlines()[len(before):], expected, label)


def main():
    """Run meaningful before/after CLI controls without changing ordinary public stream writes."""
    global OPTIONS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--ninja', type=Path, default=shutil.which('ninja'))
    parser.add_argument('--with-xml', type=int, choices=(0, 1), default=1)
    parser.add_argument('--action', choices=('test', 'generate', 'consume'), default='test')
    OPTIONS = parser.parse_args()
    OPTIONS.compiler = OPTIONS.compiler.resolve()
    OPTIONS.build = OPTIONS.build.resolve()
    if OPTIONS.action == 'generate':
        command(OPTIONS.build, ['--input', OPTIONS.build / 'model.serializer', '--output', OPTIONS.build / 'model.hpp',
                               '--cpp.format', 'false', '--depfile', OPTIONS.build / 'model.d'], 'generate')
    elif OPTIONS.action == 'consume':
        (OPTIONS.build / 'consumer.bin').write_text(hashlib.sha256((OPTIONS.build / 'model.hpp').read_bytes()).hexdigest())
    else:
        if not OPTIONS.ninja:
            parser.error('An actual Ninja executable is required')
        if OPTIONS.build == ROOT or OPTIONS.build.is_relative_to(ROOT) and not OPTIONS.build.is_relative_to(ROOT / 'out'):
            parser.error('Repository scratch must stay under ignored out/')
        OPTIONS.build.mkdir(parents=True, exist_ok=True)
        beginning = hashlib.sha256(OPTIONS.compiler.read_bytes()).hexdigest()
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(PublicationTests))
        assert beginning == hashlib.sha256(OPTIONS.compiler.read_bytes()).hexdigest(), 'Qualification compiler changed'
        report = {'compiler_sha256': beginning, 'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  'tests': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
                  'skipped': len(result.skipped), 'languages': list(LANGUAGES)}
        (OPTIONS.build / 'verification.json').write_text(json.dumps(report, indent=2))
        if not result.wasSuccessful():
            raise SystemExit(1)
        return
    with (OPTIONS.build / 'calls.txt').open('a') as stream:
        stream.write(OPTIONS.action + '\n')


if __name__ == '__main__':
    main()
