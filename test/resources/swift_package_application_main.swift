// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

// Replay the same independently authored identity/profile fixtures as every established backend.
func applicationChecks(_ directory: URL) throws {
    let cases = try String(contentsOf:directory.appendingPathComponent("cases.tsv"),encoding:.utf8)
    for row in cases.split(whereSeparator: \.isNewline) {
        let fields = row.split(separator:"\t").map(String.init); checkLabel = "application-"+fields[0]
        let input = try bytes(directory.appendingPathComponent(fields[0]+".opc").path)
        if fields[1] == "OK" { _ = try ContractArchive.decode(input) }
        else {
            do { _ = try ContractArchive.decode(input); fatalError("Application contract accepted: "+fields[0]) }
            catch let failure as P.Failure {
                if fields[1] == "REFERENCE" { require(failure.code == .semantic && !failure.fieldPath.isEmpty,"Reference diagnostic lost scope") }
                else if fields[1] == "CODEC" { require(failure.code == .codec,"Wrong package codec identity diagnostic") }
                else { require(failure.code == .invalidOptions,"Fixed compression identity changed") }
                if fields[0] == "missing" { require(failure.part == "/custom/requests.xml" && failure.relationshipId == "authored-request","Missing target lost authored source/link") }
                if fields[0] == "duplicate" { require(failure.part == "/t2.xml" && failure.relationshipId == "duplicate-key","Duplicate key lost authored source/link") }
            }
        }
    }
    let forward=try bytes(directory.appendingPathComponent("forward-repeated.opc").path), shared=try P.Budget()
    for relationships in [false,true] {
        var limits=P.Limits();if relationships{limits.maxRelationships=0}else{limits.maxReferences=0}
        do {_ = try ContractArchive.decode(forward,budget:P.Budget(limits));fatalError("Physical relationship quota admitted")}
        catch let failure as P.Failure {require(failure.code == .resourceLimit&&failure.part == "/"&&failure.fieldPath == "relationships"&&failure.relationshipId == "r0","Physical relationship quota lost actual source/field/link")}
    }
    _ = try ContractArchive.decode(forward,budget:shared)
    let used=shared.configured.maxReferences-shared.remaining(.references);require(used==6,"Expected exactly three physical links, one key and two request selectors")
    for tight in [false,true] {
        var limits=P.Limits();limits.maxReferences=used-(tight ? 1:0)
        do {_ = try ContractArchive.decode(forward,budget:P.Budget(limits));require(!tight,"One-tight reference bound admitted")}
        catch let failure as P.Failure {require(tight&&failure.code == .resourceLimit&&failure.part == "/s2.xml"&&failure.fieldPath == "sources.target_id"&&failure.relationshipId == "r1","One-tight reference bound lost actual occurrence context")}
    }
    var value = ContractArchive.Input(); var target = ContractTarget(); target.id = 7; target.name = "seven & checked"
    var source = ContractSource(); source.targetId = 7; var fixed = ContractOtherTarget(); fixed.id = 9; fixed.name = "fixed"
    value.targets = [P.ModelPart(target,name:"/t.xml",contentType:"application/vnd.serializer.target+xml")]
    value.sources = [P.ModelPart(source,name:"/s.xml",contentType:"application/vnd.serializer.source+xml")]; value.fixed = fixed
    let normal = try ContractArchive.encode(value); try Data(normal).write(to:directory.appendingPathComponent("produced-default.opc"))
    value.profiles["targets"] = .stored; let runtime = try ContractArchive.encode(value); try Data(runtime).write(to:directory.appendingPathComponent("produced-runtime.opc"))
    value.profiles["fixed"] = .deflate; rejected(.invalidOptions) { _ = try ContractArchive.encode(value) }
    let ordinary = Array("<target xmlns=\"urn:serializer:application-contract\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:n=\"urn:serializer:native:contract\" xsi:type=\"n:target\" id=\"7\" name=\"native\"/>".utf8)
    let role=P.Role("targets",name:nil,contentType:"application/vnd.serializer.target+xml",source:"/",relationship:"urn:serializer:targets",id:nil,compression:.deflate,minimum:0,maximum:nil,expectedRoot:"{urn:serializer:application-contract}target",expectedType:"{urn:serializer:native:contract}target")
    let guarded=try P.Loader(bytes(directory.appendingPathComponent("wrong-xsi-type.opc").path),budget:P.Budget());defer{guarded.close()};var calls=0
    rejected(.codec){let _: [P.ModelPart<Int>]=try guarded.models(role){_,_ in calls += 1;return 1}};require(calls==0,"Wrong XML type invoked application decoder")
    // The additive package preflight must not consume unknown type attributes in ordinary native XML.
    do { _ = try ContractTarget.decodeXml(ordinary); fatalError("Ordinary XML unexpectedly consumed package xsi:type") } catch is SerializerError {}
    print("Swift common application fixtures, matching/invalid/equivalent-prefix xsi:type, authored reference locations, exact shared bounds and ordinary native XML boundaries pass")
}
