// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"swattributes(
// Recover byte-exact attribute identities that Swift's canonically equivalent Dictionary keys merge.
// The SAX provider still validates XML syntax; this cursor retains only the input and active value.
fileprivate final class SrlXmlLexicalAttributes {
    private let bytes: [UInt8]
    private var offset = 0
    // Share immutable source storage through Swift's copy-on-write array lifetime.
    init(_ bytes: [UInt8]) { self.bytes = bytes }
    // Test a small XML delimiter without indexing beyond the admitted message.
    private func matches(_ marker: [UInt8]) -> Bool {
        marker.count <= bytes.count - offset && bytes[offset..<(offset + marker.count)].elementsEqual(marker)
    }
    // Scan comments, CDATA, processing instructions and completed closing tags exactly once.
    private func skip(_ marker: [UInt8]) throws {
        while offset < bytes.count {
            if matches(marker) { offset += marker.count; return }
            offset += 1
        }
        throw SerializerError.invalid("Incomplete XML lexical delimiter")
    }
    // Recognize only the four XML whitespace bytes while preserving referenced whitespace.
    private func whitespace(_ byte: UInt8) -> Bool { byte == 32 || byte == 9 || byte == 10 || byte == 13 }
    // Skip grammar whitespace between an attribute name, equals sign and quoted value.
    private func space() { while offset < bytes.count && whitespace(bytes[offset]) { offset += 1 } }
    // Read one provider-validated UTF8 QName without Swift canonical-equivalence comparisons.
    private func name() throws -> String {
        let begin = offset
        while offset < bytes.count && !whitespace(bytes[offset]) && bytes[offset] != 61 && bytes[offset] != 47 && bytes[offset] != 62 { offset += 1 }
        guard offset > begin, let value = String(bytes: bytes[begin..<offset], encoding: .utf8) else { throw SerializerError.invalid("Invalid XML lexical name") }
        return value
    }
    // Normalize physical attribute whitespace and decode the five predefined or numeric references.
    private func value(_ begin: Int, _ end: Int, _ maximum: Int, _ limits: XmlLimits) throws -> String {
        var result: [UInt8] = []
        var position = begin
        // Bound each decoded contribution before expanding its active native value buffer.
        func append(_ decoded: UInt8) throws {
            guard result.count < maximum else { throw srlXmlLimitFailure(limits,"XML attribute text limit") }
            result.append(decoded)
        }
        while position < end {
            let byte = bytes[position]; position += 1
            if byte == 38 {
                let reference = position
                while position < end && bytes[position] != 59 { position += 1 }
                guard position < end else { throw SerializerError.invalid("Unterminated XML attribute reference") }
                let finish = position; position += 1
                if reference < finish && bytes[reference] == 35 {
                    var digit = reference + 1
                    let hexadecimal = digit < finish && bytes[digit] == 120
                    if hexadecimal { digit += 1 }
                    guard digit < finish else { throw SerializerError.invalid("Empty XML character reference") }
                    let radix: UInt32 = hexadecimal ? 16 : 10
                    var scalar: UInt32 = 0
                    while digit < finish {
                        let current = bytes[digit]; digit += 1
                        let number: UInt32
                        if current >= 48 && current <= 57 { number = UInt32(current - 48) }
                        else if hexadecimal && current >= 65 && current <= 70 { number = UInt32(current - 65) + 10 }
                        else if hexadecimal && current >= 97 && current <= 102 { number = UInt32(current - 97) + 10 }
                        else { throw SerializerError.invalid("Invalid XML character reference") }
                        guard number < radix && scalar <= (0x10ffff - number) / radix else { throw SerializerError.invalid("XML character reference range") }
                        scalar = scalar * radix + number
                    }
                    guard scalar == 9 || scalar == 10 || scalar == 13 || scalar >= 32 && scalar <= 0xd7ff || scalar >= 0xe000 && scalar <= 0xfffd || scalar >= 0x10000 && scalar <= 0x10ffff,
                          let decoded = UnicodeScalar(scalar) else { throw SerializerError.invalid("Illegal XML character reference") }
                    for byte in String(decoded).utf8 { try append(byte) }
                } else {
                    let entity = String(decoding: bytes[reference..<finish], as: UTF8.self)
                    let decoded: UInt8
                    switch entity { case "amp": decoded = 38; case "lt": decoded = 60; case "gt": decoded = 62; case "apos": decoded = 39; case "quot": decoded = 34; default: throw SerializerError.invalid("XML entity prohibited") }
                    try append(decoded)
                }
            } else if byte == 13 {
                if position < end && bytes[position] == 10 { position += 1 }
                try append(32)
            } else { try append(byte == 9 || byte == 10 ? 32 : byte) }
        }
        guard let text = String(bytes: result, encoding: .utf8) else { throw SerializerError.invalid("Invalid XML attribute UTF8") }
        return text
    }
    // Deliver current raw attributes, including declarations, before any expanded-name lookup.
    func next(_ qualifiedName: String?, _ budget: SrlXmlBudget, _ consume: (String, String) throws -> Void) throws {
        while offset < bytes.count {
            if bytes[offset] != 60 { offset += 1; continue }
            offset += 1
            if matches([33, 45, 45]) { offset += 3; try skip([45, 45, 62]); continue }
            if matches([33, 91, 67, 68, 65, 84, 65, 91]) { offset += 8; try skip([93, 93, 62]); continue }
            if matches([63]) { offset += 1; try skip([63, 62]); continue }
            if matches([47]) { offset += 1; try skip([62]); continue }
            let elementName = try name()
            if let expected = qualifiedName {
                guard elementName.utf8.elementsEqual(expected.utf8) else { throw SerializerError.invalid("XML provider lexical position mismatch") }
            }
            while true {
                space()
                guard offset < bytes.count else { throw SerializerError.invalid("Incomplete XML start element") }
                if bytes[offset] == 62 { offset += 1; return }
                if bytes[offset] == 47 {
                    offset += 1
                    guard offset < bytes.count && bytes[offset] == 62 else { throw SerializerError.invalid("Invalid XML empty element") }
                    offset += 1; return
                }
                let raw = try name(); space()
                guard offset < bytes.count && bytes[offset] == 61 else { throw SerializerError.invalid("Missing XML attribute equals") }
                offset += 1; space()
                guard offset < bytes.count && (bytes[offset] == 34 || bytes[offset] == 39) else { throw SerializerError.invalid("Missing XML attribute quote") }
                let quote = bytes[offset]; offset += 1; let begin = offset
                while offset < bytes.count && bytes[offset] != quote { offset += 1 }
                guard offset < bytes.count else { throw SerializerError.invalid("Incomplete XML attribute") }
                let finish = offset; offset += 1
                let maximum = min(budget.limits.maxTextBytes - budget.textBytes, budget.limits.maxBytes - budget.textBytes)
                try consume(raw, value(begin, finish, maximum, budget.limits))
            }
        }
        throw SerializerError.invalid("Missing XML lexical start element")
    }
}
)swattributes"
