# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Retain actual prepared-provider and compression malformed/resource/interruption qualification evidence."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ('java', 'kotlin', 'csharp', 'go', 'javascript', 'typescript', 'rust', 'swift', 'c')


def digest(path):
    """Hash the exact retained tool/source/log bytes independently of subprocess diagnostics."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """Execute every selected real SDK; absence, interruption and failed subprocesses never count as passes."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default=','.join(LANGUAGES))
    parser.add_argument('--group', choices=('providers', 'compression', 'all'), default='all')
    parser.add_argument('--bridge', type=Path)
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--kotlinc', type=Path)
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--typescript', type=Path)
    parser.add_argument('--cargo-home', type=Path)
    parser.add_argument('--jobs', type=int, default=3)
    args = parser.parse_args()
    compiler = args.compiler.resolve(strict=True); build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in LANGUAGES for value in languages):
        parser.error('Choose distinct SDKs from '+','.join(LANGUAGES))
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    if not 1 <= args.jobs <= len(LANGUAGES): parser.error('Choose between one and nine concurrent SDK jobs')
    build.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy(); environment['PYTHONDONTWRITEBYTECODE'] = '1'
    if args.kotlinc: environment['SERIALIZER_KOTLINC'] = str(args.kotlinc.resolve(strict=True))
    if args.cargo_home: environment['CARGO_HOME'] = str(args.cargo_home.resolve(strict=True))
    commands = []

    def command(label, script, extra=(), selected=()):
        """Build one maintained corpus invocation while recording its exact claimed language inventory."""
        commands.append((label, [sys.executable, '-B', str(ROOT/'test'/script), '--compiler', str(compiler),
                                '--build', str(build/label), *map(str, extra)], tuple(selected)))

    if args.group in ('providers', 'all'):
        portable = [value for value in languages if value in ('java', 'kotlin', 'csharp')]
        if portable: command('prepared-jvm-dotnet', 'portable_xml_provider_test.py', ('--language', ','.join(portable)), portable)
        native = [value for value in languages if value in ('go', 'rust', 'swift', 'c')]
        if native:
            if not args.bridge or not args.libxml2_prefix: parser.error('Native prepared provider checks require --bridge and --libxml2-prefix')
            command('prepared-native', 'native_xml_provider_test.py', ('--language', ','.join(native), '--bridge',
                    args.bridge.resolve(strict=True), '--libxml2-prefix', args.libxml2_prefix.resolve(strict=True)), native)
        if any(value in languages for value in ('javascript', 'typescript')):
            if not args.node_dependencies: parser.error('Use --node-dependencies for the exactly locked libxml2-wasm provider installation')
            command('prepared-js-typescript', 'javascript_xml_provider_test.py',
                    ('--node-dependencies', args.node_dependencies.resolve(strict=True)), ('javascript', 'typescript'))
    if args.group in ('compression', 'all'):
        for language in languages:
            if language == 'typescript':
                if 'javascript' in languages: continue
                language = 'javascript'
            extra = []
            if language == 'kotlin':
                if not args.kotlinc: parser.error('Kotlin checks require --kotlinc')
                extra += ['--kotlinc', args.kotlinc.resolve(strict=True)]
            if language == 'javascript':
                typescript = args.typescript or (args.node_dependencies/'typescript/bin/tsc' if args.node_dependencies else None)
                if not typescript: parser.error('Strict TS checks require --typescript or --node-dependencies')
                extra += ['--tsc', typescript.resolve(strict=True)]
            if language == 'c':
                if not args.libxml2_prefix: parser.error('C compression checks require the configured --libxml2-prefix with zlib')
                zlib_library = next((args.libxml2_prefix.resolve()/'lib'/name for name in ('z.lib','zlib.lib')
                                     if (args.libxml2_prefix.resolve()/'lib'/name).is_file()), None)
                if zlib_library is None: parser.error('The selected provider prefix has no zlib import library')
                extra += ['--zlib-include', args.libxml2_prefix.resolve()/'include',
                          '--zlib-library', zlib_library]
                environment['PATH'] = str(args.libxml2_prefix.resolve()/'bin')+os.pathsep+environment.get('PATH', '')
            selected = ('javascript', 'typescript') if language == 'javascript' else (language,)
            command('compression-'+language, language+'_compression_test.py', extra, selected)
    started = time.monotonic(); compiler_hash = digest(compiler)
    report = {'status':'running', 'languages':languages, 'group':args.group, 'platform':platform.platform(),
              'python':sys.version, 'compiler':str(compiler), 'compiler_sha256':compiler_hash,
              'runner_sha256':digest(Path(__file__)), 'commands':[],
              'scope':'Actual selected SDK prepared XML providers plus independent standard-frame malformed/resource/interruption corpus. Provider-internal hard peak allocation and preemption are not inferred.'}
    report_path = build/'verification.json'

    def execute(item):
        """Retain one complete bounded-duration process log without substituting emulated SDK results."""
        label, argv, selected = item; log = build/(label+'.log'); beginning = time.monotonic()
        with log.open('wb') as output:
            result = subprocess.run(argv, cwd=ROOT, env=environment, stdout=output, stderr=subprocess.STDOUT, timeout=1800)
        return {'label':label, 'languages':selected, 'argv':argv, 'returncode':result.returncode,
                'elapsed_seconds':round(time.monotonic()-beginning,3), 'log':str(log), 'log_sha256':digest(log),
                'driver_sha256':digest(Path(argv[2]))}

    try:
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            for future in as_completed([executor.submit(execute, item) for item in commands]):
                result = future.result(); report['commands'].append(result)
                report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
                print(('PASS' if result['returncode']==0 else 'FAIL')+': '+result['label'],flush=True)
        if digest(compiler) != compiler_hash: raise AssertionError('Compiler changed during qualification')
        report['status'] = 'passed' if all(row['returncode']==0 for row in report['commands']) else 'failed'
    except BaseException as failure:
        report['status']='failed'; report['failure']=str(failure); raise
    finally:
        report['elapsed_seconds']=round(time.monotonic()-started,3)
        report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    if report['status']!='passed': raise SystemExit(1)
    print('PASS: retained '+str(len(commands))+' actual SDK corpus executions',flush=True)


if __name__ == '__main__': main()
