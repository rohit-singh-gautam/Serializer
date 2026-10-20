// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

typealias P = SerializerPackage
typealias A = RelocationArchive
var checks = 0
let previous = Array("previous destination".utf8)

// Keep assertions active in optimized SDK executions.
func check(_ value: Bool, _ message: String) {
    if !value { fatalError(message) }; checks += 1
}

// Retain an independent byte snapshot of all public preservation records.
func snapshot(_ extensions: P.Extensions) -> [UInt8] {
    var text = ""
    for part in extensions.parts {
        text += part.name+"|"+part.contentType+"|"+String(part.sourceIndex)+"|"+String(describing:part.compression)+"|"
        text += part.data.map { String(format:"%02x",$0) }.joined()+"\n"
    }
    for link in extensions.relationships { text += link.source+"|"+link.id+"|"+link.type+"|"+link.target+"|"+String(link.external)+"\n" }
    return Array(text.utf8)
}

// Match only structured package failures so assertion failures cannot produce false acceptance.
func rejected(_ operation: () throws -> Void) throws {
    do { try operation() } catch is P.Failure { checks += 1; return }
    fatalError("Invalid relocation accepted")
}

// Replay exact observed work/storage counters and one-tight independent policies.
func bounds(_ input: A.Input) throws {
    let observed = try P.Budget(); _ = try A.encode(input,budget:observed)
    for resource in [P.Resource.workUnits,.decoderAllocationBytes] {
        let maximum = resource == .workUnits ? observed.configured.maxWorkUnits : observed.configured.maxDecoderAllocationBytes
        let used = maximum-observed.remaining(resource); check(used>0,"Positive admission")
        for allowance in [used,used-1] {
            var limits = P.Limits()
            if resource == .workUnits { limits.maxWorkUnits = allowance } else { limits.maxDecoderAllocationBytes = allowance }
            var failed = false
            do { _ = try A.encode(input,budget:P.Budget(limits)) }
            catch let error as P.Failure { check(error.code == .resourceLimit,"Quota classification"); failed = true }
            check(failed == (allowance<used),"Exact and one-tight cumulative policy")
        }
    }
}

final class CallerFailure: Error {}

// Original caller errors must survive Save and leave the previous destination intact.
func cancellation(_ input: A.Input, _ destination: URL) throws {
    try Data(previous).write(to:destination); let marker = CallerFailure(); var armed = false
    var limits = P.Limits(); limits.checkCancelled = { if armed { throw marker } }
    let budget = try P.Budget(limits); armed = true
    do { try A.save(input,to:destination,budget:budget); fatalError("Cancelled Save accepted") }
    catch let error as CallerFailure { check(error === marker,"Exact cancellation object") }
    check(Array(try Data(contentsOf:destination)) == previous,"Cancelled Save retained destination")
}

// Keep combining marks after URI delimiters intact through loaded relocation and actual Save metadata.
func unicodeSelectors(_ bytes: [UInt8], _ directory: URL, _ policy: P.ExtensionPolicy) throws {
    for target in ["?\u{0301}#query", "#\u{0301}", "other.xml?\u{0301}#path"] {
        let expected = target.hasPrefix("other") ? "/alternate/other.xml" : "/alternate/first.xml"
        check(try P.resolve("/alternate/first.xml",target) == expected,"Scalar URI delimiter boundary")
    }
    let original = try P.Opc(P.ZipReader.open(bytes)); defer { original.archive.close() }
    let parts = try original.parts.map { P.BinaryPart(try original.read($0.name),name:$0.name,contentType:$0.contentType) }
    let suffixes = ["self":"?\u{0301}#query", "fragment":"#\u{0301}", "outbound":"?\u{0301}#path"]
    let links = original.relationships.map { link -> P.Relationship in
        guard let suffix = suffixes[link.id] else { return link }
        let target = link.id == "outbound" ? "../assets/blob.bin"+suffix : suffix
        return P.Relationship(source:link.source,id:link.id,type:link.type,target:target,external:link.external)
    }
    let authored = try P.opcWrite(parts,relationships:links)
    let input = try A.decode(authored,policy:policy), before = snapshot(input.extensions)
    let encoded = try A.encode(input)
    let encodedPath = directory.appendingPathComponent("swift-unicode-selectors-encoded.opc")
    let savedPath = directory.appendingPathComponent("swift-unicode-selectors-saved.opc")
    try Data(encoded).write(to:encodedPath); try A.save(input,to:savedPath)
    for output in [encoded,Array(try Data(contentsOf:savedPath))] {
        let catalog = try P.Opc(P.ZipReader.open(output)); defer { catalog.archive.close() }
        for (id,suffix) in suffixes {
            let expected = (id == "outbound" ? "/assets/blob.bin" : "/left/A.xml")+suffix
            let link = catalog.relationships.first { $0.source == "/left/A.xml" && $0.id == id }
            check(link?.target.utf8.elementsEqual(expected.utf8) == true,"Exact encoded and saved Unicode suffix")
        }
    }
    check(snapshot(input.extensions) == before,"Unicode selector input records unchanged")
}

// Produce one stable machine-readable decision for the independent Python ZIP/XML consumer.
func report(_ name: String, _ expected: String, _ stage: String = "") throws {
    var result: [String:Any] = ["case":name,"decision":expected,"checks":checks]
    if !stage.isEmpty { result["failure_stage"] = stage }
    let data = try JSONSerialization.data(withJSONObject:result,options:.sortedKeys)
    print(String(decoding:data,as:UTF8.self))
}

// Use only public owning facade APIs, preserving immutable extension snapshots across repeated Save.
func main() throws {
    let arguments = CommandLine.arguments, directory = URL(fileURLWithPath:arguments[1],isDirectory:true)
    let name = arguments[2], action = arguments[3], expected = arguments[4]
    let destination = directory.appendingPathComponent(name+"-saved.opc"); try Data(previous).write(to:destination)
    var policy = P.ExtensionPolicy(); policy.parts = .preserve; policy.relationships = .preserve
    let loaded: A.Input
    do { loaded = try A.decode(Array(Data(contentsOf:directory.appendingPathComponent(name+".opc"))),policy:policy) }
    catch is P.Failure {
        check(expected == "LOAD_ERROR" || expected == "REJECT","Unexpected load failure")
        check(Array(try Data(contentsOf:destination)) == previous,"Rejected load retained destination")
        try report(name,expected,"load"); return
    }
    check(expected != "LOAD_ERROR","Invalid load published")
    check(loaded.first?.id == 11 && loaded.first?.label == "first","First payload")
    check(loaded.second?.id == 22 && loaded.second?.label == "second","Second payload")
    check(loaded.children.count == 1 && loaded.children[0].value.id == 33 && loaded.children[0].sourceIndex == 0,"Dynamic child routing")
    var input = A.Input(); input.first = loaded.first; input.second = action == "remove_second" ? nil : loaded.second
    input.children = loaded.children; input.extensions = loaded.extensions; input.profiles = loaded.profiles
    let before = snapshot(input.extensions), child = input.children[0].name
    if expected == "REJECT" {
        try rejected { _ = try A.encode(input) }; try rejected { try A.save(input,to:destination) }
        check(Array(try Data(contentsOf:destination)) == previous,"Rejected Save retained destination")
    } else {
        check(expected == "OK","Unknown expectation")
        try Data(A.encode(input)).write(to:directory.appendingPathComponent(name+"-encoded.opc"))
        try Data(A.encode(input)).write(to:directory.appendingPathComponent(name+"-repeat.opc"))
        try A.save(input,to:destination)
        if name == "alternate" {
            try bounds(input); try cancellation(input,directory.appendingPathComponent(name+"-cancel.opc"))
            try unicodeSelectors(Array(Data(contentsOf:directory.appendingPathComponent(name+".opc"))),directory,policy)
        }
    }
    check(snapshot(input.extensions) == before,"Preserved inputs mutated")
    check(input.children[0].name == child,"Dynamic child renamed")
    check(input.first?.id == 11 && input.first?.label == "first","Typed payload mutated")
    try report(name,expected)
}

try main()
