// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <float.h>
#include <errno.h>
#include <math.h>
#include <locale.h>

/* Native C11 owning codecs. All lengths are byte counts unless documented otherwise. */
typedef enum srl_protocol { srl_json, srl_binary_none, srl_binary_integer, srl_binary_string } srl_protocol;
typedef enum srl_status { srl_ok, srl_invalid, srl_truncated, srl_range, srl_limit, srl_allocation } srl_status;
typedef struct srl_buffer { uint8_t* data; size_t size; size_t capacity; } srl_buffer;
typedef struct srl_string { char* data; size_t size; } srl_string;
typedef enum srl_read_policy { srl_read_strict, srl_read_compatible, srl_read_flexible } srl_read_policy;
typedef struct srl_limits { size_t max_bytes, max_string_bytes, max_elements, max_depth; srl_read_policy read_policy; } srl_limits;
typedef struct srl_field { const uint8_t* json; size_t json_size; const uint8_t* binary; size_t binary_size; } srl_field;
typedef struct srl_writer { srl_buffer buffer; srl_protocol protocol; srl_status status; size_t depth; } srl_writer;
enum { srl_number_scratch_bytes = 128 };
typedef struct srl_reader { const uint8_t* data; size_t size, offset, depth, elements; srl_protocol protocol; srl_status status; srl_limits limits; char token[srl_number_scratch_bytes]; } srl_reader;
typedef struct srl_key { uint32_t id; srl_string name; bool named; } srl_key;
_Static_assert(CHAR_BIT == 8 && sizeof(float) == 4 && sizeof(double) == 8 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53, "Serializer requires IEEE binary32/binary64 and 8-bit bytes");

/* Construct the default per-message policy. */
static inline srl_limits srl_default_limits(void) { const srl_limits value = {64u*1024u*1024u, 16u*1024u*1024u, 1000000u, 64u, srl_read_strict}; return value; }
/* Release owned encoded storage; zero initialization is valid. */
static inline void srl_buffer_free(srl_buffer* value) { free(value->data); memset(value, 0, sizeof(*value)); }
/* Release owned string storage, including empty strings. */
static inline void srl_string_free(srl_string* value) { free(value->data); memset(value, 0, sizeof(*value)); }
/* Validate strict UTF-8 without allocating or accepting surrogate code points. */
static inline bool srl_utf8(const uint8_t* data, size_t size) {
  if (size && !data) { return false; }
  for (size_t i = 0; i < size;) {
    const uint8_t first = data[i++]; if (first < 128) { continue; }
    size_t count; uint32_t code, minimum;
    if (first >= 0xc2 && first <= 0xdf) { count = 1; code = first & 31u; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { count = 2; code = first & 15u; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { count = 3; code = first & 7u; minimum = 0x10000; }
    else { return false; }
    if (count > size-i) { return false; }
    while (count--) { const uint8_t next = data[i++]; if ((next & 0xc0u) != 0x80u) { return false; } code = (code << 6) | (next & 63u); }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) { return false; }
  }
  return true;
}
/* Replace an initialized string transactionally; retain an owned NUL sentinel for C callers. */
static inline srl_status srl_string_set(srl_string* value, const char* data, size_t size) {
  if (size == SIZE_MAX || !srl_utf8((const uint8_t*)data, size)) { return srl_invalid; }
  char* copy = (char*)malloc(size+1); if (!copy) { return srl_allocation; }
  if (size) { memcpy(copy, data, size); } copy[size] = '\0'; free(value->data); value->data = copy; value->size = size; return srl_ok;
}
/* Copy a NUL-terminated string into initialized owned storage. */
static inline srl_status srl_string_set_cstr(srl_string* value, const char* text) { return text ? srl_string_set(value, text, strlen(text)) : srl_invalid; }
/* Compare exact unsigned UTF-8 bytes for deterministic map ordering. */
static inline int srl_string_compare(const srl_string* a, const srl_string* b) {
  const size_t count = a->size < b->size ? a->size : b->size;
  const int order = count ? memcmp(a->data, b->data, count) : 0;
  return order ? order : (a->size > b->size) - (a->size < b->size);
}
/* Test a decoded key without reading beyond its byte span. */
static inline bool srl_string_equal(const srl_string* value, const char* text) { const size_t size = strlen(text); return value->size == size && (!size || memcmp(value->data, text, size) == 0); }
/* Grow a byte buffer geometrically with overflow and allocation checks. */
static inline bool srl_reserve(srl_buffer* buffer, size_t extra) {
  if (extra > SIZE_MAX-buffer->size) { return false; }
  const size_t required = buffer->size+extra; if (required <= buffer->capacity) { return true; }
  size_t capacity = buffer->capacity ? buffer->capacity : 256;
  while (capacity < required) { if (capacity > SIZE_MAX/2) { capacity = required; break; } capacity *= 2; }
  void* next = realloc(buffer->data, capacity); if (!next) { return false; }
  buffer->data = (uint8_t*)next; buffer->capacity = capacity; return true;
}
/* Append bytes while retaining the first failure. */
static inline void srl_put(srl_writer* out, const void* bytes, size_t size) {
  if (out->status != srl_ok) { return; }
  if ((size && !bytes) || !srl_reserve(&out->buffer, size)) { out->status = size && !bytes ? srl_invalid : srl_allocation; return; }
  if (size) { memcpy(out->buffer.data+out->buffer.size, bytes, size); } out->buffer.size += size;
}
/* Append one framing byte. */
static inline void srl_put_byte(srl_writer* out, uint8_t byte) { srl_put(out, &byte, 1); }
/* Append a constant ASCII token. */
static inline void srl_raw(srl_writer* out, const char* text) { srl_put(out, text, strlen(text)); }
/* Write the native two-bit length tag. */
static inline void srl_write_compact(srl_writer* out, size_t value) {
  if (value > 0x3fffffffu) { if (out->status == srl_ok) { out->status = srl_range; } return; }
  const unsigned extra = value <= 63 ? 0 : value <= 16383 ? 1 : value <= 4194303 ? 2 : 3;
  srl_put_byte(out, (uint8_t)((extra << 6) | (value >> (extra*8))));
  for (unsigned i = extra; i > 0; --i) { srl_put_byte(out, (uint8_t)(value >> ((i-1)*8))); }
}
/* Format integer magnitudes without locale dependencies or temporary allocations. */
static inline void srl_write_integer(srl_writer* out, uint64_t value, unsigned width, bool positive) {
  if (out->protocol != srl_json) { for (unsigned i = 0; i < width; ++i) { srl_put_byte(out, (uint8_t)(value >> (i*8))); } return; }
  const unsigned bits = width*8; const uint64_t mask = width == 8 ? UINT64_MAX : (UINT64_C(1) << bits)-1;
  value &= mask; const bool negative = !positive && (value & (UINT64_C(1) << (bits-1))) != 0;
  if (negative) { srl_put_byte(out, '-'); value = (~value+1) & mask; }
  char text[20]; size_t used = sizeof(text);
  do { text[--used] = (char)('0' + value%10); value /= 10; } while (value);
  srl_put(out, text+used, sizeof(text)-used);
}
/* Encode unsigned compact fields, masking low 30 bits for lenient prefixes. */
static inline void srl_write_compact_value(srl_writer* out, uint64_t value, unsigned width, bool prefix, bool strict) {
  if (out->protocol == srl_json) { srl_write_integer(out, value, width, true); return; }
  if (prefix) {
    if (strict && value > UINT64_C(0x3fffffff)) { if (out->status == srl_ok) { out->status = srl_range; } return; }
    value &= UINT64_C(0x3fffffff);
    const unsigned extra = value <= 63 ? 0 : value <= 16383 ? 1 : value <= 4194303 ? 2 : 3;
    srl_put_byte(out, (uint8_t)((extra << 6) | (value >> (extra*8))));
    for (unsigned i = extra; i > 0; --i) { srl_put_byte(out, (uint8_t)(value >> ((i-1)*8))); }
    return;
  }
  uint8_t bytes[10]; size_t count = 0;
  while (value >= UINT64_C(0x80)) { bytes[count++] = (uint8_t)((value & UINT64_C(0x7f)) | UINT64_C(0x80)); value >>= 7; }
  bytes[count++] = (uint8_t)value; srl_put(out, bytes, count);
}
/* Normalize the process locale's decimal separator without changing global locale state. */
static inline void srl_write_float(srl_writer* out, double value, bool single) {
  if (out->protocol != srl_json) {
    if (single) { const float f = (float)value; uint32_t bits; memcpy(&bits, &f, sizeof(bits)); srl_write_integer(out, bits, 4, true); }
    else { uint64_t bits; memcpy(&bits, &value, sizeof(bits)); srl_write_integer(out, bits, 8, true); } return;
  }
  if (!isfinite(value)) { out->status = srl_range; return; }
  char text[128]; const int count = snprintf(text, sizeof(text), "%.*g", single ? FLT_DECIMAL_DIG : DBL_DECIMAL_DIG, value);
  if (count < 0 || (size_t)count >= sizeof(text)) { out->status = srl_range; return; }
  const char* decimal = localeconv()->decimal_point; const size_t size = strlen(decimal); char* point = size ? strstr(text, decimal) : NULL;
  if (point && strcmp(decimal, ".") != 0) { *point = '.'; memmove(point+1, point+size, strlen(point+size)+1); }
  srl_raw(out, text);
}
/* Write binary32 directly so its bit pattern is not widened. */
static inline void srl_write_float32(srl_writer* out, float value) { if (out->protocol == srl_json) { srl_write_float(out, value, true); } else { uint32_t bits; memcpy(&bits, &value, sizeof(bits)); srl_write_integer(out, bits, 4, true); } }
/* Write canonical booleans. */
static inline void srl_write_bool(srl_writer* out, bool value) { if (out->protocol == srl_json) { srl_raw(out, value ? "true" : "false"); } else { srl_put_byte(out, value ? 1 : 0); } }
/* Encode strict UTF-8 and escape JSON controls directly to the output. */
static inline void srl_write_string(srl_writer* out, const srl_string* value) {
  if (!srl_utf8((const uint8_t*)value->data, value->size)) { out->status = srl_invalid; return; }
  if (out->protocol != srl_json) { srl_write_compact(out, value->size); srl_put(out, value->data, value->size); return; }
  static const char hex[] = "0123456789abcdef"; srl_put_byte(out, '"');
  for (size_t i = 0; i < value->size; ++i) {
    const uint8_t b = (uint8_t)value->data[i];
    if (b == '"' || b == '\\') { srl_put_byte(out, '\\'); srl_put_byte(out, b); }
    else if (b < 32) { const char escape[] = {'\\','u','0','0',hex[b >> 4],hex[b & 15]}; srl_put(out, escape, sizeof(escape)); }
    else { srl_put_byte(out, b); }
  }
  srl_put_byte(out, '"');
}
/* Encode byte-valued characters; JSON supports ASCII only. */
static inline void srl_write_char(srl_writer* out, uint8_t value) { if (out->protocol == srl_json) { if (value > 127) { out->status = srl_range; return; } char ch = (char)value; const srl_string text = {&ch,1}; srl_write_string(out, &text); } else { srl_put_byte(out, value); } }
/* Encode a declared enum name or compact ordinal according to its context. */
static inline void srl_write_enum(srl_writer* out, uint32_t value, const char* const* names, size_t count, bool collection) {
  if (value >= count) { out->status = srl_invalid; return; }
  if (out->protocol == srl_json || (out->protocol == srl_binary_string && !collection)) { const srl_string text = {(char*)names[value], strlen(names[value])}; srl_write_string(out, &text); }
  else { srl_write_compact(out, value); }
}
/* Check recursion before writing an object's fields. */
static inline void srl_write_begin(srl_writer* out) { if (out->depth >= 64) { out->status = srl_limit; return; } ++out->depth; if (out->protocol == srl_json) { srl_put_byte(out, '{'); } }
/* Append a pre-encoded schema key. */
static inline void srl_write_field(srl_writer* out, uint32_t id, const srl_field* key, bool first) {
  if (out->protocol == srl_json) { if (!first) { srl_put_byte(out, ','); } srl_put(out, key->json, key->json_size); }
  else if (out->protocol == srl_binary_integer) { srl_write_compact(out, id); }
  else if (out->protocol == srl_binary_string) { srl_put(out, key->binary, key->binary_size); }
}
/* Finish native object framing and nesting accounting. */
static inline void srl_write_end(srl_writer* out) { if (out->protocol == srl_json) { srl_put_byte(out, '}'); } else if (out->protocol != srl_binary_none) { srl_put_byte(out, 0); } if (out->depth) { --out->depth; } }
/* Begin a counted collection. */
static inline void srl_write_array(srl_writer* out, size_t size) { if (out->protocol == srl_json) { srl_put_byte(out, '['); } else { srl_write_compact(out, size); } }
/* Separate JSON collection entries. */
static inline void srl_write_element(srl_writer* out, size_t index) { if (out->protocol == srl_json && index) { srl_put_byte(out, ','); } }
/* Finish a JSON collection. */
static inline void srl_write_array_end(srl_writer* out) { if (out->protocol == srl_json) { srl_put_byte(out, ']'); } }

/* Check bounds before every read; preserve the first reported failure. */
static inline bool srl_require(srl_reader* in, size_t count) { if (in->status != srl_ok) { return false; } if (count > in->size-in->offset) { in->status = srl_truncated; return false; } return true; }
)serializer"
R"serializer(/* Recognize exactly JSON whitespace. */
static inline bool srl_whitespace_byte(uint8_t b) { return b == 32 || b == 9 || b == 10 || b == 13; }
/* Skip JSON whitespace without reading beyond the buffer. */
static inline void srl_whitespace(srl_reader* in) { while (in->offset < in->size && srl_whitespace_byte(in->data[in->offset])) { ++in->offset; } }
/* Require a delimiter without allowing trailing commas. */
static inline void srl_expect(srl_reader* in, uint8_t value) { srl_whitespace(in); if (srl_require(in, 1) && in->data[in->offset++] != value) { in->status = srl_invalid; } }
/* Read complete compact prefixes, including accepted nonminimal forms. */
static inline uint32_t srl_read_compact(srl_reader* in) {
  if (!srl_require(in, 1)) { return 0; } const uint8_t first = in->data[in->offset]; const unsigned extra = first >> 6;
  if (!srl_require(in, extra+1)) { return 0; } uint32_t value = first & 63u;
  for (unsigned i = 1; i <= extra; ++i) { value = value*256 + in->data[in->offset+i]; } in->offset += extra+1; return value;
}
/* Release only long tokens that could not use the reader scratch buffer. */
static inline void srl_token_free(srl_reader* in, char* value) { if (value != in->token) { free(value); } }
/* Read a terminated numeric token, avoiding allocation for ordinary scalar values. */
static inline char* srl_token(srl_reader* in) {
  if (in->status != srl_ok) { return NULL; }
  srl_whitespace(in); const size_t start = in->offset;
  while (in->offset < in->size) { const uint8_t b = in->data[in->offset]; if (b == ',' || b == '}' || b == ']' || srl_whitespace_byte(b)) { break; } ++in->offset; }
  const size_t size = in->offset-start; if (!size || size == SIZE_MAX) { in->status = srl_invalid; return NULL; }
  char* value = size < sizeof(in->token) ? in->token : (char*)malloc(size+1); if (!value) { in->status = srl_allocation; return NULL; }
  memcpy(value, in->data+start, size); value[size] = 0;
  if (memchr(value, 0, size)) { srl_token_free(in, value); in->status = srl_invalid; return NULL; } return value;
}
/* Validate strict numeric grammar before invoking permissive standard parsers. */
static inline bool srl_number(const char* text, bool integer) {
  const char* p = text; if (*p == '-') { ++p; }
  if (*p == '0') { ++p; } else { const char* start = p; while (*p >= '0' && *p <= '9') { ++p; } if (start == p) { return false; } }
  if (!integer && *p == '.') { ++p; const char* start = p; while (*p >= '0' && *p <= '9') { ++p; } if (start == p) { return false; } }
  if (!integer && (*p == 'e' || *p == 'E')) { ++p; if (*p == '+' || *p == '-') { ++p; } const char* start = p; while (*p >= '0' && *p <= '9') { ++p; } if (start == p) { return false; } }
  return !*p;
}
/* Decode exact integer bits, validating width before narrowing. */
static inline uint64_t srl_read_integer(srl_reader* in, unsigned width, bool positive) {
  if (in->protocol != srl_json) { if (!srl_require(in, width)) { return 0; } uint64_t value = 0; for (unsigned i = 0; i < width; ++i) { value |= (uint64_t)in->data[in->offset+i] << (i*8); } in->offset += width; return value; }
  char* text = srl_token(in); if (!text) { return 0; } const bool negative = text[0] == '-'; uint64_t value = 0;
  if (!srl_number(text, true) || (positive && negative)) { in->status = srl_invalid; }
  else {
    errno = 0; char* end; value = strtoull(text + (negative ? 1 : 0), &end, 10);
    const uint64_t max = positive ? width == 8 ? UINT64_MAX : (UINT64_C(1) << (width*8))-1 : (UINT64_C(1) << (width*8-1)) - (negative ? 0 : 1);
    if (errno == ERANGE || *end || value > max) { in->status = srl_range; }
    if (negative) { value = ~value+1; }
  }
  srl_token_free(in, text); return value;
}
/* Validate complete compact framing and target-width overflow before cursor publication. */
static inline uint64_t srl_read_compact_value(srl_reader* in, unsigned width, bool prefix) {
  if (in->protocol == srl_json) { return srl_read_integer(in, width, true); }
  const size_t start = in->offset;
  if (prefix) {
    const uint64_t value = srl_read_compact(in);
    if (in->status == srl_ok && width < 8 && value >= (UINT64_C(1) << (width*8))) { in->offset = start; in->status = srl_range; return 0; }
    return value;
  }
  const unsigned bits = width*8;
  uint64_t value = 0;
  for (unsigned index = 0; index < (bits+6)/7; ++index) {
    if (!srl_require(in, index+1)) { return 0; }
    const uint8_t byte = in->data[start+index], payload = byte & 0x7f;
    const unsigned remaining_bits = bits-index*7;
    if (payload >= (UINT64_C(1) << (remaining_bits < 7 ? remaining_bits : 7))) { in->status = srl_range; return 0; }
    value |= (uint64_t)payload << (index*7);
    if (byte < 0x80) {
      if (index != 0 && payload == 0) { in->status = srl_invalid; return 0; }
      in->offset = start+index+1;
      return value;
    }
  }
  in->status = srl_invalid; return 0;
}
/* Recover a signed value without implementation-defined unsigned-to-signed narrowing. */
static inline int64_t srl_signed(uint64_t value, unsigned width) {
  const unsigned bits = width*8; const uint64_t sign = UINT64_C(1) << (bits-1);
  const uint64_t mask = width == 8 ? UINT64_MAX : (UINT64_C(1) << bits)-1; value &= mask;
  return (value & sign) ? -(int64_t)((~value) & mask)-1 : (int64_t)value;
}
/* Decode IEEE numbers or locale-adjusted JSON numeric tokens without global locale mutation. */
static inline double srl_read_float(srl_reader* in, bool single) {
  if (in->protocol != srl_json) {
    if (single) { uint32_t bits = (uint32_t)srl_read_integer(in, 4, true); float value; memcpy(&value, &bits, sizeof(value)); return value; }
    uint64_t bits = srl_read_integer(in, 8, true); double value; memcpy(&value, &bits, sizeof(value)); return value;
  }
  char* text = srl_token(in); if (!text) { return 0; } double value = 0;
  if (!srl_number(text, false)) { in->status = srl_invalid; }
  else {
    bool nonzero = false; for (const char* p = text; *p && *p != 'e' && *p != 'E'; ++p) { if (*p >= '1' && *p <= '9') { nonzero = true; } }
    const char* decimal = localeconv()->decimal_point; char* point = strchr(text, '.');
    if (point && strcmp(decimal, ".") != 0) {
      const size_t prefix = (size_t)(point-text), length = strlen(text), extra = strlen(decimal);
      if (extra > SIZE_MAX-length) { in->status = srl_allocation; srl_token_free(in, text); return 0; }
      char* next = (char*)malloc(length+extra); if (!next) { in->status = srl_allocation; srl_token_free(in, text); return 0; }
      memcpy(next, text, prefix); memcpy(next+prefix, decimal, extra); memcpy(next+prefix+extra, point+1, length-prefix); srl_token_free(in, text); text = next;
    }
    char* end; value = single ? (double)strtof(text, &end) : strtod(text, &end);
    if (*end || !isfinite(value) || (value == 0 && nonzero)) { in->status = srl_range; }
  }
  srl_token_free(in, text); return value;
}
/* Read binary32 without widening its native payload bits. */
static inline float srl_read_float32(srl_reader* in) { if (in->protocol == srl_json) { return (float)srl_read_float(in, true); } const uint32_t bits = (uint32_t)srl_read_integer(in, 4, true); float value; memcpy(&value, &bits, sizeof(value)); return value; }
/* Read only canonical booleans. */
static inline bool srl_read_bool(srl_reader* in) {
  if (in->protocol != srl_json) { const uint64_t value = srl_read_integer(in, 1, true); if (value > 1) { in->status = srl_invalid; } return value == 1; }
  char* text = srl_token(in); if (!text) { return false; } const bool value = strcmp(text, "true") == 0;
  if (!value && strcmp(text, "false") != 0) { in->status = srl_invalid; } srl_token_free(in, text); return value;
}
/* Decode one hexadecimal UTF-16 code unit. */
static inline uint32_t srl_hex(srl_reader* in) {
  if (!srl_require(in, 4)) { return 0; } uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) { const uint8_t b = in->data[in->offset++]; unsigned digit;
    if (b >= '0' && b <= '9') { digit = b-'0'; } else if (b >= 'a' && b <= 'f') { digit = b-'a'+10; } else if (b >= 'A' && b <= 'F') { digit = b-'A'+10; } else { in->status = srl_invalid; return 0; } value = value*16+digit;
  } return value;
}
/* Decode one bounded string and replace its initialized destination only on success. */
static inline void srl_read_string(srl_reader* in, srl_string* result) {
  if (in->status != srl_ok) { return; }
  if (in->protocol != srl_json) {
    const size_t count = srl_read_compact(in); if (count > in->limits.max_string_bytes) { in->status = srl_limit; return; }
    if (!srl_require(in, count)) { return; } in->status = srl_string_set(result, (const char*)in->data+in->offset, count); in->offset += count; return;
  }
  srl_expect(in, '"'); const size_t start = in->offset; srl_writer out = {0}; out.protocol = srl_binary_none;
  while (in->status == srl_ok && out.status == srl_ok) {
    if (!srl_require(in, 1)) { break; } const uint8_t b = in->data[in->offset++];
    if (in->offset-start-1 > in->limits.max_string_bytes) { in->status = srl_limit; break; }
    if (b == '"') { break; } if (b < 32) { in->status = srl_invalid; break; }
    if (b != '\\') { srl_put_byte(&out, b); continue; }
    if (!srl_require(in, 1)) { break; } const uint8_t escaped = in->data[in->offset++];
    switch (escaped) {
    case '"': case '\\': case '/': srl_put_byte(&out, escaped); break;
    case 'b': srl_put_byte(&out, 8); break; case 'f': srl_put_byte(&out, 12); break; case 'n': srl_put_byte(&out, 10); break; case 'r': srl_put_byte(&out, 13); break; case 't': srl_put_byte(&out, 9); break;
    case 'u': {
      uint32_t code = srl_hex(in);
      if (code >= 0xd800 && code <= 0xdbff) {
        if (!srl_require(in, 2)) { break; } if (in->data[in->offset] != '\\' || in->data[in->offset+1] != 'u') { in->status = srl_invalid; break; } in->offset += 2;
        const uint32_t low = srl_hex(in); if (low < 0xdc00 || low > 0xdfff) { in->status = srl_invalid; break; } code = 0x10000 + ((code-0xd800) << 10) + low-0xdc00;
      } else if (code >= 0xdc00 && code <= 0xdfff) { in->status = srl_invalid; break; }
      if (code < 0x80) { srl_put_byte(&out, (uint8_t)code); }
      else if (code < 0x800) { srl_put_byte(&out, (uint8_t)(0xc0 | code >> 6)); srl_put_byte(&out, (uint8_t)(0x80 | (code & 63))); }
      else if (code < 0x10000) { srl_put_byte(&out, (uint8_t)(0xe0 | code >> 12)); srl_put_byte(&out, (uint8_t)(0x80 | (code >> 6 & 63))); srl_put_byte(&out, (uint8_t)(0x80 | (code & 63))); }
      else { srl_put_byte(&out, (uint8_t)(0xf0 | code >> 18)); srl_put_byte(&out, (uint8_t)(0x80 | (code >> 12 & 63))); srl_put_byte(&out, (uint8_t)(0x80 | (code >> 6 & 63))); srl_put_byte(&out, (uint8_t)(0x80 | (code & 63))); } break;
    }
    default: in->status = srl_invalid; break;
    }
  }
  if (in->status == srl_ok) { in->status = out.status; }
  if (in->status == srl_ok && out.buffer.size > in->limits.max_string_bytes) { in->status = srl_limit; }
  if (in->status == srl_ok) { in->status = srl_string_set(result, (const char*)out.buffer.data, out.buffer.size); }
  srl_buffer_free(&out.buffer);
}
/* Decode a byte-valued character and require ASCII in JSON. */
static inline uint8_t srl_read_char(srl_reader* in) { if (in->protocol != srl_json) { return (uint8_t)srl_read_integer(in, 1, true); } srl_string value = {0}; srl_read_string(in, &value); uint8_t result = 0; if (in->status == srl_ok) { if (value.size != 1 || (uint8_t)value.data[0] > 127) { in->status = srl_invalid; } else { result = (uint8_t)value.data[0]; } } srl_string_free(&value); return result; }
/* Decode only declared enum names and ordinals. */
static inline uint32_t srl_read_enum(srl_reader* in, const char* const* names, size_t count, bool collection) {
  uint32_t value = (uint32_t)count;
  if (in->protocol == srl_json || (in->protocol == srl_binary_string && !collection)) { srl_string name = {0}; srl_read_string(in, &name); for (size_t i = 0; in->status == srl_ok && i < count; ++i) { if (srl_string_equal(&name, names[i])) { value = (uint32_t)i; break; } } srl_string_free(&name); }
  else { value = srl_read_compact(in); }
  if (in->status == srl_ok && value >= count) { in->status = srl_invalid; } return value;
}
/* Bound object nesting before decoding any fields. */
static inline void srl_read_begin(srl_reader* in) { if (in->status != srl_ok) { return; } if (in->depth >= in->limits.max_depth) { in->status = srl_limit; return; } ++in->depth; if (in->protocol == srl_json) { srl_expect(in, '{'); } }
/* Read one key; the caller releases any allocated name even after failure. */
static inline bool srl_read_key(srl_reader* in, const uint32_t* ids, size_t count, size_t cursor, srl_key* key) {
  memset(key, 0, sizeof(*key)); if (in->status != srl_ok) { return false; }
  if (in->protocol == srl_binary_none) { if (cursor == count) { return false; } key->id = ids[cursor]; return true; }
  if (in->protocol == srl_binary_integer) { key->id = srl_read_compact(in); return in->status == srl_ok && key->id != 0; }
)serializer"
R"serializer(  if (in->protocol == srl_json) { srl_whitespace(in); if (!srl_require(in, 1)) { return false; } if (in->data[in->offset] == '}') { ++in->offset; return false; } if (cursor) { srl_expect(in, ','); } }
  key->named = true; srl_read_string(in, &key->name);
  if (in->status != srl_ok || (in->protocol == srl_binary_string && !key->name.size)) { srl_string_free(&key->name); return false; }
  if (in->protocol == srl_json) { srl_expect(in, ':'); } return true;
}
/* Leave a successfully entered object. */
static inline void srl_read_end(srl_reader* in) { if (in->depth) { --in->depth; } }
/* Validate a collection count before allocating storage. */
static inline size_t srl_read_array(srl_reader* in) { if (in->protocol == srl_json) { srl_expect(in, '['); return 0; } const size_t count = srl_read_compact(in); if (in->status == srl_ok && count > in->limits.max_elements-in->elements) { in->status = srl_limit; } return count; }
/* Validate minimum binary bytes before reserving fixed-width arrays. */
static inline size_t srl_array_capacity(srl_reader* in, size_t count, size_t width) { if (in->protocol == srl_json) { return 0; } if (width && count > (in->size-in->offset)/width) { in->status = srl_truncated; return 0; } return width ? count : count < 256 ? count : 256; }
/* Charge one logical collection slot before reading its contents. */
static inline bool srl_read_element(srl_reader* in, size_t index, size_t count) {
  if (in->status != srl_ok) { return false; }
  if (in->protocol != srl_json) { if (index == count) { return false; } }
  else { srl_whitespace(in); if (!srl_require(in, 1)) { return false; } if (in->data[in->offset] == ']') { ++in->offset; return false; } if (index) { srl_expect(in, ','); } }
  if (in->elements == in->limits.max_elements) { in->status = srl_limit; return false; } ++in->elements; return in->status == srl_ok;
}
/* Grow typed collection storage through void values, avoiding pointer-to-pointer aliasing. */
static inline void* srl_grow(srl_reader* in, void* old, size_t capacity, size_t item_size) {
  if (in->status != srl_ok) { return NULL; } if (capacity && item_size > SIZE_MAX/capacity) { in->status = srl_allocation; return NULL; }
  void* next = realloc(old, capacity*item_size); if (!next && capacity) { in->status = srl_allocation; } return next;
}
/* Require exact input consumption before committing the candidate. */
static inline void srl_finish(srl_reader* in) { if (in->protocol == srl_json) { srl_whitespace(in); } if (in->status == srl_ok && in->offset != in->size) { in->status = srl_invalid; } }
/* Parse canonical uint16 components without allocating temporary text. */
static inline bool srl_version_parts(const srl_string* value, size_t count, uint16_t* parts) {
  if (count < 2 || count > 4 || !value->data || value->size > count*6-1) { return false; } size_t offset = 0;
  for (size_t i = 0; i < count; ++i) { const size_t start = offset; uint32_t n = 0; while (offset < value->size && value->data[offset] != '.') { const unsigned char c = (unsigned char)value->data[offset++]; if (c < '0' || c > '9') { return false; } n = n*10 + c-'0'; if (n > UINT16_MAX) { return false; } } if (offset == start || (offset-start > 1 && value->data[start] == '0')) { return false; } parts[i] = (uint16_t)n; if (i+1 < count) { if (offset == value->size) { return false; } ++offset; } }
  return offset == value->size;
}
/* Order numeric components; invalid source text is outside every supported range. */
static inline int srl_version_compare(const srl_string* value, const char* boundary, size_t count) { uint16_t x[4], y[4]; const srl_string text = {(char*)boundary, strlen(boundary)}; if (!srl_version_parts(value, count, x) || !srl_version_parts(&text, count, y)) { return 2; } for (size_t i = 0; i < count; ++i) { if (x[i] != y[i]) { return x[i] < y[i] ? -1 : 1; } } return 0; }
/* Encode count-free little-endian binary components or canonical JSON text. */
static inline void srl_write_version(srl_writer* out, const srl_string* value, size_t count) { uint16_t parts[4]; if (!srl_version_parts(value, count, parts)) { out->status = srl_invalid; return; } if (out->protocol == srl_json) { srl_write_string(out, value); } else { for (size_t i = 0; i < count; ++i) { srl_write_integer(out, parts[i], 2, true); } } }
/* Decode a fixed count of components and preserve transactional string ownership. */
static inline void srl_read_version(srl_reader* in, srl_string* value, size_t count) { if (in->protocol == srl_json) { uint16_t parts[4]; srl_read_string(in, value); if (in->status == srl_ok && !srl_version_parts(value, count, parts)) { in->status = srl_invalid; } return; } char text[24]; size_t offset = 0; for (size_t i = 0; i < count; ++i) { const uint64_t n = srl_read_integer(in, 2, true); if (in->status != srl_ok) { return; } int written = snprintf(text+offset, sizeof(text)-offset, i ? ".%u" : "%u", (unsigned)n); if (written < 0 || (size_t)written >= sizeof(text)-offset) { in->status = srl_range; return; } offset += (size_t)written; } in->status = srl_string_set(value, text, offset); }
/* Skip bounded well-formed JSON values only when the policy explicitly permits extensions. */
static inline void srl_skip_value(srl_reader* in) {
  if (in->status != srl_ok) { return; } if (in->protocol != srl_json || in->limits.read_policy != srl_read_flexible) { in->status = srl_invalid; return; } srl_whitespace(in); if (!srl_require(in, 1)) { return; } const uint8_t c = in->data[in->offset];
  if (c == '"') { srl_string text = {0}; srl_read_string(in, &text); srl_string_free(&text); return; }
  if (c == '{') { srl_read_begin(in); size_t cursor = 0, count = 0; srl_string* seen = NULL; srl_key key = {0}; while (srl_read_key(in, NULL, 0, cursor++, &key)) { if (in->elements >= in->limits.max_elements) { in->status = srl_limit; } else { ++in->elements; } for (size_t i = 0; i < count; ++i) { if (!srl_string_compare(&seen[i], &key.name)) { in->status = srl_invalid; break; } } if (in->status != srl_ok) { srl_string_free(&key.name); break; } srl_string* next = (srl_string*)srl_grow(in, seen, count+1, sizeof(*seen)); if (!next) { srl_string_free(&key.name); break; } seen = next; seen[count++] = key.name; memset(&key.name, 0, sizeof(key.name)); srl_skip_value(in); } srl_string_free(&key.name); for (size_t i = 0; i < count; ++i) { srl_string_free(&seen[i]); } free(seen); srl_read_end(in); return; }
  if (c == '[') { if (in->depth >= in->limits.max_depth) { in->status = srl_limit; return; } ++in->depth; const size_t count = srl_read_array(in); for (size_t i = 0; srl_read_element(in, i, count); ++i) { srl_skip_value(in); } --in->depth; return; }
  char* text = srl_token(in); if (!text) { return; } if (strcmp(text, "null") && strcmp(text, "true") && strcmp(text, "false") && !srl_number(text, false)) { in->status = srl_invalid; } srl_token_free(in, text);
}
)serializer"
R"serializer(
/* Reserve zeroed fixed digest storage; bare digest starts empty. */
static inline srl_status srl_digest_init(srl_buffer* value, size_t expected) {
  if (!expected) { return srl_ok; }
  if (!srl_reserve(value, expected)) { return srl_allocation; }
  memset(value->data, 0, expected); value->size = expected; return srl_ok;
}
/* Encode digest bytes without performing a hash computation. */
static inline void srl_write_digest(srl_writer* out, const srl_buffer* value, size_t expected) {
  if (out->status != srl_ok) { return; }
  if ((expected && value->size != expected) || (value->size && !value->data)) { out->status = srl_invalid; return; }
  srl_write_array(out, value->size);
  for (size_t i = 0; i < value->size; ++i) { srl_write_element(out, i); srl_write_integer(out, value->data[i], 1, true); }
  srl_write_array_end(out);
}
/* Decode transactionally with checked extents and geometric buffer growth. */
static inline void srl_read_digest(srl_reader* in, srl_buffer* value, size_t expected) {
  if (in->status != srl_ok) { return; }
  const size_t count = srl_read_array(in);
  if (in->status != srl_ok) { return; }
  if (expected && in->protocol != srl_json && count != expected) { in->status = srl_invalid; return; }
  srl_buffer fresh = {0};
  const size_t capacity = srl_array_capacity(in, count, 1);
  if (in->status == srl_ok && capacity && !srl_reserve(&fresh, capacity)) { in->status = srl_allocation; }
  for (size_t i = 0; srl_read_element(in, i, count); ++i) {
    if (expected && i >= expected) { in->status = srl_invalid; break; }
    const uint8_t byte = (uint8_t)srl_read_integer(in, 1, true);
    if (in->status != srl_ok) { break; }
    if (!srl_reserve(&fresh, 1)) { in->status = srl_allocation; break; }
    fresh.data[fresh.size++] = byte;
  }
  if (in->status == srl_ok && expected && fresh.size != expected) { in->status = srl_invalid; }
  if (in->status == srl_ok) { srl_buffer_free(value); *value = fresh; } else { srl_buffer_free(&fresh); }
}
)serializer"
