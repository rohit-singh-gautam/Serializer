// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

typealias A = PipelineArchive
typealias P = SerializerPackage

// Run one independent package fixture under a selected native envelope and one cumulative operation.
func run() throws {
    let directory=URL(fileURLWithPath:CommandLine.arguments[1],isDirectory:true),name=CommandLine.arguments[2],codec=CommandLine.arguments[3],decision=CommandLine.arguments[4]
    var checks=0
    func check(_ value: Bool,_ message: String) throws { guard value else { throw NSError(domain:message,code:1) }; checks += 1 }
    let pipelines=["main":P.TypedPipeline(codec)]
    let decodeBudget=try P.Budget(),source=Array(try Data(contentsOf:directory.appendingPathComponent(name+".opc")))
    var value:A.Input
    do { value=try A.decodeProfiles(source,budget:decodeBudget,pipelines:pipelines) }
    catch let failure as P.Failure { try check(decision == "LOAD_ERROR","Unexpected load rejection: \(failure)"); print("{\"decision\":\"\(decision)\",\"checks\":\(checks)}"); return }
    try check(decision == "OK","Invalid input published")
    for resource in [P.Resource.workUnits,.decoderAllocationBytes] { let defaults=P.Limits(),maximum=resource == .workUnits ? defaults.maxWorkUnits : defaults.maxDecoderAllocationBytes,used=maximum-decodeBudget.remaining(resource)
        for allowance in [used,used-1] { var limits=defaults; if resource == .workUnits { limits.maxWorkUnits=allowance } else { limits.maxDecoderAllocationBytes=allowance }; var rejected=false
            do { _ = try A.decodeProfiles(source,budget:P.Budget(limits),pipelines:pipelines) } catch let failure as P.Failure { try check(failure.code == .resourceLimit,"Decode quota classification"); rejected=true }; try check(rejected == (allowance < used),"Decode exact/one-tight native bound") }
    }
    try check(value.main!.id == 17 && value.main!.label == "pipeline" && value.main!.values == [2,4,8],"Native model changed")
    try check(value.links.count == 1 && value.links[0].contentType.isEmpty && value.links[0].data.isEmpty,"External role is not inert")
    let measured=try P.Budget(); try Data(A.encode(value,budget:measured)).write(to:directory.appendingPathComponent(name+"-encoded.opc")); try A.save(value,to:directory.appendingPathComponent(name+"-saved.opc"))
    for resource in [P.Resource.workUnits,.decoderAllocationBytes] {
        let defaults=P.Limits(),maximum=resource == .workUnits ? defaults.maxWorkUnits : defaults.maxDecoderAllocationBytes,used=maximum-measured.remaining(resource)
        try check(used > 0,"Missing native admission")
        for allowance in [used,used-1] {
            var limits=defaults; if resource == .workUnits { limits.maxWorkUnits=allowance } else { limits.maxDecoderAllocationBytes=allowance }
            var rejected=false
            do { _ = try A.encode(value,budget:P.Budget(limits)) } catch let failure as P.Failure { try check(failure.code == .resourceLimit,"Quota classification"); rejected=true }
            try check(rejected == (allowance < used),"Exact and one-tight bound")
        }
    }
    for invalid in [["fixed":P.TypedPipeline("binary_none")],["missing":P.TypedPipeline("xml")]] {
        var rejected=false; do { _ = try A.selectProfiles(pipelines:invalid) } catch is P.Failure { rejected=true }; try check(rejected,"Invalid profile accepted")
    }
    let destination=directory.appendingPathComponent(name+"-atomic.opc"),previous=Data("previous destination".utf8); try previous.write(to:destination)
    value.links=[P.BinaryPart([1],name:value.links[0].name,contentType:"")]; var rejected=false
    do { try A.save(value,to:destination) } catch is P.Failure { rejected=true }
    try check(rejected && Data(contentsOf:destination) == previous,"Failed Save changed destination")
    print("{\"decision\":\"\(decision)\",\"checks\":\(checks)}")
}
do { try run() } catch { fatalError("\(error)") }
