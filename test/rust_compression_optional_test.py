# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual Rust optional providers with offline Cargo, independent frames and native consumers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tomllib

import rust_compression_test as core
from java_compression_optional_test import fixtures as optional_fixtures
from go_compression_optional_test import native_fixtures


ROOT = Path(__file__).resolve().parents[1]
LIBRARY = ROOT / 'qualification/rust_compression_providers'
RESOURCES = ROOT / 'test/resources'
BORROW_PREFIX = r'''mod schema;use schema::__COMPRESSION_NAME__ as c;
struct Provider;
impl c::Provider for Provider {
    // Report only the diagnostic format; no provider code executes in negative compile controls.
    fn format(&self)->c::Format {c::Format::Zstd}
    // Admit a type-checking-only capability.
    fn validate_encode(&self,_level:i32)->c::Result<()> {Ok(())}
    // Admit a type-checking-only capability.
    fn validate_decode(&self,_maximum:usize)->c::Result<()> {Ok(())}
    // These compile-failure controls must never execute a codec.
    fn encoder(&self,_level:i32,_size:usize)->c::Result<Box<dyn c::Encoder+'_>> {unreachable!()}
    // These compile-failure controls must never execute a codec.
    fn decoder(&self,_maximum:usize)->c::Result<Box<dyn c::Decoder+'_>> {unreachable!()}
}
'''


def run(command, directory, environment, log, timeout=300):
    """Retain exact offline SDK commands and diagnostics and reject every unsuccessful invocation."""
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=directory, env=environment, capture_output=True,
                            text=True, encoding='utf8', errors='replace', timeout=timeout)
    log.write_text(json.dumps(command) + '\n' + result.stdout + result.stderr, encoding='utf8')
    if result.returncode:
        raise RuntimeError(f'{log}: exit {result.returncode}\n{result.stdout}{result.stderr}')
    return result.stdout.strip()


def registry_packages(lock):
    """Read the exact transitive registry package identities from an actual Cargo lockfile."""
    return {entry['name']: (entry['version'], entry.get('checksum'))
            for entry in tomllib.loads(lock.read_text(encoding='utf8'))['package'] if entry.get('source')}


def prepare_project(args, case, environment, name, schema=core.SCHEMA, flags=()):
    """Emit the actual model/runtime and resolve only the maintained explicitly pinned adapter library."""
    source = case / 'src'
    source.mkdir(parents=True, exist_ok=True)
    helper, profile = core.generate(args.compiler, source, schema, flags)
    manifest = '''[package]
name = "serializer-rust-optional-__NAME__"
version = "0.0.0"
edition = "2021"
publish = false

[features]
default = []
zstd = ["serializer-compression-providers/zstd"]
lz4 = ["serializer-compression-providers/lz4"]

[dependencies]
serializer-compression-providers = { path = __LIBRARY__, default-features = false }

[profile.release]
overflow-checks = true
'''
    (case / 'Cargo.toml').write_text(manifest.replace('__NAME__', name).replace(
        '__LIBRARY__', json.dumps(str(LIBRARY.resolve()).replace('\\', '/'))), encoding='utf8')
    # Cargo requires a target even while resolving a new consumer lockfile.
    (source / 'main.rs').write_text('// Reserve the actual Cargo target before installing its maintained harness.\nfn main() {}\n', encoding='utf8')
    run([args.cargo, 'generate-lockfile', '--offline', '--manifest-path', case / 'Cargo.toml'],
        case, environment, case / 'lock.log')
    packages = registry_packages(case / 'Cargo.lock')
    if packages != registry_packages(LIBRARY / 'Cargo.lock'):
        raise AssertionError('Consumer transitive dependencies differ from the maintained provider lock')
    profile.update(helper=helper, registry_packages=packages)
    return helper, profile


def execute_project(args, case, environment, name, features, arguments=()):
    """Compile strict checked Rust against selected feature deployments and run the actual binary."""
    command = [args.cargo, 'rustc', '--release', '--offline', '--locked', '--manifest-path', case / 'Cargo.toml']
    if features:
        command += ['--features', features]
    command += ['--', '-D', 'warnings']
    label = features.replace(',', '-') or 'absent'
    run(command, case, environment, case / ('build-' + label + '.log'))
    executable = Path(environment['CARGO_TARGET_DIR']) / 'release' / (
        'serializer-rust-optional-' + name + ('.exe' if sys.platform == 'win32' else ''))
    return run([executable, *arguments], case, environment, case / ('run-' + label + '.log'))


def verify_outputs(args, directory, environment):
    """Require exact single-frame standard output and consume every native payload through both C++ paths."""
    from compression import zstd
    import lz4.frame
    count = 0
    for name, levels in (('ZSTD', (-131072, -1, 3, 22)), ('LZ4', (0,))):
        cases = [(directory / f'output-{name}-{protocol}.bin',
                  (directory / f'native-{protocol}.bin').read_bytes())
                 for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')]
        cases += [(directory / f'level-{name}-{level}-{sample}.bin', b'A' * 65537 if sample else b'')
                  for level in levels for sample in (0, 1)]
        for source, expected in cases:
            frame = source.read_bytes()
            decoder = zstd.ZstdDecompressor() if name == 'ZSTD' else lz4.frame.LZ4FrameDecompressor()
            if decoder.decompress(frame) != expected or not decoder.eof or decoder.unused_data:
                raise AssertionError('Independent Python consumer disagrees: ' + source.name)
            if not frame[4] & 4:
                raise AssertionError('Optional output content checksum is absent')
            for mode in ('decode', 'stream_decode'):
                destination = directory / (source.stem + '-' + mode + '.plain')
                run([args.cpp_helper, mode, name.lower(), source, destination], directory,
                    environment, directory / (source.stem + '-' + mode + '.log'))
                if destination.read_bytes() != expected:
                    raise AssertionError('Native C++ consumer disagrees: ' + source.name)
            count += 1
    return count


def borrow_checks(args, case, helper, environment):
    """Require exclusive provider lifetimes and callback borrows, plus unchanged exhaustive option literals."""
    cases = {
        'reenter_output': ('let provider=Provider;let mut sink=Vec::new();let mut output=c::Output::with_provider(&mut sink,c::EncodeOptions{format:c::Format::Zstd,..Default::default()},&provider).unwrap();let _=output.encode(||{output.close();Ok(Vec::new())});', ('E0499', 'E0500', 'E0501', 'E0502')),
        'reenter_input': ('let provider=Provider;let mut source=std::io::Cursor::new(Vec::<u8>::new());let mut input=c::Input::with_provider(&mut source,c::DecodeOptions{format:c::Format::Zstd,..Default::default()},&provider).unwrap();let _=input.decode(|_|{input.close();Ok(0)});', ('E0499', 'E0500', 'E0501', 'E0502')),
        'drop_provider_output': ('let provider=Provider;let mut sink=Vec::new();let mut output=c::Output::with_provider(&mut sink,c::EncodeOptions{format:c::Format::Zstd,..Default::default()},&provider).unwrap();drop(provider);let _=output.finish();', ('E0505',)),
        'drop_provider_input': ('let provider=Provider;let mut source=std::io::Cursor::new(Vec::<u8>::new());let mut input=c::Input::with_provider(&mut source,c::DecodeOptions{format:c::Format::Zstd,..Default::default()},&provider).unwrap();drop(provider);let _=input.finish();', ('E0505',)),
        'mutate_provider': ('let mut provider=Provider;let mut sink=Vec::new();let mut output=c::Output::with_provider(&mut sink,c::EncodeOptions{format:c::Format::Zstd,..Default::default()},&provider).unwrap();provider=Provider;let _=output.finish();', ('E0506',)),
        'provider_scope': ('let mut sink=Vec::new();let mut output;{let provider=Provider;output=c::Output::with_provider(&mut sink,c::EncodeOptions{format:c::Format::Zstd,..Default::default()},&provider).unwrap();}let _=output.finish();', ('E0597',)),
    }
    executable = case / ('borrow-probe' + ('.exe' if sys.platform == 'win32' else ''))
    for name, (body, codes) in cases.items():
        source = case / (name + '.rs')
        source.write_text(BORROW_PREFIX.replace('__COMPRESSION_NAME__', helper) +
                          '// This program must be rejected by the actual borrow checker.\nfn main(){' + body + '}\n', encoding='utf8')
        result = subprocess.run([args.rustc, '--edition=2021', '-D', 'warnings', str(source), '-o', str(executable)],
                                cwd=case, env=environment, capture_output=True, text=True,
                                encoding='utf8', errors='replace', timeout=180)
        (case / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf8')
        if result.returncode == 0 or not any(code in result.stderr for code in codes):
            raise AssertionError('Expected exclusive-borrow diagnostic absent: ' + name)
    source = case / 'compatibility.rs'
    source.write_text('mod schema;use schema::' + helper + ''' as c;
// Original exhaustive options and default constructors remain dependency-free and executable.
fn main(){let options=c::EncodeOptions{format:c::Format::None,max_input_bytes:0,max_output_bytes:0,level:6,check_cancelled:None};
let decode=c::DecodeOptions{format:c::Format::None,max_input_bytes:0,max_output_bytes:0,max_window_bytes:0,check_cancelled:None};
let mut sink=Vec::new();{let mut output=c::Output::new(&mut sink,options).unwrap();output.finish().unwrap();}
let mut source=std::io::Cursor::new(sink);let mut input=c::Input::new(&mut source,decode).unwrap();input.finish().unwrap();}
''', encoding='utf8')
    run([args.rustc, '--edition=2021', '-D', 'warnings', source, '-o', executable],
        case, environment, case / 'compatibility-build.log')
    run([executable], case, environment, case / 'compatibility-run.log')
    return len(cases)


def smoke_source(helper, profile):
    """Use explicit feature-scoped providers beside preserved model names and old default admission."""
    source = '''mod schema;use schema::*;use schema::__COMPRESSION_NAME__ as c;
#[cfg(feature="zstd")] serializer_compression_providers::zstd_provider!(zstd,crate::schema::__COMPRESSION_NAME__);
#[cfg(feature="lz4")] serializer_compression_providers::lz4_provider!(lz4,crate::schema::__COMPRESSION_NAME__);
// Select immutable per-operation capabilities without implicit provider discovery.
fn selected(format:c::Format)->Option<&'static dyn c::Provider>{match format{
#[cfg(feature="zstd")] c::Format::Zstd=>Some(&zstd::Zstd),
#[cfg(feature="lz4")] c::Format::Lz4=>Some(&lz4::Lz4),_=>None}}
// Preserve native protocols and explicit absent/single/both-provider admission.
fn main(){let value=AccountRecord{account_id:7,..Default::default()};
for format in [c::Format::Zstd,c::Format::Lz4]{assert!(!c::available(format));
let provider=selected(format);assert_eq!(c::available_with_provider(format,provider),provider.is_some());
let options=c::EncodeOptions{format,level:if format==c::Format::Lz4{0}else{3},..Default::default()};
if let Some(provider)=provider {let packed=c::encode_with_provider(||value.encode(Protocol::BinaryNone).map_err(c::into_error),options,provider).unwrap();
let result=c::decode_with_provider(&packed,|bytes|AccountRecord::decode(bytes,Protocol::BinaryNone).map_err(c::into_error),c::DecodeOptions{format,..Default::default()},provider).unwrap();assert_eq!(result.account_id,7);
__BINARY_CHECK__
}else{let calls=std::cell::Cell::new(0);let check=||{calls.set(calls.get()+1);Ok(())};
let error=c::encode(||{calls.set(calls.get()+1);Ok(Vec::new())},c::EncodeOptions{check_cancelled:Some(&check),..options}).unwrap_err();
assert_eq!(error.downcast_ref::<c::Failure>().unwrap().code,c::Code::Unavailable);assert_eq!(calls.get(),0);}}
__MODEL_CHECKS__
println!("PASS explicit optional provider profile");}
'''
    models = ('SerializerCompression', 'SerializerCompression2') if profile == 'collision' else (
        ('Provider', 'Encoder', 'Decoder', 'Progress') if profile == 'models' else ())
    model_checks = '\n'.join(f'{name}::decode(&{name}::default().encode(Protocol::BinaryNone).unwrap(),Protocol::BinaryNone).unwrap();' for name in models)
    binary = 'assert!(value.encode(Protocol::Json).is_err());' if profile == 'binary' else ''
    return source.replace('__COMPRESSION_NAME__', helper).replace('__BINARY_CHECK__', binary).replace('__MODEL_CHECKS__', model_checks)


def main():
    """Require actual compiler output, pinned offline adapters, native fixtures and all four feature deployments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--cargo-home', type=Path, required=True)
    parser.add_argument('--cpp-helper', type=Path, required=True)
    parser.add_argument('--cpp-linked-helper', type=Path, required=True)
    parser.add_argument('--provider-path', type=Path)
    parser.add_argument('--cargo', default=shutil.which('cargo'))
    parser.add_argument('--rustc', default=shutil.which('rustc'))
    args = parser.parse_args()
    if not args.cargo or not args.rustc or not args.cargo_home.is_dir():
        parser.error('Actual Cargo/Rust SDKs and an explicit offline Cargo home are required')
    if not all(path.is_file() for path in (args.compiler, args.cpp_helper, args.cpp_linked_helper)):
        parser.error('The actual compiler and both native C++ helpers are required')
    args.compiler, args.cpp_helper, args.cpp_linked_helper = (
        path.resolve() for path in (args.compiler, args.cpp_helper, args.cpp_linked_helper))
    directory = args.build.resolve()
    if directory == ROOT or directory.is_relative_to(ROOT) and directory.relative_to(ROOT).parts[0] != 'out':
        parser.error('In-repository output must remain beneath ignored out/')
    directory.mkdir(parents=True, exist_ok=True)
    if args.provider_path:
        sys.path.insert(0, str(args.provider_path.resolve()))
    environment = os.environ.copy()
    environment.update(CARGO_HOME=str(args.cargo_home.resolve()), CARGO_NET_OFFLINE='true',
                       CARGO_TARGET_DIR=str(directory / 'target'), CARGO_BUILD_JOBS='4', RUSTC=args.rustc)
    versions = {name: run([sdk, '--version'], directory, environment, directory / (name + '-version.log'))
                for name, sdk in (('rustc', args.rustc), ('cargo', args.cargo))}
    fixtures = directory / 'fixtures'
    fixtures.mkdir(exist_ok=True)
    counts = optional_fixtures(fixtures, ('ZSTD', 'LZ4'))
    native = native_fixtures(args, fixtures, environment, counts)
    case = directory / 'normal'
    helper, profile = prepare_project(args, case, environment, 'normal')
    for name in ('main', 'controls'):
        (case / 'src' / (name + '.rs')).write_text((RESOURCES / (
            'compression_rust_optional_' + name + '.rs')).read_text(encoding='utf8').replace(
                '__COMPRESSION_NAME__', helper), encoding='utf8')
    output = execute_project(args, case, environment, 'normal', 'zstd,lz4', (fixtures,))
    for name, count in counts.items():
        if f'PASS actual generated {name} decisions {count}' not in output:
            raise AssertionError('Incomplete actual optional corpus: ' + name)
    if verify_outputs(args, case, environment) != 18:
        raise AssertionError('Incomplete independent output corpus')
    borrow = borrow_checks(args, case / 'src', helper, environment)
    profiles = {'normal': profile}
    for name, schema, flags in (
            ('collision', core.SCHEMA + ' class SerializerCompression {} class SerializerCompression_2 {}', ()),
            ('models', core.SCHEMA + ' class Provider {} class Encoder {} class Decoder {} class Progress {}', ()),
            ('binary', core.SCHEMA, ('--protocols', 'binary_none')),
            ('deployment', core.SCHEMA, ())):
        case = directory / name
        family, profiles[name] = prepare_project(args, case, environment, name, schema, flags)
        if name == 'collision' and family != 'serializer_compression_3':
            raise AssertionError('Existing generated helper-name models were not preserved')
        (case / 'src/main.rs').write_text(smoke_source(family, name), encoding='utf8')
        for features in (('', 'zstd', 'lz4', 'zstd,lz4') if name == 'deployment' else ('zstd,lz4',)):
            execute_project(args, case, environment, name, features)
    from compression import zstd
    import lz4
    files = [LIBRARY / name for name in ('Cargo.toml', 'Cargo.lock', 'src/lib.rs')]
    files += [RESOURCES / ('compression_rust_optional_' + name + '.rs') for name in ('main', 'controls')]
    report = {'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(), 'sdk': versions,
              'edition': '2021', 'release_overflow_checks': True, 'independent_input_decisions': counts,
              'independent_outputs': 18, 'native_cpp_consumers': 36, 'native_cpp_producers': native,
              'exclusive_borrow_compile_failures': borrow, 'profiles': profiles,
              'feature_deployments': ['absent', 'zstd', 'lz4', 'zstd,lz4'],
              'oracle_versions': {'stdlib_zstd': list(zstd.zstd_version_info), 'python_lz4': lz4.__version__,
                                  'liblz4': lz4.library_version_string()},
              'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in files},
              'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'cpp_helper_sha256': hashlib.sha256(args.cpp_helper.read_bytes()).hexdigest(),
              'cpp_linked_helper_sha256': hashlib.sha256(args.cpp_linked_helper.read_bytes()).hexdigest()}
    (directory / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
