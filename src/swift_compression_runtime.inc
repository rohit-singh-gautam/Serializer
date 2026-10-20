// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compressswift(
/// Owning standard frames and sequential borrowed wrappers; no external provider dependency.
public enum @compression_name@ {
    public enum Format: Swift.Sendable { case none, gzip, zlib, deflate, zstd, lz4 }
    public enum Code: Swift.Sendable { case unavailable, invalidOptions, invalidData, resourceLimit }
    /// Native, transport and cancellation errors keep their original values and types.
    public final class Failure: Swift.Error {
        public let code: Code
        public let message: String
        public let cause: (any Swift.Error)?
        /// Retain a stable category without including payload excerpts.
        public init(_ code: Code, _ message: String, cause: (any Swift.Error)? = nil) {
            self.code = code; self.message = message; self.cause = cause
        }
    }
    public struct EncodeOptions {
        public let format: Format
        public let provider: Provider?
        public let maxInputBytes: Int
        public let maxOutputBytes: Int
        public let level: Int
        public let checkCancelled: (() throws -> Void)?
        /// Preserve the original default profile and exact public initializer signature.
        public init(format: Format = .none, maxInputBytes: Int = 64*1024*1024,
                    maxOutputBytes: Int = 64*1024*1024, level: Int = 6,
                    checkCancelled: (() throws -> Void)? = nil) {
            self.format = format; provider = nil; self.maxInputBytes = maxInputBytes
            self.maxOutputBytes = maxOutputBytes; self.level = level; self.checkCancelled = checkCancelled
        }
        /// Select one explicit caller-owned optional provider with independent byte/window limits.
        public init(format: Format, provider: Provider, maxInputBytes: Int = 64*1024*1024,
                    maxOutputBytes: Int = 64*1024*1024, level: Int? = nil,
                    checkCancelled: (() throws -> Void)? = nil) {
            self.format = format; self.provider = provider; self.maxInputBytes = maxInputBytes
            self.maxOutputBytes = maxOutputBytes; self.level = level ?? provider.defaultLevel; self.checkCancelled = checkCancelled
        }
        /// Preserve the capability while adding wrapper-owned lifecycle checks.
        fileprivate func checking(_ check: @escaping () throws -> Void) -> EncodeOptions {
            if let provider { return EncodeOptions(format: format, provider: provider, maxInputBytes: maxInputBytes,
                maxOutputBytes: maxOutputBytes, level: level, checkCancelled: check) }
            return EncodeOptions(format: format, maxInputBytes: maxInputBytes,
                maxOutputBytes: maxOutputBytes, level: level, checkCancelled: check)
        }
    }
    public struct DecodeOptions {
        public let format: Format
        public let provider: Provider?
        public let maxInputBytes: Int
        public let maxOutputBytes: Int
        public let maxWindowBytes: Int
        public let checkCancelled: (() throws -> Void)?
        /// Preserve the original default profile and exact public initializer signature.
        public init(format: Format = .none, maxInputBytes: Int = 64*1024*1024,
                    maxOutputBytes: Int = 64*1024*1024, maxWindowBytes: Int = 8*1024*1024,
                    checkCancelled: (() throws -> Void)? = nil) {
            self.format = format; provider = nil; self.maxInputBytes = maxInputBytes
            self.maxOutputBytes = maxOutputBytes; self.maxWindowBytes = maxWindowBytes; self.checkCancelled = checkCancelled
        }
        /// Select one explicit caller-owned optional provider with independent byte/window limits.
        public init(format: Format, provider: Provider, maxInputBytes: Int = 64*1024*1024,
                    maxOutputBytes: Int = 64*1024*1024, maxWindowBytes: Int = 8*1024*1024,
                    checkCancelled: (() throws -> Void)? = nil) {
            self.format = format; self.provider = provider; self.maxInputBytes = maxInputBytes
            self.maxOutputBytes = maxOutputBytes; self.maxWindowBytes = maxWindowBytes; self.checkCancelled = checkCancelled
        }
        /// Preserve the capability while adding wrapper-owned lifecycle checks.
        fileprivate func checking(_ check: @escaping () throws -> Void) -> DecodeOptions {
            if let provider { return DecodeOptions(format: format, provider: provider, maxInputBytes: maxInputBytes,
                maxOutputBytes: maxOutputBytes, maxWindowBytes: maxWindowBytes, checkCancelled: check) }
            return DecodeOptions(format: format, maxInputBytes: maxInputBytes,
                maxOutputBytes: maxOutputBytes, maxWindowBytes: maxWindowBytes, checkCancelled: check)
        }
    }
    private static let batchBytes = 32768
    private static let windowBytes = 32768
    /// Default capability probing retains its original provider-free contract.
    public static func available(_ format: Format) -> Bool { format != .zstd && format != .lz4 }
    /// Inspect explicit immutable capabilities without calling factories or application code.
    public static func available(_ format: Format, provider: Provider?) -> Bool {
        guard let provider else { return available(format) }
        return (format == .zstd || format == .lz4) && provider.format == format &&
            provider.minimumLevel <= provider.defaultLevel && provider.defaultLevel <= provider.maximumLevel
    }
    /// Admit encoding policy and exact provider-specific levels before any callback or context.
    private static func validate(_ options: EncodeOptions) throws {
        guard options.maxInputBytes >= 0, options.maxOutputBytes >= 0 else { throw Failure(.invalidOptions, "Invalid compression encoding policy") }
        if let provider = options.provider {
            guard available(options.format, provider: provider) else { throw Failure(.unavailable, "Selected compression provider is unavailable") }
            guard options.level >= provider.minimumLevel, options.level <= provider.maximumLevel else { throw Failure(.invalidOptions, "Invalid optional compression level") }
        } else {
            guard (0...9).contains(options.level) else { throw Failure(.invalidOptions, "Invalid compression encoding policy") }
            guard available(options.format) else { throw Failure(.unavailable, "Selected compression provider is unavailable") }
        }
    }
    /// Admit conservative optional/DEFLATE history before input, contexts or callbacks.
    private static func validate(_ options: DecodeOptions) throws {
        guard options.maxInputBytes >= 0, options.maxOutputBytes >= 0, options.maxWindowBytes >= 0 else { throw Failure(.invalidOptions, "Invalid compression decoding policy") }
        guard available(options.format, provider: options.provider) else { throw Failure(.unavailable, "Selected compression provider is unavailable") }
        let minimumWindow = options.format == .zstd ? 1024 : options.format == .lz4 ? 65536 : windowBytes
        if options.format != .none && options.maxWindowBytes < minimumWindow { throw Failure(.resourceLimit, "Compression window exceeds limit") }
    }
    /// Check complete spans before indexing or subtracting untrusted extents.
    private static func span(_ data: [UInt8], _ position: Int, _ count: Int) throws {
        guard position >= 0, count >= 0, position <= data.count, count <= data.count-position else {
            throw Failure(.invalidData, "Truncated compression data")
        }
    }
    /// Read externally defined little-endian words independently of native layout.
    private static func little(_ data: [UInt8], _ position: Int, _ count: Int) throws -> UInt32 {
        try span(data, position, count); var value: UInt32 = 0
        for index in 0..<count { value |= UInt32(data[position+index]) << (index*8) }
        return value
    }
    /// Compute CRC-32 with fixed storage and bounded cooperative work intervals.
    private static func crc(_ data: [UInt8], _ start: Int, _ count: Int, _ check: (() throws -> Void)?) throws -> UInt32 {
        var value = UInt32.max
        for index in 0..<count {
            if index % 4096 == 0 { try check?() }
            value ^= UInt32(data[start+index])
            for _ in 0..<8 { value = (value >> 1) ^ (value & 1 == 0 ? 0 : 0xedb88320) }
        }
        try check?(); return ~value
    }
    /// Bound Adler sums before overflow while preserving the exact standard checksum.
    private static func adler(_ data: [UInt8], _ check: (() throws -> Void)?) throws -> UInt32 {
        var first: UInt32 = 1; var second: UInt32 = 0
        for index in data.indices {
            if index % 4096 == 0 { try check?() }
            first = (first + UInt32(data[index])) % 65521; second = (second + first) % 65521
        }
        try check?(); return (second << 16) | first
    }
    /// Accumulate only admitted bytes; allocator capacity and process RSS remain outside this cap.
    private final class Buffer {
        var bytes: [UInt8] = []
        let maximum: Int
        let check: (() throws -> Void)?
        /// Own one bounded complete output buffer.
        init(_ maximum: Int, _ check: (() throws -> Void)?) { self.maximum = maximum; self.check = check }
        /// Reject before growing the logical buffer and poll at bounded output intervals.
        func append(_ byte: UInt8) throws {
            guard bytes.count < maximum else { throw Failure(.resourceLimit, "Compression output exceeds limit") }
            if bytes.count % batchBytes == 0 { try check?() }
            bytes.append(byte)
        }
        /// Append a word using explicit standard byte order.
        func word(_ value: UInt32, bigEndian: Bool = false) throws {
            for index in 0..<4 { try append(UInt8(truncatingIfNeeded: value >> ((bigEndian ? 3-index : index)*8))) }
        }
    }
    /// Read bounded in-memory DEFLATE bits without scalar borrowed transport calls.
    private final class Bits {
        let data: [UInt8]
        var position: Int
        var value = 0
        var count = 0
        var work = 0
        let check: (() throws -> Void)?
        /// Borrow value-semantic stable input through one validation/expansion pass.
        init(_ data: [UInt8], _ start: Int, _ check: (() throws -> Void)?) {
            self.data = data; position = start; self.check = check
        }
        /// Return an RFC1951 little-bit-endian field with checked input extent.
        func read(_ size: Int) throws -> Int {
            work += 1; if work % 4096 == 0 { try check?() }
            while count < size {
                guard position < data.count else { throw Failure(.invalidData, "Truncated DEFLATE bits") }
                value |= Int(data[position]) << count; position += 1; count += 8
            }
            let result = value & ((1 << size)-1); value >>= size; count -= size; return result
        }
        /// Stored blocks discard only the remaining bits of the current byte.
        func align() { value = 0; count = 0 }
    }
    /// Small fixed-extent canonical Huffman tables never derive allocations from payload counts.
    private struct Huffman {
        var counts = [Int](repeating: 0, count: 16)
        var first = [Int](repeating: 0, count: 16)
        var offsets = [Int](repeating: 0, count: 16)
        var symbols = [Int](repeating: 0, count: 288)
        var maximum = 0
        /// Reject over/incomplete trees, allowing only RFC's one-symbol or unused-distance cases.
        init(_ lengths: [Int], _ start: Int, _ count: Int, codeLengths: Bool = false, allowEmpty: Bool = false) throws {
            var used = 0
            for index in 0..<count {
                let size = lengths[start+index]
                guard (0...15).contains(size) else { throw Failure(.invalidData, "Invalid DEFLATE code length") }
                if size != 0 { counts[size] += 1; used += 1; maximum = Swift.max(maximum, size) }
            }
            if used == 0 {
                if allowEmpty { return }
)compressswift"
R"compressswift(                throw Failure(.invalidData, "Empty DEFLATE Huffman tree")
            }
            var left = 1
            for size in 1...15 {
                left = (left << 1)-counts[size]
                if left < 0 { throw Failure(.invalidData, "Oversubscribed DEFLATE tree") }
            }
            if left != 0 && (codeLengths || used != 1 || maximum != 1) { throw Failure(.invalidData, "Incomplete DEFLATE tree") }
            var code = 0; var offset = 0
            for size in 1...15 {
                code = (code+counts[size-1]) << 1; first[size] = code
                offsets[size] = offset; offset += counts[size]
            }
            var next = offsets
            for symbol in 0..<count {
                let size = lengths[start+symbol]
                if size != 0 { symbols[next[size]] = symbol; next[size] += 1 }
            }
        }
        /// Canonical code bits arrive most-significant-code-bit first.
        func decode(_ bits: Bits) throws -> Int {
            var code = 0
            if maximum > 0 {
                for size in 1...maximum {
                    code = (code << 1) | (try bits.read(1)); let index = code-first[size]
                    if index >= 0 && index < counts[size] { return symbols[offsets[size]+index] }
                }
            }
            throw Failure(.invalidData, "Invalid DEFLATE Huffman code")
        }
    }
    private static let lengthBase = [3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258]
    private static let lengthExtra = [0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0]
    private static let distanceBase = [1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577]
    private static let distanceExtra = [0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13]
    private static let codeOrder = [16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15]
    /// Build the RFC1951 fixed literal or distance alphabet.
    private static func fixed(_ distance: Bool) throws -> Huffman {
        let lengths = (0..<(distance ? 32 : 288)).map { distance ? 5 : ($0 < 144 ? 8 : ($0 < 256 ? 9 : ($0 < 280 ? 7 : 8))) }
        return try Huffman(lengths, 0, lengths.count)
    }
    /// Validate/count first, then optionally expand into the already admitted exact logical extent.
    private static func inflate(_ data: [UInt8], _ start: Int, _ maximum: Int, _ window: Int,
                                _ check: (() throws -> Void)?, expand: Bool) throws -> (end: Int, size: Int, bytes: [UInt8]) {
        let bits = Bits(data, start, check); var output = 0; var bytes: [UInt8] = []
        if expand { bytes.reserveCapacity(maximum) }
        let fixedLiteral = try fixed(false); let fixedDistance = try fixed(true)
        var final = 0
        repeat {
            try check?(); final = try bits.read(1); let kind = try bits.read(2)
            if kind == 0 {
                bits.align(); let size = try bits.read(16); let complement = try bits.read(16)
                guard size ^ 65535 == complement else { throw Failure(.invalidData, "Invalid stored DEFLATE length") }
                try span(data, bits.position, size)
                guard size <= maximum-output else { throw Failure(.resourceLimit, "Expanded output exceeds limit") }
                if expand {
                    for index in 0..<size {
                        if index % 4096 == 0 { try check?() }; bytes.append(data[bits.position+index])
                    }
                }
                bits.position += size; output += size; continue
            }
            guard kind != 3 else { throw Failure(.invalidData, "Reserved DEFLATE block type") }
            var literal = fixedLiteral; var distance = fixedDistance
            if kind == 2 {
                let literalCount = try bits.read(5)+257; let distanceCount = try bits.read(5)+1; let codeCount = try bits.read(4)+4
                guard literalCount <= 286 else { throw Failure(.invalidData, "Invalid DEFLATE literal alphabet") }
                var codeLengths = [Int](repeating: 0, count: 19)
                for index in 0..<codeCount { codeLengths[codeOrder[index]] = try bits.read(3) }
                let codes = try Huffman(codeLengths, 0, 19, codeLengths: true)
                let total = literalCount+distanceCount; var lengths = [Int](repeating: 0, count: 320); var position = 0
                while position < total {
                    let symbol = try codes.decode(bits)
                    if symbol < 16 { lengths[position] = symbol; position += 1 }
                    else {
                        let count = try bits.read(symbol == 16 ? 2 : (symbol == 17 ? 3 : 7))+(symbol == 18 ? 11 : 3)
                        guard !(symbol == 16 && position == 0), count <= total-position else { throw Failure(.invalidData, "Invalid DEFLATE length repeat") }
                        let value = symbol == 16 ? lengths[position-1] : 0
                        for _ in 0..<count { lengths[position] = value; position += 1 }
                    }
                }
                guard lengths[256] != 0 else { throw Failure(.invalidData, "Missing DEFLATE end code") }
                literal = try Huffman(lengths, 0, literalCount)
                distance = try Huffman(lengths, literalCount, distanceCount, allowEmpty: true)
            }
            while true {
                let symbol = try literal.decode(bits)
                if symbol == 256 { break }
                if symbol < 256 {
                    guard output < maximum else { throw Failure(.resourceLimit, "Expanded output exceeds limit") }
                    output += 1; if expand { bytes.append(UInt8(symbol)) }; continue
                }
                let lengthCode = symbol-257
                guard lengthBase.indices.contains(lengthCode) else { throw Failure(.invalidData, "Reserved DEFLATE length") }
                let count = try lengthBase[lengthCode]+bits.read(lengthExtra[lengthCode]); let distanceCode = try distance.decode(bits)
                guard distanceBase.indices.contains(distanceCode) else { throw Failure(.invalidData, "Reserved DEFLATE distance") }
                let back = try distanceBase[distanceCode]+bits.read(distanceExtra[distanceCode])
                guard back <= output, back <= window else { throw Failure(.invalidData, "DEFLATE history is unavailable") }
                guard count <= maximum-output else { throw Failure(.resourceLimit, "Expanded output exceeds limit") }
                if expand { for _ in 0..<count { bytes.append(bytes[bytes.count-back]) } }
                output += count
            }
        } while final == 0
        try check?(); return (bits.position, output, bytes)
    }

    /// Emit little-bit-endian fields through the same bounded complete output buffer.
    private final class BitOutput {
        let output: Buffer
        var value = 0
        var count = 0
        /// Borrow private output only through one encoding call.
        init(_ output: Buffer) { self.output = output }
        /// Append at most sixteen bits, retaining at most seven after complete bytes are emitted.
        func write(_ value: Int, _ size: Int) throws {
            self.value |= value << count; count += size
            while count >= 8 { try output.append(UInt8(truncatingIfNeeded: self.value)); self.value >>= 8; count -= 8 }
        }
        /// Reverse a canonical Huffman code for RFC1951 bit transport.
        func code(_ value: Int, _ size: Int) throws {
            var reversed = 0
            for index in 0..<size { reversed = (reversed << 1) | ((value >> index) & 1) }
            try write(reversed, size)
        }
        /// Emit an RFC fixed literal/length symbol.
        func symbol(_ value: Int) throws {
            if value < 144 { try code(48+value, 8) }
            else if value < 256 { try code(400+value-144, 9) }
            else if value < 280 { try code(value-256, 7) }
            else { try code(192+value-280, 8) }
        }
        /// Zero-pad the one final partial byte; it is part of the exact raw frame extent.
        func finish() throws { if count != 0 { try output.append(UInt8(truncatingIfNeeded: value)); value = 0; count = 0 } }
    }
    /// Encode stored blocks at level zero or bounded LZ77 fixed-Huffman blocks at levels one to nine.
    private static func deflate(_ input: [UInt8], _ options: EncodeOptions, _ output: Buffer) throws {
        if options.level == 0 {
            var position = 0
            repeat {
                try options.checkCancelled?(); let size = Swift.min(65535, input.count-position)
                try output.append(position+size == input.count ? 1 : 0)
                try output.append(UInt8(truncatingIfNeeded: size)); try output.append(UInt8(truncatingIfNeeded: size >> 8))
                let complement = size ^ 65535
                try output.append(UInt8(truncatingIfNeeded: complement)); try output.append(UInt8(truncatingIfNeeded: complement >> 8))
                for index in 0..<size { try output.append(input[position+index]) }; position += size
            } while position < input.count
            return
        }
        let bits = BitOutput(output); try bits.write(3, 3)
        let effort = [0,4,8,12,16,24,32,48,64,96][options.level]
        var heads = [Int](repeating: -1, count: 4096); var chain = [Int](repeating: -1, count: windowBytes)
        var position = 0; var work = 0
        // Fixed-size hash chains retain at most the standard history window.
        func hash(_ index: Int) -> Int { (Int(input[index])*251+Int(input[index+1])*31+Int(input[index+2])) & 4095 }
        while position < input.count {
            try options.checkCancelled?(); var best = 0; var back = 0
            if input.count-position >= 3 {
                var candidate = heads[hash(position)]; var attempts = 0
                while candidate >= 0 && candidate < position && position-candidate <= windowBytes && attempts < effort {
                    attempts += 1; var size = 0; let maximum = Swift.min(258, input.count-position)
                    while size < maximum && input[candidate+size] == input[position+size] {
                        size += 1; work += 1; if work % 4096 == 0 { try options.checkCancelled?() }
                    }
                    if size > best && size >= 3 { best = size; back = position-candidate }
                    if best == maximum { break }
                    let previous = chain[candidate % windowBytes]
                    if previous >= candidate { break }; candidate = previous
                }
            }
            let consumed = best >= 3 ? best : 1
            if best >= 3 {
                var lengthCode = 0
                while lengthCode+1 < lengthBase.count && lengthBase[lengthCode+1] <= best { lengthCode += 1 }
                try bits.symbol(lengthCode+257); try bits.write(best-lengthBase[lengthCode], lengthExtra[lengthCode])
                var distanceCode = 0
                while distanceCode+1 < distanceBase.count && distanceBase[distanceCode+1] <= back { distanceCode += 1 }
                try bits.code(distanceCode, 5); try bits.write(back-distanceBase[distanceCode], distanceExtra[distanceCode])
            } else { try bits.symbol(Int(input[position])) }
            for index in position..<(position+consumed) {
                if input.count-index >= 3 { let key = hash(index); chain[index % windowBytes] = heads[key]; heads[key] = index }
            }
            position += consumed
        }
        try bits.symbol(256); try bits.finish()
    }
    /// Copy through bounded cancellation intervals into independent logical storage.
    private static func copy(_ input: [UInt8], _ maximum: Int, _ check: (() throws -> Void)?) throws -> [UInt8] {
        guard input.count <= maximum else { throw Failure(.resourceLimit, "Compression byte limit exceeded") }
        let output = Buffer(maximum, check)
)compressswift"
R"compressswift(        for byte in input { try output.append(byte) }; try check?(); return output.bytes
    }
    /// Serialize one standard outer frame; the inner native bytes remain unchanged.
    public static func compress(_ input: [UInt8], options: EncodeOptions = EncodeOptions()) throws -> [UInt8] {
        try validate(options); try options.checkCancelled?()
        guard input.count <= options.maxInputBytes else { throw Failure(.resourceLimit, "Serialized input exceeds limit") }
        if options.format == .none { return try copy(input, options.maxOutputBytes, options.checkCancelled) }
        if options.provider != nil { return try optionalCompress(input, options) }
        let output = Buffer(options.maxOutputBytes, options.checkCancelled)
        if options.format == .gzip { for byte: UInt8 in [31,139,8,0,0,0,0,0,0,255] { try output.append(byte) } }
        if options.format == .zlib { try output.append(120); try output.append(156) }
        try deflate(input, options, output)
        if options.format == .gzip {
            try output.word(crc(input, 0, input.count, options.checkCancelled)); try output.word(UInt32(truncatingIfNeeded: input.count))
        } else if options.format == .zlib { try output.word(adler(input, options.checkCancelled), bigEndian: true) }
        try options.checkCancelled?(); return output.bytes
    }
    /// Check optional gzip fields and header CRC without trusting their lengths or termination.
    private static func gzipStart(_ input: [UInt8], _ check: (() throws -> Void)?) throws -> Int {
        try span(input, 0, 10)
        guard input[0] == 31, input[1] == 139, input[2] == 8, input[3] & 224 == 0 else { throw Failure(.invalidData, "Invalid gzip header") }
        let flags = input[3]; var position = 10
        if flags & 4 != 0 {
            let size = Int(try little(input, position, 2)); position += 2
            try span(input, position, size); position += size
        }
        for flag: UInt8 in [8,16] {
            if flags & flag != 0 {
                while true {
                    if position % 4096 == 0 { try check?() }; try span(input, position, 1)
                    let byte = input[position]; position += 1; if byte == 0 { break }
                }
            }
        }
        if flags & 2 != 0 {
            let expected = try little(input, position, 2)
            guard expected == (try crc(input, 0, position, check)) & 65535 else { throw Failure(.invalidData, "Invalid gzip header checksum") }
            position += 2
        }
        return position
    }
    /// Admit exact grammar-derived extent/output before expansion, then verify complete frame integrity.
    public static func decompress(_ input: [UInt8], options: DecodeOptions = DecodeOptions()) throws -> [UInt8] {
        try validate(options); try options.checkCancelled?()
        guard input.count <= options.maxInputBytes else { throw Failure(.resourceLimit, "Compressed input exceeds limit") }
        if options.format == .none { return try copy(input, options.maxOutputBytes, options.checkCancelled) }
        if options.provider != nil { return try optionalDecompress(input, options) }
        var start = 0; var trailer = 0; var window = windowBytes
        if options.format == .gzip { start = try gzipStart(input, options.checkCancelled); trailer = 8 }
        else if options.format == .zlib {
            try span(input, 0, 2); let cmf = Int(input[0]); let flags = Int(input[1])
            guard cmf & 15 == 8, cmf >> 4 <= 7, ((cmf << 8) | flags) % 31 == 0, flags & 32 == 0 else {
                throw Failure(.invalidData, "Invalid or dictionary-dependent zlib header")
            }
            start = 2; trailer = 4; window = 1 << ((cmf >> 4)+8)
        }
        let measured = try inflate(input, start, options.maxOutputBytes, window, options.checkCancelled, expand: false)
        try span(input, measured.end, trailer)
        guard input.count-measured.end == trailer else { throw Failure(.invalidData, "Trailing or concatenated compression data") }
        let output = try inflate(input, start, measured.size, window, options.checkCancelled, expand: true).bytes
        if options.format == .gzip {
            guard try little(input, measured.end, 4) == crc(output, 0, output.count, options.checkCancelled),
                  try little(input, measured.end+4, 4) == UInt32(truncatingIfNeeded: output.count) else { throw Failure(.invalidData, "Invalid gzip trailer") }
        } else if options.format == .zlib {
            var expected: UInt32 = 0
            for index in 0..<4 { expected = (expected << 8) | UInt32(input[measured.end+index]) }
            guard expected == (try adler(output, options.checkCancelled)) else { throw Failure(.invalidData, "Invalid zlib checksum") }
        }
        try options.checkCancelled?(); return output
    }
    /// Adapt an owning native encoder; its own allocations remain outside the compression byte cap.
    public static func encode(_ encoder: () throws -> [UInt8], options: EncodeOptions = EncodeOptions()) throws -> [UInt8] {
        try validate(options); try options.checkCancelled?(); let bytes = try encoder()
        try options.checkCancelled?(); return try compress(bytes, options: options)
    }
    /// Return a fresh native candidate only after exact caller decoding and final cancellation.
    public static func decode<T>(_ input: [UInt8], decoder: ([UInt8]) throws -> T,
                                 options: DecodeOptions = DecodeOptions()) throws -> T {
        let bytes = try decompress(input, options: options); try options.checkCancelled?()
        let candidate = try decoder(bytes); try options.checkCancelled?(); return candidate
    }

    /// Sequential lifecycle state; synchronous reentry is fenced without adding thread synchronization.
    private final class State {
        var busy = false
        var closed = false
        var finished = false
        var first: (any Swift.Error)?
        /// Admit one operation and make a swallowed nested rejection sticky.
        func begin() throws {
            if busy { throw fail(Failure(.invalidOptions, "Reentrant compression operation")) }
            guard !closed, !finished, first == nil else { throw Failure(.invalidOptions, "Inactive compression wrapper", cause: first) }
            busy = true
        }
        /// Check state after every application callback before more borrowed work or publication.
        func active() throws {
            if let first { throw first }
            if closed || finished { throw fail(Failure(.invalidOptions, "Inactive compression wrapper")) }
        }
        /// Preserve the first original failure for the currently active outer operation.
        func fail(_ error: any Swift.Error) -> any Swift.Error { if first == nil { first = error }; return first! }
        /// Surround cancellation hooks with activity checks so close/reentry cannot escape fencing.
        func poll(_ check: (() throws -> Void)?) throws { try active(); try check?(); try active() }
    }
    /// Borrow a complete-chunk sink; finish is explicit and close emits no trailer or borrowed call.
    public final class Output {
        private let sink: ([UInt8]) throws -> Void
        private let options: EncodeOptions
        private let state = State()
        private var staged: [UInt8] = []
        /// Retain the sink and immutable policy without writing or taking transport ownership.
        public init(sink: @escaping ([UInt8]) throws -> Void, options: EncodeOptions = EncodeOptions()) throws {
            try validate(options); self.sink = sink; self.options = options
        }
        /// Stage admitted value-semantic bytes; failures never resume the wrapper.
        public func write(_ bytes: [UInt8]) throws {
            try state.begin(); defer { state.busy = false }
            do {
                try state.poll(options.checkCancelled)
                guard bytes.count <= options.maxInputBytes-staged.count else { throw Failure(.resourceLimit, "Serialized input exceeds limit") }
                var offset = 0
                while offset < bytes.count {
                    try state.poll(options.checkCancelled); let count = Swift.min(batchBytes, bytes.count-offset)
                    staged.append(contentsOf: bytes[offset..<(offset+count)]); offset += count
                }
                try state.poll(options.checkCancelled)
            } catch { throw state.fail(error) }
        }
        /// Check activity/cancellation without forwarding flush or publishing staged output.
        public func flush() throws {
            try state.begin(); defer { state.busy = false }
            do { try state.poll(options.checkCancelled) } catch { throw state.fail(error) }
        }
        /// Complete one standard frame before bounded borrowed writes; an external prefix may survive failure.
        public func finish() throws {
            try state.begin(); defer { state.busy = false }
            do {
                let policy = options.checking { try self.state.poll(self.options.checkCancelled) }
                let packed = try compress(staged, options: policy); var offset = 0
                while offset < packed.count {
                    try state.poll(options.checkCancelled); let count = Swift.min(batchBytes, packed.count-offset)
                    try sink(Array(packed[offset..<(offset+count)])); try state.poll(options.checkCancelled); offset += count
                }
                try state.poll(options.checkCancelled); staged.removeAll(); state.finished = true
            } catch { throw state.fail(error) }
        }
        /// Abandon private staging; the application continues to own its transport.
        public func close() { state.closed = true; staged.removeAll() }
    }
    /// Borrow an EOF-delimited bounded source; nil means actual EOF and empty progress is invalid.
    public final class Input {
        private let source: (Int) throws -> [UInt8]?
        private let options: DecodeOptions
        private let state = State()
        private var staged: [UInt8]?
        private var position = 0
        /// Retain a source without reading; optional-provider admission precedes all callbacks.
        public init(source: @escaping (Int) throws -> [UInt8]?, options: DecodeOptions = DecodeOptions()) throws {
            try validate(options); self.source = source; self.options = options
        }
        /// Stage and validate one complete frame while retaining this wrapper's activity policy.
        private func prepare() throws {
            if staged != nil { return }
            var bytes: [UInt8] = []
            while true {
                try state.poll(options.checkCancelled)
                let remaining = options.maxInputBytes-bytes.count
                let request = remaining >= batchBytes ? batchBytes : remaining+1
                let block = try source(request); try state.poll(options.checkCancelled)
                guard let block else { break }
                guard !block.isEmpty, block.count <= request else { throw Failure(.invalidData, "Invalid or zero-progress compression source") }
                guard block.count <= remaining else { throw Failure(.resourceLimit, "Compressed input exceeds limit") }
                bytes.append(contentsOf: block)
            }
            let policy = options.checking { try self.state.poll(self.options.checkCancelled) }
            let candidate = try decompress(bytes, options: policy); try state.poll(options.checkCancelled); staged = candidate
        }
        /// Return independent bounded bytes or nil at EOF; a zero request performs no source I/O.
        public func read(_ maxBytes: Int = 32768) throws -> [UInt8]? {
            try state.begin(); defer { state.busy = false }
            do {
                try state.poll(options.checkCancelled)
                guard maxBytes >= 0 else { throw Failure(.invalidOptions, "Negative compression read request") }
)compressswift"
R"compressswift(                if maxBytes == 0 { return [] }
                try prepare(); try state.poll(options.checkCancelled)
                guard let staged else { throw Failure(.invalidData, "Missing staged compression input") }
                if position == staged.count { return nil }
                let count = Swift.min(maxBytes, Swift.min(batchBytes, staged.count-position))
                let result = Array(staged[position..<(position+count)]); position += count
                try state.poll(options.checkCancelled); return result
            } catch { throw state.fail(error) }
        }
        /// Require exact expanded consumption; cached input and EOF still observe cancellation.
        public func finish() throws {
            try state.begin(); defer { state.busy = false }
            do {
                try state.poll(options.checkCancelled); try prepare()
                guard position == staged!.count else { throw Failure(.invalidData, "Unread expanded compression input") }
                try state.poll(options.checkCancelled); state.finished = true
            } catch { throw state.fail(error) }
        }
        /// Abandon only private staging and never close the borrowed source.
        public func close() { state.closed = true; staged = nil }
    }
    /// Read one complete borrowed source, retaining exact EOF and independent logical byte policies.
    public static func decompressFrom(_ source: @escaping (Int) throws -> [UInt8]?,
                                      options: DecodeOptions = DecodeOptions()) throws -> [UInt8] {
        let input = try Input(source: source, options: options); defer { input.close() }
        var bytes: [UInt8] = []
        while let block = try input.read() { bytes.append(contentsOf: block) }
        try input.finish(); return bytes
    }
    /// One bounded provider call reports exact progress; zero progress before completion is invalid.
    public struct Progress {
        public let consumed: Int
        public let produced: Int
        public let finished: Bool
        /// Counts describe only the supplied input slice and fixed-capacity output buffer.
        public init(consumed: Int, produced: Int, finished: Bool) {
            self.consumed = consumed; self.produced = produced; self.finished = finished
        }
    }
    /// Own one private codec context; cleanup runs once on success, failure or abandonment.
    public final class Context {
        private let step: (ArraySlice<UInt8>, inout [UInt8], Bool) throws -> Progress
        private var cleanup: (() -> Void)?
        private var busy = false
        private var closePending = false
        private var first: (any Swift.Error)?
        /// The step must not retain borrowed buffers, resize output, or share mutable codec state.
        public init(step: @escaping (ArraySlice<UInt8>, inout [UInt8], Bool) throws -> Progress,
                    close: @escaping () -> Void) {
            self.step = step; cleanup = close
        }
        /// Release only private provider resources; cleanup is nonthrowing and never publishes bytes.
        fileprivate func close() {
            if busy { closePending = true; return }
            let action = cleanup; cleanup = nil; closePending = false; action?()
        }
        /// Refuse reused/overlapping contexts before entering potentially freed native state.
        fileprivate func process(_ input: ArraySlice<UInt8>, _ output: inout [UInt8], _ finish: Bool) throws -> Progress {
            if busy {
                let error = first ?? Failure(.invalidOptions, "Reentrant compression provider context")
                first = error; throw error
            }
            guard cleanup != nil, first == nil else { throw Failure(.invalidOptions, "Inactive compression provider context", cause: first) }
            busy = true
            // A nested failed caller may request close while this step still owns native buffers.
            defer { busy = false; if closePending { close() } }
            do {
                let progress = try step(input, &output, finish)
                if let first { throw first }; return progress
            } catch {
                if first == nil { first = error }; throw first!
            }
        }
        /// Abandoned contexts also release their private resources exactly once.
        deinit { close() }
    }
    /// Explicit immutable capability factories; each call must create a fresh exclusive context.
    public struct Provider {
        public let format: Format
        public let minimumLevel: Int
        public let maximumLevel: Int
        public let defaultLevel: Int
        fileprivate let encoder: (Int, Int) throws -> Context
        fileprivate let decoder: (Int) throws -> Context
        /// Encoding gets level and pledged size; decoding gets an admitted power-of-two window log.
        public init(format: Format, minimumLevel: Int, maximumLevel: Int, defaultLevel: Int,
                    makeEncoder: @escaping (Int, Int) throws -> Context,
                    makeDecoder: @escaping (Int) throws -> Context) {
            self.format = format; self.minimumLevel = minimumLevel; self.maximumLevel = maximumLevel
            self.defaultLevel = defaultLevel; encoder = makeEncoder; decoder = makeDecoder
        }
    }
    /// Describe framing before allocating a native context; a nil size means an unknown content length.
    private struct OptionalFrame {
        let size: UInt64?
        let windowLog: Int
        let checksum: Bool
    }
    /// Read up to eight standard little-endian metadata bytes after validating their complete extent.
    private static func optionalWord(_ input: [UInt8], _ start: Int, _ count: Int) throws -> UInt64 {
        try span(input, start, count); var value: UInt64 = 0
        for index in 0..<count { value |= UInt64(input[start+index]) << (index*8) }
        return value
    }
    /// Validate standard Zstandard frame extent/dictionary/window metadata before provider allocation.
    private static func zstdFrame(_ input: [UInt8], _ options: DecodeOptions) throws -> OptionalFrame {
        try span(input, 0, 5)
        guard try optionalWord(input, 0, 4) == 0xfd2fb528 else { throw Failure(.invalidData, "Invalid Zstandard frame") }
        let descriptor = Int(input[4])
        guard descriptor & 8 == 0 else { throw Failure(.invalidData, "Reserved Zstandard descriptor") }
        let single = descriptor & 32 != 0; let sizeFlag = descriptor >> 6
        let dictionaryFlag = descriptor & 3
        let dictionaryBytes = dictionaryFlag == 3 ? 4 : dictionaryFlag
        let contentBytes = sizeFlag == 0 ? (single ? 1 : 0) : 1 << sizeFlag
        var position = 5; var window: UInt64 = 0
        if !single {
            try span(input, position, 1); let value = Int(input[position]); position += 1
            let base = UInt64(1) << (10+(value >> 3)); window = base+(base >> 3)*UInt64(value & 7)
        }
        guard try optionalWord(input, position, dictionaryBytes) == 0 else { throw Failure(.invalidData, "External Zstandard dictionary") }
        position += dictionaryBytes
        var size: UInt64?
        if contentBytes != 0 {
            let stored = try optionalWord(input, position, contentBytes)
            size = stored+(contentBytes == 2 ? 256 : 0); position += contentBytes
            guard size! <= UInt64(options.maxOutputBytes) else { throw Failure(.resourceLimit, "Zstandard output exceeds limit") }
            if single { window = size! }
        }
        let log = UInt64.bitWidth-1-UInt64(options.maxWindowBytes).leadingZeroBitCount
        guard log >= 10, window <= UInt64(1) << log else { throw Failure(.resourceLimit, "Zstandard window exceeds limit") }
        let maximumBlock = Swift.min(window, UInt64(128*1024)); var last = false
        while !last {
            try options.checkCancelled?(); let block = try optionalWord(input, position, 3); position += 3
            last = block & 1 != 0; let kind = (block >> 1) & 3; let count = block >> 3
            guard kind != 3, count <= maximumBlock else { throw Failure(.invalidData, "Invalid Zstandard block") }
            let stored = kind == 1 ? 1 : Int(count); try span(input, position, stored); position += stored
        }
        let checksum = descriptor & 4 != 0
        try span(input, position, checksum ? 4 : 0); position += checksum ? 4 : 0
        guard position == input.count else { throw Failure(.invalidData, "Trailing Zstandard data") }
        return OptionalFrame(size: size, windowLog: log, checksum: checksum)
    }
    /// Establish one LZ4 frame extent and admit its declared block maximum before native workspace.
    private static func lz4Frame(_ input: [UInt8], _ options: DecodeOptions) throws -> OptionalFrame {
        try span(input, 0, 7)
        guard try optionalWord(input, 0, 4) == 0x184d2204 else { throw Failure(.invalidData, "Invalid LZ4 frame") }
        let flags = Int(input[4]); let descriptor = Int(input[5]); let blockCode = (descriptor >> 4) & 7
        guard flags >> 6 == 1, flags & 2 == 0, descriptor & 0x8f == 0, blockCode >= 4 else { throw Failure(.invalidData, "Invalid LZ4 descriptor") }
        let blockBytes = 1 << (8+blockCode*2)
        guard blockBytes <= options.maxWindowBytes else { throw Failure(.resourceLimit, "LZ4 block exceeds window limit") }
        var position = 6; var size: UInt64?
        if flags & 8 != 0 {
            size = try optionalWord(input, position, 8); position += 8
            guard size! <= UInt64(options.maxOutputBytes) else { throw Failure(.resourceLimit, "LZ4 output exceeds limit") }
        }
        if flags & 1 != 0 {
            guard try optionalWord(input, position, 4) == 0 else { throw Failure(.invalidData, "External LZ4 dictionary") }; position += 4
        }
        try span(input, position, 1); position += 1
        while true {
            try options.checkCancelled?(); let word = try optionalWord(input, position, 4); position += 4
            if word == 0 { break }
            let count = Int(word & 0x7fffffff)
            guard count <= blockBytes else { throw Failure(.invalidData, "LZ4 block exceeds declared maximum") }
            try span(input, position, count); position += count
            if flags & 16 != 0 { try span(input, position, 4); position += 4 }
        }
        let checksum = flags & 4 != 0
        try span(input, position, checksum ? 4 : 0); position += checksum ? 4 : 0
        guard position == input.count else { throw Failure(.invalidData, "Trailing LZ4 data") }
        return OptionalFrame(size: size, windowLog: 0, checksum: checksum)
    }
    /// Check outer framing independently of provider read-ahead; providers still validate native checksums.
    private static func optionalFrame(_ input: [UInt8], _ options: DecodeOptions) throws -> OptionalFrame {
        return try options.format == .zstd ? zstdFrame(input, options) : lz4Frame(input, options)
    }
    /// Drive one exclusive context with bounded buffers and validate progress before later callbacks.
    private static func transform(_ input: [UInt8], _ context: Context, _ maximum: Int,
                                  _ check: (() throws -> Void)?) throws -> [UInt8] {
        defer { context.close() }
        var output: [UInt8] = []; var scratch = [UInt8](repeating: 0, count: batchBytes); var position = 0
        while true {
            try check?(); let count = Swift.min(batchBytes, input.count-position)
            let progress = try context.process(input[position..<(position+count)], &scratch, position+count == input.count)
            guard scratch.count == batchBytes, progress.consumed >= 0, progress.consumed <= count,
                  progress.produced >= 0, progress.produced <= batchBytes,
                  progress.finished || progress.consumed != 0 || progress.produced != 0 else {
                throw Failure(.invalidData, "Invalid compression provider progress")
            }
)compressswift"
R"compressswift(            guard progress.produced <= maximum-output.count else { throw Failure(.resourceLimit, "Compression output exceeds limit") }
            position += progress.consumed
            if progress.finished && position != input.count { throw Failure(.invalidData, "Compression provider stopped before EOF") }
            try check?(); output.append(contentsOf: scratch.prefix(progress.produced)); try check?()
            if progress.finished { return output }
        }
    }
    /// Validate an optional provider's complete standard frame before returning owning storage.
    private static func optionalCompress(_ input: [UInt8], _ options: EncodeOptions) throws -> [UInt8] {
        let provider = options.provider!; try options.checkCancelled?()
        let context = try provider.encoder(options.level, input.count)
        let packed = try transform(input, context, options.maxOutputBytes, options.checkCancelled)
        let policy = DecodeOptions(format: options.format, provider: provider, maxInputBytes: packed.count,
                                   maxOutputBytes: input.count, maxWindowBytes: Int.max, checkCancelled: options.checkCancelled)
        let frame = try optionalFrame(packed, policy)
        guard frame.checksum, frame.size == nil || frame.size == UInt64(input.count) else { throw Failure(.invalidData, "Invalid optional provider output") }
        try options.checkCancelled?(); return packed
    }
    /// Admit optional metadata before context creation, then require matching expanded content size.
    private static func optionalDecompress(_ input: [UInt8], _ options: DecodeOptions) throws -> [UInt8] {
        let frame = try optionalFrame(input, options); try options.checkCancelled?()
        let context = try options.provider!.decoder(frame.windowLog)
        let bytes = try transform(input, context, options.maxOutputBytes, options.checkCancelled)
        guard frame.size == nil || frame.size == UInt64(bytes.count) else { throw Failure(.invalidData, "Expanded content size differs") }
        try options.checkCancelled?(); return bytes
    }
}
)compressswift"
