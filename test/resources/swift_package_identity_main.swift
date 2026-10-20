// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation
typealias P=SerializerPackage
typealias A=IdentityPackageArchive
// Verify exact shared admission and reject a missing native XSD keyref before publishing any aggregate.
func check(_ condition: Bool, _ message: String) {if !condition {fatalError(message)}}
do {
    let directory=URL(fileURLWithPath:CommandLine.arguments[1],isDirectory:true)
    let data=Array(try Data(contentsOf:directory.appendingPathComponent("valid.opc"))),budget=try P.Budget()
    let value=try A.decode(data,budget:budget);check(value.registries.count==3,"Identity aggregate incomplete")
    let references=budget.configured.maxReferences-budget.remaining(.references)
    check(references==15,"Expected three OPC links and twelve complete key/unique/keyref tuples")
    for resource in [P.Resource.references,.workUnits,.decoderAllocationBytes] {
        let used=resource == .references ? references : resource == .workUnits ? budget.configured.maxWorkUnits-budget.remaining(resource) : budget.configured.maxDecoderAllocationBytes-budget.remaining(resource)
        for tight in [false,true] {
            var limits=P.Limits();let maximum=used-(tight ? 1:0)
            switch resource {case .references:limits.maxReferences=maximum;case .workUnits:limits.maxWorkUnits=maximum;default:limits.maxDecoderAllocationBytes=maximum}
            do {_ = try A.decode(data,budget:P.Budget(limits));check(!tight,"One-tight native identity admission succeeded")}
            catch let failure as P.Failure {check(tight && failure.code == .resourceLimit && !failure.part.isEmpty && !failure.fieldPath.isEmpty && !failure.relationshipId.isEmpty,"Native identity admission lost package resource locations")}
        }
    }
    let role=P.Role("registries",name:nil,contentType:"application/vnd.serializer.identity+xml",source:"/",relationship:"urn:serializer:identity-registry",id:nil,compression:.stored,minimum:3,maximum:3)
    let loader=try P.Loader(data,budget:P.Budget());defer{loader.close()}
    do {
        _ = try loader.models(role) {bytes,context in try @registry_type@.decodeXml(bytes,P.xmlLimits(context),identityLimits:XmlIdentityLimits(maxTuples:0))}
        fatalError("Local identity tuple quota admitted")
    }catch let failure as P.Failure {check(failure.code == .resourceLimit && !failure.part.isEmpty && failure.fieldPath=="registries" && !failure.relationshipId.isEmpty,"Local identity quota lost package resource context")}
    var published:A.Input?
    do {published=try A.decode(Array(Data(contentsOf:directory.appendingPathComponent("invalid.opc"))));fatalError("Missing native keyref admitted")}
    catch let failure as P.Failure {check(failure.code == .codec&&published==nil,"Failed native identity aggregate published")}
    try Data(A.encode(value)).write(to:directory.appendingPathComponent("produced.opc"))
    print("Swift native identity: exact15 shared references, exact/one-tight work/allocation, local tuple quota and atomic keyrefs pass")
}catch let failure as P.Failure {
    FileHandle.standardError.write(Data(("Swift identity package failure: \(failure.code) \(failure.message) part=\(failure.part) field=\(failure.fieldPath) link=\(failure.relationshipId)\n").utf8));exit(1)
}catch {FileHandle.standardError.write(Data(("Swift identity SDK failure: \(error)\n").utf8));exit(1)}
