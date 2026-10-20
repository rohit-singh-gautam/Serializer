# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Exercise additive Rust native XML policies and independent exact logical counters."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from xml.parsers import expat

ROOT = Path(__file__).resolve().parent.parent
RESOURCES = ROOT / 'test/resources'
CASES = {
    'scalar': '<scalar>1 2</scalar>',
    'map': '<maps><entries><entry id="1"><data>one</data></entry><entry id="2"><data>two</data></entry></entries></maps>',
    'mixed': '<mixed xmlns="urn:serializer:operation-policy" id="p">head&amp;é<f:em xmlns:f="urn:serializer:operation-format">child</f:em>tail</mixed>',
    'digest': '<digests exact="00Ff"/>',
    'defaults': '<defaults><nested bytes="000000"><wireValue>x</wireValue></nested><slots>0000</slots><slots>0000</slots><slots>0000</slots></defaults>',
}


def digest(path):
    """Bind retained evidence to the exact source, compiler or executable bytes used."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def logical_counts(data):
    """Count physical declarations and expanded names with an independent namespace-aware XML parser."""
    parser = expat.ParserCreate(namespace_separator='|')
    counts = [0, 0, 0]

    def length(value):
        """Count UTF-8 bytes without the independent parser's namespace separator."""
        return len(value.replace('|', '').encode('utf-8'))

    def namespace(prefix, uri):
        """Include each physically declared namespace attribute exactly once."""
        counts[1] += 1
        counts[2] += length(prefix or '') + length(uri or '')

    def start(name, attributes):
        """Count one logical element and all ordinary expanded attributes."""
        counts[0] += 1
        counts[1] += len(attributes)
        counts[2] += length(name) + sum(length(key) + length(value) for key, value in attributes.items())

    def text(value):
        """Count normalized text and character references independently of tokenizer chunk boundaries."""
        counts[2] += length(value)

    parser.StartNamespaceDeclHandler = namespace
    parser.StartElementHandler = start
    parser.CharacterDataHandler = text
    parser.Parse(data, True)
    return counts


def main():
    """Compile one actual Rust SDK, then retain complete evidence for five distinct native policies."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--cargo', default='cargo')
    arguments = parser.parse_args()
    compiler, build = arguments.compiler.resolve(), arguments.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository test output must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment['RUSTFLAGS'] = '-D warnings'
    started = time.monotonic()

    def run(command, name):
        """Retain both streams and fail immediately if the selected real tool or SDK fails."""
        result = subprocess.run([str(value) for value in command], capture_output=True, text=True,
                                encoding='utf-8', env=environment, timeout=300)
        (build / f'{name}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode:
            raise AssertionError(f'{name} failed ({result.returncode}):\n{result.stdout}{result.stderr}')
        return result.stdout

    source = build / 'schema.rs'
    run([compiler, '--input', RESOURCES / 'rust_xml_operation_policy.serializer',
         '--config', RESOURCES / 'rust_xml_operation_policy.ini', '--language', 'rust',
         '--rust.output', source], 'generation')
    module = re.search(r'// Generated Rust XML operation module: (\w+)', source.read_text(encoding='utf-8')).group(1)
    (build / 'main.rs').write_text((RESOURCES / 'rust_xml_operation_policy_main.rs').read_text(
        encoding='utf-8').replace('@operation@', module), encoding='utf-8')
    for name in ('Cargo.toml', 'Cargo.lock'):
        shutil.copyfile(ROOT / 'example/xml/rust' / name, build / name)
    run([arguments.cargo, 'build', '--release', '--locked', '--offline', '--manifest-path', build / 'Cargo.toml'], 'compile')
    executable = build / 'target/release' / ('serializer-xml-example.exe' if os.name == 'nt' else 'serializer-xml-example')
    evidence = {}
    for name, document in CASES.items():
        path, output = build / f'{name}.xml', build / f'{name}.output.xml'
        path.write_text(document, encoding='utf-8')
        records = [json.loads(line) for line in run([executable, name, path, output], name).splitlines() if line.startswith('{')]
        counters = records[0]
        for direction, xml_path in [('input', path), ('output', output)]:
            expected = logical_counts(xml_path.read_bytes())
            # Resource order is declared by the additive native API, independently of package ordinals.
            actual = counters[direction][2:5]
            if actual != expected:
                raise AssertionError(f'{name} {direction}: actual XML {actual}, independent XML {expected}')
        evidence[name] = {**records[-1], **counters, 'input_sha256': digest(path), 'output_sha256': digest(output)}
    report = {'compiler_sha256': digest(compiler), 'source_sha256': digest(source),
              'executable_sha256': digest(executable), 'policies': evidence,
              'test_source_sha256': {str(path.relative_to(ROOT)): digest(path) for path in [
                  Path(__file__), RESOURCES / 'rust_xml_operation_policy_main.rs',
                  RESOURCES / 'rust_xml_operation_policy.serializer', RESOURCES / 'rust_xml_operation_policy.ini']},
              'assertions': sum(item['assertions'] for item in evidence.values()),
              'elapsed_seconds': round(time.monotonic() - started, 3)}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"PASS: {report['assertions']} Rust operation policy assertions and ten independent exact XML counter comparisons")


if __name__ == '__main__':
    main()
