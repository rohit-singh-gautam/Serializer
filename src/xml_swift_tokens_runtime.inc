// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// Foundation reports Unicode namespace IRIs as a nonfatal LibXml2 URI warning.
// Preserve those names byte-exactly while rejecting every actual parser error.
fileprivate func srlXmlProviderFailure(_ error: Error?) -> Error? {
    guard let error = error else { return nil }
    let namespaceUriWarning = 99
    let value = error as NSError
    if value.domain == "NSXMLParserErrorDomain" && value.code == namespaceUriWarning { return nil }
    return error
}
// Own only the private typed destination while its provider callbacks are active.
fileprivate final class SrlXmlValueBox<T> {
    var value: T
    // Keep one independently owned candidate outside callback value-copy semantics.
    init(_ value: T) { self.value = value }
}
// One active typed element retains callbacks and its expanded name, never completed children.
fileprivate final class SrlXmlTokenFrame {
    let name: SrlXmlName
    var identity: SrlXmlIdentity?
    var knownChildren: [SrlXmlName] = []
    var foreignChild: ((SrlXmlName, [SrlXmlAttribute], () -> [WireString:String]) throws -> SrlXmlTokenFrame)?
    var scopeStart: (([WireString:String]) throws -> Void)?
    var namespaceValue = false
    var child: (SrlXmlName, [SrlXmlAttribute]) throws -> SrlXmlTokenFrame = { _, _ in throw SerializerError.invalid("Unexpected XML child") }
    var text: (String) throws -> Void = { value in
        guard srlXmlWhitespace(value) else { throw SerializerError.invalid("Unexpected XML record text") }
    }
    var finish: () throws -> Void = {}
    // Start an empty strict frame; generated bindings install its permitted callbacks.
    init(_ name: SrlXmlName) { self.name = name }
}
// Consume current-record attributes once and reject unknown expanded identities.
fileprivate final class SrlXmlInputAttributes {
    private var remaining: [SrlXmlAttribute]
    // Borrow no provider state after its synchronous start-element callback returns.
    init(_ values: [SrlXmlAttribute]) { remaining = values }
    // Preserve absence independently of an empty attribute string.
    func take(_ name: SrlXmlName, _ optional: Bool = false) throws -> String? {
        guard let index = remaining.firstIndex(where: { $0.name == name }) else {
            if optional { return nil }
            throw SerializerError.invalid("Required XML attribute missing")
        }
        return remaining.remove(at: index).value
    }
    // Require exact membership before processing any child values.
    func finish() throws {
        guard remaining.isEmpty else { throw SerializerError.invalid("Unexpected XML attribute") }
    }
    // Consume only explicitly admitted foreign attributes after known fields and model metadata.
    func extensions(_ tracker: SrlXmlExtensions) throws {
        for value in remaining { try tracker.attribute(value) }
        remaining.removeAll()
    }
}
// Remove recognized nil metadata while preserving every other expanded attribute identity.
fileprivate func srlXmlPresence(_ attributes: [SrlXmlAttribute]) throws -> (isNil: Bool, attributes: [SrlXmlAttribute]) {
    let nilName = SrlXmlName(local: "nil", namespace: "http://www.w3.org/2001/XMLSchema-instance")
    var remaining: [SrlXmlAttribute] = []
    var lexical: String? = nil
    for attribute in attributes {
        if attribute.name == nilName {
            guard lexical == nil else { throw SerializerError.invalid("Duplicate XML nil metadata") }
            lexical = attribute.value.trimmingCharacters(in: CharacterSet(charactersIn: " \t\r\n"))
        } else { remaining.append(attribute) }
    }
    guard let value = lexical else { return (false, remaining) }
    guard value == "true" || value == "1" || value == "false" || value == "0" else { throw SerializerError.invalid("Invalid XML nil value") }
    let isNil = value == "true" || value == "1"
    guard !isNil || remaining.isEmpty else { throw SerializerError.invalid("Nil XML element contains attributes") }
    return (isNil, remaining)
}
// Join only the active bounded scalar across provider text, CDATA and entity callbacks.
fileprivate func srlXmlScalarFrame(_ name: SrlXmlName, _ attributes: [SrlXmlAttribute],
    _ complete: @escaping (String) throws -> Void) throws -> SrlXmlTokenFrame {
    guard attributes.isEmpty else { throw SerializerError.invalid("XML scalar contains attributes") }
    let frame = SrlXmlTokenFrame(name)
    var text = ""
    frame.text = { text.append(contentsOf: $0) }
    frame.finish = { try complete(text) }
    return frame
}
)serializer"
R"serializer(
// Feed generated owning frames directly from Foundation's non-resolving SAX provider.
fileprivate final class SrlXmlTokenParser: NSObject, XMLParserDelegate {
    private let budget: SrlXmlBudget
    private let lexical: SrlXmlLexicalAttributes
    private let rootFactory: (SrlXmlName, [SrlXmlAttribute]) throws -> SrlXmlTokenFrame
    private var prefixes: [WireString: [String]] = ["xml": ["http://www.w3.org/XML/1998/namespace"]]
    private var scopes: [[WireString]] = []
    private(set) var stack: [SrlXmlTokenFrame] = []
    private(set) var failure: Error?
    private(set) var rootSeen = false
    private(set) var complete = false
    // Bind one provider operation; all state remains local to synchronous parsing.
    init(_ bytes: [UInt8], _ limits: XmlLimits, _ factory: @escaping (SrlXmlName, [SrlXmlAttribute]) throws -> SrlXmlTokenFrame) {
        budget = SrlXmlBudget(limits); lexical = SrlXmlLexicalAttributes(bytes); rootFactory = factory
    }
    // Preserve the first callback failure and stop the provider without publishing a model.
    private func checked(_ parser: XMLParser, _ action: () throws -> Void) {
        guard failure == nil else { return }
        do { try action() } catch { failure = error; parser.abortParsing() }
    }
    // Require the provider's complete-document callback as well as its parse return value.
    func parserDidEndDocument(_ parser: XMLParser) { complete = true }
    // Foundation can finish a root before reporting trailing malformed content.
    func parser(_ parser: XMLParser, parseErrorOccurred error: Error) {
        if failure == nil { failure = srlXmlProviderFailure(error) }
    }
    // Preserve validation failures when a provider reports them through its separate callback.
    func parser(_ parser: XMLParser, validationErrorOccurred error: Error) {
        if failure == nil { failure = srlXmlProviderFailure(error) }
    }
    // Create the exact typed child selected by generated metadata, with no intermediate node.
    func parser(_ parser: XMLParser, didStartElement elementName: String, namespaceURI: String?, qualifiedName qName: String?, attributes attributeDict: [String: String]) {
        checked(parser) {
            let name = SrlXmlName(local: elementName, namespace: namespaceURI ?? "")
            try budget.node(stack.count + 1); try budget.text(name.local); try budget.text(name.namespace)
            var attributes: [SrlXmlAttribute] = []
            // Foundation's attribute dictionary merges canonically equivalent Unicode keys.
            // Consume the validated source attributes through a byte-preserving lexical cursor.
            var rawAttributes: [(String, String)] = []
            try lexical.next(qName, budget) { raw, value in
                rawAttributes.append((raw, value))
            }
            // Namespace callbacks can lose the same identities; scope exact source declarations.
            var declared: [WireString] = []
            for (raw, value) in rawAttributes where raw == "xmlns" || raw.hasPrefix("xmlns:") {
                let prefix = raw == "xmlns" ? "" : String(raw.dropFirst(6))
                let key = WireString(prefix)
                guard !declared.contains(key) else { throw SerializerError.invalid("Duplicate XML namespace declaration") }
                try budget.node(stack.count + 1); try budget.text(prefix); try budget.text(value)
                prefixes[key, default: []].append(value); declared.append(key)
            }
            scopes.append(declared)
            for (raw, value) in rawAttributes {
                if raw == "xmlns" || raw.hasPrefix("xmlns:") { continue }
                let parts = raw.split(separator: ":", maxSplits: 1).map(String.init)
                let uri = parts.count == 2 ? prefixes[WireString(parts[0])]?.last ?? "" : ""
                guard parts.count == 1 || !uri.isEmpty else { throw SerializerError.invalid("Unbound XML attribute prefix") }
                let key = SrlXmlName(local: parts.last!, namespace: uri)
                guard !attributes.contains(where: { $0.name == key }) else { throw SerializerError.invalid("Duplicate expanded XML attribute") }
                try budget.node(stack.count + 1); try budget.text(key.local); try budget.text(key.namespace); try budget.text(value)
                let typeName = SrlXmlName(local: "type", namespace: "http://www.w3.org/2001/XMLSchema-instance")
                let identity = key == typeName ? srlXmlTypeQName(value) { prefix in self.prefixes[WireString(prefix)]?.last } : nil
                attributes.append(SrlXmlAttribute(name: key, value: value, typeIdentity: identity))
            }
            let frame: SrlXmlTokenFrame
            if let parent = stack.last {
                if let foreign = parent.foreignChild, !parent.knownChildren.contains(name) {
                    frame = try foreign(name, attributes, { self.prefixes.compactMapValues { $0.last } })
                } else { frame = try parent.child(name, attributes) }
            }
            else {
                guard !rootSeen else { throw SerializerError.invalid("Multiple XML roots") }
                rootSeen = true; frame = try rootFactory(name, attributes)
            }
            if let start = frame.scopeStart {
                if frame.namespaceValue {
                    if prefixes[WireString("")] == nil { try budget.node(stack.count+1) }
                    for (prefix,values) in prefixes {
                        if let uri=values.last { try budget.node(stack.count+1); try budget.text(prefix.value); try budget.text(uri) }
                    }
                }
                try start(prefixes.compactMapValues { $0.last })
            }
            stack.append(frame)
            // Existing ancestors and this new owner observe the same original full start tag exactly once.
            for active in stack { try active.identity?.start(name,attributes) { prefix in self.prefixes[WireString(prefix)]?.last } }
        }
    }
    // Validate the completed frame before assigning its value to its private parent.
    func parser(_ parser: XMLParser, didEndElement elementName: String, namespaceURI: String?, qualifiedName qName: String?) {
        checked(parser) {
            guard let frame = stack.last, frame.name == SrlXmlName(local: elementName, namespace: namespaceURI ?? "") else {
                throw SerializerError.invalid("Unexpected XML closing tag")
            }
            // Validate tuples before assigning the private child candidate or releasing its QName scope.
            for active in stack { try active.identity?.end { prefix in self.prefixes[WireString(prefix)]?.last } }
            try frame.identity?.finish()
            stack.removeLast()
            try frame.finish()
            guard let declared = scopes.popLast() else { throw SerializerError.invalid("XML namespace scope mismatch") }
            for key in declared {
                _ = prefixes[key]?.popLast()
                if prefixes[key]?.isEmpty == true { prefixes.removeValue(forKey: key) }
            }
        }
    }
    // Admit decoded text before retaining it in a single active scalar frame.
    func parser(_ parser: XMLParser, foundCharacters string: String) {
        checked(parser) {
            try budget.text(string)
            for frame in stack { try frame.identity?.text(string) }
            if let frame = stack.last { try frame.text(string) }
            else if !srlXmlWhitespace(string) { throw SerializerError.invalid("Unexpected XML document text") }
        }
    }
    // Preserve CDATA and escaped text through the same scalar callback.
    func parser(_ parser: XMLParser, foundCDATA CDATABlock: Data) {
        checked(parser) {
            guard let text = String(data: CDATABlock, encoding: .utf8) else { throw SerializerError.invalid("Invalid XML CDATA UTF8") }
            try budget.text(text)
            for frame in stack { try frame.identity?.text(text) }
            guard let frame = stack.last else { throw SerializerError.invalid("Unexpected XML CDATA") }
            try frame.text(text)
        }
    }
    // Reject user entity definitions independently of provider resolver defaults.
    func parser(_ parser: XMLParser, foundInternalEntityDeclarationWithName name: String, value: String?) {
        checked(parser) { throw SerializerError.invalid("XML entities prohibited") }
    }
    // Reject both local and remote external entity declarations without opening them.
    func parser(_ parser: XMLParser, foundExternalEntityDeclarationWithName name: String, publicID: String?, systemID: String?) {
        checked(parser) { throw SerializerError.invalid("XML entities prohibited") }
    }
    // Never acquire external resources even if provider defaults change.
    func parser(_ parser: XMLParser, resolveExternalEntityName name: String, systemID: String?) -> Data? {
        checked(parser) { throw SerializerError.invalid("External XML resource prohibited") }; return nil
    }
}
// Preserve the established UTF8/XML1.0/DTD policy before allowing provider callbacks.
fileprivate func srlXmlTokenPreflight(_ bytes: [UInt8], _ limits: XmlLimits) throws {
    try limits.srlPackageCheck?()
    if limits.maxBytes >= 0 && bytes.count > limits.maxBytes { throw srlXmlLimitFailure(limits,"Invalid UTF8, XML size or DTD") }
    guard limits.maxBytes >= 0 && limits.maxNodes >= 0 && limits.maxDepth >= 0 && limits.maxTextBytes >= 0,
          bytes.count <= limits.maxBytes, let text = String(bytes: bytes, encoding: .utf8),
          !text.utf8.contains(0), !srlXmlHasDoctype(text) else { throw SerializerError.invalid("Invalid UTF8, XML size or DTD") }
    let content = text.hasPrefix("\u{feff}") ? String(text.dropFirst()) : text
    if content.hasPrefix("<?xml ") || content.hasPrefix("<?xml\t") || content.hasPrefix("<?xml\r") || content.hasPrefix("<?xml\n") {
        guard let end = content.range(of: "?>") else { throw SerializerError.invalid("Invalid XML declaration") }
        let header = String(content[..<end.upperBound])
        guard header.range(of: "version\\s*=\\s*['\"]1\\.0['\"]", options: .regularExpression) != nil else { throw SerializerError.invalid("Only XML1.0 supported") }
        if header.contains("encoding") {
            guard header.range(of: "encoding\\s*=\\s*['\"]UTF-8['\"]", options: [.regularExpression, .caseInsensitive]) != nil else { throw SerializerError.invalid("XML encoding must be UTF8") }
        }
    }
}
// Publish one typed root only after the provider accepts complete document EOF.
fileprivate func srlXmlDecodeTokens<T>(_ bytes: [UInt8], _ limits: XmlLimits, _ expected: SrlXmlName,
    _ mapLimits: XmlMapLimits, _ mixedLimits: XmlMixedLimits, _ digestLimits: XmlDigestLimits, _ identityLimits: XmlIdentityLimits, _ extensionOptions: XmlExtensionOptions,
    _ factory: @escaping (SrlXmlName, [SrlXmlAttribute], XmlMapLimits, XmlMixedLimits, XmlDigestLimits, XmlIdentityLimits, XmlExtensionOptions, Int, @escaping (T) throws -> Void) throws -> SrlXmlTokenFrame) throws -> T {
    try srlXmlTokenPreflight(bytes, limits)
    var mapLimits=mapLimits;mapLimits.srlPackageLimit=limits.srlPackageLimit
    var mixedLimits=mixedLimits;mixedLimits.srlPackageLimit=limits.srlPackageLimit
    var digestLimits=digestLimits;digestLimits.srlPackageLimit=limits.srlPackageLimit
    var identityLimits=identityLimits;identityLimits.srlPackageLimit=limits.srlPackageLimit;identityLimits.srlPackageConsume=limits.srlPackageConsume
    var extensionOptions=extensionOptions;extensionOptions.srlPackageLimit=limits.srlPackageLimit;extensionOptions.srlPackageConsume=limits.srlPackageConsume
    try mapLimits.validate()
    try mixedLimits.validate()
    try digestLimits.validate()
    try identityLimits.validate()
    try extensionOptions.validate()
    let result = SrlXmlValueBox<T?>(nil)
    let handler = SrlXmlTokenParser(bytes, limits) { name, attributes in
        guard name == expected else { throw SerializerError.invalid("Unexpected XML root") }
        var selected = attributes
        if let expectedType = limits.srlPackageRootType {
            let metadata = try srlXmlVariantType(attributes,false)
            guard metadata.identity == nil || metadata.identity == expectedType else { throw SerializerError.invalid("Unexpected package XML type") }
            selected = metadata.attributes
        }
        return try factory(name, selected, mapLimits, mixedLimits, digestLimits, identityLimits, extensionOptions, limits.maxNodes) { value in result.value = value }
    }
    let parser = XMLParser(data: Data(bytes))
    parser.delegate = handler
    parser.shouldProcessNamespaces = true
    parser.shouldReportNamespacePrefixes = true
    parser.shouldResolveExternalEntities = false
    let accepted = parser.parse()
    guard accepted, srlXmlProviderFailure(parser.parserError) == nil, handler.failure == nil, handler.complete,
          handler.rootSeen, handler.stack.isEmpty, let value = result.value else {
        throw handler.failure ?? parser.parserError ?? SerializerError.invalid("Malformed XML")
    }
    return value
}
)serializer"
