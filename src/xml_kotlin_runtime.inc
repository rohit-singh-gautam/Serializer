// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// Optional complete-object XML runtime; no entity, DTD, network or schema fetching is permitted.
data class XmlLimits(val maxBytes: Int = 16777216, val maxNodes: Int = 100000,
                     val maxDepth: Int = 64, val maxTextBytes: Int = 8388608)
internal data class SrlXmlName(val local: String, val namespace: String = "")
internal data class SrlXmlAttribute(val name: SrlXmlName, val value: String)
// Count UTF-8 bytes without allocating a second copy of model text.
internal fun srlXmlUtf8Size(value: String): Long {
    var bytes = 0L
    val iterator = value.codePoints().iterator()
    while (iterator.hasNext()) {
        val scalar = iterator.nextInt()
        bytes += if (scalar <= 0x7f) 1 else if (scalar <= 0x7ff) 2 else if (scalar <= 0xffff) 3 else 4
    }
    return bytes
}
internal class SrlXmlBudget(val limits: XmlLimits) {
    private var nodes = 0
    private var textBytes = 0L
    fun node(depth: Int) { require(depth <= limits.maxDepth && ++nodes <= limits.maxNodes) { "XML output tree limit" } }
    fun text(value: String) { textBytes += srlXmlUtf8Size(value); require(textBytes <= limits.maxTextBytes && textBytes <= limits.maxBytes) { "XML output text limit" } }
}
internal class SrlXmlNode(val name: SrlXmlName, private val budget: SrlXmlBudget? = null, private val depth: Int = 1) {
    var text: String = ""
    val attributes = mutableListOf<SrlXmlAttribute>()
    val children = mutableListOf<SrlXmlNode>()
    fun makeChild(name: SrlXmlName): SrlXmlNode { budget?.node(depth+1); return SrlXmlNode(name,budget,depth+1) }
    fun setValueText(value: String) { budget?.text(value); text = value }
    fun addAttribute(value: SrlXmlAttribute) { budget?.node(depth); budget?.text(value.value); attributes.add(value) }
    companion object {
        fun root(name: SrlXmlName, limits: XmlLimits): SrlXmlNode {
            val budget = SrlXmlBudget(limits); budget.node(1); return SrlXmlNode(name,budget)
        }
    }
}
// Validate Unicode scalars and escape XML syntax without locale-sensitive conversions.
internal fun srlXmlEscape(value: String, attribute: Boolean): String {
    val out = StringBuilder()
    val iterator = value.codePoints().iterator()
    while (iterator.hasNext()) {
        val scalar = iterator.nextInt()
        require(scalar == 9 || scalar == 10 || scalar == 13 || scalar in 32..0xd7ff ||
                scalar in 0xe000..0xfffd || scalar in 0x10000..0x10ffff) { "Invalid XML character" }
        when (scalar) {
            38 -> out.append("&amp;")
            60 -> out.append("&lt;")
            62 -> out.append("&gt;")
            34 -> if (attribute) out.append("&quot;") else out.append('"')
            13 -> out.append("&#13;")
            10 -> if (attribute) out.append("&#10;") else out.append('\n')
            9 -> if (attribute) out.append("&#9;") else out.append('\t')
            else -> out.appendCodePoint(scalar)
        }
    }
    return out.toString()
}
// Detect actual DTD markup while preserving legal comment, CDATA and processing-instruction text.
internal fun srlXmlHasDoctype(text: String): Boolean {
    var cursor = 0
    while (cursor < text.length) {
        val start = text.indexOf('<', cursor)
        if (start < 0) return false
        val closing = when {
            text.startsWith("<!--", start) -> "-->"
            text.startsWith("<![CDATA[", start) -> "]]>"
            text.startsWith("<?", start) -> "?>"
            else -> null
        }
        if (closing != null) {
            val end = text.indexOf(closing, start + 2)
            if (end < 0) return false
            cursor = end + closing.length
        } else {
            if (text.startsWith("<!DOCTYPE", start)) return true
            cursor = start + 1
        }
    }
    return false
}
// Parse exact namespace-aware XML with all external resource features disabled.
internal fun srlXmlParse(bytes: ByteArray, limits: XmlLimits): SrlXmlNode {
    require(bytes.size <= limits.maxBytes) { "XML input limit" }
    val decoder = Charsets.UTF_8.newDecoder().onMalformedInput(java.nio.charset.CodingErrorAction.REPORT)
    val text = decoder.decode(java.nio.ByteBuffer.wrap(bytes)).toString()
    require(!text.contains('\u0000') && !srlXmlHasDoctype(text)) { "Invalid UTF-8 XML or prohibited DTD" }
    val declaration = Regex("^\\uFEFF?<\\?xml\\s[^?]*\\?>").find(text)?.value
    val encoding = declaration?.let { Regex("encoding\\s*=\\s*['\"]([^'\"]+)['\"]").find(it)?.groupValues?.get(1) }
    require(encoding == null || encoding.equals("UTF-8", ignoreCase = true)) { "XML encoding must be UTF-8" }
    val factory = javax.xml.parsers.DocumentBuilderFactory.newInstance()
    factory.isNamespaceAware = true
    factory.isXIncludeAware = false
    factory.isExpandEntityReferences = false
    factory.setFeature("http://apache.org/xml/features/disallow-doctype-decl", true)
    factory.setFeature("http://xml.org/sax/features/external-general-entities", false)
    factory.setFeature("http://xml.org/sax/features/external-parameter-entities", false)
    factory.setAttribute(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, "")
    factory.setAttribute(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, "")
    val builder = factory.newDocumentBuilder()
    builder.setEntityResolver { _, _ -> throw IllegalArgumentException("External XML resource prohibited") }
    builder.setErrorHandler(object : org.xml.sax.ErrorHandler {
        override fun warning(e: org.xml.sax.SAXParseException) { throw e }
        override fun error(e: org.xml.sax.SAXParseException) { throw e }
        override fun fatalError(e: org.xml.sax.SAXParseException) { throw e }
    })
    val document = builder.parse(java.io.ByteArrayInputStream(bytes))
    require(document.xmlVersion == "1.0" && document.doctype == null) { "Only XML 1.0 without DTD is supported" }
    var nodes = 0
    var textBytes = 0L
    fun convert(element: org.w3c.dom.Element, depth: Int): SrlXmlNode {
        require(depth <= limits.maxDepth && ++nodes <= limits.maxNodes) { "XML resource limit" }
        val result = SrlXmlNode(SrlXmlName(element.localName, element.namespaceURI ?: ""))
        val attributes = element.attributes
        for (index in 0 until attributes.length) {
            val item = attributes.item(index)
            require(++nodes <= limits.maxNodes) { "XML attribute/namespace limit" }
            textBytes += srlXmlUtf8Size(item.nodeValue)
            require(textBytes <= limits.maxTextBytes) { "XML text limit" }
            if (item.namespaceURI == "http://www.w3.org/2000/xmlns/") continue
            result.attributes.add(SrlXmlAttribute(SrlXmlName(item.localName, item.namespaceURI ?: ""), item.nodeValue))
        }
        var child = element.firstChild
        while (child != null) {
            when (child.nodeType) {
                org.w3c.dom.Node.ELEMENT_NODE -> result.children.add(convert(child as org.w3c.dom.Element, depth + 1))
                org.w3c.dom.Node.TEXT_NODE, org.w3c.dom.Node.CDATA_SECTION_NODE -> {
                    result.text += child.nodeValue
                    textBytes += srlXmlUtf8Size(child.nodeValue)
                }
                org.w3c.dom.Node.ENTITY_REFERENCE_NODE -> throw IllegalArgumentException("XML entity prohibited")
            }
            require(textBytes <= limits.maxTextBytes) { "XML text limit" }
            child = child.nextSibling
        }
        require(result.children.isEmpty() || result.text.all { it == ' ' || it == '\t' || it == '\r' || it == '\n' }) { "Mixed XML content unsupported" }
        return result
    }
    return convert(document.documentElement, 1)
}
// Consume record attributes and ordered children exactly once.
internal class SrlXmlReader(val node: SrlXmlNode, hasText: Boolean = false) {
    private val used = BooleanArray(node.attributes.size)
    private var cursor = 0
    init { require(hasText || node.text.all { it == ' ' || it == '\t' || it == '\r' || it == '\n' }) { "Unexpected XML text" } }
    // Read simple content without accepting element children.
    fun text(): String { require(node.children.isEmpty()) { "XML simple content contains children" }; return node.text }
    fun attribute(name: SrlXmlName, optional: Boolean = false): String? {
        val index = node.attributes.indexOfFirst { it.name == name }
        if (index < 0) { require(optional) { "Required XML attribute missing" }; return null }
        require(!used[index]) { "Repeated XML attribute" }; used[index] = true
        return node.attributes[index].value
    }
    fun child(name: SrlXmlName, optional: Boolean = false): SrlXmlNode? {
        if (cursor == node.children.size || node.children[cursor].name != name) {
            require(optional) { "Required XML child missing or out of order" }; return null
        }
        return node.children[cursor++]
    }
    fun repeated(name: SrlXmlName): List<SrlXmlNode> {
        val result = mutableListOf<SrlXmlNode>()
)serializer"
R"serializer(        while (cursor < node.children.size && node.children[cursor].name == name) result.add(node.children[cursor++])
        return result
    }
    fun finish() { require(used.all { it } && cursor == node.children.size) { "Unexpected XML member" } }
    // Require exactly one wrapper and exhaust its item contract.
    fun wrapped(wrapper: SrlXmlName, item: SrlXmlName): List<SrlXmlNode> {
        val fields = SrlXmlReader(child(wrapper)!!)
        val result = fields.repeated(item)
        fields.finish()
        return result
    }
}
// Scalar slots reject nested markup and all attributes.
internal fun srlXmlText(node: SrlXmlNode): String {
    require(node.attributes.isEmpty() && node.children.isEmpty()) { "Invalid XML scalar shape" }
    return node.text
}
// Accept exactly the XSD Boolean lexical vocabulary.
internal fun srlXmlBoolean(text: String): Boolean = when (text.trim(' ', '\t', '\r', '\n')) {
    "true", "1" -> true
    "false", "0" -> false
    else -> throw IllegalArgumentException("Invalid XML Boolean")
}
// Enforce the XML floating lexical grammar before native numeric conversion.
internal fun srlXmlFloat(text: String): Double {
    val value = text.trim(' ', '\t', '\r', '\n')
    return when (value) {
        "INF" -> Double.POSITIVE_INFINITY
        "-INF" -> Double.NEGATIVE_INFINITY
        "NaN" -> Double.NaN
        else -> {
            require(Regex("[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?").matches(value)) { "Invalid XML floating value" }
            val number = value.toDouble()
            require(number.isFinite()) { "XML floating overflow" }
            number
        }
    }
}
)serializer"
