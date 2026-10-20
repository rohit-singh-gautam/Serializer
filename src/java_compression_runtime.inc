// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"java(
/** Bounded whole-message compression; generated helper name: @compression_name@. */
public static final class @compression_name@ {
  private static final int DEFAULT_BYTES = 64 * 1024 * 1024;
  private static final int DEFAULT_WINDOW_BYTES = 8 * 1024 * 1024;
  private static final int DEFLATE_WINDOW_BYTES = 32 * 1024;
  private static final int BATCH_BYTES = 32 * 1024;
  private static final int GZIP_HEADER_BYTES = 10;
  private static final int GZIP_TRAILER_BYTES = 8;

  /** Static helpers have no shared mutable provider state. */
  private @compression_name@() {}

  /** Select framing explicitly; optional providers are discovered only when selected. */
  public enum Format { NONE, GZIP, ZLIB, DEFLATE, ZSTD, LZ4 }
  /** Compression failures contain a category and no payload excerpts. */
  public enum Code { UNAVAILABLE, INVALID_OPTIONS, INVALID_DATA, RESOURCE_LIMIT }

  /** Preserve stream, allocation, callback and ordinary codec exceptions separately. */
  public static final class Failure extends java.lang.IllegalArgumentException {
    private static final long serialVersionUID = 1L;
    public final Code code;
    /** Attach the stable failure category to a payload-free diagnostic. */
    private Failure(Code code, String message) { super(message); this.code = code; }
  }

  /** Immutable logical byte bounds; callback polling is cooperative, not preemptive. */
  public static final class EncodeOptions {
    public final Format format;
    public final int level;
    public final int maxInputBytes;
    public final int maxOutputBytes;
    public final java.lang.Runnable checkCancelled;
    public final boolean checksum;
    /** Use Zstandard level three, LZ4 fast mode or DEFLATE level six and 64 MiB byte bounds. */
    public EncodeOptions(Format format) { this(format, format == Format.ZSTD ? 3 : format == Format.LZ4 ? 0 : 6, DEFAULT_BYTES, DEFAULT_BYTES, null); }
    /** Validate every option before any serialization or underlying I/O occurs. */
    public EncodeOptions(Format format, int level, int maxInputBytes, int maxOutputBytes,
        java.lang.Runnable checkCancelled) {
      this(format, level, maxInputBytes, maxOutputBytes, checkCancelled, true);
    }
    /** Choose optional-frame content checksums; mandatory DEFLATE framing checks remain enabled. */
    public EncodeOptions(Format format, int level, int maxInputBytes, int maxOutputBytes,
        java.lang.Runnable checkCancelled, boolean checksum) {
      if (format == null || (format != Format.ZSTD && (level < 0 || level > 9)) ||
          maxInputBytes < 0 || maxOutputBytes < 0) {
        throw new Failure(Code.INVALID_OPTIONS, "Invalid compression encoding options");
      }
      this.format = format; this.level = level; this.maxInputBytes = maxInputBytes;
      this.maxOutputBytes = maxOutputBytes; this.checkCancelled = checkCancelled;
      this.checksum = checksum;
    }
  }

  /** Immutable compressed/expanded/window admission policy, independent of codec limits. */
  public static final class DecodeOptions {
    public final Format format;
    public final int maxInputBytes;
    public final int maxOutputBytes;
    public final int maxWindowBytes;
    public final java.lang.Runnable checkCancelled;
    /** Use 64 MiB input/output and an 8 MiB history budget. */
    public DecodeOptions(Format format) {
      this(format, DEFAULT_BYTES, DEFAULT_BYTES, DEFAULT_WINDOW_BYTES, null);
    }
    /** Require a complete DEFLATE window for every DEFLATE-based framing. */
    public DecodeOptions(Format format, int maxInputBytes, int maxOutputBytes,
        int maxWindowBytes, java.lang.Runnable checkCancelled) {
      if (format == null || maxInputBytes < 0 || maxOutputBytes < 0 || maxWindowBytes < 0) {
        throw new Failure(Code.INVALID_OPTIONS, "Invalid compression decoding options");
      }
      if ((format == Format.GZIP || format == Format.ZLIB || format == Format.DEFLATE) &&
          maxWindowBytes < DEFLATE_WINDOW_BYTES) {
        throw new Failure(Code.RESOURCE_LIMIT, "Compression history limit exceeded");
      }
      this.format = format; this.maxInputBytes = maxInputBytes; this.maxOutputBytes = maxOutputBytes;
      this.maxWindowBytes = maxWindowBytes; this.checkCancelled = checkCancelled;
    }
  }

  /** Probe optional deployment lazily without touching application callbacks or streams. */
  public static boolean available(Format format) {
    if (format == Format.ZSTD) {
      try { zstdProvider(); return true; }
      catch (Failure failure) { if (failure.code == Code.UNAVAILABLE) return false; throw failure; }
    }
    if (format == Format.LZ4) {
      try { lz4Provider(); return true; }
      catch (Failure failure) { if (failure.code == Code.UNAVAILABLE) return false; throw failure; }
    }
    return format == Format.NONE || format == Format.GZIP || format == Format.ZLIB || format == Format.DEFLATE;
  }
  /** Reject absent providers before callbacks, serialization, or source/sink access. */
  private static void requireAvailable(Format format) {
    if (!available(format)) throw new Failure(Code.UNAVAILABLE, "Compression provider is unavailable");
  }
  /** Validate provider-specific levels before serializers, cancellation hooks or borrowed I/O. */
  private static void requireEncoding(EncodeOptions options) {
    requireAvailable(options.format);
    if (options.format == Format.ZSTD) zstdProvider().validateLevel(options.level);
  }
  /** Admit a native history policy before reading an application source. */
  private static void requireDecoding(DecodeOptions options) {
    requireAvailable(options.format);
    if (options.format == Format.ZSTD && options.maxWindowBytes < 1024) {
      throw new Failure(Code.RESOURCE_LIMIT, "Compression history limit exceeded");
    }
  }
  /** Propagate the application's original cancellation exception unchanged. */
  private static void check(java.lang.Runnable callback) { if (callback != null) callback.run(); }
  /** Reject an advertised or actual byte count before allocation or append. */
  private static void admit(int count, int bound) {
    if (count > bound) throw new Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded");
  }
  /** Bound a provider call while allowing one byte to detect exact output-limit overflow. */
  private static int outputCapacity(int size, int bound) {
    return (int) java.lang.Math.min((long) BATCH_BYTES, (long) bound - size + 1);
  }
  /** Stage logical bytes without allowing a buffer growth past the selected bound. */
  private static final class Buffer extends java.io.ByteArrayOutputStream {
    private final int bound;
    /** Retain the bound without allocating the advertised maximum. */
    Buffer(int bound) { super(java.lang.Math.min(bound, BATCH_BYTES)); this.bound = bound; }
    /** Check subtraction before delegating any array growth to the JDK. */
    @Override public synchronized void write(byte[] bytes, int offset, int length) {
      java.util.Objects.checkFromIndexSize(offset, length, bytes.length);
      if (length > bound - count) throw new Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded");
      super.write(bytes, offset, length);
    }
    /** Charge individual bytes through the same logical output bound. */
    @Override public synchronized void write(int value) {
      if (count == bound) throw new Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded");
      super.write(value);
    }
  }

  /** Compress exactly one complete byte message into the selected standard framing. */
  public static byte[] compress(byte[] input, EncodeOptions options) {
    java.util.Objects.requireNonNull(options); requireEncoding(options);
    java.util.Objects.requireNonNull(input); admit(input.length, options.maxInputBytes);
    check(options.checkCancelled);
    if (options.format == Format.ZSTD) return zstdCompress(input, options);
    if (options.format == Format.LZ4) return lz4Compress(input, options);
    Buffer output = new Buffer(options.maxOutputBytes);
    if (options.format == Format.NONE) {
      for (int offset = 0; offset < input.length;) {
        check(options.checkCancelled); int count = java.lang.Math.min(BATCH_BYTES, input.length - offset);
        output.write(input, offset, count); offset += count; check(options.checkCancelled);
      }
      return output.toByteArray();
    }
    if (options.format == Format.GZIP) {
      output.write(new byte[] {31, (byte)139, 8, 0, 0, 0, 0, 0, 0, (byte)255}, 0, GZIP_HEADER_BYTES);
    }
    java.util.zip.CRC32 checksum = new java.util.zip.CRC32(); byte[] batch = new byte[BATCH_BYTES];
    java.util.zip.Deflater provider = new java.util.zip.Deflater(options.level, options.format != Format.ZLIB);
    try {
      for (int offset = 0; offset < input.length;) {
        int count = java.lang.Math.min(BATCH_BYTES, input.length - offset);
        check(options.checkCancelled); provider.setInput(input, offset, count);
        if (options.format == Format.GZIP) checksum.update(input, offset, count);
        offset += count;
        while (!provider.needsInput()) {
          long before = provider.getBytesRead(); check(options.checkCancelled);
          int produced = provider.deflate(batch, 0, outputCapacity(output.size(), options.maxOutputBytes));
          check(options.checkCancelled); output.write(batch, 0, produced);
          if (produced == 0 && provider.getBytesRead() == before && !provider.needsInput()) {
            throw new Failure(Code.INVALID_DATA, "Compression provider made no progress");
          }
        }
      }
      provider.finish();
      while (!provider.finished()) {
        check(options.checkCancelled);
        int produced = provider.deflate(batch, 0, outputCapacity(output.size(), options.maxOutputBytes));
        check(options.checkCancelled); output.write(batch, 0, produced);
        if (produced == 0 && !provider.finished()) {
          throw new Failure(Code.INVALID_DATA, "Compression provider made no progress");
        }
      }
      if (options.format == Format.GZIP) {
        writeLittle(output, checksum.getValue()); writeLittle(output, input.length);
      }
      check(options.checkCancelled); return output.toByteArray();
    } finally { provider.end(); }
  }

  /** Encode the ordinary codec bytes only after provider admission succeeds. */
  public static byte[] encode(java.util.function.Supplier<byte[]> encoder, EncodeOptions options) {
    java.util.Objects.requireNonNull(options); requireEncoding(options); check(options.checkCancelled);
    return compress(java.util.Objects.requireNonNull(encoder).get(), options);
  }
)java"
R"java(
  /** Decode a fresh value only after complete frame, checksum and resource validation. */
  public static <T> T decode(byte[] input, DecodeOptions options, java.util.function.Function<byte[], T> decoder) {
    byte[] plain = decompress(input, options);
    T result = java.util.Objects.requireNonNull(decoder).apply(plain);
    check(options.checkCancelled);
    return result;
  }
  /** Write one standard little-endian CRC/size word independent of host layout. */
  private static void writeLittle(Buffer output, long value) {
    for (int index = 0; index < java.lang.Integer.BYTES; ++index) output.write((int) (value >>> (index * 8)));
  }
  /** Check a byte range before header indexing or offset arithmetic. */
  private static void range(byte[] bytes, int offset, int count) {
    if (offset < 0 || offset > bytes.length || count < 0 || count > bytes.length - offset) {
      throw new Failure(Code.INVALID_DATA, "Truncated compressed message");
    }
  }
  /** Read an unsigned little-endian word after explicit range validation. */
  private static long little(byte[] bytes, int offset, int count) {
    range(bytes, offset, count); long value = 0;
    for (int index = 0; index < count; ++index) value |= (bytes[offset + index] & 255L) << (index * 8);
    return value;
  }
  /** Skip a bounded gzip optional string while polling independently of its contents. */
  private static int terminated(byte[] bytes, int offset, java.lang.Runnable callback) {
    for (int index = offset; index < bytes.length; ++index) {
      if ((index - offset) % BATCH_BYTES == 0) check(callback);
      if (bytes[index] == 0) return index + 1;
    }
    throw new Failure(Code.INVALID_DATA, "Unterminated gzip header");
  }
  /** Validate optional gzip header fields and FHCRC without trusting lengths. */
  private static int gzipHeader(byte[] bytes, java.lang.Runnable callback) {
    range(bytes, 0, GZIP_HEADER_BYTES);
    int flags = bytes[3] & 255;
    if (bytes[0] != 31 || (bytes[1] & 255) != 139 || bytes[2] != 8 || (flags & 224) != 0) {
      throw new Failure(Code.INVALID_DATA, "Invalid gzip header");
    }
    int offset = GZIP_HEADER_BYTES;
    if ((flags & 4) != 0) { int count = (int) little(bytes, offset, 2); offset += 2; range(bytes, offset, count); offset += count; }
    if ((flags & 8) != 0) offset = terminated(bytes, offset, callback);
    if ((flags & 16) != 0) offset = terminated(bytes, offset, callback);
    if ((flags & 2) != 0) {
      java.util.zip.CRC32 header = new java.util.zip.CRC32();
      for (int position = 0; position < offset;) {
        check(callback); int count = java.lang.Math.min(BATCH_BYTES, offset - position);
        header.update(bytes, position, count); position += count;
      }
      if (little(bytes, offset, 2) != (header.getValue() & 65535)) {
        throw new Failure(Code.INVALID_DATA, "Invalid gzip header checksum");
      }
      offset += 2;
    }
    return offset;
  }

  /** Validate one exact stream; trailing bytes and concatenated members always reject. */
  public static byte[] decompress(byte[] input, DecodeOptions options) {
    java.util.Objects.requireNonNull(options); requireDecoding(options);
    java.util.Objects.requireNonNull(input); admit(input.length, options.maxInputBytes); check(options.checkCancelled);
    if (options.format == Format.ZSTD) return zstdDecompress(input, options);
    if (options.format == Format.LZ4) return lz4Decompress(input, options);
    if (options.format == Format.ZLIB) validateZlibWindow(input, options);
    Buffer output = new Buffer(options.maxOutputBytes);
    if (options.format == Format.NONE) {
      for (int offset = 0; offset < input.length;) {
        check(options.checkCancelled); int count = java.lang.Math.min(BATCH_BYTES, input.length - offset);
        output.write(input, offset, count); offset += count; check(options.checkCancelled);
      }
      return output.toByteArray();
    }
    int start = options.format == Format.GZIP ? gzipHeader(input, options.checkCancelled) : 0;
    java.util.zip.CRC32 checksum = new java.util.zip.CRC32(); byte[] batch = new byte[BATCH_BYTES];
    java.util.zip.Inflater provider = new java.util.zip.Inflater(options.format != Format.ZLIB);
    int position = start; boolean dummySupplied = false;
    try {
      while (!provider.finished()) {
        if (provider.needsInput()) {
          if (position < input.length) {
            int count = java.lang.Math.min(BATCH_BYTES, input.length - position);
            provider.setInput(input, position, count); position += count;
          } else if (options.format != Format.ZLIB && !dummySupplied) {
            // The JDK nowrap contract requires a final lookahead byte; it may not belong to the stream.
            provider.setInput(new byte[1]); dummySupplied = true;
          } else { throw new Failure(Code.INVALID_DATA, "Truncated compressed message"); }
        }
        check(options.checkCancelled); long before = provider.getBytesRead();
        int produced;
        try { produced = provider.inflate(batch, 0, outputCapacity(output.size(), options.maxOutputBytes)); }
        catch (java.util.zip.DataFormatException failure) { throw new Failure(Code.INVALID_DATA, "Invalid compressed message"); }
        check(options.checkCancelled); output.write(batch, 0, produced);
        if (options.format == Format.GZIP) checksum.update(batch, 0, produced);
        if (provider.needsDictionary()) throw new Failure(Code.INVALID_DATA, "Compressed dictionaries are unsupported");
        if (produced == 0 && provider.getBytesRead() == before && !provider.needsInput() && !provider.finished()) {
          throw new Failure(Code.INVALID_DATA, "Compression provider made no progress");
        }
      }
      long consumed = provider.getBytesRead();
      if (consumed > input.length - start) throw new Failure(Code.INVALID_DATA, "Truncated compressed message");
      int end = start + (int) consumed;
      if (options.format == Format.GZIP) {
        range(input, end, GZIP_TRAILER_BYTES);
        if (little(input, end, 4) != checksum.getValue() || little(input, end + 4, 4) != output.size()) {
          throw new Failure(Code.INVALID_DATA, "Invalid gzip checksum or size");
        }
        end += GZIP_TRAILER_BYTES;
      }
      if (end != input.length) throw new Failure(Code.INVALID_DATA, "Trailing compressed input");
      check(options.checkCancelled); return output.toByteArray();
    } finally { provider.end(); }
  }

  /** Read bounded EOF-delimited input, including a one-byte overflow probe at the limit. */
  private static byte[] readSource(java.io.InputStream source, DecodeOptions options) throws java.io.IOException {
    Buffer input = new Buffer(options.maxInputBytes); byte[] batch = new byte[BATCH_BYTES];
    while (true) {
      check(options.checkCancelled);
      int count = source.read(batch, 0, outputCapacity(input.size(), options.maxInputBytes));
      check(options.checkCancelled);
      if (count < 0) return input.toByteArray();
      if (count == 0) throw new java.io.IOException("Compression source made no progress");
      input.write(batch, 0, count);
    }
  }
)java"
R"java(

  /** Borrow a sink; stage one bounded message, explicitly finish, and never close or flush it. */
  public static final class Output extends java.io.OutputStream {
    private final java.io.OutputStream sink;
    private final EncodeOptions options;
    private final EncodeOptions runOptions;
    private Buffer input;
    private boolean failed;
    private boolean finished;
    private boolean closed;
    private boolean busy;
    private java.lang.Throwable failure;
    /** Admit the provider before retaining the borrowed sink; construction performs no I/O. */
    public Output(java.io.OutputStream sink, EncodeOptions options) {
      java.util.Objects.requireNonNull(options); requireEncoding(options);
      this.sink = java.util.Objects.requireNonNull(sink); this.options = options;
      this.input = new Buffer(options.maxInputBytes);
      this.runOptions = new EncodeOptions(options.format, options.level, options.maxInputBytes,
          options.maxOutputBytes, this::providerPoll, options.checksum);
    }
    /** Fence a nested operation even if its caller catches the immediate rejection. */
    private void begin() throws java.io.IOException {
      if (failed || finished || closed || busy) {
        java.io.IOException cause = new java.io.IOException("Compression output is no longer writable", failure);
        if (busy && !failed) { failed = true; failure = cause; }
        throw cause;
      }
      busy = true;
    }
    /** Preserve the first original callback, nested-operation or transport exception. */
    private java.io.IOException fail(java.lang.Throwable cause) {
      if (!failed) { failed = true; failure = cause; }
      if (failure instanceof java.lang.RuntimeException) throw (java.lang.RuntimeException) failure;
      if (failure instanceof java.lang.Error) throw (java.lang.Error) failure;
      return (java.io.IOException) failure;
    }
    /** Check both sides of callbacks, which can close or reenter synchronously. */
    private void poll() throws java.io.IOException {
      if (failed) throw fail(failure);
      if (finished || closed) throw new java.io.IOException("Compression output is no longer writable");
      check(options.checkCancelled);
      if (failed) throw fail(failure);
      if (finished || closed) throw new java.io.IOException("Compression output is no longer writable");
    }
    /** Bridge only this wrapper's checked lifecycle error through the provider's Runnable hook. */
    private void providerPoll() {
      try { poll(); }
      catch (java.io.IOException cause) {
        if (!failed) { failed = true; failure = cause; }
        throw new java.io.UncheckedIOException(cause);
      }
    }
    /** Stage one byte through the same batch-independent input admission policy. */
    @Override public void write(int value) throws java.io.IOException {
      begin();
      try { poll(); input.write(value); poll(); }
      catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error cause) { input = null; throw fail(cause); }
      finally { busy = false; }
    }
    /** Stage codec bytes without publishing a compressed prefix. */
    @Override public void write(byte[] bytes, int offset, int length) throws java.io.IOException {
      begin();
      try {
        java.util.Objects.checkFromIndexSize(offset, length, bytes.length);
        if (length > options.maxInputBytes - input.size()) throw new Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded");
        for (int position = 0; position < length;) {
          poll(); int count = java.lang.Math.min(BATCH_BYTES, length - position);
          input.write(bytes, offset + position, count); position += count; poll();
        }
        poll();
      } catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error cause) { input = null; throw fail(cause); }
      finally { busy = false; }
    }
    /** Validate compression before publishing bounded batches; external I/O may retain a prefix. */
    public void finish() throws java.io.IOException {
      begin();
      try {
        poll(); byte[] bytes = compress(input.toByteArray(), runOptions); poll(); input = null;
        for (int offset = 0; offset < bytes.length;) {
          poll(); int count = java.lang.Math.min(BATCH_BYTES, bytes.length - offset);
          sink.write(bytes, offset, count); offset += count; poll();
        }
        poll(); finished = true;
      } catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error cause) {
        input = null; throw fail(cause);
      } finally { busy = false; }
    }
    /** Do not flush the borrowed sink or finalize a message implicitly. */
    @Override public void flush() throws java.io.IOException {
      begin();
      try { poll(); }
      catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error cause) { input = null; throw fail(cause); }
      finally { busy = false; }
    }
    /** Release staged bytes without trailers, underlying close, or any I/O, even after failure. */
    @Override public void close() { closed = true; input = null; }
  }

  /** Borrow an EOF-delimited source and validate one complete message before exposing codec bytes. */
  public static final class Input extends java.io.InputStream {
    private byte[] bytes;
    private int position;
    private final DecodeOptions options;
    private boolean failed;
    private boolean closed;
    private boolean busy;
    private java.lang.Throwable failure;
    /** Unavailable providers reject before reading; frame failures publish no decoded prefix. */
    public Input(java.io.InputStream source, DecodeOptions options) throws java.io.IOException {
      java.util.Objects.requireNonNull(options); requireDecoding(options);
      this.options = options; this.bytes = decompress(readSource(java.util.Objects.requireNonNull(source), options), options);
    }
    /** Reject nested calls before they consume staged bytes and retain the first active failure. */
    private void begin() throws java.io.IOException {
      if (failed || closed || busy) {
        java.io.IOException cause = new java.io.IOException("Compression input is no longer readable", failure);
        if (busy && !failed) { failed = true; failure = cause; }
        throw cause;
      }
      busy = true;
    }
    /** Keep checked I/O, application runtime and Error identities distinct. */
    private java.io.IOException fail(java.lang.Throwable cause) {
      if (!failed) { failed = true; failure = cause; }
      if (failure instanceof java.lang.RuntimeException) throw (java.lang.RuntimeException) failure;
      if (failure instanceof java.lang.Error) throw (java.lang.Error) failure;
      return (java.io.IOException) failure;
    }
    /** Observe successful callbacks that nevertheless closed or fenced this reader. */
    private void poll() throws java.io.IOException {
      if (failed) throw fail(failure);
      if (closed) throw new java.io.IOException("Compression input is no longer readable");
      check(options.checkCancelled);
      if (failed) throw fail(failure);
      if (closed) throw new java.io.IOException("Compression input is no longer readable");
    }
    /** Return one validated byte and preserve the original cancellation exception. */
    @Override public int read() throws java.io.IOException {
      begin();
      try {
        poll(); int value = position == bytes.length ? -1 : bytes[position] & 255; poll();
        if (value >= 0) ++position;
        return value;
      } catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error cause) { bytes = null; throw fail(cause); }
      finally { busy = false; }
    }
    /** Copy a bounded batch from staged validated bytes; zero-length reads retain Java semantics. */
    @Override public int read(byte[] destination, int offset, int length) throws java.io.IOException {
      begin();
      try {
        java.util.Objects.checkFromIndexSize(offset, length, destination.length); poll();
        if (length == 0) return 0;
        if (position == bytes.length) return -1;
        int count = java.lang.Math.min(java.lang.Math.min(length, BATCH_BYTES), bytes.length - position);
        java.lang.System.arraycopy(bytes, position, destination, offset, count); poll(); position += count; return count;
      } catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error cause) { bytes = null; throw fail(cause); }
      finally { busy = false; }
    }
    /** Require exact inner-message consumption; unread codec bytes fence subsequent reads. */
    public void finish() throws java.io.IOException {
      begin();
      try {
        poll();
        if (position != bytes.length) throw new Failure(Code.INVALID_DATA, "Unread decompressed input");
        poll();
      } catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error cause) { bytes = null; throw fail(cause); }
      finally { busy = false; }
    }
    /** Release staged bytes without closing or reading the borrowed source. */
    @Override public void close() { closed = true; bytes = null; }
  }
}
)java"
