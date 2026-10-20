// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlextension(
// Bound only explicit foreign roots and retained semantic payload, independently of document limits.
data class XmlExtensionLimits(val maxSlots: Int = srlXmlExtensionMaxSlots,
    val maxItems: Int = srlXmlExtensionMaxItems,
    val maxPreservedBytes: Long = srlXmlExtensionMaxPreservedBytes.toLong()) {
    init { require(maxSlots >= 0 && maxItems >= 0 && maxPreservedBytes >= 0) { "Negative XML extension limit" } }
}
// Declare vocabulary understanding separately from permission to preserve opaque values.
class XmlExtensionOptions(understoodNamespaces: List<String> = emptyList(),
    val requiredPrefixes: String = "", val limits: XmlExtensionLimits = XmlExtensionLimits()) {
    val understoodNamespaces: List<String>
    init {
        require(understoodNamespaces.size <= limits.maxItems) { "XML capability item limit" }
        var bytes=srlXmlUtf8Size(requiredPrefixes)
        require(bytes <= srlXmlExtensionMaxPreservedBytes) { "XML capability metadata byte limit" }
        for (uri in understoodNamespaces) {
            val size=srlXmlUtf8Size(uri)
            require(size <= srlXmlExtensionMaxPreservedBytes-bytes) { "XML capability metadata byte limit" }; bytes += size
        }
        srlXmlEscape(requiredPrefixes,true)
        for (uri in understoodNamespaces) { srlXmlEscape(uri,true) }
        this.understoodNamespaces=understoodNamespaces.toList()
    }
}
internal class SrlXmlExtensionSlot(val id: String, val attribute: Boolean, val position: Int,
    val namespaces: Array<String>, val policy: Int, val minimum: Int, val maximum: Int?)
internal class SrlXmlExtensionContract(val core: String, val known: Array<SrlXmlName>,
    val required: Array<String>, val slots: Array<SrlXmlExtensionSlot>) {
    val knownNames=known.groupBy { it.namespace }.mapValues { entry -> entry.value.map { it.local }.toSet() }
    val attributeSlots=mutableMapOf<String,Int>()
    val childSlots=mutableMapOf<String,MutableMap<Int,Int>>()
    init {
        for ((index,slot) in slots.withIndex()) {
            for (uri in slot.namespaces) {
                if (slot.attribute) { attributeSlots[uri]=index }
                else { childSlots.getOrPut(uri) { mutableMapOf() }[slot.position]=index }
            }
        }
    }
}
internal class SrlXmlForeignNode(val name: SrlXmlName, val attributes: List<SrlXmlAttribute>,
    val scope: Map<String,String>, val content: MutableList<Any> = mutableListOf())
internal class SrlXmlForeignChild(val position: Int, val item: Int, val node: SrlXmlForeignNode)
internal class SrlXmlPreserved(val scope: Map<String,String>,
    val attributes: MutableList<SrlXmlAttribute> = mutableListOf(),
    val children: MutableList<SrlXmlForeignChild> = mutableListOf())

// Validate retained namespace context without interpreting opaque attribute or text lexical values.
internal fun srlXmlExtensionBindings(scope: Map<String,String>, operation: SrlXmlBudget.Operation? = null) {
    val xmlNamespace="http://www.w3.org/XML/1998/namespace"
    val xmlnsNamespace="http://www.w3.org/2000/xmlns/"
    for ((prefix,uri) in scope) {
        require((prefix.isEmpty() || srlXmlNcName(prefix)) && prefix != "xmlns" &&
            (prefix == "xml") == (uri == xmlNamespace) && (prefix.isEmpty() || uri.isNotEmpty()) &&
            uri != xmlnsNamespace && uri.trim() == uri) { "Invalid preserved XML namespace binding" }
        srlXmlEscape(uri,true,operation)
    }
}

// Keep exact slot counts and only explicitly preserved foreign content for one owning record.
internal class SrlXmlExtensions private constructor(val meta: SrlXmlExtensionContract,
    val scope: Map<String,String>, private val options: XmlExtensionOptions, existing: SrlXmlPreserved?, private val operation: SrlXmlBudget.Operation?) {
    private val bounds=options.limits
    private var counts=IntArray(0)
    private var items=0
    private var bytes=0L
    private var cursor=0
    var state: SrlXmlPreserved? = existing
        private set
    init {
        if (!(meta.slots.size <= bounds.maxSlots && meta.known.size <= bounds.maxItems &&
            meta.required.size <= bounds.maxItems)) SrlXmlBudget.limit("XML extension slot limit",operation)
        operation?.charge("allocation",64L+meta.slots.size*4L+options.understoodNamespaces.size*32L+options.requiredPrefixes.length*4L)
        operation?.charge("work",options.requiredPrefixes.length+1L)
        counts=IntArray(meta.slots.size)
        val understood=options.understoodNamespaces.toSet()
        for (uri in meta.required) { require(uri == meta.core || uri in understood) { "Required XML capability is not understood" } }
        var required=0
        for (prefix in SrlXmlRule.tokens(options.requiredPrefixes,bounds.maxItems,operation)) {
            require(required < bounds.maxItems && srlXmlNcName(prefix) && prefix != "xmlns") { "Invalid required XML capability prefix" }; ++required
            val uri=scope[prefix]
            require(uri != null && uri.isNotEmpty()) { "Unbound required XML capability prefix" }
            require(uri == meta.core || uri in understood) { "Required XML capability is not understood" }
        }
        if (existing != null) { charge(addSize(ownerOverheadBytes,scopeBytes(scope))) }
    }
    // Admit semantic bytes before allocating corresponding owning extension storage.
    fun charge(amount: Long) { operation?.charge("work",amount); operation?.charge("allocation",amount); bytes=addSize(bytes,amount) }
    // Check the selected limit before arithmetic, including caller-selected maximum native bounds.
    private fun addSize(current: Long, amount: Long): Long {
        if (!(current >= 0 && amount >= 0 && current <= bounds.maxPreservedBytes && amount <= bounds.maxPreservedBytes-current)) SrlXmlBudget.limit("XML extension preserved-byte limit",operation)
        return current+amount
    }
    // Count complete retained prefix context, including bindings used only by unknown lexical values.
    private fun scopeBytes(bindings: Map<String,String>): Long {
        srlXmlExtensionBindings(bindings,operation); var size=0L
        for ((prefix,uri) in bindings) { size=addSize(size,scopeEntryOverheadBytes+srlXmlUtf8Size(prefix)+srlXmlUtf8Size(uri)) }
        return size
    }
    // Keep every explicit core or statically known name closed even when its URI matches a wildcard slot.
    fun slot(name: SrlXmlName, attribute: Boolean, position: Int): Int {
        if (name.namespace.isEmpty() || name.namespace == meta.core || name.namespace == srlXmlModelNamespace || meta.knownNames[name.namespace]?.contains(name.local) == true) { return -1 }
        return (if (attribute) meta.attributeSlots[name.namespace] else meta.childSlots[name.namespace]?.get(position)) ?: -1
    }
    // Enforce policy and occurrence admission before capturing or skipping one unknown owner item.
    private fun admit(name: SrlXmlName, attribute: Boolean, position: Int): Boolean {
        val index=slot(name,attribute,position)
        require(index >= 0) { "XML extension has no permitted slot" }
        val selected=meta.slots[index]
        require(selected.policy != 0) { "XML extension slot rejects foreign vocabulary" }
        if (items >= bounds.maxItems) SrlXmlBudget.limit("XML extension item/occurrence limit",operation)
        require(selected.maximum == null || counts[index] < selected.maximum) { "XML extension item/occurrence limit" }
        ++items; ++counts[index]; return selected.policy == 2
    }
    // Create a sidecar lazily; skip-only traversals allocate no retained foreign state.
    private fun storage(): SrlXmlPreserved {
        state?.let { return it }
        charge(addSize(ownerOverheadBytes,scopeBytes(scope))); return SrlXmlPreserved(scope.toMap()).also { state=it }
    }
    // Preserve exact expanded attributes only after their explicit attribute slot admits them.
    fun attribute(value: SrlXmlAttribute) {
        if (admit(value.name,true,0)) {
            charge(attributeBytes(value)); storage().attributes.add(value)
        }
    }
    // Validate an edited retained name before it can reach the token writer's trusted-name path.
    private fun name(value: SrlXmlName) {
        require(srlXmlNcName(value.local) && value.namespace != "http://www.w3.org/2000/xmlns/") { "Invalid XML extension expanded name" }
        srlXmlEscape(value.namespace,true)
    }
    // Measure exact logical attribute storage and its Unicode payload without changing its lexical meaning.
    private fun attributeBytes(value: SrlXmlAttribute): Long {
        name(value.name); srlXmlEscape(value.value,true)
        return attributeOverheadBytes+srlXmlUtf8Size(value.name.local)+srlXmlUtf8Size(value.name.namespace)+srlXmlUtf8Size(value.value)
    }
    // Measure one foreign node before creating copies of its provider attributes and namespace context.
    fun measure(value: SrlXmlName, attributes: List<SrlXmlAttribute>, bindings: Map<String,String>) {
        name(value); var size=addSize(nodeOverheadBytes+srlXmlUtf8Size(value.local)+srlXmlUtf8Size(value.namespace),scopeBytes(bindings))
        for (attribute in attributes) { size=addSize(size,attributeBytes(attribute)) }; charge(size)
    }
    // Attach a completely captured foreign root at its original field and repeated-item boundary.
    private fun child(position: Int, item: Int, value: SrlXmlForeignNode?) {
        if (value != null) { charge(contentEntryOverheadBytes); storage().children.add(SrlXmlForeignChild(position,item,value)) }
    }
    // Drain only foreign items permitted at the current boundary; future slots remain for later fields.
    fun readAt(input: SrlXmlInput, position: Int, item: Int) {
        input.skipSpace()
        while (input.kind == 's' && slot(input.name(),false,position) >= 0) {
            val preserve=admit(input.name(),false,position)
            child(position,item,input.foreign(this,preserve)); input.skipSpace()
        }
    }
    // Require every explicit slot minimum before publishing a complete model or encoded byte array.
    fun finish() {
        for (index in counts.indices) { require(counts[index] >= meta.slots[index].minimum) { "Required XML extension occurrence missing" } }
    }
)xmlextension"
R"xmlextension(
    // Revalidate retained attributes while the known record's complete start tag is still pending.
    fun emitAttributes(output: SrlXmlOutput) {
        val selected=state ?: return
        for (value in selected.attributes) {
            require(admit(value.name,true,0)) { "Retained XML attribute no longer has a preserve slot" }
            charge(attributeBytes(value)); output.attribute(value)
        }
    }
    // Replay an opaque semantic subtree through the bounded writer; known model nodes are never retained.
    private fun emitNode(output: SrlXmlOutput, value: SrlXmlForeignNode) {
        measure(value.name,value.attributes,value.scope); output.open(value.name,value.scope)
        for (attribute in value.attributes) { output.attribute(attribute) }
        for (content in value.content) {
            charge(contentEntryOverheadBytes)
            when(content) {
                is String -> { srlXmlEscape(content,false); charge(srlXmlUtf8Size(content)); output.text(content) }
                is SrlXmlForeignNode -> emitNode(output,content)
                else -> throw IllegalArgumentException("Invalid retained XML extension content")
            }
        }
        output.close()
    }
    // Emit exactly the roots belonging before this known field and repeated-item boundary.
    fun emitAt(output: SrlXmlOutput, position: Int, item: Int) {
        val selected=state ?: return
        while (cursor < selected.children.size) {
            val value=selected.children[cursor]
            require(value.position > position || value.position == position && value.item >= item) { "Preserved XML extension no longer fits edited field position" }
            if (value.position != position || value.item != item) { return }
            require(admit(value.node.name,false,position)) { "Retained XML child no longer has a preserve slot" }
            charge(contentEntryOverheadBytes); emitNode(output,value.node); ++cursor
        }
    }
    // Reject positions lost when an owning repeated collection was shortened or removed.
    fun endPosition(position: Int) {
        val selected=state ?: return
        require(cursor >= selected.children.size || selected.children[cursor].position != position) { "Preserved XML extension no longer fits edited field position" }
    }
    // Require complete replay and every slot minimum before returning an owned output message.
    fun finishOutput() {
        require(state == null || cursor == state!!.children.size) { "Unreplayed XML extension position" }; finish()
    }
    companion object {
        const val contentEntryOverheadBytes=24L
        private const val ownerOverheadBytes=64L
        private const val nodeOverheadBytes=64L
        private const val scopeEntryOverheadBytes=64L
        private const val attributeOverheadBytes=48L
        // Validate input capabilities in its original complete namespace scope before consuming known fields.
        fun input(meta: SrlXmlExtensionContract, scope: Map<String,String>, options: XmlExtensionOptions, operation: SrlXmlBudget.Operation? = null): SrlXmlExtensions =
            SrlXmlExtensions(meta,scope,options,null,operation)
        // Borrow retained state during output and validate the same original vocabulary requirements.
        fun output(meta: SrlXmlExtensionContract, state: SrlXmlPreserved?, options: XmlExtensionOptions, operation: SrlXmlBudget.Operation? = null): SrlXmlExtensions =
            SrlXmlExtensions(meta,state?.scope ?: mapOf("xml" to "http://www.w3.org/XML/1998/namespace"),options,state,operation)
    }
}
)xmlextension"
R"xmlextension(
// Capture only an explicitly preserved foreign subtree; skip retains no attribute, scope or text payload.
internal fun SrlXmlInput.foreign(tracker: SrlXmlExtensions, preserve: Boolean): SrlXmlForeignNode? {
    require(kind == 's') { "Missing XML extension root" }
    val selected=name()
    val result=if (preserve) {
        val attributes=extensionAttributes(); val scope=namespaceScope()
        tracker.measure(selected,attributes,scope); SrlXmlForeignNode(selected,attributes,scope)
    } else null
    val pending=if (preserve) StringBuilder() else null
    // Coalesce text through provider chunk boundaries, comments and CDATA without changing mixed order.
    fun flush() {
        if (pending != null && pending.isNotEmpty()) {
            tracker.charge(SrlXmlExtensions.contentEntryOverheadBytes)
            result!!.content.add(pending.toString()); pending.setLength(0)
        }
    }
    advance()
    while (kind != 'e') {
        when(kind) {
            't' -> {
                if (preserve) { val text=extensionText(); tracker.charge(srlXmlUtf8Size(text)); pending!!.append(text) }
                advance()
            }
            's' -> {
                flush()
                if (preserve) { tracker.charge(SrlXmlExtensions.contentEntryOverheadBytes) }
                val child=foreign(tracker,preserve)
                if (child != null) { result!!.content.add(child) }
            }
            else -> throw IllegalArgumentException("Incomplete XML extension subtree")
        }
    }
    require(name() == selected) { "Unexpected XML extension closing tag" }
    flush(); advance(); return result
}
)xmlextension"
