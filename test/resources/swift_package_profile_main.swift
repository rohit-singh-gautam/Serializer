// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

// Qualify generated type dispatch, aggregate publication, preservation and declarative references.
func profileChecks(_ directory: URL) throws {
    let independent = try bytes(directory.appendingPathComponent("independent.opc").path)
    var loaded = try ProfileArchive.decode(independent)
    require(loaded.main?.name == "independent" && loaded.main?.points.first?.z == 3,"Independent typed XML model differs")
    require(loaded.attachments.count == 1 && loaded.attachments[0].data == [0,1,254,255],"Independent opaque model role differs")
    loaded.profiles["main"] = .stored
    let encoded = try ProfileArchive.encode(loaded)
    try Data(encoded).write(to:directory.appendingPathComponent("swift.opc"))
    let reloaded = try ProfileArchive.decode(encoded); require(reloaded.main?.name == "independent","Typed aggregate round trip differs")
    require(try ProfileArchive.encode(reloaded) == ProfileArchive.encode(reloaded),"Repeated package composition differs")
    var invalid = loaded; invalid.profiles["missing"] = .stored; rejected(.invalidOptions) { _ = try ProfileArchive.encode(invalid) }
    let vendor = try bytes(directory.appendingPathComponent("vendor.opc").path)
    rejected(.unsupportedFeature) { _ = try ProfileArchive.decode(vendor) }
    var policy = P.ExtensionPolicy(); policy.parts = .skip; policy.relationships = .skip
    let skipped = try ProfileArchive.decode(vendor,policy:policy); require(skipped.extensions.parts.isEmpty,"Skipped vendor bytes retained")
    policy.parts = .preserve; policy.relationships = .preserve
    let preserved = try ProfileArchive.decode(vendor,policy:policy)
    require(preserved.extensions.parts.count == 1 && preserved.extensions.relationships.count == 2,"Whole-part or relationship context missing")
    let output = try ProfileArchive.encode(preserved); try Data(output).write(to:directory.appendingPathComponent("swift-preserved.opc"))
    policy.requiredPart = { _ in true }; rejected(.unsupportedFeature) { _ = try ProfileArchive.decode(vendor,policy:policy) }
    policy.requiredPart = nil; policy.requiredRelationship = { _ in true }; rejected(.unsupportedFeature) { _ = try ProfileArchive.decode(vendor,policy:policy) }
    var collision = preserved; collision.extensions = P.Extensions(parts:preserved.extensions.parts+[P.BinaryPart([],name:"/3D/model.xml",contentType:"application/octet-stream")],relationships:preserved.extensions.relationships)
    rejected(.duplicateName) { _ = try ProfileArchive.encode(collision) }
    let cases = try String(contentsOf:directory.appendingPathComponent("opc-cases.tsv"),encoding:.utf8)
    var metadataFailures: [String] = []
    for row in cases.split(whereSeparator: \.isNewline) {
        let fields = row.split(separator:"\t").map(String.init); checkLabel = fields[0]
        let input = try bytes(directory.appendingPathComponent(fields[0]).path)
        do { if fields.count > 1 && fields[1] == "valid" {
            let decoded = try ProfileArchive.decode(input)
            _ = try ProfileArchive.decode(ProfileArchive.encode(decoded))
        }
        else if fields.count > 1 && fields[1] == "preserve" {
            var policy = P.ExtensionPolicy(); policy.parts = .preserve; policy.relationships = .preserve
            let decoded = try ProfileArchive.decode(input,policy:policy)
            let roundtrip = try ProfileArchive.decode(ProfileArchive.encode(decoded),policy:policy)
            require(decoded.extensions.relationships.map(\.target) == roundtrip.extensions.relationships.map(\.target),"External relationship spelling changed")
        }
        else {
            var policy = P.ExtensionPolicy()
            if fields.count > 1 && fields[1] == "preserve-invalid" { policy.parts = .preserve; policy.relationships = .preserve }
            do { _ = try ProfileArchive.decode(input,policy:policy); metadataFailures.append(fields[0]+": unexpected acceptance") } catch is P.Failure {}
        }
        } catch { metadataFailures.append(fields[0]+": "+String(describing:error)) }
    }
    if !metadataFailures.isEmpty { throw P.Failure(.semantic,metadataFailures.joined(separator:"; ")) }
    checkLabel = "middle-typed-part"
    rejected(.codec) { _ = try ProfileCollectionArchive.decode(bytes(directory.appendingPathComponent("middle.opc").path)) }
    let role = P.Role("main",name:"/3D/model.xml",contentType:"application/vnd.serializer.test-model+xml",source:"/",relationship:"urn:serializer:main-model",id:nil,compression:.stored,minimum:1,maximum:1)
    let wrong = try bytes(directory.appendingPathComponent("wrong-media.opc").path)
    let loader = try P.Loader(wrong,budget:P.Budget()); defer { loader.close() }; var calls = 0
    rejected(.semantic) { let _: Int? = try loader.single(role) { _,_ in calls += 1; return 1 } }; require(calls == 0,"Wrong media invoked typed decoder")
    let fixed = P.Role("main",name:"/model.xml",contentType:"application/xml",source:"/",relationship:"urn:fixed",id:nil,compression:.stored,minimum:1,maximum:1,fixed:true)
    let composer = P.Composer(budget:try P.Budget(),profiles:["main":.deflate])
    rejected(.invalidOptions) { try composer.single(fixed,1) { _,_ in Array("<model/>".utf8) } }
    var limits = P.Limits(); limits.maxReferences = 0
    let bounded = P.Composer(budget:try P.Budget(limits))
    do { try bounded.single(role,1) { _,context in try context.consume(.references,1,field:"key"); return [] }; fatalError("Shared codec reference bound admitted") }
    catch let failure as P.Failure { require(failure.code == .resourceLimit && failure.part == "/3D/model.xml" && failure.fieldPath == "main.key" && !failure.relationshipId.isEmpty,"Codec exhaustion lost part/field/link context") }
    for local in 0..<4 {
        let localComposer=P.Composer(budget:try P.Budget())
        do {
            try localComposer.single(role,loaded.main) {model,context in
                var native=P.xmlLimits(context)
                switch local {case 0:native.maxNodes=0;case 1:native.maxDepth=0;case 2:native.maxTextBytes=0;default:native.maxBytes=0}
                return try model.encodeXml(native)
            }
            fatalError("Local encode quota admitted")
        } catch let failure as P.Failure {require(failure.code == .resourceLimit&&failure.part == "/3D/model.xml"&&failure.fieldPath == "main"&&failure.relationshipId == "srl_main_0","Local encode quota lost package resource context")}
        let localLoader=try P.Loader(independent,budget:P.Budget());defer{localLoader.close()}
        do {
            let _:ProfileModel?=try localLoader.single(role) {data,context in
                var native=P.xmlLimits(context)
                switch local {case 0:native.maxNodes=0;case 1:native.maxDepth=0;case 2:native.maxTextBytes=0;default:native.maxBytes=0}
                return try ProfileModel.decodeXml(data,native)
            }
            fatalError("Local decode quota admitted")
        } catch let failure as P.Failure {require(failure.code == .resourceLimit&&failure.part == "/Alternate/root.xml"&&failure.fieldPath == "main"&&failure.relationshipId == "main","Local decode quota lost package resource context")}
    }
    var digest=LinkedDigestModel();digest.value=[1,2]
    let digestComposer=P.Composer(budget:try P.Budget())
    do {try digestComposer.single(role,digest){model,context in try model.encodeXml(P.xmlLimits(context),digestLimits:XmlDigestLimits(maxBytes:0))};fatalError("Local digest quota admitted")}
    catch let failure as P.Failure {require(failure.code == .resourceLimit&&failure.part == "/3D/model.xml"&&failure.fieldPath == "main"&&failure.relationshipId == "srl_main_0","Local digest encode quota lost package context")}
    let digestLoader=try P.Loader(bytes(directory.appendingPathComponent("local-digest.opc").path),budget:P.Budget());defer{digestLoader.close()}
    do {let _:LinkedDigestModel?=try digestLoader.single(role){data,context in try LinkedDigestModel.decodeXml(data,P.xmlLimits(context),digestLimits:XmlDigestLimits(maxBytes:0))};fatalError("Local digest decode quota admitted")}
    catch let failure as P.Failure {require(failure.code == .resourceLimit&&failure.part == "/Alternate/root.xml"&&failure.fieldPath == "main"&&failure.relationshipId == "main","Local digest decode quota lost package context")}
    for encode in [false,true] {
        let options=XmlExtensionOptions(understoodNamespaces:["urn:vendor"],limits:XmlExtensionLimits(maxItems:0))
        if encode {
            let composer=P.Composer(budget:try P.Budget())
            do {try composer.single(role,loaded.main){model,context in try model.encodeXml(P.xmlLimits(context),extensionOptions:options)};fatalError("Local extension quota admitted")}
            catch let failure as P.Failure {require(failure.code == .resourceLimit&&failure.part == "/3D/model.xml"&&failure.fieldPath == "main"&&failure.relationshipId == "srl_main_0","Local extension encode quota lost package context")}
        }else{
            let loader=try P.Loader(independent,budget:P.Budget());defer{loader.close()}
            do {let _:ProfileModel?=try loader.single(role){data,context in try ProfileModel.decodeXml(data,P.xmlLimits(context),extensionOptions:options)};fatalError("Local extension quota admitted")}
            catch let failure as P.Failure {require(failure.code == .resourceLimit&&failure.part == "/Alternate/root.xml"&&failure.fieldPath == "main"&&failure.relationshipId == "main","Local extension decode quota lost package context")}
        }
    }
    var ordinaryLimits=XmlLimits();ordinaryLimits.maxDepth=0
    do {_ = try ProfileModel.decodeXml(loaded.main!.encodeXml(),ordinaryLimits);fatalError("Ordinary local quota admitted")}catch is SerializerError{}
    var linked = LinkedArchive.Input(); var first = LinkedTarget(); first.id = "one"; var second = LinkedTarget(); second.id = "two"; var reference = LinkedHolder(); reference.ref = "two"
    linked.targets = [P.ModelPart(first,name:"/target1.xml",contentType:"application/vnd.target+xml"),P.ModelPart(second,name:"/target2.xml",contentType:"application/vnd.target+xml")]
    linked.holders = [P.ModelPart(reference,name:"/holder.xml",contentType:"application/vnd.holder+xml")]
    let linkedBytes = try LinkedArchive.encode(linked); _ = try LinkedArchive.decode(linkedBytes); try Data(linkedBytes).write(to:directory.appendingPathComponent("swift-linked.opc"))
    var sameType = linked; sameType.targets = [linked.targets[0]]; sameType.alternates = [P.ModelPart(second,name:"/alternate.xml",contentType:"application/vnd.target+xml")]
    _ = try LinkedArchive.decode(LinkedArchive.encode(sameType))
    var duplicate = linked; duplicate.targets[1] = P.ModelPart(first,name:"/target2.xml",contentType:"application/vnd.target+xml")
    rejected(.semantic) { _ = try LinkedArchive.encode(duplicate) }
    reference.ref = "missing"; var missing = linked; missing.holders = [P.ModelPart(reference,name:"/holder.xml",contentType:"application/vnd.holder+xml")]
    rejected(.semantic) { _ = try LinkedArchive.encode(missing) }
    limits = P.Limits(); limits.maxReferences = 2; rejected(.resourceLimit) { _ = try LinkedArchive.encode(linked,budget:P.Budget(limits)) }
    for allocation in [false,true] {
        limits = P.Limits(); if allocation { limits.maxDecoderAllocationBytes = 0 } else { limits.maxReferences = 0 }
        do { _ = try LinkedArchive.encode(linked,budget:P.Budget(limits)); fatalError("Shared reference admission succeeded") }
        catch let failure as P.Failure { require(failure.code == .resourceLimit && failure.part == "/target1.xml" && failure.fieldPath == "targets.id" && failure.reference == "objects" && failure.relationshipId == "srl_targets_0","Reference admission lost part/field/scope/link context") }
    }
    rejected(.semantic) { _ = try LinkedArchive.decode(bytes(directory.appendingPathComponent("linked-missing.opc").path)) }
    let single=try LinkedSingleArchive.decode(bytes(directory.appendingPathComponent("linked-single-valid.opc").path));require(single.target?.id=="one"&&single.request?.ref=="one","Discovered singleton values differ")
    do {_ = try LinkedSingleArchive.decode(bytes(directory.appendingPathComponent("linked-missing.opc").path));fatalError("Missing singleton target admitted")}
    catch let failure as P.Failure {require(failure.code == .semantic&&failure.part == "/holder.xml"&&failure.fieldPath == "request.ref"&&failure.relationshipId == "holder","Singleton reference lost discovered path/link")}
    // Canonically equivalent Unicode scopes retain distinct exact UTF8 identities.
    let composedScope = "\u{e9}", decomposedScope = "e\u{301}"
    let distinct = P.References(try P.Budget())
    try distinct.add("first-model",scope:composedScope,value:"same",part:"/first.xml",field:"id")
    try distinct.add("second-model",scope:decomposedScope,value:"same",part:"/second.xml",field:"id")
    try distinct.require("first-model",scope:composedScope,value:"same",part:"/ref.xml",field:"ref")
    try distinct.require("second-model",scope:decomposedScope,value:"same",part:"/ref.xml",field:"ref")
    try distinct.finish()
    let isolated = P.References(try P.Budget())
    try isolated.add("first-model",scope:composedScope,value:"same",part:"/first.xml",field:"id")
    try isolated.require("first-model",scope:decomposedScope,value:"same",part:"/ref.xml",field:"ref")
    rejected(.semantic) { try isolated.finish() }
    let callerFailure=P.Failure(.semantic,"caller cancellation");var cancelReferences=false
    limits=P.Limits();limits.checkCancelled={if cancelReferences{throw callerFailure}}
    let cancelledReferences=P.References(try P.Budget(limits));cancelReferences=true
    do {try cancelledReferences.add("model",scope:"scope",value:"key",part:"/caller.xml",field:"id");fatalError("Caller cancellation ignored")}
    catch let failure as P.Failure {require(failure === callerFailure,"Reference annotation replaced original caller failure")}
    for tight in [false,true] {
        limits=P.Limits();limits.maxWorkUnits=tight ? 1:2;limits.maxReferences=3
        let resolutionBudget=try P.Budget(limits), resolution=P.References(resolutionBudget)
        try resolution.add("model",scope:"scope",value:"key",part:"/key.xml",field:"id",relationshipId:"key-link")
        for index in 1...2 {try resolution.require("model",scope:"scope",value:"key",part:"/request-\(index).xml",field:"request.ref",relationshipId:"request-\(index)")}
        require(resolutionBudget.remaining(.references)==0,"Key/request selectors did not each consume one reference")
        do {try resolution.finish();require(!tight&&resolutionBudget.remaining(.workUnits)==0,"Exact final-resolution work bound differs")}
        catch let failure as P.Failure {require(tight&&failure.code == .resourceLimit&&failure.part == "/request-2.xml"&&failure.fieldPath == "request.ref"&&failure.reference == "scope"&&failure.relationshipId == "request-2","Final-resolution work failure lost its occurrence")}
    }
    for type in [false,true] {
        let unicodeRole=P.Role("main",name:"/3D/model.xml",contentType:"application/vnd.serializer.test-model+xml",source:"/",relationship:"urn:serializer:main-model",id:nil,compression:.stored,minimum:1,maximum:1,expectedRoot:type ? "{urn:serializer:package-test}model":"{urn:\u{e9}}model",expectedType:type ? "{urn:\u{e9}}target":nil)
        for exact in [false,true] {
            let path="unicode-"+(type ? "type":"root")+"-"+(exact ? "exact":"distinct")+".opc"
            let unicodeLoader=try P.Loader(bytes(directory.appendingPathComponent(path).path),budget:P.Budget());defer{unicodeLoader.close()};var entered=0
            if exact {let value: Int?=try unicodeLoader.single(unicodeRole){_,_ in entered += 1;return 1};require(value==1&&entered==1,"Exact Unicode identity rejected")}
            else {rejected(.codec){let _: Int?=try unicodeLoader.single(unicodeRole){_,_ in entered += 1;return 1}};require(entered==0,"Canonically equivalent XML identity entered decoder")}
        }
    }
    let savePath = directory.appendingPathComponent("profile-save.opc"); try ProfileArchive.save(reloaded,to:savePath); _ = try ProfileArchive.decode(bytes(savePath.path))
    print("Swift OPC names/types/targets, transactional profiles, shared XML/reference budgets, required capabilities, preserved bytes/context, fixed/default/runtime profiles and declarative references pass")
}
