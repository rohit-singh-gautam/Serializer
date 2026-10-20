// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlnamespace(
// Copy namespace presentation with its owning value while keeping native wire data unchanged.
fileprivate struct SrlXmlNamespaceState {
    var owner: [WireString:String] = [:]
    var fields: [String:[[WireString:String]]] = [:]
    // Append only after a complete scalar validates in its original provider scope.
    mutating func scalar(_ key: String, _ scope: [WireString:String]) {
        fields[key,default:[]].append(scope)
    }
}
// Keep occurrence counters local to one synchronous traversal, never in the reusable native value.
fileprivate final class SrlXmlNamespaceOutput {
    private let state: SrlXmlNamespaceState
    let owner: [WireString:String]
    private var next: [String:Int] = [:]
    // Merge opaque and typed lexical vocabularies only when shared prefixes agree byte-for-byte.
    init(_ state: SrlXmlNamespaceState, _ preserved: [WireString:String]? = nil) throws {
        self.state=state; var merged=preserved ?? [:]
        for (prefix,uri) in state.owner {
            guard merged[prefix] == nil || WireString(merged[prefix]!) == WireString(uri) else { throw SerializerError.invalid("Conflicting XML namespace sidecars") }
            merged[prefix]=uri
        }
        owner=merged
    }
    // Recover scalar scopes in field order; newly authored occurrences use the owner's vocabulary.
    func scope(_ key: String) -> [WireString:String] {
        let index=next[key,default:0]; next[key]=index+1
        guard let scopes=state.fields[key], index < scopes.count else { return owner }
        return scopes[index]
    }
}
// Resolve QName and prefix-list values using Unicode scalar names and byte-exact namespace identities.
fileprivate func srlXmlNamespaceChecked(_ text: String, _ list: Bool, _ prefixes: Bool, _ namespace: (String) -> String?, _ maximum: Int) throws -> String {
    let values=(list || prefixes) ? try SrlXmlRule.tokens(text,maximum) : [text]
    for value in values {
        if prefixes {
            guard srlXmlNcName(value), value != "xmlns", value == "xml" || !(namespace(value) ?? "").isEmpty else { throw SerializerError.invalid("Unbound XML prefix-list token") }
        } else {
            guard srlXmlTypeQName(value,namespace) != nil else { throw SerializerError.invalid("Invalid or unbound XML QName") }
        }
    }
    return text
}
// Borrow the original input snapshot without allocating another dictionary for each validation.
fileprivate func srlXmlNamespaceChecked(_ text: String, _ list: Bool, _ prefixes: Bool, _ scope: [WireString:String], _ maximum: Int) throws -> String {
    try srlXmlNamespaceChecked(text,list,prefixes,{ scope[WireString($0)] },maximum)
}
// Capture a scalar's namespace vocabulary before its provider declarations leave scope.
fileprivate func srlXmlNamespaceScalarFrame(_ name: SrlXmlName, _ attributes: [SrlXmlAttribute],
    _ complete: @escaping (String,[WireString:String]) throws -> Void) throws -> SrlXmlTokenFrame {
    guard attributes.isEmpty else { throw SerializerError.invalid("XML scalar contains attributes") }
    let frame=SrlXmlTokenFrame(name); var text="", scope: [WireString:String] = [:]
    frame.namespaceValue=true
    frame.scopeStart={ scope=$0; if scope[WireString("")] == nil { scope[WireString("")]="" } }
    frame.text={ text.append(contentsOf:$0) }
    frame.finish={ try complete(text,scope) }
    return frame
}
)xmlnamespace"
