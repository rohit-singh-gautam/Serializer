R"serializer(
import Foundation

/// Existing native wire protocols; binary scalars are little-endian.
public enum Protocol: Int, CaseIterable { case JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING }
/// Immutable per-message limits; collection entries accumulate across fields.
public struct Limits {
    public let maxBytes: Int
    public let maxStringBytes: Int
    public let maxElements: Int
    public let maxDepth: Int
    /// Construct a policy; decoding rejects any negative limits.
    public init(maxBytes: Int = 64*1024*1024, maxStringBytes: Int = 16*1024*1024, maxElements: Int = 1000000, maxDepth: Int = 64) {
        self.maxBytes = maxBytes; self.maxStringBytes = maxStringBytes; self.maxElements = maxElements; self.maxDepth = maxDepth
    }
}
/// Validation or resource failure; decoded candidates never escape on error.
public enum SerializerError: Error { case invalid(String) }
/// A map key whose identity is exact UTF-8, independent of Swift normalization equality.
public struct WireString: Hashable, Comparable, ExpressibleByStringLiteral {
    public let value: String
    /// Retain the exact supplied Unicode scalar sequence.
    public init(_ value: String) { self.value = value }
    /// Allow natural string literals in map subscripts and dictionary literals.
    public init(stringLiteral value: String) { self.value = value }
    /// Compare encoded bytes instead of canonical Unicode equivalence.
    public static func == (a: Self, b: Self) -> Bool { a.value.utf8.elementsEqual(b.value.utf8) }
    /// Preserve the native wire map ordering.
    public static func < (a: Self, b: Self) -> Bool { a.value.utf8.lexicographicallyPrecedes(b.value.utf8) }
    /// Hash the same bytes used by equality.
    public func hash(into hasher: inout Hasher) { for byte in value.utf8 { hasher.combine(byte) } }
}
private struct SrlField { let json: [UInt8]; let binary: [UInt8] }
// Encode known schema keys once at module initialization.
private func srlField(_ name: String) -> SrlField {
    let bytes = Array(name.utf8)
    // Schema keys contain only identifiers, colons, and underscores.
    var binary: [UInt8] = []
    let count = bytes.count
    let extra = count <= 63 ? 0 : count <= 16383 ? 1 : count <= 4194303 ? 2 : 3
    binary.append(UInt8(extra << 6 | count >> (extra*8)))
    if extra > 0 { for i in stride(from: extra-1, through: 0, by: -1) { binary.append(UInt8(truncatingIfNeeded: count >> (i*8))) } }
    binary.append(contentsOf: bytes)
    return SrlField(json: [34] + bytes + [34,58], binary: binary)
}
fileprivate final class SrlWriter {
    let protocolValue: Protocol
    var bytes: [UInt8] = []
    private var depth = 0
    // Reserve modest initial capacity; each encoding owns its result.
    init(_ protocolValue: Protocol) { self.protocolValue = protocolValue; bytes.reserveCapacity(256) }
    // Test the active protocol.
    func json() -> Bool { protocolValue == .JSON }
    // Append validated numeric text or punctuation.
    func raw(_ value: String) { bytes.append(contentsOf: value.utf8) }
    // Write Serializer's two-bit compact length tag.
    func compact(_ value: Int) throws {
        guard value >= 0 && value <= 0x3fffffff else { throw SerializerError.invalid("Compact integer range") }
        let extra = value <= 63 ? 0 : value <= 16383 ? 1 : value <= 4194303 ? 2 : 3
        bytes.append(UInt8(extra << 6 | value >> (extra*8)))
        if extra > 0 { for i in stride(from: extra-1, through: 0, by: -1) { bytes.append(UInt8(truncatingIfNeeded: value >> (i*8))) } }
    }
    // Preserve fixed-width integer bit patterns and exact decimal spelling.
    func integer(_ value: UInt64, _ width: Int, _ unsigned: Bool) {
        if json() {
            if unsigned { raw(String(value)) }
            else { let shift = (8-width)*8; raw(String(Int64(bitPattern: value << shift) >> shift)) }
        } else { for i in 0..<width { bytes.append(UInt8(truncatingIfNeeded: value >> (i*8))) } }
    }
    // Preserve binary32 bits; reject nonfinite JSON numbers.
    func float32(_ value: Float) throws {
        if json() { guard value.isFinite else { throw SerializerError.invalid("Nonfinite JSON number") }; raw(String(value)) }
        else { integer(UInt64(value.bitPattern), 4, true) }
    }
    // Preserve binary64 bits, including negative zero.
    func float64(_ value: Double) throws {
        if json() { guard value.isFinite else { throw SerializerError.invalid("Nonfinite JSON number") }; raw(String(value)) }
        else { integer(value.bitPattern, 8, true) }
    }
    // Encode canonical booleans.
    func boolean(_ value: Bool) { if json() { raw(value ? "true" : "false") } else { bytes.append(value ? 1 : 0) } }
    // Escape JSON controls while copying UTF-8 directly to the output buffer.
    func text(_ value: String) throws {
        if !json() { try compact(value.utf8.count); bytes.append(contentsOf: value.utf8); return }
        bytes.append(34)
        let hex = Array("0123456789abcdef".utf8)
        for byte in value.utf8 {
            if byte == 34 || byte == 92 { bytes.append(92); bytes.append(byte) }
            else if byte < 32 { bytes.append(contentsOf: [92,117,48,48,hex[Int(byte >> 4)],hex[Int(byte & 15)]]) }
            else { bytes.append(byte) }
        }
        bytes.append(34)
    }
    // Encode an ASCII JSON character or an unrestricted binary byte.
    func character(_ value: UInt8) throws {
        if json() { guard value <= 127 else { throw SerializerError.invalid("Non-ASCII JSON char") }; try text(String(UnicodeScalar(value))) }
        else { bytes.append(value) }
    }
    // Select enum names only in the native contexts that use names.
    func enumeration(_ value: Int, _ names: [String], _ collection: Bool) throws {
        guard names.indices.contains(value) else { throw SerializerError.invalid("Unknown enum") }
        if json() || (protocolValue == .BINARY_STRING && !collection) { try text(names[value]) } else { try compact(value) }
    }
    // Bound recursive object traversal before writing fields.
    func beginObject() throws { depth += 1; guard depth <= 64 else { throw SerializerError.invalid("Encode depth limit") }; if json() { bytes.append(123) } }
    // Append cached schema keys without repeating UTF-8 conversion.
    func field(_ id: Int, _ key: Int, _ first: Bool) throws {
        if json() { if !first { bytes.append(44) }; bytes.append(contentsOf: srlFields[key].json) }
        else if protocolValue == .BINARY_INTEGER { try compact(id) }
        else if protocolValue == .BINARY_STRING { bytes.append(contentsOf: srlFields[key].binary) }
    }
    // Finish protocol framing and leave the object.
    func endObject() { if json() { bytes.append(125) } else if protocolValue != .BINARY_NONE { bytes.append(0) }; depth -= 1 }
    // Start a collection with its actual element count.
    func beginArray(_ count: Int) throws { if json() { bytes.append(91) } else { try compact(count) } }
    // Separate JSON collection entries.
    func element(_ index: Int) { if json() && index != 0 { bytes.append(44) } }
    // Finish a JSON collection.
    func endArray() { if json() { bytes.append(93) } }
}
fileprivate struct SrlKey { let id: Int; let name: String? }
fileprivate final class SrlReader {
    let data: [UInt8]
    let protocolValue: Protocol
    let limits: Limits
    private var offset = 0
    private var depth = 0
    private var elements = 0
    // Share immutable array storage during a call; returned model values own their data.
    init(_ data: [UInt8], _ protocolValue: Protocol, _ limits: Limits) throws {
        guard min(limits.maxBytes, limits.maxStringBytes, limits.maxElements, limits.maxDepth) >= 0 else { throw SerializerError.invalid("Invalid limits") }
        guard data.count <= limits.maxBytes else { throw SerializerError.invalid("Message byte limit") }
        self.data = data; self.protocolValue = protocolValue; self.limits = limits
    }
    // Test the selected protocol.
    func json() -> Bool { protocolValue == .JSON }
    // Validate length before indexing or arithmetic on input slices.
    private func requireBytes(_ count: Int) throws { guard count >= 0 && count <= data.count-offset else { throw SerializerError.invalid("Truncated input") } }
    // Skip exactly JSON whitespace.
    private func whitespace() { while offset < data.count && (data[offset] == 32 || data[offset] == 9 || data[offset] == 10 || data[offset] == 13) { offset += 1 } }
    // Require one delimiter without accepting trailing commas.
    private func expect(_ byte: UInt8) throws { whitespace(); try requireBytes(1); guard data[offset] == byte else { throw SerializerError.invalid("Unexpected delimiter") }; offset += 1 }
    // Read native compact prefixes including accepted nonminimal forms.
    func compact() throws -> Int {
        try requireBytes(1); let first = data[offset]; let extra = Int(first >> 6); try requireBytes(extra+1)
        var value = Int(first & 63)
        if extra > 0 { for index in 1...extra { value = value*256 + Int(data[offset+index]) } }
        offset += extra+1; return value
    }
    // Read exact token spelling without converting through a generic JSON tree.
    private func token() throws -> String {
        whitespace(); let start = offset
        while offset < data.count && ![UInt8(44),125,93,32,9,10,13].contains(data[offset]) { offset += 1 }
        guard offset != start, let text = String(validating: data[start..<offset], as: UTF8.self) else { throw SerializerError.invalid("Invalid token") }
        return text
    }
    // Preserve all integer bits and enforce exact declared JSON ranges.
    func integer(_ width: Int, _ unsigned: Bool) throws -> UInt64 {
        if !json() { try requireBytes(width); var value: UInt64 = 0; for i in 0..<width { value |= UInt64(data[offset+i]) << (i*8) }; offset += width; return value }
        let text = try token(); guard srlNumber(text, true) else { throw SerializerError.invalid("Invalid integer") }
        if unsigned {
            guard !text.hasPrefix("-"), let value = UInt64(text), width == 8 || value <= UInt64.max >> ((8-width)*8) else { throw SerializerError.invalid("Integer range") }; return value
        }
        guard let value = Int64(text), width == 8 || (value >= -(Int64(1) << (width*8-1)) && value < Int64(1) << (width*8-1)) else { throw SerializerError.invalid("Integer range") }
        return UInt64(bitPattern: value)
    }
    // Decode binary32 and reject nonfinite or underflowed JSON numbers.
    func float32() throws -> Float {
        if !json() { return Float(bitPattern: UInt32(try integer(4, true))) }
        let text = try token(); guard srlNumber(text, false), let value = Float(text), value.isFinite, value != 0 || !srlNonzero(text) else { throw SerializerError.invalid("Floating-point range") }; return value
    }
    // Decode binary64 and reject nonfinite or underflowed JSON numbers.
    func float64() throws -> Double {
        if !json() { return Double(bitPattern: try integer(8, true)) }
        let text = try token(); guard srlNumber(text, false), let value = Double(text), value.isFinite, value != 0 || !srlNonzero(text) else { throw SerializerError.invalid("Floating-point range") }; return value
    }
    // Decode canonical boolean values.
    func boolean() throws -> Bool {
        if json() { let value = try token(); if value == "true" { return true }; if value == "false" { return false } }
        else { let value = try integer(1, true); if value <= 1 { return value == 1 } }
        throw SerializerError.invalid("Invalid boolean")
    }
    // Read one hexadecimal JSON code unit.
    private func hex() throws -> UInt32 {
        try requireBytes(4); var value: UInt32 = 0
        for _ in 0..<4 { let b = data[offset]; offset += 1; let digit: UInt8
            if b >= 48 && b <= 57 { digit = b-48 } else if b >= 65 && b <= 70 { digit = b-55 } else if b >= 97 && b <= 102 { digit = b-87 } else { throw SerializerError.invalid("Invalid Unicode escape") }
            value = value*16 + UInt32(digit)
        }; return value
    }
    // Decode strict UTF-8 without Foundation's platform-dependent leading-BOM removal.
    // JSON escapes retain both escaped and decoded size bounds.
    func text() throws -> String {
        if !json() {
            let count = try compact(); guard count <= limits.maxStringBytes else { throw SerializerError.invalid("String limit") }; try requireBytes(count)
            guard let value = String(validating: data[offset..<offset+count], as: UTF8.self) else { throw SerializerError.invalid("Invalid UTF-8") }; offset += count; return value
        }
        try expect(34); let start = offset; var bytes: [UInt8] = []
        while true {
            try requireBytes(1); let byte = data[offset]; offset += 1
            guard offset-start-1 <= limits.maxStringBytes else { throw SerializerError.invalid("String limit") }
            if byte == 34 { break }; guard byte >= 32 else { throw SerializerError.invalid("Unescaped control") }
            if byte != 92 { bytes.append(byte); continue }
            try requireBytes(1); let escaped = data[offset]; offset += 1
            switch escaped {
            case 34,92,47: bytes.append(escaped)
            case 98: bytes.append(8); case 102: bytes.append(12); case 110: bytes.append(10); case 114: bytes.append(13); case 116: bytes.append(9)
            case 117:
                var code = try hex()
                if code >= 0xd800 && code <= 0xdbff {
                    try requireBytes(2); guard data[offset] == 92 && data[offset+1] == 117 else { throw SerializerError.invalid("Invalid surrogate") }; offset += 2
                    let low = try hex(); guard low >= 0xdc00 && low <= 0xdfff else { throw SerializerError.invalid("Invalid surrogate") }
                    code = 0x10000 + ((code-0xd800) << 10) + low-0xdc00
                }
                guard let scalar = UnicodeScalar(code) else { throw SerializerError.invalid("Invalid Unicode scalar") }; bytes.append(contentsOf: String(scalar).utf8)
            default: throw SerializerError.invalid("Invalid string escape")
            }
        }
        guard bytes.count <= limits.maxStringBytes, let result = String(validating: bytes, as: UTF8.self) else { throw SerializerError.invalid("Invalid string") }; return result
    }
    // Decode an ASCII JSON character or binary byte.
    func character() throws -> UInt8 { if !json() { return UInt8(try integer(1, true)) }; let value = try text(); guard value.utf8.count == 1, let byte = value.utf8.first, byte <= 127 else { throw SerializerError.invalid("Invalid char") }; return byte }
    // Decode declared enum names and ordinals in their wire context.
    func enumeration(_ names: [String], _ collection: Bool) throws -> Int {
        let value: Int
        if json() || (protocolValue == .BINARY_STRING && !collection) { let name = try text(); value = names.firstIndex(where: { $0.utf8.elementsEqual(name.utf8) }) ?? -1 } else { value = try compact() }
        guard names.indices.contains(value) else { throw SerializerError.invalid("Unknown enum") }; return value
    }
    // Check nesting before traversing fields.
    func beginObject() throws { depth += 1; guard depth <= limits.maxDepth else { throw SerializerError.invalid("Depth limit") }; if json() { try expect(123) } }
    // Return positional IDs or keyed names while retaining duplicate occurrences.
    func key(_ ids: [Int], _ cursor: Int) throws -> SrlKey? {
        if protocolValue == .BINARY_NONE { return cursor < ids.count ? SrlKey(id: ids[cursor], name: nil) : nil }
        if protocolValue == .BINARY_INTEGER { let id = try compact(); return id == 0 ? nil : SrlKey(id: id, name: nil) }
        if json() { whitespace(); try requireBytes(1); if data[offset] == 125 { offset += 1; return nil }; if cursor != 0 { try expect(44) } }
        let name = try text(); if !json() && name.isEmpty { return nil }; if json() { try expect(58) }; return SrlKey(id: 0, name: name)
    }
    // Leave a successfully decoded object.
    func endObject() { depth -= 1 }
    // Validate binary collection lengths before allocation.
    func beginArray() throws -> Int { if json() { try expect(91); return -1 }; let count = try compact(); guard count <= limits.maxElements-elements else { throw SerializerError.invalid("Element limit") }; return count }
    // Reserve fixed-width arrays only after validating their minimum byte requirement.
    func capacity(_ count: Int, _ width: Int) throws -> Int { if count < 0 { return 0 }; if width > 0 && count > (data.count-offset)/width { throw SerializerError.invalid("Truncated array") }; return width == 0 ? min(count, 256) : count }
    // Charge collection slots before reading their values.
    func nextElement(_ index: Int, _ count: Int) throws -> Bool {
        if !json() { if index == count { return false } }
        else { whitespace(); try requireBytes(1); if data[offset] == 93 { offset += 1; return false }; if index != 0 { try expect(44) } }
        guard elements < limits.maxElements else { throw SerializerError.invalid("Element limit") }; elements += 1; return true
    }
    // Require exact consumption before exposing the owning candidate.
    func finish() throws { if json() { whitespace() }; guard offset == data.count else { throw SerializerError.invalid("Trailing input") } }
}
// Distinguish a nonzero mantissa underflow from a valid signed zero.
private func srlNonzero(_ text: String) -> Bool { text.utf8.prefix(while: { $0 != 101 && $0 != 69 }).contains(where: { $0 >= 49 && $0 <= 57 }) }
// Validate JSON numeric grammar independently of Swift's numeric parser extensions.
private func srlNumber(_ text: String, _ integer: Bool) -> Bool {
    let b = Array(text.utf8); var i = b.first == 45 ? 1 : 0
    func digit(_ index: Int) -> Bool { index < b.count && b[index] >= 48 && b[index] <= 57 }
    if i < b.count && b[i] == 48 { i += 1 } else { let start = i; while digit(i) { i += 1 }; if start == i { return false } }
    if !integer && i < b.count && b[i] == 46 { i += 1; let start = i; while digit(i) { i += 1 }; if start == i { return false } }
    if !integer && i < b.count && (b[i] == 101 || b[i] == 69) { i += 1; if i < b.count && (b[i] == 43 || b[i] == 45) { i += 1 }; let start = i; while digit(i) { i += 1 }; if start == i { return false } }
    return i == b.count
}
)serializer"
