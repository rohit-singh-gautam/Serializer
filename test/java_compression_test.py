# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated Java whole-message compression against Python's standard providers."""
import argparse
import gzip
from pathlib import Path
import random
import shutil
import struct
import subprocess
import zlib
from compression_window_fixtures import small_zlib_windows


JAVA = r'''package serializer.compression.tests;
import static serializer.compression.tests.Schema.SerializerCompression.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;

/** Exercise actual generated codecs and borrowed compression stream lifecycle. */
public final class Main {
  @FunctionalInterface private interface Checked { void run() throws Exception; }
  /** Fail independently of Java's optional assertion switch. */
  private static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
  /** Require a categorized compression failure rather than an unrelated exception. */
  private static void reject(Code code, Checked operation) throws Exception {
    try { operation.run(); throw new AssertionError("Accepted " + code); }
    catch (Failure failure) { require(failure.code == code, "Wrong failure category: " + failure.code); }
  }
  /** Preserve the original cancellation or I/O exception object. */
  private static void same(Exception expected, Checked operation) throws Exception {
    try { operation.run(); throw new AssertionError("Expected original exception"); }
    catch (Exception failure) { require(failure == expected, "Original exception replaced: " + failure); }
  }
  /** Fenced stream operations must not retry I/O or publish another trailer. */
  private static void fenced(Checked operation) throws Exception {
    try { operation.run(); throw new AssertionError("Failed stream was reusable"); }
    catch (IOException expected) { }
  }
  /** Count borrowed-sink effects and optionally fail after retaining a prefix. */
  private static final class Sink extends ByteArrayOutputStream {
    int calls, maximum, flushes, closes;
    Runnable afterWrite;
    final IOException failure = new IOException("original sink failure");
    boolean fail;
    /** Require array writes so wrapper publication batching is observable. */
    @Override public synchronized void write(byte[] value, int offset, int length) {
      ++calls; maximum = Math.max(maximum, length); super.write(value, offset, length);
      if (afterWrite != null) afterWrite.run();
    }
    /** Observe accidental underlying flushes. */
    @Override public void flush() { ++flushes; }
    /** Observe accidental ownership transfer. */
    @Override public void close() { ++closes; }
  }
  /** Policy/transport callbacks cannot close or reenter an active wrapper and publish afterward. */
  private static void reentrancy() throws Exception {
    for (Format format : List.of(Format.NONE, Format.GZIP, Format.ZLIB, Format.DEFLATE)) {
      Output[] writer = {null}; boolean[] armed = {false}; Sink sink = new Sink();
      EncodeOptions encoding = new EncodeOptions(format, 6, 65536, 65536,
          () -> { if (armed[0]) writer[0].close(); });
      writer[0] = new Output(sink, encoding); writer[0].write(1); armed[0] = true;
      fenced(writer[0]::finish); require(sink.calls == 0, "Closed policy published borrowed output");
      fenced(() -> writer[0].write(2)); writer[0].close(); require(sink.closes == 0, "Borrowed sink closed");
      armed[0] = false; writer[0] = new Output(sink, encoding); armed[0] = true;
      fenced(() -> writer[0].write(1));
      armed[0] = false; writer[0] = new Output(sink, encoding); armed[0] = true;
      fenced(writer[0]::flush); require(sink.flushes == 0, "Policy flush propagated");

      byte[] packed = compress(new byte[]{1}, new EncodeOptions(format));
      Input[] reader = {null}; armed[0] = false;
      DecodeOptions decoding = new DecodeOptions(format, 65536, 65536, 32768,
          () -> { if (armed[0]) reader[0].close(); });
      Source source = new Source(packed); reader[0] = new Input(source, decoding);
      int calls = source.calls; armed[0] = true; fenced(reader[0]::read);
      fenced(reader[0]::finish); require(source.calls == calls && source.closes == 0, "Closed input retried source");
      armed[0] = false; reader[0] = new Input(new Source(compress(new byte[0], new EncodeOptions(format))), decoding);
      armed[0] = true; fenced(reader[0]::finish);

      armed[0] = false; IOException[] first = {null}; sink = new Sink();
      writer[0] = new Output(sink, new EncodeOptions(format, 6, 65536, 65536, () -> {
        if (armed[0]) { armed[0] = false; try { writer[0].flush(); } catch (IOException failure) { first[0] = failure; } }
      }));
      armed[0] = true; Exception actual = null;
      try { writer[0].write(1); } catch (Exception failure) { actual = failure; }
      require(first[0] != null && actual == first[0], "Output reentry lost the first error");
      require(sink.calls == 0, "Reentrant policy published output"); fenced(writer[0]::finish);
      armed[0] = false; first[0] = null;
      reader[0] = new Input(new Source(packed), new DecodeOptions(format, 65536, 65536, 32768, () -> {
        if (armed[0]) { armed[0] = false; try { reader[0].read(); } catch (IOException failure) { first[0] = failure; } }
      }));
      armed[0] = true; actual = null;
      try { reader[0].read(); } catch (Exception failure) { actual = failure; }
      require(first[0] != null && actual == first[0], "Input reentry lost the first error"); fenced(reader[0]::read);
    }
    Output[] writer = {null}; Sink sink = new Sink(); sink.afterWrite = () -> writer[0].close();
    writer[0] = new Output(sink, new EncodeOptions(Format.NONE)); writer[0].write(new byte[32769]);
    fenced(writer[0]::finish); require(sink.calls == 1 && sink.size() == 32768 && sink.closes == 0,
        "Closed borrowed callback continued writing");
    sink = new Sink(); IOException[] first = {null}; RuntimeException later = new RuntimeException("later sink error");
    sink.afterWrite = () -> { try { writer[0].flush(); } catch (IOException failure) { first[0] = failure; } throw later; };
    writer[0] = new Output(sink, new EncodeOptions(Format.NONE)); writer[0].write(1); Exception actual = null;
    try { writer[0].finish(); } catch (Exception failure) { actual = failure; }
    require(first[0] != null && actual == first[0], "Transport reentry replaced the first error");
    require(sink.calls == 1, "Reentrant transport retried output"); fenced(writer[0]::finish);
  }
  /** Keep checked I/O failure separate from ByteArrayOutputStream's narrow signature. */
  private static final class BrokenSink extends OutputStream {
    int calls, closes, flushes; final IOException failure = new IOException("original sink failure");
    /** Fail after retaining the fact that one publication batch reached the sink. */
    @Override public void write(int value) throws IOException { ++calls; throw failure; }
    /** Fail in the actual wrapper publication path. */
    @Override public void write(byte[] value, int offset, int length) throws IOException { ++calls; throw failure; }
    /** Observe forbidden close propagation. */
    @Override public void close() { ++closes; }
    /** Observe forbidden flush propagation. */
    @Override public void flush() { ++flushes; }
  }
  /** Observe EOF probes, provider absence before reads, and borrowed source lifetime. */
  private static final class Source extends ByteArrayInputStream {
    int calls, maximum, closes;
    /** Retain independent fixture bytes without additional parsing. */
    Source(byte[] bytes) { super(bytes); }
    /** Record every requested source batch including EOF and overflow probes. */
    @Override public synchronized int read(byte[] value, int offset, int length) {
      ++calls; maximum = Math.max(maximum, length); return super.read(value, offset, length);
    }
    /** Observe forbidden ownership transfer. */
    @Override public void close() { ++closes; }
  }
  /** Verify no schema/ordinary-codec changes are needed to wrap a whole object. */
  private static void objects() throws Exception {
    Schema.AccountRecord value = new Schema.AccountRecord(); value.accountId = 17; value.label = "hello \uD83D\uDE00";
    for (Schema.Protocol protocol : Schema.Protocol.values()) {
      byte[] ordinary = value.encode(protocol);
      for (Format format : List.of(Format.NONE, Format.GZIP, Format.ZLIB, Format.DEFLATE)) {
        byte[] packed = encode(() -> value.encode(protocol), new EncodeOptions(format));
        Schema.AccountRecord copy = decode(packed, new DecodeOptions(format), bytes -> Schema.AccountRecord.decode(bytes, protocol));
        require(copy.accountId == value.accountId && copy.label.equals(value.label), "Whole-object roundtrip");
        require(Arrays.equals(ordinary, decompress(packed, new DecodeOptions(format))), "Ordinary codec bytes changed");
        byte[] extra = Arrays.copyOf(ordinary, ordinary.length + 1);
        byte[] trailing = compress(extra, new EncodeOptions(format));
        try { decode(trailing, new DecodeOptions(format), bytes -> Schema.AccountRecord.decode(bytes, protocol));
          throw new AssertionError("Inner trailing codec input accepted"); } catch (IllegalArgumentException expected) { }
      }
    }
  }
  /** Withhold a fresh decoded object when its final callback cancels the complete operation. */
  private static void lateDecodeCancellation() throws Exception {
    Schema.AccountRecord value = new Schema.AccountRecord(); value.accountId = 17; value.label = "late cancellation";
    Schema.Protocol protocol = Schema.Protocol.BINARY_INTEGER;
    byte[] packed = compress(value.encode(protocol), new EncodeOptions(Format.GZIP));
    RuntimeException cancellation = new RuntimeException("cancelled by decoded candidate");
    boolean[] armed = {false}; int[] decoded = {0};
    DecodeOptions options = new DecodeOptions(Format.GZIP, packed.length, 65536, 32768,
        () -> { if (armed[0]) throw cancellation; });
    same(cancellation, () -> decode(packed, options, bytes -> {
      Schema.AccountRecord candidate = Schema.AccountRecord.decode(bytes, protocol);
      require(candidate.accountId == value.accountId && candidate.label.equals(value.label), "Late decode candidate");
      ++decoded[0]; armed[0] = true; return candidate;
    }));
    require(decoded[0] == 1, "Late cancellation did not reach exactly one decoded candidate");
  }
  /** Exercise exact byte/window bounds, levels, identity, unavailable providers and callback order. */
  private static void policies(byte[] plain) throws Exception {
    require(!available(Format.ZSTD) && !available(Format.LZ4) && !available(null), "Unavailable providers");
    for (Format absent : List.of(Format.ZSTD, Format.LZ4)) {
      int[] callbacks = {0}; Source source = new Source(plain); Sink sink = new Sink();
      EncodeOptions encoding = new EncodeOptions(absent, 6, plain.length, plain.length, () -> ++callbacks[0]);
      DecodeOptions decoding = new DecodeOptions(absent, plain.length, plain.length, 32768, () -> ++callbacks[0]);
      reject(Code.UNAVAILABLE, () -> encode(() -> { ++callbacks[0]; return plain; }, encoding));
      reject(Code.UNAVAILABLE, () -> decode(plain, decoding, bytes -> { ++callbacks[0]; return bytes; }));
      reject(Code.UNAVAILABLE, () -> new Input(source, decoding));
      reject(Code.UNAVAILABLE, () -> new Output(sink, encoding));
      require(callbacks[0] == 0 && source.calls == 0 && sink.calls == 0, "Unavailable provider touched application state");
    }
    reject(Code.INVALID_OPTIONS, () -> new EncodeOptions(Format.GZIP, -1, 1, 1, null));
    reject(Code.INVALID_OPTIONS, () -> new EncodeOptions(Format.GZIP, 10, 1, 1, null));
    reject(Code.INVALID_OPTIONS, () -> new EncodeOptions(Format.NONE, 6, -1, 1, null));
    reject(Code.INVALID_OPTIONS, () -> new DecodeOptions(Format.NONE, 1, -1, 0, null));
    for (Format format : List.of(Format.NONE, Format.GZIP, Format.ZLIB, Format.DEFLATE)) {
      byte[] packed = compress(plain, new EncodeOptions(format));
      EncodeOptions exact = new EncodeOptions(format, 6, plain.length, packed.length, null);
      DecodeOptions exactDecode = new DecodeOptions(format, packed.length, plain.length, 32768, null);
      require(Arrays.equals(packed, compress(plain, exact)), "Exact encode bounds");
      require(Arrays.equals(plain, decompress(packed, exactDecode)), "Exact decode bounds");
      reject(Code.RESOURCE_LIMIT, () -> compress(plain, new EncodeOptions(format, 6, plain.length - 1, packed.length, null)));
      reject(Code.RESOURCE_LIMIT, () -> compress(plain, new EncodeOptions(format, 6, plain.length, packed.length - 1, null)));
      reject(Code.RESOURCE_LIMIT, () -> decompress(packed, new DecodeOptions(format, packed.length - 1, plain.length, 32768, null)));
      reject(Code.RESOURCE_LIMIT, () -> decompress(packed, new DecodeOptions(format, packed.length, plain.length - 1, 32768, null)));
      if (format != Format.NONE) reject(Code.RESOURCE_LIMIT, () -> new DecodeOptions(format, 1, 1, 32767, null));
      byte[] empty = compress(new byte[0], new EncodeOptions(format, 6, 0, 64, null));
      require(decompress(empty, new DecodeOptions(format, empty.length, 0, 32768, null)).length == 0, "Empty zero-output bound");
      for (int level = 0; level <= 9; ++level) require(Arrays.equals(plain,
          decompress(compress(plain, new EncodeOptions(format, level, plain.length, plain.length + 65536, null)),
              new DecodeOptions(format))), "DEFLATE level " + level);
      RuntimeException cancel = new RuntimeException("original cancellation"); int[] polls = {0};
      same(cancel, () -> compress(plain, new EncodeOptions(format, 6, plain.length, plain.length + 65536,
          () -> { if (++polls[0] == 3) throw cancel; })));
      polls[0] = 0;
      same(cancel, () -> decompress(packed, new DecodeOptions(format, packed.length, plain.length, 32768,
          () -> { if (++polls[0] == 3) throw cancel; })));
    }
  }
  /** Qualify explicit finish, close-without-trailer, borrowed ownership, I/O and failure fences. */
  private static void streams(byte[] plain) throws Exception {
    for (Format format : List.of(Format.NONE, Format.GZIP, Format.ZLIB, Format.DEFLATE)) {
      Sink sink = new Sink(); Output output = new Output(sink, new EncodeOptions(format));
      output.write(plain, 0, 7); output.write(plain, 7, plain.length - 7); output.flush();
      require(sink.size() == 0 && sink.flushes == 0, "Premature sink publication"); output.finish();
      byte[] packed = sink.toByteArray(); require(Arrays.equals(plain, decompress(packed, new DecodeOptions(format))), "Wrapper output");
      fenced(output::finish); fenced(() -> output.write(0)); output.close();
      require(sink.closes == 0 && sink.flushes == 0 && sink.maximum <= 32768, "Borrowed sink or batch violation");
      Sink discarded = new Sink(); Output abandoned = new Output(discarded, new EncodeOptions(format));
      abandoned.write(plain); abandoned.close(); abandoned.close(); fenced(abandoned::finish);
      require(discarded.size() == 0 && discarded.closes == 0, "Close emitted a trailer");
      Source source = new Source(packed); Input input = new Input(source,
          new DecodeOptions(format, packed.length, plain.length, 32768, null));
      require(source.calls > 0 && source.maximum <= 32768, "Source bound or exact EOF probe");
      require(input.read(new byte[0]) == 0 && Arrays.equals(plain, input.readAllBytes()) && input.read() == -1, "Wrapper input");
      input.finish(); input.close(); require(source.closes == 0, "Borrowed source closed"); fenced(input::finish);
      Input unread = new Input(new Source(packed), new DecodeOptions(format));
      reject(Code.INVALID_DATA, unread::finish); fenced(unread::read); unread.close();
      Source over = new Source(Arrays.copyOf(packed, packed.length + 1));
      reject(Code.RESOURCE_LIMIT, () -> new Input(over, new DecodeOptions(format, packed.length, plain.length, 32768, null)));
      BrokenSink broken = new BrokenSink(); Output failed = new Output(broken, new EncodeOptions(format)); failed.write(plain);
      same(broken.failure, failed::finish); int calls = broken.calls; failed.close(); fenced(failed::finish); fenced(() -> failed.write(0));
      require(broken.calls == calls && broken.closes == 0 && broken.flushes == 0, "Failed finish retried borrowed I/O");
      Sink limited = new Sink(); Output shortOutput = new Output(limited, new EncodeOptions(format, 6, plain.length - 1, plain.length + 65536, null));
      reject(Code.RESOURCE_LIMIT, () -> shortOutput.write(plain)); fenced(shortOutput::finish); shortOutput.close();
      require(limited.size() == 0, "Input limit published output");
      Sink small = new Sink(); Output smallOutput = new Output(small, new EncodeOptions(format, 6, plain.length, 0, null)); smallOutput.write(plain);
      reject(Code.RESOURCE_LIMIT, smallOutput::finish); fenced(smallOutput::finish); smallOutput.close(); require(small.size() == 0, "Compression failure published bytes");
    }
    RuntimeException cancelled = new RuntimeException("stream cancellation"); int[] polls = {0}; Sink sink = new Sink();
    Output output = new Output(sink, new EncodeOptions(Format.GZIP, 6, plain.length, plain.length + 65536,
        () -> { if (++polls[0] == 2) throw cancelled; }));
    same(cancelled, () -> output.write(plain)); fenced(output::finish); output.close(); require(sink.size() == 0, "Cancelled writer published bytes");
    Sink partial = new Sink(); Output publishing = new Output(partial,
        new EncodeOptions(Format.NONE, 6, plain.length, plain.length,
            () -> { if (partial.calls > 0) throw cancelled; }));
    publishing.write(plain); same(cancelled, publishing::finish); int publishedCalls = partial.calls;
    require(partial.size() > 0 && partial.size() < plain.length, "External cancellation prefix contract");
    publishing.close(); fenced(publishing::finish); require(partial.calls == publishedCalls && partial.closes == 0, "Cancelled publication retried I/O");
    boolean[] armed = {false}; Source borrowed = new Source(plain);
    Input cancellable = new Input(borrowed, new DecodeOptions(Format.NONE, plain.length, plain.length, 0,
        () -> { if (armed[0]) throw cancelled; }));
    int sourceCalls = borrowed.calls; armed[0] = true; same(cancelled, cancellable::read);
    fenced(cancellable::read); fenced(cancellable::finish); cancellable.close();
    require(borrowed.calls == sourceCalls && borrowed.closes == 0, "Cancelled input retried borrowed I/O");
    IOException sourceFailure = new IOException("original source failure");
    InputStream broken = new InputStream() { @Override public int read() throws IOException { throw sourceFailure; } };
    same(sourceFailure, () -> new Input(broken, new DecodeOptions(Format.GZIP)));
    InputStream stalled = new InputStream() {
      @Override public int read() { return 0; }
      @Override public int read(byte[] bytes, int offset, int length) { return 0; }
    };
    fenced(() -> new Input(stalled, new DecodeOptions(Format.NONE)));
  }
  /** Require every small RFC1950 history boundary and preserve cancellation at every observed poll. */
  private static void smallWindows(Path directory) throws Exception {
    int decisions = 0;
    for (String line : Files.readAllLines(directory.resolve("windows.tsv"))) {
      String[] row = line.split("\t"); byte[] frame = Files.readAllBytes(directory.resolve(row[0]));
      DecodeOptions options = new DecodeOptions(Format.ZLIB, Integer.parseInt(row[3]), Integer.parseInt(row[4]), Integer.parseInt(row[5]), null);
      if (row[2].equals("ok")) require(Arrays.equals(decompress(frame, options), Files.readAllBytes(directory.resolve(row[1]))), row[0]);
      else reject(row[2].equals("invalid_data") ? Code.INVALID_DATA : Code.RESOURCE_LIMIT, () -> decompress(frame, options));
      ++decisions;
    }
    byte[] frame = Files.readAllBytes(directory.resolve("window-6-0.bin")); int[] baseline = {0};
    decompress(frame, new DecodeOptions(Format.ZLIB, frame.length, 8*1024*1024, 32768, () -> ++baseline[0]));
    require(baseline[0] > 10, "Missing bounded grammar cancellation polls");
    RuntimeException original = new RuntimeException("original small-window cancellation");
    for (int boundary = 1; boundary <= baseline[0]; ++boundary) {
      final int stop = boundary; int[] calls = {0};
      same(original, () -> decompress(frame, new DecodeOptions(Format.ZLIB, frame.length, 8*1024*1024, 32768,
          () -> { if (++calls[0] == stop) throw original; })));
    }
    System.out.println("PASS small advertised windows: " + decisions + " decisions, " + baseline[0] + " cancellation boundaries");
  }
  /** Cross-check independent fixtures and emit all four formats for a separate oracle. */
  public static void main(String[] args) throws Exception {
    reentrancy(); Path directory = Path.of(args[0]); smallWindows(directory); int fixtures = 0;
    for (String row : Files.readAllLines(directory.resolve("fixtures.tsv"))) {
      String[] values = row.split("\t"); Format format = Format.valueOf(values[0]);
      byte[] bytes = Files.readAllBytes(directory.resolve(values[2])); ++fixtures;
      if (values[1].equals("invalid")) reject(Code.INVALID_DATA, () -> decompress(bytes, new DecodeOptions(format)));
      else require(Arrays.equals(Files.readAllBytes(directory.resolve(values[3])), decompress(bytes, new DecodeOptions(format))), "Independent input " + values[2]);
    }
    byte[] plain = Files.readAllBytes(directory.resolve("plain.bin")); objects(); lateDecodeCancellation(); policies(plain); streams(plain);
    for (Format format : List.of(Format.NONE, Format.GZIP, Format.ZLIB, Format.DEFLATE)) {
      Files.write(directory.resolve("java-" + format + ".bin"), compress(plain, new EncodeOptions(format)));
    }
    Random random = new Random(731); for (int index = 0; index < 120; ++index) {
      byte[] value = new byte[index < 20 ? index : random.nextInt(8192)]; random.nextBytes(value);
      for (Format format : List.of(Format.GZIP, Format.ZLIB, Format.DEFLATE)) {
        byte[] packed = compress(value, new EncodeOptions(format)); require(Arrays.equals(value, decompress(packed, new DecodeOptions(format))), "Random frame");
        reject(Code.INVALID_DATA, () -> decompress(Arrays.copyOf(packed, packed.length - 1), new DecodeOptions(format)));
        reject(Code.INVALID_DATA, () -> decompress(Arrays.copyOf(packed, packed.length + 1), new DecodeOptions(format)));
      }
    }
    System.out.println("PASS Java compression: " + fixtures + " independent fixtures, objects, exact bounds, framing, cancellation and borrowed streams");
  }
}'''


def run(command, *, cwd=None):
    """Run a bounded provider/compiler check and preserve subprocess diagnostics on failure."""
    subprocess.run([str(value) for value in command], cwd=cwd, check=True, timeout=180)


def fixtures(directory):
    """Create gzip/zlib/raw streams independently, including optional gzip header checks."""
    rows = []

    def add(format_name, value, plain=None):
        """Record one independent framing decision and its expected owning byte message."""
        stem = f"fixture-{len(rows):04d}"
        (directory / f"{stem}.bin").write_bytes(value)
        if plain is None:
            rows.append(f"{format_name}\tinvalid\t{stem}.bin\t-")
        else:
            (directory / f"{stem}.plain").write_bytes(plain)
            rows.append(f"{format_name}\tvalid\t{stem}.bin\t{stem}.plain")

    random_source = random.Random(9182)
    payloads = [b"", b"a", bytes(range(256)), random_source.randbytes(131123), b"A" * 262144]
    for plain in payloads:
        raw_provider = zlib.compressobj(wbits=-15)
        for name, packed in (("GZIP", gzip.compress(plain, mtime=0)),
                             ("ZLIB", zlib.compress(plain)),
                             ("DEFLATE", raw_provider.compress(plain) + raw_provider.flush())):
            add(name, packed, plain)
            add(name, packed + b"\0")
            add(name, packed + packed)
            if len(plain) < 300:
                for size in range(len(packed)):
                    add(name, packed[:size])
            else:
                for removed in (1, 2, 5, min(len(packed), 17)):
                    add(name, packed[:-removed])
            if name != "DEFLATE":
                damaged = bytearray(packed)
                damaged[-1] ^= 128
                add(name, damaged)

    plain = b"gzip optional header\0payload" * 11
    basic = gzip.compress(plain, mtime=0)
    header = basic[:3] + bytes([30]) + basic[4:10] + struct.pack("<H", 5) + b"extra" + b"name\0comment\0"
    framed = header + struct.pack("<H", zlib.crc32(header) & 0xffff) + basic[10:]
    add("GZIP", framed, plain)
    corrupt = bytearray(framed)
    corrupt[len(header)] ^= 1
    add("GZIP", corrupt)
    for offset, value in ((0, 0), (2, 7), (3, 224)):
        corrupt = bytearray(basic)
        corrupt[offset] = value
        add("GZIP", corrupt)
    add("GZIP", basic[:3] + b"\x08" + basic[4:10] + b"unterminated")
    add("GZIP", basic[:3] + b"\x04" + basic[4:10] + b"\xff\xffshort")
    dictionary = zlib.compressobj(zdict=b"shared dictionary")
    add("ZLIB", dictionary.compress(b"shared dictionary payload") + dictionary.flush())
    (directory / "fixtures.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")
    (directory / "plain.bin").write_bytes(random_source.randbytes(262181))
    return len(rows)


def main():
    """Generate real Java SDKs, compile with warnings as errors, and run independent wire oracles."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--java", default=shutil.which("java"))
    args = parser.parse_args()
    if not args.javac or not args.java:
        parser.error("Java 17+ javac and java are required")
    directory = args.build.resolve()
    root = Path(__file__).resolve().parent.parent
    if directory == root or root in directory.parents and "out" not in directory.relative_to(root).parts:
        parser.error("In-repository output must remain beneath ignored out")
    directory.mkdir(parents=True, exist_ok=True)
    count = fixtures(directory)
    from csharp_compression_test import extra_fixtures
    count += extra_fixtures(directory)
    small_zlib_windows(directory)
    schema = directory / "model.serializer"
    schema.write_text("serializer version 1;\nclass account_record stable_ids {\n"
                      "  public uint32 account_id (1);\n  public string label (2);\n}\n", encoding="utf-8")
    generated = directory / "Schema.java"
    run([args.compiler.resolve(), "--input", schema, "--output", generated, "--language", "java",
         "--java.package", "serializer.compression.tests"])
    (directory / "Main.java").write_text(JAVA, encoding="utf-8")
    classes = directory / "classes"
    classes.mkdir(exist_ok=True)
    run([args.javac, "--release", "17", "-encoding", "UTF-8", "-Xlint:all", "-Werror",
         "-d", classes, generated, directory / "Main.java"])
    run([args.java, "-cp", classes, "serializer.compression.tests.Main", directory])
    plain = (directory / "plain.bin").read_bytes()
    for name, bits in (("GZIP", 31), ("ZLIB", 15), ("DEFLATE", -15)):
        packed = (directory / f"java-{name}.bin").read_bytes()
        decoder = zlib.decompressobj(bits)
        if decoder.decompress(packed) + decoder.flush() != plain or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
            raise AssertionError(f"Independent Python decoder rejected Java {name}")
    if (directory / "java-NONE.bin").read_bytes() != plain:
        raise AssertionError("Identity framing changed ordinary bytes")

    # Helper names must adapt without rejecting or renaming an existing schema declaration.
    collision = directory / "collision.serializer"
    collision.write_text("serializer version 1; class SerializerCompression {} "
                         "class SerializerCompression_2 {} class System {}", encoding="utf-8")
    preserved = directory / "Collision.java"
    run([args.compiler.resolve(), "--input", collision, "--output", preserved, "--language", "java", "--java.naming", "preserve"])
    if "generated helper name: SerializerCompression_3" not in preserved.read_text(encoding="utf-8"):
        raise AssertionError("Collision-safe Java compression helper name missing")
    run([args.javac, "--release", "17", "-Xlint:all", "-Werror", "-d", classes, preserved])
    # Java nested declarations may not repeat any enclosing output-class name.
    outer_sources = []
    for outer in ("SerializerCompression", "Format", "Code", "Failure", "EncodeOptions", "DecodeOptions", "Buffer", "Output", "Input",
                  "ZstdProvider", "ZstdSource", "Lz4Provider"):
        output = directory / f"{outer}.java"
        run([args.compiler.resolve(), "--input", schema, "--output", output, "--language", "java"])
        outer_sources.append(output)
    run([args.javac, "--release", "17", "-Xlint:all", "-Werror", "-d", classes, *outer_sources])
    print(f"PASS: {count} independent input decisions, Python output oracles, collision-safe naming and generated Java lifecycle checks", flush=True)


if __name__ == "__main__":
    main()
