# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual standalone Swift compression against independent standard frames."""
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
import tempfile
import zlib

from java_compression_test import fixtures as common_fixtures
from csharp_compression_test import extra_fixtures


SCHEMA = 'serializer version 1; class account_record stable_ids { public uint32 account_id (1); public string label (2); }'

SWIFT = r'''import Foundation
typealias C = __COMPRESSION_NAME__
let formats: [(String, C.Format)] = [("NONE",.none),("GZIP",.gzip),("ZLIB",.zlib),("DEFLATE",.deflate)]
// Keep checks active in optimized qualification builds.
func require(_ value: Bool, _ message: String) { if !value { fatalError(message) } }
// Keep external failure identity observable through every cleanup path.
final class Marker: Swift.Error { let message: String; init(_ message: String) { self.message = message } }
// Check stable compression categories independently of diagnostic text.
func reject(_ code: C.Code, _ action: () throws -> Void) {
    do { try action() } catch let failure as C.Failure { require(failure.code == code, "Wrong compression category: \(failure.message)"); return }
    catch { fatalError("Wrong exception type: \(error)") }
    fatalError("Accepted \(code)")
}
// An active operation retains the exact application's original class error.
func same(_ expected: any Swift.Error, _ action: () throws -> Void) {
    do { try action() } catch { require(error as AnyObject === expected as AnyObject, "Replaced original error"); return }
    fatalError("Missing original exception")
}
// Load ordinary fixture bytes without a helper producer creating its own test inputs.
func bytes(_ directory: String, _ name: String) throws -> [UInt8] { Array(try Data(contentsOf: URL(fileURLWithPath: directory+"/"+name))) }
// Retain actual output for a separate standard-library frame oracle.
func save(_ directory: String, _ name: String, _ value: [UInt8]) throws { try Data(value).write(to: URL(fileURLWithPath: directory+"/"+name)) }
// Replay malformed/dynamic Huffman/history fixtures at exact and one-tight byte caps.
func fixtures(_ directory: String) throws -> Int {
    let rows = try String(contentsOfFile: directory+"/fixtures.tsv", encoding: .utf8).split(whereSeparator: { $0.isNewline })
    for row in rows {
        let cells = row.split(separator: "\t").map(String.init)
        let format = formats.first { $0.0 == cells[0] }!.1; let packed = try bytes(directory, cells[2])
        if cells[1] == "invalid" { reject(.invalidData) { _ = try C.decompress(packed, options: .init(format: format)) } }
        else {
            let plain = try bytes(directory, cells[3])
            require(try C.decompress(packed, options: .init(format: format, maxInputBytes: packed.count, maxOutputBytes: plain.count)) == plain, cells[2])
            if !packed.isEmpty { reject(.resourceLimit) { _ = try C.decompress(packed, options: .init(format: format, maxInputBytes: packed.count-1)) } }
            if !plain.isEmpty { reject(.resourceLimit) { _ = try C.decompress(packed, options: .init(format: format, maxOutputBytes: plain.count-1)) } }
        }
    }
    return rows.count
}
// Exercise ordinary generated model APIs for every native protocol and independent output oracle.
func models(_ directory: String) throws {
    var value = AccountRecord(); value.accountId = 42; value.label = "owning 😀"
    for proto in Protocol.allCases {
        let native = try value.encode(proto); try save(directory, "native-\(proto).bin", native)
        for (name, format) in formats {
            let packed = try C.encode({ try value.encode(proto) }, options: .init(format: format))
            let result = try C.decode(packed, decoder: { try AccountRecord.decode($0, proto) }, options: .init(format: format))
            require(try result.encode(proto) == native, "Native bytes changed")
            try save(directory, "output-\(name)-\(proto).bin", packed)
            do {
                _ = try C.decode(C.compress(native+[0], options: .init(format: format)), decoder: { try AccountRecord.decode($0, proto) }, options: .init(format: format))
                fatalError("Accepted native unread suffix")
            } catch is SerializerError { }
        }
    }
    var armed = false; let cancellation = Marker("late candidate")
    let options = C.DecodeOptions(checkCancelled: { if armed { throw cancellation } })
    same(cancellation) {
        _ = try C.decode(value.encode(.BINARY_NONE), decoder: { let candidate = try AccountRecord.decode($0,.BINARY_NONE); armed = true; return candidate }, options: options)
    }
}
// Missing optional providers and invalid policies reject before application work.
func admission() {
    for format: C.Format in [.zstd,.lz4] {
        var calls = 0
        let encode = C.EncodeOptions(format: format, checkCancelled: { calls += 1 })
        let decode = C.DecodeOptions(format: format, checkCancelled: { calls += 1 })
        require(!C.available(format), "Missing optional provider available")
        reject(.unavailable) { _ = try C.encode({ calls += 1; return [] }, options: encode) }
        reject(.unavailable) { _ = try C.decompressFrom({ _ in calls += 1; return nil }, options: decode) }
        reject(.unavailable) { _ = try C.Output(sink: { _ in calls += 1 }, options: encode) }
        reject(.unavailable) { _ = try C.Input(source: { _ in calls += 1; return nil }, options: decode) }
        require(calls == 0, "Unavailable provider touched callback")
    }
    reject(.invalidOptions) { _ = try C.compress([], options: .init(maxInputBytes: -1)) }
    reject(.invalidOptions) { _ = try C.compress([], options: .init(level: 10)) }
    reject(.invalidOptions) { _ = try C.decompress([], options: .init(maxWindowBytes: -1)) }
    var calls = 0
    reject(.resourceLimit) { _ = try C.decompressFrom({ _ in calls += 1; return nil }, options: .init(format: .gzip, maxWindowBytes: 32767, checkCancelled: { calls += 1 })) }
    require(calls == 0, "Window admission touched source")
}
// Check native stored/fixed encoding, exact/empty bounds and independent provider consumption.
func bounds(_ directory: String, _ payload: [UInt8]) throws {
    for (name, format) in formats {
        let packed = try C.compress(payload, options: .init(format: format))
        require(try C.decompress(packed, options: .init(format: format, maxInputBytes: packed.count, maxOutputBytes: payload.count)) == payload, "Large roundtrip")
        require(try C.compress(payload, options: .init(format: format, maxInputBytes: payload.count, maxOutputBytes: packed.count)) == packed, "Exact encoded limit")
        reject(.resourceLimit) { _ = try C.compress(payload, options: .init(format: format, maxInputBytes: payload.count-1)) }
        if !packed.isEmpty { reject(.resourceLimit) { _ = try C.compress(payload, options: .init(format: format, maxOutputBytes: packed.count-1)) } }
        let empty = try C.compress([], options: .init(format: format, maxInputBytes: 0, maxOutputBytes: 64))
        require(try C.decompress(empty, options: .init(format: format, maxOutputBytes: 0)).isEmpty, "Empty bound")
        for level in [0,1,6,9] {
            let input = Array(payload.prefix(70001)); let encoded = try C.compress(input, options: .init(format: format, level: level))
            require(try C.decompress(encoded, options: .init(format: format)) == input, "Encoding level")
            try save(directory, "level-\(name)-\(level).bin", encoded)
        }
        let history = Array(payload.prefix(32768))
        for (sample, input) in [("repeat",[UInt8](repeating: 65,count: 262145)),
                                ("history",history+history+Array(history.prefix(257)))] {
            for level in [0,1,6,9] {
                let encoded = try C.compress(input, options: .init(format: format,level: level))
                require(try C.decompress(encoded, options: .init(format: format)) == input, "LZ77 history/overlap control")
                try save(directory,"sample-\(name)-\(sample)-\(level).bin",encoded)
            }
        }
        var polls = 0
        _ = try C.decompress(packed, options: .init(format: format, checkCancelled: { polls += 1 }))
        require(polls > 0, "No decode cancellation")
        let original = Marker("decode cancellation")
        for stop in [1,Swift.max(1,polls/2),polls] {
            var seen = 0
            same(original) { _ = try C.decompress(packed, options: .init(format: format, checkCancelled: { seen += 1; if seen == stop { throw original } })) }
        }
    }
}
// Observe short bounded source calls, true EOF, explicit finish and abandoned ownership.
func lifecycle(_ payload: [UInt8]) throws {
    for (_, format) in formats {
        let packed = try C.compress(payload, options: .init(format: format)); var offset = 0; var requests = 0; var maximum = 0
        let source: (Int) throws -> [UInt8]? = { request in
            requests += 1; maximum = Swift.max(maximum,request)
            if offset == packed.count { return nil }
            let count = Swift.min(7,Swift.min(request,packed.count-offset)); let result = Array(packed[offset..<(offset+count)]); offset += count; return result
        }
        let input = try C.Input(source: source, options: .init(format: format))
        require(try input.read(0) == [], "Zero read"); require(requests == 0, "Zero read borrowed source")
        var plain: [UInt8] = []
        while let block = try input.read(13) { plain.append(contentsOf: block) }
        require(plain == payload && maximum <= 32768, "Short source output/batch")
        try input.finish(); reject(.invalidOptions) { _ = try input.read() }; input.close()
        var sink: [UInt8] = []; var writes = 0
        let output = try C.Output(sink: { block in require(block.count <= 32768, "Sink batch"); sink += block; writes += 1 }, options: .init(format: format))
        try output.write(payload); try output.flush(); require(writes == 0, "Implicit output publication")
        try output.finish(); require(try C.decompress(sink, options: .init(format: format)) == payload, "Borrowed output")
        reject(.invalidOptions) { try output.finish() }; output.close()
        let original = Marker("sink failure"); var calls = 0
        let failed = try C.Output(sink: { _ in calls += 1; throw original }, options: .init(format: format))
        try failed.write([1]); same(original) { try failed.finish() }; reject(.invalidOptions) { try failed.finish() }; require(calls == 1, "Failed sink reused")
        calls = 0
        let failedInput = try C.Input(source: { _ in calls += 1; throw original }, options: .init(format: format))
        same(original) { _ = try failedInput.read() }; reject(.invalidOptions) { try failedInput.finish() }
        require(calls == 1, "Failed source reused")
        var armed = false; let cancelled = Marker("staged input cancellation"); var delivered = false
        let staged = try C.Input(source: { _ in if delivered { return nil }; delivered = true; return try C.compress([1], options: .init(format: format)) }, options: .init(format: format, checkCancelled: { if armed { throw cancelled } }))
        require(try staged.read() == [1], "Staged bytes"); armed = true
        same(cancelled) { try staged.finish() }; reject(.invalidOptions) { _ = try staged.read() }
    }
    var request = 0
    reject(.resourceLimit) { _ = try C.decompressFrom({ count in request = count; return [UInt8](repeating: 0, count: count) }, options: .init(maxInputBytes: 10)) }
    require(request == 11, "Input overflow probe")
    reject(.invalidData) { _ = try C.decompressFrom({ _ in [] }) }
    var called = false; let abandoned = try C.Output(sink: { _ in called = true })
    try abandoned.write([1]); abandoned.close(); require(!called, "Close implicitly finished")
}
// Callback close/reentry must fence the outer call even if the nested failure is swallowed.
func reentrancy() throws {
    for (_, format) in formats {
        var output: C.Output!; var armed = false; var writes = 0
        output = try C.Output(sink: { _ in writes += 1 }, options: .init(format: format, checkCancelled: { if armed { output.close() } }))
        try output.write([1]); armed = true; reject(.invalidOptions) { try output.finish() }; require(writes == 0, "Closed policy published")
        for phase in 0..<2 {
            armed = false
            output = try C.Output(sink: { _ in writes += 1 }, options: .init(format: format, checkCancelled: { if armed { output.close() } }))
            armed = true
            reject(.invalidOptions) { if phase == 0 { try output.write([1]) } else { try output.flush() } }
            require(writes == 0, "Closed write/flush published")
        }
        armed = false; var first: (any Swift.Error)?
        let later = Marker("later callback error")
        output = try C.Output(sink: { _ in writes += 1 }, options: .init(format: format, checkCancelled: {
            if armed { armed = false; do { try output.flush() } catch { first = error }; throw later }
        }))
        armed = true
        do { try output.write([1]); fatalError("Reentrant output accepted") }
        catch { require(first != nil && error as AnyObject === first as AnyObject?, "Active output lost first error") }
        do { try output.finish(); fatalError("Inactive output reusable") } catch let failure as C.Failure { require(failure.cause as AnyObject? === first as AnyObject?, "First cause lost") }
        var input: C.Input!; armed = false; var reads = 0
        input = try C.Input(source: { _ in reads += 1; return nil }, options: .init(format: format, checkCancelled: { if armed { input.close() } }))
        armed = true; reject(.invalidOptions) { _ = try input.read() }; require(reads == 0, "Closed policy read source")
        armed = false; first = nil
        input = try C.Input(source: { _ in reads += 1; return nil }, options: .init(format: format, checkCancelled: {
            if armed { armed = false; do { _ = try input.read() } catch { first = error } }
        }))
        armed = true
        do { _ = try input.read(); fatalError("Reentrant input accepted") }
        catch { require(first != nil && error as AnyObject === first as AnyObject? && reads == 0, "Active input lost first error") }
        var delivered = false; armed = false
        let empty = try C.compress([], options: .init(format: format))
        input = try C.Input(source: { _ in if delivered || empty.isEmpty { return nil }; delivered = true; return empty }, options: .init(format: format, checkCancelled: { if armed { input.close() } }))
        require(try input.read() == nil, "Empty staged EOF"); armed = true
        reject(.invalidOptions) { try input.finish() }
    }
    var output: C.Output!; var writes = 0
    output = try C.Output(sink: { _ in writes += 1; output.close() }); try output.write([UInt8](repeating: 1, count: 32769))
    reject(.invalidOptions) { try output.finish() }; require(writes == 1, "Close continued sink batches")
    var input: C.Input!; var reads = 0
    input = try C.Input(source: { _ in reads += 1; input.close(); return [1] })
    reject(.invalidOptions) { _ = try input.read() }; require(reads == 1, "Close continued source calls")
}
let directory = CommandLine.arguments[1]
let decisions = try fixtures(directory); try models(directory); admission()
let payload = try bytes(directory,"plain.bin"); try bounds(directory,payload); try lifecycle(payload); try reentrancy()
print("PASS Swift compression: \(decisions) independent decisions, native bytes, bounds and borrowed lifecycle")
'''


REMOTE = r'''import base64,json,os,pathlib,subprocess,sys,tarfile,tempfile
with tempfile.TemporaryDirectory(prefix='serializer-swift-compression-') as temporary:
 root=pathlib.Path(temporary)
 os.chdir(root)
 os.environ['PATH']='/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin'
 with tarfile.open(fileobj=sys.stdin.buffer,mode='r|gz') as archive:
  for member in archive:
   target=(root/member.name).resolve()
   if not member.isfile() or not target.is_relative_to(root):raise RuntimeError('Unexpected SDK payload member')
   target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(archive.extractfile(member).read())
 sdk=sys.argv[1]
 version=subprocess.check_output([sdk,'--version'],text=True).strip()
 for profile in ('normal','collision','binary'):
  directory=root/profile
  subprocess.run([sdk,'-O','-warnings-as-errors','-swift-version','6',str(directory/'Schema.swift'),str(directory/'main.swift'),'-o',str(directory/'consumer')],check=True)
  subprocess.run([str(directory/'consumer'),str(directory)],check=True)
 files={path.name:base64.b64encode(path.read_bytes()).decode() for path in (root/'normal').glob('*.bin') if path.name.startswith(('native-','output-','level-','sample-'))}
 print('SWIFT_RESULT '+json.dumps({'sdk':version,'files':files}),flush=True)
'''


def generate(compiler, directory, schema=SCHEMA, flags=()):
    """Emit actual Swift and discover the chosen helper without mutating native model names."""
    source, output = directory / 'model.serializer', directory / 'Schema.swift'
    source.write_text(schema, encoding='utf-8')
    subprocess.run([str(compiler), '--input', str(source), '--language', 'swift',
                    '--swift.output', str(output), *flags], check=True)
    data = output.read_bytes()
    marker = re.search(r'Generated Swift compression helper name: (\w+)', data.decode())
    if not marker:
        raise AssertionError('Actual emitted Swift compression helper is missing')
    return marker.group(1), {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def smoke(helper, collision=False, binary=False):
    """Exercise preserved schema models beside the helper and disabled native protocol selection."""
    result = ('import Foundation\ntypealias C = ' + helper + '\nvar value = AccountRecord(); value.accountId = 7\n'
              'let native = try value.encode(.BINARY_NONE)\n'
              'let packed = try C.encode({native}, options: .init(format: .gzip))\n'
              'let candidate = try C.decode(packed,decoder: {try AccountRecord.decode($0,.BINARY_NONE)},options: .init(format: .gzip))\n'
              'precondition(candidate.accountId == 7)\n')
    if collision:
        for model in ('SerializerCompression', 'SerializerCompression2'):
            result += f'let preserved{model} = try {model}().encode(.BINARY_NONE)\n_ = try {model}.decode(preserved{model},.BINARY_NONE)\n'
    if binary:
        result += 'do { _ = try value.encode(.JSON); fatalError("Disabled protocol accepted") } catch is SerializerError {}\n'
    return result + 'print("PASS Swift generated profile")\n'


def execute(directory, swiftc, wsl):
    """Compile/run locally or stream exact sources into isolated native WSL storage without mounted I/O."""
    if wsl:
        payload = io.BytesIO()
        with tarfile.open(fileobj=payload, mode='w:gz') as archive:
            for path in sorted(directory.rglob('*')):
                if path.is_file():
                    archive.add(path, arcname=path.relative_to(directory).as_posix())
        process = subprocess.run(['wsl.exe', '--exec', 'python3', '-c', REMOTE, swiftc],
                                 input=payload.getvalue(), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 check=False, timeout=300)
        text = process.stdout.decode(errors='replace')
        prefix, marker, report = text.partition('SWIFT_RESULT ')
        print(prefix, end='', flush=True)
        if process.returncode or not marker:
            raise RuntimeError('Actual native WSL Swift qualification failed: ' + str(process.returncode))
        result = json.loads(report)
        for name, data in result.pop('files').items():
            if Path(name).name != name:
                raise AssertionError('Unexpected native SDK output name')
            (directory / 'normal' / name).write_bytes(base64.b64decode(data))
        return result['sdk']
    version = subprocess.check_output([swiftc, '--version'], text=True).strip()
    for profile in ('normal', 'collision', 'binary'):
        case = directory / profile
        subprocess.run([swiftc, '-O', '-warnings-as-errors', '-swift-version', '6',
                        str(case / 'Schema.swift'), str(case / 'main.swift'), '-o', str(case / 'consumer')], check=True, timeout=180)
        subprocess.run([str(case / 'consumer'), str(case)], check=True, timeout=180)
    return version


def oracle(directory, payload):
    """Use independent zlib to verify exact standard frame EOF and unchanged native payloads."""
    expected_names = {'output-' + name + '-' + protocol + '.bin'
                      for name in ('NONE', 'GZIP', 'ZLIB', 'DEFLATE')
                      for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')}
    expected_names.update('level-' + name + '-' + str(level) + '.bin'
                          for name in ('NONE', 'GZIP', 'ZLIB', 'DEFLATE') for level in (0, 1, 6, 9))
    expected_names.update('sample-' + name + '-' + sample + '-' + str(level) + '.bin'
                          for name in ('NONE', 'GZIP', 'ZLIB', 'DEFLATE')
                          for sample in ('repeat', 'history') for level in (0, 1, 6, 9))
    actual_names = {path.name for path in directory.glob('*.bin')
                    if path.name.startswith(('output-', 'level-', 'sample-'))}
    if actual_names != expected_names:
        raise AssertionError('Missing or unexpected Swift independent oracle output')
    decisions = 0
    for path in sorted(directory.glob('*.bin')):
        if not path.name.startswith(('output-', 'level-', 'sample-')):
            continue
        kind, name, selector = path.stem.split('-', 2)
        if kind == 'sample':
            expected = (b'A' * 262145 if selector.startswith('repeat-')
                        else payload[:32768] * 2 + payload[:257])
        else:
            expected = payload[:70001] if kind == 'level' else (directory / ('native-' + selector + '.bin')).read_bytes()
        packed = path.read_bytes()
        if name == 'NONE':
            plain = packed
        else:
            decoder = zlib.decompressobj({'GZIP': 31, 'ZLIB': 15, 'DEFLATE': -15}[name])
            plain = decoder.decompress(packed) + decoder.flush()
            if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                raise AssertionError('Swift output is not exactly one standard frame')
        if plain != expected:
            raise AssertionError('Swift standard frame changed original native bytes')
        decisions += 1
    if json.loads((directory / 'native-JSON.bin').read_bytes())['account_id'] != 42:
        raise AssertionError('Ordinary native JSON field identity changed')
    return decisions


def main():
    """Qualify actual emitted Swift with mandatory explicit local SDK execution and retained provenance."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--swiftc')
    parser.add_argument('--wsl', action='store_true', help='Use native WSL Swift; --swiftc is its Linux executable path')
    args = parser.parse_args()
    args.swiftc = args.swiftc or ('/usr/bin/swiftc' if args.wsl else shutil.which('swiftc'))
    if not args.swiftc:
        parser.error('An actual Swift SDK is required')
    root = Path(__file__).resolve().parent.parent
    build = args.build.resolve()
    if build == root or root in build.parents and 'out' not in build.relative_to(root).parts:
        parser.error('Repository output must remain below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    profiles = {}
    with tempfile.TemporaryDirectory(prefix='actual-', dir=build) as temporary:
        directory = Path(temporary)
        for name, schema, flags in (
                ('normal', SCHEMA, ()),
                ('collision', SCHEMA+' class SerializerCompression {} class SerializerCompression_2 {}', ()),
                ('binary', SCHEMA, ('--protocols','binary_none'))):
            case = directory / name; case.mkdir()
            helper, profiles[name] = generate(args.compiler.resolve(), case, schema, flags)
            if name == 'normal':
                count = common_fixtures(case)+extra_fixtures(case)
                payload = (case / 'plain.bin').read_bytes()
                source = SWIFT.replace('__COMPRESSION_NAME__', helper)
            else:
                if name == 'collision' and helper != 'SerializerCompression_2':
                    raise AssertionError('Swift helper changed existing normalized model names')
                source = smoke(helper, collision=name == 'collision', binary=name == 'binary')
            (case / 'main.swift').write_text(source, encoding='utf-8')
        sdk = execute(directory, args.swiftc, args.wsl)
        outputs = oracle(directory / 'normal', payload)
    report = {'swift': sdk, 'language_mode': 'Swift6', 'native_wsl': args.wsl,
              'independent_input_decisions': count, 'independent_output_decisions': outputs,
              'profiles': profiles, 'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'test_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (build / 'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2), flush=True)
    print('PASS actual generated Swift compression, independent framing and lifecycle', flush=True)


if __name__ == '__main__':
    main()
