// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"java(

  private static volatile ZstdProvider zstdState;
  /** Cache immutable reflective capabilities, never a shared mutable native context. */
  private static ZstdProvider zstdProvider() {
    ZstdProvider state = zstdState;
    if (state != null) return state;
    try { state = new ZstdProvider(); }
    catch (java.lang.ReflectiveOperationException |
           java.lang.UnsatisfiedLinkError | java.lang.NoClassDefFoundError missing) {
      throw new Failure(Code.UNAVAILABLE, "Zstandard Java provider is unavailable");
    } catch (java.io.IOException failure) { throw new java.io.UncheckedIOException(failure); }
    zstdState = state;
    return state;
  }

  /** Resolve only public zstd-jni APIs and preserve unrelated provider/application failures. */
  private static final class ZstdProvider {
    final java.lang.reflect.Constructor<?> encoder, decoder;
    final java.lang.reflect.Method checksum, level, pledge, stream, encoderClose, window, errorCode;
    final Object continueDirective, endDirective;
    final java.lang.Class<?> dataFailure;
    final int minimumLevel, maximumLevel;
    final long windowFailure;
    final java.util.Set<Long> malformed;
    /** Verify native deployment and all required methods before application work begins. */
    ZstdProvider() throws java.lang.ReflectiveOperationException, java.io.IOException {
      java.lang.ClassLoader loader = @compression_name@.class.getClassLoader();
      java.lang.Class<?> api = java.lang.Class.forName("com.github.luben.zstd.Zstd", true, loader);
      java.lang.Class<?> output = java.lang.Class.forName("com.github.luben.zstd.ZstdCompressCtx", true, loader);
      java.lang.Class<?> input = java.lang.Class.forName("com.github.luben.zstd.ZstdInputStreamNoFinalizer", true, loader);
      java.lang.Class<?> directive = java.lang.Class.forName("com.github.luben.zstd.EndDirective", true, loader);
      dataFailure = java.lang.Class.forName("com.github.luben.zstd.ZstdIOException", false, loader);
      if (!java.io.InputStream.class.isAssignableFrom(input)) {
        throw new java.lang.NoSuchMethodException("Incompatible Zstandard stream API");
      }
      encoder = output.getConstructor();
      decoder = input.getConstructor(java.io.InputStream.class);
      checksum = output.getMethod("setChecksum", boolean.class);
      level = output.getMethod("setLevel", int.class);
      pledge = output.getMethod("setPledgedSrcSize", long.class);
      stream = output.getMethod("compressByteBufferStream", java.nio.ByteBuffer.class, java.nio.ByteBuffer.class, directive);
      encoderClose = output.getMethod("close");
      continueDirective = directive.getField("CONTINUE").get(null);
      endDirective = directive.getField("END").get(null);
      window = input.getMethod("setLongMax", int.class);
      errorCode = dataFailure.getMethod("getErrorCode");
      minimumLevel = (Integer) providerInvoke(api.getMethod("minCompressionLevel"), null);
      maximumLevel = (Integer) providerInvoke(api.getMethod("maxCompressionLevel"), null);
      windowFailure = (Long) providerInvoke(api.getMethod("errFrameParameterWindowTooLarge"), null);
      malformed = new java.util.HashSet<>();
      for (String name : new String[] {"errPrefixUnknown", "errVersionUnsupported", "errFrameParameterUnsupported",
          "errCorruptionDetected", "errChecksumWrong", "errDictionaryCorrupted", "errDictionaryWrong",
          "errSrcSizeWrong", "errTableLogTooLarge", "errMaxSymbolValueTooLarge", "errMaxSymbolValueTooSmall"}) {
        malformed.add((Long) providerInvoke(api.getMethod(name), null));
      }
    }
    /** Admit the provider's complete advertised level range, including fast negative levels. */
    void validateLevel(int level) {
      if (level < minimumLevel || level > maximumLevel) {
        throw new Failure(Code.INVALID_OPTIONS, "Invalid Zstandard compression level");
      }
    }
    /** Categorize only known malformed-frame/window errors; retain other provider I/O causes. */
    java.lang.RuntimeException failure(java.io.IOException failure) {
      if (dataFailure.isInstance(failure)) {
        final long code;
        try { code = (Long) providerInvoke(errorCode, failure); }
        catch (java.io.IOException unexpected) { return new java.io.UncheckedIOException(unexpected); }
        if (code == windowFailure) return new Failure(Code.RESOURCE_LIMIT, "Compression history limit exceeded");
        if (malformed.contains(code)) return new Failure(Code.INVALID_DATA, "Invalid Zstandard frame");
      }
      return new java.io.UncheckedIOException(failure);
    }
  }

  /** Preserve invocation causes instead of exposing reflection wrappers as public failures. */
  private static Object providerInvoke(java.lang.reflect.Method method, Object target, Object... arguments) throws java.io.IOException {
    try { return method.invoke(target, arguments); }
    catch (java.lang.reflect.InvocationTargetException failure) { throw providerException(failure.getCause()); }
    catch (java.lang.IllegalAccessException failure) { throw new java.lang.IllegalStateException("Inaccessible compression provider API", failure); }
  }
  /** Construct one owned provider without concealing its original initialization failure. */
  private static Object providerConstruct(java.lang.reflect.Constructor<?> constructor, Object... arguments) throws java.io.IOException {
    try { return constructor.newInstance(arguments); }
    catch (java.lang.reflect.InvocationTargetException failure) { throw providerException(failure.getCause()); }
    catch (java.lang.ReflectiveOperationException failure) { throw new java.lang.IllegalStateException("Inaccessible compression provider constructor", failure); }
  }
  /** Keep callback/runtime/allocation exception identity and checked I/O as a distinct cause. */
  private static java.lang.RuntimeException providerException(Throwable failure) throws java.io.IOException {
    if (failure instanceof java.io.IOException value) throw value;
    if (failure instanceof java.lang.RuntimeException value) return value;
    if (failure instanceof java.lang.Error value) throw value;
    return new java.lang.IllegalStateException("Unexpected compression provider failure", failure);
  }

  /** Bound every provider source refill and poll cancellation without borrowing application I/O. */
  private static final class ZstdSource extends java.io.ByteArrayInputStream {
    final java.lang.Runnable callback;
    /** Retain the already admitted complete input for the owned provider lifetime. */
    ZstdSource(byte[] bytes, java.lang.Runnable callback) { super(bytes); this.callback = callback; }
    /** Return at most one fixed input batch, preserving the native stream's EOF behavior. */
    @Override public synchronized int read(byte[] bytes, int offset, int length) {
      check(callback); int count = super.read(bytes, offset, java.lang.Math.min(length, BATCH_BYTES));
      check(callback); return count;
    }
    /** Poll even if a provider uses a single-byte header read. */
    @Override public synchronized int read() { check(callback); int result = super.read(); check(callback); return result; }
  }

)java"
R"java(

  /** Prove one exact standard frame and its history/output admission before native expansion. */
  private static void zstdFrame(byte[] bytes, DecodeOptions options) {
    if (little(bytes, 0, 4) != 0xfd2fb528L) throw new Failure(Code.INVALID_DATA, "Expected one standard Zstandard frame");
    range(bytes, 4, 1); int descriptor = bytes[4] & 255;
    if ((descriptor & 8) != 0) throw new Failure(Code.INVALID_DATA, "Reserved Zstandard frame descriptor");
    boolean single = (descriptor & 32) != 0;
    int offset = 5; long history = 0;
    if (!single) {
      range(bytes, offset, 1); int window = bytes[offset++] & 255;
      long base = 1L << (10 + (window >>> 3)); history = base + (base >>> 3) * (window & 7);
    }
    int dictionaryBytes = (descriptor & 3) == 3 ? 4 : descriptor & 3;
    if (little(bytes, offset, dictionaryBytes) != 0) throw new Failure(Code.INVALID_DATA, "Zstandard dictionaries are unsupported");
    offset += dictionaryBytes;
    int sizeFlag = descriptor >>> 6;
    int sizeBytes = sizeFlag == 0 ? (single ? 1 : 0) : 1 << sizeFlag;
    long contentSize = sizeBytes == 0 ? -1 : little(bytes, offset, sizeBytes);
    if (sizeBytes == 2) contentSize += 256;
    if (sizeBytes != 0 && (contentSize < 0 || contentSize > options.maxOutputBytes)) {
      throw new Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded");
    }
    offset += sizeBytes;
    if (single) history = contentSize;
    long effectiveWindow = Integer.highestOneBit(options.maxWindowBytes);
    if (history > effectiveWindow) throw new Failure(Code.RESOURCE_LIMIT, "Compression history limit exceeded");
    final long maximumBlock = java.lang.Math.min(128L * 1024, history);
    boolean last;
    do {
      check(options.checkCancelled); int block = (int) little(bytes, offset, 3); offset += 3;
      last = (block & 1) != 0; int type = (block >>> 1) & 3; int count = block >>> 3;
      if (type == 3 || count > maximumBlock) throw new Failure(Code.INVALID_DATA, "Invalid Zstandard block");
      int stored = type == 1 ? 1 : count;
      range(bytes, offset, stored); offset += stored;
    } while (!last);
    if ((descriptor & 4) != 0) { range(bytes, offset, 4); offset += 4; }
    if (offset != bytes.length) throw new Failure(Code.INVALID_DATA, "Trailing Zstandard input");
    check(options.checkCancelled);
  }

  /** Release owned providers after success/failure without replacing a primary cancellation exception. */
  private static void zstdClose(java.io.Closeable provider, Throwable primary, ZstdProvider state) {
    if (provider == null) return;
    try { provider.close(); }
    catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error closing) {
      if (primary != null) { if (closing != primary) primary.addSuppressed(closing); }
      else if (closing instanceof java.io.IOException value) throw state.failure(value);
      else if (closing instanceof java.lang.RuntimeException value) throw value;
      else throw (java.lang.Error) closing;
    }
  }

  /** Encode one checksum-selected standard frame and close its native context before publication. */
  private static byte[] zstdCompress(byte[] input, EncodeOptions options) {
    ZstdProvider state = zstdProvider(); Buffer output = new Buffer(options.maxOutputBytes);
    Object provider = null; Throwable primary = null;
    try {
      provider = providerConstruct(state.encoder);
      providerInvoke(state.level, provider, options.level);
      providerInvoke(state.checksum, provider, options.checksum);
      providerInvoke(state.pledge, provider, (long) input.length);
      byte[] batch = new byte[BATCH_BYTES];
      for (int offset = 0; offset < input.length;) {
        int count = java.lang.Math.min(BATCH_BYTES, input.length - offset);
        java.nio.ByteBuffer source = java.nio.ByteBuffer.wrap(input, offset, count);
        while (source.hasRemaining()) {
          java.nio.ByteBuffer destination = java.nio.ByteBuffer.wrap(batch, 0, outputCapacity(output.size(), options.maxOutputBytes));
          int before = source.position(); check(options.checkCancelled);
          providerInvoke(state.stream, provider, destination, source, state.continueDirective);
          check(options.checkCancelled); output.write(batch, 0, destination.position());
          if (source.position() == before && destination.position() == 0) {
            throw new Failure(Code.INVALID_DATA, "Compression provider made no progress");
          }
        }
        offset += count;
      }
      boolean finished;
      do {
        java.nio.ByteBuffer destination = java.nio.ByteBuffer.wrap(batch, 0, outputCapacity(output.size(), options.maxOutputBytes));
        check(options.checkCancelled);
        finished = (Boolean) providerInvoke(state.stream, provider, destination, java.nio.ByteBuffer.allocate(0), state.endDirective);
        check(options.checkCancelled); output.write(batch, 0, destination.position());
        if (!finished && destination.position() == 0) throw new Failure(Code.INVALID_DATA, "Compression provider made no progress");
      } while (!finished);
    } catch (java.io.IOException failure) { primary = failure; throw state.failure(failure); }
    catch (java.lang.RuntimeException | java.lang.Error failure) { primary = failure; throw failure; }
    finally {
      if (provider != null) {
        try { providerInvoke(state.encoderClose, provider); }
        catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error closing) {
          if (primary != null) { if (closing != primary) primary.addSuppressed(closing); }
          else if (closing instanceof java.io.IOException value) throw state.failure(value);
          else if (closing instanceof java.lang.RuntimeException value) throw value;
          else throw (java.lang.Error) closing;
        }
      }
    }
    check(options.checkCancelled); return output.toByteArray();
  }

  /** Apply both prevalidated frame history and the native power-of-two history ceiling. */
  private static byte[] zstdDecompress(byte[] input, DecodeOptions options) {
    zstdFrame(input, options);
    ZstdProvider state = zstdProvider(); Buffer output = new Buffer(options.maxOutputBytes);
    java.io.InputStream provider = null; Throwable primary = null;
    try {
      provider = (java.io.InputStream) providerConstruct(state.decoder, new ZstdSource(input, options.checkCancelled));
      int windowLog = Integer.SIZE - 1 - Integer.numberOfLeadingZeros(options.maxWindowBytes);
      providerInvoke(state.window, provider, windowLog);
      byte[] batch = new byte[BATCH_BYTES];
      while (true) {
        check(options.checkCancelled); int count = provider.read(batch, 0, outputCapacity(output.size(), options.maxOutputBytes));
        check(options.checkCancelled);
        if (count < 0) break;
        if (count == 0) throw new Failure(Code.INVALID_DATA, "Compression provider made no progress");
        output.write(batch, 0, count);
      }
    } catch (java.io.IOException failure) { primary = failure; throw state.failure(failure); }
    catch (java.lang.RuntimeException | java.lang.Error failure) { primary = failure; throw failure; }
    finally { zstdClose(provider, primary, state); }
    check(options.checkCancelled); return output.toByteArray();
  }
)java"
