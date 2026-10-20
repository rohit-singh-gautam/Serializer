# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated Rust compression with a dependency-free native Rust SDK."""
import argparse
import base64
import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import zlib

from java_compression_test import fixtures as common_fixtures
from csharp_compression_test import extra_fixtures

ROOT = Path(__file__).resolve().parent.parent
SCHEMA = 'serializer version 1; class account_record stable_ids { public uint32 account_id (1); public string label (2); }'
RESOURCE = ROOT / 'test/resources/compression_rust_main.rs'

REMOTE = r'''import base64,json,os,pathlib,subprocess,sys,tarfile,tempfile
with tempfile.TemporaryDirectory(prefix='serializer-rust-compression-') as temporary:
 root=pathlib.Path(temporary)
 os.environ['PATH']='/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin'
 with tarfile.open(fileobj=sys.stdin.buffer,mode='r|gz') as archive:
  for member in archive:
   target=(root/member.name).resolve()
   if not member.isfile() or not target.is_relative_to(root):raise RuntimeError('Invalid SDK archive member')
   target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(archive.extractfile(member).read())
 sdk=sys.argv[1];version=subprocess.check_output([sdk,'--version'],text=True).strip()
 for profile in ('normal','collision','binary'):
  case=root/profile
  subprocess.run([sdk,'--edition=2021','-D','warnings','-O',str(case/'main.rs'),'-o',str(case/'consumer')],check=True)
  subprocess.run([str(case/'consumer'),str(case)],check=True)
 for name in ('reenter_output','reenter_input'):
  result=subprocess.run([sdk,'--edition=2021','-D','warnings',str(root/'normal'/(name+'.rs')),'-o',str(root/'normal'/name)],capture_output=True,text=True)
  (root/'normal'/(name+'.log')).write_text(result.stderr)
  if result.returncode==0 or not any(code in result.stderr for code in ('E0499','E0500','E0501','E0502')):raise RuntimeError('Missing exclusive-borrow rejection')
 files={path.name:base64.b64encode(path.read_bytes()).decode() for path in (root/'normal').glob('*') if path.is_file() and (path.name.startswith(('native-','output-','level-','sample-')) or path.suffix=='.log')}
 print('RUST_RESULT '+json.dumps({'sdk':version,'files':files}),flush=True)
'''


def generate(compiler, case, schema, flags=()):
    """Emit real Rust, retaining the exact native model prefix and selected public helper module."""
    source, output = case / 'model.serializer', case / 'schema.rs'
    source.write_text(schema, encoding='utf-8')
    subprocess.run([str(compiler), '--input', str(source), '--language', 'rust',
                    '--rust.output', str(output), *flags], check=True)
    data = output.read_bytes()
    marker = re.search(rb'// Generated Rust compression helper module: (\w+)\n', data)
    if marker is None:
        raise AssertionError('Actual emitted Rust compression helper is absent')
    return marker.group(1).decode(), {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
                                    'native_prefix_sha256': hashlib.sha256(data[:marker.start()]).hexdigest()}


def smoke(helper, collision=False, binary=False):
    """Compile preserved normalized models beside the helper and disabled native protocols."""
    source = 'mod schema; use schema::*; use schema::' + helper + ' as c;\n'
    source += '/// Exercise ordinary generated models without dependencies.\nfn main() {\n'
    source += 'let mut value=AccountRecord::default(); value.account_id=7;\n'
    source += 'let packed=c::encode(|| value.encode(Protocol::BinaryNone).map_err(c::into_error),c::EncodeOptions {format:c::Format::Gzip,..Default::default()}).unwrap();\n'
    source += 'let candidate=c::decode(&packed,|data| AccountRecord::decode(data,Protocol::BinaryNone).map_err(c::into_error),c::DecodeOptions {format:c::Format::Gzip,..Default::default()}).unwrap(); assert_eq!(candidate.account_id,7);\n'
    if collision:
        for model in ('SerializerCompression', 'SerializerCompression2'):
            source += f'let bytes={model}::default().encode(Protocol::BinaryNone).unwrap(); {model}::decode(&bytes,Protocol::BinaryNone).unwrap();\n'
    if binary:
        source += 'assert!(value.encode(Protocol::Json).is_err());\n'
    return source + 'println!("PASS Rust generated profile");\n}\n'


def compile_fail(case, helper):
    """Prove safe Rust prevents a callback from closing or reentering the same active wrapper."""
    common = 'mod schema; use schema::' + helper + ' as c;\n'
    output = 'fn main() { let mut sink=Vec::new(); let mut wrapper=c::Output::new(&mut sink,Default::default()).unwrap(); let _=wrapper.encode(|| {wrapper.close(); Ok(Vec::new())}); }\n'
    source = 'fn main() { let mut source=std::io::Cursor::new(Vec::<u8>::new()); let mut wrapper=c::Input::new(&mut source,Default::default()).unwrap(); let _=wrapper.decode(|data| {wrapper.close(); Ok(data.len())}); }\n'
    (case / 'reenter_output.rs').write_text(common + output, encoding='utf-8')
    (case / 'reenter_input.rs').write_text(common + source, encoding='utf-8')


def execute(build, rustc, wsl):
    """Compile actual source locally or stream it to private native WSL storage without mounted I/O."""
    if wsl:
        payload = io.BytesIO()
        with tarfile.open(fileobj=payload, mode='w:gz') as archive:
            for profile in ('normal', 'collision', 'binary'):
                for path in sorted((build / profile).iterdir()):
                    if path.is_file() and path.suffix not in ('.exe', '.log'):
                        archive.add(path, arcname=profile + '/' + path.name)
        result = subprocess.run(['wsl.exe', '--cd', '/tmp', '--exec', '/usr/bin/python3', '-c', REMOTE, rustc],
                                input=payload.getvalue(), capture_output=True, timeout=360)
        text = result.stdout.decode(errors='replace')
        prefix, marker, encoded = text.partition('RUST_RESULT ')
        (build / 'sdk.log').write_bytes(prefix.encode() + result.stderr)
        print(prefix, end='', flush=True)
        if result.returncode or not marker:
            raise RuntimeError('Actual Linux Rust qualification failed: ' + result.stderr.decode(errors='replace'))
        report = json.loads(encoded)
        for name, value in report['files'].items():
            if Path(name).name != name:
                raise AssertionError('Invalid SDK output name')
            (build / 'normal' / name).write_bytes(base64.b64decode(value))
        return report['sdk']
    version = subprocess.check_output([rustc, '--version'], text=True).strip()
    with (build / 'sdk.log').open('w', encoding='utf-8') as log:
        for profile in ('normal', 'collision', 'binary'):
            case = build / profile
            executable = case / 'consumer.exe'
            subprocess.run([rustc, '--edition=2021', '-D', 'warnings', '-O', str(case / 'main.rs'), '-o', str(executable)],
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
            subprocess.run([str(executable), str(case)], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
        for name in ('reenter_output', 'reenter_input'):
            result = subprocess.run([rustc, '--edition=2021', '-D', 'warnings', str(build / 'normal' / (name + '.rs')),
                                     '-o', str(build / 'normal' / (name + '.exe'))], capture_output=True, text=True)
            (build / 'normal' / (name + '.log')).write_text(result.stderr, encoding='utf-8')
            if result.returncode == 0 or not any(code in result.stderr for code in ('E0499', 'E0500', 'E0501', 'E0502')):
                raise AssertionError('Safe Rust permitted active-wrapper reentry')
    return version


def oracle(directory, payload):
    """Decode every actual output using independent standard zlib with exact one-frame EOF checks."""
    expected_names = {'output-' + name + '-' + protocol + '.bin'
                      for name in ('NONE', 'GZIP', 'ZLIB', 'DEFLATE')
                      for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')}
    expected_names.update('level-' + name + '-' + str(level) + '.bin'
                          for name in ('NONE', 'GZIP', 'ZLIB', 'DEFLATE') for level in (0, 1, 6, 9))
    expected_names.update('sample-' + name + '-' + sample + '-' + str(level) + '.bin'
                          for name in ('NONE', 'GZIP', 'ZLIB', 'DEFLATE')
                          for sample in ('repeat', 'history') for level in (0, 1, 6, 9))
    paths = [path for path in directory.glob('*.bin') if path.name.startswith(('output-', 'level-', 'sample-'))]
    if {path.name for path in paths} != expected_names:
        raise AssertionError('Missing or unexpected Rust output oracle frames')
    for path in paths:
        kind, name, selector = path.stem.split('-', 2)
        expected = ((b'A' * 262145 if selector.startswith('repeat-') else payload[:32768] * 2 + payload[:257])
                    if kind == 'sample' else payload[:70001] if kind == 'level'
                    else (directory / ('native-' + selector + '.bin')).read_bytes())
        packed = path.read_bytes()
        if name == 'NONE':
            plain = packed
        else:
            decoder = zlib.decompressobj({'GZIP': 31, 'ZLIB': 15, 'DEFLATE': -15}[name])
            plain = decoder.decompress(packed) + decoder.flush()
            if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                raise AssertionError('Rust output is not exactly one standard frame')
        if plain != expected:
            raise AssertionError('Rust outer frame changed the original native bytes')
    if json.loads((directory / 'native-JSON.bin').read_bytes())['account_id'] != 42:
        raise AssertionError('Native JSON field identity changed')
    return len(paths)


def main():
    """Require a real SDK and actual compiler output for all three dependency-free Rust profiles."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--rustc')
    parser.add_argument('--wsl', action='store_true')
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    args.rustc = args.rustc or ('/usr/bin/rustc' if args.wsl else shutil.which('rustc'))
    if not args.rustc:
        parser.error('An actual Rust SDK is required')
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository output must remain below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    profiles = {}
    for name, schema, flags in (('normal', SCHEMA, ()), ('collision', SCHEMA + ' class SerializerCompression {} class SerializerCompression_2 {}', ()),
                                 ('binary', SCHEMA, ('--protocols', 'binary_none'))):
        case = build / name
        case.mkdir(exist_ok=True)
        helper, profiles[name] = generate(args.compiler.resolve(), case, schema, flags)
        if name == 'normal':
            count = common_fixtures(case) + extra_fixtures(case)
            payload = (case / 'plain.bin').read_bytes()
            source = RESOURCE.read_text(encoding='utf-8').replace('__COMPRESSION_NAME__', helper)
            compile_fail(case, helper)
            if args.baseline and not (case / 'schema.rs').read_bytes().startswith(args.baseline.read_bytes()):
                raise AssertionError('Rust native model/runtime prefix changed')
        else:
            if name == 'collision' and helper != 'serializer_compression_3':
                raise AssertionError('Rust compression helper did not preserve colliding model names')
            source = smoke(helper, collision=name == 'collision', binary=name == 'binary')
        (case / 'main.rs').write_text(source, encoding='utf-8')
    sdk = execute(build, args.rustc, args.wsl)
    outputs = oracle(build / 'normal', payload)
    report = {'rust': sdk, 'edition': '2021', 'native_wsl': args.wsl, 'independent_input_decisions': count,
              'independent_output_decisions': outputs, 'exclusive_borrow_compile_failures': 2, 'profiles': profiles,
              'native_prefix_matches_baseline': bool(args.baseline),
              'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'resource_sha256': hashlib.sha256(RESOURCE.read_bytes()).hexdigest()}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
