// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmloutput(
// Write tokens directly while retaining only the names of active ancestors.
fileprivate final class SrlXmlOutput {
    private let limits: XmlLimits
    var listLimit: Int { limits.maxNodes }
    let mapLimits: XmlMapLimits
    let mixedLimits: XmlMixedLimits
    let digestLimits: XmlDigestLimits
    let identityLimits: XmlIdentityLimits
    let extensionOptions: XmlExtensionOptions
    private let budget: SrlXmlBudget
    private var bytes: [UInt8] = []
    private struct Frame {
        let name: SrlXmlName, local: String
        var attributes: [SrlXmlAttribute] = [], bindings: [WireString:String] = [:], pinned = false, pending = true
    }
    private var stack: [Frame] = []
    private var identities: [(value:SrlXmlIdentity,depth:Int)] = []
    // Keep all output and limits local to one complete encode operation.
    init(_ limits: XmlLimits, _ mapLimits: XmlMapLimits = XmlMapLimits(), _ mixedLimits: XmlMixedLimits = XmlMixedLimits(), _ digestLimits: XmlDigestLimits = XmlDigestLimits(), _ identityLimits: XmlIdentityLimits = XmlIdentityLimits(), _ extensionOptions: XmlExtensionOptions = XmlExtensionOptions()) throws {
        try mapLimits.validate()
        try mixedLimits.validate()
        try digestLimits.validate()
        try identityLimits.validate()
        try extensionOptions.validate()
        self.limits = limits; self.mapLimits = mapLimits; self.mixedLimits = mixedLimits; self.digestLimits = digestLimits; self.identityLimits = identityLimits; self.extensionOptions = extensionOptions; self.budget = SrlXmlBudget(limits)
    }
    // Check encoded length before growing the owned document buffer.
    private func append(_ value: String) throws { guard bytes.count <= limits.maxBytes && value.utf8.count <= limits.maxBytes - bytes.count else { throw SerializerError.invalid("XML output limit") }; bytes.append(contentsOf: value.utf8) }
    // Resolve QName vocabulary through active generated start tags without retaining completed elements.
    private func identityNamespace(_ prefix: String) -> String? {
        if prefix == "xml" { return "http://www.w3.org/XML/1998/namespace" }
        for frame in stack.reversed() { if let uri=frame.bindings[WireString(prefix)] { return uri } }
        return prefix.isEmpty ? "" : nil
    }
    // Resolve typed scalar vocabulary from active generated scopes after all declarations are pinned.
    func namespace(_ prefix: String) -> String? { identityNamespace(prefix) }
    // Notify starts only after the current element's complete original attribute inventory is available.
    private func identityStart() throws {
        guard !identities.isEmpty, let frame=stack.last else { return }
        for identity in identities { try identity.value.start(frame.name,frame.attributes,identityNamespace) }
    }
    // Register an independent index after opening its owner but before attributes or content are emitted.
    func observe(_ scope: SrlXmlIdentityScope) throws {
        guard let frame=stack.last, frame.pending else { throw SerializerError.invalid("Invalid XML identity output position") }
        identities.append((try SrlXmlIdentity(scope,identityLimits),stack.count))
    }
    // Finish the current start tag before text or child elements.
    private func seal() throws { if let frame = stack.last, frame.pending { try identityStart(); try append(">"); stack[stack.count - 1].pending = false } }
    // Admit and emit one element before visiting any of its descendants.
    func open(_ name: SrlXmlName, _ preservedScope: [WireString:String]? = nil) throws {
        try budget.node(stack.count + 1); try budget.text(name.local); try budget.text(name.namespace); try seal()
        if let bindings=preservedScope { try srlXmlExtensionBindings(bindings) }
        var bindings=preservedScope ?? [:]
        if name.namespace.isEmpty {
            guard bindings[WireString("")] == nil || bindings[WireString("")]!.isEmpty else { throw SerializerError.invalid("Preserved default namespace conflicts with an unqualified XML name") }
            if !(identityNamespace("") ?? "").isEmpty { bindings[WireString("")]="" }
        }
        var prefix = name.namespace == "http://www.w3.org/XML/1998/namespace" ? "xml" : "srl"
        if preservedScope != nil && !name.namespace.isEmpty {
            if let selected=bindings.filter({ WireString($0.value) == WireString(name.namespace) }).keys.min() { prefix=selected.value }
        }
        var suffix=0
        while let existing=bindings[WireString(prefix)], WireString(existing) != WireString(name.namespace) { suffix += 1; prefix="srl"+String(suffix) }
        let local = name.namespace.isEmpty || prefix.isEmpty ? name.local : prefix + ":" + name.local
        try append("<" + local)
        if !name.namespace.isEmpty && prefix != "xml" { bindings[WireString(prefix)]=name.namespace }
        if preservedScope == nil {
            if !name.namespace.isEmpty && prefix != "xml" { try append(" xmlns:"+prefix+"=\""+srlXmlEscape(name.namespace,true)+"\"") }
            if bindings[WireString("")] != nil { try append(" xmlns=\"\"") }
        } else {
            for (declared,uri) in bindings.sorted(by:{ $0.key < $1.key }) {
                if declared == WireString("xml") { continue }
                try budget.node(stack.count+1); try budget.text(declared.value); try budget.text(uri)
                try append(" xmlns"+(declared.value.isEmpty ? "" : ":"+declared.value)+"=\""+srlXmlEscape(uri,true)+"\"")
            }
        }
        stack.append(Frame(name:name,local:local,bindings:bindings,pinned:preservedScope != nil))
    }
    // Choose generated prefixes without shadowing a retained opaque QName vocabulary binding.
    private func prefixFor(_ uri: String, _ preferred: String) throws -> String {
        if uri == "http://www.w3.org/XML/1998/namespace" { return "xml" }
        var prefix=preferred, suffix=0
        if stack[stack.count-1].pinned,
           let selected=stack[stack.count-1].bindings.filter({ !$0.key.value.isEmpty && WireString($0.value) == WireString(uri) }).keys.min() { return selected.value }
        while let existing=stack[stack.count-1].bindings[WireString(prefix)], WireString(existing) != WireString(uri) { suffix += 1; prefix=preferred+"_"+String(suffix) }
        if stack[stack.count-1].bindings[WireString(prefix)] == nil {
            stack[stack.count-1].bindings[WireString(prefix)]=uri
            if stack[stack.count-1].pinned { try budget.node(stack.count); try budget.text(prefix); try budget.text(uri) }
            try append(" xmlns:"+prefix+"=\""+srlXmlEscape(uri,true)+"\"")
        }
        return prefix
    }
    // Write each expanded-name attribute before content and preserve XML namespace rules.
    func attribute(_ value: SrlXmlAttribute) throws {
        guard let frame = stack.last, frame.pending && !frame.attributes.contains(where:{ $0.name == value.name }) else { throw SerializerError.invalid("Invalid XML attribute position or duplicate") }
        try budget.node(stack.count); try budget.text(value.name.local); try budget.text(value.name.namespace); try budget.text(value.value)
        stack[stack.count - 1].attributes.append(value)
        let prefix = value.name.namespace.isEmpty ? "" : try prefixFor(value.name.namespace,"a\(stack[stack.count-1].attributes.count)")
        try append(" " + (value.name.namespace.isEmpty ? value.name.local : prefix + ":" + value.name.local) + "=\"" + srlXmlEscape(value.value, true) + "\"")
    }
    // Reserve a type prefix independently of element and generated attribute prefixes.
    func typeName(_ identity: SrlXmlName) throws {
        guard let frame = stack.last, frame.pending else { throw SerializerError.invalid("Invalid XML type position") }
        let prefix = identity.namespace.isEmpty ? "" : try prefixFor(identity.namespace,"srlType")
        if prefix.isEmpty && !(identityNamespace("") ?? "").isEmpty {
            guard stack[stack.count-1].bindings[WireString("")] == nil else { throw SerializerError.invalid("Preserved default namespace conflicts with an empty XML type namespace") }
            stack[stack.count-1].bindings[WireString("")]=""; try append(" xmlns=\"\"")
        }
        if prefix != "xml" && !prefix.isEmpty {
            try budget.node(stack.count); try budget.text(prefix); try budget.text(identity.namespace)
        }
        try attribute(SrlXmlAttribute(name: SrlXmlName(local: "type", namespace: "http://www.w3.org/2001/XMLSchema-instance"), value: (prefix.isEmpty ? "" : prefix + ":") + identity.local))
    }
    // Validate and escape only the active scalar value.
    func text(_ value: String) throws { try budget.text(value); try seal(); for identity in identities { try identity.value.text(value) }; try append(srlXmlEscape(value, false)) }
    // Release each completed ancestor after its matching end token.
    func close() throws {
        guard let frame=stack.last else { throw SerializerError.invalid("Unbalanced XML output end") }
        if frame.pending { try identityStart() }
        for identity in identities { try identity.value.end(identityNamespace) }
        for identity in identities where identity.depth == stack.count { try identity.value.finish() }
        identities.removeAll { $0.depth == stack.count }; stack.removeLast()
        try append(frame.pending ? "/>" : "</"+frame.local+">")
    }
    // Publish the complete buffer only after generated traversal succeeds.
    func finish() throws -> [UInt8] { guard stack.isEmpty && identities.isEmpty else { throw SerializerError.invalid("Incomplete XML output or identity scope") }; return bytes }
}
)xmloutput"
