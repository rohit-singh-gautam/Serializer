// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagekotlin(
    /** Checked package catalog separates infrastructure from application parts. */
    private class Opc { val parts = linkedMapOf<String,PartMetadata>(); val links = mutableListOf<Relationship>() }
    /** Validate closed internal graph and exact source-local relationship IDs on one shared reference ledger. */
    private fun relationships(links: List<Relationship>, parts: Map<String,*>, state: State): List<Relationship> {
        val seen = mutableSetOf<String>(); val output = mutableListOf<Relationship>()
        for (link in links) {
            state.consume(Resource.RELATIONSHIPS,1,link.source,"",link.id); state.consume(Resource.REFERENCES,1,link.source,"",link.id)
            if (!ncname(link.id)) throw PackageError(Code.INVALID_RELATIONSHIP,"Invalid relationship ID",link.source,0,"","",link.id)
            val source = if (link.source == "/") "/" else partName(link.source)
            if (source != "/" && fold(source) !in parts) throw PackageError(Code.MISSING_PART,"Relationship source missing",source,0,"","",link.id)
            if (!seen.add(fold(source)+"\u0000"+link.id)) throw PackageError(Code.INVALID_RELATIONSHIP,"Duplicate source-local relationship ID",source,0,"","",link.id)
            uri(link.type,true)
            when (link.mode) {
                "internal" -> { val destination = target(source,link.target); if (fold(destination) !in parts) throw PackageError(Code.MISSING_PART,"Relationship target missing",source,0,"",destination,link.id) }
                "external" -> uri(link.target)
                else -> throw PackageError(Code.INVALID_RELATIONSHIP,"Invalid TargetMode",source,0,"","",link.id)
            }
            output.add(link.copy(source=source))
        }
        return output
    }
    /** Require one exact leaf-declaration grammar after optional relationship MCE. */
    private fun declarations(bytes: ByteArray, namespace: String, local: String, part: String, state: State, compatibility: Boolean): List<Node> {
        val parsed = metadata(bytes,part,state); val root = if (compatibility) Compatibility(state,part).apply(parsed) else parsed
        if (root.name != Name(namespace,local) || root.attributes.isNotEmpty()) throw PackageError(Code.INVALID_METADATA,"Invalid infrastructure root",part)
        elementContent(root,part)
        for (child in root.children) { if (child.children.isNotEmpty()) throw PackageError(Code.INVALID_METADATA,"Nested metadata declaration",part); elementContent(child,part) }
        return root.children
    }
    /** Validate all metadata and graph edges before any application codec runs. */
    private fun opcRead(archive: ZipReader, state: State): Opc {
        val manifests = archive.entries().filter { fold(it.name) == "[content_types].xml" }
        if (manifests.size != 1 || !manifests.single().regularFile()) throw PackageError(if (manifests.size > 1) Code.DUPLICATE_NAME else Code.INVALID_METADATA,"Missing or invalid content type manifest")
        val manifest = manifests.single(); val defaults = mutableMapOf<String,String>(); val overrides = mutableMapOf<String,String>()
        for (child in declarations(archive.read(manifest.name),CT,"Types","/[Content_Types].xml",state,false)) {
            when (child.name) {
                Name(CT,"Default") -> {
                    attributes(child,manifest.name,"Extension","ContentType"); val extension = fold(attribute(child,"Extension",manifest.name))
                    if (!extension.matches(kotlin.text.Regex("(?:[A-Za-z0-9_~!$&'()*+,:=@-]|%[0-9A-Fa-f]{2})+")) || extension in defaults) throw PackageError(Code.INVALID_METADATA,"Invalid or duplicate default extension")
                    val media = rawAttribute(child,"ContentType",manifest.name); mime(media); defaults[extension] = media
                }
                Name(CT,"Override") -> {
                    attributes(child,manifest.name,"PartName","ContentType"); val name = fold(partName(attribute(child,"PartName",manifest.name),true))
                    if (name in overrides) throw PackageError(Code.INVALID_METADATA,"Duplicate content type override")
                    val media = rawAttribute(child,"ContentType",manifest.name); mime(media); overrides[name] = media
                }
                else -> throw PackageError(Code.INVALID_METADATA,"Unknown content type declaration")
            }
        }
        if (defaults.isEmpty() && overrides.isEmpty()) throw PackageError(Code.INVALID_METADATA,"Empty content type manifest")
        val output = Opc(); val mapped = mutableSetOf<String>(); val identities = mutableSetOf<String>(); val infrastructure = linkedMapOf<String,String>()
        for (entry in archive.entries()) {
            state.consume(Resource.WORK_UNITS,entry.name.length+1L,entry.name)
            if (entry === manifest) continue
            val name = "/"+entry.name; val key = fold(name)
            if (!entry.regularFile() || !mapsToPart(name,state)) { archive.read(entry.name); continue }
            unique(identities,name); mapped.add(key)
            val media = overrides[key] ?: if (name.lastIndexOf('.') > name.lastIndexOf('/')) defaults[fold(name.substringAfterLast('.'))] else null
            if (media == null) throw PackageError(Code.INVALID_METADATA,"Part has no content type",name)
            val source = relationshipSource(name); val parsed = mime(media)
            if (source != null) { if (parsed.media != RELS_MIME) throw PackageError(Code.INVALID_METADATA,"Relationship part media mismatch",name); infrastructure[name] = source }
            else { partName(name); if (parsed.media == RELS_MIME) throw PackageError(Code.INVALID_METADATA,"Relationship media requires infrastructure",name); output.parts[key] = PartMetadata(name,media,entry) }
        }
        for (name in overrides.keys) if (name !in mapped) throw PackageError(Code.INVALID_METADATA,"Override names a missing or reserved part",name)
        val links = mutableListOf<Relationship>()
        for ((name,source) in infrastructure) {
            if (source != "/" && fold(source) !in output.parts) throw PackageError(Code.MISSING_PART,"Relationship-part source missing",name)
            for (child in declarations(archive.read(name.substring(1)),RELS,"Relationships",name,state,true)) {
                if (child.name != Name(RELS,"Relationship")) throw PackageError(Code.INVALID_METADATA,"Invalid relationship declaration",name)
                attributes(child,name,"Id","Type","Target","TargetMode")
                val mode = child.attributes[Name("","TargetMode")]?.let { collapse(it) } ?: "Internal"
                if (mode !in listOf("Internal","External")) throw PackageError(Code.INVALID_RELATIONSHIP,"Invalid TargetMode",name)
                links.add(Relationship(source,attribute(child,"Id",name),attribute(child,"Type",name),attribute(child,"Target",name),fold(mode)))
            }
        }
        output.links.addAll(relationships(links,output.parts,state)); state.poll(); return output
    }
    /** Count exact XML attribute output before creating escaped text or provider buffers. */
    private fun escapedSize(value: String): Long {
        var bytes = 0L; var position = 0
        while (position < value.length) {
            val code = value.codePointAt(position); position += java.lang.Character.charCount(code)
            if (!(code in listOf(9,10,13) || code in 32..0xd7ff || code in 0xe000..0xfffd || code in 0x10000..0x10ffff)) throw PackageError(Code.INVALID_METADATA,"Invalid XML metadata character")
            bytes += when (code) { 38 -> 5; 60 -> 4; 34 -> 6; 9 -> 4; 10,13 -> 5; else -> if (code < 128) 1 else if (code < 2048) 2 else if (code < 65536) 3 else 4 }
        }
        return bytes
    }
    /** Append already-admitted XML attribute escapes directly. */
    private fun escape(output: kotlin.text.StringBuilder, value: String) {
        for (character in value) when (character) { '&' -> output.append("&amp;"); '<' -> output.append("&lt;"); '"' -> output.append("&quot;"); '\t' -> output.append("&#9;"); '\n' -> output.append("&#10;"); '\r' -> output.append("&#13;"); else -> output.append(character) }
    }
    /** Emit standard metadata with canonical attribute order under the same operation ledger. */
    private fun metadataWrite(namespace: String, root: String, child: String, values: List<Map<String,String>>, state: State): ByteArray {
        val prefix = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><$root xmlns=\"$namespace\">"
        var size = prefix.length+root.length+3L
        for (attributes in values) { state.consume(Resource.WORK_UNITS,1); size += child.length+3; for ((name,value) in attributes) { state.consume(Resource.WORK_UNITS,value.length+1L); size += name.length+4+escapedSize(value) } }
        arraySize(size,state.budget.limits.maxPartBytes); state.consume(Resource.METADATA_BYTES,size); state.consume(Resource.DECODER_ALLOCATION_BYTES,size*3); state.budget.hold(size*3)
        try {
            val text = kotlin.text.StringBuilder(size.toInt()); text.append(prefix)
            for (attributes in values) { state.poll(); text.append('<').append(child); for ((name,value) in attributes.toSortedMap()) { text.append(' ').append(name).append("=\""); escape(text,value); text.append('"') }; text.append("/>") }
            text.append("</").append(root).append('>'); state.poll(); return srlUtf8(text.toString())
        } finally { state.budget.release(size*3) }
    }
    /** Build the complete canonical archive before any application sink or final file is touched. */
    private fun opcWrite(parts: List<BinaryPart>, links: List<Relationship>, state: State): ByteArray {
        val identities = mutableSetOf<String>(); val lookup = linkedMapOf<String,BinaryPart>()
        for (part in parts) { state.poll(); partName(part.name); if (mime(part.contentType).media == RELS_MIME) throw PackageError(Code.INVALID_METADATA,"Relationship media requires infrastructure",part.name); lookup[unique(identities,part.name)] = part }
        val checked = relationships(links,lookup,state)
        val declarations = mutableListOf<Map<String,String>>()
        ZipWriter(state.budget,state).use { writer ->
            for (part in parts.sortedBy { it.name }) { state.poll(); writer.addContext(part.name.substring(1),part.data,part.compression,part.name,part.contextField,part.contextId); declarations.add(mapOf("PartName" to part.name,"ContentType" to part.contentType)) }
            for ((source,group) in checked.groupBy { it.source }.toSortedMap()) {
                val name = if (source == "/") "/_rels/.rels" else source.substringBeforeLast('/')+"/_rels/"+source.substringAfterLast('/')+".rels"
                unique(identities,name)
                val children = group.sortedBy { it.id }.map { link -> linkedMapOf("Id" to link.id,"Type" to link.type,"Target" to link.target).also { if (link.mode == "external") it["TargetMode"] = "External" } }
                writer.add(name.substring(1),metadataWrite(RELS,"Relationships","Relationship",children,state),"deflate")
                declarations.add(mapOf("PartName" to name,"ContentType" to RELS_MIME))
            }
            val content = if (declarations.isEmpty()) metadataWrite(CT,"Types","Default",listOf(mapOf("Extension" to "rels","ContentType" to RELS_MIME)),state) else metadataWrite(CT,"Types","Override",declarations,state)
            writer.add("[Content_Types].xml",content,"deflate"); val result = writer.finish(); state.poll(); return result
        }
    }
)packagekotlin"
