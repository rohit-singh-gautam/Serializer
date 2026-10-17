// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
import Foundation
#if canImport(FoundationXML)
import FoundationXML
#endif

// Optional XML budgets cover one complete owning object.
public struct XmlLimits {
    public var maxBytes = 16777216
    public var maxNodes = 100000
    public var maxDepth = 64
    public var maxTextBytes = 8388608
    public init() {}
}
fileprivate struct SrlXmlName: Equatable { let local: String; let namespace: String }
fileprivate struct SrlXmlAttribute { let name: SrlXmlName; let value: String }
// Test XML whitespace as Unicode scalars; Swift treats a CRLF pair as one Character.
fileprivate func srlXmlWhitespace(_ text: String) -> Bool {
    text.unicodeScalars.allSatisfy { $0.value == 0x20 || $0.value == 0x09 || $0.value == 0x0a || $0.value == 0x0d }
}
fileprivate final class SrlXmlBudget {
    let limits: XmlLimits
    var nodes = 0
    var textBytes = 0
    init(_ limits: XmlLimits) { self.limits = limits }
    func node(_ depth: Int) throws {
        nodes += 1
        guard depth <= limits.maxDepth && nodes <= limits.maxNodes else { throw SerializerError.invalid("XML output tree limit") }
    }
    func text(_ value: String) throws {
        let bytes = value.utf8.count
        guard textBytes <= limits.maxTextBytes && bytes <= limits.maxTextBytes-textBytes,
              textBytes <= limits.maxBytes && bytes <= limits.maxBytes-textBytes else { throw SerializerError.invalid("XML output text limit") }
        textBytes += bytes
    }
}
fileprivate final class SrlXmlNode {
    let name: SrlXmlName
    var text = ""
    var attributes: [SrlXmlAttribute] = []
    var children: [SrlXmlNode] = []
    private let budget: SrlXmlBudget?
    private let depth: Int
    init(_ name: SrlXmlName, _ budget: SrlXmlBudget? = nil, _ depth: Int = 1) { self.name = name; self.budget = budget; self.depth = depth }
    static func root(_ name: SrlXmlName, _ limits: XmlLimits) throws -> SrlXmlNode {
        let budget = SrlXmlBudget(limits); try budget.node(1); return SrlXmlNode(name,budget)
    }
    func makeChild(_ name: SrlXmlName) throws -> SrlXmlNode { try budget?.node(depth+1); return SrlXmlNode(name,budget,depth+1) }
    func setValueText(_ value: String) throws { try budget?.text(value); text = value }
    func addAttribute(_ value: SrlXmlAttribute) throws { try budget?.node(depth); try budget?.text(value.value); attributes.append(value) }
}
// Preserve text and attribute whitespace with deterministic XML escaping.
fileprivate func srlXmlEscape(_ text: String, _ attribute: Bool) throws -> String {
    var result = ""
    for scalar in text.unicodeScalars {
        let value = scalar.value
        guard value == 9 || value == 10 || value == 13 || (32...0xd7ff).contains(value) ||
              (0xe000...0xfffd).contains(value) || (0x10000...0x10ffff).contains(value) else { throw SerializerError.invalid("Invalid XML character") }
        switch value {
        case 38: result += "&amp;"
        case 60: result += "&lt;"
        case 62: result += "&gt;"
        case 34: result += attribute ? "&quot;" : "\""
        case 13: result += "&#13;"
        case 10: result += attribute ? "&#10;" : "\n"
        case 9: result += attribute ? "&#9;" : "\t"
        default: result.unicodeScalars.append(scalar)
        }
    }
    return result
}
// Emit deterministic, bounded XML using only statically generated field bindings.
fileprivate func srlXmlWrite(_ root: SrlXmlNode, _ limits: XmlLimits) throws -> [UInt8] {
    var output = ""
    var nodes = 0
    var textBytes = 0
    func emit(_ node: SrlXmlNode, _ depth: Int) throws {
        nodes += 1
        guard nodes <= limits.maxNodes && depth <= limits.maxDepth else { throw SerializerError.invalid("XML node/depth limit") }
        textBytes += node.text.utf8.count
        for item in node.attributes { textBytes += item.value.utf8.count }
        guard textBytes <= limits.maxTextBytes else { throw SerializerError.invalid("XML text limit") }
        let local = node.name.namespace.isEmpty ? node.name.local : "srl:" + node.name.local
        output += "<" + local
        if !node.name.namespace.isEmpty { output += " xmlns:srl=\"" + (try srlXmlEscape(node.name.namespace, true)) + "\"" }
        for (index, item) in node.attributes.enumerated() {
            let prefix = "a\(index)"
            if !item.name.namespace.isEmpty { output += " xmlns:" + prefix + "=\"" + (try srlXmlEscape(item.name.namespace, true)) + "\"" }
            output += " " + (item.name.namespace.isEmpty ? "" : prefix + ":") + item.name.local + "=\"" + (try srlXmlEscape(item.value, true)) + "\""
        }
        output += ">" + (try srlXmlEscape(node.text, false))
        for child in node.children { try emit(child, depth + 1) }
        output += "</" + local + ">"
        guard output.utf8.count <= limits.maxBytes else { throw SerializerError.invalid("XML output limit") }
    }
    try emit(root, 1)
    return Array(output.utf8)
}
// A non-resolving SAX delegate constructs a bounded data-only XML tree.
fileprivate final class SrlXmlParser: NSObject, XMLParserDelegate {
    let limits: XmlLimits
    var root: SrlXmlNode?
    var stack: [SrlXmlNode] = []
    var prefixes: [String: [String]] = ["xml": ["http://www.w3.org/XML/1998/namespace"]]
    var nodes = 0
    var textBytes = 0
    var failure: Error?
    init(_ limits: XmlLimits) { self.limits = limits }
    func reject(_ parser: XMLParser, _ message: String) { failure = SerializerError.invalid(message); parser.abortParsing() }
    func parser(_ parser: XMLParser, didStartMappingPrefix prefix: String, toURI namespaceURI: String) {
        nodes += 1
        textBytes += prefix.utf8.count + namespaceURI.utf8.count
        guard nodes <= limits.maxNodes && textBytes <= limits.maxTextBytes else { reject(parser, "XML namespace limit"); return }
        prefixes[prefix, default: []].append(namespaceURI)
    }
    func parser(_ parser: XMLParser, didEndMappingPrefix prefix: String) { _ = prefixes[prefix]?.popLast() }
    func parser(_ parser: XMLParser, didStartElement elementName: String, namespaceURI: String?, qualifiedName qName: String?, attributes attributeDict: [String: String]) {
        nodes += 1
        guard nodes <= limits.maxNodes && stack.count < limits.maxDepth else { reject(parser, "XML node/depth limit"); return }
        let node = SrlXmlNode(SrlXmlName(local: elementName, namespace: namespaceURI ?? ""))
        for (rawName, value) in attributeDict {
            if rawName == "xmlns" || rawName.hasPrefix("xmlns:") { continue }
            nodes += 1
            guard nodes <= limits.maxNodes else { reject(parser, "XML attribute limit"); return }
            let parts = rawName.split(separator: ":", maxSplits: 1).map(String.init)
            let uri = parts.count == 2 ? prefixes[parts[0]]?.last ?? "" : ""
            if parts.count == 2 && uri.isEmpty { reject(parser, "Unbound XML attribute prefix"); return }
            node.attributes.append(SrlXmlAttribute(name: SrlXmlName(local: parts.last!, namespace: uri), value: value))
            textBytes += value.utf8.count
        }
        guard textBytes <= limits.maxTextBytes else { reject(parser, "XML text limit"); return }
        if let parent = stack.last { parent.children.append(node) }
        else if root == nil { root = node }
        else { reject(parser, "Multiple XML roots"); return }
        stack.append(node)
    }
    func parser(_ parser: XMLParser, didEndElement elementName: String, namespaceURI: String?, qualifiedName qName: String?) {
        guard let node = stack.popLast() else { reject(parser, "Unexpected XML closing tag"); return }
        if !node.children.isEmpty && !srlXmlWhitespace(node.text) { reject(parser, "Mixed XML content unsupported") }
    }
    func parser(_ parser: XMLParser, foundCharacters string: String) {
        textBytes += string.utf8.count
        guard textBytes <= limits.maxTextBytes else { reject(parser, "XML text limit"); return }
        stack.last?.text += string
    }
    func parser(_ parser: XMLParser, foundCDATA CDATABlock: Data) {
        guard let text = String(data: CDATABlock, encoding: .utf8) else { reject(parser, "Invalid XML CDATA UTF-8"); return }
        self.parser(parser, foundCharacters: text)
    }
    func parser(_ parser: XMLParser, foundInternalEntityDeclarationWithName name: String, value: String?) { reject(parser, "XML entities prohibited") }
    func parser(_ parser: XMLParser, foundExternalEntityDeclarationWithName name: String, publicID: String?, systemID: String?) { reject(parser, "XML entities prohibited") }
    func parser(_ parser: XMLParser, resolveExternalEntityName name: String, systemID: String?) -> Data? { reject(parser, "External XML resource prohibited"); return nil }
}
// Detect actual DTD markup without rejecting legal CDATA/comment/processing-instruction text.
fileprivate func srlXmlHasDoctype(_ text: String) -> Bool {
    var cursor = text.startIndex
    while let opening = text.range(of: "<", range: cursor..<text.endIndex) {
        let start = opening.lowerBound
        let remaining = text[start...]
        let closing: String? = remaining.hasPrefix("<!--") ? "-->" : remaining.hasPrefix("<![CDATA[") ? "]]>" : remaining.hasPrefix("<?") ? "?>" : nil
        if let marker = closing {
            guard let end = text.range(of: marker, range: opening.upperBound..<text.endIndex) else { return false }
            cursor = end.upperBound
        } else {
            if remaining.hasPrefix("<!DOCTYPE") { return true }
            cursor = opening.upperBound
        }
        if cursor == text.endIndex { return false }
    }
    return false
}
// Reject DTDs before SAX parsing; XMLParser never receives a resource resolver.
fileprivate func srlXmlParse(_ bytes: [UInt8], _ limits: XmlLimits) throws -> SrlXmlNode {
    guard bytes.count <= limits.maxBytes, let text = String(bytes: bytes, encoding: .utf8), !text.utf8.contains(0), !srlXmlHasDoctype(text) else { throw SerializerError.invalid("Invalid UTF-8, XML size or DTD") }
    let content = text.hasPrefix("\u{feff}") ? String(text.dropFirst()) : text
    if content.hasPrefix("<?xml ") || content.hasPrefix("<?xml\t") || content.hasPrefix("<?xml\r") || content.hasPrefix("<?xml\n") {
        guard let end = content.range(of: "?>") else { throw SerializerError.invalid("Invalid XML declaration") }
        let header = String(content[..<end.upperBound])
        guard header.range(of: "version\\s*=\\s*['\"]1\\.0['\"]", options: .regularExpression) != nil else { throw SerializerError.invalid("Only XML 1.0 supported") }
        if header.contains("encoding") {
            guard header.range(of: "encoding\\s*=\\s*['\"]UTF-8['\"]", options: [.regularExpression, .caseInsensitive]) != nil else { throw SerializerError.invalid("XML encoding must be UTF-8") }
        }
    }
    let handler = SrlXmlParser(limits)
    let parser = XMLParser(data: Data(bytes))
    parser.delegate = handler
    parser.shouldProcessNamespaces = true
    parser.shouldReportNamespacePrefixes = true
    parser.shouldResolveExternalEntities = false
    guard parser.parse(), handler.failure == nil, handler.stack.isEmpty, let root = handler.root else { throw handler.failure ?? parser.parserError ?? SerializerError.invalid("Malformed XML") }
    return root
}
// Consume all attributes and ordered children, rejecting unknown or duplicate members.
fileprivate final class SrlXmlReader {
    let node: SrlXmlNode
    var used: Set<Int> = []
    var cursor = 0
    init(_ node: SrlXmlNode) throws {
        self.node = node
        guard srlXmlWhitespace(node.text) else { throw SerializerError.invalid("Unexpected XML text") }
    }
    func attribute(_ name: SrlXmlName, _ optional: Bool = false) throws -> String? {
        guard let index = node.attributes.firstIndex(where: { $0.name == name }) else {
            if optional { return nil }; throw SerializerError.invalid("Required XML attribute missing")
        }
        guard used.insert(index).inserted else { throw SerializerError.invalid("Repeated XML attribute") }
        return node.attributes[index].value
    }
    func child(_ name: SrlXmlName, _ optional: Bool = false) throws -> SrlXmlNode? {
        if cursor == node.children.count || node.children[cursor].name != name {
            if optional { return nil }; throw SerializerError.invalid("Required XML child missing or out of order")
        }
        defer { cursor += 1 }; return node.children[cursor]
    }
    func repeated(_ name: SrlXmlName) -> [SrlXmlNode] {
        var result: [SrlXmlNode] = []
        while cursor < node.children.count && node.children[cursor].name == name { result.append(node.children[cursor]); cursor += 1 }
        return result
    }
    func finish() throws {
        guard used.count == node.attributes.count && cursor == node.children.count else { throw SerializerError.invalid("Unexpected XML member") }
    }
}
// Decode scalar content only after rejecting nested markup and attributes.
fileprivate func srlXmlText(_ node: SrlXmlNode) throws -> String {
    guard node.attributes.isEmpty && node.children.isEmpty else { throw SerializerError.invalid("Invalid XML scalar shape") }
    return node.text
}
// Preserve XSD Boolean spelling independently from Swift's Boolean parser.
fileprivate func srlXmlBoolean(_ text: String) throws -> Bool {
    switch text.trimmingCharacters(in: CharacterSet(charactersIn: " \t\r\n")) {
    case "true", "1": return true
    case "false", "0": return false
    default: throw SerializerError.invalid("Invalid XML Boolean")
    }
}
// Validate floating lexical forms and overflow before assigning generated numeric storage.
fileprivate func srlXmlFloat(_ text: String) throws -> Double {
    let value = text.trimmingCharacters(in: CharacterSet(charactersIn: " \t\r\n"))
    switch value {
    case "INF": return .infinity
    case "-INF": return -.infinity
    case "NaN": return .nan
    default:
        guard value.range(of: "^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?$", options: .regularExpression) != nil,
              let result = Double(value), result.isFinite else { throw SerializerError.invalid("Invalid XML floating value") }
        return result
    }
}
)serializer"
