// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
@file:OptIn(ExperimentalUnsignedTypes::class)
package consumer

import generated.PipelineArchive as A
import generated.SerializerPackage as P
import java.nio.file.Files
import java.nio.file.Path

// Exercise selected native pipelines and inert external resources through actual generated Kotlin APIs.
fun main(arguments: Array<String>) {
    val directory=Path.of(arguments[0]); val name=arguments[1]; val codec=arguments[2]; val decision=arguments[3]
    var checks=0
    fun check(value: Boolean, message: String) { if(!value) throw IllegalStateException(message); ++checks }
    val pipelines=mapOf("main" to P.TypedPipeline(codec))
    val decodeBudget=P.Budget(); val source=Files.readAllBytes(directory.resolve("$name.opc"))
    val loaded=try { A.loadProfiles(source,budget=decodeBudget,pipelines=pipelines) }
    catch(failure: P.PackageError) { check(decision == "LOAD_ERROR","Unexpected load rejection: $failure"); println("{\"decision\":\"$decision\",\"checks\":$checks}"); return }
    check(decision == "OK","Invalid input published")
    for(resource in listOf(P.Resource.WORK_UNITS,P.Resource.DECODER_ALLOCATION_BYTES)) {
        val defaults=P.Limits(); val maximum=if(resource == P.Resource.WORK_UNITS) defaults.maxWorkUnits else defaults.maxDecoderAllocationBytes
        val used=maximum-decodeBudget.remaining(resource)
        for(allowance in listOf(used,used-1)) { val limits=if(resource == P.Resource.WORK_UNITS) defaults.copy(maxWorkUnits=allowance) else defaults.copy(maxDecoderAllocationBytes=allowance); var rejected=false
            try { A.loadProfiles(source,budget=P.Budget(limits),pipelines=pipelines) } catch(failure: P.PackageError) { check(failure.code == P.Code.RESOURCE_LIMIT,"Decode quota classification"); rejected=true }; check(rejected == (allowance < used),"Decode exact/one-tight native bound") }
    }
    val input=loaded.value
    check(input.main!!.id == 17u && input.main!!.label == "pipeline" && input.main!!.values.contentEquals(uintArrayOf(2u,4u,8u)),"Native model changed")
    check(input.links.size == 1 && input.links[0].contentType.isEmpty() && input.links[0].data.isEmpty(),"External role is not inert")
    val measured=P.Budget(); Files.write(directory.resolve("$name-encoded.opc"),A.encode(input,measured)); A.save(directory.resolve("$name-saved.opc"),input)
    for(resource in listOf(P.Resource.WORK_UNITS,P.Resource.DECODER_ALLOCATION_BYTES)) {
        val defaults=P.Limits(); val maximum=if(resource == P.Resource.WORK_UNITS) defaults.maxWorkUnits else defaults.maxDecoderAllocationBytes
        val used=maximum-measured.remaining(resource); check(used > 0,"Missing native admission")
        for(allowance in listOf(used,used-1)) {
            val limits=if(resource == P.Resource.WORK_UNITS) defaults.copy(maxWorkUnits=allowance) else defaults.copy(maxDecoderAllocationBytes=allowance)
            var rejected=false
            try { A.encode(input,P.Budget(limits)) } catch(failure: P.PackageError) { check(failure.code == P.Code.RESOURCE_LIMIT,"Quota classification"); rejected=true }
            check(rejected == (allowance < used),"Exact/one-tight native bound")
        }
    }
    for(invalid in listOf(mapOf("fixed" to P.TypedPipeline("binary_none")),mapOf("missing" to P.TypedPipeline("xml")))) {
        var rejected=false; try { A.selectPipelines(pipelines=invalid) } catch(failure: P.PackageError) { rejected=true }; check(rejected,"Invalid profile accepted")
    }
    val path=directory.resolve("$name-atomic.opc"); val previous="previous destination".toByteArray(); Files.write(path,previous)
    val original=input.links; input.links=listOf(P.BinaryPart(byteArrayOf(1),original[0].name,"")); var rejected=false
    try { A.save(path,input) } catch(failure: P.PackageError) { rejected=true }; input.links=original
    check(rejected && Files.readAllBytes(path).contentEquals(previous),"Failed Save changed destination")
    println("{\"decision\":\"$decision\",\"checks\":$checks}")
}
