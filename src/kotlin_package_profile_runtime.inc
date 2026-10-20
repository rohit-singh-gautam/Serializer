// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagekotlin(
    /** Declarative role constants are compiler-owned; negative maximum means unbounded. */
    data class Role @JvmOverloads constructor(val field: String, val name: String?, val contentType: String?, val source: String,
        val relationship: String, val id: String?, val compression: String, val minimum: Long, val maximum: Long,
        val policy: String = "defaults", val expectedRoot: String? = null, val expectedType: String? = null, val schemaRole: String = field) {
        /** Preserve the original Java copy signature while retaining added profile metadata. */
        fun copy(field: String = this.field, name: String? = this.name, contentType: String? = this.contentType,
            source: String = this.source, relationship: String = this.relationship, id: String? = this.id,
            compression: String = this.compression, minimum: Long = this.minimum, maximum: Long = this.maximum): Role =
            Role(field,name,contentType,source,relationship,id,compression,minimum,maximum,policy,expectedRoot,expectedType,schemaRole)
    }
    /** Every generated codec receives the same package ledger with original role/part/link context. */
    class CodecContext internal constructor(private val state: State, val role: Role, val part: String, val relationshipId: String) {
        val budget: Budget get() = state.budget
        var fieldPath: String = role.field
            private set
        /** Cooperative cancellation remains active inside nested provider and native loops. */
        fun check() { state.poll() }
        /** Preserve the selected native field when its independent local quota is exhausted. */
        internal fun resourceLimit(message: String, field: String): Nothing {
            state.active();fieldPath=role.field+(if(field.isEmpty()) "" else "."+field)
            state.fail(PackageError(Code.RESOURCE_LIMIT,message,part,0,fieldPath,"",relationshipId))
        }
        /** Admit native allocations/work/references before publication, preserving selected member scope. */
        fun consume(kind: String, amount: Long, field: String = "") {
            fieldPath = role.field+(if (field.isEmpty()) "" else "."+field)
            val resource = when (kind) { "allocation" -> Resource.DECODER_ALLOCATION_BYTES; "reference" -> Resource.REFERENCES; else -> Resource.WORK_UNITS }
            state.consume(resource,amount,part,fieldPath,relationshipId)
        }
        /** Attribute a codec failure without including any model payload excerpts. */
        internal fun codec(error: kotlin.Throwable): kotlin.Nothing {
            state.first?.let { throw it }
            if (error is PackageError) throw error
            val failure = PackageError(Code.CODEC,"Package typed XML codec rejected part",part,0,fieldPath,"",relationshipId)
            failure.initCause(error); throw failure
        }
    }
    /** Check cardinality on the complete source-scoped role occurrence set. */
    private fun occurs(role: Role, count: Int) { if (count < role.minimum || role.maximum >= 0 && count > role.maximum) throw PackageError(Code.SEMANTIC,"Package role cardinality failed","",0,role.field) }
    /** Compose all declared values and preserved extensions before returning an archive. */
    class Composer(budget: Budget = Budget()) {
        private val state = State(budget)
        private val sources = mutableMapOf<String,List<String>>()
        private val parts = mutableListOf<BinaryPart>(); private val links = mutableListOf<Relationship>()
        /** Resolve one explicit parent occurrence before codec work. */
        private fun parent(role: Role, index: Int, name: String): String {
            val parents = if (role.source == "/") listOf("/") else sources[role.source]
            if (parents == null || index !in parents.indices) throw PackageError(Code.INVALID_RELATIONSHIP,"Role source occurrence missing",name,0,role.field)
            return parents[index]
        }
        /** Retain a checked payload and its immutable role-derived edge. */
        private fun add(role: Role, data: ByteArray, dynamicName: String, dynamicType: String, sourceIndex: Int, index: Int, discovered: MutableList<String>) {
            val name = role.name ?: dynamicName; val media = role.contentType ?: dynamicType
            partName(name); mime(media); val source = parent(role,sourceIndex,name)
            val id = role.id ?: "srl_${role.field}_$index"
            parts.add(BinaryPart(data,name,media,sourceIndex,role.compression).also { it.contextField=role.field; it.contextId=id })
            links.add(Relationship(source,id,role.relationship,name)); discovered.add(name)
        }
        /** Encode one optional/fixed typed value through the same repeated-role path. */
        fun <T> single(role: Role, value: T?, encoder: (T,CodecContext) -> ByteArray) {
            models(role,if (value == null) emptyList() else listOf(ModelPart(value,role.name ?: "",role.contentType ?: "")),encoder)
        }
        /** Encode every role occurrence privately; a middle failure fences the complete composition. */
        fun <T> models(role: Role, values: List<ModelPart<T>>, encoder: (T,CodecContext) -> ByteArray) {
            state.begin()
            try {
                occurs(role,values.size); val discovered = mutableListOf<String>()
                for ((index,item) in values.withIndex()) {
                    state.poll(); val name = role.name ?: item.name; parent(role,item.sourceIndex,name); partName(name); mime(role.contentType ?: item.contentType)
                    val context = CodecContext(state,role,name,role.id ?: "srl_${role.field}_$index")
                    val bytes = try { encoder(item.value,context) } catch (error: IllegalArgumentException) { context.codec(error) }
                    state.poll(); state.consume(Resource.DECODER_ALLOCATION_BYTES,bytes.size.toLong(),name,role.field,context.relationshipId)
                    add(role,bytes,item.name,item.contentType,item.sourceIndex,index,discovered)
                }
                sources[role.field] = discovered
            } catch (error: kotlin.Throwable) { state.closed = true; state.fail(error) } finally { state.busy = false }
        }
        /** Bind uninterpreted whole opaque parts without XML callbacks. */
        fun binaries(role: Role, values: List<BinaryPart>) {
            state.begin()
            try { occurs(role,values.size); val discovered = mutableListOf<String>(); for ((index,item) in values.withIndex()) { state.poll(); add(role,item.data,item.name,item.contentType,item.sourceIndex,index,discovered) }; sources[role.field] = discovered }
            catch (error: kotlin.Throwable) { state.closed = true; state.fail(error) } finally { state.busy = false }
        }
        /** Revalidate retained whole-part/link context together with every edited typed role. */
        fun finish(extensions: Extensions = Extensions()): ByteArray {
            state.begin()
            try { parts.addAll(extensions.parts); links.addAll(extensions.relationships); val bytes = opcWrite(parts,links,state); state.poll(); state.closed = true; return bytes }
            catch (error: kotlin.Throwable) { state.closed = true; state.fail(error) }
            finally { state.busy = false; parts.clear(); links.clear(); sources.clear() }
        }
    }
    /** Unpublished typed aggregate loading owns only its private archive adapter. */
    class Loader(source: Any, budget: Budget = Budget()) : java.io.Closeable {
        private val state = State(budget)
        private val archive: ZipReader
        private val opc: Opc
        private val sources = mutableMapOf<String,List<String>>()
        private val decoded = mutableSetOf<String>(); private val selected = mutableSetOf<Int>()
        private var finished = false
        init {
            state.begin(); var opened: ZipReader? = null
            try { opened = ZipReader.open(source,budget,state); archive = opened; opc = opcRead(archive,state); state.poll() }
            catch (error: kotlin.Throwable) { state.closed = true; try { opened?.close() } catch (cleanup: kotlin.Throwable) { error.addSuppressed(cleanup) }; state.fail(error) }
            finally { state.busy = false }
        }
        private data class Occurrence(val metadata: PartMetadata, val parent: Int, val id: String)
        private val locations = mutableMapOf<Pair<String,Int>,PayloadLocation>()
        /** Retrieve an already discovered occurrence for generated reference diagnostics. */
        fun location(role: String, index: Int): PayloadLocation = locations[role to index]
            ?: throw PackageError(Code.INVALID_STATE,"Package occurrence location unavailable","",0,role)
        /** Route by source/type/media identity; fixed output paths are not input dispatch keys. */
        private fun discover(role: Role): List<Occurrence> {
            val parents = if (role.source == "/") listOf("/") else sources[role.source] ?: throw PackageError(Code.INVALID_RELATIONSHIP,"Role source unavailable","",0,role.field)
            val result = mutableListOf<Occurrence>(); val discovered = mutableListOf<String>()
            for ((parent,source) in parents.withIndex()) for ((index,link) in opc.links.withIndex()) {
                state.consume(Resource.WORK_UNITS,1,source,role.field,link.id)
                if (fold(link.source) != fold(source) || link.type != role.relationship) continue
                if (link.mode != "internal") throw PackageError(Code.INVALID_RELATIONSHIP,"Declared role requires internal target",source,0,role.field,"",link.id)
                val key = fold(target(source,link.target)); val metadata = opc.parts[key] ?: throw PackageError(Code.MISSING_PART,"Role target missing",source,0,role.field,"",link.id)
                if (role.contentType != null && mime(metadata.contentType) != mime(role.contentType)) throw PackageError(Code.CODEC,"Role content type mismatch",metadata.name,0,role.field,"",link.id)
                if (role.policy == "fixed" && metadata.entry.method != if (role.compression == "deflate") 8 else 0) throw PackageError(Code.INVALID_OPTIONS,"Fixed package storage mismatch",metadata.name,0,role.field,"",link.id)
                if (!decoded.add(key)) throw PackageError(Code.INVALID_RELATIONSHIP,"Part claimed by multiple role occurrences",metadata.name,0,role.field,"",link.id)
                state.consume(Resource.DECODER_ALLOCATION_BYTES,referenceMetadataAdmissionBytes,metadata.name,role.field,link.id)
                locations[role.schemaRole to result.size] = PayloadLocation(metadata.name,link.id)
                selected.add(index); result.add(Occurrence(metadata,parent,link.id)); discovered.add(metadata.name)
            }
            occurs(role,result.size); sources[role.field] = discovered; return result
        }
        /** Decode an optional/fixed role without publishing an incomplete aggregate. */
        fun <T> single(role: Role, decoder: (ByteArray,CodecContext) -> T): T? = models(role,decoder).firstOrNull()?.value
        /** Decode all selected role occurrences to fresh values before returning the role list. */
        fun <T> models(role: Role, decoder: (ByteArray,CodecContext) -> T): List<ModelPart<T>> {
            state.begin()
            try {
                val result = mutableListOf<ModelPart<T>>()
                for (item in discover(role)) {
                    state.poll(); val bytes = archive.readContext(item.metadata.entry.name,item.metadata.name,role.field,item.id); val context = CodecContext(state,role,item.metadata.name,item.id)
                    val value = try { dispatch(bytes,role,state,item.metadata.name,item.id); decoder(bytes,context) } catch (error: IllegalArgumentException) { context.codec(error) } catch (error: javax.xml.stream.XMLStreamException) { context.codec(error) }
                    state.poll(); result.add(ModelPart(value,item.metadata.name,item.metadata.contentType,item.parent))
                }
                state.poll(); return result.toList()
            } catch (error: kotlin.Throwable) { state.closed = true; state.fail(error) } finally { leave() }
        }
        /** Return complete validated opaque payloads with their original physical method. */
        fun binaries(role: Role): List<BinaryPart> {
            state.begin()
            try { return discover(role).map { val bytes = archive.readContext(it.metadata.entry.name,it.metadata.name,role.field,it.id); state.poll(); BinaryPart(bytes,it.metadata.name,it.metadata.contentType,it.parent,if (it.metadata.entry.method == 8) "deflate" else "stored") } }
            catch (error: kotlin.Throwable) { state.closed = true; state.fail(error) } finally { leave() }
        }
        /** Check optional/required capabilities and domain validation before publishing a complete fresh value. */
        fun <T> finish(value: T, policy: ExtensionPolicy = ExtensionPolicy(), validate: ((T,Budget) -> Unit)? = null, bind: ((T,Extensions) -> Unit)? = null): Result<T> {
            state.begin()
            try {
                if (finished) throw PackageError(Code.INVALID_STATE,"Finished package loader")
                val opaque = mutableListOf<BinaryPart>(); val links = mutableListOf<Relationship>()
                for ((key,metadata) in opc.parts) {
                    if (key in decoded) continue
                    state.poll(); val required = policy.requiredPart?.invoke(metadata) ?: false; state.poll()
                    if (required || policy.parts == Unknown.REJECT) throw PackageError(Code.UNSUPPORTED_FEATURE,"Unknown required or rejected package part",metadata.name)
                    val bytes = archive.read(metadata.entry.name)
                    if (policy.parts == Unknown.PRESERVE) { state.consume(Resource.PRESERVED_BYTES,bytes.size.toLong(),metadata.name); opaque.add(BinaryPart(bytes,metadata.name,metadata.contentType,0,if (metadata.entry.method == 8) "deflate" else "stored")) }
                }
                for ((index,link) in opc.links.withIndex()) {
                    if (index in selected) continue
                    state.poll(); val required = policy.requiredRelationship?.invoke(link) ?: false; state.poll()
)packagekotlin"
R"packagekotlin(                    if (required || policy.relationships == Unknown.REJECT) throw PackageError(Code.UNSUPPORTED_FEATURE,"Unknown required or rejected relationship",link.source,0,"","",link.id)
                    if (policy.relationships == Unknown.PRESERVE) { val size = listOf(link.source,link.id,link.type,link.target,link.mode).sumOf { srlUtf8(it).size.toLong() }; state.consume(Resource.PRESERVED_BYTES,size,link.source,"",link.id); links.add(link) }
                }
                val extensions = Extensions(opaque,links)
                state.poll(); bind?.invoke(value,extensions); state.poll(); validate?.invoke(value,state.budget); state.poll()
                finished = true; return Result(value,extensions)
            } catch (error: kotlin.Throwable) { state.closed = true; state.fail(error) } finally { leave() }
        }
        /** End an active borrow before cleaning a wrapper closed by caller callbacks. */
        private fun leave() { state.busy = false; if (state.closed) archive.close() }
        /** Close only owned paths/spools; caller sources and streams are borrowed. */
        override fun close() { state.close(); archive.close() }
    }
)packagekotlin"
