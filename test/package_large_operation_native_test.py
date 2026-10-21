# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify large owning OPC/DEFLATE/XML identity operations through real native SDKs."""
import argparse
import ctypes
import dataclasses
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from package_large_operation_corpus import assert_output, fixtures

ROOT = Path(__file__).resolve().parents[1]
MEBIBYTE = 1024 * 1024
SENTINEL = b'previous complete application file\x00\x7f'


def python_gate(source, directory, inventory):
    """Use generated public APIs and their one cumulative ledger through load and atomic Save."""
    spec = importlib.util.spec_from_file_location('large_identity_generated', source)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    archive = module.IdentityPackageArchive
    policy = module.ExtensionPolicy('preserve', 'preserve')
    valid = (directory / 'valid.opc').read_bytes()
    invalid = (directory / 'invalid.opc').read_bytes()
    destination = directory / 'produced.opc'
    checks = []
    counts = {'polls': 0}

    def check(value, name):
        """Retain every successful semantic assertion independently of interpreter optimization."""
        if not value:
            raise AssertionError(name)
        checks.append(name)

    def limits(**changes):
        """Select explicit finite large policies without relaxing XML identities or owning profiles."""
        value = dict(max_archive_bytes=128*MEBIBYTE, max_io_bytes=1024*MEBIBYTE,
                     max_part_bytes=16*MEBIBYTE, max_total_expanded_bytes=512*MEBIBYTE,
                     max_buffer_bytes=256*MEBIBYTE, max_metadata_bytes=64*MEBIBYTE,
                     max_decoder_allocation_bytes=2048*MEBIBYTE, max_work_units=8192*MEBIBYTE,
                     max_references=4000000, max_entries=1024, max_relationships=1024)
        value.update(changes)
        return module.PackageLimits(**value)

    def operation(shared, fault=None):
        """Keep input decode, identity checks, preservation, DEFLATE encode/redecode and Save on one ledger."""
        loaded = archive.load(valid, budget=shared, policy=policy)
        check(len(loaded.value.registries) == inventory['registries'], 'complete owning aggregate')
        for item in loaded.value.registries:
            check(len(item.value.requests) == inventory['entries_per_registry'] and
                  len(item.value.entries) == inventory['entries_per_registry'], 'large local identity owner')
        value = archive.Input(loaded.value.registries, loaded.extensions, {'registries': 'deflate'})
        archive.save(destination, value, budget=shared, fault=fault)
        return loaded

    def observe():
        """Measure actual public callback boundaries without storing a trace proportional to model size."""
        counts['polls'] += 1
        return False

    baseline = module.PackageBudget(limits(is_cancelled=observe))
    publication = {}

    def observe_publication(boundary):
        """Locate the actual last pre-replacement poll instead of assuming the final poll precedes commit."""
        publication[boundary] = counts['polls']

    started = time.monotonic()
    print('Python large baseline started', flush=True)
    loaded = operation(baseline, observe_publication)
    duration = time.monotonic()-started
    measured = dict(baseline.used)
    polls = counts['polls']
    print('Python large baseline complete:', duration, measured, flush=True)
    check(measured['references'] >= inventory['identity_tuples']*3, 'one ledger includes all three identity traversals')
    assert_output(destination, inventory['entries_per_registry'])
    value = archive.Input(loaded.value.registries, loaded.extensions, {'registries': 'deflate'})
    publication_checks = []
    for stage in ('before_publish', 'after_publish'):
        for mode in ('cancel', 'clock'):
            destination.write_bytes(SENTINEL)
            marker = RuntimeError(stage+'-'+mode)
            state = {'armed': False, 'deadline': None}
            staging_before = set(directory.glob('.serializer-opc-*'))

            def final_policy():
                """Expose a returning hook's cancellation at the immediate publication fence."""
                if state['armed'] and (mode != 'clock' or time.monotonic() >= state['deadline']):
                    raise marker
                return False

            def fault(boundary):
                """Arm cancellation or a real expired host clock without throwing from the hook."""
                if boundary == stage:
                    if mode == 'clock':
                        state['deadline'] = time.monotonic()+0.001
                        time.sleep(0.002)
                    state['armed'] = True

            caught = None
            try:
                archive.save(destination, value, budget=module.PackageBudget(limits(is_cancelled=final_policy)), fault=fault)
            except Exception as error:
                caught = error
            if stage == 'before_publish':
                okay = caught is marker and destination.read_bytes() == SENTINEL
            else:
                okay = isinstance(caught, module.PackageError) and caught.code == 'publication_uncertain' and caught.__cause__ is marker
                if okay:
                    assert_output(destination, inventory['entries_per_registry'])
            publication_checks.append({'stage':stage, 'mode':mode, 'passed':okay,
                                       'error':getattr(caught, 'code', type(caught).__name__)})
            check(set(directory.glob('.serializer-opc-*')) == staging_before, 'publication staging cleaned '+stage+'-'+mode)
            print('Python publication control:', publication_checks[-1], flush=True)
    (directory/'publication-controls.json').write_text(json.dumps(publication_checks, indent=2)+'\n')
    check(all(item['passed'] for item in publication_checks), 'returning publication hooks are immediately fenced')
    final_reference_counts = []
    limit_fields = {'references':'max_references', 'work_units':'max_work_units',
                    'decoder_allocation_bytes':'max_decoder_allocation_bytes'}
    for resource in ('references', 'work_units', 'decoder_allocation_bytes'):
        for tight in (False, True):
            destination.write_bytes(SENTINEL)
            maximum = getattr(baseline.limits, limit_fields[resource])
            bounded = module.PackageBudget(dataclasses.replace(limits(), **{
                limit_fields[resource]: measured[resource]-int(tight)}))
            try:
                operation(bounded)
                check(not tight, 'exact cumulative '+resource)
            except module.PackageError as error:
                check(tight and error.code == 'resource_limit', 'one-tight cumulative '+resource)
                check(destination.read_bytes() == SENTINEL, 'one-tight leaves published file '+resource)
            final_reference_counts.append({'resource': resource, 'tight': tight,
                                           'used': bounded.used[resource], 'generous_maximum': maximum})
            print('Python exact bound:', final_reference_counts[-1], flush=True)
    published = []
    try:
        archive.load(invalid, budget=module.PackageBudget(limits()), policy=policy,
                     validate=lambda *_: published.append(True))
        raise AssertionError('missing final-owner keyref accepted')
    except module.PackageError as error:
        check(error.code == 'codec' and not published, 'late invalid identity publishes no aggregate')
        check(error.part.endswith('registry2.xml'), 'invalid identity keeps final owner location')

    markers = []
    for mode in ('cancel', 'clock'):
        for label, boundary in (('early', 1), ('middle', max(2, publication['before_publish']//2)),
                                ('prepublication', publication['before_publish']+1)):
            destination.write_bytes(SENTINEL)
            state = {'polls': 0}
            marker = RuntimeError(mode+'-'+label)
            deadline = time.monotonic()+max(0.05, duration*2)

            def interrupt():
                """Expire a real host clock or return the exact caller error at a measured live boundary."""
                state['polls'] += 1
                if state['polls'] == boundary:
                    if mode == 'clock':
                        time.sleep(max(0, deadline-time.monotonic())+0.002)
                        return False
                    raise marker
                return False

            candidate = None
            try:
                bounded = module.PackageBudget(limits(is_cancelled=interrupt, deadline=deadline if mode == 'clock' else None))
                candidate = operation(bounded)
                raise AssertionError('large interruption succeeded: '+mode+'-'+label)
            except Exception as error:
                check(error is marker if mode == 'cancel' else isinstance(error, module.PackageError) and error.code == 'deadline_exceeded',
                      'original cancellation or native host deadline '+mode+'-'+label)
                check(candidate is None and destination.read_bytes() == SENTINEL, 'interruption publishes no aggregate/file '+mode+'-'+label)
            markers.append({'mode': mode, 'boundary': label, 'target_poll': boundary, 'actual_polls': state['polls']})
            print('Python interruption:', markers[-1], flush=True)
    recovered = operation(module.PackageBudget(limits()))
    check(len(recovered.value.registries) == inventory['registries'], 'fresh-call recovery')
    assert_output(destination, inventory['entries_per_registry'])
    return {'checks': len(checks), 'check_names': checks, 'baseline_seconds': duration,
            'baseline_used': measured, 'baseline_polls': polls, 'bounds': final_reference_counts,
            'interruptions': markers, 'publication':publication_checks, 'public_deadline': 'native monotonic deadline'}


def main():
    """Generate an immutable SDK unit, execute its owning gate and independently inspect publication."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', choices=('cpp', 'python', 'c', 'rust'), required=True)
    parser.add_argument('--entries-per-registry', type=int, default=25000)
    parser.add_argument('--provider-prefix', type=Path)
    parser.add_argument('--runtime-build', type=Path, default=ROOT/'out/build/ReleaseWindows')
    parser.add_argument('--cc', default='cl' if os.name == 'nt' else 'cc')
    parser.add_argument('--cxx', default='cl' if os.name == 'nt' else 'c++')
    parser.add_argument('--cargo', default='cargo')
    parser.add_argument('--cargo-home', type=Path)
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(strict=True), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    (build/'verification.json').unlink(missing_ok=True)
    inventory = fixtures(build/'fixtures', args.entries_per_registry)
    env = os.environ.copy()
    prefix = args.provider_prefix.resolve() if args.provider_prefix else None
    if prefix and os.name == 'nt':
        env['PATH'] = str(prefix/'bin')+os.pathsep+env.get('PATH', '')
    if args.cargo_home:
        env['CARGO_HOME'] = str(args.cargo_home.resolve())
    env['CARGO_NET_OFFLINE'] = 'true'
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(0x8003)
        os.environ['PATH'] = env['PATH']
    commands, started = [], time.monotonic()

    def run(command, name):
        """Retain real tool output and never report an unsuccessful subprocess as qualification."""
        argv = [str(item) for item in command]
        result = subprocess.run(argv, cwd=build, env=env, capture_output=True, text=True, encoding='utf-8', timeout=1800)
        (build/(name+'.log')).write_text(result.stdout+result.stderr, encoding='utf-8')
        commands.append({'argv': argv, 'returncode': result.returncode})
        (build/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        if result.returncode:
            print(result.stdout+result.stderr); result.check_returncode()
        return result.stdout

    suffix = {'cpp':'hpp', 'python':'py', 'c':'h', 'rust':'rs'}[args.language]
    source = build/('identity.'+suffix)
    run([compiler, '--input', ROOT/'test/resources/opc_identity.serializer', '--config',
         ROOT/'test/resources/opc_identity.ini', '--language', args.language, '--output', source], 'generation')
    if args.language == 'python':
        counts = python_gate(source, build/'fixtures', inventory)
        tool = sys.version
    elif args.language == 'rust':
        shutil.copyfile(ROOT/'test/resources/package_large_operation_main.rs', build/'main.rs')
        for name in ('Cargo.toml', 'Cargo.lock'):
            shutil.copyfile(ROOT/'example/xml/rust'/name, build/name)
        tool = run([args.cargo, '--version'], 'tool-version')
        output = run([args.cargo, 'run', '--locked', '--offline', '--release', '--manifest-path', build/'Cargo.toml',
                      '--', build/'fixtures', inventory['entries_per_registry'], inventory['identity_tuples']+inventory['relationships']], 'sdk')
        counts = json.loads([line for line in output.splitlines() if line.startswith('{')][-1])
    else:
        cpp = args.language == 'cpp'
        resource = ROOT/'test/resources'/('package_large_operation_main.cpp' if cpp else 'package_large_operation_main.c')
        driver = build/resource.name; shutil.copyfile(resource, driver)
        selected = args.cxx if cpp else args.cc
        executable = build/('consumer.exe' if os.name == 'nt' else 'consumer')
        if os.name == 'nt':
            command = [selected, '/nologo', '/std:c++20' if cpp else '/std:c11', '/W4', '/WX', '/O2', '/utf-8',
                       '/D_CRT_SECURE_NO_WARNINGS', '/DSERIALIZER_COMPRESSION_ZLIB=1', driver, '/Fe'+str(executable), '/Fo'+str(build/'main.obj')]
            if cpp:
                command += ['/EHsc', '/MD', '/I'+str(ROOT/'include'), '/I'+str(args.runtime_build.resolve()/'generated/include')]
            if prefix:
                command += ['/I'+str(prefix/'include'), '/I'+str(prefix/'include/libxml2')]
            command += ['/link']
            if cpp:
                command += [args.runtime_build.resolve()/'serializer_runtime.lib']
            if prefix:
                command += ['/LIBPATH:'+str(prefix/'lib')]
            command += ['libxml2.lib', 'z.lib'] + (['zstd.lib', 'lz4.lib'] if cpp else [])
        else:
            command = [selected, '-std=c++20' if cpp else '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2', driver, '-o', executable]
            if cpp:
                command += ['-I'+str(ROOT/'include'), '-I'+str(args.runtime_build.resolve()/'generated/include'), args.runtime_build.resolve()/'libserializer_runtime.a']
            command += subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libxml-2.0'], text=True).split()+['-lz', '-lm', '-pthread']
        tool = selected
        run(command, 'compile')
        output = run([executable, build/'fixtures', inventory['entries_per_registry'], inventory['identity_tuples']+inventory['relationships']], 'sdk')
        counts = json.loads([line for line in output.splitlines() if line.startswith('{')][-1])
    assert_output(build/'fixtures/produced.opc', inventory['entries_per_registry'])
    report = {'status':'passed', 'language':args.language, 'sdk':tool, 'inventory':inventory, 'controls':counts,
              'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(), 'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
              'elapsed_seconds':round(time.monotonic()-started,3), 'commands':commands}
    (build/'verification.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
