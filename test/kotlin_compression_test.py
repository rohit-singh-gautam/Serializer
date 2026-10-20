# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual Kotlin/JVM 17 compression output against independent standard frames."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zlib

from java_compression_test import fixtures as common_fixtures
from csharp_compression_test import extra_fixtures


SCHEMA = 'serializer version 1; class account_record stable_ids { public uint32 account_id (1); public string label (2); }'

KOTLIN = r'''import java.nio.file.Files
import java.nio.file.Path
import __COMPRESSION_NAME__ as C

// Keep qualification active without optional JVM assertions.
fun requireThat(value: Boolean, message: String) { if (!value) throw AssertionError(message) }
// Distinguish stable compression categories from native, callback and transport failures.
fun reject(code: C.Code, operation: () -> Unit) {
    try { operation() } catch (failure: C.Failure) { requireThat(failure.code == code, "Wrong category: ${failure.code}"); return }
    throw AssertionError("Accepted $code")
}
// Original callback and transport exception objects must survive every cleanup path.
fun same(expected: Throwable, operation: () -> Unit) {
    try { operation() } catch (actual: Throwable) { requireThat(actual === expected, "Original failure replaced: $actual"); return }
    throw AssertionError("Expected original failure")
}
// Inactive wrappers reject before another source/sink callback.
fun fenced(operation: () -> Unit) {
    try { operation() } catch (_: IllegalStateException) { return }
    throw AssertionError("Inactive wrapper was reusable")
}
// Observe bounded short reads, explicit EOF and retained borrowed ownership.
class Source(bytes: ByteArray, private val batch: Int = Int.MAX_VALUE) : java.io.ByteArrayInputStream(bytes) {
    var calls = 0; var maximum = 0; var closes = 0
    var failure: Throwable? = null; var afterRead: (() -> Unit)? = null
    override fun read(data: ByteArray, offset: Int, length: Int): Int {
        ++calls; maximum = maxOf(maximum, length); failure?.let { throw it }
        val count = super.read(data, offset, minOf(batch, length)); afterRead?.invoke(); return count
    }
    override fun close() { ++closes }
}
// Consume independent bounded writes while detecting unwanted close/flush propagation.
class Sink : java.io.ByteArrayOutputStream() {
    var calls = 0; var maximum = 0; var closes = 0; var flushes = 0
    var failure: Throwable? = null; var afterWrite: (() -> Unit)? = null
    override fun write(data: ByteArray, offset: Int, length: Int) {
        ++calls; maximum = maxOf(maximum, length); failure?.let { throw it }
        super.write(data, offset, length); afterWrite?.invoke()
    }
    override fun close() { ++closes }
    override fun flush() { ++flushes }
}
val formats = listOf(C.Format.NONE, C.Format.GZIP, C.Format.ZLIB, C.Format.DEFLATE)

// Run one common independent valid/invalid corpus at exact and one-tight bounds.
fun fixtures(directory: Path): Int {
    var decisions = 0
    for (line in Files.readAllLines(directory.resolve("fixtures.tsv"))) {
        val row = line.split('\t'); val format = C.Format.valueOf(row[0])
        val packed = Files.readAllBytes(directory.resolve(row[2]))
        if (row[1] == "invalid") reject(C.Code.INVALID_DATA) { C.decompress(packed, C.DecodeOptions(format)) }
        else {
            val plain = Files.readAllBytes(directory.resolve(row[3]))
            requireThat(C.decompress(packed, C.DecodeOptions(format, packed.size, plain.size)).contentEquals(plain), row[2])
            if (plain.isNotEmpty()) reject(C.Code.RESOURCE_LIMIT) { C.decompress(packed, C.DecodeOptions(format, maxOutputBytes = plain.size - 1)) }
            if (packed.isNotEmpty()) reject(C.Code.RESOURCE_LIMIT) { C.decompress(packed, C.DecodeOptions(format, maxInputBytes = packed.size - 1)) }
        }
        ++decisions
    }
    return decisions
}
// Existing owning model methods supply all four unchanged native payload representations.
fun models(directory: Path) {
    val value = AccountRecord(); value.accountId = 42u; value.label = "owning \uD83D\uDE00"
    for (protocol in Protocol.entries) {
        val native = value.encode(protocol); Files.write(directory.resolve("native-$protocol.bin"), native)
        for (format in formats) {
            val packed = C.encode({ value.encode(protocol) }, C.EncodeOptions(format))
            val candidate = C.decode(packed, { AccountRecord.decode(it, protocol) }, C.DecodeOptions(format))
            requireThat(candidate.encode(protocol).contentEquals(native), "Native object changed")
            Files.write(directory.resolve("output-$format-$protocol.bin"), packed)
            try {
                C.decode(C.compress(native + byteArrayOf(0), C.EncodeOptions(format)), { AccountRecord.decode(it, protocol) }, C.DecodeOptions(format))
                throw AssertionError("Accepted inner codec suffix")
            } catch (_: IllegalArgumentException) { }
        }
    }
    var armed = false; val original = IllegalStateException("candidate cancellation")
    val policy = C.DecodeOptions(checkCancelled = { if (armed) throw original })
    same(original) { C.decode(value.encode(Protocol.BINARY_NONE), { val candidate = AccountRecord.decode(it, Protocol.BINARY_NONE); armed = true; candidate }, policy) }
}
// Provider absence and inadequate windows reject before callbacks, native codecs or transport I/O.
fun admission() {
    for (format in listOf(C.Format.ZSTD, C.Format.LZ4)) {
        var calls = 0; val source = Source(byteArrayOf()); val sink = Sink()
        val encode = C.EncodeOptions(format, checkCancelled = { ++calls })
        val decode = C.DecodeOptions(format, checkCancelled = { ++calls })
        requireThat(!C.available(format), "Unavailable optional provider")
        reject(C.Code.UNAVAILABLE) { C.encode({ ++calls; byteArrayOf() }, encode) }
        reject(C.Code.UNAVAILABLE) { C.decompress(source, decode) }
        reject(C.Code.UNAVAILABLE) { C.Output(sink, encode) }
        reject(C.Code.UNAVAILABLE) { C.Input(source, decode) }
        requireThat(calls == 0 && source.calls == 0 && sink.calls == 0, "Unavailable provider touched application")
    }
    reject(C.Code.INVALID_OPTIONS) { C.EncodeOptions(maxInputBytes = -1) }
    reject(C.Code.INVALID_OPTIONS) { C.EncodeOptions(level = 10) }
    reject(C.Code.INVALID_OPTIONS) { C.DecodeOptions(maxWindowBytes = -1) }
    var polls = 0; val source = Source(byteArrayOf())
    reject(C.Code.RESOURCE_LIMIT) { C.decompress(source, C.DecodeOptions(C.Format.GZIP, maxWindowBytes = 32767, checkCancelled = { ++polls })) }
    requireThat(polls == 0 && source.calls == 0, "Window admission touched callbacks")
}
// Bound complete messages and preserve cancellation identity at every observed provider callback.
fun bounds(payload: ByteArray) {
    for (format in formats) {
        val packed = C.compress(payload, C.EncodeOptions(format))
        requireThat(C.compress(payload, C.EncodeOptions(format, payload.size, packed.size)).contentEquals(packed), "Exact output bound")
        requireThat(C.decompress(packed, C.DecodeOptions(format, packed.size, payload.size)).contentEquals(payload), "Exact input/output bounds")
        reject(C.Code.RESOURCE_LIMIT) { C.compress(payload, C.EncodeOptions(format, maxInputBytes = payload.size - 1)) }
        if (packed.isNotEmpty()) reject(C.Code.RESOURCE_LIMIT) { C.compress(payload, C.EncodeOptions(format, maxOutputBytes = packed.size - 1)) }
        val empty = C.compress(byteArrayOf(), C.EncodeOptions(format, 0, 128))
        requireThat(C.decompress(empty, C.DecodeOptions(format, empty.size, 0)).isEmpty(), "Empty message")
        for (level in listOf(0, 1, 6, 9)) requireThat(C.decompress(C.compress(payload, C.EncodeOptions(format, level = level)), C.DecodeOptions(format)).contentEquals(payload), "Provider level")
    }
    val packed = C.compress(byteArrayOf(1, 2, 3), C.EncodeOptions(C.Format.DEFLATE))
    var polls = 0; C.decompress(packed, C.DecodeOptions(C.Format.DEFLATE, checkCancelled = { ++polls }))
    val original = java.util.zip.DataFormatException("original callback")
    for (stop in 1..polls) {
        var count = 0
        same(original) { C.decompress(packed, C.DecodeOptions(C.Format.DEFLATE, checkCancelled = { if (++count == stop) throw original })) }
    }
}
// Sequential wrappers retain ownership and never finalize or retry after close/failure.
fun lifecycle(payload: ByteArray) {
    for (format in formats) {
        val sink = Sink(); val output = C.Output(sink, C.EncodeOptions(format)); output.write(payload); output.flush()
        requireThat(sink.calls == 0 && sink.flushes == 0, "Staging published output"); output.finish()
        requireThat(C.decompress(sink.toByteArray(), C.DecodeOptions(format)).contentEquals(payload), "Wrapper output")
        fenced { output.finish() }; output.close(); requireThat(sink.closes == 0 && sink.maximum <= 32768, "Borrowed sink")
        val abandoned = Sink(); C.Output(abandoned, C.EncodeOptions(format)).also { it.write(payload); it.close() }
        requireThat(abandoned.calls == 0 && abandoned.closes == 0, "Close finalized output")
        val source = Source(sink.toByteArray(), 7); val input = C.Input(source, C.DecodeOptions(format))
        requireThat(input.read(byteArrayOf(), 0, 0) == 0 && source.calls == 0, "Zero read touched source")
        val plain = java.io.ByteArrayOutputStream(); val buffer = ByteArray(199)
        while (true) { val count = input.read(buffer); if (count == -1) break; plain.write(buffer, 0, count) }
        input.finish(); input.close(); requireThat(plain.toByteArray().contentEquals(payload) && source.closes == 0 && source.maximum <= 32768, "Borrowed input")
        val unread = C.Input(Source(sink.toByteArray()), C.DecodeOptions(format)); reject(C.Code.INVALID_DATA) { unread.finish() }; fenced { unread.read() }
        val original = java.io.IOException("original transport"); val brokenSink = Sink().also { it.failure = original }
        val failed = C.Output(brokenSink, C.EncodeOptions(format)); failed.write(payload); same(original) { failed.finish() }
        val calls = brokenSink.calls; fenced { failed.finish() }; failed.close(); requireThat(brokenSink.calls == calls && brokenSink.closes == 0, "Failed sink retried")
        val brokenSource = Source(sink.toByteArray()).also { it.failure = original }; val failedInput = C.Input(brokenSource, C.DecodeOptions(format))
        same(original) { failedInput.read() }; fenced { failedInput.finish() }; requireThat(brokenSource.calls == 1, "Failed source retried")
        var armed = false; val cancelled = IllegalStateException("late cancellation")
        val checked = C.Input(Source(sink.toByteArray()), C.DecodeOptions(format, checkCancelled = { if (armed) throw cancelled }))
        checked.read(); armed = true; same(cancelled) { checked.read() }; fenced { checked.finish() }
    }
    val overflow = Source(ByteArray(100)); reject(C.Code.RESOURCE_LIMIT) { C.decompress(overflow, C.DecodeOptions(maxInputBytes = 10)) }
    requireThat(overflow.maximum == 11 && overflow.calls == 1, "Overflow probe was not bounded")
    val stalled = object : java.io.InputStream() { override fun read() = 0; override fun read(data: ByteArray, offset: Int, length: Int) = 0 }
    try { C.decompress(stalled); throw AssertionError("Accepted zero-progress source") } catch (_: java.io.IOException) { }
}
// Callbacks may close/reenter; first failure fences the active operation and prevents later I/O.
fun reentrancy() {
    for (format in formats) {
        var armed = false; lateinit var output: C.Output; val sink = Sink()
        val policy = C.EncodeOptions(format, checkCancelled = { if (armed) output.close() })
        output = C.Output(sink, policy); output.write(1); armed = true; fenced { output.finish() }
        requireThat(sink.calls == 0, "Closed policy published output")
        armed = false; output = C.Output(sink, policy); armed = true; fenced { output.write(1) }
        var first: Throwable? = null; armed = false
        output = C.Output(sink, C.EncodeOptions(format, checkCancelled = {
            if (armed) { armed = false; try { output.flush() } catch (failure: Throwable) { first = failure } }
        }))
        armed = true; var actual: Throwable? = null
        try { output.write(1) } catch (failure: Throwable) { actual = failure }
        requireThat(first is IllegalStateException && first === actual && sink.calls == 0, "Output reentry lost first failure")
        lateinit var input: C.Input; armed = false
        input = C.Input(Source(C.compress(byteArrayOf(1), C.EncodeOptions(format))), C.DecodeOptions(format, checkCancelled = { if (armed) input.close() }))
        armed = true; fenced { input.read() }; fenced { input.finish() }
        first = null; armed = false
        input = C.Input(Source(C.compress(byteArrayOf(1), C.EncodeOptions(format))), C.DecodeOptions(format, checkCancelled = {
            if (armed) { armed = false; try { input.read() } catch (failure: Throwable) { first = failure } }
        }))
        armed = true; actual = null; try { input.read() } catch (failure: Throwable) { actual = failure }
        requireThat(first is IllegalStateException && first === actual, "Input reentry lost first failure")
    }
    lateinit var output: C.Output; val sink = Sink().also { it.afterWrite = { output.close() } }
    output = C.Output(sink); output.write(ByteArray(32769)); fenced { output.finish() }
    requireThat(sink.calls == 1 && sink.size() == 32768 && sink.closes == 0, "Close continued bounded writes")
    lateinit var input: C.Input; val source = Source(byteArrayOf(1)).also { it.afterRead = { input.close() } }
    input = C.Input(source); val destination = byteArrayOf(99); fenced { input.read(destination) }
    requireThat(source.calls == 1 && destination[0] == 99.toByte(), "Close continued source reads/publication")
}
// Run actual generated owning codecs and retain independent oracle inputs for every protocol.
fun main(args: Array<String>) {
    println("JVM " + java.lang.System.getProperty("java.version"))
    val directory = Path.of(args[0]); val count = fixtures(directory); models(directory); admission()
    val payload = Files.readAllBytes(directory.resolve("plain.bin")); bounds(payload); lifecycle(payload); reentrancy()
    println("PASS Kotlin compression: $count independent decisions, native bytes, bounds and borrowed lifecycle")
}
'''


def run(command, directory, environment):
    """Run explicit local SDKs without shell-built commands or silent missing-provider skips."""
    print('RUN', subprocess.list2cmdline([str(value) for value in command]), flush=True)
    return subprocess.run([str(value) for value in command], cwd=directory, env=environment,
                          check=True, timeout=240)


def generate(compiler, directory, schema=SCHEMA, extra=()):
    """Generate a real Kotlin module and discover its collision-safe helper container."""
    source, output = directory / 'model.serializer', directory / 'Schema.kt'
    source.write_text(schema, encoding='utf-8')
    subprocess.run([str(compiler), '--input', str(source), '--language', 'kotlin',
                    '--kotlin.output', str(output), *extra], check=True)
    text = output.read_text(encoding='utf-8')
    match = re.search(r'Generated Kotlin compression helper name: (\w+)', text)
    if not match:
        raise AssertionError('Actual generated Kotlin compression helper marker missing')
    return match.group(1), len(output.read_bytes()), hashlib.sha256(output.read_bytes()).hexdigest()


def smoke_source(helper, name):
    """Exercise real model methods alongside helper collisions and native protocol selectors."""
    source = ('import ' + helper + ' as C\nfun main() { val value = AccountRecord(); value.accountId = 7u; '
              'val wire = C.encode({value.encode(Protocol.BINARY_NONE)}, C.EncodeOptions(C.Format.GZIP)); '
              'val result = C.decode(wire, {AccountRecord.decode(it, Protocol.BINARY_NONE)}, C.DecodeOptions(C.Format.GZIP)); '
              'check(result.accountId == 7u); ')
    if name == 'collision':
        for index, model in enumerate(('SerializerCompression', 'SerializerCompression2')):
            source += (f'val preserved{index} = {model}().encode(Protocol.BINARY_NONE); '
                       f'check({model}.decode(preserved{index}, Protocol.BINARY_NONE)'
                       f'.encode(Protocol.BINARY_NONE).contentEquals(preserved{index})); ')
    if name == 'binary':
        source += 'try { value.encode(Protocol.JSON); error("Disabled protocol accepted") } catch (_: IllegalArgumentException) {} '
    return source + '}\n'


def main():
    """Compile JVM17 output and replay independent frames, ownership and first-error regressions."""
    root = Path(__file__).resolve().parent.parent
    local_kotlinc = root / 'out/build/s08-s36/kotlin-2.2.21/kotlinc/bin' / ('kotlinc.bat' if os.name == 'nt' else 'kotlinc')
    java_home = os.environ.get('JAVA_HOME')
    local_java = Path(java_home) / 'bin' / ('java.exe' if os.name == 'nt' else 'java') if java_home else None
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--kotlinc', default=str(local_kotlinc) if local_kotlinc.is_file() else shutil.which('kotlinc'))
    parser.add_argument('--java', default=str(local_java) if local_java and local_java.is_file() else shutil.which('java'))
    args = parser.parse_args()
    if not args.kotlinc or not args.java:
        parser.error('An actual Kotlin compiler and JDK17 runtime are required')
    build = args.build.resolve()
    if build == root or root in build.parents and 'out' not in build.relative_to(root).parts:
        parser.error('In-repository output must remain below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    java_version = subprocess.check_output([args.java, '-version'], stderr=subprocess.STDOUT, text=True)
    if not re.search(r'version "17\.', java_version):
        parser.error('This selected baseline requires actual JDK17 execution')
    environment = os.environ.copy()
    environment['JAVA_HOME'] = str(Path(args.java).resolve().parent.parent)
    kotlin_version = subprocess.check_output([args.kotlinc, '-version'], env=environment,
                                             stderr=subprocess.STDOUT, text=True)
    profiles = {}
    with tempfile.TemporaryDirectory(prefix='actual-', dir=build) as temporary:
        directory = Path(temporary)
        count = common_fixtures(directory) + extra_fixtures(directory)
        family, size, digest = generate(args.compiler.resolve(), directory)
        profiles['normal'] = {'bytes': size, 'sha256': digest}
        (directory / 'Main.kt').write_text(KOTLIN.replace('__COMPRESSION_NAME__', family), encoding='utf-8')
        run([args.kotlinc, '-Werror', '-jvm-target', '17', '-include-runtime', '-d', directory / 'app.jar',
             directory / 'Schema.kt', directory / 'Main.kt'], directory, environment)
        run([args.java, '-jar', directory / 'app.jar', directory], directory, environment)
        for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING'):
            native = (directory / ('native-' + protocol + '.bin')).read_bytes()
            for name, bits in (('NONE', None), ('GZIP', 31), ('ZLIB', 15), ('DEFLATE', -15)):
                packed = (directory / ('output-' + name + '-' + protocol + '.bin')).read_bytes()
                if bits is None:
                    actual = packed
                else:
                    decoder = zlib.decompressobj(bits)
                    actual = decoder.decompress(packed) + decoder.flush()
                    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                        raise AssertionError('Kotlin output is not exactly one standard frame')
                if actual != native:
                    raise AssertionError('Kotlin compression changed ordinary native bytes')
        if json.loads((directory / 'native-JSON.bin').read_bytes())['account_id'] != 42:
            raise AssertionError('Native JSON identity changed')
        for name, schema, flags in (
                ('collision', SCHEMA + ' class SerializerCompression {} class SerializerCompression_2 {}', ()),
                ('builtins', SCHEMA + ' class Inflater {} class Deflater {} class InputStream {} class OutputStream {} class Throwable {}', ()),
                ('binary', SCHEMA, ('--protocols', 'binary_none'))):
            case = directory / name
            case.mkdir()
            helper, size, digest = generate(args.compiler.resolve(), case, schema, flags)
            profiles[name] = {'bytes': size, 'sha256': digest}
            if name == 'collision':
                source = (case / 'Schema.kt').read_text(encoding='utf-8')
                if helper != 'SerializerCompression_2' or not all(re.search(r'\bclass\s+' + model + r'\b', source)
                        for model in ('SerializerCompression', 'SerializerCompression2')):
                    raise AssertionError('Kotlin helper changed the existing normalized model names')
            (case / 'Main.kt').write_text(smoke_source(helper, name), encoding='utf-8')
            run([args.kotlinc, '-Werror', '-jvm-target', '17', '-include-runtime', '-d', case / 'app.jar',
                 case / 'Schema.kt', case / 'Main.kt'], case, environment)
            run([args.java, '-jar', case / 'app.jar'], case, environment)
    report = {'kotlin': kotlin_version.strip(), 'java': java_version.strip(), 'target': 'JVM17',
              'independent_input_decisions': count, 'independent_output_decisions': 16,
              'profiles': profiles, 'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest()}
    (build / 'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    print('PASS: actual generated Kotlin/JVM17 compression, independent framing and lifecycle', flush=True)


if __name__ == '__main__':
    main()
