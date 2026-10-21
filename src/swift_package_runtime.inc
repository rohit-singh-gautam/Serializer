// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packageswift(
/// Bounded owning ZIP operations; entry names are logical metadata, never filesystem paths.
public enum @package_name@ {
    public enum Code: Swift.Sendable { case invalidOptions, invalidState, invalidArchive, unsupportedFeature, invalidName, duplicateName, integrity, resourceLimit, missingPart, codec, deadlineExceeded, invalidMetadata, invalidRelationship, semantic }
    /// Stable failure categories and bounded locations omit payload excerpts.
    public final class Failure: Swift.Error {
        public let code: Code
        public let message: Swift.String
        public let part: Swift.String
        public let offsetBytes: Swift.Int
        public let fieldPath: Swift.String
        public let reference: Swift.String
        public let relationshipId: Swift.String
        /// Retain structured context without borrowing a caller buffer.
        public init(_ code: Code, _ message: Swift.String, part: Swift.String = "", offsetBytes: Swift.Int = 0, fieldPath: Swift.String = "", reference: Swift.String = "", relationshipId: Swift.String = "") {
            self.code = code; self.message = message; self.part = part; self.offsetBytes = offsetBytes
            self.fieldPath = fieldPath; self.reference = reference; self.relationshipId = relationshipId
        }
    }
    public struct Limits {
        public var maxArchiveBytes = 256*1024*1024
        public var maxIoBytes = 1024*1024*1024
        public var maxMetadataBytes = 16*1024*1024
        public var maxPartBytes = 64*1024*1024
        public var maxTotalExpandedBytes = 256*1024*1024
        public var maxBufferBytes = 128*1024*1024
        public var maxEntries = 4096
        public var maxDecoderAllocationBytes = 128*1024*1024
        public var maxWorkUnits = 512*1024*1024
        public var maxRelationships = 16384
        public var maxReferences = 65536
        public var maxPreservedBytes = 64*1024*1024
        public var forceZip64Entries = false
        public var forceZip64 = false
        public var descriptor = false
        public var allowSpool = false
        public var maxSpoolBytes = 256*1024*1024
        public var spoolDirectory: Foundation.URL?
        public var checkCancelled: (() throws -> Swift.Void)?
        public var deadlineUptimeNanoseconds: Swift.UInt64?
        /// Construct a provider-free policy without accessing files or callbacks.
        public init() {}
    }
    public enum Resource: Swift.Int, Swift.CaseIterable { case ioBytes, metadataBytes, expandedBytes, entries, decoderAllocationBytes, workUnits, relationships, references, preservedBytes }
    /// One sequential cumulative ledger; limits are copied and callback failures remain sticky.
    public final class Budget {
        public let configured: Limits
        private var used = [Swift.Int](repeating: 0, count: Resource.allCases.count)
        fileprivate var held = 0
        private var checking = false
        private var first: (any Swift.Error)?
        /// Validate every limit before invoking application policy or touching a source.
        public init(_ limits: Limits = Limits()) throws {
            configured = limits
            guard [limits.maxArchiveBytes, limits.maxIoBytes, limits.maxMetadataBytes, limits.maxPartBytes,
                limits.maxTotalExpandedBytes, limits.maxBufferBytes, limits.maxEntries,
                limits.maxDecoderAllocationBytes, limits.maxWorkUnits, limits.maxSpoolBytes,
                limits.maxRelationships, limits.maxReferences, limits.maxPreservedBytes].allSatisfy({ $0 >= 0 }) else {
                throw Failure(.invalidOptions, "Negative package limit")
            }
            try check()
        }
        /// Inspect remaining admission without calling application code.
        public func remaining(_ resource: Resource) -> Swift.Int { maximum(resource)-used[resource.rawValue] }
        /// Derive ceilings from the immutable policy snapshot.
        private func maximum(_ resource: Resource) -> Swift.Int {
            switch resource {
            case .ioBytes: return configured.maxIoBytes
            case .metadataBytes: return configured.maxMetadataBytes
            case .expandedBytes: return configured.maxTotalExpandedBytes
            case .entries: return configured.maxEntries
            case .decoderAllocationBytes: return configured.maxDecoderAllocationBytes
            case .workUnits: return configured.maxWorkUnits
            case .relationships: return configured.maxRelationships
            case .references: return configured.maxReferences
            case .preservedBytes: return configured.maxPreservedBytes
            }
        }
        /// Preserve cancellation and deadline errors; recursive callbacks cannot relax admission.
        public func check() throws {
            if let first { throw first }
            if checking { let error = Failure(.invalidState, "Reentrant package policy"); first = error; throw error }
            checking = true
            defer { checking = false }
            do {
                try configured.checkCancelled?()
                if let first { throw first }
                if let deadline = configured.deadlineUptimeNanoseconds, DispatchTime.now().uptimeNanoseconds >= deadline {
                    throw Failure(.deadlineExceeded, "Package deadline exceeded")
                }
            } catch { if first == nil { first = error }; throw error }
        }
        /// Charge before work/allocation; failed admission leaves the counter unchanged.
        public func consume(_ resource: Resource, _ amount: Swift.Int, part: Swift.String = "") throws {
            try check()
            guard amount >= 0, amount <= remaining(resource) else { throw Failure(.resourceLimit, "Package resource limit exceeded", part: part) }
            used[resource.rawValue] += amount
        }
        /// Admit logical simultaneous owning storage, independent of allocator spare capacity.
        fileprivate func hold(_ amount: Swift.Int) throws {
            try check()
            guard amount >= 0, amount <= configured.maxBufferBytes-held else { throw Failure(.resourceLimit, "Package buffer limit exceeded") }
            held += amount
        }
        /// Release only storage owned by a completed or abandoned operation.
        fileprivate func release(_ amount: Swift.Int) { held -= amount }
    }
    /// A synchronous random-access source is borrowed only for open; exact read counts are required.
    public protocol Source {
        var size: Swift.Int { get }
        func read(offset: Swift.Int, count: Swift.Int) throws -> [Swift.UInt8]
    }
    /// A sink accepts complete borrowed chunks; write failure may leave a prefix in caller storage.
    public protocol Sink { func write(_ bytes: [Swift.UInt8]) throws }
    /// Borrow a nonseekable transport; empty means EOF and oversized chunks reject.
    public protocol SequentialSource { func read(maximumBytes: Swift.Int) throws -> [Swift.UInt8] }
    public struct MemorySource: Source {
        private let bytes: [Swift.UInt8]
        public var size: Swift.Int { bytes.count }
        /// Snapshot by Swift value semantics; subsequent caller mutations cannot affect reads.
        public init(_ bytes: [Swift.UInt8]) { self.bytes = bytes }
        /// Check subtraction bounds before creating an independent logical slice.
        public func read(offset: Swift.Int, count: Swift.Int) throws -> [Swift.UInt8] {
            try span(bytes, offset, count); return Swift.Array(bytes[offset..<(offset+count)])
        }
    }
    public enum Compression: Swift.UInt16 { case stored = 0, deflate = 8 }
    public struct Entry {
        public let name: Swift.String
        public let compression: Compression
        public let crc32: Swift.UInt32
        public let compressedBytes: Swift.Int
        public let expandedBytes: Swift.Int
        public let localOffsetBytes: Swift.Int
        public let zip64: Swift.Bool
        fileprivate let payload: Swift.Int
        fileprivate let physicalFile: Swift.Bool
    }
    private static let uint32Maximum = Swift.UInt64(Swift.UInt32.max)
    private static let uint16Maximum = Swift.Int(Swift.UInt16.max)
    private static let chunkBytes = 32768
    /// Validate every range before indexing or subtracting an untrusted length.
    private static func span(_ bytes: [Swift.UInt8], _ offset: Swift.Int, _ count: Swift.Int) throws {
        guard offset >= 0, count >= 0, offset <= bytes.count, count <= bytes.count-offset else {
            throw Failure(.invalidArchive, "ZIP range exceeds source", offsetBytes: offset)
        }
    }
    /// Read explicit little-endian wire integers without native-layout casts.
    private static func number(_ bytes: [Swift.UInt8], _ offset: Swift.Int, _ width: Swift.Int) throws -> Swift.UInt64 {
        try span(bytes, offset, width); var result: Swift.UInt64 = 0
        for index in 0..<width { result |= Swift.UInt64(bytes[offset+index]) << (index*8) }
        return result
    }
)packageswift"
R"packageswift(    /// Reject nonrepresentable unsigned extents before narrowing to native indices.
    private static func extent(_ value: Swift.UInt64) throws -> Swift.Int {
        guard value <= Swift.UInt64(Swift.Int.max) else { throw Failure(.resourceLimit, "ZIP extent exceeds native range") }
        return Swift.Int(value)
    }
    /// Keep the physical ZIP namespace independent of extraction or OPC URI normalization.
    private static func physicalName(_ value: Swift.String, opc: Swift.Bool = false) throws {
        let encoded = Swift.Array(value.utf8)
        guard !encoded.isEmpty, encoded.count <= uint16Maximum, !value.hasPrefix("/"), (opc || !value.hasSuffix("/")),
            !value.contains("\\"), (opc || !value.contains(":")), !value.unicodeScalars.contains(where: { $0.value < 32 || $0.value == 127 }),
            !(opc && value.hasSuffix("/") ? Swift.String(value.dropLast()) : value).split(separator: "/", omittingEmptySubsequences: false).contains(where: { $0.isEmpty || $0 == "." || $0 == ".." }) else {
            throw Failure(.invalidName, "Invalid physical ZIP name")
        }
    }
    /// Bound checksum work and cancellation latency without retaining an alternate payload.
    private static func checksum(_ bytes: [Swift.UInt8], _ budget: Budget) throws -> Swift.UInt32 {
        try budget.consume(.workUnits, bytes.count); var result: Swift.UInt32 = .max
        for (index, byte) in bytes.enumerated() {
            if index % chunkBytes == 0 { try budget.check() }
            result ^= Swift.UInt32(byte)
            for _ in 0..<8 { result = (result >> 1) ^ (result & 1 == 0 ? 0 : 0xedb88320) }
        }
        return ~result
    }

    /// Own one admitted archive; source lifetime ends before the reader is published.
    public final class ZipReader {
        public let entries: [Entry]
        public let budget: Budget
        private var bytes: [Swift.UInt8]
        private var closed = false
        private var busy = false
        private var reserved: Swift.Int
        /// Retain only a complete validated directory and independently owned source snapshot.
        private init(_ bytes: [Swift.UInt8], _ entries: [Entry], _ budget: Budget) {
            self.bytes = bytes; self.entries = entries; self.budget = budget; reserved = bytes.count
        }
        /// Release logical retained archive admission when its owning lifetime ends.
        deinit { budget.release(reserved) }
        /// Open an in-memory value through the same checked source boundary.
        public static func open(_ bytes: [Swift.UInt8], budget: Budget? = nil) throws -> ZipReader {
            try open(MemorySource(bytes), budget: budget)
        }
        /// Read bounded exact chunks; malformed transport counts publish no reader.
        public static func open(_ source: any Source, budget selected: Budget? = nil) throws -> ZipReader {
            try openPolicy(source,budget:selected,opc:false)
        }
        /// Read inert OPC identifiers through an explicit package-only physical naming policy.
        public static func openOpc(_ bytes: [Swift.UInt8], budget: Budget? = nil) throws -> ZipReader { try openPolicy(MemorySource(bytes),budget:budget,opc:true) }
        /// Share transport admission while retaining ordinary ZIP path validation by default.
        private static func openPolicy(_ source: any Source, budget selected: Budget?, opc: Swift.Bool) throws -> ZipReader {
            let budget = try selected ?? Budget(); try budget.check(); let size = source.size
            guard size >= 0, size <= budget.configured.maxArchiveBytes else { throw Failure(.resourceLimit, "ZIP source exceeds limit") }
            try budget.hold(size); var retained = true
            defer { if retained { budget.release(size) } }
            try budget.consume(.decoderAllocationBytes, size); var bytes: [Swift.UInt8] = []; bytes.reserveCapacity(size)
            while bytes.count < size {
                let count = Swift.min(chunkBytes, size-bytes.count)
                try budget.hold(count); defer { budget.release(count) }
                try budget.consume(.decoderAllocationBytes,count)
                try budget.consume(.ioBytes, count); let chunk = try source.read(offset: bytes.count, count: count)
                try budget.check(); guard chunk.count == count else { throw Failure(.invalidArchive, "Short or oversized ZIP source read") }
                bytes.append(contentsOf: chunk)
            }
            let entries = try directory(bytes, budget, opc:opc); try budget.check()
            let result = ZipReader(bytes, entries, budget); retained = false; return result
        }
        /// Spool a borrowed unknown-length source into an owned private temporary directory.
        public static func open(_ source: any SequentialSource, budget selected: Budget? = nil) throws -> ZipReader {
            let budget = try selected ?? Budget(); try budget.check()
            guard budget.configured.allowSpool else { throw Failure(.unsupportedFeature, "Nonseekable ZIP spooling is disabled") }
            let manager = Foundation.FileManager.default
            let parent = budget.configured.spoolDirectory ?? manager.temporaryDirectory
            let directory = parent.appendingPathComponent("serializer-package-"+Foundation.UUID().uuidString, isDirectory: true)
            try manager.createDirectory(at: directory, withIntermediateDirectories: false, attributes: [.posixPermissions: 0o700])
            defer { try? manager.removeItem(at: directory) }
            let path = directory.appendingPathComponent("archive.tmp")
            guard manager.createFile(atPath: path.path, contents: nil, attributes: [.posixPermissions: 0o600]) else { throw Failure(.invalidState, "Cannot create private ZIP spool") }
            let file = try Foundation.FileHandle(forUpdating: path); defer { try? file.close() }
            var size = 0
            while true {
                try budget.check(); let chunk = try source.read(maximumBytes: chunkBytes); try budget.check()
                guard chunk.count <= chunkBytes else { throw Failure(.invalidArchive, "Oversized sequential source read") }
                if chunk.isEmpty { break }
                guard chunk.count <= budget.configured.maxSpoolBytes-size, chunk.count <= budget.configured.maxArchiveBytes-size else { throw Failure(.resourceLimit, "ZIP spool exceeds limit") }
                try budget.consume(.ioBytes, chunk.count); try file.write(contentsOf: Foundation.Data(chunk)); size += chunk.count
            }
            try file.synchronize(); try budget.check()
            return try open(FileSource(path), budget: budget)
        }
        /// Borrow a file only for bounded positional reads during opening.
        public struct FileSource: Source {
            private let path: Foundation.URL
            public let size: Swift.Int
            /// Snapshot the file's reported extent without treating it as an extraction target.
            public init(_ path: Foundation.URL) throws {
                self.path = path
                let attributes = try Foundation.FileManager.default.attributesOfItem(atPath: path.path)
                guard let size = attributes[.size] as? Foundation.NSNumber, size.uint64Value <= Swift.UInt64(Swift.Int.max) else { throw Failure(.resourceLimit, "ZIP file extent exceeds native range") }
                self.size = size.intValue
            }
            /// Open and close only this adapter's own handle; caller transports remain untouched.
            public func read(offset: Swift.Int, count: Swift.Int) throws -> [Swift.UInt8] {
                guard offset >= 0, count >= 0, offset <= size, count <= size-offset else { throw Failure(.invalidArchive, "ZIP file read exceeds extent") }
                let handle = try Foundation.FileHandle(forReadingFrom: path); defer { try? handle.close() }
                try handle.seek(toOffset: Swift.UInt64(offset))
                return Swift.Array(try handle.read(upToCount: count) ?? Foundation.Data())
            }
        }
        /// Decode exact ZIP32 or ZIP64 directory extents before iterating any central metadata.
        private static func directory(_ bytes: [Swift.UInt8], _ budget: Budget, opc: Swift.Bool) throws -> [Entry] {
            guard bytes.count >= 22 else { throw Failure(.invalidArchive, "Missing ZIP directory") }
            var end: Swift.Int?; let lower = Swift.max(0, bytes.count-22-uint16Maximum)
            for offset in stride(from: bytes.count-22, through: lower, by: -1) {
                if try number(bytes, offset, 4) == 0x06054b50,
                    Swift.Int(try number(bytes, offset+20, 2)) == bytes.count-offset-22 { end = offset; break }
            }
            guard let end else { throw Failure(.invalidArchive, "Missing exact ZIP end record") }
            guard try number(bytes, end+4, 2) == 0, try number(bytes, end+6, 2) == 0 else { throw Failure(.unsupportedFeature, "Multi-disk ZIP is unsupported") }
            var count = Swift.Int(try number(bytes, end+10, 2)), size = try extent(number(bytes, end+12, 4)), start = try extent(number(bytes, end+16, 4))
            var directoryEnd = end
            let hasZip64Locator: Swift.Bool
            if end >= 20 { hasZip64Locator = try number(bytes, end-20, 4) == 0x07064b50 }
            else { hasZip64Locator = false }
            if Swift.UInt64(size) == uint32Maximum || Swift.UInt64(start) == uint32Maximum || hasZip64Locator {
                guard end >= 20, try number(bytes, end-20, 4) == 0x07064b50,
                    try number(bytes, end-16, 4) == 0, try number(bytes, end-4, 4) == 1 else { throw Failure(.invalidArchive, "Missing or invalid ZIP64 locator") }
                let location = try extent(number(bytes, end-12, 8)); try span(bytes, location, 56)
)packageswift"
R"packageswift(                guard location <= end-20, end-20-location >= 56, try number(bytes, location, 4) == 0x06064b50,
                    try number(bytes, location+4, 8) >= 44, try number(bytes, location+4, 8) == Swift.UInt64(end-20-location-12), try number(bytes, location+16, 4) == 0,
                    try number(bytes, location+20, 4) == 0, try number(bytes, location+24, 8) == number(bytes, location+32, 8) else { throw Failure(.invalidArchive, "Invalid ZIP64 end record") }
                let wideCount = try extent(number(bytes, location+32, 8)), wideSize = try extent(number(bytes, location+40, 8)), wideStart = try extent(number(bytes, location+48, 8))
                guard (count == uint16Maximum || count == wideCount), (Swift.UInt64(size) == uint32Maximum || size == wideSize), (Swift.UInt64(start) == uint32Maximum || start == wideStart) else { throw Failure(.invalidArchive, "ZIP32/ZIP64 directory extents differ") }
                count = wideCount; size = wideSize; start = wideStart; directoryEnd = location
            }
            let diskCount = try number(bytes, end+8, 2)
            guard diskCount == Swift.UInt64(count) || diskCount == Swift.UInt64(uint16Maximum), start <= directoryEnd, size == directoryEnd-start else { throw Failure(.invalidArchive, "Inconsistent ZIP directory extent") }
            try budget.consume(.entries, count); try budget.consume(.metadataBytes, size+bytes.count-directoryEnd)
            try budget.consume(.workUnits, size); var offset = start; var result: [Entry] = []; var names: Swift.Set<WireString> = []
            var ranges: [(Swift.Int, Swift.Int)] = []
            for _ in 0..<count {
                try budget.check(); try span(bytes, offset, 46)
                guard offset <= directoryEnd, directoryEnd-offset >= 46, try number(bytes, offset, 4) == 0x02014b50 else { throw Failure(.invalidArchive, "Invalid ZIP central record") }
                let flags = try number(bytes, offset+8, 2), method = try number(bytes, offset+10, 2)
                guard flags & ~Swift.UInt64(0x80e) == 0 else { throw Failure(.unsupportedFeature, "Encrypted or unsupported ZIP flags") }
                guard let compression = Compression(rawValue: Swift.UInt16(method)) else { throw Failure(.unsupportedFeature, "Unsupported ZIP compression") }
                guard compression == .deflate || flags & 6 == 0 else { throw Failure(.invalidArchive,"DEFLATE flags on stored entry") }
                let nameLength = Swift.Int(try number(bytes, offset+28, 2)), extraLength = Swift.Int(try number(bytes, offset+30, 2)), commentLength = Swift.Int(try number(bytes, offset+32, 2))
                let recordLength = 46+nameLength+extraLength+commentLength
                guard recordLength <= 65535, recordLength <= directoryEnd-offset else { throw Failure(.invalidArchive, "ZIP central variable fields exceed directory") }
                let nameBytes = Swift.Array(bytes[(offset+46)..<(offset+46+nameLength)])
                guard let name = Swift.String(bytes: nameBytes, encoding: .utf8), flags & 0x800 != 0 || nameBytes.allSatisfy({ $0 < 128 }) else { throw Failure(.unsupportedFeature, "Non-UTF8 ZIP names are unsupported") }
                try physicalName(name,opc:opc); guard names.insert(WireString(name)).inserted else { throw Failure(.duplicateName, "Duplicate ZIP item", part: name) }
                guard try number(bytes, offset+34, 2) == 0 else { throw Failure(.unsupportedFeature, "Multi-disk ZIP entry") }
                let checksum = Swift.UInt32(try number(bytes, offset+16, 4))
                var compressed = try number(bytes, offset+20, 4), expanded = try number(bytes, offset+24, 4), local = try number(bytes, offset+42, 4)
                let wide = compressed == uint32Maximum || expanded == uint32Maximum || local == uint32Maximum
                let extraStart = offset+46+nameLength, extraEnd = extraStart+extraLength
                var extra = extraStart; var found = false
                while extra < extraEnd {
                    guard extraEnd-extra >= 4 else { throw Failure(.invalidArchive, "Truncated ZIP extra field") }
                    let identifier = try number(bytes, extra, 2), length = Swift.Int(try number(bytes, extra+2, 2)); extra += 4
                    guard length <= extraEnd-extra else { throw Failure(.invalidArchive, "ZIP extra field exceeds record") }
                    if identifier == 1 {
                        guard !found else { throw Failure(.invalidArchive, "Duplicate ZIP64 extra field") }; found = true
                        var cursor = extra
                        for field in 0..<3 {
                            if (field == 0 ? expanded : field == 1 ? compressed : local) != uint32Maximum { continue }
                            guard extra+length-cursor >= 8 else { throw Failure(.invalidArchive, "Missing ZIP64 entry extent") }
                            let value = try number(bytes, cursor, 8); cursor += 8
                            if field == 0 { expanded = value } else if field == 1 { compressed = value } else { local = value }
                        }
                        guard cursor == extra+length else { throw Failure(.invalidArchive, "Unexpected ZIP64 entry fields") }
                    } else if identifier == 0x7075 || identifier == 0x6375 || identifier == 0x9901 {
                        throw Failure(.unsupportedFeature, "Name-altering or encrypted ZIP extra field")
                    }
                    extra += length
                }
                guard !wide || found else { throw Failure(.invalidArchive, "Missing ZIP64 entry metadata") }
                let compressedSize = try extent(compressed), expandedSize = try extent(expanded), localOffset = try extent(local)
                guard expandedSize <= budget.configured.maxPartBytes else { throw Failure(.resourceLimit, "ZIP part exceeds limit", part: name) }
                try span(bytes, localOffset, 30)
                guard localOffset <= start, start-localOffset >= 30, try number(bytes, localOffset, 4) == 0x04034b50,
                    try number(bytes, localOffset+4, 2) <= 45, try number(bytes, offset+6, 2) <= 45,
                    try number(bytes, localOffset+10, 2) == number(bytes, offset+12, 2),
                    try number(bytes, localOffset+12, 2) == number(bytes, offset+14, 2),
                    try number(bytes, localOffset+6, 2) == flags, try number(bytes, localOffset+8, 2) == method else { throw Failure(.invalidArchive, "Local/central ZIP metadata differs", part: name) }
                let localName = Swift.Int(try number(bytes, localOffset+26, 2)), localExtra = Swift.Int(try number(bytes, localOffset+28, 2))
                guard localName == nameLength, localName+localExtra <= start-localOffset-30 else { throw Failure(.invalidArchive, "Invalid ZIP local variable fields") }
                let payload = localOffset+30+localName+localExtra
                guard compressedSize <= start-payload, Swift.Array(bytes[(localOffset+30)..<(localOffset+30+localName)]) == nameBytes else { throw Failure(.invalidArchive, "ZIP payload extent or name differs") }
                var localCompressed = try number(bytes, localOffset+18, 4), localExpanded = try number(bytes, localOffset+22, 4)
                var localCursor = localOffset+30+localName; var localWide = false
                while localCursor < payload {
                    guard payload-localCursor >= 4 else { throw Failure(.invalidArchive, "Truncated local ZIP extra") }
                    let identifier = try number(bytes, localCursor, 2), length = Swift.Int(try number(bytes, localCursor+2, 2)); localCursor += 4
                    guard length <= payload-localCursor else { throw Failure(.invalidArchive, "Local ZIP extra exceeds header") }
                    if identifier == 1 {
                        guard !localWide else { throw Failure(.invalidArchive, "Duplicate local ZIP64 metadata") }; localWide = true; var cursor = localCursor
                        if localExpanded == uint32Maximum { guard length >= 8 else { throw Failure(.invalidArchive, "Missing ZIP64 local size") }; localExpanded = try number(bytes, cursor, 8); cursor += 8 }
                        if localCompressed == uint32Maximum { guard localCursor+length-cursor >= 8 else { throw Failure(.invalidArchive, "Missing ZIP64 local compressed size") }; localCompressed = try number(bytes, cursor, 8); cursor += 8 }
                        guard cursor == localCursor+length else { throw Failure(.invalidArchive, "Unexpected local ZIP64 metadata") }
                    } else if identifier == 0x7075 || identifier == 0x6375 || identifier == 0x9901 { throw Failure(.unsupportedFeature, "Unsupported local ZIP extra field") }
                    localCursor += length
                }
                let descriptor = flags & 8 != 0
                let localCrc = try number(bytes, localOffset+14, 4)
                guard (descriptor ? localCrc == 0 || localCrc == Swift.UInt64(checksum) : localCrc == Swift.UInt64(checksum)),
                    (descriptor ? localCompressed == 0 || localCompressed == compressed : localCompressed == compressed),
                    (descriptor ? localExpanded == 0 || localExpanded == expanded : localExpanded == expanded),
)packageswift"
R"packageswift(                    compression != .stored || compressed == expanded else { throw Failure(.invalidArchive, "Local ZIP sizes differ") }
                var payloadEnd = payload+compressedSize
                if descriptor {
                    let width = localWide || wide ? 8 : 4
                    let signature = try number(bytes, payloadEnd, 4) == 0x08074b50
                    let cursor = payloadEnd+(signature ? 4:0)
                    try span(bytes, cursor, 4+2*width)
                    guard cursor <= start, 4+2*width <= start-cursor,
                        try number(bytes, cursor, 4) == Swift.UInt64(checksum),
                        try number(bytes, cursor+4, width) == compressed,
                        try number(bytes, cursor+4+width, width) == expanded else { throw Failure(.integrity, "ZIP descriptor differs") }
                    payloadEnd = cursor+4+2*width
                }
                try budget.consume(.metadataBytes, 30+localName+localExtra, part: name)
                let creator = Swift.Int(try number(bytes,offset+5,1)), attributes = try number(bytes,offset+38,4)
                let physicalFile = !name.hasSuffix("/") && !([0,10,14].contains(creator) && attributes & 0x18 != 0) && !([3,19].contains(creator) && attributes >> 16 & 0xf000 != 0 && attributes >> 16 & 0xf000 != 0x8000)
                ranges.append((localOffset, payloadEnd)); result.append(Entry(name: name, compression: compression, crc32: checksum, compressedBytes: compressedSize, expandedBytes: expandedSize, localOffsetBytes: localOffset, zip64: wide || localWide, payload: payload, physicalFile: physicalFile)); offset += recordLength
            }
            if offset != directoryEnd {
                // Annex B requires consumption, with no cryptographic interpretation, of this optional record.
                guard directoryEnd-offset >= 6 else { throw Failure(.invalidArchive, "Truncated ZIP directory signature") }
                let signature = try number(bytes, offset, 4), signatureBytes = try number(bytes, offset+4, 2)
                guard signature == 0x05054b50, signatureBytes == Swift.UInt64(directoryEnd-offset-6) else { throw Failure(.invalidArchive, "ZIP directory count differs") }
            }
            ranges.sort { $0.0 < $1.0 }; var previous = 0
            for range in ranges { guard range.0 == previous else { throw Failure(.invalidArchive, "Overlapping, prefixed or unindexed ZIP data") }; previous = range.1 }
            guard previous == start else { throw Failure(.invalidArchive, "Unindexed ZIP local data") }
            return result
        }

        /// Return complete verified bytes; CRC, size and raw-DEFLATE trailing data must agree.
        public func read(_ name: Swift.String) throws -> [Swift.UInt8] {
            guard !closed, !busy else { closed = true; throw Failure(.invalidState, "Closed or reentrant ZIP reader") }
            busy = true; defer { busy = false }
            var expandedHeld = 0; defer { budget.release(expandedHeld) }
            do {
                try budget.check(); guard let entry = entries.first(where: { $0.name.utf8.elementsEqual(name.utf8) }) else { throw Failure(.missingPart, "Missing ZIP entry", part: name) }
                try budget.consume(.expandedBytes, entry.expandedBytes, part: name)
                try budget.consume(.decoderAllocationBytes, entry.compressedBytes, part: name); try budget.hold(entry.compressedBytes)
                defer { budget.release(entry.compressedBytes) }
                let compressed = Swift.Array(bytes[entry.payload..<(entry.payload+entry.compressedBytes)])
                var result: [Swift.UInt8]
                if entry.compression == .stored { result = compressed }
                else {
                    try budget.consume(.decoderAllocationBytes, entry.expandedBytes, part: name); try budget.hold(entry.expandedBytes)
                    expandedHeld = entry.expandedBytes
                    do { result = try @compression_name@.decompress(compressed, options: .init(format: .deflate, maxInputBytes: entry.compressedBytes, maxOutputBytes: entry.expandedBytes, checkCancelled: { try self.poll() })) }
                    catch let failure as @compression_name@.Failure { throw Failure(failure.code == .resourceLimit ? .resourceLimit : .integrity, "Invalid ZIP DEFLATE payload", part: name) }
                }
                guard result.count == entry.expandedBytes, try checksum(result, budget) == entry.crc32 else { throw Failure(.integrity, "ZIP payload size or CRC differs", part: name) }
                try poll(); return result
            } catch { closed = true; throw error }
        }
        /// Couple provider checks to the owning reader lifecycle.
        private func poll() throws { try budget.check(); guard !closed else { throw Failure(.invalidState, "Closed ZIP reader") } }
        /// Abandon owned storage without closing any borrowed source or sink.
        public func close() { closed = true; if !busy { bytes = []; budget.release(reserved); reserved = 0 } }
    }
    /// Stage complete independent entries; failed and finished writers cannot resume.
    public final class ZipWriter {
        public let budget: Budget
        private struct Part { let name: Swift.String; let bytes: [Swift.UInt8]; let compression: Compression }
        private var parts: [Part] = []
        private let opcNames: Swift.Bool
        private var names: Swift.Set<WireString> = []
        private var reserved = 0
        private var closed = false
        private var busy = false
        /// Construct without accessing a destination or emitting a partial header.
        public init(budget: Budget? = nil) throws { self.budget = try budget ?? Budget(); opcNames = false }
        /// Select inert package names without weakening public generic ZIP admission.
        fileprivate init(opcBudget budget: Budget) { self.budget = budget; opcNames = true }
        /// Release only retained private staging when the writer lifetime ends.
        deinit { budget.release(reserved) }
        /// Fence reentry and prior failures before exposing any operation to callbacks.
        private func begin() throws { guard !closed, !busy else { closed = true; throw Failure(.invalidState, "Closed or reentrant ZIP writer") }; busy = true }
        /// Keep cancellation and callback-driven close observable inside compression and output.
        private func poll() throws { try budget.check(); guard !closed else { throw Failure(.invalidState, "Closed ZIP writer") } }
        /// Admit a CoW value snapshot; caller mutations trigger independent native storage.
        public func add(_ name: Swift.String, _ bytes: [Swift.UInt8], compression: Compression = .stored) throws {
            try begin(); defer { busy = false }
            do {
                try poll(); try physicalName(name,opc:opcNames)
                guard bytes.count <= budget.configured.maxPartBytes else { throw Failure(.resourceLimit, "ZIP part exceeds limit", part: name) }
                guard !names.contains(WireString(name)) else { throw Failure(.duplicateName, "Duplicate ZIP entry", part: name) }
                try budget.consume(.entries, 1, part: name); try budget.consume(.expandedBytes, bytes.count, part: name)
                try budget.consume(.metadataBytes, name.utf8.count+128, part: name)
                try budget.consume(.decoderAllocationBytes, bytes.count, part: name); try budget.hold(bytes.count); reserved += bytes.count
                try poll(); parts.append(Part(name: name, bytes: bytes, compression: compression)); names.insert(WireString(name))
            } catch { closed = true; throw error }
        }
        /// Append after exact archive/buffer/I/O admission; every wire integer has explicit width.
        private func append(_ value: [Swift.UInt8], to output: inout [Swift.UInt8]) throws {
            try poll(); guard value.count <= budget.configured.maxArchiveBytes-output.count else { throw Failure(.resourceLimit, "ZIP archive extent exceeds limit") }
            try budget.consume(.ioBytes, value.count); try budget.hold(value.count); output.append(contentsOf: value)
        }
        /// Render an integer into a fixed wire-width temporary value.
        private func wire(_ value: Swift.UInt64, _ width: Swift.Int) -> [Swift.UInt8] { (0..<width).map { Swift.UInt8(truncatingIfNeeded: value >> ($0*8)) } }
        /// Build deterministic ZIP32 directories with independently selectable ZIP64 entry extras.
        private func prepare() throws -> [Swift.UInt8] {
            var output: [Swift.UInt8] = []; var central: [[Swift.UInt8]] = []; var centralHeld = 0
            var success = false
            defer { budget.release(centralHeld); if !success { budget.release(output.count) } }
            for part in parts.sorted(by: { $0.name.utf8.lexicographicallyPrecedes($1.name.utf8) }) {
                try poll(); let name = Swift.Array(part.name.utf8); let crc = try checksum(part.bytes, budget)
                var compressed = part.bytes
                if part.compression == .deflate {
                    let capacity = Swift.min(budget.configured.maxArchiveBytes-output.count, budget.configured.maxBufferBytes-budget.held)
                    do { compressed = try @compression_name@.compress(part.bytes, options: .init(format: .deflate, maxInputBytes: part.bytes.count, maxOutputBytes: capacity, checkCancelled: { try self.poll() })) }
                    catch let failure as @compression_name@.Failure { throw Failure(failure.code == .resourceLimit ? .resourceLimit : .codec, "ZIP compression failed", part: part.name) }
                }
                let compressedHeld = part.compression == .deflate ? compressed.count : 0
                try budget.hold(compressedHeld); defer { budget.release(compressedHeld) }
                let wide = budget.configured.forceZip64 || budget.configured.forceZip64Entries || Swift.UInt64(part.bytes.count) >= uint32Maximum || Swift.UInt64(compressed.count) >= uint32Maximum
)packageswift"
R"packageswift(                let local = output.count, wideOffset = Swift.UInt64(output.count) >= uint32Maximum
                guard name.count <= uint16Maximum-46-(wide || wideOffset ? 4+(wide ? 16:0)+(wideOffset ? 8:0):0) else { throw Failure(.invalidName,"ZIP central header exceeds OPC bound",part:part.name) }
                let descriptor = budget.configured.descriptor, flags: Swift.UInt64 = 0x800 | (descriptor ? 8:0)
                var header: [Swift.UInt8] = []
                for (value, width): (Swift.UInt64, Swift.Int) in [(0x04034b50,4),(wide ? 45:20,2),(flags,2),(Swift.UInt64(part.compression.rawValue),2),(0,2),(33,2),(descriptor ? 0:Swift.UInt64(crc),4),(wide ? uint32Maximum:descriptor ? 0:Swift.UInt64(compressed.count),4),(wide ? uint32Maximum:descriptor ? 0:Swift.UInt64(part.bytes.count),4),(Swift.UInt64(name.count),2),(wide ? 20:0,2)] { header += wire(value,width) }
                header += name
                if wide { header += wire(1,2)+wire(16,2)+wire(descriptor ? 0:Swift.UInt64(part.bytes.count),8)+wire(descriptor ? 0:Swift.UInt64(compressed.count),8) }
                try append(header, to: &output); try append(compressed, to: &output)
                if descriptor { try append(wire(0x08074b50,4)+wire(Swift.UInt64(crc),4)+wire(Swift.UInt64(compressed.count),wide ? 8:4)+wire(Swift.UInt64(part.bytes.count),wide ? 8:4), to: &output) }
                var record: [Swift.UInt8] = []
                for (value, width): (Swift.UInt64, Swift.Int) in [(0x02014b50,4),(wide || wideOffset ? 813:788,2),(wide || wideOffset ? 45:20,2),(flags,2),(Swift.UInt64(part.compression.rawValue),2),(0,2),(33,2),(Swift.UInt64(crc),4),(wide ? uint32Maximum:Swift.UInt64(compressed.count),4),(wide ? uint32Maximum:Swift.UInt64(part.bytes.count),4),(Swift.UInt64(name.count),2),(wide || wideOffset ? Swift.UInt64(4+(wide ? 16:0)+(wideOffset ? 8:0)):0,2),(0,2),(0,2),(0,2),(0x81800000,4),(wideOffset ? uint32Maximum:Swift.UInt64(local),4)] { record += wire(value,width) }
                record += name
                if wide || wideOffset { record += wire(1,2)+wire(Swift.UInt64((wide ? 16:0)+(wideOffset ? 8:0)),2); if wide { record += wire(Swift.UInt64(part.bytes.count),8)+wire(Swift.UInt64(compressed.count),8) }; if wideOffset { record += wire(Swift.UInt64(local),8) } }
                try budget.consume(.metadataBytes, record.count, part: part.name); try budget.hold(record.count); centralHeld += record.count; central.append(record)
            }
            let start = output.count
            for record in central { try append(record, to: &output) }
            let size = output.count-start
            let wide = budget.configured.forceZip64 || parts.count >= uint16Maximum || Swift.UInt64(start) >= uint32Maximum || Swift.UInt64(size) >= uint32Maximum
            if wide {
                let location = output.count
                var record: [Swift.UInt8] = []
                for (value, width): (Swift.UInt64, Swift.Int) in [(0x06064b50,4),(44,8),(45,2),(45,2),(0,4),(0,4),(Swift.UInt64(parts.count),8),(Swift.UInt64(parts.count),8),(Swift.UInt64(size),8),(Swift.UInt64(start),8),(0x07064b50,4),(0,4),(Swift.UInt64(location),8),(1,4)] { record += wire(value,width) }
                try append(record,to: &output)
            }
            var end: [Swift.UInt8] = []
            for (value, width): (Swift.UInt64, Swift.Int) in [(0x06054b50,4),(0,2),(0,2),(Swift.UInt64(wide ? uint16Maximum:parts.count),2),(Swift.UInt64(wide ? uint16Maximum:parts.count),2),(wide ? uint32Maximum:Swift.UInt64(size),4),(wide ? uint32Maximum:Swift.UInt64(start),4),(0,2)] { end += wire(value,width) }
            try append(end,to: &output); try poll(); success = true; return output
        }
        /// Publish owned bytes only after the complete directory and final policy checks succeed.
        public func finish() throws -> [Swift.UInt8] {
            try begin(); defer { busy = false }
            do { let output = try prepare(); defer { budget.release(output.count) }; try poll(); closed = true; cleanup(); return output }
            catch { closed = true; cleanup(); throw error }
        }
        /// Borrow a sink after staging; transport errors retain their original type and may leave a prefix.
        public func write(to sink: any Sink) throws {
            try begin(); defer { busy = false }
            do {
                let output = try prepare(); defer { budget.release(output.count) }
                var offset = 0
                while offset < output.count { try poll(); let count = Swift.min(chunkBytes, output.count-offset); try sink.write(Swift.Array(output[offset..<(offset+count)])); offset += count; try poll() }
                closed = true; cleanup()
            } catch { closed = true; cleanup(); throw error }
        }
        /// Drop owned input snapshots without closing caller transports or emitting a trailer.
        private func cleanup() { parts = []; names = []; budget.release(reserved); reserved = 0 }
        /// Abandon staging; a callback-driven close is observed at the next bounded poll.
        public func close() { closed = true; if !busy { cleanup() } }
        /// Stage and validate a complete ZIP before Foundation's same-directory atomic replacement.
        public func save(to path: Foundation.URL) throws {
            let bytes = try finish()
            let validator = try opcNames ? ZipReader.openOpc(bytes, budget: budget) : ZipReader.open(bytes, budget: budget); defer { validator.close() }
            for entry in validator.entries { _ = try validator.read(entry.name) }
            try budget.check()
            try Foundation.Data(bytes).write(to: path, options: .atomic)
            try budget.check()
        }
    }
}
)packageswift"
