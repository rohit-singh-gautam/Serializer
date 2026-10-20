// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"javadeflate(
  private static final int DEFLATE_CODE_BITS = 15;
  private static final int DEFLATE_CODE_WIDTH = DEFLATE_CODE_BITS + 1;
  private static final int DEFLATE_FIRST = DEFLATE_CODE_WIDTH;
  private static final int DEFLATE_OFFSETS = DEFLATE_FIRST + DEFLATE_CODE_WIDTH;
  private static final int DEFLATE_SYMBOLS = DEFLATE_OFFSETS + DEFLATE_CODE_WIDTH;
  private static final int DEFLATE_MAXIMUM = DEFLATE_SYMBOLS + 288;
  private static final int DEFLATE_TREE_SIZE = DEFLATE_MAXIMUM + 1;
  private static final int DEFLATE_POSITION = 0, DEFLATE_VALUE = 1, DEFLATE_AVAILABLE = 2, DEFLATE_WORK = 3;

  /** Read only required little-endian fields; four fixed state integers retain exact input extent. */
  private static int deflateBits(byte[] input, int end, int[] state, int count, java.lang.Runnable callback) {
    if ((++state[DEFLATE_WORK] & 4095) == 0) check(callback);
    while (state[DEFLATE_AVAILABLE] < count) {
      if (state[DEFLATE_POSITION] == end) throw new Failure(Code.INVALID_DATA, "Truncated DEFLATE data");
      state[DEFLATE_VALUE] |= (input[state[DEFLATE_POSITION]++] & 255) << state[DEFLATE_AVAILABLE];
      state[DEFLATE_AVAILABLE] += 8;
    }
    int result = state[DEFLATE_VALUE] & ((1 << count) - 1);
    state[DEFLATE_VALUE] >>>= count; state[DEFLATE_AVAILABLE] -= count; return result;
  }

  /** Build bounded canonical tables without allocating decoded bytes or a history dictionary. */
  private static void deflateTree(int[] tree, int[] lengths, int start, int count, boolean codeLengths, boolean allowEmpty, int[] next) {
    java.util.Arrays.fill(tree, 0); int maximum = 0, used = 0;
    for (int index = 0; index < count; ++index) {
      int length = lengths[start + index];
      if (length < 0 || length > DEFLATE_CODE_BITS) throw new Failure(Code.INVALID_DATA, "Invalid DEFLATE code length");
      if (length != 0) { ++tree[length]; ++used; maximum = java.lang.Math.max(maximum, length); }
    }
    if (used == 0) {
      if (allowEmpty) return;
      throw new Failure(Code.INVALID_DATA, "Empty DEFLATE Huffman tree");
    }
    int left = 1;
    for (int length = 1; length <= DEFLATE_CODE_BITS; ++length) {
      left = (left << 1) - tree[length];
      if (left < 0) throw new Failure(Code.INVALID_DATA, "Oversubscribed DEFLATE Huffman tree");
    }
    if (left != 0 && (codeLengths || used != 1 || maximum != 1)) throw new Failure(Code.INVALID_DATA, "Incomplete DEFLATE Huffman tree");
    int code = 0, offset = 0;
    for (int length = 1; length <= DEFLATE_CODE_BITS; ++length) {
      code = (code + tree[length - 1]) << 1;
      tree[DEFLATE_FIRST + length] = code; tree[DEFLATE_OFFSETS + length] = offset;
      next[length] = offset; offset += tree[length];
    }
    for (int symbol = 0; symbol < count; ++symbol) {
      int length = lengths[start + symbol];
      if (length != 0) tree[DEFLATE_SYMBOLS + next[length]++] = symbol;
    }
    tree[DEFLATE_MAXIMUM] = maximum;
  }

  /** Decode one canonical symbol without speculative byte reads or a payload-sized lookup table. */
  private static int deflateSymbol(byte[] input, int end, int[] state, int[] tree, java.lang.Runnable callback) {
    int code = 0;
    for (int length = 1; length <= tree[DEFLATE_MAXIMUM]; ++length) {
      code = (code << 1) | deflateBits(input, end, state, 1, callback);
      int index = code - tree[DEFLATE_FIRST + length];
      if (index >= 0 && index < tree[length]) return tree[DEFLATE_SYMBOLS + tree[DEFLATE_OFFSETS + length] + index];
    }
    throw new Failure(Code.INVALID_DATA, "Invalid DEFLATE Huffman code");
  }

  /** Refuse output-count overflow and actual expansion beyond the caller's logical bound. */
  private static int deflateAdd(int expanded, int count, int maximum) {
    if (count > maximum - expanded) throw new Failure(Code.RESOURCE_LIMIT, "Expanded output limit exceeded");
    return expanded + count;
  }

  /** Enforce a smaller RFC1950 history window that native Inflater can otherwise ignore. */
  private static void validateZlibWindow(byte[] input, DecodeOptions options) {
    range(input, 0, 6);
    int cmf = input[0] & 255, flags = input[1] & 255;
    if ((cmf & 15) != 8 || (cmf >>> 4) > 7 || ((cmf << 8) | flags) % 31 != 0 || (flags & 32) != 0) {
      throw new Failure(Code.INVALID_DATA, "Invalid or dictionary-dependent zlib header");
    }
    // Ordinary gzip, raw DEFLATE and 32 KiB zlib keep their existing native hot paths.
    if ((cmf >>> 4) == 7) return;
    int window = 1 << ((cmf >>> 4) + 8), end = input.length - 4;
    int[] lengthBase = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    int[] lengthExtra = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    int[] distanceBase = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    int[] distanceExtra = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    int[] order = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    int[] lengths = new int[320], codeLengths = new int[19], next = new int[DEFLATE_CODE_WIDTH];
    int[] fixedLiteral = new int[DEFLATE_TREE_SIZE], fixedDistance = new int[DEFLATE_TREE_SIZE];
    int[] literalTree = new int[DEFLATE_TREE_SIZE], distanceTree = new int[DEFLATE_TREE_SIZE], codes = new int[DEFLATE_TREE_SIZE];
    for (int index = 0; index < 288; ++index) lengths[index] = index < 144 ? 8 : index < 256 ? 9 : index < 280 ? 7 : 8;
    deflateTree(fixedLiteral, lengths, 0, 288, false, false, next);
    java.util.Arrays.fill(lengths, 0, 32, 5);
    deflateTree(fixedDistance, lengths, 0, 32, false, false, next);
    int[] state = {2, 0, 0, 0}; int expanded = 0; boolean last;
    do {
      check(options.checkCancelled); last = deflateBits(input, end, state, 1, options.checkCancelled) != 0;
      int kind = deflateBits(input, end, state, 2, options.checkCancelled);
      if (kind == 0) {
        state[DEFLATE_VALUE] = 0; state[DEFLATE_AVAILABLE] = 0;
        int count = deflateBits(input, end, state, 16, options.checkCancelled);
        int complement = deflateBits(input, end, state, 16, options.checkCancelled);
        if ((count ^ 65535) != complement || count > end - state[DEFLATE_POSITION]) throw new Failure(Code.INVALID_DATA, "Invalid DEFLATE stored extent");
        state[DEFLATE_POSITION] += count; expanded = deflateAdd(expanded, count, options.maxOutputBytes); continue;
      }
      if (kind == 3) throw new Failure(Code.INVALID_DATA, "Reserved DEFLATE block type");
      int[] literal = fixedLiteral, distance = fixedDistance;
      if (kind == 2) {
        int literalCount = deflateBits(input, end, state, 5, options.checkCancelled) + 257;
        int distanceCount = deflateBits(input, end, state, 5, options.checkCancelled) + 1;
        int codeCount = deflateBits(input, end, state, 4, options.checkCancelled) + 4;
        if (literalCount > 286) throw new Failure(Code.INVALID_DATA, "Invalid DEFLATE literal alphabet");
        java.util.Arrays.fill(codeLengths, 0);
        for (int index = 0; index < codeCount; ++index) codeLengths[order[index]] = deflateBits(input, end, state, 3, options.checkCancelled);
        deflateTree(codes, codeLengths, 0, codeLengths.length, true, false, next);
        int total = literalCount + distanceCount, position = 0;
        while (position < total) {
          int symbol = deflateSymbol(input, end, state, codes, options.checkCancelled);
          if (symbol < 16) lengths[position++] = symbol;
          else {
            int count = symbol == 16 ? deflateBits(input, end, state, 2, options.checkCancelled) + 3 :
                symbol == 17 ? deflateBits(input, end, state, 3, options.checkCancelled) + 3 : deflateBits(input, end, state, 7, options.checkCancelled) + 11;
            if ((symbol == 16 && position == 0) || count > total - position) throw new Failure(Code.INVALID_DATA, "Invalid DEFLATE code-length repeat");
            int value = symbol == 16 ? lengths[position - 1] : 0;
            java.util.Arrays.fill(lengths, position, position + count, value); position += count;
          }
        }
        if (lengths[256] == 0) throw new Failure(Code.INVALID_DATA, "Missing DEFLATE end-of-block code");
        deflateTree(literalTree, lengths, 0, literalCount, false, false, next);
        deflateTree(distanceTree, lengths, literalCount, distanceCount, false, true, next);
        literal = literalTree; distance = distanceTree;
      }
      for (;;) {
        int symbol = deflateSymbol(input, end, state, literal, options.checkCancelled);
        if (symbol < 256) { expanded = deflateAdd(expanded, 1, options.maxOutputBytes); continue; }
        if (symbol == 256) break;
        int lengthCode = symbol - 257;
        if (lengthCode >= lengthBase.length) throw new Failure(Code.INVALID_DATA, "Reserved DEFLATE length code");
        int count = lengthBase[lengthCode] + deflateBits(input, end, state, lengthExtra[lengthCode], options.checkCancelled);
        int backCode = deflateSymbol(input, end, state, distance, options.checkCancelled);
        if (backCode >= distanceBase.length) throw new Failure(Code.INVALID_DATA, "Reserved DEFLATE distance code");
        int back = distanceBase[backCode] + deflateBits(input, end, state, distanceExtra[backCode], options.checkCancelled);
        if (back > expanded || back > window) throw new Failure(Code.INVALID_DATA, "DEFLATE distance exceeds available history");
        expanded = deflateAdd(expanded, count, options.maxOutputBytes);
      }
    } while (!last);
    if (state[DEFLATE_POSITION] != end) throw new Failure(Code.INVALID_DATA, "Trailing DEFLATE data");
    check(options.checkCancelled);
  }
)javadeflate"
