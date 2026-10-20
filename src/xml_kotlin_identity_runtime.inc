// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"identitykotlin(
// Bound one operation-local identity index, including incomplete selections and deferred forward references.
data class XmlIdentityLimits(val maxConstraints: Int = srlXmlIdentityMaxConstraints,
    val maxFields: Int = srlXmlIdentityMaxFields, val maxDepth: Int = srlXmlIdentityMaxDepth,
    val maxSelectedNodes: Int = srlXmlIdentityMaxSelectedNodes, val maxTuples: Int = srlXmlIdentityMaxTuples,
    val maxTupleBytes: Int = srlXmlIdentityMaxTupleBytes, val maxWorkUnits: Int = srlXmlIdentityMaxWorkUnits) {
    init { require(listOf(maxConstraints,maxFields,maxDepth,maxSelectedNodes,maxTuples,maxTupleBytes,maxWorkUnits).all { it >= 0 }) { "Negative XML identity limit" } }
}
// Metadata is compiler-owned and operation-independent; expanded names retain exact Unicode identities.
internal class SrlXmlIdentityPath(val steps: Array<SrlXmlName>, val descendants: Boolean, val attribute: SrlXmlName?)
internal class SrlXmlIdentityField(val path: SrlXmlIdentityPath, val kind: String, val nillable: Boolean) {
    val rule = SrlXmlRule(kind,if (kind == "string") "preserve" else if (kind == "normalizedString") "replace" else "collapse",
        arrayOf("","","",""),-1L,-1L,-1L,emptyArray(),false,emptyArray(),"",emptyArray(),null)
}
internal class SrlXmlIdentityConstraint(val kind: String, val selector: SrlXmlIdentityPath, val fields: Array<SrlXmlIdentityField>, val refer: Int)
internal class SrlXmlIdentityScope(val constraints: Array<SrlXmlIdentityConstraint>)

// Observe starts/text/ends directly, retaining only bounded tuples and active scalar captures, never sibling trees.
internal class SrlXmlIdentity(private val scope: SrlXmlIdentityScope, private val limits: XmlIdentityLimits, private val operation: SrlXmlBudget.Operation? = null) {
    private val packageField = operation?.field ?: ""
    // Deferred identity work retains the owner selected when its observer was created.
    private fun quota(message: String): Nothing { operation?.at(packageField) { operation.limit(message) };throw IllegalArgumentException(message) }
    private class Capture(val depth: Int, val nil: Boolean) { val text = StringBuilder(); var bytes = 0 }
    private class Candidate(val constraint: Int, val depth: Int, count: Int) {
        val values = arrayOfNulls<String>(count); val seen = BooleanArray(count)
        val captures = arrayOfNulls<Capture>(count); var bytes = 0
    }
    private val path = mutableListOf<SrlXmlName>()
    private val active = mutableListOf<Candidate>()
    private val tables = Array(scope.constraints.size) { mutableSetOf<String>() }
    private var selected = 0; private var tuples = 0; private var tupleBytes = 0
    private var workUnits = 0L; private var complete = false; private var failed = false
    init {
        operation?.charge("allocation",scope.constraints.size*128L,packageField)
        if (!(scope.constraints.size <= limits.maxConstraints)) quota("XML identity constraint limit")
        for (item in scope.constraints) {
            require(item.fields.isNotEmpty()) { "XML identity field limit" };if(item.fields.size>limits.maxFields) quota("XML identity field limit")
            require(item.kind != "keyref" || item.refer in scope.constraints.indices && scope.constraints[item.refer].kind != "keyref") { "XML keyref target outside owner scope" }
        }
    }
    // A callback failure permanently invalidates this private index; the caller's exact exception is preserved.
    private inline fun checked(action: () -> Unit) { try { action() } catch (failure: Exception) { failed = true; operation?.failure(packageField); throw failure } }
    // Charge before scanning names or retaining values; subtraction keeps zero/maximum policies exact.
    private fun work(count: Long) { operation?.charge("work",count,packageField); if (!(count >= 0 && count <= limits.maxWorkUnits - workUnits)) quota("XML identity work limit"); workUnits += count }
    // Charge full expanded names, including large common URI prefixes, before exact comparisons.
    private fun nameBytes(name: SrlXmlName): Long = srlXmlUtf8Size(name.local) + srlXmlUtf8Size(name.namespace) + 2
    // Compare prepared relative paths without allocating suffix arrays or interpreting XPath.
    private fun matches(first: Int, expected: SrlXmlIdentityPath): Boolean {
        val count = path.size-first; work(count.toLong()+expected.steps.size+1)
        if (if (expected.descendants) count < expected.steps.size else count != expected.steps.size) return false
        val offset = if (expected.descendants) path.size-expected.steps.size else first
        for (index in expected.steps.indices) {
            val actual=path[offset+index]; val wanted=expected.steps[index]
            work(minOf(nameBytes(actual),nameBytes(wanted))+1); if (actual != wanted) return false
        }
        return true
    }
    // Canonicalize exact builtin values independently of native numeric widths and QName prefixes.
    private fun key(raw: String, field: SrlXmlIdentityField, resolve: (String) -> String?): String {
        val bytes=srlXmlUtf8Size(raw); if (!(bytes <= limits.maxTupleBytes)) quota("XML identity field-byte limit"); work(bytes+1)
        if (field.kind == "QName") {
            val text=raw.trim(' ','\t','\r','\n'); val parts=text.split(':')
            require(parts.size in 1..2 && parts.all { it.isNotEmpty() }) { "Invalid XML identity QName" }
            val rule=SrlXmlRule("NCName","collapse",arrayOf("","","",""),-1L,-1L,-1L,emptyArray(),false,emptyArray(),"",emptyArray(),null)
            for (part in parts) rule.check(part)
            val prefix=if (parts.size == 2) parts[0] else ""; val uri=resolve(prefix)
            require(prefix.isEmpty() || uri != null) { "Undeclared XML identity QName prefix" }
            return "q"+(uri ?: "")+'\u0000'+parts.last()
        }
        val value=field.rule.check(raw); val kind=field.kind
        if (kind == "boolean") return if (srlXmlBoolean(value)) "btrue" else "bfalse"
        if (kind in integerKinds || kind == "decimal") {
            val decimal=java.math.BigDecimal(value).stripTrailingZeros()
            return "d"+(if (decimal.signum() == 0) "0" else decimal.toPlainString())
        }
        require(kind != "float" && kind != "double") { "Floating XML identity fields are unsupported" }
        return (if (kind == "anyURI") "u" else "s")+value
    }
    // Admit canonical bytes before retaining one partial field; type markers are private indexing overhead.
    private fun store(candidate: Candidate, index: Int, value: String) {
        val bytes=srlXmlUtf8Size(value)-(if (value.startsWith("q")) 2 else 1)
        operation?.charge("allocation",bytes*2+32,packageField)
        if (!(bytes <= limits.maxTupleBytes-candidate.bytes)) quota("XML identity candidate tuple-byte limit")
        candidate.bytes += bytes.toInt(); candidate.values[index]=value
    }
    // Observe original full attributes before nil/type metadata is consumed by the generated owning frame.
    fun start(name: SrlXmlName, attributes: List<SrlXmlAttribute>, resolve: (String) -> String?) = checked {
        require(!failed && !complete) { "XML identity observer inactive" }; work(nameBytes(name)+1)
        if (!(path.size < limits.maxDepth)) quota("XML identity depth limit")
        require(active.none { candidate -> candidate.captures.any { it != null } }) { "XML identity scalar has complex child content" }
        path.add(name)
        for ((index,item) in scope.constraints.withIndex()) if (matches(1,item.selector)) {
            if (!(selected < limits.maxSelectedNodes)) quota("XML identity selected-node limit"); ++selected
            operation?.charge("allocation",128L+item.fields.size*32,packageField)
            active.add(Candidate(index,path.size,item.fields.size))
        }
        for (candidate in active) {
            val constraint=scope.constraints[candidate.constraint]
            for ((index,field) in constraint.fields.withIndex()) {
                if (!matches(candidate.depth,field.path)) continue
                val attribute=field.path.attribute?.let { wanted -> attributes.firstOrNull { it.name == wanted } }
                if (field.path.attribute != null && attribute == null) continue
                require(constraint.kind != "key" || !field.nillable) { "XSD key field declaration must not be nillable" }
                require(!candidate.seen[index]) { "XML identity field selects multiple values" }; candidate.seen[index]=true
                if (attribute != null) store(candidate,index,key(attribute.value,field,resolve))
                else {
                    val nil=(attributes.firstOrNull { it.name == nilName }?.value ?: "false").trim(' ','\t','\r','\n')
                    require(nil == "true" || nil == "1" || nil == "false" || nil == "0") { "Malformed XML identity nil" }
                    candidate.captures[index]=Capture(path.size,nil == "true" || nil == "1")
                }
            }
        }
    }
    // Charge logical UTF8 text independently of parser chunk/entity/CDATA boundaries.
    fun text(value: String) = checked {
        require(!failed && !complete && path.isNotEmpty()) { "XML identity observer inactive" }
        val bytes=srlXmlUtf8Size(value); work(bytes)
        for (candidate in active) for (capture in candidate.captures) if (capture != null) {
            work(bytes); require(!capture.nil || value.isEmpty()) { "Nil XML identity field has text" }
            operation?.charge("allocation",bytes*2,packageField)
            if (!(bytes <= limits.maxTupleBytes-capture.bytes)) quota("XML identity field-byte limit")
            capture.bytes += bytes.toInt(); capture.text.append(value)
        }
    }
    // Close captures while the provider still exposes their original namespace vocabulary.
    fun end(resolve: (String) -> String?) = checked {
        require(!failed && !complete && path.isNotEmpty()) { "Unbalanced XML identity end" }; work(1)
        for (candidate in active) {
            val constraint=scope.constraints[candidate.constraint]
            for (index in candidate.captures.indices) {
                work(1); val capture=candidate.captures[index]
                if (capture != null && capture.depth == path.size) {
                    if (!capture.nil) store(candidate,index,key(capture.text.toString(),constraint.fields[index],resolve))
                    candidate.captures[index]=null
                }
            }
        }
        val iterator=active.iterator()
        while (iterator.hasNext()) {
            val candidate=iterator.next(); if (candidate.depth != path.size) continue; iterator.remove()
            val constraint=scope.constraints[candidate.constraint]
            if (candidate.values.any { it == null }) { require(constraint.kind != "key") { "XSD key tuple incomplete or nil" }; continue }
            if (!(tuples < limits.maxTuples)) quota("XML identity tuple-count limit")
            if (!(candidate.bytes <= limits.maxTupleBytes-tupleBytes)) quota("XML identity tuple-byte limit")
            operation?.charge("reference",1,packageField)
            operation?.charge("allocation",candidate.bytes*2L+64,packageField)
            ++tuples; tupleBytes += candidate.bytes; work(candidate.bytes.toLong()+1)
            val tuple=buildString { for (value in candidate.values) { append(value!!.length); append(':'); append(value) } }
            require(tables[candidate.constraint].add(tuple) || constraint.kind == "keyref") { "Duplicate XSD key/unique tuple" }
        }
        path.removeAt(path.lastIndex); complete=path.isEmpty()
    }
    // Resolve complete forward references only after all owner-local target tables exist.
    fun finish() = checked {
        require(!failed && complete && path.isEmpty() && active.isEmpty()) { "Incomplete or failed XML identity scope" }; work(1)
        for ((index,item) in scope.constraints.withIndex()) if (item.kind == "keyref") {
            for (tuple in tables[index]) { work(srlXmlUtf8Size(tuple)+1); require(tuple in tables[item.refer]) { "Unresolved XSD keyref tuple" } }
        }
    }
    companion object {
        private val nilName=SrlXmlName("nil","http://www.w3.org/2001/XMLSchema-instance")
)identitykotlin"
R"identitykotlin(        private val integerKinds=setOf("byte","unsignedByte","short","unsignedShort","int","unsignedInt","long","unsignedLong","integer","positiveInteger","negativeInteger","nonPositiveInteger","nonNegativeInteger")
    }
}
)identitykotlin"
