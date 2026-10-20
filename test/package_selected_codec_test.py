# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify public selected-codec composition from separate Go, JS/TS and Kotlin consumers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
import zipfile

from package_node_dependencies import prepare_node_dependencies
from package_shared_codec_budget_test import fixtures

ROOT = Path(__file__).resolve().parents[1]


def main():
    """Generate real SDK libraries and compile consumers without access to private runtime hooks."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', choices=('go', 'javascript', 'typescript', 'kotlin'), required=True)
    parser.add_argument('--go', default='go')
    parser.add_argument('--node', default='node')
    parser.add_argument('--typescript-cli', type=Path)
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--java', default='java')
    parser.add_argument('--kotlinc', type=Path)
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(strict=True), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    (build / 'verification.json').unlink(missing_ok=True)
    fixtures(build / 'fixtures')
    environment = os.environ.copy()
    environment.update(GOWORK='off', GOFLAGS='-mod=readonly', GOCACHE=str(build / 'gocache'))
    commands, started = [], time.monotonic()

    def run(command, name):
        """Retain actual SDK diagnostics and refuse to report partial qualification as success."""
        argv = [str(value) for value in command]
        result = subprocess.run(argv, cwd=build, env=environment, capture_output=True, text=True,
                                encoding='utf-8', timeout=600)
        (build / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'argv': argv, 'returncode': result.returncode})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if result.returncode:
            print(result.stdout + result.stderr, end='')
            result.check_returncode()
        return result.stdout + result.stderr

    base = [compiler, '--input', ROOT / 'test/resources/opc_identity.serializer',
            '--config', ROOT / 'test/resources/opc_identity.ini']
    if args.language == 'go':
        source = build / 'model/identity.go'
        source.parent.mkdir(exist_ok=True)
        run(base + ['--language', 'go', '--go.output', source, '--go.package', 'model'], 'generation')
        prefix = re.search(r'// Generated Go selected-codec prefix: (\w+)', source.read_text(encoding='utf-8')).group(1)
        (build / 'go.mod').write_text('module serializer-selected\n\ngo 1.23\n', encoding='utf-8')
        (build / 'main.go').write_text((ROOT / 'test/resources/package_selected_codec_main.go').read_text(
            encoding='utf-8').replace('@selected@', prefix), encoding='utf-8')
        tool = run([args.go, 'version'], 'version').strip()
        output = run([args.go, 'run', '.'], 'sdk')
    elif args.language == 'kotlin':
        if not args.kotlinc:
            parser.error('Provide the actual Kotlin compiler in --kotlinc')
        source = build / 'Identity.kt'
        run(base + ['--language', 'kotlin', '--output', source, '--kotlin.package', 'generated'], 'generation')
        (build / 'Main.kt').write_text((ROOT / 'test/resources/package_selected_codec_main.kt').read_text(
            encoding='utf-8'), encoding='utf-8')
        home = args.kotlinc.resolve().parent.parent
        command = [args.java, '-Xmx2g', '-cp', home / 'lib/kotlin-compiler.jar',
                   'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-kotlin-home', home, '-jvm-target', '17', '-Werror']
        tool = run(command[:7] + ['-version'], 'version').strip()
        run(command + ['-module-name', 'serializer_library', '-d', build / 'generated.jar', source], 'library-compile')
        run(command + ['-module-name', 'independent_consumer', '-classpath', build / 'generated.jar',
                       '-include-runtime', '-d', build / 'consumer.jar', build / 'Main.kt'], 'consumer-compile')
        output = run([args.java, '-cp', str(build / 'consumer.jar') + os.pathsep + str(build / 'generated.jar'),
                      'consumer.MainKt'], 'sdk')
    else:
        if not args.node_dependencies:
            parser.error('Provide the repository-locked Node dependencies')
        prepare_node_dependencies(build, args.node_dependencies, run)
        typescript = args.language == 'typescript'
        source = build / ('identity.d.mts' if typescript else 'identity.mjs')
        if typescript:
            run(base + ['--language', 'js,typescript', '--js.output', build / 'identity.mjs',
                        '--typescript.output', source], 'generation')
        else:
            run(base + ['--language', 'js', '--output', source], 'generation')
        module = './identity.mjs'
        if typescript:
            if not args.typescript_cli:
                parser.error('Provide the actual TypeScript CLI')
            (build / 'api.mts').write_text((ROOT / 'test/resources/package_selected_codec_api.ts').read_text(
                encoding='utf-8'), encoding='utf-8')
            run([args.node, args.typescript_cli.resolve(), '--target', 'ES2022', '--module', 'NodeNext',
                 '--moduleResolution', 'NodeNext', '--strict', '--noEmitOnError',
                 '--outDir', build, build / 'api.mts'], 'strict-consumer-compile')
        (build / 'main.mjs').write_text((ROOT / 'test/resources/package_selected_codec_main.mjs').read_text(
            encoding='utf-8').replace('@module@', module).replace('@typed_api@',
                "const {typedLoad}=await import('./api.mjs');const typedBudget=new P.Budget();await typedLoad(bytes,typedBudget,provider);check(typedBudget.used.references===27,'strict typed external callbacks share ledger');"
                if typescript else ''), encoding='utf-8')
        tool = run([args.node, '--version'], 'version').strip()
        output = run([args.node, build / 'main.mjs'], 'sdk')
    counts = [json.loads(line) for line in output.splitlines() if line.startswith('{')][-1]
    with zipfile.ZipFile(build / 'fixtures/produced.opc') as archive:
        assert archive.testzip() is None
        assert len([name for name in archive.namelist() if name.startswith('Registries/')]) == 3
    report = {'language': args.language, 'tool': tool, 'sdk': counts, 'separate_consumer': True,
              'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
              'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'commands': commands,
              'elapsed_seconds': round(time.monotonic() - started, 3)}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
