R"serializer(
/** Native protocols. Binary scalars are little-endian. */
export const Protocol = Object.freeze({JSON: 0, BINARY_NONE: 1, BINARY_INTEGER: 2, BINARY_STRING: 3});
/** Per-message policy; entries are counted cumulatively. */
export class Limits {
  constructor(maxBytes = 64 * 1024 * 1024, maxStringBytes = 16 * 1024 * 1024, maxElements = 1000000, maxDepth = 64) {
    for (const value of [maxBytes, maxStringBytes, maxElements, maxDepth]) {
      if (!Number.isSafeInteger(value) || value < 0) throw new RangeError('Invalid decode limit');
    }
    Object.assign(this, {maxBytes, maxStringBytes, maxElements, maxDepth});
    Object.freeze(this);
  }
}
const srlEncoder = new TextEncoder();
const srlDecoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});
/** Validate UTF-16 before encoding; TextEncoder otherwise replaces malformed input. */
function srlUtf8(value) {
  if (typeof value !== 'string') throw new TypeError('Expected string');
  for (let i = 0; i < value.length; ++i) {
    const c = value.charCodeAt(i);
    if (c >= 0xd800 && c <= 0xdbff) {
      const next = value.charCodeAt(++i);
      if (!(next >= 0xdc00 && next <= 0xdfff)) throw new TypeError('Invalid UTF-16');
    } else if (c >= 0xdc00 && c <= 0xdfff) throw new TypeError('Invalid UTF-16');
  }
  return srlEncoder.encode(value);
}
/** Compare text by unsigned UTF-8 bytes for native map ordering. */
function srlCompare(a, b) {
  if (typeof a === 'string') {
    const x = srlUtf8(a), y = srlUtf8(b);
    for (let i = 0; i < Math.min(x.length, y.length); ++i) if (x[i] !== y[i]) return x[i] - y[i];
    return x.length - y.length;
  }
  return a < b ? -1 : a > b ? 1 : 0;
}
/** An isolated growing byte writer; no caller-owned storage is retained. */
class SrlWriter {
  constructor(protocol) {
    if (!Number.isInteger(protocol) || protocol < 0 || protocol > 3) throw new RangeError('Unknown protocol');
    this.protocol = protocol; this.buffer = new Uint8Array(256); this.view = new DataView(this.buffer.buffer); this.offset = 0; this.depth = 0;
  }
  // Report invalid values before a result is exposed.
  fail(message) { throw new RangeError(message); }
  // Test the active wire protocol.
  json() { return this.protocol === Protocol.JSON; }
  // Reserve storage before writing a bounded sequence.
  reserve(count) {
    if (!Number.isSafeInteger(count) || count < 0 || this.offset + count > 0x7fffffff) throw new RangeError('Output too large');
    if (this.offset + count > this.buffer.length) {
      const next = new Uint8Array(Math.max(this.offset + count, Math.min(0x7fffffff, this.buffer.length * 2)));
      next.set(this.buffer); this.buffer = next; this.view = new DataView(next.buffer);
    }
  }
  // Append a byte sequence.
  put(bytes) { this.reserve(bytes.length); this.buffer.set(bytes, this.offset); this.offset += bytes.length; }
  // Append numeric text or punctuation.
  raw(text) {
    if (text.length === 1 && text.charCodeAt(0) < 128) { this.reserve(1); this.buffer[this.offset++] = text.charCodeAt(0); return; }
    this.reserve(text.length * 3);
    this.offset += srlEncoder.encodeInto(text, this.buffer.subarray(this.offset)).written;
  }
  // Return an independent exact-size message.
  bytes() { return this.buffer.slice(0, this.offset); }
  // Encode Serializer's two-bit length tag, not a Protobuf varint.
  compact(value) {
    if (!Number.isInteger(value) || value < 0 || value > 0x3fffffff) throw new RangeError('Compact integer out of range');
    const extra = value <= 0x3f ? 0 : value <= 0x3fff ? 1 : value <= 0x3fffff ? 2 : 3;
    this.reserve(extra + 1); this.buffer[this.offset++] = (extra << 6) | (value >>> (extra * 8));
    for (let shift = (extra - 1) * 8; shift >= 0; shift -= 8) this.buffer[this.offset++] = value >>> shift;
  }
  // Range-check integers before preserving their declared-width bit patterns.
  integer(value, width, unsigned) {
    if (width === 8) {
      if (typeof value !== 'bigint') throw new TypeError('Expected bigint');
      const min = unsigned ? 0n : -(1n << 63n), max = unsigned ? (1n << 64n) - 1n : (1n << 63n) - 1n;
      if (value < min || value > max) throw new RangeError('Integer out of range');
      if (this.json()) { this.raw(value.toString()); return; }
      this.reserve(width);
      this.view.setBigUint64(this.offset, BigInt.asUintN(64, value), true);
    } else {
      const bits = width * 8, min = unsigned ? 0 : -(2 ** (bits - 1)), max = unsigned ? 2 ** bits - 1 : 2 ** (bits - 1) - 1;
      if (!Number.isInteger(value) || value < min || value > max) throw new RangeError('Integer out of range');
      if (this.json()) { this.raw(String(value)); return; }
      this.reserve(width);
      const view = this.view;
      if (width === 1) view.setUint8(this.offset, value);
      else if (width === 2) view.setUint16(this.offset, value, true);
      else view.setUint32(this.offset, value, true);
    }
    this.offset += width;
  }
  // Round single precision once and preserve IEEE bits in binary output.
  floating(value, single) {
    if (typeof value !== 'number') throw new TypeError('Expected number');
    const rounded = single ? Math.fround(value) : value;
    if (this.json()) {
      if (!Number.isFinite(rounded)) throw new RangeError('Nonfinite JSON number');
      this.raw(Object.is(rounded, -0) ? '-0' : String(rounded)); return;
    }
    const width = single ? 4 : 8; this.reserve(width);
    const view = this.view;
    if (single) view.setFloat32(this.offset, rounded, true); else view.setFloat64(this.offset, rounded, true);
    this.offset += width;
  }
  // Write only canonical boolean values.
  bool(value) {
    if (typeof value !== 'boolean') throw new TypeError('Expected boolean');
    if (this.json()) this.raw(value ? 'true' : 'false'); else this.put(Uint8Array.of(value ? 1 : 0));
  }
  // Schema char is one byte; JSON supports ASCII only.
  character(value) {
    if (!Number.isInteger(value) || value < 0 || value > (this.json() ? 127 : 255)) throw new RangeError('Invalid char');
    if (this.json()) this.text(String.fromCharCode(value)); else this.put(Uint8Array.of(value));
  }
  // Validate strings before adding any payload bytes.
  text(value) {
    const bytes = srlUtf8(value);
    if (this.json()) this.raw(JSON.stringify(value)); else { this.compact(bytes.length); this.put(bytes); }
  }
  // Bound recursion before traversing object fields.
  beginObject() { if (++this.depth > 64) throw new RangeError('Encode depth limit'); if (this.json()) this.raw('{'); }
  // Encode a schema key in the selected protocol.
  field(id, key, first) {
    if (this.json()) { if (!first) this.raw(','); this.put(key.json); }
    else if (this.protocol === Protocol.BINARY_INTEGER) this.compact(id);
    else if (this.protocol === Protocol.BINARY_STRING) this.put(key.binary);
  }
  // Close the object with its native terminator.
  endObject() { if (this.json()) this.raw('}'); else if (this.protocol !== Protocol.BINARY_NONE) this.compact(0); --this.depth; }
  // Begin a collection without changing element representation.
  beginArray(count) { if (this.json()) this.raw('['); else this.compact(count); }
  // Separate JSON entries.
  element(index) { if (this.json() && index !== 0) this.raw(','); }
  // Close a JSON collection.
  endArray() { if (this.json()) this.raw(']'); }
}
/** Pre-encode schema field names once; arrays remain private to this generated module. */
function srlMakeField(name) {
  const binary = new SrlWriter(Protocol.BINARY_STRING); binary.text(name);
  return Object.freeze({binary: binary.bytes(), json: srlEncoder.encode(JSON.stringify(name) + ':')});
}
/** Streaming token reader preserves duplicate fields and exact integer tokens. */
class SrlReader {
  constructor(bytes, protocol, limits) {
    if (!(bytes instanceof Uint8Array) || !(limits instanceof Limits)) throw new TypeError('Expected Uint8Array and Limits');
    if (!Number.isInteger(protocol) || protocol < 0 || protocol > 3) throw new RangeError('Unknown protocol');
    this.bytes = bytes; this.view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength); this.protocol = protocol; this.limits = limits; this.offset = 0; this.depth = 0; this.elements = 0;
    if (bytes.length > limits.maxBytes) this.fail('Message byte limit');
  }
  // Report the failing byte position without disclosing the payload.
  fail(message) { throw new RangeError(`${message} at byte ${this.offset}`); }
  // Test the active protocol.
  json() { return this.protocol === Protocol.JSON; }
  // Check before reading or slicing input.
  require(count) { if (count < 0 || count > this.bytes.length - this.offset) this.fail('Truncated input'); }
  // Skip exactly JSON whitespace.
  whitespace() { while ([32, 9, 10, 13].includes(this.bytes[this.offset])) ++this.offset; }
  // Require one delimiter, rejecting trailing commas through subsequent value parsing.
  expect(value) { this.whitespace(); this.require(1); if (this.bytes[this.offset++] !== value.charCodeAt(0)) this.fail('Unexpected delimiter'); }
  // Consume an intact compact prefix; tolerate legacy nonminimal encodings.
  compact() {
    this.require(1); const first = this.bytes[this.offset], extra = first >>> 6; this.require(extra + 1);
    let value = first & 63;
    for (let i = 1; i <= extra; ++i) value = value * 256 + this.bytes[this.offset + i];
    this.offset += extra + 1; return value;
  }
  // Read one unconverted JSON token, retaining all integer digits.
  token() {
    this.whitespace(); const start = this.offset;
    while (this.offset < this.bytes.length && ![44, 125, 93, 32, 9, 10, 13].includes(this.bytes[this.offset])) ++this.offset;
    if (start === this.offset) this.fail('Missing token');
    return srlDecoder.decode(this.bytes.subarray(start, this.offset));
  }
  // Decode an integer with explicit signedness and width.
  integer(width, unsigned) {
    if (!this.json()) {
      this.require(width);
      const view = this.view, offset = this.offset;
      this.offset += width;
      if (width === 8) return unsigned ? view.getBigUint64(offset, true) : view.getBigInt64(offset, true);
      if (width === 4) return unsigned ? view.getUint32(offset, true) : view.getInt32(offset, true);
      if (width === 2) return unsigned ? view.getUint16(offset, true) : view.getInt16(offset, true);
      return unsigned ? view.getUint8(offset) : view.getInt8(offset);
    }
    const text = this.token();
    if (!/^-?(0|[1-9][0-9]*)$/.test(text) || (unsigned && text.startsWith('-'))) this.fail('Invalid integer');
    if (width === 8) {
      // Reject oversized tokens before allocating arbitrary-precision integers.
      if (text.length > 20) this.fail('Integer out of range');
      const value = BigInt(text), min = unsigned ? 0n : -(1n << 63n), max = unsigned ? (1n << 64n) - 1n : (1n << 63n) - 1n;
      if (value < min || value > max) this.fail('Integer out of range'); return value;
    }
    const value = Number(text), bits = width * 8, min = unsigned ? 0 : -(2 ** (bits - 1)), max = unsigned ? 2 ** bits - 1 : 2 ** (bits - 1) - 1;
    if (!Number.isInteger(value) || value < min || value > max) this.fail('Integer out of range'); return value;
  }
  // Validate JSON number grammar, range, and underflow.
  floating(single) {
    if (!this.json()) {
      const width = single ? 4 : 8; this.require(width);
      const offset = this.offset; this.offset += width;
      return single ? this.view.getFloat32(offset, true) : this.view.getFloat64(offset, true);
    }
    const text = this.token(); if (!/^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$/.test(text)) this.fail('Invalid number');
    const value = single ? Math.fround(Number(text)) : Number(text);
    if (!Number.isFinite(value) || (value === 0 && /[1-9]/.test(text.split(/[eE]/)[0]))) this.fail('Floating-point range');
    return value;
  }
  // Reject noncanonical booleans.
  bool() {
    if (this.json()) { const text = this.token(); if (text === 'true') return true; if (text === 'false') return false; }
    else { this.require(1); const value = this.bytes[this.offset++]; if (value <= 1) return value === 1; }
    return this.fail('Invalid boolean');
  }
  // Decode a byte character or one ASCII JSON character.
  character() {
    if (!this.json()) return this.integer(1, true);
    const text = this.text(); if (text.length !== 1 || text.charCodeAt(0) > 127) this.fail('Invalid char'); return text.charCodeAt(0);
  }
  // Bound escaped and decoded UTF-8 string sizes before exposing a value.
  text() {
    if (!this.json()) {
      const count = this.compact(); if (count > this.limits.maxStringBytes) this.fail('String limit'); this.require(count);
      const value = srlDecoder.decode(this.bytes.subarray(this.offset, this.offset + count)); this.offset += count; return value;
    }
    this.expect('"'); const start = this.offset; let escaped = false;
    while (true) {
      this.require(1); const c = this.bytes[this.offset++];
      if (this.offset - start - 1 > this.limits.maxStringBytes) this.fail('String limit');
      if (!escaped && c === 34) break;
      if (!escaped && c === 92) escaped = true; else escaped = false;
    }
    const value = JSON.parse(srlDecoder.decode(this.bytes.subarray(start - 1, this.offset)));
    if (srlUtf8(value).length > this.limits.maxStringBytes) this.fail('String limit'); return value;
  }
  // Check nesting before entering a generated object.
  beginObject() { if (++this.depth > this.limits.maxDepth) this.fail('Depth limit'); if (this.json()) this.expect('{'); }
  // Read object framing without collapsing duplicate keys.
  nextObject(first) { this.whitespace(); this.require(1); if (this.bytes[this.offset] === 125) { ++this.offset; return false; } if (!first) this.expect(','); return true; }
  // Read a decoded wire name.
  key() { const value = this.text(); if (this.json()) this.expect(':'); return value; }
  // Leave a previously entered object.
  endObject() { --this.depth; }
  // Validate collection counts before allocation.
  beginArray() { if (this.json()) { this.expect('['); return -1; } const count = this.compact(); if (count > this.limits.maxElements - this.elements) this.fail('Element limit'); return count; }
  // Charge every collection slot, including empty objects.
  nextElement(index, count) {
    if (this.json()) { this.whitespace(); this.require(1); if (this.bytes[this.offset] === 93) { ++this.offset; return false; } if (index !== 0) this.expect(','); }
    else if (index === count) return false;
    if (++this.elements > this.limits.maxElements) this.fail('Element limit'); return true;
  }
  // Require exact consumption of one message.
  finish() { if (this.json()) this.whitespace(); if (this.offset !== this.bytes.length) this.fail('Trailing input'); }
}
/** Encode declared enum values with context-sensitive native representation. */
function srlWriteEnum(out, value, names, collection) {
  if (!Number.isInteger(value) || value < 0 || value >= names.length) throw new RangeError('Unknown enum');
  if (out.json() || (!collection && out.protocol === Protocol.BINARY_STRING)) out.text(names[value]); else out.compact(value);
}
/** Decode only declared enum names or ordinals. */
function srlReadEnum(input, names, collection) {
  const value = input.json() || (!collection && input.protocol === Protocol.BINARY_STRING) ? names.indexOf(input.text()) : input.compact();
  if (value < 0 || value >= names.length) input.fail('Unknown enum'); return value;
}
)serializer"
