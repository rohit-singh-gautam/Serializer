// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// Recognize XML 1.0 NCName starts independently of Foundation character classes.
fileprivate func srlXmlNameStart(_ value: UInt32) -> Bool {
    value == 95 || (65...90).contains(value) || (97...122).contains(value) ||
        (0xc0...0xd6).contains(value) || (0xd8...0xf6).contains(value) || (0xf8...0x2ff).contains(value) ||
        (0x370...0x37d).contains(value) || (0x37f...0x1fff).contains(value) || (0x200c...0x200d).contains(value) ||
        (0x2070...0x218f).contains(value) || (0x2c00...0x2fef).contains(value) || (0x3001...0xd7ff).contains(value) ||
        (0xf900...0xfdcf).contains(value) || (0xfdf0...0xfffd).contains(value) || (0x10000...0xeffff).contains(value)
}
// Reject empty, malformed or colon-containing NCNames using exact Unicode scalar ranges.
fileprivate func srlXmlNcName(_ text: String) -> Bool {
    var first = true
    for scalar in text.unicodeScalars {
        let code = scalar.value
        if !srlXmlNameStart(code) && (first || !(code == 45 || code == 46 || code == 0xb7 || (48...57).contains(code) || (0x300...0x36f).contains(code) || (0x203f...0x2040).contains(code))) { return false }
        first = false
    }
    return !first
}
// Capture only a type attribute's resolved identity while its exact source scope is active.
fileprivate func srlXmlTypeQName(_ raw: String, _ namespace: (String) -> String?) -> SrlXmlName? {
    let text = raw.trimmingCharacters(in: CharacterSet(charactersIn: " \t\r\n"))
    let parts = text.split(separator: ":", omittingEmptySubsequences: false).map(String.init)
    guard (parts.count == 1 || parts.count == 2), parts.allSatisfy(srlXmlNcName) else { return nil }
    let prefix = parts.count == 2 ? parts[0] : ""
    let uri = prefix == "xml" ? "http://www.w3.org/XML/1998/namespace" : namespace(prefix)
    guard prefix.isEmpty || !(uri ?? "").isEmpty else { return nil }
    return SrlXmlName(local: parts.last!, namespace: uri ?? "")
}
// Consume only validated variant type metadata; ordinary unknown attributes remain visible.
fileprivate func srlXmlVariantType(_ attributes: [SrlXmlAttribute], _ required: Bool) throws -> (identity: SrlXmlName?, attributes: [SrlXmlAttribute]) {
    let typeName = SrlXmlName(local: "type", namespace: "http://www.w3.org/2001/XMLSchema-instance")
    var identity: SrlXmlName? = nil; var remaining: [SrlXmlAttribute] = []
    for attribute in attributes {
        if attribute.name == typeName {
            guard identity == nil, let resolved = attribute.typeIdentity else { throw SerializerError.invalid("Malformed XML variant type") }
            identity = resolved
        } else { remaining.append(attribute) }
    }
    guard !required || identity != nil else { throw SerializerError.invalid("Missing XML variant type") }
    return (identity, remaining)
}
)serializer"
