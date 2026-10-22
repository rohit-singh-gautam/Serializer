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
    private var qnames: Array<SrlXmlName> = emptyArray()
    private var qnameLists: Array<Array<SrlXmlName>> = emptyArray()
    private var unionConstants: Array<UnionValue> = emptyArray()
    private var fixedUnion: UnionValue? = null
    private var listItem: SrlXmlRule? = null
    // Prepare one complete atomic/union item rule without altering builtin list fast paths.
    fun listItemRule(item: SrlXmlRule): SrlXmlRule { listItem=item; return this }
    private var schemaScope: Map<String,String>? = null
    private var schemaDefault: String? = null
    // Prepare trusted schema aliases separately from every occurrence's input vocabulary.
    fun namespaceDefault(text: String, scope: Map<String,String>): SrlXmlRule { schemaDefault=text; schemaScope=scope; return this }
    // Borrow immutable schema bindings only when this declaration supplies its absent/empty default.
    fun inputScope(text: String?, scope: Map<String,String>, attribute: Boolean): Map<String,String> =
        if(text==null || !attribute && text.isEmpty()) schemaScope ?: scope else scope
    // Omit a defaulted owner scalar only when its exact lexical value and selected schema scope agree.
    fun matchesDefault(text: String, scope: Map<String,String>): Boolean =
        text==schemaDefault && schemaScope?.all { (prefix,uri) -> (scope[prefix] ?: "")==uri }==true
    // An authored empty string must not silently acquire a different value on XML replay.
    fun outputScoped(raw: String, namespace: (String)->String?, maximumItems: Int, operation: SrlXmlBudget.Operation?): String {
        require(raw.isNotEmpty() || schemaDefault.isNullOrEmpty()) { "Empty XML union would activate a different schema default" }
        return checkScoped(raw,namespace,maximumItems,operation)
    }
    // Keep the selected primitive and expanded schema identities independent of instance aliases.
    class UnionValue(kind: String, listKind: String, val text: String, val names: Array<SrlXmlName>, val items: Array<UnionValue> = emptyArray()) {
        val rule=SrlXmlRule(kind,"preserve",arrayOf("","","",""),-1,-1,-1,emptyArray(),false,emptyArray(),listKind,emptyArray(),null)
    }
    // Prepare complete immutable enumeration metadata before publishing the generated rule.
    fun unionValues(values: Array<UnionValue>): SrlXmlRule { unionConstants=values; return this }
    // Preserve a fixed union value's schema-owned selected identity.
    fun unionFixed(value: UnionValue): SrlXmlRule { fixedUnion=value; return this }
    // Prepare schema-owned expanded identities before exposing the immutable generated rule.
    fun namespaceValues(names: Array<SrlXmlName>, lists: Array<Array<SrlXmlName>>): SrlXmlRule {
        qnames=names; qnameLists=lists; return this
    }
    // Retain finite work shared by inherited patterns, lists and union alternatives.
    class Work(var remaining: Int = srlXmlMaxPatternWork, val maximumItems: Int = srlXmlMaxListItems,
        private val operation: SrlXmlBudget.Operation? = null) {
        var namespace: ((String) -> String?)? = null
        var selected: SrlXmlRule? = null
        var callbackFailure: kotlin.Throwable? = null
        // Charge before scanning or traversing a prepared instruction.
        fun spend(units: Int) {
            try { operation?.charge("work",units.toLong()) } catch (failure: kotlin.Throwable) { callbackFailure=failure; throw failure }
            if (units < 0 || units > remaining) throw SrlXmlScalarLimit("Portable XML regex work limit exceeded"); remaining -= units
        }
        // Admit temporary expanded-name storage before resolving occurrence-local aliases.
        fun allocate(bytes: Long) {
            try { operation?.charge("allocation",bytes) } catch (failure: kotlin.Throwable) { callbackFailure=failure; throw failure }
        }
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
    fun check(raw: String, maximumItems: Int = srlXmlMaxListItems): String {
        val checked=check(raw,Work(maximumItems=maximumItems)); return if (members.isEmpty()) checked else raw
    }
    // Package-only classification leaves ordinary scalar-limit exception behavior untouched.
    fun check(raw: String, maximumItems: Int, operation: SrlXmlBudget.Operation?): String =
        try { check(raw,maximumItems) } catch (failure: SrlXmlScalarLimit) { operation?.limit(failure.message ?: "XML scalar resource limit"); throw failure }
    // Check aliases against schema identities in the exact input or active output vocabulary.
    fun checkScoped(raw: String, namespace: (String) -> String?, maximumItems: Int, operation: SrlXmlBudget.Operation?): String {
        operation?.charge("allocation",128L+raw.length*4L)
        val work=Work(maximumItems=maximumItems,operation=operation); work.namespace=namespace
        return try { val checked=check(raw,work); if(members.isEmpty()) checked else raw } catch (failure: SrlXmlScalarLimit) { if(work.callbackFailure===failure)throw failure; operation?.limit(failure.message ?: "XML scalar resource limit"); throw failure }
    }
    // Borrow captured bindings without copying the input's admitted namespace map.
    fun checkScoped(raw: String, scope: Map<String,String>, maximumItems: Int, operation: SrlXmlBudget.Operation?): String =
        checkScoped(raw,{ scope[it] },maximumItems,operation)
    // A union's invalid lexical branch may fall through; a depleted operation budget may not.
    private fun check(raw: String, work: Work): String {
        work.spend(raw.length); var text=if(members.isEmpty()) normalize(raw) else raw; if(members.isEmpty()) lexical(text)
        var selected=this
        if (members.isNotEmpty()) {
            var matched=false
            for (member in members) { try { text=member.check(text,work); selected=work.selected!!; matched=true; break } catch (failure: SrlXmlScalarLimit) { throw failure } catch (_: IllegalArgumentException) { work.callbackFailure?.let { throw it } } }
            require(matched) { "XML union has no matching member" }
        }
        var count=text.codePointCount(0,text.length).toLong()
        if (listKind.isNotEmpty()) { val items=tokens(text,work.maximumItems); count=items.size.toLong(); val rule=listItem ?: itemRule(listKind); for (item in items) rule.check(item,work) }
        require((length < 0 || count == length) && (minimum < 0 || count >= minimum) && (maximum < 0 || count <= maximum)) { "XML length facet failed" }
        for (index in bounds.indices) { if (bounds[index].isEmpty()) continue; val order=compare(text,bounds[index]); require(order != null && when(index) { 0->order >= 0; 1->order <= 0; 2->order > 0; else->order < 0 }) { "XML numeric facet failed" } }
        require(enumeration.isEmpty() || qnames.isNotEmpty() || qnameLists.isNotEmpty() || enumeration.indices.any { if(unionConstants.isNotEmpty()) constantEqual(text,selected,unionConstants[it],work) else if(members.isEmpty()) compare(text,normalize(enumeration[it])) == 0 else unionEqual(text,selected,enumeration[it],work) }) { "XML enumeration facet failed" }
        work.namespace?.let { namespace ->
            if (kind == "QName" || listKind == "QName") {
                val items=if (listKind == "QName") tokens(text,work.maximumItems) else listOf(text)
                val names=items.map { work.spend(it.length+1); srlXmlTypeQName(it,namespace) }
                require(qnames.isEmpty() || names.size == 1 && qnames.any { work.spend(it.local.length+it.namespace.length+1); it == names[0] }) { "XML QName enumeration facet failed" }
                require(qnameLists.isEmpty() || qnameLists.any { allowed ->
                    allowed.size == names.size && allowed.indices.all { work.spend(allowed[it].local.length+allowed[it].namespace.length+1); allowed[it] == names[it] }
                }) { "XML QName-list enumeration facet failed" }
            }
        }
        require(fixed == null || fixedUnion?.let { constantEqual(text,selected,it,work) } ?: if(members.isEmpty()) compare(text,normalize(fixed)) == 0 else unionEqual(text,selected,fixed,work)) { "XML fixed value differs" }
        require(!finite || (if (kind == "float") srlXmlFloat32(text).isFinite() else srlXmlFloat(text).isFinite())) { "XML finite facet failed" }
        for (group in groups) require(group.any { it.matches(text,work) }) { "XML portable pattern facet failed" }
        work.selected=selected; return text
    }
    // Preserve primitive family identity across independent authored union alternatives.
    private fun valueFamily(): String = if(listKind.isNotEmpty()) "list:"+itemRule(listKind).valueFamily() else if(integerKind() || kind == "decimal") "decimal" else if(kind in setOf("boolean","float","double","anyURI","QName")) kind else "string"
    // Compare exact normalized values without converting arbitrary precision numbers to binary64.
    private fun identityEqual(first: String, other: SrlXmlRule, second: String, work: Work): Boolean {
        work.spend(first.length+second.length+1); if(valueFamily()!=other.valueFamily()) return false)xmlscalar"
R"xmlscalar(
        if(listKind.isNotEmpty()) { val left=tokens(first,work.maximumItems); val right=tokens(second,work.maximumItems); if(left.size!=right.size)return false; val item=itemRule(listKind);val otherItem=itemRule(other.listKind);return left.indices.all { item.identityEqual(left[it],otherItem,right[it],work) } }
        return if(valueFamily()=="decimal") java.math.BigDecimal(first).compareTo(java.math.BigDecimal(second))==0 else compare(first,second)==0
    }
    // Select each schema literal with the same first-successful-member rule as the instance.
    private fun unionEqual(text: String, selected: SrlXmlRule, allowed: String, work: Work): Boolean {
        for(member in members) { val expected=try { member.check(allowed,work) } catch(failure: SrlXmlScalarLimit) { throw failure } catch(_: IllegalArgumentException) { work.callbackFailure?.let { throw it };continue }; return selected.identityEqual(text,work.selected!!,expected,work) };return false
    }
    // Match the selected family and ordered schema names before accepting equivalent source aliases.
    private fun constantEqual(text: String, selected: SrlXmlRule, expected: UnionValue, work: Work): Boolean {
        if(selected.listKind.isEmpty()!=expected.rule.listKind.isEmpty())return false
        if(selected.listKind.isNotEmpty() && (expected.items.isNotEmpty() || expected.text.isEmpty())) {
            val values=tokens(text,work.maximumItems);if(values.size!=expected.items.size)return false
            val item=selected.listItem ?: itemRule(selected.listKind)
            return values.indices.all { val value=item.check(values[it],work);constantEqual(value,work.selected!!,expected.items[it],work) }
        }
        if(selected.valueFamily()!=expected.rule.valueFamily())return false
        if(selected.kind!="QName" && selected.listKind!="QName")return selected.identityEqual(text,expected.rule,expected.text,work)
        val namespace=work.namespace ?: throw IllegalArgumentException("Missing XML union namespace scope")
        val items=if(selected.listKind=="QName") tokens(text,work.maximumItems) else listOf(text)
        work.allocate(items.size*64L+text.length*4L)
        if(items.size!=expected.names.size)return false
        return items.indices.all { val name=srlXmlTypeQName(items[it],namespace);work.spend(name.local.length+name.namespace.length+1);name==expected.names[it] }
    }
    // Preserve normalized empty payloads when an empty element would activate a nonempty default.
    fun output(raw: String, fallback: String, maximumItems: Int = srlXmlMaxListItems): String {
        val text=check(raw,maximumItems)
        if (text.isEmpty() && normalize(fallback).isNotEmpty()) { require(whitespace == "collapse") { "Empty XML value would activate a different XSD default" }; return " " }
        return text
    }
    // Preserve located failures before generated member-owner scopes are restored.
    fun output(raw: String, fallback: String, maximumItems: Int, operation: SrlXmlBudget.Operation?): String =
        try { output(raw,fallback,maximumItems) } catch (failure: SrlXmlScalarLimit) { operation?.limit(failure.message ?: "XML scalar resource limit"); throw failure }
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
        // Distinguish a configured item quota from an ordinary fixed-extent mismatch.
        fun listText(items: List<String>, extent: Int, maximum: Int, operation: SrlXmlBudget.Operation?): String {
            if(items.size>maximum) SrlXmlBudget.limit("XML lexical list size",operation)
            return listText(items,extent,maximum)
        }
        // Build unrestricted builtin list-item metadata without mutable global counters.
        private fun itemRule(kind: String) = SrlXmlRule(kind,"collapse",arrayOf("","","",""),-1,-1,-1,emptyArray(),false,emptyArray(),"",emptyArray(),null)
        // Count list items before allocating any item strings or token arrays.
        fun tokens(raw: String, maximum: Int = srlXmlMaxListItems): List<String> {
            var count=0; var item=false
            for (character in raw) { val space=character in " \t\r\n"; if (!space && !item) { if (count >= maximum) throw SrlXmlScalarLimit("XML list item limit exceeded"); ++count }; item = !space }
            return if (count == 0) emptyList() else raw.trim(' ','\t','\r','\n').split(Regex("[ \\t\\r\\n]+"))
        }
        // Keep the existing ordinary exception while attaching package resource context when present.
        fun tokens(raw: String, maximum: Int, operation: SrlXmlBudget.Operation?): List<String> =
            try { tokens(raw,maximum) } catch (failure: SrlXmlScalarLimit) { operation?.limit(failure.message ?: "XML list resource limit"); throw failure }
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
// Locate only quota failures; fixed extent and item conversion failures retain ordinary semantics.
internal fun <T> srlXmlReadList(text: String, extent: Int, maximum: Int, operation: SrlXmlBudget.Operation?, decode: (String) -> T): MutableList<T> {
    val items=SrlXmlRule.tokens(text,maximum,operation)
    require(extent == 0 || items.size == extent) { "XML lexical list fixed extent" }
    return items.mapTo(mutableListOf(),decode)
}
)xmlscalar"
