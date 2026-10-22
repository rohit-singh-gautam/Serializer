// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlextension(
extension SrlXmlExtensions {
    // Select an exact foreign slot without crossing a missing required field or a consumed scalar field.
    fileprivate func position(_ name: SrlXmlName, _ cursor: Int, _ counts: [Int], _ minimums: [Int],
        _ positions: [Int], _ repeated: [Bool], _ fieldCount: Int) throws -> (position:Int,item:Int,cursor:Int) {
        var selected=cursor
        while selected < positions.count {
            if let index=slot(name,false,positions[selected]), counts[selected] == 0 || repeated[selected] && (meta.slots[index].match == 0 || meta.slots[index].match == 4) {
                return (positions[selected],counts[selected],selected)
            }
            guard counts[selected] >= minimums[selected] else { throw SerializerError.invalid("Required XML field missing before extension slot") }
            selected += 1
        }
        guard slot(name,false,fieldCount) != nil else { throw SerializerError.invalid("XML extension has no permitted slot") }
        return (fieldCount,0,positions.count)
    }
}

// Capture only an admitted opaque subtree; skip frames retain no attribute, namespace or text payload.
fileprivate func srlXmlForeignFrame(_ name: SrlXmlName, _ attributes: [SrlXmlAttribute],
    _ scope: [WireString:String], _ tracker: SrlXmlExtensions, _ preserve: Bool,
    _ validation: Int = 0,
    _ complete: @escaping (SrlXmlForeignNode?) throws -> Void) throws -> SrlXmlTokenFrame {
    let frame=SrlXmlTokenFrame(name)
    let node: SrlXmlForeignNode?
    let capture=preserve || validation != 0
    let temporary=validation != 0 && !preserve
    if temporary { tracker.beginTemporary() }
    if capture { try tracker.measure(name,attributes,scope); node=SrlXmlForeignNode(name,attributes,scope) }
    else { node=nil }
    var pending=""
    // Join provider chunks, comments and CDATA into one semantic text run between opaque child elements.
    func flush() throws {
        if !pending.isEmpty {
            try tracker.charge(SrlXmlExtensions.contentEntryOverheadBytes)
            node!.content.append(.text(pending)); pending=""
        }
    }
    frame.knownChildren=[]
    frame.foreignChild = { childName, childAttributes, namespaceScope in
        if capture { try flush() }
        if capture { try tracker.charge(SrlXmlExtensions.contentEntryOverheadBytes) }
        return try srlXmlForeignFrame(childName,childAttributes,capture ? namespaceScope() : [:],tracker,capture) { value in
            if let value=value { node!.content.append(.element(value)) }
        }
    }
    frame.text = { text in
        if capture { try tracker.charge(text.utf8.count); pending.append(contentsOf:text) }
    }
    frame.finish = { defer { if temporary { tracker.endTemporary() } };if capture { try flush() };if validation != 0, let node=node { try tracker.validate(node,false,validation) }; try complete(preserve ? node : nil) }
    return frame
}
)xmlextension"
