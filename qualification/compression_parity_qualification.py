# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Require all eleven selected compression SDK profiles and their shared model/frame inventory."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ROOT/'qualification/compression_parity'
sys.path.insert(0, str(ROOT/'test'))
from java_compression_test import fixtures as core_fixtures
from csharp_compression_test import extra_fixtures
from java_compression_optional_test import fixtures as optional_fixtures
from go_compression_optional_test import native_fixtures
from compression_window_fixtures import small_zlib_windows

FORMATS = ('NONE', 'GZIP', 'ZLIB', 'DEFLATE', 'ZSTD', 'LZ4')
PROTOCOLS = ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')
LANGUAGES = ('cpp', 'c', 'python', 'java', 'kotlin', 'csharp', 'go', 'javascript', 'typescript', 'rust', 'swift')
MAX_BYTES = 8*1024*1024


def sha(path):
    """Identify exact inputs, emitted SDKs and outputs without embedding payloads in reports."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(command, directory, environment, log, timeout=300):
    """Execute explicit SDK commands without shell expansion or successful missing-job skips."""
    command = list(map(str, command))
    print('RUN', subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=directory, env=environment, capture_output=True,
                            text=True, encoding='utf-8', errors='replace', timeout=timeout)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(json.dumps(command)+'\n'+result.stdout+result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{log}: exit {result.returncode}\n{result.stdout}{result.stderr}')
    return result.stdout.strip()


def required_path(config, name, *, directory=False):
    """Reject every absent explicitly selected SDK/provider instead of shrinking the inventory."""
    spelling = config[name]
    path = Path(os.path.expandvars(spelling)).resolve()
    if not (path.is_dir() if directory else path.is_file()):
        raise ValueError('Missing required path '+name+': '+str(path))
    return path


def tools(config):
    """Resolve all mandatory host tools before generating models or consuming fixtures."""
    result = {}
    for name in ('cmake', 'ninja', 'cc', 'cxx', 'java', 'javac', 'kotlinc', 'dotnet', 'node', 'tsc', 'go', 'rustc', 'cargo', 'swiftc'):
        spelling = os.path.expandvars(config['tools'][name])
        found = str(Path(spelling).resolve()) if name == 'tsc' and Path(spelling).is_file() else shutil.which(spelling)
        if not found:
            raise ValueError('Missing required SDK '+name+': '+spelling)
        result[name] = str(Path(found).resolve())
    return result


def sdk_versions(build,sdk,environment):
    """Retain actual selected compiler/runtime versions without installing or probing replacement SDKs."""
    commands = {'java':[sdk['java'],'-version'],'javac':[sdk['javac'],'-version'],
                'kotlin':[sdk['kotlinc'],'-version'],'dotnet':[sdk['dotnet'],'--version'],
                'dotnet_runtimes':[sdk['dotnet'],'--list-runtimes'],'node':[sdk['node'],'--version'],
                'typescript':[sdk['node'],sdk['tsc'],'--version'],'go':[sdk['go'],'version'],
                'rustc':[sdk['rustc'],'--version'],'cargo':[sdk['cargo'],'--version'],
                'swift':[sdk['swiftc'],'--version'],'cmake':[sdk['cmake'],'--version']}
    result = {'python':sys.version}
    for name,command in commands.items():
        directory = build/'csharp' if name.startswith('dotnet') else build
        value = run(command,directory,environment,build/'logs'/('version-'+name+'.log'))
        log = (build/'logs'/('version-'+name+'.log')).read_text(encoding='utf-8').splitlines()[1:]
        result[name] = '\n'.join(log).strip() if not value else value
    import zlib
    from compression import zstd
    import lz4
    result.update(zlib_compiled=zlib.ZLIB_VERSION,zlib_runtime=zlib.ZLIB_RUNTIME_VERSION,
                  python_lz4=lz4.__version__,native_lz4=lz4.library_version_string(),
                  native_zstd=getattr(zstd,'zstd_version',None))
    return result


def shared_corpus(directory, config, environment):
    """Materialize the existing common 974+317 independent inputs once for every consumer."""
    directory.mkdir(parents=True, exist_ok=True)
    core = directory/'core'; core.mkdir(exist_ok=True)
    count = core_fixtures(core)+extra_fixtures(core)
    if count != 974:
        raise AssertionError('Common core fixture inventory changed: '+str(count))
    optional = directory/'optional'; optional.mkdir(exist_ok=True)
    counts = optional_fixtures(optional, ('ZSTD', 'LZ4'))
    args = argparse.Namespace(cpp_helper=required_path(config, 'cpp_helper'),
                             cpp_linked_helper=required_path(config, 'cpp_linked_helper'))
    producers = native_fixtures(args, optional, environment, counts)
    if sum(counts.values()) != 317:
        raise AssertionError('Common optional fixture inventory changed: '+str(counts))
    rows = []
    valid_core = {}

    def add(case, format, source, plain, wanted, input=MAX_BYTES, output=MAX_BYTES, window=MAX_BYTES):
        """Record one exact normalized category and policy with corpus-relative paths."""
        rows.append((case, format, source.relative_to(directory).as_posix(),
                     plain.relative_to(directory).as_posix(), wanted, input, output, window))

    # Every valid core input also receives exact and one-too-tight byte-policy controls.
    for index, line in enumerate((core/'fixtures.tsv').read_text(encoding='utf-8').splitlines()):
        format, status, source, plain = line.split('\t')
        source = core/source
        if status == 'invalid':
            add('core-'+str(index), format, source, source, 'invalid_data')
        else:
            plain = core/plain
            valid_core.setdefault(format,(source,plain))
            add('core-'+str(index), format, source, plain, 'ok', source.stat().st_size, plain.stat().st_size)
            if source.stat().st_size:
                add('core-input-tight-'+str(index), format, source, plain, 'resource_limit', source.stat().st_size-1)
            if plain.stat().st_size:
                add('core-output-tight-'+str(index), format, source, plain, 'resource_limit', output=plain.stat().st_size-1)
    for format in ('ZSTD', 'LZ4'):
        for index, line in enumerate((optional/(format+'.tsv')).read_text(encoding='utf-8').splitlines()):
            source, plain, status, input, output, window = line.split('\t')
            add(format.lower()+'-'+str(index), format, optional/source, optional/plain,
                status.lower(), int(input), int(output), int(window))
    # Identity is an owned byte copy with the same exact/one-tight byte caps.
    identity = core/'identity.bin'; identity.write_bytes(b'identity\x00\xff')
    add('identity-exact', 'NONE', identity, identity, 'ok', identity.stat().st_size, identity.stat().st_size)
    add('identity-input-tight', 'NONE', identity, identity, 'resource_limit', identity.stat().st_size-1)
    add('identity-output-tight', 'NONE', identity, identity, 'resource_limit', output=identity.stat().st_size-1)
    for format in ('GZIP','ZLIB','DEFLATE'):
        source,plain = valid_core[format]
        add('history-budget-'+format+'-exact',format,source,plain,'ok',window=32768)
        add('history-budget-'+format+'-tight',format,source,plain,'resource_limit',window=32767)
    windows = directory/'windows'; windows.mkdir(exist_ok=True)
    window_rows = small_zlib_windows(windows)
    if len(window_rows)!=64 or len({row[0] for row in window_rows})!=44:
        raise AssertionError('Mandatory advertised-window grammar inventory changed')
    for index, (source, plain, wanted, input, output, window) in enumerate(window_rows):
        add('advertised-history-'+str(index), 'ZLIB', windows/source, windows/plain, wanted, input, output, window)
    (directory/'cases.tsv').write_text(''.join('\t'.join(map(str,row))+'\n' for row in rows), encoding='utf-8')
    return {'core_inputs': count, 'optional_inputs': counts, 'advertised_history_frames':44,
            'advertised_history_decisions':64,'core_window_policy_decisions':6,
            'decisions': len(rows), 'cpp_producers': producers}


def prepare(language, directory, compiler, sdk, config, environment, jobs=2):
    """Build one real emitted owning model and its thin explicit-provider adapter with the actual SDK."""
    directory.mkdir(parents=True, exist_ok=True)
    logs = directory/'logs'; logs.mkdir(exist_ok=True)
    command = [compiler, '--input', SOURCES/'model.serializer']

    def execute(command, name):
        """Retain each generation/build command beside its immutable generated SDK."""
        return run(command, directory, environment, logs/(name+'.log'))

    if language == 'cpp':
        prefixes = [required_path(config, 'runtime_prefix', directory=True), required_path(config, 'dependency_prefix', directory=True)]
        execute([sdk['cmake'], '-S', SOURCES, '-B', directory/'build', '-G', 'Ninja',
                 '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_MAKE_PROGRAM='+sdk['ninja'], '-DCMAKE_CXX_COMPILER='+sdk['cxx'],
                 '-DCMAKE_PREFIX_PATH='+';'.join(map(str,prefixes)), '-DSerializer_DIR='+str(prefixes[0]/'lib/cmake/Serializer'),
                 '-DSERIALIZER_GENERATOR_EXECUTABLE='+str(compiler)], 'configure')
        execute([sdk['cmake'], '--build', directory/'build', '--target', 'compression_parity_cpp', '--parallel', str(jobs)], 'build')
        return [directory/'build'/('compression_parity_cpp.exe' if os.name == 'nt' else 'compression_parity_cpp')]
    if language == 'c':
        execute(command+['--language', 'c', '--output', directory/'schema.h'], 'generate')
        shutil.copyfile(SOURCES/'adapter_c.c', directory/'main.c')
        program = directory/('adapter.exe' if os.name == 'nt' else 'adapter')
        include, library = (required_path(config, name, directory=True) for name in ('provider_include', 'provider_library'))
        msvc = Path(sdk['cc']).name.lower() in ('cl', 'cl.exe')
        if msvc:
            arguments = [sdk['cc'], '/nologo', '/MD', '/std:c11', '/W4', '/WX', '/O2', '/utf-8', '/D_CRT_SECURE_NO_WARNINGS',
                         '/I'+str(include), directory/'main.c', '/Fe:'+str(program), '/Fo:'+str(directory/'main.obj')]
        else:
            arguments = [sdk['cc'], '-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I'+str(include), directory/'main.c', '-o', program]
        for provider in ('ZLIB', 'ZSTD', 'LZ4'):
            arguments += [('/D' if msvc else '-D')+'SERIALIZER_COMPRESSION_'+provider+'=1',
                          library/(('z' if provider == 'ZLIB' else provider.lower())+'.lib') if msvc else required_path(config,provider.lower()+'_library')]
        execute(arguments, 'build')
        return [program]
    if language == 'python':
        execute(command+['--language', 'python', '--python.output', directory/'schema.py'], 'generate')
        shutil.copyfile(SOURCES/'adapter_python.py', directory/'adapter.py')
        return [sys.executable, '-B', directory/'adapter.py']
    if language == 'java':
        execute(command+['--language', 'java', '--output', directory/'Schema.java', '--java.package', 'compression.parity'], 'generate')
        shutil.copyfile(SOURCES/'Adapter.java', directory/'Adapter.java')
        execute([sdk['javac'], '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d', directory/'classes', directory/'Schema.java', directory/'Adapter.java'], 'build')
        classpath = os.pathsep.join(map(str, (directory/'classes', required_path(config, 'zstd_jar'), required_path(config, 'lz4_jar'))))
        return [sdk['java'], '-cp', classpath, 'compression.parity.Adapter']
    if language == 'kotlin':
        execute(command+['--language', 'kotlin', '--kotlin.output', directory/'Schema.kt'], 'generate')
        shutil.copyfile(SOURCES/'adapter_kotlin.kt', directory/'Adapter.kt')
        execute([sdk['kotlinc'], '-Werror', '-jvm-target', '17', '-include-runtime', directory/'Schema.kt', directory/'Adapter.kt', '-d', directory/'app.jar'], 'build')
        classpath = os.pathsep.join(map(str, (directory/'app.jar', required_path(config, 'zstd_jar'), required_path(config, 'lz4_jar'))))
        return [sdk['java'], '-cp', classpath, 'AdapterKt']
    if language == 'csharp':
        execute(command+['--language', 'csharp', '--output', directory/'Schema.cs', '--csharp.namespace', 'CompressionParity'], 'generate')
        shutil.copyfile(SOURCES/'adapter_csharp.cs', directory/'Program.cs')
        (directory/'global.json').write_text(json.dumps({'sdk': {'version': config['dotnet_sdk'], 'rollForward': 'disable'}}), encoding='utf-8')
        (directory/'NuGet.Config').write_text('<configuration><packageSources><clear /></packageSources></configuration>', encoding='utf-8')
        (directory/'Adapter.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><TargetFramework>net8.0</TargetFramework><OutputType>Exe</OutputType><Nullable>enable</Nullable><TreatWarningsAsErrors>true</TreatWarningsAsErrors><UseSharedCompilation>false</UseSharedCompilation><NuGetAudit>false</NuGetAudit></PropertyGroup></Project>', encoding='utf-8')
        execute([sdk['dotnet'], 'build', 'Adapter.csproj', '-c', 'Release', '--configfile', 'NuGet.Config', '--nologo'], 'build')
        return [sdk['dotnet'], directory/'bin/Release/net8.0/Adapter.dll']
    if language in ('javascript', 'typescript'):
        execute(command+['--language', 'js,typescript', '--js.output', directory/'schema.mjs', '--typescript.output', directory/'schema.d.mts'], 'generate')
        shutil.copyfile(SOURCES/'adapter_javascript.mjs', directory/'adapter.mjs')
        if language == 'typescript':
            # The strict SDK verifies the real model/helper declarations before the shared JS implementation runs.
            (directory/'contract.mts').write_text("import {AccountRecord,Protocol,SerializerCompression as C} from './schema.mjs';\nconst model=new AccountRecord();const ordinary:Uint8Array=model.encode(Protocol.JSON);\nconst provider:Promise<C.ZstdProvider>=C.zstdProvider();void provider;void ordinary;\n", encoding='utf-8')
            execute([sdk['node'], sdk['tsc'], '--strict', '--noEmit', '--target', 'ES2022', '--module', 'NodeNext', '--moduleResolution', 'NodeNext', '--lib', 'ES2022,DOM', directory/'contract.mts'], 'strict-typescript')
        return [sdk['node'], directory/'adapter.mjs']
    if language == 'go':
        execute(command+['--language', 'go', '--go.output', directory/'schema.go', '--go.package', 'main'], 'generate')
        shutil.copyfile(SOURCES/'adapter_go.go', directory/'main.go')
        for name in ('go.mod', 'go.sum', 'providers.go'):
            shutil.copyfile(ROOT/'qualification/go_compression_providers'/name, directory/name)
        program = directory/('adapter.exe' if os.name == 'nt' else 'adapter')
        execute([sdk['go'], 'build', '-trimpath', '-mod=readonly', '-o', program, '.'], 'build')
        return [program]
    if language == 'rust':
        source = directory/'src'; source.mkdir(exist_ok=True)
        execute(command+['--language', 'rust', '--rust.output', source/'schema.rs'], 'generate')
        shutil.copyfile(SOURCES/'adapter_rust.rs', source/'main.rs')
        library = (ROOT/'qualification/rust_compression_providers').resolve().as_posix()
        (directory/'Cargo.toml').write_text('[package]\nname="serializer-compression-parity"\nversion="0.0.0"\nedition="2021"\npublish=false\n[dependencies]\nserializer-compression-providers={path='+json.dumps(library)+',default-features=false,features=["zstd","lz4"]}\n[profile.release]\noverflow-checks=true\n', encoding='utf-8')
        execute([sdk['cargo'], 'generate-lockfile', '--offline'], 'lock')
        from rust_compression_optional_test import registry_packages
        if registry_packages(directory/'Cargo.lock') != registry_packages(ROOT/'qualification/rust_compression_providers/Cargo.lock'):
            raise AssertionError('Rust provider registry lock changed')
        execute([sdk['cargo'], 'build', '--release', '--offline', '--locked'], 'build')
        return [directory/'target/release'/('serializer-compression-parity.exe' if os.name == 'nt' else 'serializer-compression-parity')]
    if language == 'swift':
        execute(command+['--language', 'swift', '--swift.output', directory/'Schema.swift'], 'generate')
        shutil.copyfile(SOURCES/'adapter_swift.swift', directory/'main.swift')
        from swift_compression_optional_test import compile_adapter, compile_consumer
        args = argparse.Namespace(cc=sdk['cc'], swiftc=sdk['swiftc'], provider_include=required_path(config, 'provider_include', directory=True),
                                  zstd_library=required_path(config, 'zstd_library'), lz4_library=required_path(config, 'lz4_library'))
        provider, obj = compile_adapter(args, directory, environment)
        (directory/'adapter.swift').write_text((ROOT/'qualification/swift_compression_providers/adapter.swift').read_text(encoding='utf-8').replace('__COMPRESSION_NAME__', 'SerializerCompression'), encoding='utf-8')
        return [compile_consumer(args, directory, provider, obj, environment)]
    raise ValueError('Undeclared language '+language)


def lifecycle_checks(build, compiler, sdk, config, environment, jobs=2):
    """Replay maintained actual-SDK cancellation, failure-fencing, ownership and deployment controls."""
    native = ['--cpp-helper', required_path(config,'cpp_helper'), '--cpp-linked-helper', required_path(config,'cpp_linked_helper'),
              '--provider-path', required_path(config,'python_provider',directory=True)]
    cases = []

    def add(language, script, extra=(), profile='core'):
        """Give each maintained runner an isolated output directory and explicit SDK/provider selection."""
        case = build/'lifecycle'/language/profile
        command = [sys.executable,'-B',ROOT/'test'/script,'--compiler',compiler,'--build',case,*extra]
        cases.append((language+'-'+profile,case,command))

    add('python','python_compression_test.py',['--provider-path',required_path(config,'python_provider',directory=True)])
    add('java','java_compression_test.py',['--java',sdk['java'],'--javac',sdk['javac']])
    add('java','java_compression_optional_test.py',['--java',sdk['java'],'--javac',sdk['javac'],
        '--provider-jar',required_path(config,'zstd_jar'),'--provider-jar',required_path(config,'lz4_jar'),
        '--cpp-helper',required_path(config,'cpp_helper'),'--provider-path',required_path(config,'python_provider',directory=True)],'optional')
    add('kotlin','kotlin_compression_test.py',['--java',sdk['java'],'--kotlinc',sdk['kotlinc']])
    add('kotlin','kotlin_compression_optional_test.py',['--java',sdk['java'],'--kotlinc',sdk['kotlinc'],
        '--zstd-jar',required_path(config,'zstd_jar'),'--lz4-jar',required_path(config,'lz4_jar'),*native],'optional')
    add('csharp','csharp_compression_test.py',['--dotnet',sdk['dotnet']])
    add('csharp','csharp_compression_optional_test.py',['--dotnet',sdk['dotnet'],
        '--zstd-dll',required_path(config,'zstd_dll'),'--lz4-dll',required_path(config,'lz4_dll'),*native],'optional')
    add('go','go_compression_test.py',['--go',sdk['go']])
    add('go','go_compression_optional_test.py',['--go',sdk['go'],'--module-cache',required_path(config,'go_cache',directory=True),*native],'optional')
    add('javascript-typescript','javascript_compression_test.py',['--node',sdk['node'],'--tsc',sdk['tsc']])
    add('javascript-typescript','javascript_compression_optional_test.py',['--node',sdk['node'],'--tsc',sdk['tsc'],
        '--lz4-module',required_path(config,'lz4_module'),*native],'optional')
    add('rust','rust_compression_test.py',['--rustc',sdk['rustc']])
    add('rust','rust_compression_optional_test.py',['--rustc',sdk['rustc'],'--cargo',sdk['cargo'],
        '--cargo-home',required_path(config,'cargo_home',directory=True),*native],'optional')
    add('swift','swift_compression_test.py',['--swiftc',sdk['swiftc']])
    add('swift','swift_compression_optional_test.py',['--swiftc',sdk['swiftc'],'--cc',sdk['cc'],
        '--provider-include',required_path(config,'provider_include',directory=True),
        '--zstd-library',required_path(config,'zstd_library'),'--lz4-library',required_path(config,'lz4_library'),
        '--provider-runtime-directory',required_path(config,'dependency_prefix',directory=True)/'bin',*native],'optional')
    add('c','c_compression_test.py',['--cc',sdk['cc'],'--zlib-include',required_path(config,'provider_include',directory=True),
        '--zlib-library',required_path(config,'zlib_library')])
    c_library = ['--library-directory',required_path(config,'provider_library',directory=True)] if Path(sdk['cc']).name.lower() in ('cl','cl.exe') else []
    add('c','c_compression_optional_test.py',['--cc',sdk['cc'],'--include',required_path(config,'provider_include',directory=True),
        *c_library,*native],'optional')
    results = {}; failures = {}

    def execute(job):
        """Keep each maintained runner's complete diagnostics and reports separate from cross-language adapters."""
        name, case, command = job
        case.mkdir(parents=True,exist_ok=True)
        text = run(command, ROOT, environment, build/'logs'/('lifecycle-'+name+'.log'),timeout=900)
        return name, {'command':list(map(str,command)),'runner_sha256':sha(command[2]),
                      'stdout_tail':text.splitlines()[-4:],'report_sha256':{str(path.relative_to(case)).replace('\\','/'):sha(path)
                      for path in sorted(case.rglob('verification.json'))}}

    # These independent SDK builds own disjoint outputs; cap host contention while native qualification runs.
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        futures = {executor.submit(execute,job):job[0] for job in cases}
        for future in as_completed(futures):
            name = futures[future]
            try:
                name, result = future.result(); results[name] = result
            except (RuntimeError,subprocess.TimeoutExpired) as failure:
                failures[name] = str(failure)
    return results, failures


def main():
    """Build every selected SDK, require a complete all-producer inventory and retain exact provenance."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--config', required=True, type=Path, help='Explicit local SDK/provider deployment paths; see configuration documentation')
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--jobs', type=int, choices=(1, 2), default=2,
                        help='Maximum native build and independent lifecycle workers; use 1 on a shared host')
    parser.add_argument('--prepare-only', action='store_true', help='Diagnostic build phase only; never writes a successful qualification report')
    parser.add_argument('--languages', nargs='+', choices=LANGUAGES, help='Diagnostic subset only; final qualification always requires the full manifest')
    parser.add_argument('--skip-lifecycle',action='store_true',help='Diagnostic framing/exchange pass only; never writes a successful full qualification report')
    args = parser.parse_args()
    build = args.build.resolve(); output = args.output.resolve(); compiler = args.compiler.resolve()
    if build == ROOT or build.is_relative_to(ROOT) and build.relative_to(ROOT).parts[0] != 'out':
        parser.error('In-repository output must stay beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((SOURCES/'manifest.json').read_text(encoding='utf-8'))
    if tuple(manifest['languages']) != LANGUAGES or tuple(manifest['formats']) != FORMATS or tuple(manifest['protocols']) != PROTOCOLS or manifest['expected_model_exchanges'] != 2904:
        raise ValueError('Unknown or incomplete mandatory parity inventory')
    config = json.loads(args.config.read_text(encoding='utf-8-sig')); sdk = tools(config)
    environment = os.environ.copy(); environment.update({key: os.path.expandvars(value) for key,value in config.get('environment',{}).items()})
    environment['PATH'] = os.pathsep.join(str(required_path(config,name,directory=True)/'bin') for name in ('runtime_prefix','dependency_prefix'))+os.pathsep+environment.get('PATH','')
    for name in ('LIBRARY_PATH','LD_LIBRARY_PATH'):
        environment[name] = str(required_path(config,'provider_library',directory=True))+os.pathsep+environment.get(name,'')
    environment.update(CARGO_HOME=str(required_path(config, 'cargo_home', directory=True)), GOMODCACHE=str(required_path(config, 'go_cache', directory=True)), GOCACHE=str(build/'go-build-cache'),
                       GOPROXY='off', GOSUMDB='off', RUSTFLAGS='-D warnings', PYTHONDONTWRITEBYTECODE='1', DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1', DOTNET_CLI_TELEMETRY_OPTOUT='1',
                       DOTNET_CLI_HOME=str(build/'dotnet-home'), DOTNET_GENERATE_ASPNET_CERTIFICATE='false', DOTNET_ADD_GLOBAL_TOOLS_TO_PATH='false')
    provider_path = required_path(config, 'python_provider', directory=True)
    sys.path.insert(0, str(provider_path)); environment['PYTHONPATH'] = str(provider_path)+os.pathsep+environment.get('PYTHONPATH','')
    temporary = build/'temporary'; temporary.mkdir(exist_ok=True)
    environment.update(TMP=str(temporary), TEMP=str(temporary), TMPDIR=str(temporary))
    compiler_hash = sha(compiler)
    runners = {}
    for language in args.languages or LANGUAGES:
        runners[language] = prepare(language, build/language, compiler, sdk, config, environment, args.jobs)
    if args.prepare_only or args.languages:
        print('Diagnostic SDK preparation complete; no full parity qualification asserted.'); return
    versions = sdk_versions(build,sdk,environment)
    corpus = build/'corpus'; counts = shared_corpus(corpus, config, environment)
    results = {}

    def execute(language, mode):
        """Require one adapter's structured exact inventory count rather than process success alone."""
        extra = []
        if language == 'csharp': extra = [required_path(config, 'zstd_dll'), required_path(config, 'lz4_dll')]
        if language in ('javascript','typescript'): extra = [required_path(config, 'lz4_module'), language]
        text = run(runners[language]+[mode,corpus]+extra, build/language, environment, build/'logs'/(language+'-'+mode+'.log'))
        return json.loads(text.splitlines()[-1])

    for language in LANGUAGES:
        if execute(language, 'produce') != {'produced':24,'encode_bounds':72}:
            raise AssertionError('Incomplete mandatory producer '+language)
    jobs = []; independent = 0
    from compression import zstd
    import lz4.frame
    import zlib
    for language in LANGUAGES:
        for protocol in PROTOCOLS:
            for format in FORMATS:
                stem = language+'-'+protocol+'-'+format
                frame = (corpus/(stem+'.frame')).read_bytes(); plain = (corpus/(stem+'.plain')).read_bytes()
                if format == 'NONE': actual = frame
                else:
                    decoder = zstd.ZstdDecompressor() if format == 'ZSTD' else lz4.frame.LZ4FrameDecompressor() if format == 'LZ4' else zlib.decompressobj({'GZIP':31,'ZLIB':15,'DEFLATE':-15}[format])
                    actual = decoder.decompress(frame)
                    if not decoder.eof or decoder.unused_data:
                        raise AssertionError('Independent exact frame boundary differs: '+stem)
                if actual != plain: raise AssertionError('Native bytes changed in producer '+stem)
                independent += 1; jobs.append((stem,protocol,format,stem+'.frame',stem+'.plain'))
    (corpus/'exchange.tsv').write_text(''.join('\t'.join(row)+'\n' for row in jobs), encoding='utf-8')
    failures = {}
    for language in LANGUAGES:
        try:
            result = execute(language, 'check')
            if result != {'decisions':counts['decisions'],'exchanges':len(jobs)}:
                raise AssertionError('Incomplete mandatory consumer '+language+': '+str(result))
            results[language] = result
        except (RuntimeError, AssertionError) as failure:
            failures[language] = str(failure)
    for mode, expected in (('controls',6),('candidate-controls',6),('reentry-controls',1),('source-controls',6),('input-finish-controls',6)):
        try:
            control_output = run(runners['cpp']+[mode,corpus], build/'cpp', environment, build/('logs/cpp-'+mode+'.log'))
            if json.loads(control_output.splitlines()[-1]) != {'controls':expected}:
                raise AssertionError('Incomplete C++ '+mode)
        except (RuntimeError, AssertionError) as failure:
            failures['cpp-'+mode] = str(failure)
    lifecycle = {}
    if not args.skip_lifecycle:
        lifecycle, lifecycle_failures = lifecycle_checks(build,compiler,sdk,config,environment,args.jobs)
        failures.update(lifecycle_failures)
    else:
        failures['lifecycle'] = 'Diagnostic invocation intentionally omitted mandatory maintained lifecycle/deployment runners'
    if failures:
        (build/'failed-decisions.json').write_text(json.dumps({'compiler_sha256':compiler_hash,'fixture_counts':counts,'passed_consumers':results,'lifecycle':lifecycle,'failures':failures},indent=2)+'\n', encoding='utf-8')
        raise AssertionError('Mandatory parity decisions failed: '+', '.join(failures))
    if sha(compiler) != compiler_hash: raise AssertionError('Compiler changed during qualification')
    if sum(value['exchanges'] for value in results.values()) != manifest['expected_model_exchanges']:
        raise AssertionError('Mandatory all-producer/all-consumer jobs differ')
    runtime_prefix = required_path(config,'runtime_prefix',directory=True)
    report = {'qualification':manifest['id'],'utc':datetime.now(timezone.utc).isoformat(),'platform':platform.platform(),
              'manifest':manifest,'compiler_sha256':compiler_hash,'configuration_sha256':sha(args.config),
              'sdk_versions':versions,'sdk_paths':sdk,'maximum_workers':args.jobs,
              'fixture_counts':counts,'results':results,'model_exchanges':2904,'independent_producer_frames':independent,
              'encode_bound_decisions':72*len(LANGUAGES),
              'cpp_lifecycle_controls':25,'maintained_lifecycle_runners':lifecycle,
              'common_corpus_sha256':sha(corpus/'cases.tsv'),'exchange_sha256':sha(corpus/'exchange.tsv'),
              'source_sha256':{str(path.relative_to(ROOT)).replace('\\','/'):sha(path) for path in sorted(SOURCES.iterdir()) if path.is_file()},
              'input_sha256':{str(path.relative_to(corpus)).replace('\\','/'):sha(path) for path in sorted(corpus.rglob('*')) if path.is_file()},
              'sdk_commands':{language:list(map(str,command)) for language,command in runners.items()},
              'provider_sha256':{name:sha(required_path(config,name)) for name in
                                 ('zstd_jar','lz4_jar','zstd_dll','lz4_dll','lz4_module','zlib_library','zstd_library','lz4_library','cpp_helper','cpp_linked_helper')},
              'runtime_prefix_sha256':{str(path.relative_to(runtime_prefix)).replace('\\','/'):sha(path)
                                      for path in sorted(runtime_prefix.rglob('*')) if path.is_file()},
              'generated_sha256':{str(path.relative_to(build)).replace('\\','/'):sha(path)
                                  for language in LANGUAGES for path in (build/language).rglob('*') if path.is_file()
                                  and path.name in ('schema.h','schema.py','Schema.java','Schema.kt','Schema.cs','schema.go','schema.mjs','schema.d.mts','schema.rs','Schema.swift','model.hpp')},
              'limitations':['Selected host/provider profiles only; SDK/platform matrix is recorded separately.',
                             'Native owning-model and whole-compression operation boundaries; no nested XML/OPC context or performance claim.',
                             'Byte/history limits are logical admission limits, not a physical heap/RSS cap.']}
    report['source_sha256'][str(Path(__file__).relative_to(ROOT)).replace('\\','/')] = sha(__file__)
    for name in ('java_compression_test.py','csharp_compression_test.py','java_compression_optional_test.py','go_compression_optional_test.py','compression_window_fixtures.py'):
        report['source_sha256']['test/'+name] = sha(ROOT/'test'/name)
    output.parent.mkdir(parents=True, exist_ok=True); output.write_text(json.dumps(report,indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'model_exchanges':2904,'shared_decisions':counts['decisions']*len(LANGUAGES),'independent_frames':independent}))


if __name__ == '__main__':
    main()
