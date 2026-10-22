// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagekotlin(
    /** Retain the source part and relationship without owning any native model. */
    data class PayloadLocation(val part: String, val relationshipId: String)
    private const val referenceMetadataAdmissionBytes = 64L
    private const val xmlDispatchAdmissionBytes = 256L
    /** Resolve only declared ZIP storage choices; generated type and vocabulary identities stay bound. */
    data class TypedPipeline(val codec: String, val compression: String? = null)
    /** Resolve native payload and optional storage without replacing declared role/model identity. */
    fun resolveProfiles(roles: List<Role>, overrides: Map<String,String> = emptyMap(), pipelines: Map<String,TypedPipeline> = emptyMap()): List<Role> {
        if (overrides.keys.any { key -> roles.none { it.schemaRole == key } }) throw PackageError(Code.INVALID_OPTIONS,"Unknown package profile role")
        if (pipelines.keys.any { key -> roles.none { it.schemaRole == key } }) throw PackageError(Code.INVALID_OPTIONS,"Unknown package pipeline role")
        return roles.map { role ->
            val pipeline=pipelines[role.schemaRole]; val storage = pipeline?.compression ?: overrides[role.schemaRole] ?: role.compression
            val codec=pipeline?.codec ?: role.codec
            if (storage !in listOf("stored","deflate") || codec !in (if(role.expectedRoot == null) listOf("bytes") else listOf("xml","json","binary_none")) || role.policy == "fixed" && (storage != role.compression || codec != role.codec)) throw PackageError(Code.INVALID_OPTIONS,"Package profile override conflicts with declared policy","",0,role.field)
            Role(role.field,role.name,role.contentType,role.source,role.relationship,role.id,storage,role.minimum,role.maximum,role.policy,role.expectedRoot,role.expectedType,role.schemaRole,codec,role.targetMode,role.requiresType,role.vocabularies)
        }
    }
    /** Inspect only the expanded root and optional xsi:type before native model decoding. */
    private fun dispatch(bytes: ByteArray, role: Role, state: State, part: String, link: String) {
        if (role.expectedRoot == null || role.codec != "xml") return
        state.consume(Resource.WORK_UNITS,bytes.size.toLong(),part,role.field,link)
        state.consume(Resource.DECODER_ALLOCATION_BYTES,xmlDispatchAdmissionBytes,part,role.field,link)
        val factory = javax.xml.stream.XMLInputFactory.newFactory()
        factory.setProperty(javax.xml.stream.XMLInputFactory.SUPPORT_DTD,false)
        factory.setProperty("javax.xml.stream.isSupportingExternalEntities",false)
        val reader = factory.createXMLStreamReader(java.io.ByteArrayInputStream(bytes))
        try {
            while (reader.hasNext()) {
                state.poll()
                when (reader.next()) {
                    javax.xml.stream.XMLStreamConstants.DTD -> throw PackageError(Code.CODEC,"Package XML DTD forbidden",part,0,role.field,"",link)
                    javax.xml.stream.XMLStreamConstants.START_ELEMENT -> {
                        val root = if (reader.namespaceURI.isNullOrEmpty()) reader.localName else "{${reader.namespaceURI}}${reader.localName}"
                        if (root != role.expectedRoot) throw PackageError(Code.CODEC,"Package expected XML root mismatch",part,0,role.field,"",link)
                        val declared = reader.getAttributeValue("http://www.w3.org/2001/XMLSchema-instance","type")
                        if (role.requiresType && declared == null) throw PackageError(Code.CODEC,"Required package XML type is missing",part,0,role.field,"",link)
                        if (declared != null) {
                            val pieces = declared.trim(' ','\t','\r','\n').split(':')
                            val uri = if (pieces.size <= 2) reader.namespaceContext.getNamespaceURI(if (pieces.size == 2) pieces[0] else "") else ""
                            val expanded = if (uri.isNullOrEmpty()) pieces.last() else "{$uri}${pieces.last()}"
                            if (pieces.size > 2 || pieces.any { it.isEmpty() } || pieces.size == 2 && uri.isNullOrEmpty() || expanded != role.expectedType) throw PackageError(Code.CODEC,"Package expected XML type mismatch",part,0,role.field,"",link)
                        }
                        state.poll(); return
                    }
                }
            }
            throw PackageError(Code.CODEC,"Package XML root missing",part,0,role.field,"",link)
        } finally { reader.close() }
    }
    /** One scalar index validates typed forward references without retaining native model pointers. */
    class ReferenceIndex(private val budget: Budget) {
        private val targets = mutableMapOf<Triple<String,String,String>,String>()
        private data class Request(val key: Triple<String,String,String>, val type: String, val part: String, val field: String, val link: String)
        private val requests = mutableListOf<Request>()
        /** Admit logical storage before retaining one schema-selected key. */
        private fun key(scope: String, value: Any, part: String, field: String, link: String): Triple<String,String,String> {
            val text = value.toString()
            budget.consume(Resource.REFERENCES,1,part,field,link)
            budget.consume(Resource.METADATA_BYTES,referenceMetadataAdmissionBytes+scope.length*2L+text.length*2L,part,field,link)
            budget.consume(Resource.WORK_UNITS,1,part,field,link)
            return Triple(scope,if (value is String) "string" else "integer",text)
        }
        /** Reject duplicate scoped keys even when the candidate types differ. */
        fun add(scope: String, value: Any, type: String, part: String, field: String, link: String = "") {
            val identity = key(scope,value,part,field,link)
            if (targets.putIfAbsent(identity,type) != null) throw PackageError(Code.INVALID_REFERENCE,"Duplicate package payload key",part,0,field,identity.third,link)
        }
        /** Retain forward references until all selected candidate roles exist. */
        fun require(scope: String, value: Any, type: String, part: String, field: String, link: String = "") { requests.add(Request(key(scope,value,part,field,link),type,part,field,link)) }
        /** Reject missing or wrong-type targets before publication. */
        fun validate() { for (item in requests) { budget.consume(Resource.WORK_UNITS,1,item.part,item.field,item.link); if (targets[item.key] != item.type) throw PackageError(Code.INVALID_REFERENCE,"Missing or wrong-type package payload target",item.part,0,item.field,item.key.third,item.link) } }
    }
)packagekotlin"
