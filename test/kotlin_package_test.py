# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated Kotlin/JVM ZIP/OPC against independent wire, XML and fault oracles."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
import xml.etree.ElementTree as etree
import zipfile

import java_package_test as corpus
import python_package_test as independent
from csharp_package_test import inspect_directory

ROOT = Path(__file__).resolve().parents[1]
RELATIONSHIP_MIME = 'application/vnd.openxmlformats-package.relationships+xml'


def digest(path):
    """Retain hashes for exact emitted sources, fixtures and actual executed tools."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """Generate through the real compiler, compile strict JVM17 Kotlin, and run independent contracts."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--kotlinc', type=Path, default=os.environ.get('SERIALIZER_KOTLINC'))
    parser.add_argument('--java', default=os.environ.get('SERIALIZER_JAVA', 'java'))
    parser.add_argument('--cpp-example', type=Path, help='Actual native OPC producer/consumer executable')
    parser.add_argument('--filter', default='')
    args = parser.parse_args()
    build, compiler = args.build.resolve(), args.compiler.resolve(strict=True)
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    (build / 'verification.json').unlink(missing_ok=True)
    kotlinc = args.kotlinc or shutil.which('kotlinc') or shutil.which('kotlinc.bat')
    if not kotlinc:
        parser.error('Provide an actual Kotlin compiler with --kotlinc or SERIALIZER_KOTLINC')
    home = Path(kotlinc).resolve().parent.parent
    compiler_jar = home / 'lib/kotlin-compiler.jar'
    if not compiler_jar.is_file():
        parser.error('Expected the standard Kotlin compiler distribution lib/kotlin-compiler.jar')
    started, commands = time.monotonic(), []

    def run(command, name, expected_failure=False):
        """Retain exact arguments and diagnostics; failed SDK subprocesses never count as qualification."""
        argv = [str(item) for item in command]
        result = subprocess.run(argv, capture_output=True, text=True, encoding='utf-8', timeout=1200)
        (build / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'argv': argv, 'returncode': result.returncode, 'expected_failure': expected_failure})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if expected_failure:
            assert result.returncode != 0, 'Invalid reserved source unexpectedly succeeded'
        elif result.returncode:
            print(result.stdout + result.stderr, end='')
            result.check_returncode()
        return result.stdout + result.stderr

    rows = corpus.build_fixtures(build / 'fixtures')
    for label, target in [('escaping-root', '../../Alternate/root.xml'), ('escaping-source', '../../../Textures/raw.bin')]:
        parts = independent.independent_parts()
        key = '_rels/.rels' if label == 'escaping-root' else 'Alternate/_rels/root.xml.rels'
        old = b'Alternate/root.xml' if label == 'escaping-root' else b'../Textures/raw.bin'
        parts[key] = parts[key].replace(old, target.encode())
        path = label + '.zip'
        (build / 'fixtures' / path).write_bytes(independent.independent_zip(parts))
        row = (label, 'package', path, 'OK', '', 'independent')
        rows.append(row)
        with (build / 'fixtures/cases.tsv').open('a', encoding='utf-8') as output:
            output.write('\t'.join(row) + '\n')
    profile = build / 'profile-with-qnames.serializer'
    profile.write_text((ROOT / 'test/resources/opc_profile.serializer').read_text(encoding='utf-8') + '''
namespace scoped {
  xml(namespace("urn:scoped"));
  class model { public string first xml(element,qname); public string second xml(element,qname); }
  package archive { part main:model {
    name("/model.xml"); content_type("application/vnd.scoped+xml"); relationship("urn:scoped:main");
  }}
}
''', encoding='utf-8')
    scoped_parts = {
        'model.xml': ('<model xmlns="urn:scoped" xmlns:unused="urn:' + 'x' * 512 + '"><first xmlns:p="urn:first">p:item</first><second xmlns:p="urn:second">p:item</second></model>').encode(),
        '_rels/.rels': ('<Relationships xmlns="' + independent.RELS + '"><Relationship Id="scope" Type="urn:scoped:main" Target="model.xml"/></Relationships>').encode(),
        '[Content_Types].xml': ('<Types xmlns="' + independent.CT + '"><Override PartName="/model.xml" ContentType="application/vnd.scoped+xml"/><Default Extension="rels" ContentType="' + RELATIONSHIP_MIME + '"/></Types>').encode()}
    (build / 'fixtures/scoped.opc').write_bytes(independent.independent_zip(scoped_parts))
    run([compiler, '--input', profile, '--language', 'kotlin',
         '--output', build / 'Schema.kt'], 'generation')
    parameterized = build / 'parameterized.serializer'
    parameterized.write_text((ROOT / 'test/resources/opc_profile.serializer').read_text(encoding='utf-8').replace(
        independent.MODEL_MIME, independent.MODEL_MIME + ';profile=\\"a  b\\";mode=read'), encoding='utf-8')
    run([compiler, '--input', parameterized, '--language', 'kotlin', '--kotlin.package', 'parameterized',
         '--output', build / 'Parameterized.kt'], 'generation-parameterized')
    run([compiler, '--input', ROOT / 'test/resources/opc_identity.serializer', '--config', ROOT / 'test/resources/opc_identity.ini',
         '--language', 'kotlin', '--kotlin.package', 'identity', '--output', build / 'Identity.kt'], 'generation-identity')
    # Independent identity tuples deliberately include equal repeated keyrefs before deduplication.
    ct = ['<Override PartName="/_rels/.rels" ContentType="' + RELATIONSHIP_MIME + '"/>']
    links, parts = [], {}
    for index in range(3):
        name = f'/r{index}.xml'
        ct.append(f'<Override PartName="{name}" ContentType="application/vnd.serializer.identity+xml"/>')
        links.append(f'<Relationship Id="r{index}" Type="urn:serializer:identity-registry" Target="{name}"/>')
        parts[name[1:]] = b'<registry xmlns="urn:serializer:identity-test"><ref id="7"/><ref id="7"/><entry id="7" label="Bolt"/></registry>'
    parts['[Content_Types].xml'] = ('<Types xmlns="' + independent.CT + '">' + ''.join(ct) + '</Types>').encode()
    parts['_rels/.rels'] = ('<Relationships xmlns="' + independent.RELS + '">' + ''.join(links) + '</Relationships>').encode()
    (build / 'identity.opc').write_bytes(independent.independent_zip(parts))
    parts['r2.xml'] = parts['r2.xml'].replace(b'<ref id="7"/>', b'<ref id="8"/>', 1)
    (build / 'identity-invalid.opc').write_bytes(independent.independent_zip(parts))
    shutil.copyfile(ROOT / 'test/resources/kotlin_package_main.kt', build / 'Main.kt')
    collision = build / 'collision.serializer'
    collision.write_text('serializer version 1.20.0; class SerializerPackage {} class SerializerPackage_2 {} '
                         'class Throwable {} class System {} class Nothing {} class CharArray {} '
                         'class Operation {} '
                         'class Inflater {} class Deflater {} class InputStream {} class OutputStream {} '
                         'package bundle {part attachments:bytes {name(dynamic);content_type(dynamic);'
                         'relationship("urn:serializer:test-part");occurs(0,unbounded);}}', encoding='utf-8')
    run([compiler, '--input', collision, '--language', 'kotlin', '--kotlin.package', 'collision',
         '--output', build / 'Collision.kt'], 'generation-collision')
    reserved = build / 'reserved-hook.serializer'
    reserved.write_text('serializer version 1.20.0; class SrlXmlOperation {}', encoding='utf-8')
    reserved_output = build / 'ReservedHook.kt'
    reserved_output.unlink(missing_ok=True)
    diagnostic = run([compiler, '--input', reserved, '--language', 'kotlin', '--output', reserved_output],
                     'generation-reserved-hook', expected_failure=True)
    assert 'reserved native identifier' in diagnostic and not reserved_output.exists()
    source = (build / 'Collision.kt').read_text(encoding='utf-8')
    helper = re.search(r'Generated Kotlin package helper name: (\w+)', source).group(1)
    assert helper != 'SerializerPackage'
    collision_probe = ('import collision.encodeXml\nimport collision.decodeXml\n'
        '/** Preserve native/XML model names while runtime helpers avoid their vocabulary. */\n'
        'fun collisionProbe() { val writer=collision.' + helper + '.ZipWriter(); '
        'writer.add("item.bin",byteArrayOf(1)); val bytes=writer.finish(); collision.' + helper +
        '.ZipReader.open(bytes).use { check(it.read("item.bin").contentEquals(byteArrayOf(1))) }; ')
    for index, name in enumerate(('Throwable', 'System', 'Nothing', 'CharArray', 'Inflater', 'Deflater',
                                  'InputStream', 'OutputStream', 'Operation')):
        collision_probe += ('val wire' + str(index) + '=collision.' + name + '().encode(collision.Protocol.BINARY_NONE); '
            'check(collision.' + name + '.decode(wire' + str(index) + ',collision.Protocol.BINARY_NONE)'
            '.encode(collision.Protocol.BINARY_NONE).contentEquals(wire' + str(index) + ')); ')
    collision_probe += ('val xml=collision.Operation().encodeXml(); '
                        'check(collision.Operation.decodeXml(xml).encodeXml().contentEquals(xml)) }\n')
    (build / 'CollisionProbe.kt').write_text(collision_probe, encoding='utf-8')
    files = [build / name for name in ('Schema.kt', 'Parameterized.kt', 'Identity.kt', 'Collision.kt', 'Main.kt', 'CollisionProbe.kt')]
    if args.cpp_example:
        run([compiler, '--input', ROOT / 'example/opc/model.serializer', '--language', 'kotlin',
             '--kotlin.package', 'examplemodel', '--output', build / 'Example.kt'], 'generation-example')
        shutil.copyfile(ROOT / 'test/resources/kotlin_package_exchange.kt', build / 'Exchange.kt')
        files.extend((build / 'Example.kt', build / 'Exchange.kt'))
    tools = {'java': run([args.java, '-version'], 'java-version'),
             'kotlin': run([args.java, '-cp', compiler_jar, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-version'], 'kotlin-version')}
    run([args.java, '-Xmx2g', '-cp', compiler_jar, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-kotlin-home', home,
         '-jvm-target', '17', '-Werror', '-include-runtime', '-d', build / 'checks.jar', *files], 'compilation')
    output = run([args.java, '-cp', build / 'checks.jar', 'MainKt', build, args.filter], 'execution')
    counts = json.loads(output.splitlines()[-1])
    assert counts['failed'] == 0
    inspected = corpus.inspect_outputs(build) if not args.filter else 0
    directory_inspections = 0
    directory_negative_probes = 0
    if not args.filter:
        for path in sorted(build.glob('produced-*.*')):
            _, wide, descriptor = path.stem.split('-')
            with zipfile.ZipFile(path) as archive:
                assert archive.namelist() == sorted(archive.namelist())
                assert all(item.date_time == (1980, 1, 1, 0, 0, 0) for item in archive.infolist())
                data = path.read_bytes()
                inspect_directory(data, archive.infolist(), wide == 'true', descriptor == 'true',
                                  allow_known_descriptor_sizes=True)
                if wide == descriptor == 'true':
                    first = archive.infolist()[0]
                    name_size = struct.unpack_from('<H', data, first.header_offset + 26)[0]
                    extra = first.header_offset + 30 + name_size
                    for offset, expected in ((extra + 4, first.file_size), (extra + 12, first.compress_size)):
                        invalid = bytearray(data)
                        struct.pack_into('<Q', invalid, offset, expected + 1)
                        try:
                            inspect_directory(invalid, archive.infolist(), True, True,
                                              allow_known_descriptor_sizes=True)
                        except AssertionError:
                            directory_negative_probes += 1
                        else:
                            raise AssertionError('Independent inspector accepted an unrelated nonzero ZIP64 size')
            directory_inspections += 1
        assert directory_inspections == 8
        assert directory_negative_probes == 4
        with zipfile.ZipFile(build / 'large-count.zip') as archive:
            assert len(archive.infolist()) == 65536 and archive.read('part65535') == b''
            assert b'PK\x06\x06' in (build / 'large-count.zip').read_bytes()
        inspected += 1
        with zipfile.ZipFile(build / 'scoped-roundtrip.opc') as archive:
            scopes, pending, identities = [], {}, []
            for event, value in etree.iterparse(io.BytesIO(archive.read('model.xml')), events=('start-ns', 'start', 'end')):
                if event == 'start-ns':
                    pending[value[0]] = value[1]
                elif event == 'start':
                    scope = dict(scopes[-1]) if scopes else {}
                    scope.update(pending)
                    pending.clear()
                    scopes.append(scope)
                else:
                    if value.tag in ('{urn:scoped}first', '{urn:scoped}second'):
                        prefix, local = value.text.split(':')
                        identities.append((scopes[-1][prefix], local))
                    scopes.pop()
            assert identities == [('urn:first', 'item'), ('urn:second', 'item')]
        inspected += 1
    exchanges = 0
    if args.cpp_example:
        native, managed = build / 'cpp.opc', build / 'kotlin.opc'
        run([args.cpp_example.resolve(strict=True), native], 'cpp-producer')
        run([args.java, '-cp', build / 'checks.jar', 'ExchangeKt', native, managed], 'kotlin-exchange')
        run([args.cpp_example.resolve(strict=True), '--read', managed], 'cpp-consumer')
        for path in (native, managed):
            with zipfile.ZipFile(path) as archive:
                assert archive.testzip() is None
                assert archive.read('Attachments/sample.bin') == b'\0\1\xfe\xff'
                model = etree.fromstring(archive.read('Catalog/catalog.xml'))
                assert model.attrib == {'title': 'Parts & tools'}
                assert [child.attrib for child in model] == [
                    {'id': '7', 'label': 'Bolt <M6>'}, {'id': '8', 'label': 'Nut'}]
        exchanges = 2
    report = {'status': 'passed', 'scope': 'Actual Kotlin/JVM17 owning package profile on ' + sys.platform,
              'compiler_sha256': digest(compiler), 'kotlin_compiler_sha256': digest(compiler_jar), 'tools': tools,
              'independent_corpus_rows': len(rows), 'checks': counts, 'independent_outputs': inspected,
              'native_exchanges': exchanges,
              'wire_directory_inspections': directory_inspections,
              'wire_directory_negative_probes': directory_negative_probes,
              'reserved_hook_preflight_rejections': 1,
              'helper_collision': helper, 'source_hashes': {path.name: digest(path) for path in files},
              'elapsed_seconds': time.monotonic()-started,
              'limits': ['Logical cumulative/held-byte admission does not measure hard provider/process peak memory.',
                         'Private files use exclusive creation; owner-only POSIX permissions are used where supported.',
                         'Atomic replacement and directory synchronization are filesystem/platform dependent.',
                         'No application payload reference grammar or generated runtime-profile API is qualified by this chain.']}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"PASS: Kotlin {counts['passed']} checks, {len(rows)} independent corpus decisions, {inspected} independently inspected outputs")


if __name__ == '__main__':
    main()
