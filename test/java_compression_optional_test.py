# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify explicitly selected optional Java providers against independent standard frames."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import zipfile


JAVA = r'''package serializer.compression.optional;
import static serializer.compression.optional.Schema.SerializerCompression.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;

/** Exercise real emitted codecs, optional-provider admission and complete-operation failures. */
public final class Main {
  @FunctionalInterface private interface Checked { void run() throws Exception; }
  /** Fail independently of the JVM assertion switch. */
  private static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
  /** Require an expected stable category rather than another failure. */
  private static void reject(Code code, Checked operation) throws Exception {
    try { operation.run(); throw new AssertionError("Accepted " + code); }
    catch (Failure failure) { require(failure.code == code, "Wrong category: " + failure.code); }
  }
  /** Retain the application's exact cancellation exception even through provider cleanup. */
  private static void same(RuntimeException expected, Checked operation) throws Exception {
    try { operation.run(); throw new AssertionError("Cancellation was ignored"); }
    catch (RuntimeException failure) { require(failure == expected, "Cancellation exception replaced: " + failure); }
  }
  /** Verify unavailable deployment does not invoke any user serializer, cancellation or stream. */
  private static void absent(Format format) throws Exception {
    require(!available(format), "Absent provider reported available");
    int[] effects = {0}; Runnable callback = () -> ++effects[0];
    EncodeOptions encoding = new EncodeOptions(format, 0, 1024, 1024, callback);
    DecodeOptions decoding = new DecodeOptions(format, 1024, 1024, 65536, callback);
    InputStream source = new InputStream() { /** Detect any forbidden source access. */
      @Override public int read() { ++effects[0]; return -1; } };
    OutputStream sink = new OutputStream() { /** Detect any forbidden sink publication. */
      @Override public void write(int value) { ++effects[0]; } };
    reject(Code.UNAVAILABLE, () -> encode(() -> { ++effects[0]; return new byte[0]; }, encoding));
    reject(Code.UNAVAILABLE, () -> decode(new byte[0], decoding, bytes -> { ++effects[0]; return bytes; }));
    reject(Code.UNAVAILABLE, () -> new Input(source, decoding));
    reject(Code.UNAVAILABLE, () -> new Output(sink, encoding));
    require(effects[0] == 0, "Unavailable deployment entered application work");
  }
  /** Qualify real owning values in every existing native protocol and exact inner EOF. */
  private static void objects(Format format) throws Exception {
    Schema.AccountRecord value = new Schema.AccountRecord(); value.accountId = 17; value.label = "shared λ 🙂";
    for (Schema.Protocol protocol : Schema.Protocol.values()) {
      byte[] ordinary = value.encode(protocol);
      byte[] packed = encode(() -> value.encode(protocol), new EncodeOptions(format));
      Schema.AccountRecord copy = decode(packed, new DecodeOptions(format), bytes -> Schema.AccountRecord.decode(bytes, protocol));
      require(copy.accountId == value.accountId && copy.label.equals(value.label), "Owning model differs");
      require(Arrays.equals(ordinary, decompress(packed, new DecodeOptions(format))), "Native bytes changed");
      byte[] suffix = compress(Arrays.copyOf(ordinary, ordinary.length + 1), new EncodeOptions(format));
      try { decode(suffix, new DecodeOptions(format), bytes -> Schema.AccountRecord.decode(bytes, protocol));
        throw new AssertionError("Inner suffix accepted"); } catch (IllegalArgumentException expected) { }
    }
  }
  /** Exercise exact byte bounds, active-provider cancellation, staged wrappers and final publication. */
  private static void policies(Format format, byte[] plain) throws Exception {
    int level = new EncodeOptions(format).level;
    for (boolean checksum : new boolean[] {false, true}) {
      EncodeOptions options = new EncodeOptions(format, level, plain.length, plain.length + 65536, null, checksum);
      byte[] packed = compress(plain, options);
      require(Arrays.equals(plain, decompress(packed, new DecodeOptions(format, packed.length, plain.length, 8*1024*1024, null))), "Exact decode bounds");
      require(Arrays.equals(packed, compress(plain, new EncodeOptions(format, level, plain.length, packed.length, null, checksum))), "Exact encode bounds");
      reject(Code.RESOURCE_LIMIT, () -> compress(plain, new EncodeOptions(format, level, plain.length-1, packed.length, null, checksum)));
      reject(Code.RESOURCE_LIMIT, () -> compress(plain, new EncodeOptions(format, level, plain.length, packed.length-1, null, checksum)));
      reject(Code.RESOURCE_LIMIT, () -> decompress(packed, new DecodeOptions(format, packed.length-1, plain.length, 8*1024*1024, null)));
      reject(Code.RESOURCE_LIMIT, () -> decompress(packed, new DecodeOptions(format, packed.length, plain.length-1, 8*1024*1024, null)));
      boolean headerChecksum = (packed[4] & 4) != 0;
      require(headerChecksum == checksum, "Checksum option did not reach standard frame header");
    }
    RuntimeException cancellation = new RuntimeException("original optional-provider cancellation");
    int[] polls = {0};
    same(cancellation, () -> compress(plain, new EncodeOptions(format, level, plain.length, plain.length+65536,
        () -> { if (++polls[0] >= 4) throw cancellation; })));
    byte[] packed = compress(plain, new EncodeOptions(format)); polls[0] = 0;
    same(cancellation, () -> decompress(packed, new DecodeOptions(format, packed.length, plain.length, 8*1024*1024,
        () -> { if (++polls[0] >= 4) throw cancellation; })));
    require(Arrays.equals(plain, decompress(compress(plain, new EncodeOptions(format)), new DecodeOptions(format))), "Provider recovery after cancellation");
    boolean[] armed = {false}; Runnable check = () -> { if (armed[0]) throw cancellation; };
    for (boolean finishing : new boolean[] {false, true}) {
      Input input = new Input(new ByteArrayInputStream(packed), new DecodeOptions(format, packed.length, plain.length, 8*1024*1024, check));
      if (finishing) require(Arrays.equals(input.readAllBytes(), plain), "Staged input differs");
      armed[0] = true;
      same(cancellation, () -> { if (finishing) input.finish(); else input.read(); });
      try { input.read(); throw new AssertionError("Cancelled input reused"); } catch (IOException expected) { }
      input.close(); armed[0] = false;
    }
    ByteArrayOutputStream sink = new ByteArrayOutputStream() { /** Cancel only after successful final publication. */
      @Override public synchronized void write(byte[] bytes, int offset, int length) { super.write(bytes, offset, length); armed[0] = true; } };
    Output output = new Output(sink, new EncodeOptions(format, level, plain.length, plain.length+65536, check));
    output.write(plain); same(cancellation, output::finish); int published = sink.size();
    try { output.finish(); throw new AssertionError("Cancelled output reused"); } catch (IOException expected) { }
    require(sink.size() == published, "Failed output retried publication"); output.close(); armed[0] = false;
    Schema.AccountRecord value = new Schema.AccountRecord(); value.label = "late native decode";
    byte[] model = compress(value.encode(Schema.Protocol.JSON), new EncodeOptions(format));
    same(cancellation, () -> decode(model, new DecodeOptions(format, model.length, 65536, 8*1024*1024, check), bytes -> {
      Schema.AccountRecord candidate = Schema.AccountRecord.decode(bytes, Schema.Protocol.JSON); armed[0] = true; return candidate;
    }));
    armed[0] = false;
    if (format == Format.ZSTD) {
      Class<?> provider = Class.forName("com.github.luben.zstd.Zstd");
      int minimum = (Integer) provider.getMethod("minCompressionLevel").invoke(null);
      int maximum = (Integer) provider.getMethod("maxCompressionLevel").invoke(null);
      for (int selected : new int[] {minimum, -1, 0, 3, 10, maximum}) {
        byte[] frame = compress(new byte[] {1}, new EncodeOptions(format, selected, 1, 65536, null));
        require(Arrays.equals(new byte[] {1}, decompress(frame, new DecodeOptions(format))), "Zstandard advertised level");
      }
      int[] effects = {0};
      reject(Code.INVALID_OPTIONS, () -> encode(() -> { ++effects[0]; return plain; },
          new EncodeOptions(format, maximum+1, plain.length, plain.length+65536, () -> ++effects[0])));
      reject(Code.INVALID_OPTIONS, () -> encode(() -> { ++effects[0]; return plain; },
          new EncodeOptions(format, minimum-1, plain.length, plain.length+65536, () -> ++effects[0])));
      reject(Code.RESOURCE_LIMIT, () -> new Input(new InputStream() { /** Detect premature reads. */
        @Override public int read() { ++effects[0]; return -1; } }, new DecodeOptions(format, 1, 1, 1023, () -> ++effects[0])));
      require(effects[0] == 0, "Invalid provider policy entered user work");
    }
  }
  /** Replay required independent frames, then publish frames for the independent native oracle. */
  public static void main(String[] args) throws Exception {
    Format format = Format.valueOf(args[1]);
    if (args[0].equals("absent")) { absent(format); System.out.println("{\"unavailable_before_effects\":true}"); return; }
    require(available(format), "Requested optional provider is unavailable");
    Path root = Path.of(args[2]); int decisions = 0;
    for (String line : Files.readAllLines(root.resolve(format.name()+".tsv"))) {
      String[] row = line.split("\t"); byte[] frame = Files.readAllBytes(root.resolve(row[0]));
      Code wanted = row[2].equals("ok") ? null : Code.valueOf(row[2]);
      DecodeOptions options = new DecodeOptions(format, Integer.parseInt(row[3]), Integer.parseInt(row[4]), Integer.parseInt(row[5]), null);
      if (wanted == null) require(Arrays.equals(decompress(frame, options), Files.readAllBytes(root.resolve(row[1]))), "Independent frame differs: " + row[0]);
      else reject(wanted, () -> decompress(frame, options));
      ++decisions;
    }
    byte[] plain = Files.readAllBytes(root.resolve("plain.bin"));
    objects(format); policies(format, plain);
    Files.write(root.resolve("java-"+format+".bin"), compress(plain, new EncodeOptions(format)));
    Files.write(root.resolve("java-"+format+"-unchecked.bin"), compress(plain, new EncodeOptions(format, new EncodeOptions(format).level, plain.length, plain.length+65536, null, false)));
    System.out.println("{\"independent_decisions\":"+decisions+",\"model_roundtrips\":"+Schema.Protocol.values().length+"}");
  }
}
'''


def run(command, log):
    """Retain every real compiler/SDK result and reject missing explicit dependencies without skips."""
    result = subprocess.run([str(item) for item in command], capture_output=True, text=True,
                            encoding='utf-8', errors='replace', timeout=180)
    log.write_text(json.dumps(list(map(str, command)))+'\n'+result.stdout+result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(log.name+'\n'+result.stdout+result.stderr)
    return result.stdout.strip()


def fixtures(directory, formats):
    """Construct standard optional frames and shared malformed/byte/window recipes independently."""
    if 'ZSTD' in formats:
        from compression import zstd
    payloads = [b'', b'A', bytes(range(256))*256, b'complete object\x00'*16385,
                random.Random(107).randbytes(131073)]
    plain = payloads[-1]
    (directory/'plain.bin').write_bytes(plain)
    rows = {name: [] for name in formats}

    def add(name, frame, expected=b'', code='ok', max_input=8*1024*1024,
            max_output=8*1024*1024, window=8*1024*1024):
        """Write exact independent input and expected bytes with a bounded policy row."""
        number = len(rows[name]); stem = f'{name.lower()}-{number:04d}'
        (directory/(stem+'.bin')).write_bytes(frame)
        (directory/(stem+'.plain')).write_bytes(expected)
        rows[name].append((stem+'.bin', stem+'.plain', code, max_input, max_output, window))

    for name in formats:
        for payload in payloads:
            for checked in (False, True):
                if name == 'ZSTD':
                    encoder = zstd.ZstdCompressor(options={zstd.CompressionParameter.checksum_flag: int(checked),
                                                           zstd.CompressionParameter.content_size_flag: 0,
                                                           zstd.CompressionParameter.window_log: 16})
                    frame = encoder.compress(payload)+encoder.flush()
                else:
                    import lz4.frame
                    frame = lz4.frame.compress(payload, block_size=lz4.frame.BLOCKSIZE_MAX64KB,
                        block_linked=True, block_checksum=True, content_checksum=checked, store_size=True)
                add(name, frame, payload, max_input=len(frame), max_output=len(payload))
                for position in sorted({0, 1, 4, 6, len(frame)//2, len(frame)-1}):
                    add(name, frame[:position], code='INVALID_DATA')
                for suffix in (b'\x00', frame, b'\x50\x2a\x4d\x18\x00\x00\x00\x00'):
                    add(name, frame+suffix, code='INVALID_DATA')
                if checked:
                    bad = bytearray(frame); bad[-1] ^= 1
                    add(name, bytes(bad), code='INVALID_DATA')
                add(name, frame, code='RESOURCE_LIMIT', max_input=len(frame)-1)
                if payload:
                    add(name, frame, code='RESOURCE_LIMIT', max_output=len(payload)-1)
        add(name, b'\x50\x2a\x4d\x18\x00\x00\x00\x00', code='INVALID_DATA')
        if name == 'ZSTD':
            frame = zstd.compress(payloads[2])
            add(name, frame, payloads[2], window=65537)
            add(name, frame, code='RESOURCE_LIMIT', window=65535)
            add(name, b'\x28\xb5\x2f\xfd\x21\x01\x00\x01\x00\x00', code='INVALID_DATA')
            for size in (0, 1, 256, 65536):
                payload = b'Z'*size
                add(name, zstd.compress(payload), payload)
            # The specification ignores the unused descriptor bit and defines exact non-power-of-two windows.
            magic = b'\x28\xb5\x2f\xfd'
            non_power_window = magic+b'\x00\x31\x09\x00\x00A'
            zero_dictionary = magic+b'\x21\x00\x01\x09\x00\x00A'
            for valid in (non_power_window, zero_dictionary):
                if zstd.decompress(valid) != b'A':
                    raise AssertionError('Independent manual Zstandard frame differs')
            add(name, non_power_window, b'A', window=131072)
            add(name, non_power_window, code='RESOURCE_LIMIT', window=80000)
            add(name, zero_dictionary, b'A')
            unused = bytearray(zstd.compress(b'A')); unused[4] |= 16
            if zstd.decompress(unused) != b'A':
                raise AssertionError('Independent unused Zstandard descriptor bit differs')
            add(name, bytes(unused), b'A')
            reserved = bytearray(unused); reserved[4] |= 8
            add(name, bytes(reserved), code='INVALID_DATA')
        else:
            import lz4.block
            import lz4.frame
            for block_size in (lz4.frame.BLOCKSIZE_MAX64KB, lz4.frame.BLOCKSIZE_MAX256KB,
                               lz4.frame.BLOCKSIZE_MAX1MB, lz4.frame.BLOCKSIZE_MAX4MB):
                for linked in (False, True):
                    for checked in (False, True):
                        payload = random.Random(173).randbytes(65536)*17
                        frame = lz4.frame.compress(payload, block_size=block_size, block_linked=linked,
                            block_checksum=checked, content_checksum=checked, store_size=checked)
                        maximum = lz4.frame.get_frame_info(frame)['block_size']
                        add(name, frame, payload, window=maximum)
                        add(name, frame, code='RESOURCE_LIMIT', window=maximum-1)
            add(name, b'\x02\x21\x4c\x18', code='INVALID_DATA')
            add(name, b'\x04\x22\x4d\x18\x61\x40\x00\x00\x00\x00\x00', code='INVALID_DATA')
            empty = lz4.frame.compress(b'', block_size=lz4.frame.BLOCKSIZE_MAX64KB,
                                       block_checksum=False, content_checksum=False, store_size=False)
            header = empty[:-4]
            empty_raw = header+struct.pack('<I', 0x80000000)+struct.pack('<I', 0)
            if lz4.frame.decompress(empty_raw) != b'':
                raise AssertionError('Independent empty raw LZ4 block differs')
            add(name, empty_raw)
            for invalid in (b'\x00\x00\x00\x00', b'\xf0\xff', b'\x10'):
                add(name, header+struct.pack('<I', len(invalid))+invalid+struct.pack('<I', 0), code='INVALID_DATA')
            add(name, header+struct.pack('<I', 65537)+b'A'+struct.pack('<I', 0), code='INVALID_DATA')
            wrong_header = bytearray(empty); wrong_header[len(header)-1] ^= 1
            add(name, bytes(wrong_header), code='INVALID_DATA')
            short = lz4.frame.compress(b'A', block_size=lz4.frame.BLOCKSIZE_MAX64KB,
                                       content_checksum=False, store_size=True)
            longer = lz4.frame.compress(b'AA', block_size=lz4.frame.BLOCKSIZE_MAX64KB,
                                        content_checksum=False, store_size=True)
            add(name, short[:15]+longer[15:], code='INVALID_DATA')
            checked = lz4.frame.compress(b'A'*1000, block_size=lz4.frame.BLOCKSIZE_MAX64KB,
                                         block_checksum=True, content_checksum=False, store_size=False)
            checksum_position = 7+4+(struct.unpack_from('<I', checked, 7)[0] & 0x7fffffff)
            bad = bytearray(checked); bad[checksum_position] ^= 1
            add(name, bytes(bad), code='INVALID_DATA')
            # Independent dictionary compression supplies true prior-block references at literal extension boundaries.
            seed = random.Random(981).randbytes(256*1024)
            linked = lz4.frame.compress(seed+seed, block_size=lz4.frame.BLOCKSIZE_MAX256KB,
                block_linked=True, block_checksum=False, content_checksum=False, store_size=False)
            linked_header = linked[:7]
            if linked_header[4] & 32:
                raise AssertionError('Independent provider did not produce a linked-frame descriptor')
            for history_size in (0, 1, 14, 15, 16, 254, 255, 256, 257, 65535, 65536):
                history = random.Random(history_size+100).randbytes(history_size)
                payload = (history[-min(history_size, 65000):]+b'match extension edge')*3
                block = lz4.block.compress(payload, store_size=False, dict=history)
                frame = (linked_header+struct.pack('<I', 0x80000000 | len(history))+history+
                         struct.pack('<I', len(block))+block+struct.pack('<I', 0))
                if lz4.frame.decompress(frame) != history+payload:
                    raise AssertionError('Independent linked-history frame differs')
                add(name, frame, history+payload)
    for name, decisions in rows.items():
        (directory/(name+'.tsv')).write_text(''.join('\t'.join(map(str, row))+'\n' for row in decisions), encoding='utf-8')
    return {name: len(value) for name, value in rows.items()}


def main():
    """Compile without provider jars, then require absent and selected present-provider profiles."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--provider-jar', type=Path, action='append', required=True)
    parser.add_argument('--provider-path', type=Path)
    parser.add_argument('--cpp-helper', type=Path, help='Explicit existing native compression_interop executable')
    parser.add_argument('--format', choices=('ZSTD', 'LZ4'), action='append')
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--java', default=shutil.which('java'))
    args = parser.parse_args()
    if not args.javac or not args.java or not all(path.is_file() for path in args.provider_jar):
        parser.error('Every explicitly selected Java SDK and provider jar is required')
    if args.cpp_helper and not args.cpp_helper.is_file():
        parser.error('The explicitly selected native interoperability helper is required')
    root = Path(__file__).resolve().parent.parent
    directory = args.build.resolve()
    if directory == root or root in directory.parents and directory.relative_to(root).parts[0] != 'out':
        parser.error('In-repository output must stay beneath ignored out')
    directory.mkdir(parents=True, exist_ok=True)
    if args.provider_path:
        sys.path.insert(0, str(args.provider_path.resolve()))
    formats = args.format or ['ZSTD', 'LZ4']
    count = fixtures(directory, formats)
    if args.cpp_helper:
        for name in formats:
            for mode in ('encode', 'stream_encode'):
                frame = directory/f'cpp-{name}-{mode}.bin'
                run([args.cpp_helper.resolve(), mode, name.lower(), directory/'plain.bin', frame],
                    directory/f'cpp-{name}-{mode}.log')
                with (directory/(name+'.tsv')).open('a', encoding='utf-8') as inventory:
                    inventory.write(f'{frame.name}\tplain.bin\tok\t{frame.stat().st_size}\t{(directory/"plain.bin").stat().st_size}\t{8*1024*1024}\n')
                count[name] += 1
    schema = directory/'model.serializer'
    schema.write_text('serializer version 1; class account_record stable_ids { public uint32 account_id(1); public string label(2); }', encoding='utf-8')
    generated = directory/'Schema.java'
    run([args.compiler.resolve(), '--input', schema, '--language', 'java', '--output', generated,
         '--java.package', 'serializer.compression.optional'], directory/'generate.log')
    (directory/'Main.java').write_text(JAVA, encoding='utf-8')
    classes = directory/'classes'; classes.mkdir(exist_ok=True)
    run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d', classes,
         generated, directory/'Main.java'], directory/'compile-without-providers.log')
    main_class = 'serializer.compression.optional.Main'
    classpath = os.pathsep.join([str(classes), *(str(path.resolve()) for path in args.provider_jar)])
    results = {}
    native_checks = 0
    for name in formats:
        run([args.java, '-cp', classes, main_class, 'absent', name], directory/(name+'-absent.log'))
        results[name] = json.loads(run([args.java, '-cp', classpath, main_class, 'present', name, directory],
                                       directory/(name+'-present.log')).splitlines()[-1])
        if results[name] != {'independent_decisions': count[name], 'model_roundtrips': 4}:
            raise AssertionError('Incomplete optional-provider qualification')
        for suffix in ('', '-unchecked'):
            frame = (directory/f'java-{name}{suffix}.bin').read_bytes()
            if name == 'ZSTD':
                from compression import zstd
                decoder = zstd.ZstdDecompressor()
            else:
                import lz4.frame
                decoder = lz4.frame.LZ4FrameDecompressor()
            actual = decoder.decompress(frame)
            if actual != (directory/'plain.bin').read_bytes() or not decoder.eof or decoder.unused_data:
                raise AssertionError('Independent decoder rejected Java '+name+suffix)
            if args.cpp_helper:
                for mode in ('decode', 'stream_decode'):
                    destination = directory/f'cpp-{name}{suffix}-{mode}.plain'
                    run([args.cpp_helper.resolve(), mode, name.lower(), directory/f'java-{name}{suffix}.bin', destination],
                        directory/f'cpp-{name}{suffix}-{mode}.log')
                    if destination.read_bytes() != actual:
                        raise AssertionError('Native consumer differs from Java '+name+suffix)
                    native_checks += 1
    if 'ZSTD' in formats:
        run([args.java, '-DZstdNativePath='+str(directory/'absent-native-provider.dll'), '-cp', classpath,
             main_class, 'absent', 'ZSTD'], directory/'ZSTD-failed-native-deployment.log')
    provider_metadata = {}
    for path in args.provider_jar:
        with zipfile.ZipFile(path) as archive:
            provider_metadata[path.name] = {name: archive.read(name).decode('utf-8')
                for name in archive.namelist() if name.endswith('/pom.properties')}
    oracle_versions = {}
    if 'ZSTD' in formats:
        from compression import zstd
        oracle_versions['stdlib_zstd'] = list(zstd.zstd_version_info)
    if 'LZ4' in formats:
        import lz4
        oracle_versions.update(python_lz4=lz4.__version__, liblz4=lz4.library_version_string())
    result = {'formats': results, 'generated_sha256': hashlib.sha256(generated.read_bytes()).hexdigest(),
              'provider_jars': {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in args.provider_jar},
              'provider_metadata': provider_metadata, 'oracle_versions': oracle_versions,
              'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'compiler_version': run([args.compiler.resolve(), '--version'], directory/'compiler-version.log'),
              'test_source_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'cpp_consumer_checks': native_checks,
              'cpp_producer_checks': 2*len(formats) if args.cpp_helper else 0,
              'cpp_helper_sha256': hashlib.sha256(args.cpp_helper.read_bytes()).hexdigest() if args.cpp_helper else None,
              'java': run([args.java, '--version'], directory/'java-version.log'),
              'python': sys.version, 'source': 'Actual current selected compiler output; no provider jars on javac classpath'}
    (directory/'result.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
