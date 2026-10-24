# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Qualify real Java native streaming, channel failures, preflight size patches and packed output."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
JAVA = r'''
import java.io.*;
import java.nio.*;
import java.nio.channels.*;
import java.util.*;
public final class Main {
  /** Fail immediately without relying on the JVM assertion flag. */
  static void check(boolean condition) { if (!condition) throw new AssertionError(); }
  /** Exercise each established wire format and the bounded streaming failure contracts. */
  public static void main(String[] args) throws Exception {
    var value = new Schema.BufferRecord();
    value.payload = new byte[1024 * 1024]; Arrays.fill(value.payload, (byte)37);
    value.text = "mapped \u03c0"; value.number = 1.25;
    for (var protocol : Schema.Protocol.values()) {
      byte[] expected = value.encode(protocol);
      var stream = new ByteArrayOutputStream();
      check(value.encode(protocol, stream) == expected.length);
      check(Arrays.equals(expected, stream.toByteArray()));
      stream.reset();
      var channel = new WritableByteChannel() {
        boolean open = true;
        /** Force partial writes so the generated adapter must preserve every byte. */
        public int write(ByteBuffer bytes) { int count = Math.min(bytes.remaining(), 7); for (int i=0;i<count;i++) stream.write(bytes.get()); return count; }
        public boolean isOpen() { return open; }
        public void close() { throw new AssertionError("Caller channel closed"); }
      };
      check(value.encode(protocol, channel) == expected.length);
      check(Arrays.equals(expected, stream.toByteArray()));
      var zero = new WritableByteChannel() {
        public int write(ByteBuffer bytes) { return 0; }
        public boolean isOpen() { return true; }
        public void close() { throw new AssertionError(); }
      };
      try { value.encode(protocol, zero); throw new AssertionError(); } catch (IOException expectedFailure) { check(expectedFailure.getMessage().contains("no progress")); }
      IOException injected = new IOException("injected");
      var failing = new OutputStream() {
        public void write(int ignored) throws IOException { throw injected; }
        public void write(byte[] bytes, int offset, int length) throws IOException { throw injected; }
        public void flush() { throw new AssertionError("Caller sink flushed"); }
        public void close() { throw new AssertionError("Caller sink closed"); }
      };
      try { value.encode(protocol, failing); throw new AssertionError(); } catch (IOException failure) { check(failure == injected); }
    }
    var sized = new Schema.StreamSized(); sized.payload = new byte[17000];
    for (var protocol : Schema.Protocol.values()) {
      var out = new ByteArrayOutputStream(); sized.encode(protocol, out);
      check(Arrays.equals(sized.encode(protocol), out.toByteArray()));
    }
    var envelope = new Schema.SizedEnvelope(); envelope.payload.text = "nested";
    envelope.payload.samples = new int[]{1, 150, 300};
    envelope.items = new Schema.SizedPayload[]{envelope.payload};
    var source = new Schema.TranslatorSource(); source.number = 41; source.details.label = "projection";
    for (var protocol : Schema.Protocol.values()) {
      var out = new ByteArrayOutputStream(); envelope.encode(protocol, out);
      check(Arrays.equals(envelope.encode(protocol), out.toByteArray()));
      source.withProject(view -> {
        try {
          var projection = new ByteArrayOutputStream(); view.encode(protocol, projection);
          check(Arrays.equals(source.materialize().encode(protocol), projection.toByteArray()));
          return 0;
        } catch (IOException error) { throw new AssertionError(error); }
      });
    }
    var packed = new Schema.StreamPacked(); packed.code = 37; packed.amount = 42; packed.number = 1.25;
    for (ByteBuffer destination : new ByteBuffer[]{ByteBuffer.allocate(64), ByteBuffer.allocateDirect(64)}) {
      destination.order(ByteOrder.BIG_ENDIAN).position(3).limit(3 + Schema.StreamPacked.PACKED_SIZE_BYTES);
      check(packed.pack(destination) == Schema.StreamPacked.PACKED_SIZE_BYTES);
      check(destination.position() == destination.limit() && destination.order() == ByteOrder.BIG_ENDIAN);
      destination.position(3);
      var copy = Schema.StreamPacked.unpack(destination);
      check(copy.code == packed.code && copy.amount == packed.amount && copy.number == packed.number);
      destination.position(3); byte[] bytes = new byte[destination.remaining()]; destination.get(bytes);
      check(Arrays.equals(bytes, packed.encode(Schema.Protocol.BINARY_NONE)));
    }
    try { packed.pack(ByteBuffer.allocate(0)); throw new AssertionError(); } catch (BufferOverflowException expectedFailure) { }
    check(Schema.StreamPacked.PACKED_SIZE_BYTES == Byte.BYTES + Integer.BYTES + Double.BYTES);
    ByteBuffer immutable = ByteBuffer.allocate(64).asReadOnlyBuffer(); immutable.position(3);
    try { packed.pack(immutable); throw new AssertionError(); } catch (ReadOnlyBufferException expectedFailure) { check(immutable.position() == 3); }
    System.out.println("PASS: Java streaming/channels, four-protocol wire parity, short/zero writes, I/O identity, size patches and packed heap/direct buffers");
  }
}
'''


def run(command):
    """Use SDK processes directly and fail on any warning or runtime assertion."""
    subprocess.run([str(value) for value in command], check=True, timeout=240)


def main():
    """Compile fresh generators and execute the actual Java APIs with bounded scratch output."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--java', default='java')
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    schema = args.build.resolve() / 'stream.serializer'
    dependency = Path(os.path.relpath(ROOT / 'test/resources/java_output_buffer.serializer', schema.parent)).as_posix()
    schema.write_text('serializer version 1.24.0;\ninclude ' + dependency + ';\n'
        'class stream_packed packed { public uint8 code; public uint32 amount; public double number; }\n'
        'class stream_sized { public uint32 size auto_size(binary_none, binary_integer, binary_string); public array uint8 payload; }\n')
    run([args.compiler, '--input', schema, '--language', 'java', '--output', args.build / 'Schema.java',
         '--java.stream_output', 'true'])
    (args.build / 'Main.java').write_text(JAVA)
    run([args.javac, '--release', '17', '-Xlint:all', '-Werror', '-d', args.build, args.build / 'Schema.java', args.build / 'Main.java'])
    run([args.java, '-cp', args.build, 'Main'])
    for outer in ('CountBuffer', 'StreamBuffer'):
        output = args.build / (outer + '.java')
        run([args.compiler, '--input', schema, '--language', 'java', '--output', output,
             '--java.stream_output', 'true'])
        run([args.javac, '--release', '17', '-Xlint:all', '-Werror', '-d', args.build, output])


if __name__ == '__main__':
    main()
