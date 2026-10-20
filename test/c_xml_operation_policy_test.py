# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify real C operation policies, default admissions and exact independent XML inventories."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from rust_xml_operation_policy_test import CASES, logical_counts

ROOT = Path(__file__).resolve().parents[1]
RESOURCES = ROOT / 'test/resources'


def digest(path):
    """Pin retained verification to the exact maintained source or generated executable."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """Regenerate native C and exercise complete operation calls with actual configured providers."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--cc', default=shutil.which('cl') if os.name == 'nt' else shutil.which('cc'))
    parser.add_argument('--provider-prefix', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(strict=True), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must stay beneath ignored out/')
    if not args.cc:
        parser.error('Provide an actual C11 compiler in --cc')
    build.mkdir(parents=True, exist_ok=True)
    prefix = args.provider_prefix.resolve() if args.provider_prefix else None
    environment = os.environ.copy()
    if prefix and os.name == 'nt':
        environment['PATH'] = str(prefix / 'bin') + os.pathsep + environment['PATH']
    started = time.monotonic()

    def run(command, name):
        """Retain both streams from each real generation, compiler and SDK invocation."""
        result = subprocess.run([str(value) for value in command], capture_output=True,
                                text=True, encoding='utf-8', env=environment, timeout=300)
        (build / f'{name}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode:
            raise AssertionError(f'{name} failed ({result.returncode}):\n{result.stdout}{result.stderr}')
        return result.stdout

    source = build / 'schema.h'
    run([compiler, '--input', RESOURCES / 'c_xml_operation_policy.serializer', '--config',
         RESOURCES / 'rust_xml_operation_policy.ini', '--language', 'c', '--c.output', source], 'generation')
    consumer = build / 'main.c'
    shutil.copyfile(RESOURCES / 'c_xml_operation_policy_main.c', consumer)
    executable = build / ('consumer.exe' if os.name == 'nt' else 'consumer')
    msvc = Path(args.cc).name.lower() in ('cl', 'cl.exe')
    if msvc:
        command = [args.cc, '/nologo', '/std:c11', '/W4', '/WX', '/O2', '/utf-8',
                   '/D_CRT_SECURE_NO_WARNINGS', consumer, '/Fe' + str(executable), '/Fo' + str(build / 'main.obj')]
        if prefix:
            command += ['/I' + str(prefix / 'include'), '/I' + str(prefix / 'include/libxml2')]
        command += ['/link'] + (['/LIBPATH:' + str(prefix / 'lib')] if prefix else []) + ['libxml2.lib']
    else:
        command = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2', consumer, '-o', executable]
        command += subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libxml-2.0'], text=True).split()
        command += ['-lm']
    run(command, 'compile')
    cases = {**CASES, 'factory': '<factory_defaults><nested><label>wire</label></nested></factory_defaults>'}
    evidence = {}
    for name, document in cases.items():
        path, output = build / f'{name}.xml', build / f'{name}.output.xml'
        path.write_text(document, encoding='utf-8')
        record = json.loads(run([executable, name, path, output], name).strip())
        for direction, xml_path in [('input', path), ('output', output)]:
            expected = logical_counts(xml_path.read_bytes())
            if record[direction][2:5] != expected:
                raise AssertionError(f'{name} {direction}: {record[direction][2:5]} != independent {expected}')
        evidence[name] = {**record, 'input_sha256': digest(path), 'output_sha256': digest(output)}
    if args.sanitize:
        sanitized = build / ('consumer_asan.exe' if os.name == 'nt' else 'consumer_asan')
        checked = [str(value).replace(str(executable), str(sanitized)).replace(str(build / 'main.obj'),
                   str(build / 'main_asan.obj')) for value in command if value != '/O2']
        checked[1:1] = (['/Od', '/Zi', '/fsanitize=address', '/Fd' + str(build / 'native_asan.pdb')] if msvc else
                       ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])
        run(checked, 'sanitize-compile')
        for name in cases:
            run([sanitized, name, build / f'{name}.xml', build / f'{name}.asan.xml'], name + '-sanitize')
    report = {'status': 'passed', 'compiler_sha256': digest(compiler), 'generated_sha256': digest(source),
              'executable_sha256': digest(executable), 'policies': evidence, 'sanitize': args.sanitize,
              'assertions': sum(value['assertions'] for value in evidence.values()),
              'test_source_sha256': {str(path.relative_to(ROOT)): digest(path) for path in [Path(__file__),
                  RESOURCES / 'c_xml_operation_policy.serializer', RESOURCES / 'c_xml_operation_policy_main.c',
                  RESOURCES / 'rust_xml_operation_policy.serializer', RESOURCES / 'rust_xml_operation_policy.ini']},
              'elapsed_seconds': round(time.monotonic() - started, 3)}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"PASS: {report['assertions']} C operation assertions and twelve independent exact XML inventories")


if __name__ == '__main__':
    main()
