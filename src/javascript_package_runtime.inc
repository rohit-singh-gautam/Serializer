// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejs(
/** Bounded ZIP/OPC support. Node providers and filesystem hosts are explicitly acquired. */
export const SerializerPackage = (() => {
  const C = SerializerCompression, NativePromise = (async () => {})().constructor;
  const chunkBytes = 32768, u32max = 0xffffffff, arrayMax = 0x7fffffff;
  const signature = Object.freeze({local: 0x04034b50, central: 0x02014b50, end: 0x06054b50, wideEnd: 0x06064b50, locator: 0x07064b50, descriptor: 0x08074b50});
  const Code = Object.freeze({InvalidOptions: 'invalid_options', InvalidArchive: 'invalid_archive', Integrity: 'integrity', Unsupported: 'unsupported_feature', Unavailable: 'unavailable', ResourceLimit: 'resource_limit', InvalidMetadata: 'invalid_metadata', InvalidPartName: 'invalid_part_name', InvalidRelationship: 'invalid_relationship', MissingPart: 'missing_part', ContentType: 'content_type_mismatch', Occurrence: 'occurrence', UnknownPart: 'unknown_part', UnknownRelationship: 'unknown_relationship', PublicationUncertain: 'publication_uncertain'});
  /** Diagnostics identify locations without including private payload excerpts. */
  class PackageError extends Error {
    constructor(code, message, part = '', relationshipId = '') { super(message); this.name = 'SerializerPackageError'; Object.assign(this, {code, part, relationshipId}); }
  }
  // Keep all public offsets, arithmetic and allocation limits exactly representable.
  function integer(value, label) {
    if (!Number.isSafeInteger(value) || value < 0) throw new PackageError(Code.InvalidOptions, 'Invalid ' + label);
    return value;
  }
  const defaults = Object.freeze({maxArchiveBytes: 256 * 1024 * 1024, maxIoBytes: 1024 * 1024 * 1024, maxMetadataBytes: 16 * 1024 * 1024, maxPartBytes: 64 * 1024 * 1024, maxTotalExpandedBytes: 256 * 1024 * 1024, maxBufferBytes: 128 * 1024 * 1024, maxEntries: 4096, maxRelationships: 16384, maxDecoderAllocationBytes: 128 * 1024 * 1024, maxWorkUnits: 512 * 1024 * 1024, maxReferences: 65536, maxPreservedBytes: 64 * 1024 * 1024});
  /** Immutable logical limits; cancellation and an optional explicit monotonic clock are synchronous. */
  class Limits {
    constructor(options = {}) {
      if (!options || typeof options !== 'object') throw new PackageError(Code.InvalidOptions, 'Invalid package limits');
      for (const key of Object.keys(defaults)) this[key] = integer(options[key] === undefined ? defaults[key] : options[key], key);
      this.checkCancelled = options.checkCancelled === undefined ? null : options.checkCancelled;
      this.clock = options.clock === undefined ? null : options.clock; this.deadline = options.deadline === undefined ? null : options.deadline;
      if (this.checkCancelled !== null && typeof this.checkCancelled !== 'function' || this.clock !== null && typeof this.clock !== 'function' || this.deadline !== null && (!Number.isFinite(this.deadline) || this.clock === null)) throw new PackageError(Code.InvalidOptions, 'Invalid cancellation or deadline policy');
      Object.freeze(this);
    }
  }
  const resourceLimits = Object.freeze({ioBytes: 'maxIoBytes', metadataBytes: 'maxMetadataBytes', expandedBytes: 'maxTotalExpandedBytes', entries: 'maxEntries', relationships: 'maxRelationships', decoderAllocationBytes: 'maxDecoderAllocationBytes', workUnits: 'maxWorkUnits', references: 'maxReferences', preservedBytes: 'maxPreservedBytes'});
  /** One cumulative operation budget; counters are never refunded, unlike live scratch reservations. */
  class Budget {
    constructor(limits = new Limits()) {
      if (!(limits instanceof Limits)) throw new PackageError(Code.InvalidOptions, 'Expected immutable package limits');
      this.limits = limits; this.used = Object.create(null); this.bufferBytes = 0;
      for (const key of Object.keys(resourceLimits)) this.used[key] = 0;
    }
    // Invoke application hooks without translating their exception identity.
    check() {
      if (this.limits.checkCancelled) {
        const result = this.limits.checkCancelled();
        if (result && typeof result.then === 'function') { NativePromise.resolve(result).catch(() => {}); throw new PackageError(Code.InvalidOptions, 'Cancellation callback must be synchronous'); }
      }
      if (this.limits.deadline !== null) {
        const now = this.limits.clock();
        if (now && typeof now.then === 'function') NativePromise.resolve(now).catch(() => {});
        if (!Number.isFinite(now)) throw new PackageError(Code.InvalidOptions, 'Clock must return a finite monotonic value');
        if (now >= this.limits.deadline) throw new PackageError(Code.ResourceLimit, 'Package deadline exceeded');
      }
    }
    // Charge before the corresponding work or allocation, with subtraction-safe arithmetic.
    consume(resource, count, part = '') {
      if (!Object.hasOwn(resourceLimits, resource)) throw new PackageError(Code.InvalidOptions, 'Unknown budget resource');
      integer(count, 'resource count'); const limit = this.limits[resourceLimits[resource]];
      if (count > limit - this.used[resource]) throw new PackageError(Code.ResourceLimit, 'Package ' + resource + ' limit exceeded', part);
      this.used[resource] += count;
    }
    // Return the remaining cumulative allowance without invoking application callbacks.
    remaining(resource) { if (!Object.hasOwn(resourceLimits, resource)) throw new PackageError(Code.InvalidOptions, 'Unknown budget resource'); return this.limits[resourceLimits[resource]] - this.used[resource]; }
    // Reserve simultaneous owning storage; the returned release closure is idempotent.
    reserve(count, part = '') {
      integer(count, 'buffer count'); if (count > this.limits.maxBufferBytes - this.bufferBytes) throw new PackageError(Code.ResourceLimit, 'Package live buffer limit exceeded', part);
      this.bufferBytes += count; let live = true;
      return () => { if (live) { live = false; this.bufferBytes -= count; } };
    }
  }
  // Share a caller budget only when no second contradictory limits object was supplied.
  function getBudget(options = {}) {
    if (!options || typeof options !== 'object') throw new PackageError(Code.InvalidOptions, 'Invalid package options');
    if (options.budget !== undefined) {
      if (!(options.budget instanceof Budget) || options.limits !== undefined) throw new PackageError(Code.InvalidOptions, 'Supply either limits or budget');
      return options.budget;
    }
    return new Budget(options.limits === undefined ? new Limits() : options.limits instanceof Limits ? options.limits : new Limits(options.limits));
  }
  /** Guard every external hook and await; the first failure fences all subsequent borrowed I/O. */
  class State {
    constructor(budget) { this.budget = budget; this.busy = false; this.closed = false; this.finished = false; this.failed = false; this.failure = undefined; this.holds = []; }
    // Preserve even falsy JavaScript thrown values as the exact first failure.
    fail(error) { if (!this.failed) { this.failed = true; this.failure = error; } return this.failure; }
    // Reject use after failure, closure or completion before any external callback.
    active() { if (this.failed) throw this.failure; if (this.closed || this.finished) throw new PackageError(Code.InvalidOptions, 'Package operation is closed or finished'); }
    // A cancellation callback may close or reenter the wrapper, so check on both sides.
    poll() { this.active(); this.budget.check(); this.active(); }
    // A nested operation poisons its outer operation even if the callback catches the nested error.
    begin() { this.active(); if (this.busy) throw this.fail(new PackageError(Code.InvalidOptions, 'Overlapping package operation')); this.busy = true; }
    // Release a borrowed operation slot without clearing a failure.
    end() { this.busy = false; }
    // Keep aggregate snapshots charged for their complete asynchronous staging lifetime.
    hold(count, part = '') { const release = this.budget.reserve(count, part); this.holds.push(release); }
    // Release only this operation's private staging reservations after completion or failure.
    releaseHolds() { for (const release of this.holds) release(); this.holds = []; }
    // Close fences synchronously; implementations release only their own private resources.
    close() { this.closed = true; }
  }
  // Require a bounded owning byte-array extent before allocation.
  function allocation(count, label = 'byte allocation') { integer(count, label); if (count > arrayMax) throw new PackageError(Code.ResourceLimit, 'Owning byte-array extent exceeds supported limit'); return count; }
  // Copy mutable input synchronously before the first asynchronous yield.
  function copyBytes(input, state) {
    if (!(input instanceof Uint8Array)) throw new PackageError(Code.InvalidOptions, 'Expected Uint8Array');
    allocation(input.length); const result = new Uint8Array(input.length);
    for (let offset = 0; offset < input.length; offset += chunkBytes) { state.poll(); result.set(input.subarray(offset, offset + chunkBytes), offset); }
    state.poll(); return result;
  }
  // Decode only a previously bounds-checked little-endian integer field.
  function u16(bytes, offset) { return bytes[offset] + bytes[offset + 1] * 256; }
  // Avoid signed bitwise conversion of the high ZIP32 offset bit.
  function u32(bytes, offset) { return (bytes[offset] | bytes[offset + 1] << 8 | bytes[offset + 2] << 16 | bytes[offset + 3] << 24) >>> 0; }
  // ZIP64 integers larger than JavaScript's exact offset range are rejected before any seek.
  function u64(bytes, offset) { const value = BigInt(u32(bytes, offset)) | BigInt(u32(bytes, offset + 4)) << 32n; if (value > BigInt(Number.MAX_SAFE_INTEGER)) throw new PackageError(Code.ResourceLimit, 'ZIP64 extent exceeds exact offset range'); return Number(value); }
  // ZIP extra fields are length-delimited; reject truncated or duplicate ZIP64 extents.
  function wideValues(extra, fields) {
    let offset = 0, selected = null;
    while (offset < extra.length) {
      if (extra.length - offset < 4) throw new PackageError(Code.InvalidArchive, 'Truncated ZIP extra field');
      const tag = u16(extra, offset), count = u16(extra, offset + 2); offset += 4;
      if (count > extra.length - offset) throw new PackageError(Code.InvalidArchive, 'Truncated ZIP extra payload');
      if (tag === 1) { if (selected !== null) throw new PackageError(Code.InvalidArchive, 'Duplicate ZIP64 extra field'); selected = extra.subarray(offset, offset + count); }
      offset += count;
    }
    const result = []; offset = 0;
    for (const width of fields) {
      if (selected === null || width > selected.length - offset) throw new PackageError(Code.InvalidArchive, 'Missing ZIP64 extra extent');
      result.push(width === 8 ? u64(selected, offset) : u32(selected, offset)); offset += width;
    }
    return result;
  }
  const cp437 = 'ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ';
  // Preserve ZIP's legacy CP437 filename semantics when the UTF-8 flag is absent.
  function decodeName(bytes, flags) {
    if (flags & 0x800) { try { return srlDecoder.decode(bytes); } catch { throw new PackageError(Code.InvalidArchive, 'Invalid UTF-8 ZIP name'); } }
    let result = ''; for (const byte of bytes) result += byte < 128 ? String.fromCharCode(byte) : cp437[byte - 128]; return result;
  }
  // Enforce relative physical names while retaining directory entries for integrity checks.
  function zipName(name) {
    if (typeof name !== 'string' || !name || name.startsWith('/') || name.includes('\\') || name.includes('\0') || /^[A-Za-z]:/.test(name)) throw new PackageError(Code.InvalidArchive, 'Unsafe ZIP entry name');
    const parts = name.split('/'); if (parts[parts.length - 1] === '') parts.pop();
)packagejs"
R"packagejs(    if (parts.some(part => !part || part === '.' || part === '..')) throw new PackageError(Code.InvalidArchive, 'Unsafe ZIP entry path');
    return name;
  }
  // Compute CRC in bounded loops so cancellation remains observable during large stored entries.
  function crc32(data, state) {
    let value = u32max;
    for (let offset = 0; offset < data.length; ++offset) {
      if (offset % chunkBytes === 0) state.poll(); value ^= data[offset];
)packagejs"
R"packagejs(      for (let bit = 0; bit < 8; ++bit) value = value >>> 1 ^ (value & 1 ? 0xedb88320 : 0);
    }
    state.poll(); return (value ^ u32max) >>> 0;
  }
  // Admit raw DEFLATE before exposing encoder or transport callbacks.
  function admitCompression(method, provider) {
    if (method !== 0 && method !== 8) throw new PackageError(Code.Unsupported, 'Unsupported ZIP compression method');
    if (method === 8 && !C.available(C.Format.Deflate, provider)) throw new PackageError(Code.Unavailable, 'Explicit DEFLATE provider is unavailable');
  }
  // Only explicit private host sources are released; arbitrary borrowed source.close is never called.
  const privateSources = new Map();
  /** Random-access ZIP reader. Borrowed readAt storage is copied before the next callback. */
  class ZipReader extends State {
    constructor(source, size, budget, options, release = null) { super(budget); this.source = source; this.size = size; this.options = options; this.release = release; this.entries = []; this.byName = new Map(); this.owned = privateSources.get(source) || null; }
    // Open an independent byte snapshot, borrowed random source, or explicitly hosted path/spool source.
    static async open(input, options = {}) {
      const budget = getBudget(options); let source = input, size, release = null, owned = null;
      const admission = new State(budget);
      try {
        if (input instanceof Uint8Array) {
          if (input.length > budget.limits.maxArchiveBytes) throw new PackageError(Code.ResourceLimit, 'Archive extent exceeds limit');
          release = budget.reserve(input.length); const data = copyBytes(input, admission); size = data.length;
          source = {size, readAt: (offset, count) => data.subarray(offset, offset + count)};
        } else if (typeof input === 'string' || input && typeof input.read === 'function' && typeof input.readAt !== 'function') {
          if (!options.host || typeof options.host.open !== 'function') throw new PackageError(Code.InvalidOptions, 'Path or sequential input requires an explicit package host');
          admission.poll(); source = await options.host.open(input, options, budget); admission.poll(); owned = privateSources.get(source);
          if (!owned) throw new PackageError(Code.InvalidOptions, 'Host did not return a private package source');
          size = integer(source.size, 'archive size');
        } else if (input && typeof input.readAt === 'function') size = integer(input.size, 'archive size');
        else throw new PackageError(Code.InvalidOptions, 'Expected bytes or random-access source');
        if (size > budget.limits.maxArchiveBytes) throw new PackageError(Code.ResourceLimit, 'Archive extent exceeds limit');
        const result = new ZipReader(source, size, budget, options, release); await result.index(); return result;
      } catch (error) { if (release) release(); if (owned) await owned(); throw error; }
    }
    // Read exactly the admitted extent, allowing bounded short reads but no empty progress.
    async readAt(offset, count) {
      this.poll(); integer(offset, 'ZIP offset'); allocation(count);
      if (offset > this.size || count > this.size - offset) throw new PackageError(Code.InvalidArchive, 'ZIP extent lies outside the archive');
      const result = new Uint8Array(count); let position = 0;
      while (position < count) {
        this.poll(); const request = Math.min(chunkBytes, count - position); this.budget.consume('workUnits', 1);
        let chunk = this.source.readAt(offset + position, request); if (chunk && typeof chunk.then === 'function') chunk = await chunk; this.poll();
        if (!(chunk instanceof Uint8Array) || !chunk.length || chunk.length > request) throw new PackageError(Code.InvalidArchive, 'Random source failed to deliver the requested ZIP extent');
        this.budget.consume('ioBytes', chunk.length); result.set(chunk, position); position += chunk.length;
      }
      this.poll(); return result;
    }
    // Validate the central index and every local extent before publishing any entry list.
    async index() {
      this.begin(); const release = this.budget.reserve(Math.min(this.size, 65557));
      try {
        if (this.size < 22) throw new PackageError(Code.InvalidArchive, 'Missing ZIP end record');
        const tailSize = Math.min(this.size, 65557), tailStart = this.size - tailSize, tail = await this.readAt(tailStart, tailSize); let end = -1;
        for (let at = tail.length - 22; at >= 0; --at) { if (u32(tail, at) === signature.end && u16(tail, at + 20) === tail.length - at - 22) { end = at; break; } }
        if (end < 0) throw new PackageError(Code.InvalidArchive, 'Missing exact ZIP end record');
        if (u16(tail, end + 4) || u16(tail, end + 6)) throw new PackageError(Code.Unsupported, 'Split ZIP archives are unsupported');
        const narrow = [u16(tail, end + 8), u16(tail, end + 10), u32(tail, end + 12), u32(tail, end + 16)];
        let count = narrow[1], centralSize = narrow[2], centralStart = narrow[3], boundary = tailStart + end;
        const hasLocator = boundary >= 20 && u32(await this.readAt(boundary - 20, 4), 0) === signature.locator;
        if (hasLocator) {
          const locator = await this.readAt(boundary - 20, 20);
          if (u32(locator, 4) || u32(locator, 16) !== 1) throw new PackageError(Code.Unsupported, 'Split ZIP64 archives are unsupported');
          const start = u64(locator, 8), record = await this.readAt(start, 56), recordSize = u64(record, 4);
          if (u32(record, 0) !== signature.wideEnd || recordSize < 44 || recordSize > boundary - 20 - start - 12 || start + 12 + recordSize !== boundary - 20) throw new PackageError(Code.InvalidArchive, 'Invalid ZIP64 end extent');
          if (u16(record, 14) > 45) throw new PackageError(Code.Unsupported, 'Unsupported ZIP64 extraction version');
          if (u32(record, 16) || u32(record, 20)) throw new PackageError(Code.Unsupported, 'Split ZIP64 archives are unsupported');
          const wide = [u64(record, 24), u64(record, 32), u64(record, 40), u64(record, 48)];
          if (wide[0] !== wide[1]) throw new PackageError(Code.InvalidArchive, 'Conflicting ZIP64 counts');
          for (let i = 0; i < 4; ++i) if (narrow[i] !== (i < 2 ? 65535 : u32max) && narrow[i] !== wide[i]) throw new PackageError(Code.InvalidArchive, 'Conflicting ZIP32/ZIP64 end fields');
          [count, centralSize, centralStart] = [wide[1], wide[2], wide[3]]; boundary = start;
        } else if (narrow[0] !== count) throw new PackageError(Code.InvalidArchive, 'Conflicting ZIP entry counts');
        if (centralStart > boundary || centralSize !== boundary - centralStart || count > Math.floor(centralSize / 46)) throw new PackageError(Code.InvalidArchive, 'Invalid central directory geometry');
        this.budget.consume('entries', count); this.budget.consume('metadataBytes', centralSize); allocation(centralSize);
        const releaseCentral = this.budget.reserve(centralSize); const entries = [], names = new Map();
        try {
          const central = await this.readAt(centralStart, centralSize); let at = 0;
          for (let index = 0; index < count; ++index) {
            this.poll(); this.budget.consume('workUnits', 1);
            if (central.length - at < 46 || u32(central, at) !== signature.central) throw new PackageError(Code.InvalidArchive, 'Truncated central entry');
            const flags = u16(central, at + 8), method = u16(central, at + 10), nameSize = u16(central, at + 28), extraSize = u16(central, at + 30), commentSize = u16(central, at + 32), extent = 46 + nameSize + extraSize + commentSize;
            if (flags & ~0x80e) throw new PackageError(Code.Unsupported, 'Encrypted or unsupported ZIP flags');
            if (u16(central, at + 6) > 45) throw new PackageError(Code.Unsupported, 'Unsupported ZIP extraction version');
            admitCompression(method, this.options.compressionProvider);
            if (extent > central.length - at) throw new PackageError(Code.InvalidArchive, 'Truncated central entry payload');
            const rawName = central.slice(at + 46, at + 46 + nameSize), name = zipName(decodeName(rawName, flags));
            this.budget.consume('decoderAllocationBytes', nameSize * 3 + 256, name);
            if (names.has(name)) throw new PackageError(Code.InvalidArchive, 'Duplicate physical ZIP entry', name);
            let expanded = u32(central, at + 24), compressed = u32(central, at + 20), local = u32(central, at + 42), disk = u16(central, at + 34);
            const fields = []; if (expanded === u32max) fields.push(8); if (compressed === u32max) fields.push(8); if (local === u32max) fields.push(8); if (disk === 65535) fields.push(4);
            const wide = wideValues(central.subarray(at + 46 + nameSize, at + 46 + nameSize + extraSize), fields); let wi = 0;
            const zip64Sizes = expanded === u32max || compressed === u32max;
            if (expanded === u32max) expanded = wide[wi++]; if (compressed === u32max) compressed = wide[wi++]; if (local === u32max) local = wide[wi++]; if (disk === 65535) disk = wide[wi++];
            if (disk) throw new PackageError(Code.Unsupported, 'Split ZIP entry is unsupported', name);
            if (expanded > this.budget.limits.maxPartBytes) throw new PackageError(Code.ResourceLimit, 'Declared part extent exceeds limit', name);
            if (local > centralStart || 30 > centralStart - local) throw new PackageError(Code.InvalidArchive, 'Local header overlaps central directory', name);
            const entry = {name, method: method === 8 ? 'deflate' : 'stored', flags, compressedBytes: compressed, expandedBytes: expanded, crc32: u32(central, at + 16), headerOffsetBytes: local, zip64Sizes, creatorSystem: central[at + 5], externalAttributes: u32(central, at + 38), rawName};
            entries.push(entry); names.set(name, entry); at += extent;
          }
          if (at !== central.length) throw new PackageError(Code.InvalidArchive, 'Unclaimed central directory bytes');
        } finally { releaseCentral(); }
        const ranges = [];
        for (const entry of entries) {
          this.poll(); const local = await this.readAt(entry.headerOffsetBytes, 30);
          if (u32(local, 0) !== signature.local || u16(local, 6) !== entry.flags || u16(local, 8) !== (entry.method === 'deflate' ? 8 : 0)) throw new PackageError(Code.InvalidArchive, 'Conflicting local header', entry.name);
          const nameSize = u16(local, 26), extraSize = u16(local, 28), start = entry.headerOffsetBytes + 30;
          if (start > centralStart || nameSize + extraSize > centralStart - start) throw new PackageError(Code.InvalidArchive, 'Local name or extra overlaps central directory', entry.name);
          const raw = await this.readAt(start, nameSize + extraSize);
          if (nameSize !== entry.rawName.length || !entry.rawName.every((byte, index) => byte === raw[index])) throw new PackageError(Code.InvalidArchive, 'Conflicting local filename', entry.name);
          let compressed = u32(local, 18), expanded = u32(local, 22), wi = 0; const fields = [];
          const descriptorWide = entry.zip64Sizes || compressed === u32max || expanded === u32max;
          if (expanded === u32max) fields.push(8); if (compressed === u32max) fields.push(8);
          const values = wideValues(raw.subarray(nameSize), fields);
          if (expanded === u32max) expanded = values[wi++]; if (compressed === u32max) compressed = values[wi++];
          const crc = u32(local, 14), descriptor = (entry.flags & 8) !== 0;
          if ((!descriptor || crc !== 0) && crc !== entry.crc32 || (!descriptor || compressed !== 0) && compressed !== entry.compressedBytes || (!descriptor || expanded !== 0) && expanded !== entry.expandedBytes) throw new PackageError(Code.Integrity, 'Conflicting local sizes or CRC', entry.name);
          const dataStart = start + nameSize + extraSize;
)packagejs"
R"packagejs(          if (dataStart > centralStart || entry.compressedBytes > centralStart - dataStart) throw new PackageError(Code.InvalidArchive, 'Payload overlaps central directory', entry.name);
          let dataEnd = dataStart + entry.compressedBytes;
          if (descriptor) {
)packagejs"
R"packagejs(            const width = descriptorWide ? 8 : 4, length = 4 + width * 2;
            if (length > centralStart - dataEnd) throw new PackageError(Code.InvalidArchive, 'Truncated ZIP descriptor', entry.name);
            const bytes = await this.readAt(dataEnd, Math.min(length + 4, centralStart - dataEnd));
            const matches = offset => bytes.length - offset >= length && u32(bytes, offset) === entry.crc32 && (width === 8 ? u64(bytes, offset + 4) : u32(bytes, offset + 4)) === entry.compressedBytes && (width === 8 ? u64(bytes, offset + 4 + width) : u32(bytes, offset + 4 + width)) === entry.expandedBytes;
            if (matches(0)) dataEnd += length;
            else if (u32(bytes, 0) === signature.descriptor && matches(4)) dataEnd += length + 4;
            else throw new PackageError(Code.Integrity, 'Conflicting ZIP descriptor', entry.name);
          }
          delete entry.rawName; entry.dataOffsetBytes = dataStart; Object.freeze(entry); ranges.push([entry.headerOffsetBytes, dataEnd]);
        }
        ranges.sort((a, b) => a[0] - b[0]); for (let i = 1; i < ranges.length; ++i) if (ranges[i][0] < ranges[i - 1][1]) throw new PackageError(Code.InvalidArchive, 'Overlapping ZIP local extents');
        this.poll(); this.entries = Object.freeze(entries); this.byName = names;
      } catch (error) { throw this.fail(error); } finally { release(); this.end(); }
    }
    // Return a verified independent part; declared size is never trusted as actual output.
    async read(name) {
      this.begin(); let release = null;
      try {
        this.poll(); const entry = this.byName.get(name); if (!entry) throw new PackageError(Code.MissingPart, 'ZIP part is missing', name);
        this.budget.consume('expandedBytes', entry.expandedBytes, name); this.budget.consume('decoderAllocationBytes', entry.expandedBytes, name);
        this.budget.consume('workUnits', entry.compressedBytes + entry.expandedBytes, name);
        allocation(entry.compressedBytes); allocation(entry.expandedBytes);
        const scratch = entry.compressedBytes * 2 + entry.expandedBytes * 2 + (entry.method === 'deflate' ? chunkBytes : 0); release = this.budget.reserve(scratch, name);
        const raw = await this.readAt(entry.dataOffsetBytes, entry.compressedBytes); let data;
        if (entry.method === 'stored') { if (raw.length !== entry.expandedBytes) throw new PackageError(Code.Integrity, 'Stored ZIP size mismatch', name); data = raw; }
        else {
          let hookFailed = false, hookFailure;
          try { data = await C.decompress(raw, new C.DecodeOptions({format: C.Format.Deflate, provider: this.options.compressionProvider, maxInputBytes: raw.length, maxOutputBytes: entry.expandedBytes, maxWindowBytes: chunkBytes, checkCancelled: () => { try { this.poll(); } catch (error) { hookFailed = true; hookFailure = error; throw error; } }})); }
          catch (error) { if (hookFailed) throw hookFailure; if (error instanceof C.Error && (error.code === C.Code.InvalidData || error.code === C.Code.ResourceLimit)) throw new PackageError(Code.Integrity, 'Invalid ZIP DEFLATE payload or size', name); throw error; }
        }
        this.poll(); if (data.length !== entry.expandedBytes || crc32(data, this) !== entry.crc32) throw new PackageError(Code.Integrity, 'ZIP CRC or expanded size mismatch', name);
        this.poll(); return data;
      } catch (error) { throw this.fail(error); } finally { if (release) release(); this.end(); }
    }
    // Fence immediately, release private snapshots/spools, and never close borrowed input.
    async close() { super.close(); this.source = null; if (this.release) { this.release(); this.release = null; } if (this.owned) { const cleanup = this.owned; this.owned = null; await cleanup(); } }
  }
)packagejs"
