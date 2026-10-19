// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"kotlinhtml(
// Bound ordinary approved HTML independently of XML and native wire policy.
data class HtmlLimits(val maxInputBytes: Int = 8388608, val maxOutputBytes: Int = 16777216,
    val maxEvents: Int = 200000, val maxDepth: Int = 128,
    val maxAttributesPerElement: Int = 256, val maxTextBytes: Int = 8388608) {
    init { require(listOf(maxInputBytes,maxOutputBytes,maxEvents,maxDepth,maxAttributesPerElement,maxTextBytes).all { it >= 0 }) { "Invalid HTML limit" } }
}
// Own one lowercase HTML attribute; hidden is represented by Boolean presence.
data class HtmlAttribute(val name: String, val value: String = "", val boolean: Boolean = false)
// Own a semantic HTML trace event with explicit fragment and void-element boundaries.
data class HtmlEvent(val kind: String, val name: String = "", val attributes: List<HtmlAttribute> = emptyList(), val text: String = "")

// Parse and write only explicitly balanced approved HTML; no browser recovery or URL sanitization is implied.
object Html {
    private val tags = "div p span b i strong em a ul ol li section article h1 h2 h3 h4 h5 h6 br hr img small s u code blockquote nav".split(' ').toSet()
    private val voids = setOf("br","hr","img")
    private val phrasing = "span b i strong em a br img small s u code".split(' ').toSet()
    private val entities = mapOf("amp" to "&", "lt" to "<", "gt" to ">", "quot" to "\"", "apos" to "'", "nbsp" to "\u00a0")
    private const val space = " \t\r\n\u000c"
    // Explain the exact contract rather than advertising a general-purpose HTML5 parser.
    fun supportedSubset(): String = "UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; br/hr/img void tags; ASCII case-insensitive names; quoted/unquoted and hidden Boolean attributes; amp/lt/gt/quot/apos/nbsp and numeric references; comments omitted; no doctype, implied closes, raw text, foreign content or browser recovery"
    // Require Unicode scalars and the shared HTML whitespace set before output encoding can substitute anything.
    private fun text(value: String): String {
        var index = 0
        while (index < value.length) {
            val first = value[index++].code
            val scalar = if (first in 0xd800..0xdbff) {
                require(index < value.length && value[index].code in 0xdc00..0xdfff) { "Unsupported HTML Unicode scalar" }
                Character.toCodePoint(first.toChar(), value[index++])
            } else { require(first !in 0xdc00..0xdfff) { "Unsupported HTML Unicode scalar" }; first }
            require(scalar != 0 && scalar !in 0x7f..0x9f && (scalar >= 0x20 || scalar == 9 || scalar == 10 || scalar == 13 || scalar == 12)) { "Unsupported HTML control character" }
        }
        return value
    }
    // Decode approved terminated references and normalize raw source CR/CRLF without changing numeric CR values.
    private fun decode(input: String): String {
        val value = input.replace("\r\n", "\n").replace('\r','\n')
        val result = StringBuilder(); var position = 0
        while (position < value.length) {
            if (value[position] != '&') { result.append(value[position++]); continue }
            val end = value.indexOf(';', position + 1)
            require(end >= 0 && end - position <= 12) { "HTML references require supported terminated spelling" }
            val entity = value.substring(position + 1, end)
            val decoded = entities[entity] ?: run {
                require(entity.startsWith('#')) { "HTML entity outside approved subset" }
                val hex = entity.getOrNull(1) == 'x' || entity.getOrNull(1) == 'X'
                val digits = entity.substring(if (hex) 2 else 1)
                require(digits.isNotEmpty() && digits.all { if (hex) it in '0'..'9' || it in 'a'..'f' || it in 'A'..'F' else it in '0'..'9' }) { "Malformed HTML numeric reference" }
                val scalar = digits.toLong(if (hex) 16 else 10)
                require(scalar <= 0x10ffff && scalar !in 0xd800L..0xdfffL) { "Unsupported HTML character reference" }
                String(Character.toChars(scalar.toInt()))
            }
            result.append(text(decoded)); position = end + 1
        }
        return text(result.toString())
    }
    // Reject relationships that independent HTML parsers would implicitly close or rearrange.
    private fun child(open: List<String>, child: String) {
        require(child in tags) { "HTML tag outside approved subset" }
        val parent = open.lastOrNull()
        require(child != "li" || parent == "ul" || parent == "ol") { "HTML list item requires direct list parent" }
        require(parent == null || parent !in voids && ((parent != "ul" && parent != "ol") || child == "li") &&
            ((parent !in phrasing && parent != "p" && parent !in setOf("h1","h2","h3","h4","h5","h6")) || child in phrasing)) { "HTML child requires unsupported browser recovery" }
        require(child != "a" || "a" !in open) { "Nested HTML anchor outside approved subset" }
    }
    // Validate the complete event trace before returning deterministic ordinary HTML UTF-8 bytes.
    fun writeFragment(events: List<HtmlEvent>, limits: HtmlLimits = HtmlLimits()): ByteArray {
        require(events.size <= limits.maxEvents && events.size >= 2 && events.first().kind == "start_fragment" && events.last().kind == "end_fragment") { "HTML requires bounded explicit fragment boundaries" }
        val result = StringBuilder(); val open = mutableListOf<String>(); var bytes = 0L; var textBytes = 0L
        fun append(value: String) {
            val count = srlXmlUtf8Size(value)
            require(count <= limits.maxOutputBytes.toLong() - bytes) { "HTML output byte limit" }
            bytes += count; result.append(value)
        }
        fun escape(value: String, attribute: Boolean = false) {
            text(value); val batch = StringBuilder()
            for (item in value) {
                batch.append(when (item) { '&' -> "&amp;"; '<' -> "&lt;"; '>' -> "&gt;"; '\r' -> "&#13;"; '"' -> if (attribute) "&quot;" else "\""; else -> item.toString() })
                // Never split a UTF-16 surrogate pair while accounting encoded output bytes.
                if (batch.length >= 4096 && !item.isHighSurrogate()) { append(batch.toString()); batch.setLength(0) }
            }
            if (batch.isNotEmpty()) append(batch.toString())
        }
        for ((index,event) in events.withIndex()) {
            when (event.kind) {
                "start_fragment", "end_fragment" -> require((index == 0 || index == events.lastIndex) && open.isEmpty() && event.name.isEmpty() && event.attributes.isEmpty() && event.text.isEmpty()) { "Malformed HTML fragment boundary" }
                "start_element" -> {
                    child(open,event.name)
                    require(event.text.isEmpty() && open.size < limits.maxDepth && event.attributes.size <= limits.maxAttributesPerElement) { "HTML element payload/depth/attribute limit" }
                    append("<" + event.name); val names = mutableSetOf<String>()
                    for (attribute in event.attributes) {
                        val key = attribute.name
                        val allowed = key in setOf("id","class","title","lang","dir","hidden") || event.name == "a" && key in setOf("href","rel") || event.name == "img" && key in setOf("src","alt","width","height")
                        require(allowed && names.add(key) && attribute.boolean == (key == "hidden") && (!attribute.boolean || attribute.value.isEmpty())) { "Unsupported or duplicate HTML attribute" }
                        text(attribute.value); append(" " + key)
                        if (!attribute.boolean) { append("=\""); escape(attribute.value,true); append("\"") }
                    }
                    append(">"); open.add(event.name)
                }
                "end_element" -> {
                    require(open.lastOrNull() == event.name && event.attributes.isEmpty() && event.text.isEmpty()) { "Unbalanced HTML close" }
                    if (event.name !in voids) append("</" + event.name + ">")
                    open.removeAt(open.lastIndex)
                }
                "text" -> {
                    val count = srlXmlUtf8Size(event.text); val parent = open.lastOrNull()
                    require(event.name.isEmpty() && event.attributes.isEmpty() && parent !in voids && count <= limits.maxTextBytes.toLong() - textBytes &&
                        (parent != "ul" && parent != "ol" || event.text.all { it in space })) { "Invalid HTML text event or cumulative byte limit" }
                    textBytes += count; escape(event.text)
                }
                else -> throw SerializerException("Unknown HTML event kind")
            }
        }
        return result.toString().toByteArray(Charsets.UTF_8)
    }

)kotlinhtml"
R"kotlinhtml(
    // Parse fatal UTF-8 with explicit nesting, then return the event trace only after complete validated EOF.
    fun parseFragment(input: ByteArray, limits: HtmlLimits = HtmlLimits()): List<HtmlEvent> {
        require(input.size <= limits.maxInputBytes) { "HTML input byte limit" }
        val source = try {
            text(Charsets.UTF_8.newDecoder().onMalformedInput(java.nio.charset.CodingErrorAction.REPORT)
                .onUnmappableCharacter(java.nio.charset.CodingErrorAction.REPORT).decode(java.nio.ByteBuffer.wrap(input)).toString())
        } catch (error: java.nio.charset.CharacterCodingException) { throw SerializerException("Invalid HTML UTF-8") }
        val events = mutableListOf<HtmlEvent>(); val open = mutableListOf<String>(); var position = 0; var textBytes = 0L
        fun add(event: HtmlEvent) {
            require(events.size < limits.maxEvents) { "HTML event count limit" }
            if (event.kind == "text") { val count = srlXmlUtf8Size(event.text); require(count <= limits.maxTextBytes.toLong() - textBytes) { "HTML text byte limit" }; textBytes += count }
            events.add(event)
        }
        fun skipSpace(offset: Int): Int { var selected = offset; while (selected < source.length && source[selected] in space) ++selected; return selected }
        fun name(offset: Int): String {
            var end = offset
            while (end < source.length && (source[end] in 'A'..'Z' || source[end] in 'a'..'z' || source[end] in '0'..'9' || source[end] == '-')) ++end
            require(end > offset) { "Unsupported HTML tag/attribute syntax" }; return source.substring(offset,end)
        }
        add(HtmlEvent("start_fragment"))
        while (position < source.length) {
            if (source[position] != '<') {
                val found = source.indexOf('<',position); val end = if (found < 0) source.length else found
                add(HtmlEvent("text",text=decode(source.substring(position,end)))); position = end; continue
            }
            if (source.startsWith("<!--",position)) {
                val end = source.indexOf("-->",position + 4)
                require(end >= 0 && !source.substring(position + 4,end).contains("--")) { "Malformed HTML comment" }; position = end + 3; continue
            }
            ++position; val closing = source.getOrNull(position) == '/'; if (closing) ++position
            val original = name(position); val tag = original.lowercase(java.util.Locale.ROOT); position += original.length
            if (closing) {
                position = skipSpace(position)
                require(source.getOrNull(position) == '>' && tag !in voids && open.lastOrNull() == tag) { "HTML close requires matching explicit nonvoid start" }
                ++position; open.removeAt(open.lastIndex); add(HtmlEvent("end_element",tag)); continue
            }
            child(open,tag); require(open.size < limits.maxDepth) { "HTML nesting limit" }
            val attributes = mutableListOf<HtmlAttribute>(); var selfClosing = false
            while (true) {
                val separator = position; position = skipSpace(position)
                require(position < source.length) { "Truncated HTML start tag" }
                if (source[position] == '>') { ++position; break }
                if (source.startsWith("/>",position)) { selfClosing = true; position += 2; break }
                require(separator != position && attributes.size < limits.maxAttributesPerElement) { "HTML attributes require separation and bounded cardinality" }
                val originalKey = name(position); val key = originalKey.lowercase(java.util.Locale.ROOT); position += originalKey.length
                val keyEnd = position; position = skipSpace(position); val assigned = source.getOrNull(position) == '='; var value = ""
                if (assigned) {
                    position = skipSpace(position + 1); require(position < source.length) { "Truncated HTML attribute" }
                    val quote = if (source[position] == '\"' || source[position] == '\'') source[position++] else null
                    var end = if (quote != null) source.indexOf(quote,position) else position
                    if (quote != null) require(end >= 0) { "Unterminated HTML attribute" }
                    else {
                        while (end < source.length && source[end] !in space && source[end] != '>') {
                            require(source[end] !in "\"'< =`") { "Invalid unquoted HTML attribute" }; ++end
                        }
                        require(end > position) { "Empty unquoted HTML attribute" }
                    }
                    value = decode(source.substring(position,end)); position = end + if (quote != null) 1 else 0
                } else position = keyEnd
                if (key == "hidden") { require(!assigned || value.isEmpty() || value.equals("hidden",ignoreCase=true)) { "HTML hidden requires Boolean spelling" }; value = "" }
                else require(assigned) { "Only approved Boolean HTML attributes may omit a value" }
                attributes.add(HtmlAttribute(key,value,key == "hidden"))
            }
            require(!selfClosing || tag in voids) { "HTML self-closing nonvoid tag requires browser recovery" }
            add(HtmlEvent("start_element",tag,attributes.toList()))
            if (tag in voids) add(HtmlEvent("end_element",tag)) else open.add(tag)
        }
        require(open.isEmpty()) { "HTML fragment has unclosed elements" }; add(HtmlEvent("end_fragment"))
        writeFragment(events,limits); return events
    }
}
)kotlinhtml"
