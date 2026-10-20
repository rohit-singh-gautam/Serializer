# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual Kotlin/JVM17 optional compression deployments and independent native frames."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'test'))
import kotlin_compression_test as core
from java_compression_optional_test import fixtures as optional_fixtures
from go_compression_optional_test import native_fixtures


EXTRA = r'''
// Replay independently authored exact-frame decisions with explicit compressed/expanded/history bounds.
fun optionalFixtures(directory: Path): Int {
    var count = 0
    for (format in formats) for (line in Files.readAllLines(directory.resolve("$format.tsv"))) {
        val row = line.split('\t'); val frame = Files.readAllBytes(directory.resolve(row[0]))
        val policy = C.DecodeOptions(format, row[3].toInt(), row[4].toInt(), row[5].toInt())
        if (row[2] == "ok") {
            val expected = Files.readAllBytes(directory.resolve(row[1]))
            requireThat(C.decompress(frame, policy).contentEquals(expected), row[0])
        } else reject(C.Code.valueOf(row[2])) { C.decompress(frame, policy) }
        ++count
    }
    return count
}
// Probe full deployment absence and native policy admission before model, cancellation or borrowed I/O.
fun providerAdmission(format: C.Format, present: Boolean) {
    requireThat(C.available(format) == present, "Provider availability: $format")
    var calls = 0; val source = Source(byteArrayOf()); val sink = Sink()
    if (!present) {
        val encode = C.EncodeOptions(format, checkCancelled = { ++calls })
        val decode = C.DecodeOptions(format, checkCancelled = { ++calls })
        reject(C.Code.UNAVAILABLE) { C.encode({ ++calls; byteArrayOf() }, encode) }
        reject(C.Code.UNAVAILABLE) { C.decompress(source, decode) }
        reject(C.Code.UNAVAILABLE) { C.Input(source, decode) }
        reject(C.Code.UNAVAILABLE) { C.Output(sink, encode) }
    } else {
        val minimum = if (format == C.Format.ZSTD) 1023 else 65535
        reject(C.Code.RESOURCE_LIMIT) { C.decompress(source,
            C.DecodeOptions(format, maxWindowBytes = minimum, checkCancelled = { ++calls })) }
        val range = if (format == C.Format.ZSTD) {
            val api = java.lang.Class.forName("com.github.luben.zstd.Zstd")
            (api.getMethod("minCompressionLevel").invoke(null) as Int)..
                (api.getMethod("maxCompressionLevel").invoke(null) as Int)
        } else 0..9
        for (level in listOf(range.first - 1, range.last + 1)) {
            reject(C.Code.INVALID_OPTIONS) { C.encode({ ++calls; byteArrayOf() },
                C.EncodeOptions(format, level = level, checkCancelled = { ++calls })) }
        }
        requireThat(calls == 0 && source.calls == 0, "Provider policy touched application")
        for (level in listOf(range.first, 0, range.last)) {
            val frame = C.compress(byteArrayOf(1), C.EncodeOptions(format, level = level))
            requireThat(C.decompress(frame, C.DecodeOptions(format)).contentEquals(byteArrayOf(1)), "Native level $level")
        }
    }
    requireThat(calls == 0 && source.calls == 0 && sink.calls == 0, "Admission side effects")
}
// Retain independent output bytes for every selected level, including empty and multichunk messages.
fun outputLevels(directory: Path) {
    for (format in formats) for (level in listOf(0, 1, 6, 9)) for (sample in 0..1) {
        val data = if (sample == 0) byteArrayOf() else ByteArray(65537) { 65 }
        Files.write(directory.resolve("level-$format-$level-$sample.bin"),
            C.compress(data, C.EncodeOptions(format, level = level)))
    }
}
// Cancellation at every observed optional-provider boundary must retain the original exception object.
fun providerCancellation(payload: ByteArray) {
    for (format in formats) {
        val frame = C.compress(payload, C.EncodeOptions(format))
        for (encoding in listOf(false, true)) {
            var observed = 0
            if (encoding) C.compress(payload, C.EncodeOptions(format, checkCancelled = { ++observed }))
            else C.decompress(frame, C.DecodeOptions(format, checkCancelled = { ++observed }))
            val original = java.io.IOException("original optional cancellation")
            for (boundary in 1..observed) {
                var count = 0; val poll = { if (++count == boundary) throw original }
                same(original) {
                    if (encoding) C.compress(payload, C.EncodeOptions(format, checkCancelled = poll))
                    else C.decompress(frame, C.DecodeOptions(format, checkCancelled = poll))
                }
            }
        }
    }
}
// Use one compiled generated module for both complete and single/absent-provider deployments.
fun main(args: Array<String>) {
    if (args[0] == "availability") {
        for (item in args.drop(1)) {
            val pair = item.split('='); providerAdmission(C.Format.valueOf(pair[0]), pair[1] == "true")
        }
        println("{\"admission\":true}"); return
    }
    val directory = Path.of(args[0])
    for (format in formats) providerAdmission(format, true)
    val count = optionalFixtures(directory); models(directory)
    val payload = Files.readAllBytes(directory.resolve("plain.bin"))
    bounds(payload); lifecycle(payload); reentrancy(); providerCancellation(payload); outputLevels(directory)
    println("{\"input_decisions\":$count,\"independent_outputs\":24}")
}
'''


def consumer(helper):
    """Reuse the established model/lifecycle/reentry controls, replacing only format admission/corpus."""
    code = core.KOTLIN[:core.KOTLIN.index('// Run actual generated owning codecs')]
    begin, end = code.index('// Run one common independent'), code.index('// Existing owning model')
    code = code[:begin] + code[end:]
    begin, end = code.index('// Provider absence'), code.index('// Bound complete messages')
    code = code[:begin] + code[end:]
    code = code.replace('listOf(C.Format.NONE, C.Format.GZIP, C.Format.ZLIB, C.Format.DEFLATE)',
                        'listOf(C.Format.ZSTD, C.Format.LZ4)')
    return (code + EXTRA).replace('__COMPRESSION_NAME__', helper)


def run(command, directory, environment):
    """Capture explicit SDK/provider invocations and require successful completion."""
    command = [str(value) for value in command]
    print('RUN', subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=directory, env=environment, capture_output=True,
                            text=True, timeout=300)
    print(result.stdout+result.stderr, end='', flush=True)
    result.check_returncode()
    return result.stdout


def verify_outputs(directory, cpp_helper, environment):
    """Require exact independent Python and native C++ consumption of all produced standard frames."""
    from compression import zstd
    import lz4.frame
    count = 0
    for name in ('ZSTD', 'LZ4'):
        cases = [(directory/f'output-{name}-{protocol}.bin', (directory/f'native-{protocol}.bin').read_bytes())
                 for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')]
        cases += [(directory/f'level-{name}-{level}-{sample}.bin', b'A'*65537 if sample else b'')
                  for level in (0, 1, 6, 9) for sample in (0, 1)]
        for path, expected in cases:
            frame = path.read_bytes()
            decoder = zstd.ZstdDecompressor() if name == 'ZSTD' else lz4.frame.LZ4FrameDecompressor()
            if decoder.decompress(frame) != expected or not decoder.eof or decoder.unused_data:
                raise AssertionError('Independent Kotlin output differs: '+path.name)
            if not frame[4] & 4:
                raise AssertionError('Optional output content checksum absent')
            for mode in ('decode', 'stream_decode'):
                destination = directory/(path.stem+'-'+mode+'.plain')
                run([cpp_helper, mode, name.lower(), path, destination], directory, environment)
                if destination.read_bytes() != expected:
                    raise AssertionError('Native C++ consumer differs: '+path.name)
            count += 1
    return count


def main():
    """Compile without provider jars, then qualify explicit absent/single/both deployments and collisions."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--kotlinc', required=True, type=Path)
    parser.add_argument('--java', required=True, type=Path)
    parser.add_argument('--zstd-jar', required=True, type=Path)
    parser.add_argument('--lz4-jar', required=True, type=Path)
    parser.add_argument('--provider-path', type=Path)
    parser.add_argument('--cpp-helper', required=True, type=Path)
    parser.add_argument('--cpp-linked-helper', required=True, type=Path)
    args = parser.parse_args()
    if args.provider_path:
        sys.path.insert(0, str(args.provider_path.resolve()))
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and build.relative_to(ROOT).parts[0] != 'out':
        parser.error('In-repository output must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    temporary = build/'temporary'; temporary.mkdir(exist_ok=True)
    environment['TMP'] = environment['TEMP'] = str(temporary)
    args.java, args.kotlinc = args.java.resolve(), args.kotlinc.resolve()
    environment['JAVA_HOME'] = str(args.java.parent.parent)
    java = subprocess.check_output([str(args.java), '-version'], stderr=subprocess.STDOUT, text=True)
    if not re.search(r'version "17\.', java):
        parser.error('This profile requires actual JDK17 execution')
    jars = {'ZSTD': args.zstd_jar.resolve(), 'LZ4': args.lz4_jar.resolve()}
    counts = optional_fixtures(build, tuple(jars))
    args.cpp_helper, args.cpp_linked_helper = args.cpp_helper.resolve(), args.cpp_linked_helper.resolve()
    native = native_fixtures(args, build, environment, counts)
    helper, size, digest = core.generate(args.compiler.resolve(), build)
    (build/'Main.kt').write_text(consumer(helper), encoding='utf-8')
    run([args.kotlinc, '-Werror', '-jvm-target', '17', '-include-runtime', '-d', build/'app.jar',
         build/'Schema.kt', build/'Main.kt'], build, environment)
    deployments = {}
    for names in ((), ('ZSTD',), ('LZ4',), ('ZSTD', 'LZ4')):
        classpath = os.pathsep.join(str(value) for value in (build/'app.jar', *(jars[name] for name in names)))
        arguments = [name+'='+str(name in names).lower() for name in jars]
        run([args.java, '-cp', classpath, 'MainKt', 'availability', *arguments], build, environment)
        deployments['+'.join(names) or 'absent'] = True
    classpath = os.pathsep.join(map(str, (build/'app.jar', *jars.values())))
    # A present jar whose native library cannot load must fail during admission,
    # before cancellation, source, or sink callbacks can observe the operation.
    missing_native = build/'absent-native-provider.dll'
    if missing_native.exists():
        raise AssertionError('Missing-native admission probe must name an absent file')
    run([args.java, '-DZstdNativePath='+str(missing_native), '-cp', classpath,
         'MainKt', 'availability', 'ZSTD=false', 'LZ4=true'], build, environment)
    deployments['ZSTD-native-unavailable'] = True
    output = run([args.java, '-cp', classpath, 'MainKt', build], build, environment)
    result = json.loads(output.strip().splitlines()[-1])
    if result['input_decisions'] != sum(counts.values()) or verify_outputs(build, args.cpp_helper, environment) != 24:
        raise AssertionError('Incomplete Kotlin optional corpus')
    profiles = {'normal': {'generated_bytes': size, 'generated_sha256': digest}}
    for name, schema, flags in (
            ('collision', core.SCHEMA+' class SerializerCompression {} class SerializerCompression_2 {}', ()),
            ('builtins', core.SCHEMA+' class Inflater {} class Deflater {} class InputStream {} class OutputStream {} class Throwable {}', ()),
            ('binary', core.SCHEMA, ('--protocols', 'binary_none'))):
        case = build/name; case.mkdir(exist_ok=True)
        family, size, digest = core.generate(args.compiler.resolve(), case, schema, flags)
        smoke = core.smoke_source(family, name).replace('C.Format.GZIP', 'C.Format.ZSTD')
        (case/'Main.kt').write_text(smoke, encoding='utf-8')
        run([args.kotlinc, '-Werror', '-jvm-target', '17', '-include-runtime', '-d', case/'app.jar',
             case/'Schema.kt', case/'Main.kt'], case, environment)
        run([args.java, '-cp', os.pathsep.join(map(str, (case/'app.jar', *jars.values()))), 'MainKt'], case, environment)
        profiles[name] = {'generated_bytes': size, 'generated_sha256': digest}
    report = {'java': java.strip(), 'kotlin': subprocess.check_output([str(args.kotlinc), '-version'],
              env=environment, stderr=subprocess.STDOUT, text=True).strip(), 'input_decisions': counts,
              'independent_outputs': 24, 'native_cpp_consumers': 48, 'native_cpp_producers': native,
              'deployments': deployments, 'profiles': profiles,
              'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'test_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'provider_jars': {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in jars.items()}}
    (build/'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
