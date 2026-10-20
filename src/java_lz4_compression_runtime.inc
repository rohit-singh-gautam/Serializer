// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"java(

  private static final int LZ4_HISTORY_BYTES = 64 * 1024;
  private static final int LZ4_BLOCK_BYTES = 64 * 1024;
  private static volatile Lz4Provider lz4State;
  /** Resolve immutable pure-Java capabilities without loading a native LZ4 library. */
  private static Lz4Provider lz4Provider() {
    Lz4Provider state = lz4State;
    if (state != null) return state;
    try { state = new Lz4Provider(); }
    catch (java.lang.ReflectiveOperationException | java.lang.NoClassDefFoundError missing) {
      throw new Failure(Code.UNAVAILABLE, "LZ4 Java provider is unavailable");
    } catch (java.io.IOException failure) { throw new java.io.UncheckedIOException(failure); }
    lz4State = state; return state;
  }

  /** Use public safe block/XXHash APIs; frame streams do not support linked blocks. */
  private static final class Lz4Provider {
    final Object factory, hashes, decompressor, hash;
    final java.lang.reflect.Method fast, high, compress, compressedBound, decompress, hash32;
    final java.lang.reflect.Method newHash, updateHash, hashValue;
    final java.lang.Class<?> malformed;
    /** Admit every required API before callbacks, model serialization or borrowed I/O. */
    Lz4Provider() throws java.lang.ReflectiveOperationException, java.io.IOException {
      java.lang.ClassLoader loader = @compression_name@.class.getClassLoader();
      java.lang.Class<?> factoryType = java.lang.Class.forName("net.jpountz.lz4.LZ4Factory", true, loader);
      java.lang.Class<?> hashFactory = java.lang.Class.forName("net.jpountz.xxhash.XXHashFactory", true, loader);
      java.lang.Class<?> compressor = java.lang.Class.forName("net.jpountz.lz4.LZ4Compressor", false, loader);
      java.lang.Class<?> safe = java.lang.Class.forName("net.jpountz.lz4.LZ4SafeDecompressor", false, loader);
      java.lang.Class<?> hashType = java.lang.Class.forName("net.jpountz.xxhash.XXHash32", false, loader);
      java.lang.Class<?> streamingHash = java.lang.Class.forName("net.jpountz.xxhash.StreamingXXHash32", false, loader);
      malformed = java.lang.Class.forName("net.jpountz.lz4.LZ4Exception", false, loader);
      factory = providerInvoke(factoryType.getMethod("safeInstance"), null);
      hashes = providerInvoke(hashFactory.getMethod("safeInstance"), null);
      decompressor = providerInvoke(factoryType.getMethod("safeDecompressor"), factory);
      hash = providerInvoke(hashFactory.getMethod("hash32"), hashes);
      fast = factoryType.getMethod("fastCompressor"); high = factoryType.getMethod("highCompressor", int.class);
      compressedBound = compressor.getMethod("maxCompressedLength", int.class);
      compress = compressor.getMethod("compress", byte[].class, int.class, int.class, byte[].class, int.class, int.class);
      decompress = safe.getMethod("decompress", byte[].class, int.class, int.class, byte[].class, int.class, int.class);
      hash32 = hashType.getMethod("hash", byte[].class, int.class, int.class, int.class);
      newHash = hashFactory.getMethod("newStreamingHash32", int.class);
      updateHash = streamingHash.getMethod("update", byte[].class, int.class, int.class);
      hashValue = streamingHash.getMethod("getValue");
      if (!java.io.Closeable.class.isAssignableFrom(streamingHash)) throw new java.lang.NoSuchMethodException("LZ4 checksum cleanup is unavailable");
    }
    /** Hash one admitted header/block with the standard frame seed. */
    int hash(byte[] input, int offset, int length) throws java.io.IOException {
      return (Integer) providerInvoke(hash32, hash, input, offset, length, 0);
    }
    /** Update complete-content checksums through fixed-size cooperative batches. */
    void update(Object context, byte[] input, int offset, int length, java.lang.Runnable callback) throws java.io.IOException {
      for (int position = 0; position < length;) {
        check(callback); int count = java.lang.Math.min(BATCH_BYTES, length-position);
        providerInvoke(updateHash, context, input, offset+position, count); position += count; check(callback);
      }
    }
  }

  /** Release only the owned checksum context and preserve a primary application failure. */
  private static void lz4Close(Object context, Throwable primary) {
    if (context == null) return;
    try { ((java.io.Closeable) context).close(); }
    catch (java.io.IOException | java.lang.RuntimeException | java.lang.Error closing) {
      if (primary != null) { if (closing != primary) primary.addSuppressed(closing); }
      else if (closing instanceof java.io.IOException value) throw new java.io.UncheckedIOException(value);
      else if (closing instanceof java.lang.RuntimeException value) throw value;
      else throw (java.lang.Error) closing;
    }
  }

  /** Emit one standard independent-block frame with explicit optional content checksum. */
  private static byte[] lz4Compress(byte[] input, EncodeOptions options) {
    Lz4Provider state = lz4Provider(); Buffer output = new Buffer(options.maxOutputBytes);
    Object content = null; Throwable primary = null;
    try {
      Object compressor = options.level == 0 ? providerInvoke(state.fast, state.factory) :
          providerInvoke(state.high, state.factory, options.level);
      int capacity = (Integer) providerInvoke(state.compressedBound, compressor, LZ4_BLOCK_BYTES);
      byte[] batch = new byte[capacity];
      byte[] header = new byte[] {4, 34, 77, 24, (byte)(options.checksum ? 100 : 96), 64, 0};
      header[6] = (byte)(state.hash(header, 4, 2) >>> 8); output.write(header, 0, header.length);
      if (options.checksum) content = providerInvoke(state.newHash, state.hashes, 0);
      for (int offset = 0; offset < input.length;) {
        check(options.checkCancelled); int count = java.lang.Math.min(LZ4_BLOCK_BYTES, input.length-offset);
        int produced = (Integer) providerInvoke(state.compress, compressor, input, offset, count, batch, 0, batch.length);
        check(options.checkCancelled);
        if (produced <= 0 || produced > batch.length) throw new Failure(Code.INVALID_DATA, "LZ4 provider made no progress");
        if (produced < count) { writeLittle(output, produced); output.write(batch, 0, produced); }
        else { writeLittle(output, count | Integer.MIN_VALUE); output.write(input, offset, count); }
        if (content != null) state.update(content, input, offset, count, options.checkCancelled);
        offset += count;
      }
      writeLittle(output, 0);
      if (content != null) writeLittle(output, (Integer) providerInvoke(state.hashValue, content));
      check(options.checkCancelled); return output.toByteArray();
    } catch (java.io.IOException failure) { primary = failure; throw new java.io.UncheckedIOException(failure); }
    catch (java.lang.RuntimeException | java.lang.Error failure) { primary = failure; throw failure; }
    finally { lz4Close(content, primary); }
  }

  /** Read one bounded literal/match length without allowing extension-byte integer overflow. */
  private static int lz4Length(byte[] input, int[] cursor, int initial, int end, int maximum, java.lang.Runnable callback) {
    int length = initial;
    if (initial == 15) {
      int extension;
      do {
        if (cursor[0] >= end) throw new Failure(Code.INVALID_DATA, "Truncated LZ4 length");
        if (cursor[0] % BATCH_BYTES == 0) check(callback);
        extension = input[cursor[0]++] & 255;
        if (extension > maximum-length) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 block length");
        length += extension;
      } while (extension == 255);
    }
    return length;
  }

  /** Count exact regenerated bytes and validate back references before allocating block output. */
  private static int lz4BlockSize(byte[] input, int offset, int length, int history, int maximum,
      java.lang.Runnable callback) {
    int[] cursor = {offset}; int end = offset+length, produced = 0, nextPoll = offset;
    while (cursor[0] < end) {
      if (cursor[0] >= nextPoll) { check(callback); nextPoll = cursor[0]+BATCH_BYTES; }
      int token = input[cursor[0]++] & 255;
      int literals = lz4Length(input, cursor, token >>> 4, end, maximum, callback);
      if (literals > end-cursor[0] || literals > maximum-produced) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 literals");
      cursor[0] += literals; produced += literals;
      if (cursor[0] == end) return produced;
      if (end-cursor[0] < 2) throw new Failure(Code.INVALID_DATA, "Truncated LZ4 match");
      int distance = (int) little(input, cursor[0], 2); cursor[0] += 2;
      if (distance == 0 || distance > produced+history) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 match distance");
      int match = lz4Length(input, cursor, token & 15, end, maximum, callback);
      if (maximum-produced < 4 || match > maximum-produced-4) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 match length");
      produced += match+4;
    }
    throw new Failure(Code.INVALID_DATA, "Missing LZ4 final literals");
  }

  /** Merge retained history into the first literal run so the safe decoder needs no dictionary API. */
  private static byte[] lz4Prefix(byte[] input, int offset, int length, byte[] history, java.lang.Runnable callback) {
    int token = input[offset] & 255; int[] cursor = {offset+1};
    int literal = lz4Length(input, cursor, token >>> 4, offset+length, 4*1024*1024, callback);
    int augmented = history.length+literal;
    int extensionBytes = augmented < 15 ? 0 : (augmented-15)/255+1;
    int remainder = length-(cursor[0]-offset);
    byte[] result = new byte[1+extensionBytes+history.length+remainder];
    int position = 0; result[position++] = (byte)((java.lang.Math.min(15, augmented) << 4) | (token & 15));
    if (augmented >= 15) {
      int extension = augmented-15;
      while (extension >= 255) { result[position++] = (byte)255; extension -= 255; }
      result[position++] = (byte)extension;
    }
    java.lang.System.arraycopy(history, 0, result, position, history.length); position += history.length;
    java.lang.System.arraycopy(input, cursor[0], result, position, remainder);
    return result;
  }

  /** Retain only the last dictionary window, independently of total message growth. */
  private static byte[] lz4History(byte[] history, byte[] block, int offset, int length) {
    int count = java.lang.Math.min(LZ4_HISTORY_BYTES, history.length+length);
    byte[] result = new byte[count]; int fromBlock = java.lang.Math.min(count, length);
    if (count > fromBlock) java.lang.System.arraycopy(history, history.length-(count-fromBlock), result, 0, count-fromBlock);
    java.lang.System.arraycopy(block, offset+length-fromBlock, result, count-fromBlock, fromBlock);
    return result;
  }
)java"
R"java(

  /** Validate descriptor/block/content checksums, linked history, exact size and one complete frame. */
  private static byte[] lz4Decompress(byte[] input, DecodeOptions options) {
    Lz4Provider state = lz4Provider(); Object content = null; Throwable primary = null;
    try {
      if (little(input, 0, 4) != 0x184d2204L) throw new Failure(Code.INVALID_DATA, "Expected one standard LZ4 frame");
      range(input, 4, 2); int flags = input[4] & 255, descriptor = input[5] & 255;
      if ((flags >>> 6) != 1 || (flags & 2) != 0 || (descriptor & 143) != 0) {
        throw new Failure(Code.INVALID_DATA, "Invalid LZ4 frame descriptor");
      }
      if ((flags & 1) != 0) throw new Failure(Code.INVALID_DATA, "LZ4 dictionaries are unsupported");
      int blockId = (descriptor >>> 4) & 7;
      if (blockId < 4 || blockId > 7) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 block maximum");
      int maximum = 1 << (8+2*blockId), offset = 6; long expected = -1;
      if ((flags & 8) != 0) {
        expected = little(input, offset, 8); offset += 8;
        if (expected < 0 || expected > options.maxOutputBytes) throw new Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded");
      }
      range(input, offset, 1);
      if ((input[offset] & 255) != ((state.hash(input, 4, offset-4) >>> 8) & 255)) {
        throw new Failure(Code.INVALID_DATA, "Invalid LZ4 descriptor checksum");
      }
      ++offset;
      if (maximum > options.maxWindowBytes) throw new Failure(Code.RESOURCE_LIMIT, "Compression history limit exceeded");
      if ((flags & 4) != 0) content = providerInvoke(state.newHash, state.hashes, 0);
      Buffer output = new Buffer(options.maxOutputBytes); byte[] history = new byte[0];
      while (true) {
        check(options.checkCancelled); long block = little(input, offset, 4); offset += 4;
        if (block == 0) break;
        boolean raw = (block & 0x80000000L) != 0; int count = (int)(block & 0x7fffffffL);
        if (count > maximum) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 block size");
        range(input, offset, count);
        if ((flags & 16) != 0) {
          long checksum = little(input, offset+count, 4);
          if (checksum != (state.hash(input, offset, count) & 0xffffffffL)) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 block checksum");
        }
        byte[] plain = input; int plainOffset = offset, plainCount = count;
        if (!raw) {
          plainCount = lz4BlockSize(input, offset, count, history.length, maximum, options.checkCancelled);
          if (plainCount > options.maxOutputBytes-output.size()) throw new Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded");
          byte[] packed = input; int packedOffset = offset, packedCount = count;
          if (history.length != 0) { packed = lz4Prefix(input, offset, count, history, options.checkCancelled); packedOffset = 0; packedCount = packed.length; }
          plain = new byte[history.length+plainCount]; plainOffset = history.length;
          final int decoded;
          try { decoded = (Integer) providerInvoke(state.decompress, state.decompressor, packed, packedOffset, packedCount, plain, 0, plain.length); }
          catch (java.lang.RuntimeException failure) {
            if (state.malformed.isInstance(failure)) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 block");
            throw failure;
          }
          if (decoded != plain.length) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 regenerated block size");
        }
        check(options.checkCancelled); output.write(plain, plainOffset, plainCount);
        if (content != null) state.update(content, plain, plainOffset, plainCount, options.checkCancelled);
        if ((flags & 32) == 0) history = lz4History(history, plain, plainOffset, plainCount);
        offset += count + ((flags & 16) != 0 ? 4 : 0);
      }
      if (content != null) {
        long checksum = little(input, offset, 4); offset += 4;
        if (checksum != (((Integer) providerInvoke(state.hashValue, content)) & 0xffffffffL)) {
          throw new Failure(Code.INVALID_DATA, "Invalid LZ4 content checksum");
        }
      }
      if (offset != input.length) throw new Failure(Code.INVALID_DATA, "Trailing LZ4 input");
      if (expected != -1 && expected != output.size()) throw new Failure(Code.INVALID_DATA, "Invalid LZ4 content size");
      check(options.checkCancelled); return output.toByteArray();
    } catch (java.io.IOException failure) { primary = failure; throw new java.io.UncheckedIOException(failure); }
    catch (java.lang.RuntimeException | java.lang.Error failure) { primary = failure; throw failure; }
    finally { lz4Close(content, primary); }
  }
)java"
