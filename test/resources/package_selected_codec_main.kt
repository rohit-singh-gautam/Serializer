// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package consumer

import generated.IdentityPackageArchive as A
import generated.IdentityXmlRegistry as Model
import generated.SerializerPackage as P
import generated.decodeXml
import generated.encodeXml
import generated.XmlLimits
import java.nio.file.Files
import java.nio.file.Path

private var passed = 0

// Keep assertions active in optimized execution of this separately compiled consumer module.
private fun checkValue(condition: Boolean, message: String) {
    if (!condition) error(message)
    ++passed
}

private class Sentinel : RuntimeException("original caller error")

// Decode a complete typed occurrence set through only public library callbacks and policies.
private fun selectedLoad(bytes: ByteArray, budget: P.Budget,
    decode: (ByteArray, P.CodecContext) -> Model): List<P.ModelPart<Model>> {
    return P.Loader(bytes, budget).use { loader ->
        val values = loader.models(A.selectProfiles().single(), decode)
        loader.finish(values).value
    }
}

// Compose two native decoders through the exact same public facade-created XML policy.
private fun composed(bytes: ByteArray, context: P.CodecContext): Model {
    checkValue(context.part.isNotEmpty() && context.relationshipId.isNotEmpty(), "public native origin")
    val limits = A.xmlLimits(context)
    Model.decodeXml(bytes, limits)
    return Model.decodeXml(bytes, limits)
}

// Stage caller-selected encoding and validation before entering the public atomic byte publisher.
private fun selectedSave(path: Path, values: List<P.ModelPart<Model>>, budget: P.Budget,
    encode: (Model, P.CodecContext) -> ByteArray) {
    val composer = P.Composer(budget)
    composer.models(A.selectProfiles().single(), values, encode)
    val bytes = composer.finish()
    selectedLoad(bytes, budget) { payload, context -> Model.decodeXml(payload, A.xmlLimits(context)) }
    P.save(path, bytes, budget)
}

// The generated library and this consumer are separate Kotlin modules, so internal hooks are inaccessible.
fun main() {
    val directory = Path.of("fixtures")
    val bytes = Files.readAllBytes(directory.resolve("valid.opc"))
    val baseline = P.Budget()
    val ordinary = A.load(bytes, baseline).value
    checkValue(ordinary.registries.size == 3, "old generated facade remains usable")
    checkValue(baseline.configured().maxReferences - baseline.remaining(P.Resource.REFERENCES) == 15L, "default fifteen references")
    var emptyCalls = 0
    val empty = selectedLoad(Files.readAllBytes(directory.resolve("empty.opc")), P.Budget()) { payload, context ->
        ++emptyCalls
        Model.decodeXml(payload, A.xmlLimits(context))
    }
    checkValue(empty.size == 3 && emptyCalls == 3, "small valid empty XML passes typed dispatch")
    emptyCalls = 0
    try {
        selectedLoad(Files.readAllBytes(directory.resolve("wrong-root.opc")), P.Budget()) { payload, context ->
            ++emptyCalls
            Model.decodeXml(payload, A.xmlLimits(context))
        }
        error("wrong XML root accepted")
    } catch (failure: P.PackageError) { checkValue(emptyCalls == 0, "wrong root rejects before selected native callback") }
    var limitedCalls = 0
    try {
        selectedLoad(bytes, P.Budget()) { payload, context ->
            ++limitedCalls
            Model.decodeXml(payload, A.xmlLimits(context, XmlLimits(maxNodes = 0)))
        }
        error("configured local XML cap accepted")
    } catch (failure: P.PackageError) {
        checkValue(limitedCalls == 1 && failure.code == P.Code.RESOURCE_LIMIT && failure.part == "/Registries/registry0.xml" &&
            failure.relationshipId == "record0" && failure.fieldPath.isNotEmpty(), "public configured local XML cap retains shared context")
    }
    val measured = P.Budget()
    val values = selectedLoad(bytes, measured, ::composed)
    checkValue(values.size == 3 && measured.configured().maxReferences - measured.remaining(P.Resource.REFERENCES) == 27L, "selected double-decode shares twenty-seven references")
    val resources = listOf(P.Resource.REFERENCES, P.Resource.WORK_UNITS, P.Resource.DECODER_ALLOCATION_BYTES)
    for (resource in resources) {
        val original = measured.configured()
        val maximum = when (resource) {
            P.Resource.REFERENCES -> original.maxReferences
            P.Resource.WORK_UNITS -> original.maxWorkUnits
            else -> original.maxDecoderAllocationBytes
        }
        val used = maximum - measured.remaining(resource)
        for (tight in listOf(false, true)) {
            val limits = P.Limits()
            val bound = used - if (tight) 1 else 0
            when (resource) {
                P.Resource.REFERENCES -> limits.maxReferences = bound
                P.Resource.WORK_UNITS -> limits.maxWorkUnits = bound
                else -> limits.maxDecoderAllocationBytes = bound
            }
            try {
                selectedLoad(bytes, P.Budget(limits), ::composed)
                checkValue(!tight, "one-tight selected quota accepted")
            } catch (failure: P.PackageError) {
                checkValue(tight && failure.code == P.Code.RESOURCE_LIMIT, "selected quota classification")
                checkValue(failure.part.isNotEmpty() && failure.fieldPath.isNotEmpty() && failure.relationshipId.isNotEmpty(), "selected quota physical and native context")
            }
        }
    }
    val marker = Sentinel()
    var calls = 0
    try {
        selectedLoad(bytes, P.Budget()) { payload, context ->
            if (++calls == 2) {
                checkValue(context.part == "/Registries/registry1.xml" && context.relationshipId == "record1", "middle caller context")
                throw marker
            }
            Model.decodeXml(payload, A.xmlLimits(context))
        }
        error("caller failure accepted")
    } catch (failure: Sentinel) { checkValue(failure === marker && calls == 2, "original selected callback identity") }
    var swallowed: Throwable? = null
    try {
        selectedLoad(bytes, P.Budget(P.Limits(maxReferences = 3))) { payload, context ->
            try { Model.decodeXml(payload, A.xmlLimits(context)) }
            catch (failure: P.PackageError) { swallowed = failure; Model() }
        }
        error("swallowed native resource failure accepted")
    } catch (failure: P.PackageError) {
        checkValue(swallowed != null && failure === swallowed, "selected callback cannot suppress first native shared-budget failure")
    }
    var cancel = false
    calls = 0
    val cancelled = P.Budget(P.Limits(checkCancelled = { if (cancel) throw marker }))
    try {
        selectedLoad(bytes, cancelled) { payload, context ->
            if (++calls == 2) cancel = true
            val configured = if (cancel) XmlLimits(maxNodes = 0) else XmlLimits()
            Model.decodeXml(payload, A.xmlLimits(context, configured))
        }
        error("native cancellation accepted")
    } catch (failure: Sentinel) { checkValue(failure === marker && calls == 2, "original middle native cancellation identity") }
    val destination = directory.resolve("atomic.opc")
    Files.write(destination, "previous destination".toByteArray())
    calls = 0
    try {
        selectedSave(destination, ordinary.registries, P.Budget()) { model, context ->
            if (++calls == 2) throw marker
            model.encodeXml(A.xmlLimits(context))
        }
        error("middle encoder failure accepted")
    } catch (failure: Sentinel) {
        checkValue(failure === marker && Files.readAllBytes(destination).contentEquals("previous destination".toByteArray()), "selected middle Save failure is atomic")
    }
    selectedSave(directory.resolve("produced.opc"), ordinary.registries, P.Budget()) { model, context ->
        model.encodeXml(A.xmlLimits(context))
    }
    println("{\"passed\":$passed,\"default_references\":15,\"composed_references\":27}")
}
