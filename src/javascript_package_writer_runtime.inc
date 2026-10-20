// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejs(
  // Write exact unsigned little-endian ZIP fields into an already admitted byte array.
  function put16(bytes, at, value) { bytes[at] = value & 255; bytes[at + 1] = value >>> 8 & 255; }
  // Avoid signed conversion when writing the high ZIP32 offset bit.
  function put32(bytes, at, value) { put16(bytes, at, value % 65536); put16(bytes, at + 2, Math.floor(value / 65536)); }
  // Split an exactly representable public ZIP64 extent into its wire words.
  function put64(bytes, at, value) { put32(bytes, at, value % 0x100000000); put32(bytes, at + 4, Math.floor(value / 0x100000000)); }
  // Join prepared chunks only after admitting the complete output extent.
  function joinBytes(chunks, count, state) {
    allocation(count); const data = new Uint8Array(count); let at = 0;
    for (const chunk of chunks) { for (let start = 0; start < chunk.length; start += chunkBytes) { state.poll(); const piece = chunk.subarray(start, start + chunkBytes); data.set(piece, at); at += piece.length; } }
    state.poll(); return data;
  }
  /** Deterministic staged writer; add copies caller bytes, finish publishes one complete archive. */
  class ZipWriter extends State {
    constructor(options = {}) {
      super(getBudget(options)); this.options = options; this.items = []; this.names = new Map(); this.releases = [];
      this.descriptor = options.descriptor === undefined ? 'signed' : options.descriptor;
      if (!['signed', 'unsigned', 'none'].includes(this.descriptor) || options.forceZip64 !== undefined && typeof options.forceZip64 !== 'boolean') throw new PackageError(Code.InvalidOptions, 'Invalid ZIP writer options');
    }
    // Snapshot mutable bytes before yielding; duplicates and provider admission precede callbacks.
    async add(input) {
      this.begin(); let release = null;
      try {
        if (!input || typeof input !== 'object') throw new PackageError(Code.InvalidOptions, 'Expected ZIP entry');
        const name = zipName(input.name), method = input.compression === undefined || input.compression === 'stored' ? 0 : input.compression === 'deflate' ? 8 : -1;
        admitCompression(method, this.options.compressionProvider);
        if (this.names.has(name)) throw new PackageError(Code.InvalidArchive, 'Duplicate physical ZIP entry', name);
        if (!(input.data instanceof Uint8Array)) throw new PackageError(Code.InvalidOptions, 'ZIP entry requires bytes', name);
        if (input.data.length > this.budget.limits.maxPartBytes) throw new PackageError(Code.ResourceLimit, 'Part extent exceeds limit', name);
        this.budget.consume('entries', 1, name); this.budget.consume('expandedBytes', input.data.length, name); this.budget.consume('workUnits', input.data.length, name);
        const rawName = srlUtf8(name); if (rawName.length > 65535) throw new PackageError(Code.InvalidOptions, 'ZIP filename is too long', name);
        this.budget.consume('metadataBytes', rawName.length + 76, name); release = this.budget.reserve(input.data.length, name);
        const data = copyBytes(input.data, this), item = {name, rawName, data, method, crc: crc32(data, this), wide: this.options.forceZip64 === true || input.forceZip64 === true, level: input.level === undefined ? 6 : input.level};
        if (!Number.isInteger(item.level) || item.level < 0 || item.level > 9) throw new PackageError(Code.InvalidOptions, 'Invalid ZIP compression level', name);
        this.poll(); this.items.push(item); this.names.set(name, item); this.releases.push(release); release = null;
      } catch (error) { throw this.fail(error); } finally { if (release) release(); this.end(); }
    }
    // Build canonical sorted headers, descriptors and exact ZIP32/ZIP64 directory framing.
    async prepare() {
      const chunks = [], centralItems = []; let size = 0;
      const append = chunk => { if (chunk.length > this.budget.limits.maxArchiveBytes - size) throw new PackageError(Code.ResourceLimit, 'Archive output extent exceeds limit'); allocation(size + chunk.length); chunks.push(chunk); size += chunk.length; };
      const ordered = this.items.slice().sort((a, b) => a.name < b.name ? -1 : a.name > b.name ? 1 : 0);
      for (const item of ordered) {
        this.poll(); let packed = item.data, packedRelease = null;
        if (item.method === 8) {
          const maximum = Math.min(arrayMax, this.budget.limits.maxArchiveBytes - size), scratch = item.data.length + Math.min(maximum, item.data.length + Math.ceil(item.data.length / 16384) * 5 + 64) * 2 + chunkBytes;
          packedRelease = this.budget.reserve(scratch, item.name);
          try { packed = await C.compress(item.data, new C.EncodeOptions({format: C.Format.Deflate, provider: this.options.compressionProvider, level: item.level, maxInputBytes: item.data.length, maxOutputBytes: maximum, checkCancelled: () => this.poll()})); this.poll(); }
          catch (error) { packedRelease(); throw error; }
        }
        const wide = item.wide || packed.length >= u32max || item.data.length >= u32max || size >= u32max;
        const descriptor = this.descriptor !== 'none', flags = 0x800 | (descriptor ? 8 : 0), start = size;
        const local = new Uint8Array(30 + item.rawName.length + (wide ? 20 : 0));
        put32(local, 0, signature.local); put16(local, 4, wide ? 45 : 20); put16(local, 6, flags); put16(local, 8, item.method); put16(local, 12, 33);
        put32(local, 14, descriptor ? 0 : item.crc); put32(local, 18, wide ? u32max : descriptor ? 0 : packed.length); put32(local, 22, wide ? u32max : descriptor ? 0 : item.data.length);
        put16(local, 26, item.rawName.length); put16(local, 28, wide ? 20 : 0); local.set(item.rawName, 30);
        if (wide) { const at = 30 + item.rawName.length; put16(local, at, 1); put16(local, at + 2, 16); put64(local, at + 4, item.data.length); put64(local, at + 12, descriptor ? 0 : packed.length); }
        append(local); append(packed);
        if (packedRelease) { this.releases.push(packedRelease); packedRelease = null; }
        if (descriptor) {
          const signed = this.descriptor === 'signed', width = wide ? 8 : 4, data = new Uint8Array((signed ? 4 : 0) + 4 + width * 2), at = signed ? 4 : 0;
          if (signed) put32(data, 0, signature.descriptor); put32(data, at, item.crc);
          if (wide) { put64(data, at + 4, packed.length); put64(data, at + 12, item.data.length); } else { put32(data, at + 4, packed.length); put32(data, at + 8, item.data.length); } append(data);
        }
        const central = new Uint8Array(46 + item.rawName.length + (wide ? 28 : 0));
        put32(central, 0, signature.central); put16(central, 4, 0x300 | (wide ? 45 : 20)); put16(central, 6, wide ? 45 : 20); put16(central, 8, flags); put16(central, 10, item.method); put16(central, 14, 33);
        put32(central, 16, item.crc); put32(central, 20, wide ? u32max : packed.length); put32(central, 24, wide ? u32max : item.data.length); put16(central, 28, item.rawName.length); put16(central, 30, wide ? 28 : 0); put32(central, 38, 0o100600 * 65536); put32(central, 42, wide ? u32max : start); central.set(item.rawName, 46);
        if (wide) { const at = 46 + item.rawName.length; put16(central, at, 1); put16(central, at + 2, 24); put64(central, at + 4, item.data.length); put64(central, at + 12, packed.length); put64(central, at + 20, start); }
        centralItems.push(central);
      }
      const centralStart = size; for (const central of centralItems) append(central); const centralSize = size - centralStart;
      const wide = this.options.forceZip64 === true || this.items.some(item => item.wide) || this.items.length >= 65535 || centralSize >= u32max || centralStart >= u32max;
      if (wide) {
        const wideStart = size, end = new Uint8Array(56), locator = new Uint8Array(20);
        put32(end, 0, signature.wideEnd); put64(end, 4, 44); put16(end, 12, 45); put16(end, 14, 45); put64(end, 24, this.items.length); put64(end, 32, this.items.length); put64(end, 40, centralSize); put64(end, 48, centralStart); append(end);
        put32(locator, 0, signature.locator); put64(locator, 8, wideStart); put32(locator, 16, 1); append(locator);
      }
      const end = new Uint8Array(22); put32(end, 0, signature.end); put16(end, 8, wide ? 65535 : this.items.length); put16(end, 10, wide ? 65535 : this.items.length); put32(end, 12, wide ? u32max : centralSize); put32(end, 16, wide ? u32max : centralStart); append(end);
      const release = this.budget.reserve(size); try { const output = joinBytes(chunks, size, this); this.poll(); return output; } finally { release(); }
    }
    // Publish owning bytes only after exact complete framing and final cancellation checks.
    async finish() {
      this.begin(); try { this.poll(); const data = await this.prepare(); this.poll(); this.finished = true; return data; }
      catch (error) { throw this.fail(error); } finally { for (const release of this.releases) release(); this.releases = []; this.items = []; this.names.clear(); this.end(); }
    }
    // Prepare fully before borrowing a sink; each independent chunk is consumed completely or throws.
    async writeTo(sink) {
      this.begin(); try {
        if (!sink || typeof sink.write !== 'function') throw new PackageError(Code.InvalidOptions, 'Expected borrowed byte sink');
        this.poll(); const data = await this.prepare(); this.poll();
        for (let at = 0; at < data.length; at += chunkBytes) { this.poll(); const chunk = data.slice(at, at + chunkBytes); this.budget.consume('ioBytes', chunk.length); let result = sink.write(chunk); if (result && typeof result.then === 'function') await result; this.poll(); }
        this.poll(); this.finished = true;
      } catch (error) { throw this.fail(error); } finally { for (const release of this.releases) release(); this.releases = []; this.items = []; this.names.clear(); this.end(); }
    }
    // Abandon prepared state without finishing or touching a borrowed sink.
    close() { super.close(); for (const release of this.releases) release(); this.releases = []; this.items = []; this.names.clear(); }
  }
)packagejs"
