// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import generated.IdentityPackageArchive as A
import generated.SerializerPackage as P
import java.nio.file.Files
import java.nio.file.Path

private var passed=0
// Keep every qualification assertion active in optimized JVM execution.
private fun requireValue(value:Boolean,message:String){if(!value)error(message);++passed}
private class Marker:RuntimeException("Original large operation interruption")
// Bound each cumulative category explicitly; this policy is not a hard process heap limit.
private fun limits()=P.Limits(maxArchiveBytes=64L shl 20,maxPartBytes=32L shl 20,
    maxTotalExpandedBytes=128L shl 20,maxBufferBytes=128L shl 20,maxMetadataBytes=4L shl 20,
    maxDecoderAllocationBytes=2L shl 30,maxWorkUnits=4L shl 30,maxReferences=1000000,
    maxPreservedBytes=1L shl 20,maxEntries=32,maxRelationships=32)
// Inspect the public ledger without invoking additional policy callbacks.
private fun used(budget:P.Budget,resource:P.Resource):Long{
    val policy=budget.configured()
    val maximum=when(resource){P.Resource.REFERENCES->policy.maxReferences;P.Resource.WORK_UNITS->policy.maxWorkUnits
        P.Resource.PRESERVED_BYTES->policy.maxPreservedBytes;else->policy.maxDecoderAllocationBytes}
    return maximum-budget.remaining(resource)
}
// Recover the original policy exception object rather than matching a diagnostic string.
private fun original(marker:Marker,action:()->Unit){try{action()}catch(failure:Marker){requireValue(failure===marker,"Original exception replaced");return};error("Interrupted operation returned")}

// Exercise three owning native identity owners through one ZIP/DEFLATE/XML/preservation ledger.
fun main(args:Array<String>){
    val directory=Path.of(args[0]);val entries=args[1].toInt()
    val bytes=Files.readAllBytes(directory.resolve("valid.opc"));val invalid=Files.readAllBytes(directory.resolve("invalid.opc"))
    val preserve=P.ExtensionPolicy(P.Unknown.PRESERVE,P.Unknown.PRESERVE)
    var polls=0;val baselinePolicy=limits().also{it.checkCancelled={++polls}}
    val baseline=P.Budget(baselinePolicy);var aggregateCalls=0
    val value=A.load(bytes,baseline,preserve){candidate,shared->
        requireValue(shared===baseline&&candidate.registries.size==3,"Aggregate callback changed ledger/owners");++aggregateCalls
    }.value
    requireValue(aggregateCalls==1&&polls>1000,"Incomplete aggregate callback or missing operation polls")
    requireValue(value.registries.all{it.value.requests.size==entries&&it.value.entries.size==entries},"Owning counts changed")
    requireValue(value.extensions.parts.size==1&&value.extensions.relationships.size==3,"Preserved asset/edges missing")
    val references=used(baseline,P.Resource.REFERENCES)
    requireValue(references==9L*entries+6,"Native identity tuples/OPC references did not share one ledger")
    val statistics=listOf(P.Resource.REFERENCES,P.Resource.WORK_UNITS,P.Resource.DECODER_ALLOCATION_BYTES,P.Resource.PRESERVED_BYTES).associateWith{used(baseline,it)}
    for(resource in listOf(P.Resource.REFERENCES,P.Resource.WORK_UNITS,P.Resource.DECODER_ALLOCATION_BYTES)){
        for(tight in listOf(false,true)){
            val policy=limits();val maximum=statistics.getValue(resource)-(if(tight)1 else 0)
            when(resource){P.Resource.REFERENCES->policy.maxReferences=maximum;P.Resource.WORK_UNITS->policy.maxWorkUnits=maximum;else->policy.maxDecoderAllocationBytes=maximum}
            var published=false
            try{A.load(bytes,P.Budget(policy),preserve){_,_->published=true};requireValue(!tight&&published,"One-tight resource admitted or exact candidate missing")}
            catch(failure:P.PackageError){requireValue(tight&&failure.code==P.Code.RESOURCE_LIMIT&&!published,"Resource failure published aggregate or changed category")}
        }
    }
    aggregateCalls=0;var invalidCandidate:A.Value?=null
    try{invalidCandidate=A.load(invalid,P.Budget(limits()),preserve){_,_->++aggregateCalls}.value;error("Invalid final keyref accepted")}
    catch(failure:P.PackageError){requireValue(failure.code==P.Code.CODEC&&invalidCandidate==null&&aggregateCalls==0,"Invalid final owner reached publication")}
    for(threshold in listOf(1000,polls-1)){
        var count=0;val marker=Marker();val policy=limits().also{it.checkCancelled={if(++count>=threshold)throw marker}}
        var candidate:A.Value?=null;var callbacks=0
        original(marker){candidate=A.load(bytes,P.Budget(policy),preserve){_,_->++callbacks}.value}
        requireValue(candidate==null&&callbacks==0&&count==threshold,"Mid/late cancellation published aggregate")
    }
    val late=Marker();var cancel=false;var lateCallbacks=0
    val latePolicy=limits().also{it.checkCancelled={if(cancel)throw late}}
    original(late){A.load(bytes,P.Budget(latePolicy),preserve){_,_->++lateCallbacks;cancel=true}}
    requireValue(lateCallbacks==1,"Aggregate completion cancellation did not execute")
    var clockPolls=0;val expired=Marker();val deadline=System.nanoTime()-1
    original(expired){A.load(bytes,P.Budget(limits().also{it.checkCancelled={
        if(++clockPolls>=1000&&System.nanoTime()>=deadline)throw expired
    }}),preserve)}
    requireValue(clockPolls==1000,"Host-clock expiry did not reach the shared ledger")
    value.profileOverrides=mapOf("registries" to "deflate")
    val encoded=A.encode(value,P.Budget(limits()));Files.write(directory.resolve("encoded.opc"),encoded)
    val reopened=A.load(encoded,P.Budget(limits()),preserve).value
    reopened.profileOverrides=mapOf("registries" to "deflate")
    requireValue(reopened.registries.size==3&&reopened.extensions.parts.size==1,"Fresh load/encode recovery failed")
    val sentinel=directory.resolve("sentinel.opc");val oldBytes=byteArrayOf(11,22,33);Files.write(sentinel,oldBytes)
    val saveMarker=Marker();var saveHooks=0
    original(saveMarker){A.save(sentinel,value,P.Budget(limits()),validate={_,_->throw saveMarker},fault={++saveHooks})}
    requireValue(saveHooks==0&&Files.readAllBytes(sentinel).contentEquals(oldBytes),"Failed atomic Save changed sentinel or touched publication")
    var publicationCancelled=false;var beforeReplaceCalls=0;val publicationMarker=Marker()
    val publicationPolicy=limits().also{it.checkCancelled={if(publicationCancelled)throw publicationMarker}}
    original(publicationMarker){A.save(sentinel,value,P.Budget(publicationPolicy),fault={point->
        if(point=="before_replace"){++beforeReplaceCalls;publicationCancelled=true}
    })}
    requireValue(beforeReplaceCalls==1&&Files.readAllBytes(sentinel).contentEquals(oldBytes),"before_replace hook returned after cancellation but replacement published")
    Files.list(directory).use{paths->requireValue(paths.noneMatch{it.fileName.toString().startsWith(".serializer-save-")},"Failed Save leaked a private sibling")}
    val saved=directory.resolve("saved.opc");A.save(saved,reopened,P.Budget(limits()))
    requireValue(A.load(saved,P.Budget(limits()),preserve).value.registries.size==3,"Saved archive did not reopen")
    println("{\"passed\":$passed,\"owners\":3,\"entries_per_owner\":$entries,\"polls\":$polls,\"references\":$references,\"work_units\":${statistics.getValue(P.Resource.WORK_UNITS)},\"allocation_bytes\":${statistics.getValue(P.Resource.DECODER_ALLOCATION_BYTES)},\"preserved_bytes\":${statistics.getValue(P.Resource.PRESERVED_BYTES)},\"aggregate_callbacks\":1}")
}
