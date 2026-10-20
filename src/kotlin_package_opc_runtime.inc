// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagekotlin(
    private const val CT = "http://schemas.openxmlformats.org/package/2006/content-types"
    private const val RELS = "http://schemas.openxmlformats.org/package/2006/relationships"
    private const val RELS_MIME = "application/vnd.openxmlformats-package.relationships+xml"
    private const val MC = "http://schemas.openxmlformats.org/markup-compatibility/2006"
    private const val XML = "http://www.w3.org/XML/1998/namespace"
    /** Whole opaque bytes remain uninterpreted; loaded arrays are fresh owning values. */
    data class BinaryPart(val data: ByteArray, val name: String, val contentType: String, val sourceIndex: Int = 0, val compression: String = "stored") {
        internal var contextField = ""; internal var contextId = ""
    }
    /** Dynamic typed occurrences retain actual logical names and parent occurrence indexes. */
    data class ModelPart<T>(val value: T, val name: String, val contentType: String, val sourceIndex: Int = 0)
    /** Targets are inert text; external links are never fetched or normalized. */
    data class Relationship(val source: String, val id: String, val type: String, val target: String, val mode: String = "internal")
    /** Whole-part preservation keeps namespace declarations and relationship context intact. */
    class Extensions(parts: List<BinaryPart> = emptyList(), relationships: List<Relationship> = emptyList()) {
        val parts = parts.toList(); val relationships = relationships.toList()
    }
    enum class Unknown { REJECT, SKIP, PRESERVE }
    /** Required capabilities override independent optional part/link policy. */
    data class ExtensionPolicy(val parts: Unknown = Unknown.REJECT, val relationships: Unknown = Unknown.REJECT,
        val requiredPart: ((PartMetadata) -> Boolean)? = null, val requiredRelationship: ((Relationship) -> Boolean)? = null)
    /** Results publish only after complete aggregate validation. */
    data class Result<T>(val value: T, val extensions: Extensions)
    /** Checked logical metadata retains the physical entry privately. */
    class PartMetadata internal constructor(val name: String, val contentType: String, internal val entry: Entry)
    /** ASCII folding preserves Unicode external IRI spelling. */
    private fun fold(value: String): String = value.map { if (it in 'A'..'Z') it+32 else it }.joinToString("")
    /** Recognize one ASCII hexadecimal digit. */
    private fun hex(value: Char): Int = when (value) { in '0'..'9' -> value-'0'; in 'A'..'F' -> value-'A'+10; in 'a'..'f' -> value-'a'+10; else -> -1 }
    /** Recognize unreserved ASCII URI bytes. */
    private fun unreserved(value: Int): Boolean = value in 65..90 || value in 97..122 || value in 48..57 || value in "-._~".map { it.code }
    /** Identify reserved relationship infrastructure case-insensitively. */
    private fun relationshipPath(name: String): String? {
        val key = fold(name)
        if (key == "/_rels/.rels") return "/"
        val basename = key.lastIndexOf('/'); val separator = key.lastIndexOf('/',basename-1)
        if (separator < 0 || key.substring(separator+1,basename) != "_rels" || !key.endsWith(".rels")) return null
        val file = name.substring(basename+1,name.length-5)
        if (file.isEmpty()) throw PackageError(Code.INVALID_METADATA,"Invalid relationship part name",name)
        return name.substring(0,separator+1)+file
    }
    /** Reject relationship-of-relationship sources without recursive name validation or repeated suffix copies. */
    private fun relationshipSource(name: String): String? {
        val source = relationshipPath(name) ?: return null
        if (source == "/") return source
        partName(source,true)
        if (fold(source) == "/[content_types].xml" || relationshipPath(source) != null) throw PackageError(Code.INVALID_NAME,"Reserved relationship-part source",name)
        return source
    }
    /** Validate selected ASCII OPC part URIs and disallow reserved infrastructure for payloads. */
    private fun partName(name: String, infrastructure: Boolean = false): String {
        if (!name.startsWith('/') || name.length < 2 || name.length-1 > 65535 || '?' in name || '#' in name) throw PackageError(Code.INVALID_NAME,"Invalid OPC part URI",name)
        if (name == "/[Content_Types].xml" && infrastructure) return name
        for (piece in name.substring(1).split('/')) {
            if (piece.isEmpty() || piece.endsWith('.')) throw PackageError(Code.INVALID_NAME,"Invalid OPC URI segment",name)
            var position = 0
            while (position < piece.length) {
                var code = piece[position++].code
                if (code == 37) {
                    if (piece.length-position < 2 || hex(piece[position]) < 0 || hex(piece[position+1]) < 0) throw PackageError(Code.INVALID_NAME,"Malformed OPC URI escape",name)
                    code = hex(piece[position])*16+hex(piece[position+1]); position += 2
                    if (code >= 128 || unreserved(code) || code == 47 || code == 92) throw PackageError(Code.INVALID_NAME,"Forbidden OPC URI escape",name)
                } else if (code >= 128 || !unreserved(code) && code !in "!$&'()*+,;=:@".map { it.code }) throw PackageError(Code.INVALID_NAME,"Invalid OPC URI character",name)
            }
        }
        if (!infrastructure && (fold(name) == "/[content_types].xml" || relationshipSource(name) != null)) throw PackageError(Code.INVALID_NAME,"Reserved OPC infrastructure URI",name)
        return name
    }
    /** Invalid physical item names remain outside OPC; valid unsupported Unicode URIs fail closed. */
    private fun mapsToPart(name: String, state: State): Boolean {
        var unsupported = false; val probe = kotlin.text.StringBuilder(); var position = 0
        while (position < name.length) {
            val value = name[position++]
            if (value.code >= 128) { unsupported = true; probe.append('_') }
            else if (value == '%' && name.length-position >= 2 && hex(name[position]) >= 8 && hex(name[position+1]) >= 0) { unsupported = true; probe.append("%20"); position += 2 }
            else probe.append(value)
        }
        state.consume(Resource.WORK_UNITS,name.length.toLong(),name)
        try { partName(probe.toString(),true) } catch (error: PackageError) { if (error.code == Code.INVALID_NAME) return false; throw error }
        if (unsupported) throw PackageError(Code.UNSUPPORTED_FEATURE,"Non-ASCII OPC part URI unsupported",name)
        return true
    }
    /** Reject case aliases and file/descendant collisions before retaining an identity. */
    private fun unique(names: MutableSet<String>, name: String): String {
        val key = fold(name)
        if (key in names) throw PackageError(Code.DUPLICATE_NAME,"Duplicate OPC part identity",name)
        if (names.any { key.startsWith("$it/") || it.startsWith("$key/") }) throw PackageError(Code.INVALID_NAME,"Conflicting OPC part ancestry",name)
        names.add(key); return key
    }
    /** Parsed media identity compares parameter names without folding parameter values. */
    private data class Mime(val media: String, val parameters: Map<String,String>)
    /** MIME token parsing uses ASCII syntax only. */
    private fun mimeToken(value: Char): Boolean = value in 'A'..'Z' || value in 'a'..'z' || value in '0'..'9' || value in "!#$%&'*+-.^_`|~"
    /** Validate complete MIME values, including quoted semicolons and reserved media restrictions. */
    private fun mime(value: String): Mime {
        var position = 0
        while (position < value.length && mimeToken(value[position])) position++
        if (position == 0 || position == value.length || value[position++] != '/') throw PackageError(Code.INVALID_METADATA,"Invalid OPC media type")
        val start = position
        while (position < value.length && mimeToken(value[position])) position++
        if (position == start) throw PackageError(Code.INVALID_METADATA,"Invalid OPC media subtype")
        val media = fold(value.substring(0,position)); val parameters = sortedMapOf<String,String>()
        while (position < value.length) {
            while (position < value.length && value[position] in " \t") position++
            if (position == value.length || value[position++] != ';') throw PackageError(Code.INVALID_METADATA,"Invalid MIME parameter delimiter")
            while (position < value.length && value[position] in " \t") position++
            val begin = position
            while (position < value.length && mimeToken(value[position])) position++
            if (position == begin || position == value.length || value[position++] != '=') throw PackageError(Code.INVALID_METADATA,"Invalid MIME parameter")
            val name = fold(value.substring(begin,position-1)); val text = kotlin.text.StringBuilder()
            if (position < value.length && value[position] == '"') {
                position++; var ended = false
                while (position < value.length) {
                    var character = value[position++]
                    if (character == '"') { ended = true; break }
                    if (character == '\\') { if (position == value.length) break; character = value[position++] }
                    if (character.code !in 32..126 && character != '\t') throw PackageError(Code.INVALID_METADATA,"Invalid MIME quoted value")
                    text.append(character)
                }
                if (!ended) throw PackageError(Code.INVALID_METADATA,"Unterminated MIME parameter")
            } else {
                val first = position
                while (position < value.length && mimeToken(value[position])) text.append(value[position++])
                if (first == position) throw PackageError(Code.INVALID_METADATA,"Empty MIME parameter")
            }
            if (parameters.put(name,text.toString()) != null) throw PackageError(Code.INVALID_METADATA,"Duplicate MIME parameter")
        }
        if (parameters.isNotEmpty() && media in listOf(RELS_MIME,"application/vnd.openxmlformats-package.core-properties+xml")) throw PackageError(Code.INVALID_METADATA,"Reserved OPC media has parameters")
        return Mime(media,parameters)
    }
    /** Parse inert URI components using literal ASCII delimiters, never network resolution. */
    private data class Uri(val scheme: String?, val authority: String?, val path: String, val query: String?, val fragment: String?)
    /** Validate one exact IRI component without normalization. */
    private fun iri(value: String?, extra: String, query: Boolean = false) {
        if (value == null) return
        var position = 0
        while (position < value.length) {
            val code = value.codePointAt(position); position += java.lang.Character.charCount(code)
            if (code == 37) { if (value.length-position < 2 || hex(value[position]) < 0 || hex(value[position+1]) < 0) throw PackageError(Code.INVALID_RELATIONSHIP,"Malformed IRI escape"); position += 2; continue }
            val ordinary = code in 0xa0..0xd7ff || code in 0xf900..0xfdcf || code in 0xfdf0..0xffef || code in 0x10000..0xdfffd && code and 65535 <= 0xfffd || code in 0xe1000..0xefffd
            val privateUse = query && (code in 0xe000..0xf8ff || code in 0xf0000..0xffffd || code in 0x100000..0x10fffd)
            if (!ordinary && !privateUse && !(code < 128 && (unreserved(code) || code.toChar() in "!$&'()*+,;=" || code.toChar() in extra))) throw PackageError(Code.INVALID_RELATIONSHIP,"Invalid IRI character")
        }
    }
    /** Validate complete IPv6 spelling without DNS or zone-ID interpretation. */
    private fun ipv6(value: String): Boolean {
        if ('%' in value) return false
        /** Count explicit groups, admitting embedded IPv4 only on the global final side. */
        fun groups(text: String, final: Boolean): Int {
            if (text.isEmpty()) return 0
            var count = 0; val pieces = text.split(':')
            for ((index,piece) in pieces.withIndex()) {
                if (piece.isEmpty()) return -1
                if ('.' in piece) {
                    val quads = piece.split('.')
                    if (!final || index != pieces.lastIndex || quads.size != 4 || quads.any { !it.matches(kotlin.text.Regex("0|[1-9][0-9]{0,2}")) || it.toInt() > 255 }) return -1
                    count += 2
                } else { if (!piece.matches(kotlin.text.Regex("[0-9a-fA-F]{1,4}"))) return -1; count++ }
            }
            return count
        }
        val at = value.indexOf("::")
        if (at < 0) return groups(value,true) == 8
        if (value.indexOf("::",at+2) >= 0) return false
        val left = groups(value.substring(0,at),false); val right = groups(value.substring(at+2),true)
)packagekotlin"
R"packagekotlin(        return left >= 0 && right >= 0 && left+right < 8
    }
    /** Relationship type requires an absolute fragment-free IRI; external targets remain inert. */
    private fun uri(original: String, absolute: Boolean = false): Uri {
        var value = original; var fragment: String? = null; var query: String? = null; var scheme: String? = null; var authority: String? = null
        var at = value.indexOf('#'); if (at >= 0) { fragment = value.substring(at+1); value = value.substring(0,at) }
        at = value.indexOf('?'); if (at >= 0) { query = value.substring(at+1); value = value.substring(0,at) }
        val colon = value.indexOf(':'); val slash = value.indexOf('/')
        if (colon >= 0 && (slash < 0 || colon < slash)) { scheme = value.substring(0,colon); value = value.substring(colon+1); if (!scheme.matches(kotlin.text.Regex("[A-Za-z][A-Za-z0-9+.-]*"))) throw PackageError(Code.INVALID_RELATIONSHIP,"Invalid URI scheme") }
        if (absolute && (scheme == null || fragment != null)) throw PackageError(Code.INVALID_RELATIONSHIP,"Relationship type requires absolute IRI")
        if (value.startsWith("//")) { at = value.indexOf('/',2); authority = value.substring(2,if (at < 0) value.length else at); value = if (at < 0) "" else value.substring(at) }
        if (authority != null) {
            var host = authority; at = host.indexOf('@'); if (at >= 0) { iri(host.substring(0,at),":"); host = host.substring(at+1) }
            if (host.startsWith('[')) {
                val end = host.indexOf(']'); if (end < 0) throw PackageError(Code.INVALID_RELATIONSHIP,"Unterminated IP authority")
                val literal = host.substring(1,end)
                if (!(ipv6(literal) || literal.matches(kotlin.text.Regex("[vV][0-9a-fA-F]+\\.[A-Za-z0-9._~!$&'()*+,;=:-]+"))) || !host.substring(end+1).matches(kotlin.text.Regex("(?::[0-9]*)?"))) throw PackageError(Code.INVALID_RELATIONSHIP,"Invalid IP authority")
            } else { at = host.indexOf(':'); iri(if (at < 0) host else host.substring(0,at),""); if (at >= 0 && !host.substring(at+1).matches(kotlin.text.Regex("[0-9]*"))) throw PackageError(Code.INVALID_RELATIONSHIP,"Invalid URI port") }
        }
        iri(value,":@/"); iri(query,":@/?",true); iri(fragment,":@/?")
        return Uri(scheme,authority,value,query,fragment)
    }
    /** Resolve source-relative path segments while retaining target query/fragment text separately. */
    private fun target(source: String, text: String): String {
        val value = uri(text)
        if (value.scheme != null || value.authority != null) throw PackageError(Code.INVALID_RELATIONSHIP,"Internal target has scheme or authority",source)
        if (value.path.isEmpty()) return partName(source)
        if (value.path.endsWith('/') || value.path.substringAfterLast('/') in listOf(".","..")) throw PackageError(Code.INVALID_RELATIONSHIP,"Relationship targets a directory",source)
        val pieces = mutableListOf<String>()
        if (!value.path.startsWith('/')) pieces.addAll(source.substring(1,source.lastIndexOf('/')+1).split('/').filter { it.isNotEmpty() })
        for (piece in value.path.split('/')) when (piece) { "", "." -> {}; ".." -> { if (pieces.isEmpty()) throw PackageError(Code.INVALID_RELATIONSHIP,"Relationship target escapes package root",source); pieces.removeAt(pieces.lastIndex) }; else -> pieces.add(piece) }
        return partName("/"+pieces.joinToString("/"))
    }
    /** XML metadata tokens use only the four XML whitespace characters. */
    private fun collapse(value: String): String = value.replace(kotlin.text.Regex("[ \\t\\r\\n]+")," ").trim(' ')
    /** NCName validates XML1.0 Unicode ranges independently of locale. */
    private fun ncname(value: String): Boolean {
        /** Apply XML1.0's start-character ranges to a decoded scalar. */
        fun start(code: Int): Boolean = code == 95 || code in 65..90 || code in 97..122 || code in 0xc0..0xd6 || code in 0xd8..0xf6 || code in 0xf8..0x2ff || code in 0x370..0x37d || code in 0x37f..0x1fff || code in 0x200c..0x200d || code in 0x2070..0x218f || code in 0x2c00..0x2fef || code in 0x3001..0xd7ff || code in 0xf900..0xfdcf || code in 0xfdf0..0xfffd || code in 0x10000..0xeffff
        if (value.isEmpty()) return false
        var position = 0
        while (position < value.length) { val code = value.codePointAt(position); if (!start(code) && !(position > 0 && (code in 48..57 || code == 45 || code == 46 || code == 0xb7 || code in 0x300..0x36f || code in 0x203f..0x2040))) return false; position += java.lang.Character.charCount(code) }
        return true
    }
)packagekotlin"
