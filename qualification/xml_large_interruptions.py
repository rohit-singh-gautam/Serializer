# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Replay public owning XML interruption callbacks against retained large benchmark profiles."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from python_xml_benchmark import ROOT, suppress_windows_error_dialogs
from portable_xml_benchmark import digest

LANGUAGES = ('java', 'go', 'javascript', 'typescript', 'c', 'rust', 'csharp', 'kotlin', 'swift')
CONSUMERS = ROOT / 'qualification/xml_interruptions'


def main():
    """Build real SDK consumers from pinned benchmark sources and verify original failure plus recovery."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--benchmark', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--node-dependencies', type=Path, required=True)
    parser.add_argument('--provider-prefix', type=Path, required=True)
    parser.add_argument('--cargo-home', type=Path, required=True)
    parser.add_argument('--language', default=','.join(LANGUAGES))
    parser.add_argument('--compiler', type=Path, help='Immutable compiler with the additive public C#/Kotlin/Swift wrapper APIs')
    parser.add_argument('--kotlin-home', type=Path)
    args = parser.parse_args()
    suppress_windows_error_dialogs()
    languages = args.language.split(',')
    if os.name != 'nt' or len(languages) != len(set(languages)) or any(language not in LANGUAGES for language in languages):
        parser.error('Select distinct supported Windows SDKs: ' + ','.join(LANGUAGES))
    if any(language in languages for language in ('csharp','kotlin','swift')) and args.compiler is None:
        parser.error('The additive public wrapper profiles require --compiler')
    build, benchmark = args.build.resolve(), args.benchmark.resolve(strict=True)
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository output must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=False)
    environment = dict(os.environ)
    environment['CARGO_HOME'] = str(args.cargo_home.resolve(strict=True))
    environment['CARGO_NET_OFFLINE'] = 'true'
    environment['PATH'] = str(args.provider_prefix.resolve() / 'bin') + os.pathsep + environment.get('PATH', '')
    commands = []
    wrapper_compiler_hash = digest(args.compiler.resolve(strict=True)) if args.compiler else None

    def run(command, name, cwd=None):
        """Retain exact argv, output, and failures from actual SDK processes without interactive dialogs."""
        command = [str(part) for part in command]
        result = subprocess.run(command, cwd=cwd, env=environment, capture_output=True,
                                text=True, encoding='utf-8', errors='strict', timeout=240)
        commands.append({'argv': command, 'cwd': str(cwd) if cwd else None, 'returncode': result.returncode})
        (build / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if result.returncode:
            report['status'] = 'failed'
            report['failed_command'] = name
            (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        result.check_returncode()
        return result.stdout

    portable = json.loads((benchmark / 'portable/verification.json').read_text(encoding='utf-8'))
    mobile = json.loads((benchmark / 'mobile-bounded/verification.json').read_text(encoding='utf-8'))
    if portable['status'] != 'passed' or mobile['status'] != 'passed' or portable['compiler_sha256'] != mobile['compiler_sha256']:
        raise AssertionError('Require successful profiles from the same pinned compiler')
    fixtures = [benchmark / 'portable' / (name + '.xml') for name in ('repeated', 'fixed_seed')]
    if any(row['tags'] != 75000 for row in portable['workloads'][1:]):
        raise AssertionError('This qualification requires the retained 75,000-tag workloads')
    for fixture, row in zip(fixtures, portable['workloads'][1:]):
        if digest(fixture) != row['input_sha256']:
            raise AssertionError('Retained large fixture changed')
    report = {'status': 'running', 'scope': 'Actual Windows public owning XML callback interruption and recovery',
              'compiler_sha256': portable['compiler_sha256'], 'runner_sha256': digest(Path(__file__)),
              'public_wrapper_compiler_sha256': wrapper_compiler_hash,
              'benchmark_reports': {group: digest(benchmark / group / 'verification.json') for group in ('portable', 'mobile-bounded')},
              'sdk_metadata': {'portable': portable['tools'], 'mobile': mobile['tools']},
              'fixtures': [{'name': fixture.name, 'bytes': fixture.stat().st_size, 'sha256': digest(fixture)} for fixture in fixtures],
              'profiles': {}}
    if any(language in languages for language in ('javascript', 'typescript')):
        shutil.copytree(args.node_dependencies.resolve(strict=True), build / 'node_modules')
    canonical = {}
    for language in languages:
        directory = build / language
        directory.mkdir()
        group = 'mobile-bounded' if language in ('c', 'rust','kotlin','swift') else 'portable'
        original = benchmark / group / (language + '-xml')
        metadata = (mobile if group == 'mobile-bounded' else portable)['profiles'][language + '-xml']
        profile_compiler = portable['compiler_sha256']
        if language in ('csharp','kotlin','swift'):
            output=directory / {'csharp':'Schema.cs','kotlin':'Schema.kt','swift':'Schema.swift'}[language]
            run([args.compiler.resolve(), '--input', ROOT / 'example/xml/model.serializer', '--config', ROOT / 'example/xml/codegen.ini',
                 '--language',language,'--'+language+'.output',output,'--protocols','binary_none','--xml.enabled','true','--kotlin.package='],language+'-generate')
            metadata={'generated_sources':[{'name':output.name,'bytes':output.stat().st_size,'sha256':digest(output)}]}
            profile_compiler=digest(args.compiler)
        else:
            for item in metadata['generated_sources']:
                source = original / item['name']
                if digest(source) != item['sha256']:
                    raise AssertionError('Generated benchmark profile changed: ' + language)
                shutil.copyfile(source, directory / source.name)
        filename = {'java': 'Main.java', 'go': 'main.go', 'javascript': 'main.mjs',
                    'typescript': 'main.mts', 'c': 'main.c', 'rust': 'main.rs','csharp':'Program.cs','kotlin':'Main.kt','swift':'main.swift'}[language]
        source = CONSUMERS / ('main.mjs' if language == 'typescript' else filename)
        shutil.copyfile(source, directory / filename)
        if language == 'java':
            javac = portable['java_process_image']['compiler_path']
            run([javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d', directory / 'classes',
                 directory / 'Schema.java', directory / filename], language + '-compile')
            launch = [portable['java_process_image']['path'], '-Djdk.xml.maxGeneralEntitySizeLimit=4194304',
                      '-Djdk.xml.totalEntitySizeLimit=8388608', '-cp', directory / 'classes', 'Main']
        elif language == 'go':
            (directory / 'go.mod').write_text('module serializer.interruptions\n\ngo 1.22\n', encoding='utf-8')
            run(['go', '-C', directory, 'build', '-o', directory / 'interruptions.exe', '.'], language + '-compile')
            launch = [directory / 'interruptions.exe']
        elif language in ('javascript', 'typescript'):
            if language == 'typescript':
                shutil.copyfile(ROOT / 'qualification/xml_performance/node.d.mts', directory / 'node.d.mts')
                run(['node', build / 'node_modules/typescript/bin/tsc', '--strict', '--target', 'ES2020', '--module', 'NodeNext',
                     '--moduleResolution', 'NodeNext', directory / filename, directory / 'node.d.mts'], language + '-compile')
            launch = ['node', directory / 'main.mjs']
        elif language == 'csharp':
            project=directory / 'Controls.csproj'
            project.write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><Nullable>enable</Nullable><TreatWarningsAsErrors>true</TreatWarningsAsErrors></PropertyGroup></Project>',encoding='utf-8')
            run(['dotnet','build',project,'-c','Release','--nologo','-v:q'],language+'-compile')
            launch=['dotnet',directory / 'bin/Release/net8.0/Controls.dll']
        elif language == 'kotlin':
            if args.kotlin_home is None:
                parser.error('Kotlin requires --kotlin-home')
            run(['java','-cp',args.kotlin_home.resolve() / 'lib/kotlin-compiler.jar','org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                 '-kotlin-home',args.kotlin_home.resolve(),'-Werror','-include-runtime','-d',directory / 'controls.jar',directory / 'Schema.kt',directory / filename],language+'-compile')
            launch=[portable['java_process_image']['path'],'-Djdk.xml.maxGeneralEntitySizeLimit=4194304','-Djdk.xml.totalEntitySizeLimit=8388608','-jar',directory / 'controls.jar']
        elif language == 'swift':
            run(['swiftc','-O','-warnings-as-errors',directory / 'Schema.swift',directory / filename,'-o',directory / 'controls.exe'],language+'-compile')
            launch=[directory / 'controls.exe']
        elif language == 'rust':
            for name in ('Cargo.toml', 'Cargo.lock'):
                shutil.copyfile(original / name, directory / name)
            run(['cargo', 'build', '--locked', '--release', '--offline', '--manifest-path', directory / 'Cargo.toml'], language + '-compile')
            launch = [directory / 'target/release/serializer-xml-example.exe']
        else:
            (directory / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 4.0)\nproject(xml_interruptions LANGUAGES C)\n'
                'find_package(LibXml2 REQUIRED)\nadd_executable(interruptions main.c)\n'
                'target_link_libraries(interruptions PRIVATE LibXml2::LibXml2)\n'
                'target_compile_options(interruptions PRIVATE /std:c11 /W4 /WX /utf-8)\n'
                'target_compile_definitions(interruptions PRIVATE _CRT_SECURE_NO_WARNINGS)\n', encoding='utf-8')
            run(['cmake', '-S', directory, '-B', directory / 'native', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                 '-DCMAKE_PREFIX_PATH=' + str(args.provider_prefix.resolve())], language + '-configure')
            run(['cmake', '--build', directory / 'native', '--parallel', '2'], language + '-compile')
            launch = [directory / 'native/interruptions.exe']
        text = run([*launch, directory, *fixtures], language + '-controls')
        rows = [json.loads(line) for line in text.splitlines() if line.startswith('{')]
        if len(rows) != 2 or any(row['verified_interruptions'] != 6 or row['recovered'] is not True for row in rows):
            raise AssertionError('Incomplete actual interruption controls: ' + language)
        for index in (1, 2):
            wire = (directory / (str(index) + '.wire')).read_bytes()
            if index in canonical and canonical[index] != wire:
                raise AssertionError('Recovered native values differ across SDKs: ' + language)
            canonical[index] = wire
        report['profiles'][language] = {'consumer_sha256': digest(directory / filename), 'source_profile': metadata,'compiler_sha256':profile_compiler,
            'controls': rows, 'native_outputs': [{'bytes': len(canonical[index]), 'sha256': digest(directory / (str(index) + '.wire'))} for index in (1, 2)],
            'deadline_category': 'Caller callback checks an already expired host clock; no native deadline API asserted'}
        (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('PASS: twelve large XML interruptions and per-failure recovery: ' + language, flush=True)
    report['status'] = 'passed'
    if args.compiler and digest(args.compiler) != wrapper_compiler_hash:
        raise AssertionError('Public wrapper compiler changed during qualification')
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
