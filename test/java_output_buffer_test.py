# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify caller-owned Java encoding windows with actual SDKs and baseline wire bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = 'serializer.outputbuffer.tests'

COMMON = r'''
  private static final byte SENTINEL = (byte)0x5a;
  private static final int GUARD_BYTES = 8;
  private static long checks;
  /** Report a failed SDK invariant without depending on the JVM assertion flag. */
  private static void check(boolean value) { ++checks; if (!value) throw new AssertionError("Output window invariant " + checks); }
  /** Build one deterministic owning graph with full-width values and non-ASCII text. */
  private static Schema.BufferRecord sample() {
    var value = new Schema.BufferRecord();
    value.payload = new byte[]{0, 1, 127, -128, -1};
    value.text = "window \u03c0 \ud83d\ude00";
    var child = new Schema.BufferChild(); child.identity = -1L; child.label = "first";
    var second = new Schema.BufferChild(); second.identity = 42; second.label = "second";
    value.elements = new Schema.BufferChild[]{child, second};
    value.byName.put("z", -1); value.byName.put("\u03c0", 42);
    value.selected = java.util.Optional.of(child);
    value.choiceIndex = 1; value.choiceText = "variant";
    value.number = 1.25;
    return value;
  }
'''

JAVA = 'package ' + PACKAGE + r''';
public final class Main {
  private static final boolean RESTRICTED = __RESTRICTED__;
''' + COMMON + r'''
  @FunctionalInterface private interface Encoder {
    /** Encode into the caller's exclusive destination window. */
    int encode(Schema.Protocol protocol, byte[] bytes, int offset, int length);
  }
  /** Return the expected runtime failure while rejecting silent success. */
  private static RuntimeException failure(Runnable action) {
    try { action.run(); } catch (RuntimeException error) { ++checks; return error; }
    throw new AssertionError("Expected failure");
  }
  /** Check bytes outside the caller-authorized window after successful or failed encoding. */
  private static void guard(byte[] bytes, int offset, int length) {
    for (int index = 0; index < bytes.length; ++index) {
      if (index < offset || index >= offset + length) check(bytes[index] == SENTINEL);
    }
  }
  /** Compare every window against the existing independently owning encoding API. */
  private static void verify(Encoder encoder, byte[] expected, Schema.Protocol protocol) {
    for (int offset : new int[]{0, 1, 7, 31}) {
      for (int extra : new int[]{0, 13}) {
        int capacity = expected.length + extra;
        byte[] destination = new byte[offset + capacity + GUARD_BYTES];
        java.util.Arrays.fill(destination, SENTINEL);
        check(encoder.encode(protocol, destination, offset, capacity) == expected.length);
        check(java.util.Arrays.equals(expected, java.util.Arrays.copyOfRange(destination, offset, offset + expected.length)));
        guard(destination, offset, expected.length);
      }
      if (expected.length != 0) {
        for (int capacity : new int[]{0, 1, expected.length / 2, expected.length - 1}) {
          if (capacity >= expected.length) continue;
          byte[] destination = new byte[offset + capacity + GUARD_BYTES];
          java.util.Arrays.fill(destination, SENTINEL);
          RuntimeException error = failure(() -> encoder.encode(protocol, destination, offset, capacity));
          check(error instanceof IllegalArgumentException);
          check(error.getMessage().equals("Output buffer capacity exceeded"));
          guard(destination, offset, capacity);
        }
      }
    }
    byte[] destination = new byte[32];
    for (int[] range : new int[][]{{-1,0},{33,0},{0,-1},{Integer.MAX_VALUE,1},{1,Integer.MAX_VALUE}}) {
      java.util.Arrays.fill(destination, SENTINEL);
      check(failure(() -> encoder.encode(protocol, destination, range[0], range[1])) instanceof IndexOutOfBoundsException);
      guard(destination, 0, 0);
    }
    check(failure(() -> encoder.encode(protocol, null, 0, 0)) instanceof NullPointerException);
    java.util.Arrays.fill(destination, SENTINEL);
    check((RESTRICTED ? IllegalArgumentException.class : NullPointerException.class).isInstance(
        failure(() -> encoder.encode(null, destination, 0, destination.length))));
    guard(destination, 0, 0);
    java.util.Arrays.fill(destination, SENTINEL);
    if (expected.length == 0) {
      check(encoder.encode(protocol, destination, destination.length, 0) == 0);
    } else {
      check(failure(() -> encoder.encode(protocol, destination, destination.length, 0)) instanceof IllegalArgumentException);
    }
    guard(destination, 0, 0);
  }
  /** Keep typed/depth failures equivalent while confining any partial write to the destination window. */
  private static void sameFailure(Runnable owning, Encoder encoder, Schema.Protocol protocol) {
    RuntimeException expected = failure(owning);
    int offset = 7, capacity = 65536;
    byte[] destination = new byte[offset + capacity + GUARD_BYTES];
    java.util.Arrays.fill(destination, SENTINEL);
    RuntimeException actual = failure(() -> encoder.encode(protocol, destination, offset, capacity));
    check(actual.getClass() == expected.getClass());
    check(java.util.Objects.equals(actual.getMessage(), expected.getMessage()));
    guard(destination, offset, capacity);
  }
  @FunctionalInterface private interface NioEncoder {
    /** Encode into the caller's writable position-to-limit window. */
    int encode(Schema.Protocol protocol, java.nio.ByteBuffer destination);
  }
  /** Inspect the entire backing view without changing the caller's cursor, limit or mark. */
  private static void guardNio(java.nio.ByteBuffer bytes, int offset, int length) {
    var full = bytes.duplicate(); full.clear();
    for (int index = 0; index < full.capacity(); ++index) {
      if (index < offset || index >= offset + length) check(full.get(index) == SENTINEL);
    }
  }
  /** Allocate and mark one deterministic heap/direct window for a successful or failed call. */
  private static java.nio.ByteBuffer nioWindow(boolean direct, int offset, int capacity) {
    int total = offset + capacity + GUARD_BYTES;
    var bytes = direct ? java.nio.ByteBuffer.allocateDirect(total) : java.nio.ByteBuffer.allocate(total);
    for (int index = 0; index < total; ++index) bytes.put(index, SENTINEL);
    bytes.limit(offset + capacity); bytes.position(offset); bytes.order((offset & 1) == 0 ? java.nio.ByteOrder.BIG_ENDIAN : java.nio.ByteOrder.LITTLE_ENDIAN); bytes.mark();
    return bytes;
  }
  /** Preserve all cursor metadata while admitting only the returned successful bytes. */
  private static void nioState(java.nio.ByteBuffer bytes, int position, int limit, int mark) {
    check(bytes.position() == position && bytes.limit() == limit);
    check(bytes.order() == ((mark & 1) == 0 ? java.nio.ByteOrder.BIG_ENDIAN : java.nio.ByteOrder.LITTLE_ENDIAN));
    bytes.reset(); check(bytes.position() == mark); bytes.position(position);
  }
  /** Compare exact/short, heap/direct, sliced and read-only windows with independent owning bytes. */
  private static void verifyNio(NioEncoder encoder, byte[] expected, Schema.Protocol protocol) {
    for (boolean direct : new boolean[]{false, true}) {
      for (int offset : new int[]{0, 1, 7, 31}) {
        for (int extra : new int[]{0, 13}) {
          int capacity = expected.length + extra;
          var bytes = nioWindow(direct, offset, capacity);
          check(encoder.encode(protocol, bytes) == expected.length);
          check(bytes.isDirect() == direct);
          nioState(bytes, offset + expected.length, offset + capacity, offset);
          for (int index = 0; index < expected.length; ++index) check(bytes.get(offset + index) == expected[index]);
          guardNio(bytes, offset, expected.length);
        }
        if (expected.length != 0) {
          for (int capacity : new int[]{0, 1, expected.length / 2, expected.length - 1}) {
            if (capacity >= expected.length) continue;
            var bytes = nioWindow(direct, offset, capacity);
            RuntimeException error = failure(() -> encoder.encode(protocol, bytes));
            check(error instanceof IllegalArgumentException && error.getMessage().equals("Output buffer capacity exceeded"));
            nioState(bytes, offset, offset + capacity, offset);
            guardNio(bytes, offset, capacity);
          }
        }
      }
      var parent = nioWindow(direct, 0, expected.length + 40);
      parent.position(11); parent.limit(11 + expected.length + 25); parent.mark();
      var slice = parent.slice(); slice.position(7); slice.limit(7 + expected.length + 3);
      slice.order(java.nio.ByteOrder.LITTLE_ENDIAN); slice.mark();
      check(encoder.encode(protocol, slice) == expected.length);
      nioState(slice, 7 + expected.length, 7 + expected.length + 3, 7);
      for (int index = 0; index < expected.length; ++index) check(slice.get(7 + index) == expected[index]);
      guardNio(parent, 18, expected.length);
      check(parent.position() == 11 && parent.limit() == 11 + expected.length + 25);
      parent.reset(); check(parent.position() == 11);
      var original = nioWindow(direct, 7, expected.length + 16);
      var readonly = original.asReadOnlyBuffer(); readonly.order(java.nio.ByteOrder.LITTLE_ENDIAN); readonly.mark();
      check(failure(() -> encoder.encode(protocol, readonly)) instanceof java.nio.ReadOnlyBufferException);
      nioState(readonly, 7, 7 + expected.length + 16, 7); guardNio(original, 0, 0);
      var zero = nioWindow(direct, 8, 0);
      if (expected.length == 0) check(encoder.encode(protocol, zero) == 0);
      else check(failure(() -> encoder.encode(protocol, zero)) instanceof IllegalArgumentException);
      nioState(zero, 8, 8, 8); guardNio(zero, 0, 0);
    }
    check(failure(() -> encoder.encode(protocol, null)) instanceof NullPointerException);
    var bytes = nioWindow(false, 7, 32);
    check((RESTRICTED ? IllegalArgumentException.class : NullPointerException.class).isInstance(
        failure(() -> encoder.encode(null, bytes))));
    nioState(bytes, 7, 39, 7); guardNio(bytes, 0, 0);
  }
  /** Keep model failures identical while leaving the NIO cursor and outside bytes unchanged. */
  private static void sameNioFailure(Runnable owning, NioEncoder encoder, Schema.Protocol protocol) {
    RuntimeException expected = failure(owning);
    for (boolean direct : new boolean[]{false, true}) {
      var bytes = nioWindow(direct, 7, 65536);
      RuntimeException actual = failure(() -> encoder.encode(protocol, bytes));
      check(actual.getClass() == expected.getClass() && java.util.Objects.equals(actual.getMessage(), expected.getMessage()));
      nioState(bytes, 7, 65543, 7); guardNio(bytes, 7, 65536);
    }
  }

  /** Exercise bounds, reuse, metadata backpatches, live projections and preserved wire/error contracts. */
  public static void main(String[] args) throws Exception {
    var value = sample();
    byte[] originalPayload = value.payload.clone();
    var sized = new Schema.SizedPayload(); sized.encodedSize = 123; sized.text = "sized"; sized.samples = new int[]{0,42,-1};
    var envelope = new Schema.SizedEnvelope(); envelope.encodedSize = -1L; envelope.payload = sized; envelope.items = new Schema.SizedPayload[]{sized};
    var magic = new Schema.SizedMagic(); magic.text = "magic";
    var wire = new Schema.WireSized(); wire.text = "wire";
    var payload = new Schema.PayloadSized(); payload.encodedSize = 321; payload.text = "payload";
    var source = new Schema.TranslatorSource(); source.number = 41; source.details.label = "projection";
    var generic = new Schema.TranslatorShort(); generic.value = 17;
    for (var protocol : Schema.Protocol.values()) {
      if (RESTRICTED && protocol != Schema.Protocol.BINARY_NONE) {
        byte[] destination = new byte[128]; java.util.Arrays.fill(destination, SENTINEL);
        RuntimeException old = failure(() -> value.encode(protocol));
        RuntimeException added = failure(() -> value.encode(protocol, destination, 7, 100));
        check(old.getClass() == added.getClass()); check(old.getMessage().equals(added.getMessage())); guard(destination, 0, 0);
        source.withProject(view -> { check(failure(() -> view.encode(protocol, destination, 7, 100)) instanceof IllegalArgumentException); guard(destination, 0, 0); return 0; });
        var nio = nioWindow(true, 7, 100);
        RuntimeException nioError = failure(() -> value.encode(protocol, nio));
        check(old.getClass() == nioError.getClass() && old.getMessage().equals(nioError.getMessage()));
        nioState(nio, 7, 107, 7); guardNio(nio, 0, 0);
        source.withProject(view -> { check(failure(() -> view.encode(protocol, nio)) instanceof IllegalArgumentException); nioState(nio, 7, 107, 7); guardNio(nio, 0, 0); return 0; });
        continue;
      }
      byte[] expected = value.encode(protocol);
      verify(value::encode, expected, protocol); verifyNio(value::encode, expected, protocol);
      java.nio.file.Files.write(java.nio.file.Path.of(args[0], protocol.name()+".bin"), expected);
      verify(sized::encode, sized.encode(protocol), protocol); verifyNio(sized::encode, sized.encode(protocol), protocol);
      verify(envelope::encode, envelope.encode(protocol), protocol); verifyNio(envelope::encode, envelope.encode(protocol), protocol);
      verify(magic::encode, magic.encode(protocol), protocol); verifyNio(magic::encode, magic.encode(protocol), protocol);
      verify(wire::encode, wire.encode(protocol), protocol); verifyNio(wire::encode, wire.encode(protocol), protocol);
      verify(payload::encode, payload.encode(protocol), protocol); verifyNio(payload::encode, payload.encode(protocol), protocol);
      check(sized.encodedSize == 123 && envelope.encodedSize == -1L && payload.encodedSize == 321);
      source.withProject(view -> { verify(view::encode, source.materialize().encode(protocol), protocol); verifyNio(view::encode, source.materialize().encode(protocol), protocol); return 0; });
      generic.withProjectValue(view -> { verify(view::encode, view.encode(protocol), protocol); verifyNio(view::encode, view.encode(protocol), protocol); return 0; });
      var empty = new Schema.BufferEmpty(); verify(empty::encode, empty.encode(protocol), protocol); verifyNio(empty::encode, empty.encode(protocol), protocol);
      byte[] reusable = new byte[expected.length + 64];
      var reusableNio = nioWindow(true, 3, expected.length + 61);
      for (int iteration = 0; iteration < 10; ++iteration) {
        value.text = iteration % 2 == 0 ? "short" : "longer replacement text";
        byte[] golden = value.encode(protocol);
        check(value.encode(protocol, reusable, 3, reusable.length-3) == golden.length);
        check(java.util.Arrays.equals(golden, java.util.Arrays.copyOfRange(reusable, 3, 3+golden.length)));
        reusableNio.position(3);
        check(value.encode(protocol, reusableNio) == golden.length);
        nioState(reusableNio, 3 + golden.length, expected.length + 64, 3);
        for (int index = 0; index < golden.length; ++index) check(reusableNio.get(3 + index) == golden[index]);
        guardNio(reusableNio, 3, expected.length + 61);
      }
      value.text = "window \u03c0 \ud83d\ude00";
      value.number = Double.longBitsToDouble(0x7ff8000000000042L);
      if (protocol == Schema.Protocol.JSON) {
        sameFailure(() -> value.encode(protocol), value::encode, protocol); sameNioFailure(() -> value.encode(protocol), value::encode, protocol);
      } else {
        verify(value::encode, value.encode(protocol), protocol); verifyNio(value::encode, value.encode(protocol), protocol);
      }
      value.number = 1.25;
      value.text = "\ud800";
      sameFailure(() -> value.encode(protocol), value::encode, protocol); sameNioFailure(() -> value.encode(protocol), value::encode, protocol);
      value.text = "window \u03c0 \ud83d\ude00";
      var tree = new Schema.BufferTree(); var cursor = tree;
      for (int depth = 0; depth < 70; ++depth) { var child = new Schema.BufferTree(); cursor.children = new Schema.BufferTree[]{child}; cursor = child; }
      sameFailure(() -> tree.encode(protocol), tree::encode, protocol); sameNioFailure(() -> tree.encode(protocol), tree::encode, protocol);
      check(java.util.Arrays.equals(originalPayload, value.payload));
    }
    check(new Schema.FixedBuffer().encode(Schema.Protocol.BINARY_NONE, new byte[4], 0, 4) == 4);
    check(new Schema.NioBuffer().encode(Schema.Protocol.BINARY_NONE, java.nio.ByteBuffer.allocateDirect(4)) == 4);
    System.out.println("PASS: " + checks + " Java output-window assertions");
  }
}
'''

ORACLE = 'package ' + PACKAGE + r''';
public final class WireOracle {
''' + COMMON + r'''
  /** Capture bytes with the previous compiler without using the new output-window API. */
  public static void main(String[] args) throws Exception {
    var value = sample();
    for (var protocol : Schema.Protocol.values()) {
      java.nio.file.Files.write(java.nio.file.Path.of(args[0], protocol.name()+".bin"), value.encode(protocol));
    }
  }
}
'''


def run(command):
    """Fail on missing tools, compiler diagnostics or SDK failures without silently skipping cases."""
    subprocess.run([str(item) for item in command], check=True, timeout=180)


def generate(args, directory, profile='oracle', restricted=False, compiler=None, reuse=True, nio=False):
    """Generate the maintained fixture with one explicit presentation/protocol profile."""
    directory.mkdir(parents=True, exist_ok=True)
    command = [compiler or args.compiler, '--input', ROOT / 'test/resources/java_output_buffer.serializer',
               '--language', 'java', '--output', directory / 'Schema.java', '--java.package', PACKAGE,
               '--java.coding_standard', profile]
    if restricted:
        command += ['--protocols', 'binary_none']
    if reuse:
        command += ['--java.output_buffer_reuse', 'true']
    if nio:
        command += ['--java.nio_output_buffer_reuse', 'true']
    run(command)


def compile_run(args, directory, source, name):
    """Compile Java 17-compatible source strictly and run its actual generated codec APIs."""
    path = directory / (name + '.java')
    path.write_text(source, encoding='utf-8')
    classes = directory / 'classes'
    classes.mkdir(exist_ok=True)
    run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
         '-d', classes, directory / 'Schema.java', path])
    run([args.java, '-cp', classes, PACKAGE + '.' + name, directory])


def main():
    """Keep qualification artifacts under ignored output and preserve baseline protocol bytes."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--baseline-compiler', type=Path)
    parser.add_argument('--java', default='java')
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if not args.build.is_relative_to(ROOT / 'out'):
        parser.error('Qualification output must remain beneath the repository out/ directory')
    args.build.mkdir(parents=True, exist_ok=True)
    for profile in ('oracle', 'google', 'serializer'):
        directory = args.build / profile
        generate(args, directory, profile, nio=True)
        compile_run(args, directory, JAVA.replace('__RESTRICTED__', 'false'), 'Main')
    default = args.build / 'default-api'
    generate(args, default, reuse=False)
    if 'public int encode(Protocol protocol, byte[] destination' in (default / 'Schema.java').read_text(encoding='utf-8'):
        raise AssertionError('Output-window overload must remain opt-in')
    nio_only = args.build / 'nio-only'
    generate(args, nio_only, reuse=False, nio=True)
    nio_source = (nio_only / 'Schema.java').read_text(encoding='utf-8')
    if 'public int encode(Protocol protocol, byte[] destination' in nio_source:
        raise AssertionError('NIO opt-in unexpectedly enabled the array overload')
    compile_run(args, nio_only, 'package '+PACKAGE+';\npublic final class NioOnlyMain {\n'+COMMON+'''
  /** Exercise NIO without enabling the independent byte-array overload. */
  public static void main(String[] args) {
    var value = sample(); var bytes = java.nio.ByteBuffer.allocateDirect(4096);
    for (var protocol : Schema.Protocol.values()) {
      bytes.clear(); int written = value.encode(protocol, bytes); byte[] expected = value.encode(protocol);
      if (written != expected.length || bytes.position() != written) throw new AssertionError();
      for (int index = 0; index < written; ++index) if (bytes.get(index) != expected[index]) throw new AssertionError();
    }
  }
}
''', 'NioOnlyMain')
    array_only = args.build / 'array-only'
    generate(args, array_only)
    if 'public int encode(Protocol protocol, java.nio.ByteBuffer destination)' in (array_only / 'Schema.java').read_text(encoding='utf-8'):
        raise AssertionError('Array opt-in unexpectedly enabled the NIO overload')
    if 'public int encode(Protocol protocol, java.nio.ByteBuffer destination)' in (default / 'Schema.java').read_text(encoding='utf-8'):
        raise AssertionError('NIO overload must remain opt-in')
    restricted = args.build / 'binary-none'
    generate(args, restricted, restricted=True, nio=True)
    compile_run(args, restricted, JAVA.replace('__RESTRICTED__', 'true'), 'Main')
    collision = args.build / 'outer-collision'
    collision.mkdir(exist_ok=True)
    schema = collision / 'model.serializer'
    schema.write_text('serializer version 1; class account { public uint32 value; }', encoding='utf-8')
    run([args.compiler, '--input', schema, '--output', collision / 'FixedBuffer.java', '--language', 'java',
         '--java.output_buffer_reuse', 'true'])
    (collision / 'CollisionMain.java').write_text('''public final class CollisionMain {
  /** Keep the older outer filename usable while executing the opt-in overload. */
  public static void main(String[] args) {
    byte[] bytes = new byte[8]; java.util.Arrays.fill(bytes, (byte)90);
    if (new FixedBuffer.Account().encode(FixedBuffer.Protocol.BINARY_NONE, bytes, 2, 4) != 4 ||
        bytes[0] != 90 || bytes[1] != 90 || bytes[6] != 90 || bytes[7] != 90) throw new AssertionError();
  }
}
''', encoding='utf-8')
    run([args.javac, '--release', '17', '-Xlint:all', '-Werror', '-d', collision / 'classes',
         collision / 'FixedBuffer.java', collision / 'CollisionMain.java'])
    run([args.java, '-cp', collision / 'classes', 'CollisionMain'])
    nio_collision = args.build / 'nio-outer-collision'
    nio_collision.mkdir(exist_ok=True)
    run([args.compiler, '--input', schema, '--output', nio_collision / 'NioBuffer.java', '--language', 'java',
         '--java.nio_output_buffer_reuse', 'true'])
    (nio_collision / 'NioCollisionMain.java').write_text('''public final class NioCollisionMain {
  /** Keep an older enclosing filename usable without adding a new reserved public type. */
  public static void main(String[] args) {
    var bytes = java.nio.ByteBuffer.allocateDirect(8); bytes.position(2); bytes.limit(6);
    if (new NioBuffer.Account().encode(NioBuffer.Protocol.BINARY_NONE, bytes) != 4 || bytes.position() != 6) throw new AssertionError();
  }
}
''', encoding='utf-8')
    run([args.javac, '--release', '17', '-Xlint:all', '-Werror', '-d', nio_collision / 'classes',
         nio_collision / 'NioBuffer.java', nio_collision / 'NioCollisionMain.java'])
    run([args.java, '-cp', nio_collision / 'classes', 'NioCollisionMain'])
    compatibility = args.build / 'native-compatibility'
    compatibility.mkdir(exist_ok=True)
    native = compatibility / 'model.serializer'
    native.write_text('''serializer version 1.6.0;
class legacy_encoder {
  public java {
    /** Preserve an existing application-defined overload when generation has not opted in. */
    public int encode(Protocol protocol, byte[] destination, int offset, int length) { return 17; }
    /** Preserve an application-defined NIO overload unless generation explicitly opts in. */
    public int encode(Protocol protocol, java.nio.ByteBuffer destination) { return 23; }
  }
}
''', encoding='utf-8')
    config = compatibility / 'serializer.ini'
    config.write_text('[java]\noutput_buffer_reuse = true\nnio_output_buffer_reuse = true\n', encoding='utf-8')
    command = [args.compiler, '--input', native, '--config', config, '--language', 'java',
               '--output', compatibility / 'Schema.java', '--java.package', PACKAGE, '--java.naming', 'preserve',
               '--java.output_buffer_reuse', 'false', '--java.nio_output_buffer_reuse', 'false']
    run(command)
    compile_run(args, compatibility, 'package '+PACKAGE+''';
public final class CompatMain {
  /** Verify CLI precedence and the existing native overload without a duplicate generated signature. */
  public static void main(String[] args) {
    var value = new Schema.legacy_encoder();
    if (value.encode(Schema.Protocol.BINARY_NONE, new byte[1], 0, 1) != 17 ||
        value.encode(Schema.Protocol.BINARY_NONE, java.nio.ByteBuffer.allocate(1)) != 23 ||
        value.encode(Schema.Protocol.BINARY_NONE).length != 0) throw new AssertionError();
  }
}
''', 'CompatMain')
    previous = (compatibility / 'Schema.java').read_bytes()
    for option in ['output_buffer_reuse', 'nio_output_buffer_reuse']:
        invalid = command.copy()
        invalid[invalid.index('--java.' + option) + 1] = 'yes'
        rejected = subprocess.run([str(item) for item in invalid], capture_output=True, text=True, timeout=30)
        if rejected.returncode == 0 or 'java.' + option + ' must be true or false' not in rejected.stdout + rejected.stderr:
            raise AssertionError('Invalid output-window option was not rejected: ' + option)
        if previous != (compatibility / 'Schema.java').read_bytes():
            raise AssertionError('Rejected option replaced the previous generated output')
    if args.baseline_compiler:
        baseline = args.build / 'baseline'
        generate(args, baseline, compiler=args.baseline_compiler.resolve(), reuse=False)
        compile_run(args, baseline, ORACLE, 'WireOracle')
        hashes = {}
        for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING'):
            expected = (baseline / (protocol + '.bin')).read_bytes()
            for profile in ('oracle', 'google', 'serializer'):
                if expected != (args.build / profile / (protocol + '.bin')).read_bytes():
                    raise AssertionError('Earlier wire bytes changed: ' + profile + '/' + protocol)
            hashes[protocol] = hashlib.sha256(expected).hexdigest()
        (args.build / 'wire-baseline.json').write_text(json.dumps(hashes, indent=2) + '\n', encoding='utf-8')
    print('PASS: three Java profiles, bounded/failed/reused windows, projections, auto-size, restricted output and collision-safe helpers', flush=True)


if __name__ == '__main__':
    main()
