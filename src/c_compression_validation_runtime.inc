// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
#if defined(SERIALIZER_COMPRESSION_ZLIB) && SERIALIZER_COMPRESSION_ZLIB
typedef struct @c@_bits {
  const uint8_t* data;
  size_t size, position, work;
  uint32_t pending;
  unsigned available;
  @c@_decode_options options;
  @c@_code status;
} @c@_bits;
typedef struct @c@_tree {
  unsigned counts[16], first[16], offsets[16], symbols[288], maximum;
} @c@_tree;
/* Read only required little-endian bits, retaining the exact consumed byte extent. */
static inline unsigned @c@_read_bits(@c@_bits* bits, unsigned count) {
  if (bits->status != @c@_ok) { return 0; }
  if ((++bits->work & 4095u) == 0) {
    bits->status = @c@_poll(bits->options.is_cancelled, bits->options.context);
    if (bits->status != @c@_ok) { return 0; }
  }
  while (bits->available < count) {
    if (bits->position == bits->size) { bits->status = @c@_invalid_data; return 0; }
    bits->pending |= (uint32_t)bits->data[bits->position++] << bits->available;
    bits->available += 8;
  }
  const unsigned result = bits->pending & (((uint32_t)1 << count)-1);
  bits->pending >>= count; bits->available -= count; return result;
}
/* Build small canonical tables without allocating payload or history storage. */
static inline bool @c@_build(@c@_tree* tree, const unsigned* lengths,
                              unsigned count, bool code_lengths, bool allow_empty) {
  memset(tree, 0, sizeof(*tree)); unsigned used = 0;
  for (unsigned i = 0; i < count; ++i) {
    if (lengths[i] > 15) { return false; }
    if (lengths[i]) {
      ++tree->counts[lengths[i]]; ++used;
      if (lengths[i] > tree->maximum) { tree->maximum = lengths[i]; }
    }
  }
  if (!used) { return allow_empty; }
  int left = 1;
  for (unsigned length = 1; length <= 15; ++length) {
    left = (left << 1)-(int)tree->counts[length]; if (left < 0) { return false; }
  }
  if (left && (code_lengths || tree->maximum != 1)) { return false; }
  unsigned code = 0, offset = 0, next[16] = {0};
  for (unsigned length = 1; length <= 15; ++length) {
    code = (code+tree->counts[length-1]) << 1;
    tree->first[length] = code; tree->offsets[length] = offset; next[length] = offset;
    offset += tree->counts[length];
  }
  for (unsigned symbol = 0; symbol < count; ++symbol) {
    if (lengths[symbol]) { tree->symbols[next[lengths[symbol]]++] = symbol; }
  }
  return true;
}
/* Decode one canonical code in transmission order without speculative byte reads. */
static inline unsigned @c@_symbol(@c@_bits* bits, const @c@_tree* tree) {
  unsigned code = 0;
  for (unsigned length = 1; length <= tree->maximum; ++length) {
    code = (code << 1) | @c@_read_bits(bits, 1);
    if (bits->status != @c@_ok) { return 0; }
    if (code >= tree->first[length] && code-tree->first[length] < tree->counts[length]) {
      return tree->symbols[tree->offsets[length]+code-tree->first[length]];
    }
  }
  bits->status = @c@_invalid_data; return 0;
}
/* Account only actual expanded bytes, refusing overflow before any output allocation. */
static inline bool @c@_add_count(@c@_bits* bits, size_t* output, size_t count) {
  if (count > bits->options.max_output_bytes-*output) { bits->status = @c@_resource_limit; return false; }
  *output += count; return true;
}
/* Establish exact DEFLATE extent and valid dynamic trees/history using fixed-size scratch. */
static inline @c@_code @c@_validate_frame(const uint8_t* data, size_t size, @c@_decode_options options) {
  static const unsigned length_base[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
  static const unsigned length_extra[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
  static const unsigned distance_base[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
  static const unsigned distance_extra[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
  static const unsigned order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
  size_t start = 0, end = size, output = 0;
  unsigned window = @c@_window_bytes;
  if (options.format == @c@_zlib) {
    if (size < 6 || (data[0] & 15) != 8 || data[0] >> 4 > 7 ||
        (((unsigned)data[0] << 8) | data[1]) % 31 || (data[1] & 32)) { return @c@_invalid_data; }
    window = 1u << ((data[0] >> 4)+8); start = 2; end -= 4;
  } else if (options.format == @c@_gzip) {
    if (size < 18 || data[0] != 31 || data[1] != 139 || data[2] != 8 || (data[3] & 224)) { return @c@_invalid_data; }
    start = 10; end -= 8;
    if (data[3] & 4) {
      if (end-start < 2) { return @c@_invalid_data; }
      const size_t extra = data[start] | ((size_t)data[start+1] << 8); start += 2;
      if (extra > end-start) { return @c@_invalid_data; } start += extra;
    }
    for (unsigned flag = 8; flag <= 16; flag *= 2) {
      if (!(data[3] & flag)) { continue; }
      for (;;) {
        if (start == end) { return @c@_invalid_data; }
        if ((start & (@c@_batch_bytes-1u)) == 0) {
          const @c@_code status = @c@_poll(options.is_cancelled, options.context);
          if (status != @c@_ok) { return status; }
        }
        if (!data[start++]) { break; }
      }
    }
    if (data[3] & 2) { if (end-start < 2) { return @c@_invalid_data; } start += 2; }
  }
  @c@_bits bits; memset(&bits, 0, sizeof(bits));
  bits.data = data; bits.size = end; bits.position = start; bits.options = options;
  @c@_tree literal, distance, codes, fixed_literal, fixed_distance;
  unsigned lengths[320], code_lengths[19];
  for (unsigned i = 0; i < 288; ++i) { lengths[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8; }
  (void)@c@_build(&fixed_literal, lengths, 288, false, false);
  for (unsigned i = 0; i < 32; ++i) { lengths[i] = 5; }
  (void)@c@_build(&fixed_distance, lengths, 32, false, false);
  bool final = false;
  do {
    bits.status = @c@_poll(options.is_cancelled, options.context);
    if (bits.status != @c@_ok) { return bits.status; }
    final = @c@_read_bits(&bits, 1) != 0;
    const unsigned kind = @c@_read_bits(&bits, 2);
    if (bits.status != @c@_ok) { return bits.status; }
    if (kind == 0) {
      bits.pending = 0; bits.available = 0;
      const unsigned count = @c@_read_bits(&bits, 16);
      const unsigned complement = @c@_read_bits(&bits, 16);
      if (bits.status != @c@_ok) { return bits.status; }
      if ((count ^ 65535u) != complement || count > end-bits.position) { return @c@_invalid_data; }
      bits.position += count;
      if (!@c@_add_count(&bits, &output, count)) { return bits.status; }
      continue;
    }
    if (kind == 3) { return @c@_invalid_data; }
    const @c@_tree* selected_literal = &fixed_literal;
    const @c@_tree* selected_distance = &fixed_distance;
    if (kind == 2) {
      const unsigned literal_count = @c@_read_bits(&bits, 5)+257;
      const unsigned distance_count = @c@_read_bits(&bits, 5)+1;
      const unsigned code_count = @c@_read_bits(&bits, 4)+4;
      if (bits.status != @c@_ok) { return bits.status; }
      if (literal_count > 286) { return @c@_invalid_data; }
      memset(code_lengths, 0, sizeof(code_lengths));
      for (unsigned i = 0; i < code_count; ++i) { code_lengths[order[i]] = @c@_read_bits(&bits, 3); }
      if (bits.status != @c@_ok) { return bits.status; }
      if (!@c@_build(&codes, code_lengths, 19, true, false)) { return @c@_invalid_data; }
      const unsigned total = literal_count+distance_count;
      unsigned position = 0;
      while (position < total) {
        const unsigned symbol = @c@_symbol(&bits, &codes);
        if (bits.status != @c@_ok) { return bits.status; }
        if (symbol < 16) { lengths[position++] = symbol; }
        else {
          const unsigned count = symbol == 16 ? @c@_read_bits(&bits, 2)+3 :
              symbol == 17 ? @c@_read_bits(&bits, 3)+3 : @c@_read_bits(&bits, 7)+11;
          if (bits.status != @c@_ok) { return bits.status; }
          if ((symbol == 16 && !position) || count > total-position) { return @c@_invalid_data; }
          const unsigned value = symbol == 16 ? lengths[position-1] : 0;
          for (unsigned i = 0; i < count; ++i) { lengths[position++] = value; }
        }
      }
      if (!lengths[256] || !@c@_build(&literal, lengths, literal_count, false, false) ||
          !@c@_build(&distance, lengths+literal_count, distance_count, false, true)) { return @c@_invalid_data; }
      selected_literal = &literal; selected_distance = &distance;
    }
    for (;;) {
      const unsigned symbol = @c@_symbol(&bits, selected_literal);
      if (bits.status != @c@_ok) { return bits.status; }
      if (symbol < 256) { if (!@c@_add_count(&bits, &output, 1)) { return bits.status; } continue; }
      if (symbol == 256) { break; }
      if (symbol-257 >= sizeof(length_base)/sizeof(*length_base)) { return @c@_invalid_data; }
      const unsigned count = length_base[symbol-257]+@c@_read_bits(&bits, length_extra[symbol-257]);
      const unsigned back_code = @c@_symbol(&bits, selected_distance);
      if (bits.status != @c@_ok) { return bits.status; }
      if (back_code >= sizeof(distance_base)/sizeof(*distance_base)) { return @c@_invalid_data; }
      const unsigned back = distance_base[back_code]+@c@_read_bits(&bits, distance_extra[back_code]);
      if (bits.status != @c@_ok) { return bits.status; }
      if (back > output || back > window) { return @c@_invalid_data; }
      if (!@c@_add_count(&bits, &output, count)) { return bits.status; }
    }
  } while (!final);
  if (bits.position != end) { return @c@_invalid_data; }
  return @c@_poll(options.is_cancelled, options.context);
}
#endif
)serializer"
