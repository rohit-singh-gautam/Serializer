// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmloutput(
// Write tokens directly; completed descendants are never retained.
internal class SrlXmlOutput(private val limits: XmlLimits, val mapLimits: XmlMapLimits = XmlMapLimits(), val mixedLimits: XmlMixedLimits = XmlMixedLimits(), val digestLimits: XmlDigestLimits = XmlDigestLimits(), val identityLimits: XmlIdentityLimits = XmlIdentityLimits(), val extensionOptions: XmlExtensionOptions = XmlExtensionOptions()) {
    val listLimit: Int get() = limits.maxNodes
    // Optional package scopes leave ordinary XML policies and model namespaces unchanged.
    fun packageRecord(): java.io.Closeable? = limits.srlPackageContext?.record()
    // Borrow only this synchronous codec's optional operation context.
    fun packageContext(): SrlXmlBudget.Operation? = limits.srlPackageContext
    // Select a schema member before validation or provider presentation begins.
    fun packageField(name: String) { limits.srlPackageContext?.member(name) }
    // Keep lexical failures attributed to their member after nested token scopes have unwound.
    fun packageFailure() { limits.srlPackageContext?.failure() }
    private val budget = SrlXmlBudget(limits)
    private val bytes = java.io.ByteArrayOutputStream()
    private data class Frame(val name: SrlXmlName, val local: String,
        val attributes: MutableMap<SrlXmlName,String> = linkedMapOf(), val bindings: MutableMap<String,String> = mutableMapOf(), val pinned: Boolean = false, var pending: Boolean = true, val packageField: String = "")
    private val stack = mutableListOf<Frame>()
    private data class Identity(val value: SrlXmlIdentity, val depth: Int)
    private val identities = mutableListOf<Identity>()
    init { require(limits.maxBytes >= 0 && limits.maxNodes >= 0 && limits.maxDepth >= 0 && limits.maxTextBytes >= 0) { "Negative XML limit" } }
    // Check UTF-8 output size before growing the owned document buffer.
    private fun append(value: String) { val count = srlXmlUtf8Size(value); limits.srlPackageContext?.charge("work",count); limits.srlPackageContext?.charge("allocation",count*2); if (!(count <= limits.maxBytes - bytes.size())) SrlXmlBudget.limit("XML output limit",limits.srlPackageContext); bytes.write(value.toByteArray(Charsets.UTF_8)) }
    // Resolve QName vocabulary through active generated start tags without retaining completed elements.
    private fun identityNamespace(prefix: String): String? {
        if (prefix == "xml") return "http://www.w3.org/XML/1998/namespace"
        for (index in stack.indices.reversed()) stack[index].bindings[prefix]?.let { return it }
        return if (prefix.isEmpty()) "" else null
    }
    // Resolve typed lexical values through active generated scopes without a detached namespace copy.
    fun namespace(prefix: String): String? = identityNamespace(prefix)
    // Delay observer starts until every original expanded attribute and namespace declaration is available.
    private fun identityStart() {
        if (identities.isEmpty()) return
        val frame=stack.last(); val attributes=frame.attributes.map { SrlXmlAttribute(it.key,it.value) }
        for (identity in identities) identity.value.start(frame.name,attributes,::identityNamespace)
    }
    // Register an index after opening its owner but before writing the owner's attributes or content.
    fun observe(scope: SrlXmlIdentityScope) {
        require(stack.isNotEmpty() && stack.last().pending) { "Invalid XML identity output position" }
        identities.add(Identity(SrlXmlIdentity(scope,identityLimits,limits.srlPackageContext),stack.size))
    }
    // Close a pending start tag before its first text or child token.
    private fun seal() { if (stack.isNotEmpty() && stack.last().pending) { val frame = stack.last(); val action = { identityStart(); append(">"); frame.pending = false }; limits.srlPackageContext?.at(frame.packageField,action) ?: action() } }
    // Admit one generated element without allocating a tree node.
    fun open(name: SrlXmlName, preservedScope: Map<String,String>? = null) {
        budget.node(stack.size + 1); budget.text(name.local); budget.text(name.namespace); seal()
        if (preservedScope != null) {
            if (limits.srlPackageContext != null) for ((prefix,uri) in preservedScope) {
                limits.srlPackageContext!!.charge("work",prefix.length+uri.length+1L)
                limits.srlPackageContext!!.charge("allocation",64L+prefix.length*2L+uri.length*2L)
            }
            srlXmlExtensionBindings(preservedScope,limits.srlPackageContext)
        }
        val bindings=preservedScope?.toMutableMap() ?: mutableMapOf()
        if (name.namespace.isEmpty()) {
            require(bindings[""] == null || bindings[""]!!.isEmpty()) { "Preserved default namespace conflicts with an unqualified XML name" }
            if (!identityNamespace("").isNullOrEmpty()) { bindings[""]="" }
        }
        var prefix=if (name.namespace == "http://www.w3.org/XML/1998/namespace") "xml" else "srl"
        if (preservedScope != null && name.namespace.isNotEmpty()) {
            bindings.entries.filter { it.value == name.namespace }.minOfOrNull { it.key }?.let { prefix=it }
        }
        var suffix=0
        while (bindings[prefix] != null && bindings[prefix] != name.namespace) { ++suffix; prefix="srl"+suffix }
        val local = if (name.namespace.isEmpty() || prefix.isEmpty()) name.local else prefix + ":" + name.local
        append("<" + local)
        if (name.namespace.isNotEmpty() && prefix != "xml") { bindings[prefix]=name.namespace }
        if (preservedScope == null) {
            if (name.namespace.isNotEmpty() && prefix != "xml") { append(" xmlns:"+prefix+"=\""+srlXmlEscape(name.namespace,true,limits.srlPackageContext)+"\"") }
            if (bindings.containsKey("")) { append(" xmlns=\"\"") }
        } else {
            for ((declared,uri) in bindings.toSortedMap()) {
                if (declared == "xml") { continue }
                budget.node(stack.size+1); budget.text(declared); budget.text(uri)
                append(" xmlns"+(if (declared.isEmpty()) "" else ":"+declared)+"=\""+srlXmlEscape(uri,true,limits.srlPackageContext)+"\"")
            }
        }
        stack.add(Frame(name,local,bindings=bindings,pinned=preservedScope != null,packageField=limits.srlPackageContext?.field ?: ""))
    }
    // Choose a generated prefix without changing the meaning of any retained opaque lexical value.
    private fun prefixFor(uri: String, preferred: String): String {
        if (uri == "http://www.w3.org/XML/1998/namespace") { return "xml" }
        val frame=stack.last(); var prefix=preferred; var suffix=0
        if (frame.pinned) {
            frame.bindings.entries.filter { it.key.isNotEmpty() && it.value == uri }.minOfOrNull { it.key }?.let { return it }
        }
        while (frame.bindings[prefix] != null && frame.bindings[prefix] != uri) { ++suffix; prefix=preferred+"_"+suffix }
        if (frame.bindings[prefix] == null) {
            frame.bindings[prefix]=uri
            if (frame.pinned) { budget.node(stack.size); budget.text(prefix); budget.text(uri) }
            append(" xmlns:"+prefix+"=\""+srlXmlEscape(uri,true,limits.srlPackageContext)+"\"")
        }
        return prefix
    }
    // Emit each qualified attribute once, before record content.
    fun attribute(value: SrlXmlAttribute) {
        val frame = stack.last(); require(frame.pending && !frame.attributes.containsKey(value.name)) { "Invalid XML attribute position or duplicate" }
        frame.attributes[value.name]=value.value
        budget.node(stack.size); budget.text(value.name.local); budget.text(value.name.namespace); budget.text(value.value)
        val prefix = if (value.name.namespace.isEmpty()) "" else prefixFor(value.name.namespace,"a"+frame.attributes.size)
        append(" " + (if (value.name.namespace.isEmpty()) value.name.local else prefix + ":" + value.name.local) + "=\"" + srlXmlEscape(value.value, true, limits.srlPackageContext) + "\"")
    }
    // Use an isolated prefix so payload and attribute namespaces cannot shadow a type QName.
    fun typeName(identity: SrlXmlName) {
        require(stack.isNotEmpty() && stack.last().pending) { "Invalid XML type position" }
        val prefix = if (identity.namespace.isEmpty()) "" else prefixFor(identity.namespace,"srlType")
        if (prefix.isEmpty() && !identityNamespace("").isNullOrEmpty()) {
            require(stack.last().bindings[""] == null) { "Preserved default namespace conflicts with an empty XML type namespace" }
            stack.last().bindings[""]=""; append(" xmlns=\"\"")
        }
        if (prefix != "xml" && prefix.isNotEmpty()) {
            budget.node(stack.size); budget.text(prefix); budget.text(identity.namespace)
        }
        attribute(SrlXmlAttribute(SrlXmlName("type", "http://www.w3.org/2001/XMLSchema-instance"), (if (prefix.isEmpty()) "" else prefix + ":") + identity.local))
    }
    // Validate and escape only the active scalar value.
    fun text(value: String) { budget.text(value); seal(); for (identity in identities) identity.value.text(value); append(srlXmlEscape(value, false, limits.srlPackageContext)) }
    // Release the completed ancestor after writing its matching end token.
    fun close() {
        val frame=stack.last(); if (frame.pending) identityStart()
        val previousPackageField = limits.srlPackageContext?.field
        if (limits.srlPackageContext != null) limits.srlPackageContext!!.field = frame.packageField
        try {
        for (identity in identities) identity.value.end(::identityNamespace)
        val iterator=identities.iterator()
        while (iterator.hasNext()) { val identity=iterator.next(); if (identity.depth == stack.size) { identity.value.finish(); iterator.remove() } }
        stack.removeAt(stack.lastIndex); append(if (frame.pending) "/>" else "</"+frame.local+">")
        } finally { if (limits.srlPackageContext != null) limits.srlPackageContext!!.field = previousPackageField!! }
    }
    // Publish a complete bounded byte array after traversal succeeds.
    fun finish(): ByteArray { require(stack.isEmpty() && identities.isEmpty()) { "Incomplete XML output or identity scope" }; limits.srlPackageContext?.charge("allocation",bytes.size().toLong()); return bytes.toByteArray() }
}
)xmloutput"
