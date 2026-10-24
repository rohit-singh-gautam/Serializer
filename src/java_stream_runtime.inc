// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
R"java(
/** Count native output without allocating message storage; retain only fixed-size backpatch bytes. */
private static final class CountBuffer extends Writer.SizeBuffer {
  final java.util.TreeMap<Integer, Byte> patches = new java.util.TreeMap<>();
  /** Start a count-only traversal at the native message origin. */
  CountBuffer() { super(0); }
  /** Reject integer overflow before advancing the count. */
  private void advance(int length) {
    if (length < 0 || length > Integer.MAX_VALUE - count) {
      throw new IllegalArgumentException("Native output exceeds JVM byte limit");
    }
    count += length;
  }
  /** Count a scalar byte without retaining its contents. */
  @Override public synchronized void write(int value) { advance(1); }
  /** Count a complete source range after checking its bounds. */
  @Override public synchronized void write(byte[] value, int offset, int length) {
    java.util.Objects.checkFromIndexSize(offset, length, value.length);
    advance(length);
  }
  /** Retain only the final bytes required by generated size fields. */
  @Override void patchInteger(int position, long value, int width) {
    if (position < 0 || width < 0 || width > count - position) {
      throw new IllegalArgumentException("Invalid size patch");
    }
    for (int index = 0; index < width; ++index) {
      patches.put(position + index, (byte) (value >>> (index * Byte.SIZE)));
    }
  }
}

/** Send a second deterministic traversal through bounded staging, applying measured size fields. */
private static final class StreamBuffer extends Writer.SizeBuffer {
  private static final int CHUNK_SIZE_BYTES = 8192;
  private final java.io.OutputStream destination;
  private final CountBuffer measured;
  private final byte[] chunk = new byte[CHUNK_SIZE_BYTES];
  private int pending;
  /** Borrow a sink and the successful preflight; neither is closed or flushed. */
  StreamBuffer(java.io.OutputStream destination, CountBuffer measured) {
    super(0);
    this.destination = java.util.Objects.requireNonNull(destination);
    this.measured = measured;
  }
  /** Send only the populated private chunk and preserve the original I/O failure as its cause. */
  private void drain() {
    try { destination.write(chunk, 0, pending); pending = 0; }
    catch (java.io.IOException error) { throw new java.io.UncheckedIOException(error); }
  }
  /** Write a measured byte, replacing size placeholders before they reach the sink. */
  @Override public synchronized void write(int value) {
    if (count >= measured.size()) { throw new IllegalArgumentException("Model changed during stream encoding"); }
    Byte patch = measured.patches.get(count);
    chunk[pending++] = patch == null ? (byte) value : patch;
    ++count;
    if (pending == chunk.length) { drain(); }
  }
  /** Copy ranges in chunks; only measured size positions require byte-wise substitution. */
  @Override public synchronized void write(byte[] value, int offset, int length) {
    java.util.Objects.checkFromIndexSize(offset, length, value.length);
    if (length > measured.size() - count) { throw new IllegalArgumentException("Model changed during stream encoding"); }
    while (length > 0) {
      Integer next = measured.patches.ceilingKey(count);
      if (next != null && next == count) { write(value[offset++]); --length; continue; }
      int amount = Math.min(length, chunk.length - pending);
      if (next != null) { amount = Math.min(amount, next - count); }
      System.arraycopy(value, offset, chunk, pending, amount);
      pending += amount; count += amount; offset += amount; length -= amount;
      if (pending == chunk.length) { drain(); }
    }
  }
  /** Check that measured backpatches still describe the second traversal. */
  @Override void patchInteger(int position, long value, int width) {
    for (int index = 0; index < width; ++index) {
      Byte expected = measured.patches.get(position + index);
      if (expected == null || expected != (byte) (value >>> (index * Byte.SIZE))) {
        throw new IllegalArgumentException("Model changed during stream encoding");
      }
    }
  }
  /** Finish the traversal without invoking flush or close on the borrowed sink. */
  void finish() {
    if (count != measured.size()) { throw new IllegalArgumentException("Model changed during stream encoding"); }
    if (pending != 0) { drain(); }
  }
}
)java"
