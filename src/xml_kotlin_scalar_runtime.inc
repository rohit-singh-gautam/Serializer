// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlscalar(
// Preserve resource failures across union alternatives instead of selecting another branch.
internal class SrlXmlScalarLimit(message: String) : IllegalArgumentException(message)
// Execute immutable compiler-lowered scalar restrictions independently of host regex syntax.
internal class SrlXmlRule(private val kind: String, private val whitespace: String,
    private val bounds: Array<String>, private val length: Long, private val minimum: Long,
    private val maximum: Long, private val enumeration: Array<String>, private val finite: Boolean,
    private val groups: Array<Array<Pattern>>, private val listKind: String,
    private val members: Array<SrlXmlRule>, private val fixed: String?) {
    // Retain finite work shared by inherited patterns, lists and union alternatives.
    class Work(var remaining: Int = srlXmlMaxPatternWork, val maximumItems: Int = srlXmlMaxListItems) {
        // Charge before scanning or traversing a prepared instruction.
        fun spend(units: Int) { if (units < 0 || units > remaining) throw SrlXmlScalarLimit("Portable XML regex work limit exceeded"); remaining -= units }
    }
    // Normalize only XML whitespace; preserve all other Unicode separators byte-for-byte.
    private fun normalize(raw: String): String {
        if (whitespace != "replace" && whitespace != "collapse") return raw
        val text=raw.replace('\t',' ').replace('\r',' ').replace('\n',' ')
        if (whitespace != "collapse") return text
        val result=StringBuilder(); var pending=false
        for (item in text) { if (item == ' ') { pending=result.isNotEmpty(); continue }; if (pending) result.append(' '); result.append(item); pending=false }
        return result.toString()
    }
    // Distinguish exact bounded and unbounded XML integer families.
    private fun integerKind(): Boolean = when(kind) {
        "byte","unsignedByte","short","unsignedShort","int","unsignedInt","long","unsignedLong","integer","positiveInteger","negativeInteger","nonPositiveInteger","nonNegativeInteger" -> true
        else -> false
    }
    // Validate lexical and builtin value spaces before comparing facets or enumerations.
    private fun lexical(text: String) {
        if (integerKind()) {
            require(integerLexeme.matches(text)) { "Invalid XML integer" }
            val value=java.math.BigInteger(text); val sign=value.signum()
            val width=when(kind) { "byte","unsignedByte"->8; "short","unsignedShort"->16; "int","unsignedInt"->32; "long","unsignedLong"->64; else->0 }
            if (width != 0) {
                val unsigned=kind.startsWith("unsigned"); val span=java.math.BigInteger.ONE.shiftLeft(if (unsigned) width else width-1)
                require(value >= (if (unsigned) java.math.BigInteger.ZERO else span.negate()) && value < span) { "Overflowing XML integer" }
            }
            require(!(kind == "positiveInteger" && sign <= 0 || kind == "negativeInteger" && sign >= 0 || kind == "nonPositiveInteger" && sign > 0 || kind == "nonNegativeInteger" && sign < 0)) { "XML integer sign failed" }
        } else when(kind) {
            "decimal" -> require(decimalLexeme.matches(text)) { "Invalid XML decimal" }
            "boolean" -> { srlXmlBoolean(text) }
            "float" -> { srlXmlFloat32(text) }
            "double" -> { srlXmlFloat(text) }
            "language" -> require(languageLexeme.matches(text)) { "Invalid XML language" }
            "Name","NCName","NMTOKEN","ID","IDREF","ENTITY" -> {
                require(text.isNotEmpty()) { "Empty XML name token" }; var offset=0
                while (offset < text.length) {
                    val code=Character.codePointAt(text,offset); val first=offset == 0 && kind != "NMTOKEN"
                    val colon=(kind == "Name" || kind == "NMTOKEN") && code == 58
                    require(srlXmlNameStart(code) || colon || !first && (code == 45 || code == 46 || code == 0xb7 || code in 48..57 || code in 0x300..0x36f || code in 0x203f..0x2040)) { "Invalid XML name token" }
                    offset += Character.charCount(code)
                }
            }
        }
    }
    // Compare exact decimal/integer/list spaces; NaN is unordered except equal NaN enumeration.
    private fun compare(left: String, right: String): Int? {
        if (listKind.isNotEmpty()) {
            val first=tokens(left,Int.MAX_VALUE); val second=tokens(right,Int.MAX_VALUE)
            if (first.size != second.size) return 1
            val item=itemRule(listKind)
            return if (first.indices.all { item.compare(first[it],second[it]) == 0 }) 0 else 1
        }
        if (kind == "float" || kind == "double") {
            val first=if (kind == "float") srlXmlFloat32(left).toDouble() else srlXmlFloat(left)
            val second=if (kind == "float") srlXmlFloat32(right).toDouble() else srlXmlFloat(right)
            if (first.isNaN() || second.isNaN()) return if (first.isNaN() && second.isNaN()) 0 else null
            return if (first < second) -1 else if (first > second) 1 else 0
        }
        if (integerKind() || kind == "decimal") return java.math.BigDecimal(left).compareTo(java.math.BigDecimal(right))
        if (kind == "boolean") return if (srlXmlBoolean(left) == srlXmlBoolean(right)) 0 else 1
        return left.compareTo(right)
    }
    // Validate the complete normalized candidate before native conversion or output publication.
    fun check(raw: String, maximumItems: Int = srlXmlMaxListItems): String = check(raw,Work(maximumItems=maximumItems))
    // A union's invalid lexical branch may fall through; a depleted operation budget may not.
    private fun check(raw: String, work: Work): String {
        work.spend(raw.length); val text=normalize(raw); lexical(text)
        if (members.isNotEmpty()) {
            var matched=false
            for (member in members) { try { member.check(text,work); matched=true; break } catch (failure: SrlXmlScalarLimit) { throw failure } catch (_: IllegalArgumentException) { } }
            require(matched) { "XML union has no matching member" }
        }
        var count=text.codePointCount(0,text.length).toLong()
        if (listKind.isNotEmpty()) { val items=tokens(text,work.maximumItems); count=items.size.toLong(); val rule=itemRule(listKind); for (item in items) rule.check(item,work) }
        require((length < 0 || count == length) && (minimum < 0 || count >= minimum) && (maximum < 0 || count <= maximum)) { "XML length facet failed" }
        for (index in bounds.indices) { if (bounds[index].isEmpty()) continue; val order=compare(text,bounds[index]); require(order != null && when(index) { 0->order >= 0; 1->order <= 0; 2->order > 0; else->order < 0 }) { "XML numeric facet failed" } }
        require(enumeration.isEmpty() || enumeration.any { compare(text,normalize(it)) == 0 }) { "XML enumeration facet failed" }
        require(fixed == null || compare(text,normalize(fixed)) == 0) { "XML fixed value differs" }
        require(!finite || (if (kind == "float") srlXmlFloat32(text).isFinite() else srlXmlFloat(text).isFinite())) { "XML finite facet failed" }
        for (group in groups) require(group.any { it.matches(text,work) }) { "XML portable pattern facet failed" }
        return text
    }
    // Preserve normalized empty payloads when an empty element would activate a nonempty default.
    fun output(raw: String, fallback: String, maximumItems: Int = srlXmlMaxListItems): String {
        val text=check(raw,maximumItems)
        if (text.isEmpty() && normalize(fallback).isNotEmpty()) { require(whitespace == "collapse") { "Empty XML value would activate a different XSD default" }; return " " }
        return text
    }
    companion object {
        private val integerLexeme=Regex("[+-]?[0-9]+")
        private val decimalLexeme=Regex("[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)")
        private val languageLexeme=Regex("[A-Za-z]{1,8}(?:-[A-Za-z0-9]{1,8})*")
        // Apply element defaults to empty content while preserving present empty attributes.
        fun defaultText(raw: String?, fallback: String, attribute: Boolean): String = if (raw == null || !attribute && raw.isEmpty()) fallback else raw
        // Preserve each native item identity before joining a lexical list with one XML space.
        fun listText(items: List<String>, extent: Int, maximum: Int): String {
            require(items.size <= maximum && (extent == 0 || items.size == extent)) { "XML lexical list size" }
            require(items.all { it.isNotEmpty() && it.none { character -> character in " \t\r\n" } }) { "Unrepresentable XML list item" }
            return items.joinToString(" ")
        }
        // Build unrestricted builtin list-item metadata without mutable global counters.
        private fun itemRule(kind: String) = SrlXmlRule(kind,"collapse",arrayOf("","","",""),-1,-1,-1,emptyArray(),false,emptyArray(),"",emptyArray(),null)
        // Count list items before allocating any item strings or token arrays.
        fun tokens(raw: String, maximum: Int = srlXmlMaxListItems): List<String> {
            var count=0; var item=false
            for (character in raw) { val space=character in " \t\r\n"; if (!space && !item) { if (count >= maximum) throw SrlXmlScalarLimit("XML list item limit exceeded"); ++count }; item = !space }
            return if (count == 0) emptyList() else raw.trim(' ','\t','\r','\n').split(Regex("[ \\t\\r\\n]+"))
        }
    }
)xmlscalar"
R"xmlscalar(
    // Run only compiler-lowered Thompson instructions over Unicode scalars with bounded work.
    class Pattern private constructor(private val start: Int, private val states: List<State>) {
        private data class State(val epsilon: IntArray, val ranges: IntArray, val next: Int, val flags: Int)
        // Deduplicate on insertion so nullable loops fit one fixed array of pending states.
        private fun closure(selected: IntArray, count: Int, visited: BooleanArray, stack: IntArray, work: Work): Int {
            work.spend(visited.size); visited.fill(false); var pending=0; var kept=0
            for (index in 0 until count) { val target=selected[index]; work.spend(1); if (!visited[target]) { visited[target]=true; stack[pending++]=target } }
            while (pending != 0) {
                work.spend(1); val current=stack[--pending]; val state=states[current]
                if (state.flags and 9 != 0) selected[kept++]=current
                for (target in state.epsilon) { work.spend(1); if (!visited[target]) { visited[target]=true; stack[pending++]=target } }
            }
            return kept
        }
        // Match complete scalar sequences without host-regex semantics or backtracking.
        fun matches(text: String, work: Work): Boolean {
            if (srlXmlUtf8Size(text) > srlXmlMaxPatternBytes) throw SrlXmlScalarLimit("Portable XML regex input limit exceeded")
            val visited=BooleanArray(states.size); var active=IntArray(states.size); var next=IntArray(states.size); val stack=IntArray(states.size)
            active[0]=start; var count=closure(active,1,visited,stack,work); var offset=0
            while (offset < text.length) {
                work.spend(1); val code=Character.codePointAt(text,offset); offset += Character.charCount(code); var following=0
                for (index in 0 until count) {
                    work.spend(1); val state=states[active[index]]; if (state.flags and 1 == 0) continue
                    var matched=false
                    if (state.flags and 4 != 0) matched=code != 10 && code != 13
                    else { for (range in state.ranges.indices step 2) { work.spend(1); matched = matched || code in state.ranges[range]..state.ranges[range+1] }; if (state.flags and 2 != 0) matched = !matched }
                    if (matched) next[following++]=state.next
                }
                count=closure(next,following,visited,stack,work); val previous=active; active=next; next=previous
            }
            return (0 until count).any { states[active[it]].flags and 8 != 0 }
        }
        companion object {
            // Load checked compiler-owned instructions once, avoiding JVM literal size limits.
            fun prepare(fragments: Array<String>): Pattern {
                val records=fragments.joinToString("").split(';'); val count=records.size-1
                require(count in 1..srlXmlMaxPatternStates) { "Portable XML regex state limit exceeded" }
                val start=records[0].toInt(); require(start in 0 until count) { "Invalid portable XML regex start" }
                // Decode a bounded comma-separated instruction array without retaining source fragments.
                fun integers(text: String): IntArray = if (text.isEmpty()) IntArray(0) else text.split(',').map { it.toInt() }.toIntArray()
                val states=(1..count).map { index ->
                    val fields=records[index].split('|'); require(fields.size == 4) { "Invalid portable XML regex instruction" }
                    val epsilon=integers(fields[0]); val ranges=integers(fields[1]); val next=fields[2].toInt(); val flags=fields[3].toInt()
                    require(ranges.size % 2 == 0 && next in 0 until count && flags in 0..15 && epsilon.all { it in 0 until count }) { "Invalid portable XML regex instruction" }
                    for (range in ranges.indices step 2) require(ranges[range] >= 0 && ranges[range] <= ranges[range+1] && ranges[range+1] <= 0x10ffff) { "Invalid portable XML regex range" }
                    State(epsilon,ranges,next,flags)
                }
                return Pattern(start,states)
            }
        }
    }
}
// Convert one fully validated lexical list into a private native candidate with exact extent.
internal fun <T> srlXmlReadList(text: String, extent: Int, maximum: Int, decode: (String) -> T): MutableList<T> {
    val items=SrlXmlRule.tokens(text,maximum)
    require(extent == 0 || items.size == extent) { "XML lexical list fixed extent" }
    return items.mapTo(mutableListOf(),decode)
}
)xmlscalar"
