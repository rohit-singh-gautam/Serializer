// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
import (
  "encoding/binary"
  "encoding/json"
  "fmt"
  "math"
  "regexp"
  "sort"
  "strconv"
  "strings"
  "unicode/utf8"
)

// Protocol selects the existing native wire format.
type Protocol uint8
const (
  JSON Protocol = iota
  BINARY_NONE
  BINARY_INTEGER
  BINARY_STRING
)
const srlCompactPrefixMax uint64 = 0x3fffffff
const srlVarintPayloadBits = 7
const srlVarintPayloadMask uint64 = 0x7f
const srlVarintContinuation byte = 0x80

// Limits bounds each message; MaxElements counts collection entries cumulatively.
// ReadPolicy controls revision acceptance and safe JSON extensions.
type ReadPolicy uint8
const ( STRICT ReadPolicy = iota; COMPATIBLE; FLEXIBLE )
type Limits struct { MaxBytes, MaxStringBytes, MaxElements, MaxDepth int; ReadPolicy ReadPolicy }
// DefaultLimits returns an independent resource policy.
func DefaultLimits() Limits { return Limits{64 * 1024 * 1024, 16 * 1024 * 1024, 1000000, 64, STRICT} }

var srlInteger = regexp.MustCompile(`^-?(0|[1-9][0-9]*)$`)
var srlNumber = regexp.MustCompile(`^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$`)

// srlVersionParts validates canonical bounded decimal components.
func srlVersionParts(value string, count int) ([]uint64, bool) {
  parts := strings.Split(value, "."); if len(parts) != count { return nil, false }; result := make([]uint64, count)
  for i, part := range parts { if !srlInteger.MatchString(part) || strings.HasPrefix(part, "-") || len(part) > 5 { return nil, false }; n, err := strconv.ParseUint(part, 10, 16); if err != nil { return nil, false }; result[i] = n }; return result, true
}
// srlVersionCompare orders valid numeric components; invalid input lies outside every supported range.
func srlVersionCompare(a, b string, count int) int {
  x, ok := srlVersionParts(a, count); if !ok { return 2 }; y, ok := srlVersionParts(b, count); if !ok { return 2 }
  for i := range x { if x[i] < y[i] { return -1 }; if x[i] > y[i] { return 1 } }; return 0
}
// version writes a fixed number of little-endian components.
func (out *srlWriter) version(value string, count int) { parts, ok := srlVersionParts(value, count); if !ok { out.fail("Invalid dotted revision"); return }; if out.json() { out.text(value) } else { for _, n := range parts { out.integer(n, 2, true) } } }
// version reads components without a binary count prefix.
func (in *srlReader) version(count int) string { value := ""; if in.json() { value = in.text() } else { parts := make([]string, count); for i := range parts { parts[i] = strconv.FormatUint(in.integer(2, true), 10) }; value = strings.Join(parts, ".") }; if _, ok := srlVersionParts(value, count); !ok { in.fail("Invalid dotted revision") }; return value }
// skipValue consumes only bounded well-formed JSON extensions.
func (in *srlReader) skipValue() {
  if !in.json() || in.limits.ReadPolicy != FLEXIBLE { in.fail("Unknown field"); return }; in.whitespace(); if !in.require(1) { return }
  switch in.bytes[in.offset] {
  case '"': in.text(); return
  case '{': in.beginObject(); first := true; seen := make(map[string]bool); for in.nextObject(first) { first = false; key := in.key(); if seen[key] { in.fail("Duplicate field"); return }; seen[key] = true; in.elements++; if in.elements > in.limits.MaxElements { in.fail("Element limit"); return }; in.skipValue() }; in.endObject(); return
  case '[': in.depth++; if in.depth > in.limits.MaxDepth { in.fail("Depth limit"); return }; count := in.beginArray(); for i := 0; in.nextElement(i, count); i++ { in.skipValue() }; in.depth--; return
  }
  value := in.token(); if value != "null" && value != "true" && value != "false" && !srlNumber.MatchString(value) { in.fail("Invalid JSON value") }
}

// srlWriter owns one output message and retains its first failure.
type srlWriter struct { protocol Protocol; bytes []byte; depth int; err error }
// srlNewWriter validates protocol selection.
func srlNewWriter(protocol Protocol) *srlWriter {
  result := &srlWriter{protocol: protocol, bytes: make([]byte, 0, 256)}
  if protocol > BINARY_STRING { result.fail("Unknown protocol") }
  return result
}
// fail records a stable failure without panicking on untrusted values.
func (out *srlWriter) fail(message string) { if out.err == nil { out.err = fmt.Errorf("%s", message) } }
// json reports whether output is JSON.
func (out *srlWriter) json() bool { return out.protocol == JSON }
// raw appends punctuation or already validated numeric text.
func (out *srlWriter) raw(value string) { if out.err == nil { out.bytes = append(out.bytes, value...) } }
// compact writes the native two-bit length tag.
func (out *srlWriter) compact(value int) {
  if out.err != nil { return }
  if value < 0 || value > 0x3fffffff { out.fail("Compact integer out of range"); return }
  extra := 0
  if value > 0x3fffff { extra = 3 } else if value > 0x3fff { extra = 2 } else if value > 0x3f { extra = 1 }
  out.bytes = append(out.bytes, byte(extra << 6 | value >> (extra * 8)))
  for shift := (extra - 1) * 8; shift >= 0; shift -= 8 { out.bytes = append(out.bytes, byte(value >> shift)) }
}
// compactPrefixBits writes a proven 30-bit value without a range branch.
func (out *srlWriter) compactPrefixBits(value uint64) {
  extra := 0
  if value > 0x3fffff { extra = 3 } else if value > 0x3fff { extra = 2 } else if value > 0x3f { extra = 1 }
  out.bytes = append(out.bytes, byte(uint64(extra) << 6 | value >> (extra * 8)))
  for shift := (extra - 1) * 8; shift >= 0; shift -= 8 { out.bytes = append(out.bytes, byte(value >> shift)) }
}
// compactPrefix rejects values above its 30-bit binary contract before payload output.
func (out *srlWriter) compactPrefix(value uint64, width int) {
  if out.err != nil { return }
  if out.json() { out.integer(value, width, true); return }
  if value > srlCompactPrefixMax { out.fail("Compact prefix out of range"); return }
  out.compactPrefixBits(value)
}
// compactPrefixLenient deliberately masks binary values without a range branch.
func (out *srlWriter) compactPrefixLenient(value uint64, width int) {
  if out.err != nil { return }
  if out.json() { out.integer(value, width, true); return }
  out.compactPrefixBits(value & srlCompactPrefixMax)
}
// compactVarint preserves all unsigned storage bits using canonical LEB128.
func (out *srlWriter) compactVarint(value uint64, width int) {
  if out.err != nil { return }
  if out.json() { out.integer(value, width, true); return }
  for {
    payload := byte(value & srlVarintPayloadMask); value >>= srlVarintPayloadBits
    if value != 0 { payload |= srlVarintContinuation }
    out.bytes = append(out.bytes, payload)
    if value == 0 { return }
  }
}
// compactVarintLenient needs no checks because the generated storage is unsigned and typed.
func (out *srlWriter) compactVarintLenient(value uint64, width int) { out.compactVarint(value, width) }
// integer preserves the declared width and formats signed values explicitly.
func (out *srlWriter) integer(value uint64, width int, unsigned bool) {
  if out.err != nil { return }
  if out.json() {
    if unsigned { out.raw(strconv.FormatUint(value, 10)) } else {
      shift := 64 - width * 8
      out.raw(strconv.FormatInt(int64(value << shift) >> shift, 10))
    }
  } else {
    switch width {
    case 1: out.bytes = append(out.bytes, byte(value))
    case 2: out.bytes = binary.LittleEndian.AppendUint16(out.bytes, uint16(value))
    case 4: out.bytes = binary.LittleEndian.AppendUint32(out.bytes, uint32(value))
    default: out.bytes = binary.LittleEndian.AppendUint64(out.bytes, value)
    }
  }
}
// floating serializes a finite JSON number or IEEE binary bits.
func (out *srlWriter) floating(value float64, single bool) {
  if out.err != nil { return }
  bits := 64
  if single { bits = 32; value = float64(float32(value)) }
  if out.json() {
    if math.IsInf(value, 0) || math.IsNaN(value) { out.fail("Nonfinite JSON number"); return }
    out.raw(strconv.FormatFloat(value, 'g', -1, bits))
  } else if single { out.integer(uint64(math.Float32bits(float32(value))), 4, true) } else { out.integer(math.Float64bits(value), 8, true) }
}
// float32 preserves binary NaN payloads without widening.
func (out *srlWriter) float32(value float32) {
  if out.json() { out.floating(float64(value), true) } else { out.integer(uint64(math.Float32bits(value)), 4, true) }
}
// boolean writes canonical booleans.
func (out *srlWriter) boolean(value bool) {
  if out.json() { out.raw(strconv.FormatBool(value)) } else if value { out.integer(1, 1, true) } else { out.integer(0, 1, true) }
}
// character writes a byte, limiting JSON characters to ASCII.
func (out *srlWriter) character(value byte) {
  if out.json() { if value > 127 { out.fail("JSON char must be ASCII"); return }; out.text(string(rune(value))) } else { out.integer(uint64(value), 1, true) }
}
// text enforces the portable UTF-8 string contract.
func (out *srlWriter) text(value string) {
  if !utf8.ValidString(value) { out.fail("Invalid UTF-8"); return }
  if out.json() { encoded, err := json.Marshal(value); if err != nil { out.fail("Invalid string"); return }; out.raw(string(encoded)) } else { out.compact(len(value)); out.raw(value) }
}
// beginObject bounds output recursion, including cycles in user objects.
func (out *srlWriter) beginObject() {
  out.depth++
  if out.depth > 64 { out.fail("Encode depth limit"); return }
  if out.json() { out.raw("{") }
}
// field emits the native key without changing its schema identity.
func (out *srlWriter) field(id int, key srlField, first bool) {
  if out.err != nil { return }
  if out.json() { if !first { out.raw(",") }; out.bytes = append(out.bytes, key.json...) } else if out.protocol == BINARY_INTEGER { out.compact(id) } else if out.protocol == BINARY_STRING { out.bytes = append(out.bytes, key.binary...) }
}
// srlField holds immutable pre-encoded schema names shared by encoding sessions.
type srlField struct { binary, json []byte }
// srlMakeField computes key bytes once during package initialization.
func srlMakeField(name string) srlField {
  out := srlNewWriter(BINARY_STRING); out.text(name)
  text, _ := json.Marshal(name)
  return srlField{out.bytes, append(text, ':')}
}
// endObject emits the required terminator.
func (out *srlWriter) endObject() { if out.json() { out.raw("}") } else if out.protocol != BINARY_NONE { out.compact(0) }; out.depth-- }
// beginArray writes collection framing.
func (out *srlWriter) beginArray(count int) { if out.json() { out.raw("[") } else { out.compact(count) } }
// element separates JSON entries.
func (out *srlWriter) element(index int) { if out.json() && index != 0 { out.raw(",") } }
// endArray closes a JSON collection.
func (out *srlWriter) endArray() { if out.json() { out.raw("]") } }

)serializer"
R"serializer(// srlReader is a bounded shared cursor; methods stop after the first error.
type srlReader struct { bytes []byte; protocol Protocol; limits Limits; offset, depth, elements int; err error }
// srlNewReader checks policy and input before any allocation driven by data.
func srlNewReader(bytes []byte, protocol Protocol, limits Limits) *srlReader {
  in := &srlReader{bytes: bytes, protocol: protocol, limits: limits}
  if limits.ReadPolicy > FLEXIBLE { in.fail("Invalid read policy") }; if protocol > BINARY_STRING { in.fail("Unknown protocol") }
  if limits.MaxBytes < 0 || limits.MaxStringBytes < 0 || limits.MaxElements < 0 || limits.MaxDepth < 0 { in.fail("Negative decode limit") }
  if len(bytes) > limits.MaxBytes { in.fail("Message byte limit") }
  return in
}
// fail records the first byte offset without including message contents.
func (in *srlReader) fail(message string) { if in.err == nil { in.err = fmt.Errorf("%s at byte %d", message, in.offset) } }
// json tests the active protocol.
func (in *srlReader) json() bool { return in.protocol == JSON }
// require checks lengths before indexing or arithmetic.
func (in *srlReader) require(count int) bool {
  if in.err != nil { return false }
  if count < 0 || count > len(in.bytes) - in.offset { in.fail("Truncated input"); return false }; return true
}
// whitespace skips only the four JSON whitespace bytes.
func (in *srlReader) whitespace() {
  for in.offset < len(in.bytes) && strings.ContainsRune(" \t\r\n", rune(in.bytes[in.offset])) { in.offset++ }
}
// expect consumes one required delimiter.
func (in *srlReader) expect(value byte) {
  in.whitespace(); if !in.require(1) { return }; if in.bytes[in.offset] != value { in.fail("Unexpected delimiter"); return }; in.offset++
}
// compact consumes a complete compact prefix atomically.
func (in *srlReader) compact() int {
  if !in.require(1) { return 0 }; first := in.bytes[in.offset]; extra := int(first >> 6)
  if !in.require(extra + 1) { return 0 }; value := int(first & 63)
  for i := 1; i <= extra; i++ { value = value << 8 | int(in.bytes[in.offset + i]) }
  in.offset += extra + 1; return value
}
// compactPrefix validates destination width before committing the complete binary prefix.
func (in *srlReader) compactPrefix(width int) uint64 {
  if in.json() { return in.integer(width, true) }
  if !in.require(1) { return 0 }; first := in.bytes[in.offset]; extra := int(first >> 6)
  if !in.require(extra + 1) { return 0 }; value := uint64(first & 63)
  for index := 1; index <= extra; index++ { value = value << 8 | uint64(in.bytes[in.offset + index]) }
  if width < 4 && value >= uint64(1) << (width * 8) { in.fail("Compact prefix out of range"); return 0 }
  in.offset += extra + 1; return value
}
// compactVarint validates count, overflow, and canonical form before committing its cursor.
func (in *srlReader) compactVarint(width int) uint64 {
  if in.json() { return in.integer(width, true) }
  bits := width * 8; count := (bits + srlVarintPayloadBits - 1) / srlVarintPayloadBits
  var value uint64
  for index := 0; index < count; index++ {
    if !in.require(index + 1) { return 0 }
    encoded := in.bytes[in.offset + index]; payload := uint64(encoded) & srlVarintPayloadMask
    shift := index * srlVarintPayloadBits; available := bits - shift
    if available < srlVarintPayloadBits && payload >= uint64(1) << available { in.fail("Compact varint out of range"); return 0 }
    value |= payload << shift
    if encoded & srlVarintContinuation == 0 {
      if index > 0 && payload == 0 { in.fail("Noncanonical compact varint"); return 0 }
      in.offset += index + 1; return value
    }
  }
  in.fail("Compact varint out of range"); return 0
}
// token preserves numeric digits until a typed parse is requested.
func (in *srlReader) token() string {
  in.whitespace(); start := in.offset
  for in.offset < len(in.bytes) && !strings.ContainsRune(",}] \t\r\n", rune(in.bytes[in.offset])) { in.offset++ }
  if start == in.offset { in.fail("Missing token") }; return string(in.bytes[start:in.offset])
}
// integer checks JSON grammar and exact integer ranges.
func (in *srlReader) integer(width int, unsigned bool) uint64 {
  if !in.json() {
    if !in.require(width) { return 0 }; start := in.offset; in.offset += width
    switch width {
    case 1: return uint64(in.bytes[start])
    case 2: return uint64(binary.LittleEndian.Uint16(in.bytes[start:in.offset]))
    case 4: return uint64(binary.LittleEndian.Uint32(in.bytes[start:in.offset]))
    default: return binary.LittleEndian.Uint64(in.bytes[start:in.offset])
    }
  }
  text := in.token(); if !srlInteger.MatchString(text) { in.fail("Invalid integer"); return 0 }
  if unsigned { result, err := strconv.ParseUint(text, 10, width * 8); if err != nil { in.fail("Integer out of range") }; return result }
  result, err := strconv.ParseInt(text, 10, width * 8); if err != nil { in.fail("Integer out of range") }; return uint64(result)
}
// floating rejects malformed numbers, overflow, and underflow to zero.
func (in *srlReader) floating(single bool) float64 {
  if !in.json() { if single { return float64(math.Float32frombits(uint32(in.integer(4, true)))) }; return math.Float64frombits(in.integer(8, true)) }
  text := in.token(); if !srlNumber.MatchString(text) { in.fail("Invalid number"); return 0 }
  bits := 64; if single { bits = 32 }; value, err := strconv.ParseFloat(text, bits)
  mantissa := strings.FieldsFunc(text, func(c rune) bool { return c == 'e' || c == 'E' })
  if err != nil || math.IsInf(value, 0) || (value == 0 && len(mantissa) > 0 && strings.ContainsAny(mantissa[0], "123456789")) { in.fail("Floating-point range") }; return value
}
// float32 preserves binary single-precision payload bits.
func (in *srlReader) float32() float32 { if in.json() { return float32(in.floating(true)) }; return math.Float32frombits(uint32(in.integer(4, true))) }
// boolean accepts only canonical booleans.
func (in *srlReader) boolean() bool {
  if in.json() { text := in.token(); if text == "true" { return true }; if text == "false" { return false } } else { value := in.integer(1, true); if value <= 1 { return value == 1 } }; in.fail("Invalid boolean"); return false
}
// character reads one byte or one ASCII JSON character.
func (in *srlReader) character() byte {
  if !in.json() { return byte(in.integer(1, true)) }; value := in.text(); if len(value) != 1 || value[0] > 127 { in.fail("Invalid char"); return 0 }; return value[0]
}
// srlValidJSONText rejects surrogate replacement accepted by encoding/json.
func srlValidJSONText(bytes []byte) bool {
  if !utf8.Valid(bytes) { return false }
  for i := 1; i + 1 < len(bytes); i++ {
    if bytes[i] != '\\' { continue }; i++
    if bytes[i] != 'u' { continue }; if i + 4 >= len(bytes) { return false }
    value, err := strconv.ParseUint(string(bytes[i+1:i+5]), 16, 16); if err != nil { return false }; i += 4
    if value >= 0xdc00 && value <= 0xdfff { return false }
    if value >= 0xd800 && value <= 0xdbff {
      if i + 6 >= len(bytes) || bytes[i+1] != '\\' || bytes[i+2] != 'u' { return false }
      low, err := strconv.ParseUint(string(bytes[i+3:i+7]), 16, 16); if err != nil || low < 0xdc00 || low > 0xdfff { return false }; i += 6
    }
  }
  return true
}
// text validates escaped and decoded UTF-8 lengths.
func (in *srlReader) text() string {
  if !in.json() {
    count := in.compact(); if count > in.limits.MaxStringBytes { in.fail("String limit"); return "" }; if !in.require(count) { return "" }
    value := string(in.bytes[in.offset:in.offset+count]); in.offset += count; if !utf8.ValidString(value) { in.fail("Invalid UTF-8") }; return value
  }
  in.expect('"'); start := in.offset; escaped := false
  for in.require(1) {
    c := in.bytes[in.offset]; in.offset++
)serializer"
R"serializer(    if in.offset - start - 1 > in.limits.MaxStringBytes { in.fail("String limit"); return "" }
    if !escaped && c == '"' {
      raw := in.bytes[start-1:in.offset]; var value string
      if !srlValidJSONText(raw) || json.Unmarshal(raw, &value) != nil { in.fail("Invalid JSON string"); return "" }
      if len(value) > in.limits.MaxStringBytes { in.fail("String limit") }; return value
    }
    if !escaped && c == '\\' { escaped = true } else { escaped = false }
  }
  return ""
}
// beginObject bounds recursion before descending.
func (in *srlReader) beginObject() { in.depth++; if in.depth > in.limits.MaxDepth { in.fail("Depth limit"); return }; if in.json() { in.expect('{') } }
// nextObject reads JSON framing while preserving duplicate keys.
func (in *srlReader) nextObject(first bool) bool {
  in.whitespace(); if !in.require(1) { return false }; if in.bytes[in.offset] == '}' { in.offset++; return false }; if !first { in.expect(',') }; return in.err == nil
}
// key consumes a decoded name and JSON separator.
func (in *srlReader) key() string { value := in.text(); if in.json() { in.expect(':') }; return value }
// endObject leaves one object scope.
func (in *srlReader) endObject() { in.depth-- }
// beginArray checks declared counts without preallocation.
func (in *srlReader) beginArray() int {
  if in.json() { in.expect('['); return -1 }; count := in.compact(); if count > in.limits.MaxElements - in.elements { in.fail("Element limit") }; return count
}
// arrayCapacity validates fixed-width payload extents before reserving binary array storage.
func (in *srlReader) arrayCapacity(count, width int) int {
  if in.err != nil || in.json() { return 0 }
  if width != 0 && count > (len(in.bytes) - in.offset) / width { in.fail("Truncated array"); return 0 }
  return count
}
// nextElement charges every entry and terminates immediately on failure.
func (in *srlReader) nextElement(index, count int) bool {
  if in.err != nil { return false }
  if in.json() { in.whitespace(); if !in.require(1) { return false }; if in.bytes[in.offset] == ']' { in.offset++; return false }; if index != 0 { in.expect(',') } } else if index == count { return false }
  if in.elements >= in.limits.MaxElements { in.fail("Element limit"); return false }; in.elements++; return in.err == nil
}
// finish checks exact consumption of one message.
func (in *srlReader) finish() { if in.json() { in.whitespace() }; if in.offset != len(in.bytes) { in.fail("Trailing input") } }
// srlWriteEnum writes names only in the contexts required by the wire contract.
func srlWriteEnum(out *srlWriter, value int, names []string, collection bool) {
  if value < 0 || value >= len(names) { out.fail("Unknown enum"); return }
  if out.json() || (!collection && out.protocol == BINARY_STRING) { out.text(names[value]) } else { out.compact(value) }
}
// srlReadEnum rejects unknown names and ordinals.
func srlReadEnum(in *srlReader, names []string, collection bool) int {
  if in.json() || (!collection && in.protocol == BINARY_STRING) { text := in.text(); for i, name := range names { if text == name { return i } }; in.fail("Unknown enum"); return 0 }
  value := in.compact(); if value >= len(names) { in.fail("Unknown enum"); return 0 }; return value
}
// Keep the standard map-sort and endian helpers available for every schema shape.
var _ = sort.Slice
)serializer"
