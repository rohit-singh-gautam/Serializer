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
    fileprivate var srlDeclarationPolicy: SrlXmlDeclarationPolicy?
    fileprivate var srlDeclarationTemporary = false
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
// Borrow the original independent XML policies while every private decoder shares caller admission.
fileprivate final class SrlXmlDeclarationPolicy {
    let limits: XmlLimits, map: XmlMapLimits, mixed: XmlMixedLimits, digest: XmlDigestLimits, identity: XmlIdentityLimits
    init(_ limits: XmlLimits, _ map: XmlMapLimits, _ mixed: XmlMixedLimits, _ digest: XmlDigestLimits, _ identity: XmlIdentityLimits) {
        var selected=limits;selected.srlPackageRootType=nil;selected.srlPackageVocabularies=nil;selected.srlPackageRequiresType=false;selected.srlPackageNamespaceScope=nil;selected.srlDeclarationTemporary=true
        self.limits=selected;self.map=map;self.mixed=mixed;self.digest=digest;self.identity=identity
    }
}
// Select the ordinary or attributes-only private declaration codec without retaining a payload model.
fileprivate struct SrlXmlDeclaredCodec {
    let nillable: Bool, abstract: Bool, fixed: Bool
    let type: SrlXmlName?
    let root: SrlXmlName
    let abstractType: Bool, types: [SrlXmlName:SrlXmlDeclaredType]
    let read: ([UInt8],SrlXmlDeclarationPolicy,XmlExtensionOptions) throws -> Void
    let readNil: (([UInt8],SrlXmlDeclarationPolicy,XmlExtensionOptions) throws -> Void)?
    init(_ nillable: Bool, _ abstract: Bool, _ fixed: Bool, _ type: SrlXmlName?, _ root: SrlXmlName,
         _ abstractType: Bool, _ types: [SrlXmlName:SrlXmlDeclaredType],
         _ read: @escaping ([UInt8],SrlXmlDeclarationPolicy,XmlExtensionOptions) throws -> Void,
         _ readNil: (([UInt8],SrlXmlDeclarationPolicy,XmlExtensionOptions) throws -> Void)?) {
        self.nillable=nillable;self.abstract=abstract;self.fixed=fixed;self.type=type;self.root=root;self.abstractType=abstractType;self.types=types;self.read=read;self.readNil=readNil
    }
}
// Retain each permitted named xsi:type's complete ordinary/nil validator selection.
fileprivate struct SrlXmlDeclaredType {
    let root: SrlXmlName, abstract: Bool
    let read: ([UInt8],SrlXmlDeclarationPolicy,XmlExtensionOptions) throws -> Void
    let readNil: (([UInt8],SrlXmlDeclarationPolicy,XmlExtensionOptions) throws -> Void)?
    init(_ root: SrlXmlName, _ abstract: Bool, _ read: @escaping ([UInt8],SrlXmlDeclarationPolicy,XmlExtensionOptions) throws -> Void,
         _ readNil: (([UInt8],SrlXmlDeclarationPolicy,XmlExtensionOptions) throws -> Void)?) {
        self.root=root;self.abstract=abstract;self.read=read;self.readNil=readNil
    }
}
fileprivate struct SrlXmlExtensionSlot {
    let id: String, attribute: Bool, position: Int, namespaces: [String], policy: Int, minimum: Int, maximum: Int?, match: Int, process: Int
    // Retain checked compiler-owned URI permissions and exact occurrence intervals.
    init(_ id: String, _ attribute: Bool, _ position: Int, _ namespaces: [String], _ policy: Int, _ minimum: Int, _ maximum: Int?, _ match: Int, _ process: Int) {
        self.id=id; self.attribute=attribute; self.position=position; self.namespaces=namespaces
        self.policy=policy; self.minimum=minimum; self.maximum=maximum
        self.match=match; self.process=process
    }
}
fileprivate struct SrlXmlExtensionContract {
    let core: String, known: [SrlXmlName], required: [String], slots: [SrlXmlExtensionSlot], knownPositions: [Int]
    let knownNames: Set<SrlXmlName>, knownAttributeNames: Set<SrlXmlName>, attributeSlots: [WireString:Int], childSlots: [WireString:[Int:Int]]
    let declarations: ((SrlXmlName,Bool) -> SrlXmlDeclaredCodec?)?
    // Keep known names closed independently of foreign slot namespace inventories.
    init(_ core: String, _ known: [SrlXmlName], _ knownAttributes: [SrlXmlName], _ required: [String], _ slots: [SrlXmlExtensionSlot], _ knownPositions: [Int], _ declarations: ((SrlXmlName,Bool) -> SrlXmlDeclaredCodec?)? = nil) {
        self.core=core; self.known=known; self.required=required; self.slots=slots; self.knownPositions=knownPositions
        knownNames=Set(known)
        knownAttributeNames=Set(knownAttributes)
        self.declarations=declarations
        var attributes: [WireString:Int]=[:], children: [WireString:[Int:Int]]=[:]
        for (index,slot) in slots.enumerated() {
            if slot.match == 1 || slot.match == 3 || slot.match == 4 { continue }
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
    private(set) var process=0
    private var temporaryDepth=0, temporaryBytes=0
    private static let ownerOverheadBytes=64, nodeOverheadBytes=64, scopeEntryOverheadBytes=64, attributeOverheadBytes=48
    static let contentEntryOverheadBytes=24
    // Check required capabilities in the original namespace scope before consuming native model fields.
    private init(_ meta: SrlXmlExtensionContract, _ scope: [WireString:String], _ options: XmlExtensionOptions, _ existing: SrlXmlPreserved?) throws {
        self.meta=meta; self.scope=scope; self.options=options; state=existing
        temporaryDepth=options.srlDeclarationTemporary ? 1 : 0
        try options.validate()
        guard meta.slots.count <= options.limits.maxSlots, meta.known.count <= options.limits.maxItems, meta.knownAttributeNames.count <= options.limits.maxItems,
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
        let maximum=byteLimit
        guard current >= 0 && amount >= 0 && current <= maximum,
              amount <= maximum-current else { throw srlXmlLimitFailure(options.srlPackageLimit,temporaryDepth == 0 ? "XML extension preserved-byte limit" : "XML declaration temporary-byte limit") }
        return current+amount
    }
    // Bound validation workspace by the original document's byte, text and node capacities.
    private var byteLimit: Int {
        if temporaryDepth == 0 { return options.limits.maxPreservedBytes }
        let limits=options.srlDeclarationPolicy?.limits ?? XmlLimits()
        let nodes=limits.maxNodes.multipliedReportingOverflow(by:128)
        if nodes.overflow { return Int.max }
        let text=limits.maxBytes.addingReportingOverflow(limits.maxTextBytes)
        if text.overflow { return Int.max }
        let total=text.partialValue.addingReportingOverflow(nodes.partialValue)
        return total.overflow ? Int.max : total.partialValue
    }
    // Mark validation-only capture without borrowing a caller's retained-payload allowance.
    func beginTemporary() { temporaryDepth += 1 }
    // End synchronous validation-only capture after its complete private decoder returns.
    func endTemporary() { temporaryDepth -= 1 }
    // Admit logical bytes before any corresponding owning payload allocation.
    func charge(_ amount: Int) throws {
        let next=try addSize(temporaryDepth == 0 ? bytes : temporaryBytes,amount)
        if temporaryDepth == 0 { try options.srlPackageConsume?("preserved",amount);bytes=next } else { temporaryBytes=next }
        try options.srlPackageConsume?("allocation",amount);try options.srlPackageConsume?("work",amount)
    }
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
        guard name.namespace != srlXmlModelNamespace, !attribute || !meta.knownAttributeNames.contains(name) else { return nil }
        func knownAllowed(_ index: Int) -> Bool {
            if attribute || !meta.knownNames.contains(name) { return true }
            let slot=meta.slots[index],known=meta.known.firstIndex(of:name)!
            return slot.match != 0 && slot.match != 4 && (known < meta.knownPositions.count && meta.knownPositions[known] >= 0 && meta.knownPositions[known] < position || counts[index] < slot.minimum)
        }
        if let direct = attribute ? meta.attributeSlots[WireString(name.namespace)] : meta.childSlots[WireString(name.namespace)]?[position], knownAllowed(direct) { return direct }
        return meta.slots.indices.first { index in let slot=meta.slots[index]; return slot.attribute == attribute && (attribute || slot.position < 0 || slot.position == position) && (slot.match == 1 || (slot.match == 3 || slot.match == 4) && !slot.namespaces.contains(where: { WireString($0) == WireString(name.namespace) })) && knownAllowed(index) }
    }
    // Admit one unknown root's policy, item count and occurrence before capture or skip begins.
    func admit(_ name: SrlXmlName, _ attribute: Bool, _ position: Int) throws -> Bool {
        guard let index=slot(name,attribute,position) else { throw SerializerError.invalid("XML extension has no permitted slot") }
        let selected=meta.slots[index]
        guard selected.policy != 0 else { throw SerializerError.invalid("XML extension slot rejects foreign vocabulary") }
        guard items < options.limits.maxItems else { throw srlXmlLimitFailure(options.srlPackageLimit,"XML extension item/occurrence limit") }
        guard selected.maximum == nil || counts[index] < selected.maximum! else { throw SerializerError.invalid("XML extension item/occurrence limit") }
        items += 1; counts[index] += 1;process=selected.process
        if process != 0 && meta.declarations?(name,attribute) == nil {
            guard process != 2 else { throw SerializerError.invalid("Strict XML wildcard requires a global declaration") };process=0
        }
        return selected.policy == 2
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
        let preserve=try admit(value.name,true,0)
        if process != 0 {
            let node=SrlXmlForeignNode(value.name,[value],scope)
            try validate(node,true,process)
        }
        if preserve { try charge(attributeBytes(value)); try storage().attributes.append(value) }
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
    // Validate strict/lax declarations through their generated DTO codecs using the same operation.
    func validate(_ value: SrlXmlForeignNode, _ attribute: Bool, _ process: Int) throws {
        if process == 0 { return }
        guard let codec=meta.declarations?(value.name,attribute) else {
            guard process != 2 else { throw SerializerError.invalid("Strict XML wildcard requires a global declaration") };return
        }
        guard !codec.abstract else { throw SerializerError.invalid("Abstract XML declaration cannot appear directly") }
        guard let policy=options.srlDeclarationPolicy else { throw SerializerError.invalid("Missing XML declaration operation policy") }
        beginTemporary();defer { endTemporary() }
        var node=value,read=codec.read,readNil=codec.readNil,abstractType=codec.abstractType,root=codec.root
        let typeName=SrlXmlName(local:"type",namespace:"http://www.w3.org/2001/XMLSchema-instance")
        if let typeValue=node.attributes.first(where:{$0.name == typeName}) {
            guard let identity=typeValue.typeIdentity else { throw SerializerError.invalid("Unexpected declared XML type") }
            if identity != codec.type {
                guard let selected=codec.types[identity] else { throw SerializerError.invalid("Unexpected declared XML type") }
                read=selected.read;readNil=selected.readNil;abstractType=selected.abstract;root=selected.root
            }
            try charge(node.attributes.count*48)
            let selected=SrlXmlForeignNode(node.name,node.attributes.filter{$0.name != typeName},node.scope);selected.content=node.content;node=selected
        }
        guard !abstractType else { throw SerializerError.invalid("Abstract XML type cannot appear directly") }
        if node.name != root { let selected=SrlXmlForeignNode(root,node.attributes,node.scope);selected.content=node.content;node=selected }
        let nilName=SrlXmlName(local:"nil",namespace:"http://www.w3.org/2001/XMLSchema-instance")
        if let lexical=node.attributes.first(where:{$0.name == nilName})?.value {
            let nilValue=lexical.trimmingCharacters(in:CharacterSet(charactersIn:" \t\r\n"))
            guard codec.nillable, ["true","false","1","0"].contains(nilValue) else { throw SerializerError.invalid("Invalid declared XML nil state") }
            if nilValue == "true" || nilValue == "1" {
                guard !codec.fixed, value.content.allSatisfy({ content in if case .text(let text)=content { return text.isEmpty };return false }), let nilReader=readNil else { throw SerializerError.invalid("Nil XML declaration contains a value") }
                read=nilReader
            }
            try charge(node.attributes.count*48)
            let selected=SrlXmlForeignNode(node.name,node.attributes.filter{$0.name != nilName},node.scope);selected.content=node.content;node=selected
        }
        let output=try SrlXmlOutput(policy.limits,policy.map,policy.mixed,policy.digest,policy.identity,options)
        try emitNode(output,node)
        try read(output.finish(),policy,options)
    }
    // Revalidate retained attributes while the owning start tag is still pending.
    func emitAttributes(_ output: SrlXmlOutput) throws {
        guard let selected=state else { return }
        for value in selected.attributes {
            guard try admit(value.name,true,0) else { throw SerializerError.invalid("Retained XML attribute no longer has a preserve slot") }
            if process != 0 { let node=SrlXmlForeignNode(value.name,[value],scope);try validate(node,true,process) }
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
            try validate(value.node,false,process)
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
