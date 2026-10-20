# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Requalify the isolated C++ window repair while requiring the complete unchanged SDK baseline."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import sys

import compression_parity_qualification as parity

GENERATED = {
    'c': ('schema.h', ['--language','c','--output','schema.h']),
    'python': ('schema.py', ['--language','python','--python.output','schema.py']),
    'java': ('Schema.java', ['--language','java','--output','Schema.java','--java.package','compression.parity']),
    'kotlin': ('Schema.kt', ['--language','kotlin','--kotlin.output','Schema.kt']),
    'csharp': ('Schema.cs', ['--language','csharp','--output','Schema.cs','--csharp.namespace','CompressionParity']),
    'go': ('schema.go', ['--language','go','--go.output','schema.go','--go.package','main']),
    'javascript': ('schema.mjs', ['--language','js,typescript','--js.output','schema.mjs','--typescript.output','schema.d.mts']),
    'typescript': ('schema.mjs', ['--language','js,typescript','--js.output','schema.mjs','--typescript.output','schema.d.mts']),
    'rust': ('src/schema.rs', ['--language','rust','--rust.output','src/schema.rs']),
    'swift': ('Schema.swift', ['--language','swift','--swift.output','Schema.swift']),
}


def inventory(directory):
    """Retain exact bytes for replayed inputs, generated sources and installed runtime overlays."""
    return {path.relative_to(directory).as_posix():parity.sha(path)
            for path in sorted(directory.rglob('*')) if path.is_file()}


def python_deployment(directory):
    """Identify provider sources/native binaries while excluding regenerable interpreter bytecode caches."""
    return {path.relative_to(directory).as_posix():parity.sha(path)
            for path in sorted(directory.rglob('*')) if path.is_file()
            and '__pycache__' not in path.parts and path.suffix not in ('.pyc','.pyo')}


def native_deployment(config, artifact, directories):
    """Bind every controlled Windows provider DLL search location to the immutable native capture."""
    if os.name!='nt':
        return {}
    captured = json.loads((artifact/'source-manifest.json').read_text(encoding='utf-8-sig'))
    hashes = {item['name']:item['sha256'] for item in captured['artifacts']}
    locations = [parity.required_path(config,'runtime_prefix',directory=True)/'bin',
                 parity.required_path(config,'dependency_prefix',directory=True)/'bin',*directories]
    observed = {}
    for name in ('z.dll','zstd.dll','lz4.dll'):
        expected = hashes[name]
        if parity.sha(artifact/name)!=expected:
            raise AssertionError('Captured provider DLL changed: '+name)
        for directory in locations:
            path = directory/name
            if path.exists():
                actual = parity.sha(path)
                if actual!=expected:
                    raise AssertionError('Conflicting reachable provider DLL: '+str(path))
                observed[str(path)] = actual
        for directory in locations[:2]:
            if not (directory/name).is_file():
                raise AssertionError('Required selected provider DLL is absent: '+str(directory/name))
    return observed


def production_inputs(path):
    """Require by-value compiler input manifests rather than assuming an unchanged emitter."""
    value = json.loads(path.read_text(encoding='utf-8-sig'))
    files = value['windows']['files']
    return {item['path']:item['sha256'] for item in files
            if item['path'].startswith(('src/','include/','cmake/')) or item['path']=='CMakeLists.txt'}


def structured_log(path, wanted):
    """Read the required structured result even when a native SDK also emits stderr warnings."""
    matches = []
    for line in path.read_text(encoding='utf-8').splitlines():
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(value,dict) and all(name in value for name in wanted):
            matches.append(value)
    if len(matches)!=1:
        raise AssertionError('Missing or ambiguous structured result: '+str(path))
    return matches[0]


def main():
    """Publish success only after the unchanged ten SDKs and the repaired C++ inventory all qualify."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path)
    parser.add_argument('--baseline-compiler',required=True,type=Path)
    parser.add_argument('--baseline-build',required=True,type=Path)
    parser.add_argument('--baseline-config',required=True,type=Path)
    parser.add_argument('--config',required=True,type=Path)
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--seal-only',action='store_true',help='Freeze the completed baseline before any runtime refresh; no success qualification is asserted')
    args = parser.parse_args()
    baseline = args.baseline_build.resolve(); build = args.build.resolve()
    compiler = args.compiler.resolve(); baseline_compiler = args.baseline_compiler.resolve()
    if build==baseline or build==parity.ROOT or build.is_relative_to(parity.ROOT) and build.relative_to(parity.ROOT).parts[0]!='out':
        parser.error('Use a separate ignored output directory')
    failure_path = baseline/'failed-decisions.json'
    previous = json.loads(failure_path.read_text(encoding='utf-8'))
    if set(previous['failures'])!={'cpp'} or 'zstd-123: ok != resource_limit' not in previous['failures']['cpp']:
        raise AssertionError('Baseline must contain exactly the independently reproduced C++ window failure')
    if previous['compiler_sha256']!=parity.sha(baseline_compiler):
        raise AssertionError('Baseline compiler identity changed')
    expected = {'decisions':1400,'exchanges':264}
    if set(previous['passed_consumers'])!=set(GENERATED) or any(result!=expected for result in previous['passed_consumers'].values()):
        raise AssertionError('All ten unchanged consumer profiles are mandatory')
    lifecycle = previous['lifecycle']
    lifecycle_languages = {'javascript-typescript' if language in ('javascript','typescript') else language
                           for language in parity.LANGUAGES if language!='cpp'}
    expected_suites = {language+'-core' for language in lifecycle_languages} | {
        language+'-optional' for language in lifecycle_languages if language!='python'}
    if set(lifecycle)!=expected_suites:
        raise AssertionError('All seventeen maintained SDK suites must have passed')
    for name,result in lifecycle.items():
        if parity.sha(Path(result['command'][2]))!=result['runner_sha256']:
            raise AssertionError('Maintained runner changed: '+name)
        language,profile = name.rsplit('-',1)
        case = baseline/'lifecycle'/language/profile
        for relative,digest in result['report_sha256'].items():
            if parity.sha(case/relative)!=digest:
                raise AssertionError('Maintained report changed: '+name+'/'+relative)
    for language in parity.LANGUAGES:
        if structured_log(baseline/'logs'/(language+'-produce.log'),('produced','encode_bounds'))!={'produced':24,'encode_bounds':72}:
            raise AssertionError('Incomplete baseline producer '+language)
    prior_config = json.loads(args.baseline_config.read_text(encoding='utf-8-sig'))
    qualification_sources = [path for path in parity.SOURCES.iterdir() if path.is_file()]
    qualification_sources.append(Path(parity.__file__))
    qualification_sources.extend((parity.ROOT/'test').glob('*.py'))
    qualification_sources.extend(path for path in (parity.ROOT/'test/resources').rglob('*') if path.is_file())
    for name in ('go','rust','swift'):
        qualification_sources.extend(path for path in (parity.ROOT/('qualification/'+name+'_compression_providers')).rglob('*')
                                     if path.is_file() and not {'target','__pycache__'}.intersection(path.parts))
    generated = {}
    for language,(filename,_) in GENERATED.items():
        names = (filename,'schema.d.mts') if language in ('javascript','typescript') else (filename,)
        generated.update({language+'/'+name:parity.sha(baseline/language/name) for name in names})
    generated.update({path.relative_to(baseline).as_posix():parity.sha(path)
                      for path in sorted((baseline/'cpp/build/generated').rglob('*.hpp'))})
    seal = {
        'compiler_sha256':parity.sha(baseline_compiler),'failure_report_sha256':parity.sha(failure_path),
        'configuration_sha256':parity.sha(args.baseline_config),
        'qualification_source_sha256':{path.relative_to(parity.ROOT).as_posix():parity.sha(path) for path in qualification_sources},
        'input_sha256':inventory(baseline/'corpus'),'generated_source_sha256':generated,
        'logs_sha256':inventory(baseline/'logs'),
        'runtime_prefix_sha256':inventory(parity.required_path(prior_config,'runtime_prefix',directory=True)),
        'provider_sha256':{name:parity.sha(parity.required_path(prior_config,name)) for name in
                           ('zstd_jar','lz4_jar','zstd_dll','lz4_dll','lz4_module','zlib_library','zstd_library','lz4_library','cpp_helper','cpp_linked_helper')},
        'python_provider_sha256':python_deployment(parity.required_path(prior_config,'python_provider',directory=True)),
        'native_provider_deployment_sha256':native_deployment(prior_config,baseline_compiler.parent,
                                                            [baseline/'cpp',baseline/'cpp/build']),
    }
    if args.seal_only:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(seal,indent=2)+'\n',encoding='utf-8')
        print('Completed baseline sealed; no repaired-runtime qualification asserted.')
        return
    seal_path = baseline/'baseline-seal.json'
    if json.loads(seal_path.read_text(encoding='utf-8'))!=seal:
        raise AssertionError('Baseline source, corpus, runtime, reports or logs changed after sealing')
    before = production_inputs(baseline_compiler.parent/'source-inventory.json')
    after = production_inputs(compiler.parent/'source-inventory.json')
    changed = sorted(path for path in before.keys()|after.keys() if before.get(path)!=after.get(path))
    if changed!=['src/compression.cpp']:
        raise AssertionError('The refresh requires an isolated C++ runtime repair: '+str(changed))
    config = json.loads(args.config.read_text(encoding='utf-8-sig'))
    for name in ('zstd_jar','lz4_jar','zstd_dll','lz4_dll','lz4_module','zlib_library','zstd_library','lz4_library'):
        if parity.sha(parity.required_path(config,name))!=parity.sha(parity.required_path(prior_config,name)):
            raise AssertionError('Selected provider changed: '+name)
    if python_deployment(parity.required_path(config,'python_provider',directory=True))!=seal['python_provider_sha256']:
        raise AssertionError('Python oracle deployment changed')
    runtime_name = 'serializer_runtime.lib' if os.name=='nt' else 'libserializer_runtime.a'
    runtime_prefix = parity.required_path(config,'runtime_prefix',directory=True)
    captured_runtime = compiler.parent/runtime_name
    if parity.sha(runtime_prefix/'lib'/runtime_name)!=parity.sha(captured_runtime):
        raise AssertionError('Installed runtime is not the candidate native capture')
    artifact_manifest = json.loads((compiler.parent/'source-manifest.json').read_text(encoding='utf-8-sig'))
    captured_artifacts = {item['name']:item['sha256'] for item in artifact_manifest['artifacts']}
    if captured_artifacts[runtime_name]!=parity.sha(captured_runtime) or captured_artifacts[compiler.name]!=parity.sha(compiler):
        raise AssertionError('Candidate artifacts disagree with their native capture manifest')
    for path,digest in after.items():
        if path.startswith('include/') and not path.endswith('.in') and parity.sha(runtime_prefix/path)!=digest:
            raise AssertionError('Installed header is not the candidate source capture: '+path)
    current_prefix = inventory(runtime_prefix)
    for path,digest in seal['runtime_prefix_sha256'].items():
        if path.startswith(('include/','lib/cmake/')) and current_prefix.get(path)!=digest:
            raise AssertionError('Installed headers or CMake import metadata changed: '+path)
    native_deployment(config,compiler.parent,[build/'cpp',build/'cpp/build'])
    sdk = parity.tools(config)
    environment = os.environ.copy()
    environment.update({key:os.path.expandvars(value) for key,value in config.get('environment',{}).items()})
    environment['PATH'] = os.pathsep.join(str(parity.required_path(config,name,directory=True)/'bin')
                                        for name in ('runtime_prefix','dependency_prefix'))+os.pathsep+environment.get('PATH','')
    provider_path = parity.required_path(config,'python_provider',directory=True)
    sys.path.insert(0,str(provider_path))
    environment['PYTHONPATH'] = str(provider_path)+os.pathsep+environment.get('PYTHONPATH','')
    environment.update(PYTHONDONTWRITEBYTECODE='1',DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1',
                       DOTNET_CLI_TELEMETRY_OPTOUT='1',DOTNET_GENERATE_ASPNET_CERTIFICATE='false')
    build.mkdir(parents=True,exist_ok=True)
    logs = build/'logs'; logs.mkdir(exist_ok=True)
    temporary = build/'temporary'; temporary.mkdir(exist_ok=True)
    environment.update(TMP=str(temporary),TEMP=str(temporary),TMPDIR=str(temporary))
    identities = {}
    for language,(filename,options) in GENERATED.items():
        directory = build/'emission'/language
        directory.mkdir(parents=True,exist_ok=True)
        if language=='rust': (directory/'src').mkdir(exist_ok=True)
        parity.run([compiler,'--input',parity.SOURCES/'model.serializer',*options],directory,environment,logs/(language+'-emission.log'))
        paths = (filename,'schema.d.mts') if language in ('javascript','typescript') else (filename,)
        identities[language] = {}
        for spelling in paths:
            digest = parity.sha(directory/spelling)
            if digest!=parity.sha(baseline/language/spelling):
                raise AssertionError('Unchanged SDK output drifted: '+language+'/'+spelling)
            identities[language][spelling] = digest
    corpus = build/'corpus'
    shutil.copytree(baseline/'corpus',corpus,dirs_exist_ok=True)
    baseline_inputs = inventory(baseline/'corpus')
    if inventory(corpus)!=baseline_inputs:
        raise AssertionError('Common corpus copy differs')
    command = parity.prepare('cpp',build/'cpp',compiler,sdk,config,environment)
    deployment = native_deployment(config,compiler.parent,[build/'cpp',build/'cpp/build'])
    # Depfiles contain the selected build directory; compare emitted model headers themselves.
    cpp_generated = {path:digest for path,digest in inventory(build/'cpp/build/generated').items()
                     if path.endswith('.hpp')}
    baseline_cpp_generated = {path:digest for path,digest in inventory(baseline/'cpp/build/generated').items()
                              if path.endswith('.hpp')}
    if not cpp_generated or cpp_generated!=baseline_cpp_generated:
        raise AssertionError('C++ generated model header inventory drifted')
    produced = parity.run(command+['produce',corpus],build/'cpp',environment,logs/'cpp-produce.log')
    if json.loads(produced.splitlines()[-1])!={'produced':24,'encode_bounds':72}:
        raise AssertionError('Incomplete repaired C++ producer')
    if inventory(corpus)!=baseline_inputs:
        raise AssertionError('A producer frame or ordinary native byte payload changed; repeat the complete qualification')
    checked = parity.run(command+['check',corpus],build/'cpp',environment,logs/'cpp-check.log')
    if json.loads(checked.splitlines()[-1])!=expected:
        raise AssertionError('Incomplete repaired C++ consumer')
    controls = {}
    for mode,count in (('controls',6),('candidate-controls',6),('reentry-controls',1),('source-controls',6),('input-finish-controls',6)):
        result = parity.run(command+[mode,corpus],build/'cpp',environment,logs/('cpp-'+mode+'.log'))
        if json.loads(result.splitlines()[-1])!={'controls':count}:
            raise AssertionError('Incomplete repaired C++ '+mode)
        controls[mode] = count
    # Source equality, identical provider inputs and the complete unchanged SDK records justify reuse.
    results = {**previous['passed_consumers'],'cpp':expected}
    report = {
        'qualification':'compression_selected_s35_parity_cpp_runtime_refresh',
        'utc':datetime.now(timezone.utc).isoformat(),
        'baseline_compiler_sha256':parity.sha(baseline_compiler),'compiler_sha256':parity.sha(compiler),
        'baseline_failure_report_sha256':parity.sha(failure_path),'baseline_build':str(baseline),
        'baseline_seal_sha256':parity.sha(seal_path),
        'baseline_configuration_sha256':parity.sha(args.baseline_config),'configuration_sha256':parity.sha(args.config),
        'production_delta':changed,'compiler_input_manifest_sha256':parity.sha(compiler.parent/'source-inventory.json'),
        'baseline_input_manifest_sha256':parity.sha(baseline_compiler.parent/'source-inventory.json'),
        'unchanged_generated_sdk_sha256':identities,'maintained_lifecycle_runners':lifecycle,
        'unchanged_cpp_generated_sha256':cpp_generated,'captured_runtime_sha256':parity.sha(captured_runtime),
        'native_provider_deployment_sha256':deployment,
        'manifest':json.loads((parity.SOURCES/'manifest.json').read_text(encoding='utf-8')),
        'fixture_counts':previous['fixture_counts'],'results':results,'model_exchanges':2904,
        'shared_decisions':15400,'independent_producer_frames':264,'encode_bound_decisions':792,
        'cpp_lifecycle_controls':controls,'cpp_byte_identical_producer_frames':24,
        'baseline_producer_log_sha256':{language:parity.sha(baseline/'logs'/(language+'-produce.log')) for language in parity.LANGUAGES},
        'baseline_consumer_log_sha256':{language:parity.sha(baseline/'logs'/(language+'-check.log')) for language in parity.LANGUAGES},
        'baseline_sdk_version_logs_sha256':{path.name:parity.sha(path) for path in sorted((baseline/'logs').glob('version-*.log'))},
        'input_sha256':baseline_inputs,'runtime_prefix_sha256':inventory(parity.required_path(config,'runtime_prefix',directory=True)),
        'refresh_driver_sha256':parity.sha(__file__),'baseline_driver_sha256':parity.sha(parity.__file__),
        'scope':'Ten unchanged profiles and seventeen SDK suites ran on the baseline compiler; C++ replay uses the isolated repaired runtime and fresh compiler. Full source/producer identity checks are mandatory.',
        'limitations':['Selected SDK/provider/host profiles only.','No XML/OPC context, performance, RSS ceiling or provider-preemption claim.'],
    }
    output = args.output.resolve(); output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'shared_decisions':15400,'model_exchanges':2904,'maintained_sdk_suites':17,'cpp_controls':25}))


if __name__=='__main__':
    main()
