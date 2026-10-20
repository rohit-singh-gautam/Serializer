// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlextension(
// Bound explicit foreign roots and retained semantic bytes independently of complete-document limits.
public struct XmlExtensionLimits {
    public let maxSlots: Int, maxItems: Int, maxPreservedBytes: Int
    // Keep zero admission exact; operation entry points reject negative selected bounds.
    public init(maxSlots: Int? = nil, maxItems: Int? = nil, maxPreservedBytes: Int? = nil) {
        self.maxSlots=maxSlots ?? srlXmlExtensionMaxSlots
        self.maxItems=maxItems ?? srlXmlExtensionMaxItems
        self.maxPreservedBytes=maxPreservedBytes ?? srlXmlExtensionMaxPreservedBytes
    }
    // Validate before slot counters or retained payload are allocated.
    fileprivate func validate() throws {
        guard maxSlots >= 0 && maxItems >= 0 && maxPreservedBytes >= 0 else { throw SerializerError.invalid("Negative XML extension limit") }
    }
}
// Permission to preserve opaque content never establishes semantic understanding of its vocabulary.
public struct XmlExtensionOptions {
    fileprivate var srlPackageLimit: ((String) -> any Swift.Error)?
    fileprivate var srlPackageConsume: ((String,Int) throws -> Void)?
    public let understoodNamespaces: [String], requiredPrefixes: String
    public let limits: XmlExtensionLimits
    // Own immutable capability metadata while preserving ordinary Swift copy-on-write array semantics.
    public init(understoodNamespaces: [String] = [], requiredPrefixes: String = "", limits: XmlExtensionLimits = XmlExtensionLimits()) {
        self.understoodNamespaces=understoodNamespaces; self.requiredPrefixes=requiredPrefixes; self.limits=limits
    }
    // Bound capability metadata before creating membership indexes or prefix-token storage.
    fileprivate func validate() throws {
        try limits.validate()
        guard understoodNamespaces.count <= limits.maxItems else { throw srlXmlLimitFailure(srlPackageLimit,"XML capability item limit") }
        var bytes=requiredPrefixes.utf8.count
        guard bytes <= srlXmlExtensionMaxPreservedBytes else { throw srlXmlLimitFailure(srlPackageLimit,"XML capability metadata byte limit") }
        for uri in understoodNamespaces {
            guard uri.utf8.count <= srlXmlExtensionMaxPreservedBytes-bytes else { throw srlXmlLimitFailure(srlPackageLimit,"XML capability metadata byte limit") }
            bytes += uri.utf8.count; _=try srlXmlEscape(uri,true)
        }
        _=try srlXmlEscape(requiredPrefixes,true)
    }
}
fileprivate struct SrlXmlExtensionSlot {
    let id: String, attribute: Bool, position: Int, namespaces: [String], policy: Int, minimum: Int, maximum: Int?
    // Retain checked compiler-owned URI permissions and exact occurrence intervals.
    init(_ id: String, _ attribute: Bool, _ position: Int, _ namespaces: [String], _ policy: Int, _ minimum: Int, _ maximum: Int?) {
        self.id=id; self.attribute=attribute; self.position=position; self.namespaces=namespaces
        self.policy=policy; self.minimum=minimum; self.maximum=maximum
    }
}
fileprivate struct SrlXmlExtensionContract {
    let core: String, known: [SrlXmlName], required: [String], slots: [SrlXmlExtensionSlot]
    let knownNames: Set<SrlXmlName>, attributeSlots: [WireString:Int], childSlots: [WireString:[Int:Int]]
    // Keep known names closed independently of foreign slot namespace inventories.
    init(_ core: String, _ known: [SrlXmlName], _ required: [String], _ slots: [SrlXmlExtensionSlot]) {
        self.core=core; self.known=known; self.required=required; self.slots=slots
        knownNames=Set(known)
        var attributes: [WireString:Int]=[:], children: [WireString:[Int:Int]]=[:]
        for (index,slot) in slots.enumerated() {
            for uri in slot.namespaces {
                if slot.attribute { attributes[WireString(uri)]=index }
                else { children[WireString(uri),default:[:]][slot.position]=index }
            }
        }
        attributeSlots=attributes; childSlots=children
    }
}
fileprivate enum SrlXmlForeignContent { case text(String), element(SrlXmlForeignNode) }
fileprivate final class SrlXmlForeignNode {
    let name: SrlXmlName, attributes: [SrlXmlAttribute], scope: [WireString:String]
    var content: [SrlXmlForeignContent] = []
    // Copy only an admitted foreign start tag and its complete original namespace context.
    init(_ name: SrlXmlName, _ attributes: [SrlXmlAttribute], _ scope: [WireString:String]) {
        self.name=name; self.attributes=attributes; self.scope=scope
    }
}
fileprivate struct SrlXmlForeignChild {
    let position: Int, item: Int, node: SrlXmlForeignNode
}
fileprivate final class SrlXmlPreserved {
    let scope: [WireString:String]
    var attributes: [SrlXmlAttribute] = [], children: [SrlXmlForeignChild] = []
    // Allocate the XML-only sidecar only after a preserve policy admits an actual foreign item.
    init(_ scope: [WireString:String]) { self.scope=scope }
}
// Validate exact prefix identities without interpreting unknown text or attribute lexical values.
fileprivate func srlXmlExtensionBindings(_ scope: [WireString:String]) throws {
    let xmlNamespace="http://www.w3.org/XML/1998/namespace", xmlnsNamespace="http://www.w3.org/2000/xmlns/"
    for (key,uri) in scope {
        let prefix=key.value
        guard (prefix.isEmpty || srlXmlNcName(prefix)), prefix != "xmlns",
              (prefix == "xml") == (uri == xmlNamespace), (prefix.isEmpty || !uri.isEmpty),
              uri != xmlnsNamespace, uri.trimmingCharacters(in:.whitespacesAndNewlines).utf8.elementsEqual(uri.utf8) else {
            throw SerializerError.invalid("Invalid preserved XML namespace binding")
        }
        _=try srlXmlEscape(uri,true)
    }
}
// Track exact owner-local slots while retaining only explicitly preserved foreign semantic content.
fileprivate final class SrlXmlExtensions {
    let meta: SrlXmlExtensionContract, scope: [WireString:String], options: XmlExtensionOptions
    private var counts: [Int] = [], items=0, bytes=0, cursor=0
    private(set) var state: SrlXmlPreserved?
    private static let ownerOverheadBytes=64, nodeOverheadBytes=64, scopeEntryOverheadBytes=64, attributeOverheadBytes=48
    static let contentEntryOverheadBytes=24
    // Check required capabilities in the original namespace scope before consuming native model fields.
    private init(_ meta: SrlXmlExtensionContract, _ scope: [WireString:String], _ options: XmlExtensionOptions, _ existing: SrlXmlPreserved?) throws {
        self.meta=meta; self.scope=scope; self.options=options; state=existing
        try options.validate()
        guard meta.slots.count <= options.limits.maxSlots, meta.known.count <= options.limits.maxItems,
              meta.required.count <= options.limits.maxItems else { throw srlXmlLimitFailure(options.srlPackageLimit,"XML extension slot limit") }
        counts=[Int](repeating:0,count:meta.slots.count)
        let understood=Set(options.understoodNamespaces.map { WireString($0) })
        for uri in meta.required {
            guard WireString(uri) == WireString(meta.core) || understood.contains(WireString(uri)) else { throw SerializerError.invalid("Required XML capability is not understood") }
        }
        var required=0
        for prefix in try SrlXmlRule.tokens(options.requiredPrefixes,options.limits.maxItems,options.srlPackageLimit) {
            guard required < options.limits.maxItems, srlXmlNcName(prefix), prefix != "xmlns" else { throw SerializerError.invalid("Invalid required XML capability prefix") }; required += 1
            guard let uri=scope[WireString(prefix)], !uri.isEmpty else { throw SerializerError.invalid("Unbound required XML capability prefix") }
            guard WireString(uri) == WireString(meta.core) || understood.contains(WireString(uri)) else { throw SerializerError.invalid("Required XML capability is not understood") }
        }
        if existing != nil { try charge(try addSize(Self.ownerOverheadBytes,scopeBytes(scope))) }
    }
    // Check arithmetic and the selected bound before accumulating a logical storage size.
    private func addSize(_ current: Int, _ amount: Int) throws -> Int {
        guard current >= 0 && amount >= 0 && current <= options.limits.maxPreservedBytes,
              amount <= options.limits.maxPreservedBytes-current else { throw srlXmlLimitFailure(options.srlPackageLimit,"XML extension preserved-byte limit") }
        return current+amount
    }
    // Admit logical bytes before any corresponding owning payload allocation.
    func charge(_ amount: Int) throws { let next=try addSize(bytes,amount); try options.srlPackageConsume?("preserved",amount); bytes=next }
    // Measure all in-scope declarations, including prefixes used only by unknown QName lexical values.
    private func scopeBytes(_ bindings: [WireString:String]) throws -> Int {
        try srlXmlExtensionBindings(bindings); var size=0
        for (prefix,uri) in bindings {
            size=try addSize(size,Self.scopeEntryOverheadBytes)
            size=try addSize(size,prefix.value.utf8.count); size=try addSize(size,uri.utf8.count)
        }
        return size
    }
    // Close core and statically known names even if a wildcard allows their vocabulary URI.
    func slot(_ name: SrlXmlName, _ attribute: Bool, _ position: Int) -> Int? {
        guard !name.namespace.isEmpty, WireString(name.namespace) != WireString(meta.core), name.namespace != srlXmlModelNamespace, !meta.knownNames.contains(name) else { return nil }
        return attribute ? meta.attributeSlots[WireString(name.namespace)] : meta.childSlots[WireString(name.namespace)]?[position]
    }
    // Admit one unknown root's policy, item count and occurrence before capture or skip begins.
    func admit(_ name: SrlXmlName, _ attribute: Bool, _ position: Int) throws -> Bool {
        guard let index=slot(name,attribute,position) else { throw SerializerError.invalid("XML extension has no permitted slot") }
        let selected=meta.slots[index]
        guard selected.policy != 0 else { throw SerializerError.invalid("XML extension slot rejects foreign vocabulary") }
        guard items < options.limits.maxItems else { throw srlXmlLimitFailure(options.srlPackageLimit,"XML extension item/occurrence limit") }
        guard selected.maximum == nil || counts[index] < selected.maximum! else { throw SerializerError.invalid("XML extension item/occurrence limit") }
        items += 1; counts[index] += 1; return selected.policy == 2
    }
    // Lazily construct a private sidecar; skip-only traversal retains no foreign candidate.
    private func storage() throws -> SrlXmlPreserved {
        if let value=state { return value }
        try charge(try addSize(Self.ownerOverheadBytes,scopeBytes(scope)))
        let value=SrlXmlPreserved(scope); state=value; return value
    }
)xmlextension"
R"xmlextension(
    // Validate edited retained names before the token writer's statically trusted-name path.
    private func name(_ value: SrlXmlName) throws {
        guard srlXmlNcName(value.local), value.namespace != "http://www.w3.org/2000/xmlns/" else { throw SerializerError.invalid("Invalid XML extension expanded name") }
        _=try srlXmlEscape(value.namespace,true)
    }
    // Measure an exact expanded attribute without changing its opaque lexical value.
    private func attributeBytes(_ value: SrlXmlAttribute) throws -> Int {
        try name(value.name); _=try srlXmlEscape(value.value,true)
        var size=try addSize(Self.attributeOverheadBytes,value.name.local.utf8.count)
        size=try addSize(size,value.name.namespace.utf8.count); return try addSize(size,value.value.utf8.count)
    }
    // Preserve only foreign attributes admitted by an explicit attribute slot.
    func attribute(_ value: SrlXmlAttribute) throws {
        if try admit(value.name,true,0) { try charge(attributeBytes(value)); try storage().attributes.append(value) }
    }
    // Admit a foreign start tag before allocating copies of provider attributes and scope bindings.
    func measure(_ value: SrlXmlName, _ attributes: [SrlXmlAttribute], _ bindings: [WireString:String]) throws {
        try name(value); var size=try addSize(Self.nodeOverheadBytes,value.local.utf8.count)
        size=try addSize(size,value.namespace.utf8.count); size=try addSize(size,scopeBytes(bindings))
        for attribute in attributes { size=try addSize(size,attributeBytes(attribute)) }; try charge(size)
    }
    // Attach a fully captured foreign subtree at its original static field and repeated-item boundary.
    func child(_ position: Int, _ item: Int, _ node: SrlXmlForeignNode?) throws {
        if let node=node { try charge(Self.contentEntryOverheadBytes); try storage().children.append(SrlXmlForeignChild(position:position,item:item,node:node)) }
    }
    // Require every declared slot minimum before returning a complete owning candidate.
    func finish() throws {
        for index in counts.indices { guard counts[index] >= meta.slots[index].minimum else { throw SerializerError.invalid("Required XML extension occurrence missing") } }
    }
    // Revalidate retained attributes while the owning start tag is still pending.
    func emitAttributes(_ output: SrlXmlOutput) throws {
        guard let selected=state else { return }
        for value in selected.attributes {
            guard try admit(value.name,true,0) else { throw SerializerError.invalid("Retained XML attribute no longer has a preserve slot") }
            try charge(attributeBytes(value)); try output.attribute(value)
        }
    }
    // Replay only foreign semantic nodes; statically known owning fields never enter this storage.
    private func emitNode(_ output: SrlXmlOutput, _ value: SrlXmlForeignNode) throws {
        try measure(value.name,value.attributes,value.scope); try output.open(value.name,value.scope)
        for attribute in value.attributes { try output.attribute(attribute) }
        for content in value.content {
            try charge(Self.contentEntryOverheadBytes)
            switch content {
            case .text(let text): _=try srlXmlEscape(text,false); try charge(text.utf8.count); try output.text(text)
            case .element(let node): try emitNode(output,node)
            }
        }
        try output.close()
    }
    // Emit exactly the retained roots at the current field and repeated-item boundary.
    func emitAt(_ output: SrlXmlOutput, _ position: Int, _ item: Int) throws {
        guard let selected=state else { return }
        while cursor < selected.children.count {
            let value=selected.children[cursor]
            guard value.position > position || value.position == position && value.item >= item else { throw SerializerError.invalid("Preserved XML extension no longer fits edited field position") }
            if value.position != position || value.item != item { return }
            guard try admit(value.node.name,false,position) else { throw SerializerError.invalid("Retained XML child no longer has a preserve slot") }
            try charge(Self.contentEntryOverheadBytes); try emitNode(output,value.node); cursor += 1
        }
    }
    // Reject boundaries lost after native repeated storage was shortened or removed.
    func endPosition(_ position: Int) throws {
        guard state == nil || cursor >= state!.children.count || state!.children[cursor].position != position else { throw SerializerError.invalid("Preserved XML extension no longer fits edited field position") }
    }
    // Require complete replay and every minimum before publishing encoded bytes.
    func finishOutput() throws {
        guard state == nil || cursor == state!.children.count else { throw SerializerError.invalid("Unreplayed XML extension position") }; try finish()
    }
    // Start an input tracker in the exact original complete owner namespace scope.
    static func input(_ meta: SrlXmlExtensionContract, _ scope: [WireString:String], _ options: XmlExtensionOptions) throws -> SrlXmlExtensions {
        try SrlXmlExtensions(meta,scope,options,nil)
    }
    // Borrow the original XML-only sidecar for synchronous output without cloning opaque payloads.
    static func output(_ meta: SrlXmlExtensionContract, _ state: SrlXmlPreserved?, _ options: XmlExtensionOptions) throws -> SrlXmlExtensions {
        try SrlXmlExtensions(meta,state?.scope ?? [WireString("xml"):"http://www.w3.org/XML/1998/namespace"],options,state)
    }
}
)xmlextension"
