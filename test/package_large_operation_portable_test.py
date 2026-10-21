# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Execute large composed OPC/DEFLATE/XML identity/publication controls in actual portable SDKs."""

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from package_large_operation_corpus import fixtures, assert_output, DEFAULT_ENTRIES_PER_REGISTRY
from package_node_dependencies import prepare_node_dependencies

ROOT = Path(__file__).resolve().parents[1]
RESOURCES = ROOT / 'test/resources/package_large_operation'


def main():
    """Retain exact compiler/SDK commands, failures, logical counters and independent output decisions."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp,go,javascript,typescript')
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--entries-per-registry', type=int, default=DEFAULT_ENTRIES_PER_REGISTRY)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve(strict=True)
    args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(
            language not in ('java', 'csharp', 'go', 'javascript', 'typescript') for language in languages):
        parser.error('Choose distinct portable SDK names')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath ignored out/')
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    args.build.mkdir(parents=True, exist_ok=True)
    commands, failures, results = [], [], {}
    report = {'status': 'running', 'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'commands': commands, 'failures': failures, 'results': results}

    def persist():
        """Keep interrupted or failed execution distinct from completed qualification."""
        (args.build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(command, label, directory=None):
        """Capture every real SDK process and preserve its failing output before raising."""
        command = [str(value) for value in command]
        working = directory or args.build
        started = time.monotonic()
        process = subprocess.run(command, cwd=working, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True, encoding='utf-8', errors='replace', timeout=1200)
        log = args.build / (label + '.log')
        log.write_text(process.stdout, encoding='utf-8')
        evidence = {'command': command, 'cwd': str(working), 'log': str(log), 'exit_code': process.returncode,
                    'seconds': time.monotonic() - started}
        commands.append(evidence)
        if process.returncode:
            failures.append(evidence)
            report['status'] = 'failed'
            persist()
            print(process.stdout, flush=True)
            raise subprocess.CalledProcessError(process.returncode, command)
        persist()
        return process.stdout

    fixture = args.build / 'corpus'
    report['inventory'] = fixtures(fixture, args.entries_per_registry)
    modules = None
    if set(languages) & {'javascript', 'typescript'}:
        modules = prepare_node_dependencies(args.build, args.node_dependencies or ROOT, run)
    outputs = {'java': 'Schema.java', 'csharp': 'Schema.cs', 'go': 'schema.go',
               'javascript': 'schema.mjs', 'typescript': 'schema.mjs'}
    for language in languages:
        directory = args.build / language
        directory.mkdir(exist_ok=True)
        for name in ('valid.opc', 'invalid.opc', 'inventory.json'):
            shutil.copyfile(fixture / name, directory / name)
        backend = 'js,typescript' if language in ('javascript', 'typescript') else language
        command = [args.compiler, '--input', ROOT / 'test/resources/opc_identity.serializer', '--config',
                   ROOT / 'test/resources/opc_identity.ini', '--language', backend]
        if language in ('javascript', 'typescript'):
            command += ['--js.output', directory / 'schema.mjs', '--typescript.output', directory / 'schema.d.mts']
        else:
            command += [f'--{language}.output', directory / outputs[language]]
        if language == 'go':
            command += ['--go.package', 'main']
        if language == 'csharp':
            command += ['--csharp.namespace', 'SerializerGenerated']
        run(command, language + '-generate')
        if language == 'java':
            shutil.copyfile(RESOURCES / 'Main.java', directory / 'Main.java')
            run(['javac', '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d',
                 directory / 'classes', directory / 'Schema.java', directory / 'Main.java'], language + '-compile')
            launch = ['java', '-Xmx2g', '-cp', directory / 'classes', 'Main']
        elif language == 'csharp':
            shutil.copyfile(RESOURCES / 'Program.cs', directory / 'Program.cs')
            (directory / 'Large.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup>'
                '<OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><Nullable>enable</Nullable>'
                '<TreatWarningsAsErrors>true</TreatWarningsAsErrors></PropertyGroup></Project>', encoding='utf-8')
            run(['dotnet', 'build', directory / 'Large.csproj', '--configuration', 'Release', '--nologo',
                 '--disable-build-servers', '-nodeReuse:false'], language + '-compile')
            launch = ['dotnet', directory / 'bin/Release/net8.0/Large.dll']
        elif language == 'go':
            shutil.copyfile(RESOURCES / 'main.go', directory / 'main.go')
            (directory / 'go.mod').write_text('module serializer.large.operation\n\ngo 1.22\n', encoding='utf-8')
            executable = directory / ('main.exe' if os.name == 'nt' else 'main')
            run(['go', '-C', directory, 'build', '-trimpath', '-o', executable, '.'], language + '-compile')
            launch = [executable]
        else:
            shutil.copyfile(RESOURCES / 'main.mts', directory / 'main.mts')
            shutil.copyfile(RESOURCES / 'node.d.mts', directory / 'node.d.mts')
            run(['node', modules / 'typescript/bin/tsc', '--strict', '--target', 'ES2022', '--module', 'NodeNext',
                 '--moduleResolution', 'NodeNext', directory / 'main.mts', directory / 'node.d.mts'], language + '-compile')
            launch = ['node', '--max-old-space-size=2048', directory / 'main.mjs']
        output = run([*launch, directory, str(args.entries_per_registry)], language + '-acceptance')
        lines = [line.removeprefix('LEDGER\t') for line in output.splitlines() if line.startswith('LEDGER\t')]
        if len(lines) != 1:
            raise AssertionError('Missing actual SDK ledger result: ' + language)
        result = json.loads(lines[0])
        try:
            for name in ('output.opc', 'exact.opc'):
                assert_output(directory / name, args.entries_per_registry)
            if list(directory.glob('.serializer-*')):
                raise AssertionError('Atomic Save retained private staging artifacts')
        except Exception as error:
            failures.append({'stage': 'independent-output', 'language': language, 'error': repr(error)})
            report['status'] = 'failed'
            persist()
            raise
        result['independent_zip_xsd_preservation_outputs'] = 2
        result['generated_sha256'] = hashlib.sha256((directory / outputs[language]).read_bytes()).hexdigest()
        consumer = directory / {'java': 'Main.java', 'csharp': 'Program.cs', 'go': 'main.go',
                                'javascript': 'main.mts', 'typescript': 'main.mts'}[language]
        result['consumer_sha256'] = hashlib.sha256(consumer.read_bytes()).hexdigest()
        if language in ('javascript', 'typescript'):
            result['declarations_sha256'] = hashlib.sha256((directory / 'schema.d.mts').read_bytes()).hexdigest()
        results[language] = result
        persist()
        print(language + ': ' + str(result['checks']) + ' large composed controls pass', flush=True)
    report['status'] = 'pass'
    persist()
    print('PASS: actual large composed package controls and independent ZIP/XSD/preservation oracles', flush=True)


if __name__ == '__main__':
    main()
