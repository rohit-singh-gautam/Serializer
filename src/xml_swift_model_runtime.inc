// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlmodel(
// Bound each owning digest independently of document and ordered-content policies.
public struct XmlDigestLimits {
    public let maxBytes: Int
    // Preserve exact zero admission; operation entry points reject negative bounds.
    public init(maxBytes: Int = 1048576) { self.maxBytes=maxBytes }
    // Validate before any candidate or provider work begins.
    fileprivate func validate() throws { guard maxBytes >= 0 else { throw SerializerError.invalid("Negative XML digest limit") } }
}
// Keep reserved model identities independent of presentation prefixes.
fileprivate let srlXmlModelNamespace="urn:serializer:xml:model"
// Validate canonical native revision syntax before selecting a historical/current layout.
fileprivate func srlXmlRevisionLexical(_ raw: String, _ kind: String) throws -> String {
    if kind.hasPrefix("version") { _=try srlVersionParts(raw,Int(String(kind.last!))!); return raw }
    let text=raw.trimmingCharacters(in:CharacterSet(charactersIn:" \t\r\n"))
    guard text.range(of:"\\A(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\\z",options:.regularExpression) != nil else { throw SerializerError.invalid("Noncanonical XML revision") }
    if kind.hasPrefix("uint") {
        guard text.utf8.allSatisfy({ $0 >= 48 && $0 <= 57 }) else { throw SerializerError.invalid("Noncanonical XML integer revision") }
    } else {
        let value=kind == "float" ? Double(try srlXmlFloat32(text)) : try srlXmlFloat(text)
        let mantissa=text.split(whereSeparator:{ $0 == "e" || $0 == "E" })[0]
        guard value.isFinite && (value != 0 || !mantissa.utf8.contains(where:{ $0 >= 49 && $0 <= 57 })) else { throw SerializerError.invalid("XML revision overflow or underflow") }
    }
    return text
}
// Convert one exact ASCII hexadecimal digit without accepting whitespace or other scripts.
fileprivate func srlXmlHexDigit(_ value: UInt8) -> Int {
    switch value { case 48...57: return Int(value-48); case 97...102: return Int(value-97)+10; case 65...70: return Int(value-65)+10; default: return -1 }
}
// Admit the complete lexical value and extent before allocating owning bytes.
fileprivate func srlXmlDigestRead(_ text: String, _ extent: Int, _ limits: XmlDigestLimits) throws -> [UInt8] {
    let size=text.utf8.count/2
    guard text.utf8.count%2 == 0 && (extent == 0 || size == extent) else { throw SerializerError.invalid("XML digest extent or hexadecimal pairs") }
    guard size <= limits.maxBytes else { throw SerializerError.invalid("XML digest byte limit") }
    guard text.utf8.allSatisfy({ srlXmlHexDigit($0) >= 0 }) else { throw SerializerError.invalid("Invalid XML digest hexadecimal") }
    var output: [UInt8]=[]; output.reserveCapacity(size); var high: Int? = nil
    for byte in text.utf8 { let value=srlXmlHexDigit(byte); if let previous=high { output.append(UInt8(previous*16+value)); high=nil } else { high=value } }
    return output
}
// Emit exact lowercase hex from borrowed owning storage without hashing or payload copies.
fileprivate func srlXmlDigestText(_ value: [UInt8], _ extent: Int, _ limits: XmlDigestLimits) throws -> String {
    guard extent == 0 || value.count == extent else { throw SerializerError.invalid("XML digest extent") }
    guard value.count <= limits.maxBytes && value.count <= Int.max/2 else { throw SerializerError.invalid("XML digest byte limit") }
    let digits=Array("0123456789abcdef".utf8); var output: [UInt8]=[]; output.reserveCapacity(value.count*2)
    for byte in value { output.append(digits[Int(byte >> 4)]); output.append(digits[Int(byte & 15)]) }
    return String(decoding:output,as:UTF8.self)
}
)xmlmodel"
