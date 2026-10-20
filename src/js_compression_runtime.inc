// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compressjs(
/** Owning asynchronous compression; Node is loaded only by explicit nodeProvider(). */
export const SerializerCompression = (() => {
  // Capture Promise without reserving a previously legal schema type named Promise.
  const NativePromise = (async () => {})().constructor;
  const chunkBytes = 32768, defaultBytes = 64 * 1024 * 1024, deflateWindowBytes = 32768;
  const Format = Object.freeze({None: 'none', Gzip: 'gzip', Zlib: 'zlib', Deflate: 'deflate', Zstd: 'zstd', Lz4: 'lz4'});
  const Code = Object.freeze({Unavailable: 'unavailable', InvalidOptions: 'invalid_options', InvalidData: 'invalid_data', ResourceLimit: 'resource_limit'});
  /** Compression diagnostics contain no payload excerpts; application exceptions remain unchanged. */
  class CompressionError extends Error {
    constructor(code, message) { super(message); this.name = 'SerializerCompressionError'; this.code = code; }
  }
  // Reject malformed selectors and logical byte limits before callbacks or I/O.
  function formatValue(format) {
    if (!Object.values(Format).includes(format)) throw new CompressionError(Code.InvalidOptions, 'Unknown compression format');
    return format;
  }
  // Keep arithmetic within exact, supported owning byte-array extents.
  function limit(value) {
    if (!Number.isSafeInteger(value) || value < 0 || value > 0x7fffffff) throw new CompressionError(Code.InvalidOptions, 'Invalid compression byte limit');
    return value;
  }
  // Cancellation is synchronous; accepting a rejected Promise would hide the original failure.
  function check(policy) {
    if (!policy.checkCancelled) return;
    const result = policy.checkCancelled();
    if (result && typeof result.then === 'function') {
      NativePromise.resolve(result).catch(() => {});
      throw new CompressionError(Code.InvalidOptions, 'Cancellation callback must be synchronous');
    }
  }
  /** Immutable encoding options; provider is obtained explicitly from nodeProvider(). */
  class EncodeOptions {
    constructor(options = {}) {
      if (options === null || typeof options !== 'object') throw new CompressionError(Code.InvalidOptions, 'Invalid compression options');
      const {format = Format.None, maxInputBytes = defaultBytes, maxOutputBytes = defaultBytes, level = format === Format.Zstd ? 3 : format === Format.Lz4 ? 0 : 6, provider = null, checkCancelled = null} = options;
      formatValue(format); limit(maxInputBytes); limit(maxOutputBytes);
      if (!Number.isInteger(level) || level < (format === Format.Zstd ? zstdMinimumLevel : 0) || level > (format === Format.Zstd ? zstdMaximumLevel : 9) || checkCancelled !== null && typeof checkCancelled !== 'function') throw new CompressionError(Code.InvalidOptions, 'Invalid compression options');
      Object.assign(this, {format, maxInputBytes, maxOutputBytes, level, provider, checkCancelled}); Object.freeze(this);
    }
  }
  /** Immutable decoding options; compressed input, actual output and window limits are independent. */
  class DecodeOptions {
    constructor(options = {}) {
      if (options === null || typeof options !== 'object') throw new CompressionError(Code.InvalidOptions, 'Invalid compression options');
      const {format = Format.None, maxInputBytes = defaultBytes, maxOutputBytes = defaultBytes, maxWindowBytes = 8 * 1024 * 1024, provider = null, checkCancelled = null} = options;
      formatValue(format); limit(maxInputBytes); limit(maxOutputBytes); limit(maxWindowBytes);
      if (checkCancelled !== null && typeof checkCancelled !== 'function') throw new CompressionError(Code.InvalidOptions, 'Invalid cancellation callback');
      Object.assign(this, {format, maxInputBytes, maxOutputBytes, maxWindowBytes, provider, checkCancelled}); Object.freeze(this);
    }
  }
  let installedProvider = null, installedZstdProvider = null, nodeModule = null, loading = null;
  // Selected Zstandard 1.5 API interval; ordinary DEFLATE keeps its existing 0..9 policy.
  const zstdMinimumLevel = -131072, zstdMaximumLevel = 22, zstdMinimumWindowBytes = 1024;
  /** Report availability without loading dependencies, invoking cancellation or touching transports. */
  function available(format, provider = null) {
    formatValue(format);
    return format === Format.None || provider !== null && (format === Format.Zstd ? provider === installedZstdProvider : format === Format.Lz4 ? Lz4ProviderHandle.accepts(provider) : [Format.Gzip, Format.Zlib, Format.Deflate].includes(format) && provider === installedProvider);
  }
  // Complete provider and window admission before even cancellation/codec/transport callbacks.
  function admit(options, decode) {
    const policy = options === undefined ? decode ? new DecodeOptions() : new EncodeOptions() : options;
    if (!(policy instanceof (decode ? DecodeOptions : EncodeOptions))) throw new CompressionError(Code.InvalidOptions, 'Expected immutable compression options');
    if (!available(policy.format, policy.provider)) throw new CompressionError(Code.Unavailable, 'Selected compression provider is unavailable');
    if (!decode && policy.format === Format.Lz4 && policy.level !== 0) throw new CompressionError(Code.InvalidOptions, 'LZ4 block provider supports only fast level zero');
    if (decode && policy.format !== Format.None && policy.maxWindowBytes < (policy.format === Format.Zstd ? zstdMinimumWindowBytes : policy.format === Format.Lz4 ? lz4HistoryBytes : deflateWindowBytes)) throw new CompressionError(Code.ResourceLimit, 'Compression history window exceeds limit');
    check(policy); return policy;
  }
  /** Lazily load the maintained optional Node provider; importing the generated module does not call this. */
  async function nodeProvider() {
    if (installedProvider) return installedProvider;
    if (!loading) loading = import('node:zlib').then(module => {
      if (typeof module.createDeflateRaw !== 'function' || typeof module.createInflateRaw !== 'function') throw new CompressionError(Code.Unavailable, 'Node DEFLATE provider is unavailable');
      nodeModule = module; installedProvider = Object.freeze({name: 'node:zlib'}); return installedProvider;
    }, () => { throw new CompressionError(Code.Unavailable, 'Node DEFLATE provider is unavailable'); });
    return loading;
  }
  /** Explicitly select Node's optional Zstandard transforms; older Node providers remain usable. */
  async function zstdProvider() {
    if (installedZstdProvider) return installedZstdProvider;
    await nodeProvider();
    if (typeof nodeModule.createZstdCompress !== 'function' || typeof nodeModule.createZstdDecompress !== 'function' ||
        !Number.isInteger(nodeModule.constants.ZSTD_c_compressionLevel) || !Number.isInteger(nodeModule.constants.ZSTD_c_checksumFlag) || !Number.isInteger(nodeModule.constants.ZSTD_d_windowLogMax)) {
      throw new CompressionError(Code.Unavailable, 'Node Zstandard provider is unavailable');
    }
    if (!installedZstdProvider) installedZstdProvider = Object.freeze({name: 'node:zlib:zstd'});
    return installedZstdProvider;
  }
  // Copy before the first asynchronous yield and poll through bounded memory work.
  function copy(input, maximum, policy) {
    if (!(input instanceof Uint8Array)) throw new CompressionError(Code.InvalidOptions, 'Expected owning byte input');
    if (input.length > maximum) throw new CompressionError(Code.ResourceLimit, 'Compression byte limit exceeded');
    const result = new Uint8Array(input.length);
    for (let offset = 0; offset < input.length; offset += chunkBytes) {
      check(policy); result.set(input.subarray(offset, offset + chunkBytes), offset);
    }
    check(policy); return result;
  }
  // Assemble only already admitted independent chunks into exact-size owning storage.
  function join(chunks, size, policy) {
    const result = new Uint8Array(size); let offset = 0;
    for (const chunk of chunks) { check(policy); result.set(chunk, offset); offset += chunk.length; }
    check(policy); return result;
  }
  // RFC 1952 CRC-32 table is private and immutable by the generated public API.
  const crcTable = Array.from({length: 256}, (_, value) => {
    for (let bit = 0; bit < 8; ++bit) value = value & 1 ? 0xedb88320 ^ value >>> 1 : value >>> 1;
    return value >>> 0;
  });
  // Compute checksums with bounded cancellation intervals over admitted bytes.
  function crc(data, start, count, policy) {
    let value = 0xffffffff;
    for (let index = 0; index < count; ++index) {
      if ((index & (chunkBytes - 1)) === 0) check(policy);
      value = crcTable[(value ^ data[start + index]) & 255] ^ value >>> 8;
    }
    check(policy); return (value ^ 0xffffffff) >>> 0;
  }
  // Adler-32 reduces within bounded blocks to retain exact integer arithmetic.
  function adler(data, policy) {
    let first = 1, second = 0;
    for (let start = 0; start < data.length; start += 5552) {
      check(policy);
      for (let index = start; index < Math.min(start + 5552, data.length); ++index) { first += data[index]; second += first; }
      first %= 65521; second %= 65521;
    }
    check(policy); return ((second << 16) | first) >>> 0;
  }
  // Every wrapper span is checked before indexing or subtracting its length.
  function requireSpan(data, position, count) {
    if (position < 0 || count < 0 || position > data.length || count > data.length - position) throw new CompressionError(Code.InvalidData, 'Truncated compression framing');
  }
  // Parse optional gzip fields and FHCRC without interpreting opaque header text.
  function gzipStart(data, policy) {
    requireSpan(data, 0, 10);
    if (data[0] !== 31 || data[1] !== 139 || data[2] !== 8 || (data[3] & 224) !== 0) throw new CompressionError(Code.InvalidData, 'Invalid gzip header');
    const flags = data[3]; let position = 10;
    if (flags & 4) { requireSpan(data, position, 2); const size = data[position] | data[position + 1] << 8; position += 2; requireSpan(data, position, size); position += size; }
    for (const flag of [8, 16]) if (flags & flag) {
      do { if ((position & (chunkBytes - 1)) === 0) check(policy); requireSpan(data, position, 1); } while (data[position++] !== 0);
    }
    if (flags & 2) {
      requireSpan(data, position, 2);
      if ((crc(data, 0, position, policy) & 65535) !== (data[position] | data[position + 1] << 8)) throw new CompressionError(Code.InvalidData, 'Invalid gzip header checksum');
      position += 2;
    }
    check(policy); return position;
  }
)compressjs"
R"compressjs(
  // Read little-bit-endian RFC fields from bounded memory, never per-byte transport calls.
  class Bits {
    constructor(data, position, policy) { this.data = data; this.position = position; this.bits = 0; this.value = 0; this.work = 0; this.policy = policy; }
    read(count) {
      if ((++this.work & 4095) === 0) check(this.policy);
      while (this.bits < count) {
        if (this.position >= this.data.length) throw new CompressionError(Code.InvalidData, 'Truncated DEFLATE data');
        this.value |= this.data[this.position++] << this.bits; this.bits += 8;
      }
      const result = this.value & ((1 << count) - 1); this.value >>>= count; this.bits -= count; return result;
    }
    // Stored blocks start on the next byte boundary.
    align() { this.bits = 0; this.value = 0; }
    // Skip admitted stored bytes without allocating output/history.
    skip(count) { requireSpan(this.data, this.position, count); this.position += count; }
  }
  // Canonical Huffman tables stay small and never allocate from attacker-supplied counts.
  class Huffman {
    constructor() { this.counts = Array(16).fill(0); this.first = Array(16).fill(0); this.offsets = Array(16).fill(0); this.symbols = Array(288).fill(0); this.maximum = 0; }
    build(lengths, start, count, codeLengths = false, allowEmpty = false) {
      this.counts.fill(0); this.maximum = 0; let used = 0;
      for (let index = 0; index < count; ++index) {
        const length = lengths[start + index];
        if (length < 0 || length > 15) throw new CompressionError(Code.InvalidData, 'Invalid DEFLATE code length');
        if (length) { ++this.counts[length]; ++used; this.maximum = Math.max(this.maximum, length); }
      }
      if (!used) { if (allowEmpty) return; throw new CompressionError(Code.InvalidData, 'Empty DEFLATE Huffman tree'); }
      let left = 1;
      for (let length = 1; length <= 15; ++length) {
        left = (left << 1) - this.counts[length];
        if (left < 0) throw new CompressionError(Code.InvalidData, 'Oversubscribed DEFLATE Huffman tree');
      }
      if (left && (codeLengths || used !== 1 || this.maximum !== 1)) throw new CompressionError(Code.InvalidData, 'Incomplete DEFLATE Huffman tree');
      let code = 0, offset = 0;
      for (let length = 1; length <= 15; ++length) {
        code = (code + this.counts[length - 1]) << 1; this.first[length] = code; this.offsets[length] = offset; offset += this.counts[length];
      }
      const next = this.offsets.slice();
      for (let symbol = 0; symbol < count; ++symbol) if (lengths[start + symbol]) this.symbols[next[lengths[start + symbol]]++] = symbol;
    }
    // Canonical code bits arrive in most-significant-code-bit order.
    decode(bits) {
      let code = 0;
      for (let length = 1; length <= this.maximum; ++length) {
        code = code << 1 | bits.read(1); const index = code - this.first[length];
        if (index >= 0 && index < this.counts[length]) return this.symbols[this.offsets[length] + index];
      }
      throw new CompressionError(Code.InvalidData, 'Invalid DEFLATE Huffman code');
    }
  }
  const lengthBase = [3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258];
  const lengthExtra = [0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0];
  const distanceBase = [1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577];
  const distanceExtra = [0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13];
  const codeOrder = [16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15];
  // Fixed alphabets are immutable private tables shared across blocks.
  function fixed(distance) {
    const lengths = Array.from({length: distance ? 32 : 288}, (_, symbol) => distance ? 5 : symbol < 144 ? 8 : symbol < 256 ? 9 : symbol < 280 ? 7 : 8);
    const result = new Huffman(); result.build(lengths, 0, lengths.length); return result;
  }
  const fixedLiteral = fixed(false), fixedDistance = fixed(true);
  // Derive actual output count, exact stream extent and history availability before output allocation.
  function extent(data, start, maximum, window, policy) {
    const bits = new Bits(data, start, policy), literal = new Huffman(), distance = new Huffman(), codes = new Huffman();
    const lengths = Array(320).fill(0), codeLengths = Array(19).fill(0); let output = 0, final;
    // Reject before arithmetic overflow or expanded allocation.
    function add(count) { if (count > maximum - output) throw new CompressionError(Code.ResourceLimit, 'Expanded output limit exceeded'); output += count; }
    do {
      check(policy); final = bits.read(1); const kind = bits.read(2);
      if (kind === 0) {
        bits.align(); const size = bits.read(16), complement = bits.read(16);
        if ((size ^ 65535) !== complement) throw new CompressionError(Code.InvalidData, 'Invalid DEFLATE stored length');
        bits.skip(size); add(size); continue;
      }
      if (kind === 3) throw new CompressionError(Code.InvalidData, 'Reserved DEFLATE block type');
      let selectedLiteral = fixedLiteral, selectedDistance = fixedDistance;
      if (kind === 2) {
        const literalCount = bits.read(5) + 257, distanceCount = bits.read(5) + 1, codeCount = bits.read(4) + 4;
        if (literalCount > 286) throw new CompressionError(Code.InvalidData, 'Invalid DEFLATE literal alphabet');
        codeLengths.fill(0);
        for (let index = 0; index < codeCount; ++index) codeLengths[codeOrder[index]] = bits.read(3);
        codes.build(codeLengths, 0, codeLengths.length, true);
        const total = literalCount + distanceCount; let position = 0;
        while (position < total) {
          const symbol = codes.decode(bits);
          if (symbol < 16) lengths[position++] = symbol;
          else {
            const count = symbol === 16 ? bits.read(2) + 3 : symbol === 17 ? bits.read(3) + 3 : bits.read(7) + 11;
            if (symbol === 16 && position === 0 || count > total - position) throw new CompressionError(Code.InvalidData, 'Invalid DEFLATE code-length repeat');
            const value = symbol === 16 ? lengths[position - 1] : 0;
            for (let index = 0; index < count; ++index) lengths[position++] = value;
          }
        }
        if (!lengths[256]) throw new CompressionError(Code.InvalidData, 'DEFLATE end-of-block code is absent');
        literal.build(lengths, 0, literalCount); distance.build(lengths, literalCount, distanceCount, false, true);
        selectedLiteral = literal; selectedDistance = distance;
      }
      while (true) {
        const symbol = selectedLiteral.decode(bits);
        if (symbol < 256) { add(1); continue; }
        if (symbol === 256) break;
        const lengthCode = symbol - 257;
        if (lengthCode < 0 || lengthCode >= lengthBase.length) throw new CompressionError(Code.InvalidData, 'Reserved DEFLATE length code');
        const count = lengthBase[lengthCode] + bits.read(lengthExtra[lengthCode]), distanceCode = selectedDistance.decode(bits);
        if (distanceCode < 0 || distanceCode >= distanceBase.length) throw new CompressionError(Code.InvalidData, 'Reserved DEFLATE distance code');
        const back = distanceBase[distanceCode] + bits.read(distanceExtra[distanceCode]);
        if (back > output || back > window) throw new CompressionError(Code.InvalidData, 'DEFLATE distance exceeds available history');
        add(count);
      }
    } while (!final);
    check(policy); return {end: bits.position, size: output};
  }
)compressjs"
R"compressjs(
  // Read unsigned frame fields without narrowing overflowing uint64 sizes into safe JS integers.
  function zstdLittle(data, offset, count) {
    requireSpan(data, offset, count); let value = 0;
    for (let index = count; index > 0; --index) value = value * 256 + data[offset + index - 1];
    return Number.isSafeInteger(value) ? value : Infinity;
  }
  // Admit exactly one standard frame, dictionary absence, content size and history before expansion.
  function zstdFrame(data, policy) {
    if (zstdLittle(data, 0, 4) !== 0xfd2fb528) throw new CompressionError(Code.InvalidData, 'Expected one standard Zstandard frame');
    requireSpan(data, 4, 1); const descriptor = data[4], single = (descriptor & 32) !== 0;
    if (descriptor & 8) throw new CompressionError(Code.InvalidData, 'Reserved Zstandard frame descriptor');
    let offset = 5, history = 0;
    if (!single) {
      requireSpan(data, offset, 1); const window = data[offset++], base = 2 ** (10 + (window >>> 3));
      history = base + (base / 8) * (window & 7);
    }
    const dictionaryBytes = (descriptor & 3) === 3 ? 4 : descriptor & 3;
    if (zstdLittle(data, offset, dictionaryBytes) !== 0) throw new CompressionError(Code.InvalidData, 'Zstandard dictionaries are unsupported');
    offset += dictionaryBytes;
    const sizeFlag = descriptor >>> 6, sizeBytes = sizeFlag === 0 ? single ? 1 : 0 : 1 << sizeFlag;
    let contentSize = sizeBytes ? zstdLittle(data, offset, sizeBytes) : null;
    if (sizeBytes === 2) contentSize += 256;
    if (contentSize !== null && contentSize > policy.maxOutputBytes) throw new CompressionError(Code.ResourceLimit, 'Expanded output limit exceeded');
    offset += sizeBytes; if (single) history = contentSize;
    const effectiveWindow = 2 ** Math.floor(Math.log2(policy.maxWindowBytes));
    if (history > effectiveWindow) throw new CompressionError(Code.ResourceLimit, 'Compression history window exceeds limit');
    const maximumBlock = Math.min(128 * 1024, history); let last;
    do {
      check(policy); const header = zstdLittle(data, offset, 3); offset += 3;
      last = (header & 1) !== 0; const type = (header >>> 1) & 3, count = header >>> 3;
      if (type === 3 || count > maximumBlock) throw new CompressionError(Code.InvalidData, 'Invalid Zstandard block');
      const stored = type === 1 ? 1 : count; requireSpan(data, offset, stored); offset += stored;
    } while (!last);
    if (descriptor & 4) { requireSpan(data, offset, 4); offset += 4; }
    if (offset !== data.length) throw new CompressionError(Code.InvalidData, 'Trailing or concatenated Zstandard input');
    check(policy); return contentSize;
  }
  // Map only documented native malformed/window errors, preserving unrelated provider failures.
  function zstdFailure(cause) {
    if (cause && cause.code === 'ZSTD_error_frameParameter_windowTooLarge') return new CompressionError(Code.ResourceLimit, 'Compression history window exceeds limit');
    if (cause && ['ZSTD_error_prefix_unknown', 'ZSTD_error_version_unsupported', 'ZSTD_error_frameParameter_unsupported',
      'ZSTD_error_corruption_detected', 'ZSTD_error_checksum_wrong', 'ZSTD_error_literals_headerWrong',
      'ZSTD_error_dictionary_corrupted', 'ZSTD_error_dictionary_wrong', 'ZSTD_error_srcSize_wrong',
      'ZSTD_error_tableLog_tooLarge', 'ZSTD_error_maxSymbolValue_tooLarge', 'ZSTD_error_maxSymbolValue_tooSmall'].includes(cause.code)) {
      return new CompressionError(Code.InvalidData, 'Invalid Zstandard frame');
    }
    return cause;
  }
)compressjs"
R"compressjs(
  const lz4HistoryBytes = 64 * 1024, lz4OutputBlockBytes = 64 * 1024, lz4MatchTableEntries = 1 << 16;
  // Own only immutable callable handles; private brands reject forged or inherited provider tokens.
  class Lz4ProviderHandle {
    #compress; #decompress;
    // Resolve only synchronous block calls before this handle can enter an application policy.
    constructor(module) {
      if (module === null || typeof module !== 'object' || typeof module.compressBlock !== 'function' || typeof module.decompressBlock !== 'function') {
        throw new CompressionError(Code.Unavailable, 'LZ4 block provider is unavailable');
      }
      this.#compress = module.compressBlock.bind(module); this.#decompress = module.decompressBlock.bind(module);
      this.name = 'lz4js'; Object.freeze(this);
    }
    // Inspect the private brand without invoking a caller-supplied object method or getter.
    static accepts(value) { return value !== null && typeof value === 'object' && #compress in value; }
    // Encode one independently owned bounded block with an operation-private match table.
    encode(input, output, table) { return this.#compress(input, output, 0, input.length, table); }
    // Decode only independently validated lengths/offsets into already admitted prefix storage.
    decode(input, offset, length, output, begin) { return this.#decompress(input, output, offset, length, begin); }
  }
  Object.freeze(Lz4ProviderHandle.prototype); Object.freeze(Lz4ProviderHandle);
  /** Select caller-owned lz4js block exports explicitly; generation/import installs no dependency. */
  function lz4Provider(module) { return new Lz4ProviderHandle(module); }
  // Compute the standard seed-zero XXHash32 with fixed lanes and cooperative bounded work.
  function lz4Hash(input, start, length, policy) {
    requireSpan(input, start, length);
    const prime1 = 0x9e3779b1, prime2 = 0x85ebca77, prime3 = 0xc2b2ae3d, prime4 = 0x27d4eb2f, prime5 = 0x165667b1;
    const rotate = (value, count) => value << count | value >>> (32 - count);
    const lane = (value, word) => Math.imul(rotate(value + Math.imul(word, prime2), 13), prime1);
    let offset = start, hash;
    const end = start + length;
    if (length >= 16) {
      let first = (prime1 + prime2) >>> 0, second = prime2, third = 0, fourth = (-prime1) >>> 0;
      while (end - offset >= 16) {
        if ((offset - start) % chunkBytes === 0) check(policy);
        first = lane(first, zstdLittle(input, offset, 4)); second = lane(second, zstdLittle(input, offset + 4, 4));
        third = lane(third, zstdLittle(input, offset + 8, 4)); fourth = lane(fourth, zstdLittle(input, offset + 12, 4));
        offset += 16;
      }
      hash = (rotate(first, 1) + rotate(second, 7) + rotate(third, 12) + rotate(fourth, 18) + length) >>> 0;
    } else hash = (prime5 + length) >>> 0;
    while (end - offset >= 4) { hash = Math.imul(rotate(hash + Math.imul(zstdLittle(input, offset, 4), prime3), 17), prime4); offset += 4; }
    while (offset < end) { hash = Math.imul(rotate(hash + Math.imul(input[offset++], prime5), 11), prime1); }
    hash = Math.imul(hash ^ hash >>> 15, prime2); hash = Math.imul(hash ^ hash >>> 13, prime3);
    check(policy); return (hash ^ hash >>> 16) >>> 0;
  }
  // Own admitted frame chunks until one exact final byte array is ready for publication.
  class Lz4Output {
    // Retain one operation's immutable policy without allocating untrusted-size storage.
    constructor(maximum, policy) { this.maximum = maximum; this.policy = policy; this.chunks = []; this.size = 0; }
    // Copy reused provider buffers before another block can mutate them.
    append(bytes) {
      if (bytes.length > this.maximum - this.size) throw new CompressionError(Code.ResourceLimit, 'Compression output limit exceeded');
      this.chunks.push(copy(bytes, this.maximum - this.size, this.policy)); this.size += bytes.length;
    }
    // Emit standard uint32 fields independently of host object layout.
    word(value) { const bytes = new Uint8Array(4); new DataView(bytes.buffer).setUint32(0, value, true); this.append(bytes); }
    // Join only admitted private chunks and observe final cancellation before returning.
    finish() { return join(this.chunks, this.size, this.policy); }
  }
  // Produce a checksummed standard frame with independent 64KiB blocks and explicit fast-mode policy.
  function lz4Compress(input, policy) {
    const output = new Lz4Output(policy.maxOutputBytes, policy);
    const header = Uint8Array.of(4, 34, 77, 24, 100, 64, 0);
    if (policy.maxOutputBytes < header.length + 8) throw new CompressionError(Code.ResourceLimit, 'LZ4 framing exceeds output limit');
    header[6] = lz4Hash(header, 4, 2, policy) >>> 8 & 255; output.append(header);
    const capacity = lz4OutputBlockBytes + Math.floor(lz4OutputBlockBytes / 255) + 16;
    const batch = new Uint8Array(capacity), table = Array(lz4MatchTableEntries).fill(0);
    for (let offset = 0; offset < input.length; offset += lz4OutputBlockBytes) {
      check(policy); const block = input.subarray(offset, offset + lz4OutputBlockBytes); table.fill(0);
      const produced = policy.provider.encode(block, batch, table);
      if (produced && typeof produced.then === 'function') NativePromise.resolve(produced).catch(() => {});
      if (!Number.isInteger(produced) || produced < 0 || produced > batch.length) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 provider output');
      check(policy);
      if (produced > 0 && produced < block.length) { output.word(produced); output.append(batch.subarray(0, produced)); }
      else { output.word((block.length | 0x80000000) >>> 0); output.append(block); }
    }
    output.word(0); output.word(lz4Hash(input, 0, input.length, policy)); return output.finish();
  }
  // Read extension lengths before indexing or adding attacker-controlled regenerated extents.
  function lz4Length(input, cursor, initial, end, maximum, policy) {
    let count = initial;
    if (initial === 15) {
      let extension;
      do {
        if (cursor.offset >= end) throw new CompressionError(Code.InvalidData, 'Truncated LZ4 length');
        if (cursor.offset % chunkBytes === 0) check(policy);
        extension = input[cursor.offset++];
        if (extension > maximum - count) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 block length');
        count += extension;
      } while (extension === 255);
    }
    return count;
  }
  // Count exact block output and prove every back-reference before invoking the optional decoder.
  function lz4BlockSize(input, offset, length, history, maximum, policy) {
    const cursor = {offset}, end = offset + length; let size = 0, nextPoll = offset;
    while (cursor.offset < end) {
      if (cursor.offset >= nextPoll) { check(policy); nextPoll = cursor.offset + chunkBytes; }
      const token = input[cursor.offset++], literals = lz4Length(input, cursor, token >>> 4, end, maximum, policy);
      if (literals > end - cursor.offset || literals > maximum - size) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 literals');
      cursor.offset += literals; size += literals; if (cursor.offset === end) return size;
      if (end - cursor.offset < 2) throw new CompressionError(Code.InvalidData, 'Truncated LZ4 match');
      const distance = zstdLittle(input, cursor.offset, 2); cursor.offset += 2;
      if (distance === 0 || distance > history + size) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 match distance');
      const matched = lz4Length(input, cursor, token & 15, end, maximum, policy);
      if (maximum - size < 4 || matched > maximum - size - 4) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 match length');
      size += matched + 4;
    }
    throw new CompressionError(Code.InvalidData, 'Missing LZ4 final literals');
  }
  // Retain only the final 64KiB from already validated preceding blocks.
  function lz4History(previous, input, offset, count, policy) {
    const size = Math.min(lz4HistoryBytes, previous.length + count), fromBlock = Math.min(size, count);
    const result = new Uint8Array(size);
    if (size > fromBlock) result.set(previous.subarray(previous.length - (size - fromBlock)));
    result.set(copy(input.subarray(offset + count - fromBlock, offset + count), fromBlock, policy), size - fromBlock);
    return result;
  }
  // Validate frame checksums, exact EOF, size/window admission and linked block history.
  function lz4Decompress(input, policy) {
    if (zstdLittle(input, 0, 4) !== 0x184d2204) throw new CompressionError(Code.InvalidData, 'Expected one standard LZ4 frame');
    requireSpan(input, 4, 2); const flags = input[4], descriptor = input[5], blockId = descriptor >>> 4 & 7;
    if (flags >>> 6 !== 1 || flags & 2 || descriptor & 143 || blockId < 4 || blockId > 7) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 descriptor');
    if (flags & 1) throw new CompressionError(Code.InvalidData, 'LZ4 dictionaries are unsupported');
    const maximum = 2 ** (8 + 2 * blockId); let offset = 6, expected = null;
    if (flags & 8) { expected = zstdLittle(input, offset, 8); offset += 8; if (expected > policy.maxOutputBytes) throw new CompressionError(Code.ResourceLimit, 'Expanded output limit exceeded'); }
    requireSpan(input, offset, 1);
    if (input[offset] !== (lz4Hash(input, 4, offset - 4, policy) >>> 8 & 255)) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 descriptor checksum');
    ++offset; if (maximum > policy.maxWindowBytes) throw new CompressionError(Code.ResourceLimit, 'Compression history window exceeds limit');
    const output = new Lz4Output(policy.maxOutputBytes, policy); let history = new Uint8Array();
    for (;;) {
      check(policy); const word = zstdLittle(input, offset, 4); offset += 4; if (word === 0) break;
      const raw = (word & 0x80000000) !== 0, count = word & 0x7fffffff;
      if (count > maximum) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 block size');
      requireSpan(input, offset, count);
      if (flags & 16 && zstdLittle(input, offset + count, 4) !== lz4Hash(input, offset, count, policy)) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 block checksum');
      let plain = input, begin = offset, size = count;
      if (!raw) {
        size = lz4BlockSize(input, offset, count, history.length, maximum, policy);
        if (size > policy.maxOutputBytes - output.size) throw new CompressionError(Code.ResourceLimit, 'Expanded output limit exceeded');
        plain = new Uint8Array(history.length + size); plain.set(history); begin = history.length;
        check(policy); const produced = policy.provider.decode(input, offset, count, plain, begin);
        if (produced && typeof produced.then === 'function') NativePromise.resolve(produced).catch(() => {});
        if (produced !== plain.length) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 regenerated block size');
        check(policy);
      }
      output.append(plain.subarray(begin, begin + size));
      if (!(flags & 32)) history = lz4History(history, plain, begin, size, policy);
      offset += count + (flags & 16 ? 4 : 0);
    }
    const result = output.finish();
    if (flags & 4) { const checksum = zstdLittle(input, offset, 4); offset += 4; if (checksum !== lz4Hash(result, 0, result.length, policy)) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 content checksum'); }
    if (offset !== input.length) throw new CompressionError(Code.InvalidData, 'Trailing or concatenated LZ4 input');
    if (expected !== null && expected !== result.length) throw new CompressionError(Code.InvalidData, 'Invalid LZ4 content size');
    check(policy); return result;
  }

)compressjs"
R"compressjs(
  // Feed bounded chunks through an owned Node transform and retain the first original failure.
  async function providerRun(input, maximum, policy, encode) {
    const zstd = policy.format === Format.Zstd;
    const stream = zstd ? encode ? nodeModule.createZstdCompress({chunkSize: chunkBytes, pledgedSrcSize: input.length,
      params: {[nodeModule.constants.ZSTD_c_compressionLevel]: policy.level, [nodeModule.constants.ZSTD_c_checksumFlag]: 1}}) :
      nodeModule.createZstdDecompress({chunkSize: chunkBytes, params: {[nodeModule.constants.ZSTD_d_windowLogMax]: Math.floor(Math.log2(policy.maxWindowBytes))}}) :
      encode ? nodeModule.createDeflateRaw({level: policy.level, windowBits: 15, chunkSize: chunkBytes}) : nodeModule.createInflateRaw({windowBits: 15, chunkSize: chunkBytes});
    const chunks = []; let size = 0, failure = null, failed = false, nativeFailure = false;
    // Preserve application cancellation objects instead of replacing them with destroy/write errors.
    function fail(cause) { if (!failed) { failed = true; failure = cause; } stream.destroy(); }
    const ended = new NativePromise((resolve, reject) => {
      let finished = false;
      stream.once('end', () => { finished = true; resolve(); });
      stream.on('error', cause => { if (!failed) { failed = true; failure = cause; nativeFailure = true; } reject(failure); });
      stream.once('close', () => {
        if (!finished) {
          if (!failed) { failed = true; failure = new CompressionError(Code.InvalidData, 'Compression provider closed before completion'); }
          reject(failure);
        }
      });
    });
    // Writes may fail before the final await; install a rejection handler immediately.
    ended.catch(() => {});
    stream.on('data', chunk => {
      if (failed) return;
      try {
        check(policy);
        if (chunk.length > maximum - size) throw new CompressionError(Code.ResourceLimit, 'Compression output limit exceeded');
        chunks.push(new Uint8Array(chunk)); size += chunk.length; check(policy);
      } catch (cause) { fail(cause); }
    });
    try {
      for (let offset = 0; offset < input.length; offset += chunkBytes) {
        if (failed) throw failure; check(policy);
        // Native errors can close a transform without completing its pending write callback.
        await NativePromise.race([new NativePromise((resolve, reject) => stream.write(input.subarray(offset, offset + chunkBytes), cause => cause ? reject(cause) : resolve())), ended]);
        if (failed) throw failure; check(policy);
      }
      if (failed) throw failure; check(policy); stream.end(); await ended;
      if (failed) throw failure; check(policy); return join(chunks, size, policy);
    } catch (cause) {
      const first = failed ? failure : cause;
      if (nativeFailure && zstd) throw zstdFailure(first);
      if (nativeFailure && first && ['Z_DATA_ERROR', 'Z_BUF_ERROR', 'Z_NEED_DICT'].includes(first.code)) throw new CompressionError(Code.InvalidData, 'Invalid DEFLATE provider data');
      throw first;
    } finally { stream.destroy(); }
  }
  /** Return independent complete message bytes; copy mutable input before the first asynchronous yield. */
  async function compress(input, options) {
    const policy = admit(options, false), data = copy(input, policy.format === Format.None ? Math.min(policy.maxInputBytes, policy.maxOutputBytes) : policy.maxInputBytes, policy);
    if (policy.format === Format.None) {
      if (data.length > policy.maxOutputBytes) throw new CompressionError(Code.ResourceLimit, 'Compression output limit exceeded');
      check(policy); return data;
    }
    if (policy.format === Format.Lz4) return lz4Compress(data, policy);
    if (policy.format === Format.Zstd) return providerRun(data, policy.maxOutputBytes, policy, true);
    const overhead = policy.format === Format.Gzip ? 18 : policy.format === Format.Zlib ? 6 : 0;
    if (policy.maxOutputBytes < overhead) throw new CompressionError(Code.ResourceLimit, 'Compression framing exceeds output limit');
    const raw = await providerRun(data, policy.maxOutputBytes - overhead, policy, true);
    check(policy); const result = new Uint8Array(raw.length + overhead), view = new DataView(result.buffer);
    if (policy.format === Format.Gzip) {
      result.set([31,139,8,0,0,0,0,0,0,255]); result.set(raw, 10);
      view.setUint32(result.length - 8, crc(data, 0, data.length, policy), true); view.setUint32(result.length - 4, data.length, true);
    } else if (policy.format === Format.Zlib) {
      result.set([0x78,0x9c]); result.set(raw, 2); view.setUint32(result.length - 4, adler(data, policy));
    } else result.set(raw);
    check(policy); return result;
  }
  /** Validate one complete frame before allocating expanded bytes or calling a model decoder. */
  async function decompress(input, options) {
    const policy = admit(options, true), data = copy(input, policy.format === Format.None ? Math.min(policy.maxInputBytes, policy.maxOutputBytes) : policy.maxInputBytes, policy);
    if (policy.format === Format.None) {
      if (data.length > policy.maxOutputBytes) throw new CompressionError(Code.ResourceLimit, 'Expanded output limit exceeded');
      check(policy); return data;
    }
    if (policy.format === Format.Lz4) return lz4Decompress(data, policy);
    if (policy.format === Format.Zstd) {
      const expected = zstdFrame(data, policy), result = await providerRun(data, expected === null ? policy.maxOutputBytes : expected, policy, false);
      if (expected !== null && result.length !== expected) throw new CompressionError(Code.InvalidData, 'Unexpected Zstandard expanded byte count');
      check(policy); return result;
    }
    let start = 0, trailer = 0, window = deflateWindowBytes;
    if (policy.format === Format.Gzip) { start = gzipStart(data, policy); trailer = 8; }
    else if (policy.format === Format.Zlib) {
      requireSpan(data, 0, 2);
      if ((data[0] & 15) !== 8 || (data[0] >> 4) > 7 || ((data[0] << 8) | data[1]) % 31 !== 0 || (data[1] & 32)) throw new CompressionError(Code.InvalidData, 'Invalid or dictionary-dependent zlib header');
      window = 1 << ((data[0] >> 4) + 8); start = 2; trailer = 4;
    }
    const measured = extent(data, start, policy.maxOutputBytes, window, policy);
    requireSpan(data, measured.end, trailer);
    if (data.length - measured.end !== trailer) throw new CompressionError(Code.InvalidData, 'Trailing or concatenated compression data');
    const result = await providerRun(data.subarray(start, measured.end), measured.size, policy, false);
    if (result.length !== measured.size) throw new CompressionError(Code.InvalidData, 'Unexpected expanded byte count');
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
    if (policy.format === Format.Gzip) {
      if (view.getUint32(measured.end, true) !== crc(result, 0, result.length, policy) || view.getUint32(measured.end + 4, true) !== result.length) throw new CompressionError(Code.InvalidData, 'Invalid gzip trailer');
    } else if (policy.format === Format.Zlib && view.getUint32(measured.end) !== adler(result, policy)) throw new CompressionError(Code.InvalidData, 'Invalid zlib checksum');
    check(policy); return result;
  }
  /** Admit the provider before a native owning encoder; callback allocations have their own limits. */
  async function encode(encoder, options) {
    const policy = admit(options, false);
    if (typeof encoder !== 'function') throw new CompressionError(Code.InvalidOptions, 'Expected encoder callback');
    const value = encoder();
    // Avoid yielding before copying a synchronous encoder's mutable owning result.
    return compress(value && typeof value.then === 'function' ? await value : value, policy);
  }
  /** Return a fresh candidate only after complete framing, native decoding and final cancellation. */
  async function decode(input, decoder, options) {
    const policy = admit(options, true);
    if (typeof decoder !== 'function') throw new CompressionError(Code.InvalidOptions, 'Expected decoder callback');
    const plain = await decompress(input, policy); check(policy);
    const candidate = await decoder(plain); check(policy); return candidate;
  }
  // Read to explicit borrowed EOF, including a bounded one-byte overflow probe.
  async function decompressFrom(source, options) {
    const policy = admit(options, true);
    if (!source || typeof source.read !== 'function') throw new CompressionError(Code.InvalidOptions, 'Expected borrowed byte source');
    const chunks = []; let size = 0;
    while (true) {
      check(policy); const requested = Math.min(chunkBytes, policy.maxInputBytes - size + 1);
      let chunk = source.read(requested);
      if (chunk && typeof chunk.then === 'function') chunk = await chunk;
      check(policy);
      if (chunk === null) break;
      if (!(chunk instanceof Uint8Array) || chunk.length === 0 || chunk.length > requested) throw new TypeError('Invalid borrowed source read result');
      if (chunk.length > policy.maxInputBytes - size) throw new CompressionError(Code.ResourceLimit, 'Compressed input limit exceeded');
      chunks.push(copy(chunk, requested, policy)); size += chunk.length;
    }
    check(policy); return decompress(join(chunks, size, policy), policy);
  }
)compressjs"
R"compressjs(
  // A first lifecycle failure fences pending and future operations without retrying borrowed callbacks.
  class Lifecycle {
    constructor() { this.failed = false; this.finished = false; this.closed = false; this.busy = false; this.failure = null; }
    begin() {
      if (this.failed || this.finished || this.closed || this.busy) {
        const cause = new Error('Compression wrapper is busy, finished, closed or failed');
        if (this.busy && !this.failed) { this.failure = cause; this.failed = true; }
        throw cause;
      }
      this.busy = true;
    }
    // An overlapping operation or close is observed after every external asynchronous boundary.
    active() { if (this.failed) throw this.failure; if (this.closed) throw new Error('Compression wrapper is closed'); }
    // Policy hooks are application callbacks and may close or reenter the same wrapper synchronously.
    poll() { this.active(); check(this.policy); this.active(); }
    fail(cause) { if (!this.failed) this.failure = cause; this.failed = true; return this.failure; }
    // Closing releases private storage only; no finish, flush or borrowed transport callback occurs.
    close() { this.closed = true; }
  }
  /** Staged borrowed output. Await write/finish sequentially; close never emits a trailer. */
  class Output extends Lifecycle {
    constructor(sink, options) {
      super(); this.policy = admit(options, false);
      if (!sink || typeof sink.write !== 'function') throw new CompressionError(Code.InvalidOptions, 'Expected borrowed byte sink');
      this.sink = sink; this.chunks = []; this.size = 0;
      this.runPolicy = new EncodeOptions({...this.policy, checkCancelled: () => this.poll()});
    }
    // Copy each admitted mutable byte span before yielding to any caller.
    async write(input) {
      this.begin();
      try {
        this.poll();
        const data = copy(input, this.policy.maxInputBytes - this.size, this.runPolicy);
        this.chunks.push(data); this.size += data.length; this.poll();
      } catch (cause) { throw this.fail(cause); } finally { this.busy = false; }
    }
    /** Publish bounded independent byte chunks only after the complete message is compressed. */
    async finish() {
      this.begin();
      try {
        this.active(); const wire = await compress(join(this.chunks, this.size, this.runPolicy), this.runPolicy); this.active();
        for (let offset = 0; offset < wire.length; offset += chunkBytes) {
          this.poll();
          await this.sink.write(wire.slice(offset, offset + chunkBytes));
          this.poll();
        }
        this.poll(); this.finished = true; this.chunks = []; this.size = 0;
      } catch (cause) { this.chunks = []; throw this.fail(cause); } finally { this.busy = false; }
    }
    // Flush performs policy/lifecycle checks and never propagates to the borrowed sink.
    async flush() { this.begin(); try { this.poll(); } catch (cause) { throw this.fail(cause); } finally { this.busy = false; } }
    close() { super.close(); this.chunks = []; this.size = 0; }
  }
  /** Validate borrowed source EOF before exposing expanded bytes; finish requires complete consumption. */
  class Input extends Lifecycle {
    constructor(source, options) {
      super(); this.policy = admit(options, true);
      if (!source || typeof source.read !== 'function') throw new CompressionError(Code.InvalidOptions, 'Expected borrowed byte source');
      this.source = source; this.data = null; this.position = 0;
      this.runPolicy = new DecodeOptions({...this.policy, checkCancelled: () => this.poll()});
    }
    // Retain and observe the original policy even after staging has completed.
    async prepare() {
      this.poll();
      if (this.data === null) this.data = await decompressFrom(this.source, this.runPolicy);
      this.poll();
    }
    /** Return independent bytes or null at expanded EOF; zero-size reads are not EOF probes. */
    async read(maxBytes = chunkBytes) {
      this.begin();
      try {
        if (!Number.isSafeInteger(maxBytes) || maxBytes <= 0 || maxBytes > 0x7fffffff) throw new CompressionError(Code.InvalidOptions, 'Invalid requested read size');
        await this.prepare(); const count = Math.min(maxBytes, this.data.length - this.position);
        if (count === 0) { this.poll(); return null; }
        const result = copy(this.data.subarray(this.position, this.position + count), count, this.runPolicy);
        this.poll(); this.position += count; return result;
      } catch (cause) { this.data = null; throw this.fail(cause); } finally { this.busy = false; }
    }
    /** Require exact expanded consumption; unread bytes are a permanent failure. */
    async finish() {
      this.begin();
      try {
        await this.prepare();
        if (this.position !== this.data.length) throw new CompressionError(Code.InvalidData, 'Unread expanded compression data');
        this.poll(); this.finished = true; this.data = null;
      } catch (cause) { this.data = null; throw this.fail(cause); } finally { this.busy = false; }
    }
    // Flush neither reads nor finishes the borrowed source.
    async flush() { this.begin(); try { this.poll(); } catch (cause) { throw this.fail(cause); } finally { this.busy = false; } }
    close() { super.close(); this.data = null; }
  }
  return Object.freeze({Format, Code, Error: CompressionError, EncodeOptions, DecodeOptions, available, nodeProvider, zstdProvider, lz4Provider, compress, decompress, encode, decode, decompressFrom, Output, Input});
})();
)compressjs"
