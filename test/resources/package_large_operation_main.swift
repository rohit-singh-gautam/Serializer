// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation
typealias P=SerializerPackage
typealias A=IdentityPackageArchive
private var passed=0
// Keep every contract check active in optimized native execution.
func check(_ value:Bool,_ message:String){if !value{fatalError(message)};passed+=1}
final class Marker:Swift.Error{}
// Explicit finite cumulative bounds admit the large selected owning profile.
func limits()->P.Limits{
    var result=P.Limits();result.maxArchiveBytes=64<<20;result.maxPartBytes=32<<20
    result.maxTotalExpandedBytes=128<<20;result.maxBufferBytes=128<<20;result.maxMetadataBytes=4<<20
    result.maxDecoderAllocationBytes=2<<30;result.maxWorkUnits=4<<30;result.maxReferences=1000000
    result.maxPreservedBytes=1<<20;result.maxEntries=32;result.maxRelationships=32;return result
}
// Read public cumulative accounting without adding policy calls.
func used(_ budget:P.Budget,_ resource:P.Resource)->Int{
    let maximum:Int
    switch resource{case .references:maximum=budget.configured.maxReferences;case .workUnits:maximum=budget.configured.maxWorkUnits
    case .preservedBytes:maximum=budget.configured.maxPreservedBytes;default:maximum=budget.configured.maxDecoderAllocationBytes}
    return maximum-budget.remaining(resource)
}
// Retain the original caller-owned class error across all nested public adapters.
func original(_ marker:Marker,_ action:()throws->Void)throws{
    do{try action()}catch{guard let value=error as? Marker,value===marker else{throw error};check(true,"original");return}
    fatalError("Interrupted operation returned a candidate")
}
// Keep aggregate completion local until public facade decode and the shared final check succeed.
func load(_ bytes:[UInt8],_ budget:P.Budget,_ policy:P.ExtensionPolicy,_ completed:()throws->Void={})throws->A.Input{
    let candidate=try A.decode(bytes,budget:budget,policy:policy);try completed();try budget.check();return candidate
}

let directory=URL(fileURLWithPath:CommandLine.arguments[1],isDirectory:true)
let entries=Int(CommandLine.arguments[2])!
let bytes=Array(try Data(contentsOf:directory.appendingPathComponent("valid.opc")))
let invalid=Array(try Data(contentsOf:directory.appendingPathComponent("invalid.opc")))
var preserve=P.ExtensionPolicy();preserve.parts = .preserve;preserve.relationships = .preserve
var polls=0;var baselinePolicy=limits();baselinePolicy.checkCancelled={polls+=1}
let baseline=try P.Budget(baselinePolicy);var aggregateCalls=0
var value=try load(bytes,baseline,preserve){aggregateCalls+=1}
check(aggregateCalls==1&&polls>1000&&value.registries.count==3,"Incomplete aggregate or missing polls")
check(value.registries.allSatisfy{$0.value.requests.count==entries&&$0.value.entries.count==entries},"Owning counts changed")
check(value.extensions.parts.count==1&&value.extensions.relationships.count==3,"Preserved asset/edges missing")
let references=used(baseline,.references)
check(references==9*entries+6,"Native identity tuples and OPC links did not share one ledger")
let statistics=[used(baseline,.references),used(baseline,.workUnits),used(baseline,.decoderAllocationBytes),used(baseline,.preservedBytes)]
for (index,resource) in [P.Resource.references,.workUnits,.decoderAllocationBytes].enumerated(){
    for tight in [false,true]{
        var policy=limits();let maximum=statistics[index]-(tight ? 1:0)
        switch resource{case .references:policy.maxReferences=maximum;case .workUnits:policy.maxWorkUnits=maximum;default:policy.maxDecoderAllocationBytes=maximum}
        var published:A.Input?
        do{published=try load(bytes,P.Budget(policy),preserve);check(!tight&&published != nil,"One-tight resource admitted")}
        catch let failure as P.Failure{check(tight&&failure.code == .resourceLimit&&published==nil,"Resource failure published aggregate or changed category")}
    }
}
var invalidCandidate:A.Input?;aggregateCalls=0
do{invalidCandidate=try load(invalid,P.Budget(limits()),preserve){aggregateCalls+=1};fatalError("Invalid final keyref accepted")}
catch let failure as P.Failure{check(failure.code == .codec&&invalidCandidate==nil&&aggregateCalls==0,"Invalid final owner reached publication")}
for threshold in [1000,polls-1]{
    var count=0;let marker=Marker();var policy=limits();policy.checkCancelled={count+=1;if count>=threshold{throw marker}}
    var candidate:A.Input?;var callbacks=0
    try original(marker){candidate=try load(bytes,P.Budget(policy),preserve){callbacks+=1}}
    check(candidate==nil&&callbacks==0&&count==threshold,"Mid/late cancellation published aggregate")
}
let late=Marker();var cancel=false;var lateCallbacks=0;var latePolicy=limits();latePolicy.checkCancelled={if cancel{throw late}}
var latePreserve=preserve
latePreserve.requiredRelationship={link in
    if link.type=="urn:serializer:large-external"{lateCallbacks+=1;cancel=true}
    return false
}
var lateCandidate:A.Input?
try original(late){lateCandidate=try A.decode(bytes,budget:P.Budget(latePolicy),policy:latePreserve)}
check(lateCallbacks==1&&lateCandidate==nil,"Preservation callback cancellation returned an aggregate")
var clockPolls=0;let expired=Marker();let deadline=DispatchTime.now().uptimeNanoseconds-1
var clockPolicy=limits();clockPolicy.checkCancelled={clockPolls+=1;if clockPolls>=1000&&DispatchTime.now().uptimeNanoseconds>=deadline{throw expired}}
try original(expired){_ = try load(bytes,P.Budget(clockPolicy),preserve)}
check(clockPolls==1000,"Host-clock expiry did not reach shared ledger")
value.profiles=["registries":.deflate]
let encoded=try A.encode(value,budget:P.Budget(limits()));try Data(encoded).write(to:directory.appendingPathComponent("encoded.opc"))
var reopened=try load(encoded,P.Budget(limits()),preserve)
reopened.profiles=["registries":.deflate]
check(reopened.registries.count==3&&reopened.extensions.parts.count==1,"Fresh load/encode recovery failed")
let sentinel=directory.appendingPathComponent("sentinel.opc");let old=Data([11,22,33]);try old.write(to:sentinel)
let saveMarker=Marker();var saveBudget:P.Budget?
var savePolicy=limits();savePolicy.checkCancelled={if let selected=saveBudget,used(selected,.references)>=9*entries{throw saveMarker}}
saveBudget=try P.Budget(savePolicy)
try original(saveMarker){try A.save(value,to:sentinel,budget:saveBudget!)}
check(try Data(contentsOf:sentinel)==old,"Failed atomic Save replaced sentinel")
let saved=directory.appendingPathComponent("saved.opc");try A.save(reopened,to:saved,budget:P.Budget(limits()))
check(try load(Array(Data(contentsOf:saved)),P.Budget(limits()),preserve).registries.count==3,"Saved archive did not reopen")
print("{\"passed\":\(passed),\"owners\":3,\"entries_per_owner\":\(entries),\"polls\":\(polls),\"references\":\(references),\"work_units\":\(statistics[1]),\"allocation_bytes\":\(statistics[2]),\"preserved_bytes\":\(statistics[3]),\"host_aggregate_callbacks\":1,\"native_preservation_callbacks\":\(lateCallbacks)}")
