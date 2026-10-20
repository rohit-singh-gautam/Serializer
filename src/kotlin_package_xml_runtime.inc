// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagekotlin(
    /** Expanded metadata names preserve original source namespace scopes. */
    private data class Name(val uri: String, val local: String)
    private class Node(val name: Name, val namespaces: Map<String,String>) {
        val attributes = linkedMapOf<Name,String>(); var children = mutableListOf<Node>(); var text = kotlin.text.StringBuilder()
    }
    /** Parse only explicitly supported UTF-8/UTF-16 transport encodings with bounded SAX events. */
    private fun metadata(bytes: ByteArray, part: String, state: State): Node {
        state.consume(Resource.METADATA_BYTES,bytes.size.toLong(),part)
        val bom16 = bytes.size >= 2 && (bytes[0] == 0xff.toByte() && bytes[1] == 0xfe.toByte() || bytes[0] == 0xfe.toByte() && bytes[1] == 0xff.toByte())
        val charset = if (bom16) java.nio.charset.Charset.forName("UTF-16") else Charsets.UTF_8
        state.consume(Resource.DECODER_ALLOCATION_BYTES,bytes.size*2L,part)
        val prefix = try { charset.newDecoder().onMalformedInput(java.nio.charset.CodingErrorAction.REPORT).decode(java.nio.ByteBuffer.wrap(bytes)).toString() }
            catch (error: java.nio.charset.CharacterCodingException) { throw PackageError(Code.INVALID_METADATA,"Invalid metadata encoding",part) }
        val declaration = kotlin.text.Regex("^\\uFEFF?<\\?xml\\s+[^?]*\\?>").find(prefix)?.value ?: ""
        val encoding = kotlin.text.Regex("encoding\\s*=\\s*['\"]([^'\"]+)['\"]").find(declaration)?.groupValues?.get(1)
        if (prefix.any { it == '\u0000' } || bom16 && encoding != null && !encoding.equals("UTF-16",true) || !bom16 && encoding != null && !encoding.equals("UTF-8",true) || declaration.contains(kotlin.text.Regex("version\\s*=\\s*['\"](?!1\\.0['\"])[^'\"]+"))) throw PackageError(Code.INVALID_METADATA,"Unsupported metadata encoding or XML version",part)
        val factory = javax.xml.parsers.SAXParserFactory.newInstance(); factory.isNamespaceAware = true
        factory.setFeature("http://apache.org/xml/features/disallow-doctype-decl",true)
        factory.setFeature("http://xml.org/sax/features/external-general-entities",false)
        factory.setFeature("http://xml.org/sax/features/external-parameter-entities",false)
        factory.setFeature("http://apache.org/xml/features/nonvalidating/load-external-dtd",false)
        factory.setFeature(javax.xml.XMLConstants.FEATURE_SECURE_PROCESSING,true)
        val reader = factory.newSAXParser().xmlReader
        reader.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD,""); reader.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA,"")
        val stack = mutableListOf<Node>(); val pending = linkedMapOf<String,String>(); var root: Node? = null
        val handler = object : org.xml.sax.helpers.DefaultHandler() {
            /** Retain original prefix declarations until their owning start event is admitted. */
            override fun startPrefixMapping(prefix: String, uri: String) { pending[prefix] = uri }
            /** Admit owned node, scope and attribute storage before retaining the provider event. */
            override fun startElement(uri: String, local: String, qualified: String, attributes: org.xml.sax.Attributes) {
                state.consume(Resource.WORK_UNITS,1,part)
                if (stack.size >= 64) throw PackageError(Code.RESOURCE_LIMIT,"Metadata XML depth limit",part)
                val inherited = stack.lastOrNull()?.namespaces ?: mapOf("xml" to XML)
                val cost = (inherited.entries+pending.entries).sumOf { 64L+it.key.length*2+it.value.length*2 }
                state.consume(Resource.DECODER_ALLOCATION_BYTES,256+cost,part)
                val scope = inherited.toMutableMap(); scope.putAll(pending); pending.clear()
                val node = Node(Name(uri,local),scope)
                for (index in 0 until attributes.length) {
                    val value = attributes.getValue(index)
                    state.consume(Resource.DECODER_ALLOCATION_BYTES,64L+value.length*2,part)
                    node.attributes[Name(attributes.getURI(index),attributes.getLocalName(index))] = value
                }
                if (stack.isEmpty()) { if (root != null) throw PackageError(Code.INVALID_METADATA,"Multiple metadata roots",part); root = node } else stack.last().children.add(node)
                stack.add(node)
            }
            /** Release the completed ancestor cursor after the provider has validated its end. */
            override fun endElement(uri: String, local: String, qualified: String) { state.poll(); stack.removeAt(stack.lastIndex) }
            /** Charge logical text independently of provider chunk boundaries. */
            override fun characters(chars: kotlin.CharArray, offset: Int, count: Int) {
                state.consume(Resource.WORK_UNITS,count.toLong(),part); state.consume(Resource.DECODER_ALLOCATION_BYTES,count*2L,part)
                if (stack.isNotEmpty()) stack.last().text.append(chars,offset,count)
            }
            /** Reject every external resolution request without accessing a transport. */
            override fun resolveEntity(publicId: String?, systemId: String?): org.xml.sax.InputSource { throw PackageError(Code.INVALID_METADATA,"External metadata entity unavailable",part) }
            /** Stop on recoverable parser errors before publishing a metadata tree. */
            override fun error(error: org.xml.sax.SAXParseException) { throw error }
            /** Retain the original provider failure for the bounded outer diagnostic. */
            override fun fatalError(error: org.xml.sax.SAXParseException) { throw error }
        }
        reader.contentHandler = handler; reader.entityResolver = handler; reader.errorHandler = handler
        try { reader.parse(org.xml.sax.InputSource(java.io.ByteArrayInputStream(bytes))); state.poll() }
        catch (error: PackageError) { throw error }
        catch (error: org.xml.sax.SAXException) { state.first?.let { throw it }; val failure = PackageError(Code.INVALID_METADATA,"Malformed or unsupported metadata XML",part); failure.initCause(error); throw failure }
        return root ?: throw PackageError(Code.INVALID_METADATA,"Metadata root missing",part)
    }
    /** Element-only metadata permits XML whitespace but no hidden text. */
    private fun elementContent(node: Node, part: String) { if (node.text.any { it !in " \t\r\n" }) throw PackageError(Code.INVALID_METADATA,"Unexpected metadata text",part) }
    /** Require exact unqualified infrastructure attributes, retaining media spelling separately. */
    private fun attributes(node: Node, part: String, vararg allowed: String) {
        if (node.attributes.keys.any { it.uri.isNotEmpty() || it.local !in allowed }) throw PackageError(Code.INVALID_METADATA,"Unknown metadata attribute",part)
    }
    /** Extract a required raw attribute without collapsing MIME parameter whitespace. */
    private fun rawAttribute(node: Node, name: String, part: String): String = node.attributes[Name("",name)] ?: throw PackageError(Code.INVALID_METADATA,"Missing metadata attribute",part)
    /** Apply XML token whitespace only where the metadata schema defines it. */
    private fun attribute(node: Node, name: String, part: String): String = collapse(rawAttribute(node,name,part))
    /** Apply selected OPC relationship MCE without interpreting discarded subtrees. */
    private class Compatibility(private val state: State, private val part: String) {
        private class Scope(val parent: Scope?, val ignorable: Set<String>, val process: Set<Name>)
        /** Split original-scope tokens after cumulative allocation/work admission. */
        private fun tokens(value: String): List<String> { state.consume(Resource.WORK_UNITS,value.length+1L,part); state.consume(Resource.DECODER_ALLOCATION_BYTES,value.length*8L,part); return collapse(value).split(' ').filter { it.isNotEmpty() } }
        /** Resolve one prefix while reserving compatibility's own namespace. */
        private fun prefix(node: Node, token: String): String {
            val uri = node.namespaces[token]
            if (!ncname(token) || uri.isNullOrEmpty() || uri == MC) throw PackageError(Code.INVALID_METADATA,"Invalid compatibility prefix",part)
            return uri
        }
        /** Search parent capability declarations without copying inherited sets. */
        private fun ignorable(scope: Scope?, uri: String): Boolean { var current = scope; while (current != null) { state.consume(Resource.WORK_UNITS,1,part); if (uri in current.ignorable) return true; current = current.parent }; return false }
        /** Match exact expanded ProcessContent names and namespace wildcards. */
        private fun process(scope: Scope?, name: Name): Boolean { var current = scope; while (current != null) { state.consume(Resource.WORK_UNITS,1,part); if (name in current.process || Name(name.uri,"*") in current.process) return true; current = current.parent }; return false }
        /** Validate declarations even on an unselected Choice/Fallback wrapper. */
        private fun declarations(node: Node, parent: Scope?): Scope {
            state.consume(Resource.DECODER_ALLOCATION_BYTES,128,part)
            val ignore = mutableSetOf<String>(); val names = mutableSetOf<Name>(); val scope = Scope(parent,ignore,names)
            node.attributes[Name(MC,"Ignorable")]?.let { value -> tokens(value).forEach { ignore.add(prefix(node,it)) } }
            for ((name,value) in node.attributes) if (name.uri == MC) when (name.local) {
                "ProcessContent" -> for (token in tokens(value)) {
                    val colon = token.indexOf(':')
                    if (colon < 1 || colon != token.lastIndexOf(':') || token.substring(colon+1) != "*" && !ncname(token.substring(colon+1))) throw PackageError(Code.INVALID_METADATA,"Invalid ProcessContent name",part)
                    val uri = prefix(node,token.substring(0,colon))
                    if (!ignorable(scope,uri)) throw PackageError(Code.INVALID_METADATA,"ProcessContent namespace is not ignorable",part)
                    names.add(Name(uri,token.substring(colon+1)))
                }
                "MustUnderstand" -> tokens(value).forEach { prefix(node,it) }
                "Ignorable" -> {}
                else -> throw PackageError(Code.INVALID_METADATA,"Unknown compatibility attribute",part)
            }
            return scope
        }
        /** Required capabilities are checked only on retained, selected or unwrapped content. */
        private fun must(node: Node) { node.attributes[Name(MC,"MustUnderstand")]?.let { value -> if (tokens(value).any { prefix(node,it) != RELS }) throw PackageError(Code.INVALID_METADATA,"Unsupported required metadata namespace",part) } }
        /** Validate all Choice/Fallback attributes before selecting the first supported branch. */
        private fun wrapper(node: Node, scope: Scope, choice: Boolean): Boolean {
            var requires: String? = null
            for ((name,value) in node.attributes) if (name.uri.isEmpty()) { if (!choice || name.local != "Requires") throw PackageError(Code.INVALID_METADATA,"Unexpected compatibility attribute",part); requires = value }
                else if (name.uri != MC && (name.uri == XML || !ignorable(scope,name.uri))) throw PackageError(Code.INVALID_METADATA,"Invalid qualified wrapper attribute",part)
            if (!choice) return false
            val selected = requires ?: throw PackageError(Code.INVALID_METADATA,"Missing Choice Requires",part)
            val tokens = tokens(selected); if (tokens.isEmpty()) throw PackageError(Code.INVALID_METADATA,"Empty Choice Requires",part)
            return tokens.map { prefix(node,it) }.all { it == RELS }
        }
        /** Flatten selected children while retaining text that can violate element-only content. */
        private fun children(node: Node, scope: Scope?) {
            val output = mutableListOf<Node>(); val text = kotlin.text.StringBuilder(node.text)
            for (child in node.children) when (visit(child,scope,false)) { 0 -> output.add(child); 1 -> { output.addAll(child.children); text.append(child.text) } }
            node.children = output; node.text = text
        }
        /** Return keep=0, unwrap=1 or discard=2 for one original-scope node. */
        private fun visit(node: Node, parent: Scope?, selectedBranch: Boolean): Int {
            state.consume(Resource.WORK_UNITS,1,part); val scope = declarations(node,parent); val name = node.name
            if (name.uri != RELS && ignorable(scope,name.uri)) {
                if (!process(scope,name)) return 2
)packagekotlin"
R"packagekotlin(                if (listOf("base","lang","space").any { Name(XML,it) in node.attributes }) throw PackageError(Code.INVALID_METADATA,"XML context on unwrapped metadata",part)
                must(node); children(node,scope); return 1
            }
            if (name.uri == MC) {
                if (name.local == "AlternateContent") {
                    wrapper(node,scope,false); elementContent(node,part); var choices = 0; var fallback = false; var selected: Node? = null
                    for (child in node.children) {
                        val childScope = declarations(child,scope)
                        if (child.name.uri != RELS && ignorable(childScope,child.name.uri) && !process(childScope,child.name)) continue
                        if (child.name.uri != MC) throw PackageError(Code.INVALID_METADATA,"Unexpected AlternateContent child",part)
                        if (child.name.local == "Choice" && !fallback) { choices++; if (wrapper(child,childScope,true) && selected == null) selected = child }
                        else if (child.name.local == "Fallback" && choices > 0 && !fallback) { fallback = true; wrapper(child,childScope,false); if (selected == null) selected = child }
                        else throw PackageError(Code.INVALID_METADATA,"Invalid Choice/Fallback order",part)
                    }
                    if (choices == 0) throw PackageError(Code.INVALID_METADATA,"AlternateContent requires Choice",part)
                    must(node)
                    if (selected == null) { node.children.clear(); node.text.setLength(0) } else { visit(selected,scope,true); node.children = selected.children; node.text = selected.text }
                    return 1
                }
                if (!selectedBranch || name.local !in listOf("Choice","Fallback")) throw PackageError(Code.INVALID_METADATA,"Unexpected compatibility element",part)
                wrapper(node,scope,name.local == "Choice"); must(node); children(node,scope); return 1
            }
            must(node); children(node,scope)
            node.attributes.keys.removeAll { it.uri == MC || it.uri != RELS && ignorable(scope,it.uri) }
            return 0
        }
        /** Require exactly one document root after compatibility processing. */
        fun apply(root: Node): Node { val parent = Node(Name("",""),emptyMap()); parent.children.add(root); children(parent,null); elementContent(parent,part); if (parent.children.size != 1) throw PackageError(Code.INVALID_METADATA,"Compatibility requires one root",part); return parent.children.single() }
    }
)packagekotlin"
