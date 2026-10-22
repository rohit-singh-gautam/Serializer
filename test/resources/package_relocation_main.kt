// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package consumer

import generated.RelocationArchive as Archive
import generated.SerializerPackage as P
import java.nio.file.Files
import java.nio.file.Path

private var checks = 0
private val previous = "previous destination".toByteArray()

/** Keep assertions active in the independently compiled external consumer. */
private fun check(value: Boolean, message: String) {
    if (!value) throw AssertionError(message)
    ++checks
}

/** Observe public retained payload/link values without touching private provenance. */
private fun snapshot(extensions: P.Extensions): String = buildString {
    for (part in extensions.parts) append(listOf(part.name,part.contentType,part.sourceIndex,part.compression,part.data.contentToString())).append('\n')
    for (link in extensions.relationships) append(listOf(link.source,link.id,link.type,link.target,link.mode)).append('\n')
}

/** Verify exact and one-tight cumulative work/allocation through fresh public budgets. */
private fun bounds(input: Archive.Input) {
    val observed = P.Budget(); Archive.encode(input,observed)
    for (resource in listOf(P.Resource.WORK_UNITS,P.Resource.DECODER_ALLOCATION_BYTES)) {
        val configured = observed.configured(); val maximum = if (resource == P.Resource.WORK_UNITS) configured.maxWorkUnits else configured.maxDecoderAllocationBytes
        val used = maximum-observed.remaining(resource); check(used > 0,"positive resource inventory")
        for (allowance in listOf(used,used-1)) {
            val limits = P.Limits()
            if (resource == P.Resource.WORK_UNITS) limits.maxWorkUnits = allowance else limits.maxDecoderAllocationBytes = allowance
            var rejected = false
            try { Archive.encode(input,P.Budget(limits)) } catch (error: P.PackageError) { check(error.code == P.Code.RESOURCE_LIMIT,"bounded failure classification"); rejected = true }
            check(rejected == (allowance < used),"exact and one-tight cumulative bound")
        }
    }
}

/** A caller cancellation keeps exact throwable identity and the previous Save destination. */
private fun cancellation(input: Archive.Input, path: Path) {
    Files.write(path,previous); val original = IllegalStateException("caller cancellation identity"); var armed = false
    val budget = P.Budget(P.Limits(checkCancelled = { if (armed) throw original })); armed = true
    try { Archive.save(path,input,budget); throw AssertionError("cancelled Save accepted") }
    catch (error: IllegalStateException) { check(error === original,"exact caller exception identity") }
    check(Files.readAllBytes(path).contentEquals(previous),"cancelled Save retains destination")
}

/** Exercise loaded provenance, fresh authored inputs, repeated composition and atomic failure. */
fun main(args: Array<String>) {
    val directory = Path.of(args[0]); val name = args[1]; val action = args[2]; val expected = args[3]
    val policy = P.ExtensionPolicy(P.Unknown.PRESERVE,P.Unknown.PRESERVE)
    val loaded = try { Archive.load(Files.readAllBytes(directory.resolve("$name.opc")),policy = policy) }
    catch (error: P.PackageError) {
        check(expected == "LOAD_ERROR","unexpected load rejection")
        println("{\"case\":\"$name\",\"decision\":\"$expected\",\"checks\":$checks,\"failure_stage\":\"load\"}"); return
    }
    check(expected != "LOAD_ERROR","invalid load published"); check(loaded.value.extensions === loaded.extensions,"owning result extension identity")
    check(loaded.value.first?.id == 11u && loaded.value.first?.label == "first","first typed payload")
    check(loaded.value.second?.id == 22u && loaded.value.second?.label == "second","second typed payload")
    check(loaded.value.children.size >= 1 && loaded.value.children[0].value.id == 33u && loaded.value.children[0].sourceIndex == 0,"dynamic child routing")
    val input = Archive.Input().also { it.first = loaded.value.first; it.second = if (action == "remove_second") null else loaded.value.second; it.children = loaded.value.children; it.extensions = loaded.extensions }
    if (name.startsWith("dynamic-")) {
        val original="/children/child.xml"; val names=if(action=="swap_children") mapOf(original to "/children/other.xml","/children/other.xml" to original) else if(action=="remove_unreferenced_child") emptyMap() else mapOf(original to if(action=="collision_child") "/assets/blob.bin" else "/renamed/child.xml")
        input.extensions=loaded.extensions.withRenames(names);var children=loaded.value.children.map { it.copy(name=names[it.name]?:it.name) };if(action=="remove_child"||action=="remove_unreferenced_child")children=children.drop(1);if(action=="swap_children")children=children.reversed();input.children=children
    }
    val before = snapshot(input.extensions); val childName = input.children[0].name
    val saved = directory.resolve("$name-saved.opc"); Files.write(saved,previous)
    if (expected == "REJECT") {
        var rejected = false; try { Archive.encode(input) } catch (error: P.PackageError) { rejected = true }; check(rejected,"invalid relocation encoded")
        rejected = false; try { Archive.save(saved,input) } catch (error: P.PackageError) { rejected = true }; check(rejected,"invalid relocation saved")
        check(Files.readAllBytes(saved).contentEquals(previous),"failed Save retains destination")
    } else {
        check(expected == "OK","unknown decision"); Files.write(directory.resolve("$name-encoded.opc"),Archive.encode(input)); Files.write(directory.resolve("$name-repeat.opc"),Archive.encode(input)); Archive.save(saved,input)
        if (name == "alternate") { bounds(input); cancellation(input,directory.resolve("$name-cancel.opc")) }
    }
    check(snapshot(input.extensions) == before,"extension input mutated"); check(input.children[0].name == childName,"dynamic child renamed"); check(input.first?.id == 11u && input.first?.label == "first","typed model mutated")
    println("{\"case\":\"$name\",\"decision\":\"$expected\",\"checks\":$checks}")
}
