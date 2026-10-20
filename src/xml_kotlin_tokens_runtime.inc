// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// Check strict UTF8 without retaining a second complete copy of the input document.
internal fun srlXmlCheckUtf8(bytes: ByteArray) {
    var index = 0
    while (index < bytes.size) {
        val first = bytes[index++].toInt() and 255
        if (first < 128) continue
        val count = when (first) { in 194..223 -> 1; in 224..239 -> 2; in 240..244 -> 3; else -> throw IllegalArgumentException("Invalid XML UTF8") }
        require(count <= bytes.size - index) { "Incomplete XML UTF8" }
        val second = bytes[index].toInt() and 255
        require((first != 224 || second >= 160) && (first != 237 || second < 160) &&
            (first != 240 || second >= 144) && (first != 244 || second < 144)) { "Invalid XML UTF8 scalar" }
        repeat(count) { require((bytes[index++].toInt() and 192) == 128) { "Invalid XML UTF8 continuation" } }
    }
}

// Keep provider-specific parse failures inside the generated XML argument-error contract.
internal inline fun <T> srlXmlTokenCheck(action: () -> T): T = try { action() }
    catch (failure: javax.xml.stream.XMLStreamException) { throw IllegalArgumentException("Malformed or unsupported XML document", failure) }

// Identify the reserved nil attribute independently of its presentation prefix.
internal val srlXmlNilName = SrlXmlName("nil", "http://www.w3.org/2001/XMLSchema-instance")
// Pull one operation-local provider event without retaining completed model subtrees.
internal class SrlXmlInput(bytes: ByteArray, limits: XmlLimits, val mapLimits: XmlMapLimits = XmlMapLimits(), val mixedLimits: XmlMixedLimits = XmlMixedLimits(), val digestLimits: XmlDigestLimits = XmlDigestLimits(), val identityLimits: XmlIdentityLimits = XmlIdentityLimits(), val extensionOptions: XmlExtensionOptions = XmlExtensionOptions(), private val preserveNamespaces: Boolean = false) : java.io.Closeable {
    private val budget = SrlXmlBudget(limits)
    private val operation = limits.srlPackageContext
    val packageActive: Boolean get() = operation != null
    // Borrow the active package ledger adapter without copying its namespace or field state.
    fun packageContext(): SrlXmlBudget.Operation? = operation
    private data class PackageRecord(val depth: Int, val owner: String, val children: Map<SrlXmlName,String>, val attributes: Map<SrlXmlName,String>)
    private val packageRecords = mutableListOf<PackageRecord>()
    private val packagePaths = mutableListOf<String>()
    // Register source member names only for package calls, before entering record content.
    fun packageRecord(children: Map<SrlXmlName,String>, attributes: Map<SrlXmlName,String>): java.io.Closeable? {
        val selected = operation ?: return null
        val owner = selected.field; val scope = selected.record()
        packageRecords.add(PackageRecord(depth,owner,children,attributes))
        return java.io.Closeable { packageRecords.removeAt(packageRecords.lastIndex); scope.close() }
    }
    // Select a schema member before lexical conversion or native construction.
    fun packageField(name: String) { operation?.member(name) }
    // Reattribute semantic failures after lookahead has already admitted a sibling provider token.
    fun packageFailure() { operation?.failure() }
    // A provider token uses its original element scope rather than the traversal's lookahead cursor.
    private fun packagePath(name: SrlXmlName, selectedDepth: Int): String {
        val scope = packageRecords.lastOrNull { it.depth+1 == selectedDepth }
        val member = scope?.children?.get(name)
        if (scope != null && member != null) return listOf(scope.owner,member).filter { it.isNotEmpty() }.joinToString(".")
        return packagePaths.lastOrNull() ?: operation?.field ?: ""
    }
    val listLimit: Int get() = budget.limits.maxNodes
    private val reader: javax.xml.stream.XMLStreamReader
    private var depth = 0
    private var consumeNil = false
    private var consumeType = false
    private val namespaceScopes=mutableListOf<Map<String,String>>()
    private data class Identity(val value: SrlXmlIdentity, val depth: Int)
    private val identities = mutableListOf<Identity>()
    var kind: Char = '\u0000'
        private set
    init {
        require(limits.maxBytes >= 0 && limits.maxNodes >= 0 && limits.maxDepth >= 0 && limits.maxTextBytes >= 0) { "Negative XML limit" }
        require(bytes.size <= limits.maxBytes) { "XML byte limit" }
        operation?.charge("work",bytes.size.toLong()); operation?.charge("allocation",bytes.size.toLong())
        srlXmlCheckUtf8(bytes)
        val factory = javax.xml.stream.XMLInputFactory.newFactory()
        factory.setProperty(javax.xml.stream.XMLInputFactory.SUPPORT_DTD, false)
        factory.setProperty(javax.xml.stream.XMLInputFactory.IS_SUPPORTING_EXTERNAL_ENTITIES, false)
        factory.setProperty(javax.xml.stream.XMLInputFactory.IS_NAMESPACE_AWARE, true)
        factory.xmlResolver = javax.xml.stream.XMLResolver { _, _, _, _ -> throw javax.xml.stream.XMLStreamException("External XML resolution prohibited") }
        reader = srlXmlTokenCheck { factory.createXMLStreamReader(java.io.ByteArrayInputStream(bytes)) }
        try {
            require(reader.version == null || reader.version == "1.0") { "XML1.0 required" }
            val encoding = reader.characterEncodingScheme
            require(encoding == null || encoding.replace("-", "").equals("UTF8", ignoreCase = true)) { "XML UTF8 required" }
            advance()
        } catch (failure: Exception) {
            try { reader.close() } catch (closing: Exception) { failure.addSuppressed(closing) }
            throw failure
        }
    }
    // Return prefix-independent identity for the active start or end element.
    fun name(): SrlXmlName = SrlXmlName(reader.localName, reader.namespaceURI ?: "")
    // Snapshot complete vocabulary only for graphs with explicit foreign-preservation mappings.
    fun namespaceScope(): Map<String,String> {
        require(preserveNamespaces) { "XML namespace preservation was not enabled for this graph" }
        if (operation != null) for (scope in namespaceScopes) for ((prefix,uri) in scope) {
            operation.charge("work",prefix.length+uri.length+1L); operation.charge("allocation",64L+prefix.length*2L+uri.length*2L)
        }
        val result=linkedMapOf("xml" to "http://www.w3.org/XML/1998/namespace")
        for (scope in namespaceScopes) { result.putAll(scope) }; return result
    }
    // Admit every inherited entry before retaining a namespace-dependent scalar's owning snapshot.
    fun namespaceValueScope(): Map<String,String> {
        require(preserveNamespaces) { "XML namespace preservation was not enabled for this graph" }
        budget.node(depth); budget.text("xml"); budget.text("http://www.w3.org/XML/1998/namespace")
        budget.node(depth)
        for (scope in namespaceScopes) for ((prefix,uri) in scope) {
            budget.node(depth); budget.text(prefix); budget.text(uri)
        }
        val result=linkedMapOf("xml" to "http://www.w3.org/XML/1998/namespace", "" to "")
        for (scope in namespaceScopes) { result.putAll(scope) }
        return result
    }
    // Expose only the current provider token to the opaque foreign-only capture helper.
    fun extensionText(): String { require(kind == 't') { "Missing XML extension text" }; return reader.text }
    // Retain original expanded attributes independently of generated nil/type consumption.
    fun extensionAttributes(): List<SrlXmlAttribute> = identityAttributes()
    // Snapshot original full start-tag attributes before generated nil/type consumption.
    private fun identityAttributes(): List<SrlXmlAttribute> = (0 until reader.attributeCount).map { index ->
        SrlXmlAttribute(SrlXmlName(reader.getAttributeLocalName(index),reader.getAttributeNamespace(index) ?: ""),reader.getAttributeValue(index))
    }
    // Start an owner-local index on the current event, which ancestor indexes already observed exactly once.
    fun observe(scope: SrlXmlIdentityScope) {
        require(kind == 's') { "XML identity owner must start on an element" }
        val identity=SrlXmlIdentity(scope,identityLimits,operation)
        identity.start(name(),identityAttributes()) { prefix -> reader.getNamespaceURI(prefix) }
        identities.add(Identity(identity,depth))
    }
    // Charge provider tokens before exposing them to the generated record traversal.
    fun advance() {
      srlXmlTokenCheck {
        while (reader.hasNext()) {
            operation?.charge("work",1)
            when (reader.next()) {
                javax.xml.stream.XMLStreamConstants.START_ELEMENT -> {
                    val packagePath = if (operation == null) "" else packagePath(name(),depth+1)
                    if (operation != null) packagePaths.add(packagePath)
                    val previousPackagePath = operation?.field
                    if (operation != null) operation.field = packagePath
                    try {
                    budget.node(++depth); budget.text(name().local); budget.text(name().namespace)
                    if (preserveNamespaces) {
                        val declarations=linkedMapOf<String,String>()
                        for (index in 0 until reader.namespaceCount) {
                            declarations[reader.getNamespacePrefix(index) ?: ""]=reader.getNamespaceURI(index) ?: ""
                        }
                        namespaceScopes.add(declarations)
                    }
                    for (index in 0 until reader.namespaceCount) {
                        budget.node(depth); budget.text(reader.getNamespacePrefix(index) ?: ""); budget.text(reader.getNamespaceURI(index) ?: "")
                    }
                    for (index in 0 until reader.attributeCount) {
                        budget.node(depth); budget.text(reader.getAttributeLocalName(index)); budget.text(reader.getAttributeNamespace(index) ?: ""); budget.text(reader.getAttributeValue(index))
                    }
                    if (identities.isNotEmpty()) {
                        val attributes=identityAttributes()
                        for (identity in identities) identity.value.start(name(),attributes) { prefix -> reader.getNamespaceURI(prefix) }
                    }
                    } finally { if (operation != null) operation.field = previousPackagePath!! }
                    kind = 's'; return
                }
                javax.xml.stream.XMLStreamConstants.END_ELEMENT -> {
                    for (identity in identities) identity.value.end { prefix -> reader.getNamespaceURI(prefix) }
                    val iterator=identities.iterator()
                    while (iterator.hasNext()) { val identity=iterator.next(); if (identity.depth == depth) { identity.value.finish(); iterator.remove() } }
                    if (preserveNamespaces) { namespaceScopes.removeAt(namespaceScopes.lastIndex) }
                    if (operation != null) packagePaths.removeAt(packagePaths.lastIndex)
                    --depth; kind = 'e'; return
                }
                javax.xml.stream.XMLStreamConstants.CHARACTERS, javax.xml.stream.XMLStreamConstants.CDATA,
                javax.xml.stream.XMLStreamConstants.SPACE -> {
                    val action = { budget.text(reader.text); for (identity in identities) identity.value.text(reader.text) }
                    operation?.at(packagePaths.lastOrNull() ?: "",action) ?: action()
                    kind = 't'; return
                }
                javax.xml.stream.XMLStreamConstants.DTD, javax.xml.stream.XMLStreamConstants.ENTITY_REFERENCE ->
                    throw IllegalArgumentException("XML entities and DTDs are prohibited")
            }
        }
        kind = '\u0000'
      }
    }
    // Accept only XML indentation between statically mapped fields.
    fun skipSpace() {
        while (kind == 't') {
            require(reader.text.all { it == ' ' || it == '\t' || it == '\r' || it == '\n' }) { "Unexpected XML record text" }
            advance()
        }
    }
    // Inspect the next child without consuming an unrelated or duplicate name.
    fun has(name: SrlXmlName): Boolean { skipSpace(); return kind == 's' && name() == name }
    // Snapshot only this record's attributes before advancing into its content.
    fun open(name: SrlXmlName): SrlXmlInputFields {
)serializer"
R"serializer(        require(has(name)) { "Unexpected XML element" }
        val attributes = linkedMapOf<SrlXmlName, String>()
        for (index in 0 until reader.attributeCount) {
            val key = SrlXmlName(reader.getAttributeLocalName(index), reader.getAttributeNamespace(index) ?: "")
            if (consumeNil && key == srlXmlNilName) continue
            if (consumeType && key == SrlXmlName("type", "http://www.w3.org/2001/XMLSchema-instance")) continue
            if (operation != null) {
                val scope = packageRecords.lastOrNull { it.depth == depth }
                val field = scope?.attributes?.get(key)
                val selected = if (scope != null && field != null) listOf(scope.owner,field).filter { it.isNotEmpty() }.joinToString(".") else operation.field
                operation.charge("allocation",64L+reader.getAttributeValue(index).length*2,selected)
            }
            require(attributes.put(key, reader.getAttributeValue(index)) == null) { "Duplicate XML attribute" }
        }
        consumeNil = false
        consumeType = false
        advance()
        return SrlXmlInputFields(this, attributes)
    }
    // Resolve type metadata before advancing beyond its provider namespace context.
    fun typeIdentity(required: Boolean): SrlXmlName? {
        require(kind == 's') { "Missing XML variant payload" }
        val raw = reader.getAttributeValue("http://www.w3.org/2001/XMLSchema-instance", "type")
        require(!required || raw != null) { "Missing XML variant type" }
        if (raw == null) return null
        val identity = srlXmlTypeQName(raw) { prefix -> reader.getNamespaceURI(prefix) }
        consumeType = true
        return identity
    }
    // Element selection may carry a matching hint, but never a conflicting or unknown type.
    fun expectType(expected: SrlXmlName, required: Boolean) {
        val actual = typeIdentity(required)
        require(actual == null || actual == expected) { "Conflicting XML variant type" }
    }
    // Resolve explicit nil states without conflating absent, nil and empty content.
    fun presence(name: SrlXmlName, absent: Boolean, nil: Boolean): Char {
        if (!has(name)) { require(absent) { "Required XML presence element is absent" }; return 'a' }
        var marker: String? = null
        for (index in 0 until reader.attributeCount) {
            if (SrlXmlName(reader.getAttributeLocalName(index), reader.getAttributeNamespace(index) ?: "") == srlXmlNilName) {
                require(marker == null) { "Duplicate XML xsi:nil attribute" }
                marker = reader.getAttributeValue(index)
            }
        }
        if (marker == null) return 'v'
        require(nil) { "XML xsi:nil is not permitted by presence policy" }
        val value = marker.trim(' ', '\t', '\r', '\n')
        require(value in setOf("true", "1", "false", "0")) { "Invalid XML xsi:nil Boolean" }
        consumeNil = true
        if (value == "false" || value == "0") return 'v'
        require(open(name).text().isEmpty()) { "Nil XML value contains text or children" }
        return 'n'
    }
    // Join only the active bounded scalar across entity and CDATA provider boundaries.
    fun text(): String {
        val text = StringBuilder()
        while (kind == 't') { operation?.charge("allocation",reader.textLength*2L); text.append(reader.text); advance() }
        require(kind == 'e') { "XML scalar contains nested content" }
        advance()
        operation?.charge("allocation",text.length*2L); return text.toString()
    }
    // Coalesce adjacent provider chunks through comments and CDATA until the next typed child.
    fun mixedText(mixed: SrlXmlMixedBudget): String {
        val text = StringBuilder()
        while (kind == 't') { val chunk = reader.text; mixed.text(chunk); text.append(chunk); advance() }
        return text.toString()
    }
    // Refuse attributes on scalar child elements before decoding their lexical value.
    fun scalar(name: SrlXmlName): String = open(name).text()
    // Require exact document completion before publishing any candidate.
    fun finish() { skipSpace(); require(kind == '\u0000' && identities.isEmpty()) { "Unexpected XML trailing content or incomplete identity scope" } }
    // Release the provider on lexical, shape, resource and successful completion paths.
    override fun close() { srlXmlTokenCheck { reader.close() } }
}

// Retain only current-record attribute values until membership has been checked.
internal class SrlXmlInputFields(private val input: SrlXmlInput, private val attributes: MutableMap<SrlXmlName, String>) {
    private var closed = false
    // Preserve absence and require each declared attribute exactly once.
    fun attribute(name: SrlXmlName, optional: Boolean): String? {
        val value = attributes.remove(name)
        require(optional || value != null) { "Required XML attribute missing" }
        return value
    }
    // Reject wrapper or leftover entry attributes before any map payload allocation.
    fun requireNoAttributes() { require(attributes.isEmpty()) { "Unexpected XML map attribute" } }
    // Consume only explicitly permitted foreign attributes after all known metadata and fields.
    fun extensions(tracker: SrlXmlExtensions) {
        for ((name,value) in attributes) { tracker.attribute(SrlXmlAttribute(name,value)) }
        attributes.clear()
    }
    // Finish simple content after all declared attributes have been consumed.
    fun text(): String {
        require(attributes.isEmpty()) { "Unexpected XML attribute" }
        val result = input.text(); closed = true; return result
    }
    // Reject unknown attributes and out-of-order or duplicate children transactionally.
    fun finish() {
        require(attributes.isEmpty()) { "Unexpected XML attribute" }
        if (!closed) {
            input.skipSpace(); require(input.kind == 'e') { "Unexpected XML field" }; input.advance()
        }
    }
}
)serializer"
