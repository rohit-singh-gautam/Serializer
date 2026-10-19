// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"identityswift(
// Bound one operation-local identity index independently of the ordinary document/parser limits.
public struct XmlIdentityLimits {
    public let maxConstraints: Int, maxFields: Int, maxDepth: Int, maxSelectedNodes: Int
    public let maxTuples: Int, maxTupleBytes: Int, maxWorkUnits: Int
    // Nil arguments select compiler-emitted shared defaults; explicit zero prohibits the resource.
    public init(maxConstraints: Int? = nil, maxFields: Int? = nil, maxDepth: Int? = nil,
        maxSelectedNodes: Int? = nil, maxTuples: Int? = nil, maxTupleBytes: Int? = nil, maxWorkUnits: Int? = nil) {
        self.maxConstraints=maxConstraints ?? srlXmlIdentityMaxConstraints
        self.maxFields=maxFields ?? srlXmlIdentityMaxFields; self.maxDepth=maxDepth ?? srlXmlIdentityMaxDepth
        self.maxSelectedNodes=maxSelectedNodes ?? srlXmlIdentityMaxSelectedNodes
        self.maxTuples=maxTuples ?? srlXmlIdentityMaxTuples; self.maxTupleBytes=maxTupleBytes ?? srlXmlIdentityMaxTupleBytes
        self.maxWorkUnits=maxWorkUnits ?? srlXmlIdentityMaxWorkUnits
    }
    // Reject negative admission before constructing or observing any owning candidate.
    fileprivate func validate() throws {
        guard [maxConstraints,maxFields,maxDepth,maxSelectedNodes,maxTuples,maxTupleBytes,maxWorkUnits].allSatisfy({ $0 >= 0 }) else { throw SerializerError.invalid("Negative XML identity limit") }
    }
}
// Compiler-owned expanded paths contain no runtime XPath syntax or mutable document state.
fileprivate struct SrlXmlIdentityPath {
    let steps: [SrlXmlName], descendants: Bool, attribute: SrlXmlName?
    // Retain exact names for one prepared relative path.
    init(_ steps: [SrlXmlName], _ descendants: Bool, _ attribute: SrlXmlName?) { self.steps=steps; self.descendants=descendants; self.attribute=attribute }
}
fileprivate struct SrlXmlIdentityField {
    let path: SrlXmlIdentityPath, kind: String, nillable: Bool, rule: SrlXmlRule
    // Preserve scalar value space independently of its generated native storage type.
    init(_ path: SrlXmlIdentityPath, _ kind: String, _ nillable: Bool) {
        self.path=path; self.kind=kind; self.nillable=nillable
        rule=SrlXmlRule(kind,kind == "string" ? "preserve" : kind == "normalizedString" ? "replace" : "collapse",["","","",""],-1,-1,-1,[],false,[],"",[],nil)
    }
}
fileprivate struct SrlXmlIdentityConstraint {
    let kind: String, selector: SrlXmlIdentityPath, fields: [SrlXmlIdentityField], refer: Int
    // Bind checked owner-local target indexes and ordered tuple fields.
    init(_ kind: String, _ selector: SrlXmlIdentityPath, _ fields: [SrlXmlIdentityField], _ refer: Int) { self.kind=kind; self.selector=selector; self.fields=fields; self.refer=refer }
}
fileprivate struct SrlXmlIdentityScope {
    let constraints: [SrlXmlIdentityConstraint]
    // Retain metadata only; the current generated field may override the owning element's root spelling.
    init(_ constraints: [SrlXmlIdentityConstraint]) { self.constraints=constraints }
}

// Observe direct typed events, retaining bounded tuples/open scalar captures without known-element trees.
fileprivate final class SrlXmlIdentity {
    private final class Capture {
        let depth: Int, isNil: Bool
        var text="", bytes=0
        // Capture exactly one selected scalar at its active element depth.
        init(_ depth: Int, _ isNil: Bool) { self.depth=depth; self.isNil=isNil }
    }
    private final class Candidate {
        let constraint: Int, depth: Int
        var values: [String?], seen: [Bool], captures: [Capture?], bytes=0
        // Selection admission precedes every field-sized allocation.
        init(_ constraint: Int, _ depth: Int, _ count: Int) {
            self.constraint=constraint; self.depth=depth
            values=Array(repeating:nil,count:count); seen=Array(repeating:false,count:count); captures=Array(repeating:nil,count:count)
        }
    }
    private let scope: SrlXmlIdentityScope, limits: XmlIdentityLimits
    private var path: [SrlXmlName]=[], active: [Candidate]=[], tables: [Set<WireString>]
    private var selected=0, tuples=0, tupleBytes=0, workUnits=0
    private var complete=false, failed=false
    // Allocate only admitted prepared tables for this independent owner scope.
    init(_ scope: SrlXmlIdentityScope, _ limits: XmlIdentityLimits) throws {
        try limits.validate(); self.scope=scope; self.limits=limits
        guard scope.constraints.count <= limits.maxConstraints else { throw SerializerError.invalid("XML identity constraint limit") }
        for item in scope.constraints {
            guard !item.fields.isEmpty && item.fields.count <= limits.maxFields else { throw SerializerError.invalid("XML identity field limit") }
            guard item.kind != "keyref" || scope.constraints.indices.contains(item.refer) && scope.constraints[item.refer].kind != "keyref" else { throw SerializerError.invalid("XML keyref target outside owner scope") }
        }
        tables=Array(repeating:Set<WireString>(),count:scope.constraints.count)
    }
    // Charge before scanning names or retaining values; subtraction keeps exact zero/maximum policies safe.
    private func work(_ count: Int) throws {
        guard count >= 0 && count <= limits.maxWorkUnits-workUnits else { throw SerializerError.invalid("XML identity work limit") }; workUnits += count
    }
    // Account for complete expanded identities, including long common namespace URI prefixes.
    private func nameBytes(_ name: SrlXmlName) -> Int { name.local.utf8.count+name.namespace.utf8.count+2 }
    // Compare prepared relative paths without allocating suffix arrays or interpreting XPath strings.
    private func matches(_ first: Int, _ expected: SrlXmlIdentityPath) throws -> Bool {
        let count=path.count-first; try work(count+expected.steps.count+1)
        guard expected.descendants ? count >= expected.steps.count : count == expected.steps.count else { return false }
        let offset=expected.descendants ? path.count-expected.steps.count : first
        for index in expected.steps.indices {
            let actual=path[offset+index], wanted=expected.steps[index]
            try work(min(nameBytes(actual),nameBytes(wanted))+1); if actual != wanted { return false }
        }
        return true
    }
    // Remove decimal presentation zeros using arbitrary-length digits, preserving exact signed value equality.
    private func decimal(_ value: String) -> String {
        let raw=Array(value.utf8); let negative=raw.first == 45
        let start=raw.first == 43 || raw.first == 45 ? 1 : 0
        let dot=raw[start...].firstIndex(of:46) ?? raw.count
        var first=start, last=raw.count
        while first < dot && raw[first] == 48 { first += 1 }
        while last > dot+1 && raw[last-1] == 48 { last -= 1 }
        let whole=first == dot ? "0" : String(decoding:raw[first..<dot],as:UTF8.self)
        let fraction=dot < raw.count && last > dot+1 ? String(decoding:raw[(dot+1)..<last],as:UTF8.self) : ""
        let nonzero=whole != "0" || !fraction.isEmpty
        return "d"+(negative && nonzero ? "-" : "")+whole+(fraction.isEmpty ? "" : "."+fraction)
    }
    // Canonicalize exact builtin values independently of native widths, locale and QName presentation prefixes.
    private func key(_ raw: String, _ field: SrlXmlIdentityField, _ resolve: (String) -> String?) throws -> String {
        let bytes=raw.utf8.count; guard bytes <= limits.maxTupleBytes else { throw SerializerError.invalid("XML identity field-byte limit") }; try work(bytes+1)
        if field.kind == "QName" {
            let text=raw.trimmingCharacters(in:CharacterSet(charactersIn:" \t\r\n"))
            let parts=text.split(separator:":",omittingEmptySubsequences:false).map(String.init)
            guard (1...2).contains(parts.count) && parts.allSatisfy({ !$0.isEmpty }) else { throw SerializerError.invalid("Invalid XML identity QName") }
            let rule=SrlXmlRule("NCName","collapse",["","","",""],-1,-1,-1,[],false,[],"",[],nil)
            for part in parts { _=try rule.check(part) }
            let prefix=parts.count == 2 ? parts[0] : "", uri=resolve(prefix)
            guard prefix.isEmpty || uri != nil else { throw SerializerError.invalid("Undeclared XML identity QName prefix") }
            return "q"+(uri ?? "")+"\0"+parts.last!
        }
        let value=try field.rule.check(raw), kind=field.kind
        if kind == "boolean" { return try srlXmlBoolean(value) ? "btrue" : "bfalse" }
        if kind == "decimal" || Self.integerKinds.contains(kind) { return decimal(value) }
        guard kind != "float" && kind != "double" else { throw SerializerError.invalid("Floating XML identity fields are unsupported") }
        return (kind == "anyURI" ? "u" : "s")+value
    }
    // Admit canonical bytes before retaining one partial field; private type tags do not consume tuple bytes.
    private func store(_ candidate: Candidate, _ index: Int, _ value: String) throws {
        let bytes=value.utf8.count-(value.hasPrefix("q") ? 2 : 1)
        guard bytes <= limits.maxTupleBytes-candidate.bytes else { throw SerializerError.invalid("XML identity candidate tuple-byte limit") }
        candidate.bytes += bytes; candidate.values[index]=value
    }
    // Observe original full attributes before generated nil/type consumption or namespace scope removal.
    func start(_ name: SrlXmlName, _ attributes: [SrlXmlAttribute], _ resolve: (String) -> String?) throws {
        do {
            guard !failed && !complete else { throw SerializerError.invalid("XML identity observer inactive") }; try work(nameBytes(name)+1)
            guard path.count < limits.maxDepth else { throw SerializerError.invalid("XML identity depth limit") }
            guard active.allSatisfy({ $0.captures.allSatisfy({ $0 == nil }) }) else { throw SerializerError.invalid("XML identity scalar has complex child content") }
            path.append(name)
            for (index,item) in scope.constraints.enumerated() where try matches(1,item.selector) {
                guard selected < limits.maxSelectedNodes else { throw SerializerError.invalid("XML identity selected-node limit") }; selected += 1
                active.append(Candidate(index,path.count,item.fields.count))
            }
            for candidate in active {
                let constraint=scope.constraints[candidate.constraint]
                for (index,field) in constraint.fields.enumerated() {
                    guard try matches(candidate.depth,field.path) else { continue }
                    let attribute=field.path.attribute.flatMap { wanted in attributes.first { $0.name == wanted } }
                    if field.path.attribute != nil && attribute == nil { continue }
                    guard constraint.kind != "key" || !field.nillable else { throw SerializerError.invalid("XSD key field declaration must not be nillable") }
                    guard !candidate.seen[index] else { throw SerializerError.invalid("XML identity field selects multiple values") }; candidate.seen[index]=true
                    if let attribute=attribute { try store(candidate,index,key(attribute.value,field,resolve)) }
                    else {
                        let lexical=(attributes.first { $0.name == Self.nilName }?.value ?? "false").trimmingCharacters(in:CharacterSet(charactersIn:" \t\r\n"))
                        guard ["true","1","false","0"].contains(lexical) else { throw SerializerError.invalid("Malformed XML identity nil") }
                        candidate.captures[index]=Capture(path.count,lexical == "true" || lexical == "1")
                    }
                }
            }
        } catch { failed=true; throw error }
    }
    // Charge logical UTF8 text independently of provider chunk/entity/CDATA boundaries.
    func text(_ value: String) throws {
        do {
            guard !failed && !complete && !path.isEmpty else { throw SerializerError.invalid("XML identity observer inactive") }
            let bytes=value.utf8.count; try work(bytes)
            for candidate in active { for capture in candidate.captures.compactMap({ $0 }) {
                try work(bytes); guard !capture.isNil || value.isEmpty else { throw SerializerError.invalid("Nil XML identity field has text") }
                guard bytes <= limits.maxTupleBytes-capture.bytes else { throw SerializerError.invalid("XML identity field-byte limit") }
                capture.bytes += bytes; capture.text.append(contentsOf:value)
            } }
        } catch { failed=true; throw error }
    }
    // Close captures while their original namespace vocabulary remains visible to the provider.
    func end(_ resolve: (String) -> String?) throws {
        do {
            guard !failed && !complete && !path.isEmpty else { throw SerializerError.invalid("Unbalanced XML identity end") }; try work(1)
            for candidate in active {
                let constraint=scope.constraints[candidate.constraint]
                for index in candidate.captures.indices {
                    try work(1)
                    if let capture=candidate.captures[index], capture.depth == path.count {
                        if !capture.isNil { try store(candidate,index,key(capture.text,constraint.fields[index],resolve)) }
                        candidate.captures[index]=nil
                    }
                }
            }
            var retained: [Candidate]=[]
            for candidate in active {
                guard candidate.depth == path.count else { retained.append(candidate); continue }
                let constraint=scope.constraints[candidate.constraint]
                if candidate.values.contains(where:{ $0 == nil }) {
                    guard constraint.kind != "key" else { throw SerializerError.invalid("XSD key tuple incomplete or nil") }; continue
                }
                guard tuples < limits.maxTuples else { throw SerializerError.invalid("XML identity tuple-count limit") }
                guard candidate.bytes <= limits.maxTupleBytes-tupleBytes else { throw SerializerError.invalid("XML identity tuple-byte limit") }
                tuples += 1; tupleBytes += candidate.bytes; try work(candidate.bytes+1)
                let tuple=candidate.values.map { value -> String in let text=value!; return String(text.utf8.count)+":"+text }.joined()
                guard tables[candidate.constraint].insert(WireString(tuple)).inserted || constraint.kind == "keyref" else { throw SerializerError.invalid("Duplicate XSD key/unique tuple") }
            }
            active=retained; path.removeLast(); complete=path.isEmpty
        } catch { failed=true; throw error }
    }
    // Resolve complete forward references only after every owner-local target table exists.
    func finish() throws {
        do {
            guard !failed && complete && path.isEmpty && active.isEmpty else { throw SerializerError.invalid("Incomplete or failed XML identity scope") }; try work(1)
            for (index,item) in scope.constraints.enumerated() where item.kind == "keyref" {
                for tuple in tables[index] {
                    try work(tuple.value.utf8.count+1)
                    guard tables[item.refer].contains(tuple) else { throw SerializerError.invalid("Unresolved XSD keyref tuple") }
                }
            }
        } catch { failed=true; throw error }
    }
    private static let nilName=SrlXmlName(local:"nil",namespace:"http://www.w3.org/2001/XMLSchema-instance")
    private static let integerKinds:Set<String>=["byte","unsignedByte","short","unsignedShort","int","unsignedInt","long","unsignedLong","integer","positiveInteger","negativeInteger","nonPositiveInteger","nonNegativeInteger"]
}
)identityswift"
