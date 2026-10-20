// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagekotlin(
/** Bounded ZIP/OPC operations; instances never install a process-global provider. */
object @package_name@ {
    private const val CHUNK = 32768
    private const val U32 = 0xffffffffL
    enum class Code { INVALID_OPTIONS, INVALID_STATE, INVALID_ARCHIVE, UNSUPPORTED_FEATURE,
        INVALID_NAME, DUPLICATE_NAME, INTEGRITY, RESOURCE_LIMIT, INVALID_METADATA,
        INVALID_RELATIONSHIP, MISSING_PART, SEMANTIC, CODEC, PUBLICATION_UNCERTAIN, DEADLINE_EXCEEDED }
    /** Logical diagnostics deliberately exclude input and model excerpts. */
    class PackageError(val code: Code, message: String, val part: String = "", val offsetBytes: Long = 0,
        val fieldPath: String = "", val reference: String = "", val relationshipId: String = "") : java.io.IOException(message)
    /** A budget takes an immutable snapshot before touching sources or application hooks. */
    data class Limits(var maxArchiveBytes: Long = 256L*1024*1024, var maxIoBytes: Long = 1024L*1024*1024,
        var maxMetadataBytes: Long = 16L*1024*1024, var maxPartBytes: Long = 64L*1024*1024,
        var maxTotalExpandedBytes: Long = 256L*1024*1024, var maxBufferBytes: Long = 128L*1024*1024,
        var maxEntries: Long = 4096, var maxRelationships: Long = 16384,
        var maxDecoderAllocationBytes: Long = 128L*1024*1024, var maxWorkUnits: Long = 512L*1024*1024,
        var maxReferences: Long = 65536, var maxPreservedBytes: Long = 64L*1024*1024,
        var checkCancelled: (() -> Unit)? = null, var deadlineNanos: Long? = null,
        var spoolDirectory: java.nio.file.Path? = null, var maxSpoolBytes: Long = 256L*1024*1024,
        var allowSpool: Boolean = false, var forceZip64: Boolean = false, var descriptor: Boolean = true)
    enum class Resource { IO_BYTES, METADATA_BYTES, EXPANDED_BYTES, ENTRIES, RELATIONSHIPS,
        DECODER_ALLOCATION_BYTES, WORK_UNITS, REFERENCES, PRESERVED_BYTES }
    /** Failed and repeated operations retain cumulative admission; this is not a hard heap limit. */
    class Budget(value: Limits = Limits()) {
        internal val limits = value.copy()
        private val used = LongArray(Resource.entries.size)
        internal var held = 0L
        private var checking = false
        private var failure: kotlin.Throwable? = null
        init {
            if (!(Resource.entries.all { maximum(it) >= 0 } && limits.maxArchiveBytes >= 0 &&
                limits.maxPartBytes >= 0 && limits.maxBufferBytes >= 0 && limits.maxSpoolBytes >= 0)) throw PackageError(Code.INVALID_OPTIONS,"Negative package limit")
            check()
        }
        /** Return a detached policy snapshot. */
        fun configured(): Limits = limits.copy()
        /** Resolve each resource directly from the retained limit profile. */
        private fun maximum(resource: Resource): Long = when (resource) {
            Resource.IO_BYTES -> limits.maxIoBytes; Resource.METADATA_BYTES -> limits.maxMetadataBytes
            Resource.EXPANDED_BYTES -> limits.maxTotalExpandedBytes; Resource.ENTRIES -> limits.maxEntries
            Resource.RELATIONSHIPS -> limits.maxRelationships; Resource.DECODER_ALLOCATION_BYTES -> limits.maxDecoderAllocationBytes
            Resource.WORK_UNITS -> limits.maxWorkUnits; Resource.REFERENCES -> limits.maxReferences
            Resource.PRESERVED_BYTES -> limits.maxPreservedBytes
        }
        /** Inspect remaining capacity without running caller code. */
        fun remaining(resource: Resource): Long = maximum(resource) - used[resource.ordinal]
        /** Poll cancellation/deadline and fence even swallowed recursive policy failures. */
        fun check() {
            failure?.let { throw it }
            if (checking) { val error = PackageError(Code.INVALID_STATE,"Reentrant package policy"); failure = error; throw error }
            checking = true
            try {
                limits.checkCancelled?.invoke()
                failure?.let { throw it }
                limits.deadlineNanos?.let { if (java.lang.System.nanoTime()-it >= 0) throw PackageError(Code.DEADLINE_EXCEEDED,"Package deadline exceeded") }
            } catch (error: kotlin.Throwable) { if (failure == null) failure = error; throw failure!! }
            finally { checking = false }
        }
        /** Charge before allocation, callbacks or publication. */
        fun consume(resource: Resource, amount: Long, part: String = "", fieldPath: String = "", relationshipId: String = "") {
            check(); charge(resource,amount,part,fieldPath,relationshipId)
        }
        /** Charge only after an enclosing state guard has polled the application hook. */
        internal fun charge(resource: Resource, amount: Long, part: String = "", fieldPath: String = "", relationshipId: String = "") {
            if (amount < 0 || amount > remaining(resource)) throw PackageError(Code.RESOURCE_LIMIT,"Package resource limit exceeded",part,0,fieldPath,"",relationshipId)
            used[resource.ordinal] += amount
        }
        /** Bound simultaneous explicitly tracked byte arrays independently of cumulative allocations. */
        internal fun hold(amount: Long, part: String = "") {
            if (amount < 0 || amount > limits.maxBufferBytes-held) throw PackageError(Code.RESOURCE_LIMIT,"Package buffer limit exceeded",part)
            held += amount
        }
        /** Release a previous private reservation. */
        internal fun release(amount: Long) { held -= amount }
    }
    /** Preserve first failures and detect same-wrapper reentry on both sides of callbacks. */
    internal class State(val budget: Budget, private val parent: State? = null) {
        var busy = false; var closed = false; var first: kotlin.Throwable? = null
        var contextPart = ""; var contextField = ""; var contextId = ""
        /** Admit a sequential operation; recursive calls poison the active outer call. */
        fun begin() {
            if (busy) fail(PackageError(Code.INVALID_STATE,"Reentrant package operation"))
            first?.let { throw it }
            if (closed) throw PackageError(Code.INVALID_STATE,"Inactive package operation")
            busy = true
        }
        /** Retain exact caller exception identity. */
        fun fail(error: kotlin.Throwable): kotlin.Nothing { if (first == null) first = error; throw first!! }
        /** Attribute runtime-owned package errors without replacing an already latched caller failure. */
        fun own(error: kotlin.Throwable): kotlin.Nothing {
            if (first == null && contextField.isNotEmpty() && error is PackageError && error.fieldPath.isEmpty()) {
                val located = PackageError(error.code,error.message ?: "Package operation rejected",contextPart,error.offsetBytes,contextField,error.reference,contextId)
                located.initCause(error); fail(located)
            }
            fail(error)
        }
        /** Observe parent and local lifecycle failures without caller hooks. */
        fun active() { first?.let { throw it }; try { parent?.active() } catch (error: kotlin.Throwable) { fail(error) }; if (closed) fail(PackageError(Code.INVALID_STATE,"Closed package operation")) }
        /** Check lifecycle before and after a policy callback. */
        fun poll() { active(); try { budget.check() } catch (error: kotlin.Throwable) { fail(error) }; active() }
        /** Admit work after polling the enclosing wrapper. */
        fun consume(resource: Resource, amount: Long, part: String = "", field: String = "", id: String = "") {
            poll()
            try { budget.charge(resource,amount,contextPart.ifEmpty { part },contextField.ifEmpty { field },contextId.ifEmpty { id }) }
            catch (error: kotlin.Throwable) { fail(error) }
        }
        /** Closure during a callback remains visible even if the callback swallows errors. */
        fun close() { closed = true; if (busy && first == null) first = PackageError(Code.INVALID_STATE,"Closed active package operation") }
    }
    /** Application random sources are stable borrowed values; methods may return bounded short reads. */
    interface RandomSource {
        /** Return the stable logical source extent without transferring ownership. */
        fun size(): Long
        /** Return one nonempty slice of at most maximum admitted bytes; short reads are legal. */
        fun readAt(offset: Long, maximum: Int): ByteArray
    }
    /** Immutable checked metadata never interprets names as filesystem paths. */
    class Entry internal constructor(val name: String, val method: Int, val flags: Int, val crc32: Long,
        val compressedBytes: Long, val expandedBytes: Long, internal val dataOffset: Long,
        internal val localOffset: Long, internal val recordEnd: Long, val creatorSystem: Int, val externalAttributes: Long) {
        /** Reject directories, symlinks and devices when mapping physical items to OPC parts. */
        fun regularFile(): Boolean {
            if (name.endsWith('/') || creatorSystem in listOf(0,10,14) && externalAttributes and 24L != 0L) return false
            val kind = (externalAttributes ushr 16).toInt() and 0xf000
            return creatorSystem !in listOf(3,19) || kind == 0 || kind == 0x8000
        }
    }
    /** Validate extents before narrowing to the owning array representation. */
    private fun arraySize(size: Long, maximum: Long, part: String = ""): Int {
        if (size < 0 || size > maximum || size > Int.MAX_VALUE-8) throw PackageError(Code.RESOURCE_LIMIT,"Package byte extent exceeds limit",part)
        return size.toInt()
    }
    /** Read unsigned little-endian values while rejecting values beyond signed JVM offsets. */
    private fun number(bytes: ByteArray, offset: Int, count: Int): Long {
        if (offset < 0 || count > bytes.size-offset) throw PackageError(Code.INVALID_ARCHIVE,"Truncated ZIP field")
        var value = 0L
        for (index in 0 until count) { val byte = bytes[offset+index].toLong() and 255; if (index == 7 && byte >= 128) throw PackageError(Code.UNSUPPORTED_FEATURE,"ZIP unsigned offset exceeds JVM range"); value = value or (byte shl (index*8)) }
        return value
    }
    /** Decode filenames strictly without Unicode normalization. */
    private fun text(bytes: ByteArray, unicode: Boolean): String = try {
        (if (unicode) Charsets.UTF_8 else java.nio.charset.Charset.forName("IBM437")).newDecoder()
            .onMalformedInput(java.nio.charset.CodingErrorAction.REPORT).onUnmappableCharacter(java.nio.charset.CodingErrorAction.REPORT)
            .decode(java.nio.ByteBuffer.wrap(bytes)).toString()
    } catch (error: java.nio.charset.CharacterCodingException) { throw PackageError(Code.INVALID_NAME,"Invalid ZIP filename encoding") }
    /** ZIP names remain logical metadata; dot, empty and backslash segments reject. */
    private fun physicalName(name: String): String {
        if (name.isEmpty() || name.length > 65535 || name.startsWith('/') || '\\' in name || '\u0000' in name) throw PackageError(Code.INVALID_NAME,"Invalid ZIP item name")
        val pieces = name.split('/')
        if (pieces.withIndex().any { (index,piece) -> piece == "." || piece == ".." || piece.isEmpty() && index+1 < pieces.size }) throw PackageError(Code.INVALID_NAME,"Invalid ZIP item segment")
        return name
    }
    /** Private staging files use exclusive creation and owner-only permissions where supported. */
    private fun privateFile(directory: java.nio.file.Path, prefix: String): java.nio.file.Path {
        val attributes = if (java.nio.file.Files.getFileStore(directory).supportsFileAttributeView("posix"))
            arrayOf(java.nio.file.attribute.PosixFilePermissions.asFileAttribute(java.nio.file.attribute.PosixFilePermissions.fromString("rw-------"))) else emptyArray()
        return java.nio.file.Files.createTempFile(directory,prefix,".tmp",*attributes)
    }
    /** Path sources own their channel; temporary sources additionally remove their private file. */
    private class FileSource(path: java.nio.file.Path, private val temporary: java.nio.file.Path? = null) : RandomSource, java.io.Closeable {
        private val channel = java.nio.channels.FileChannel.open(path,java.nio.file.StandardOpenOption.READ)
        /** Report the owned channel's current extent during attachment. */
        override fun size(): Long = channel.size()
        /** Read a bounded positional slice without changing a shared seek cursor. */
        override fun readAt(offset: Long, maximum: Int): ByteArray {
            val bytes = ByteArray(maximum); val count = channel.read(java.nio.ByteBuffer.wrap(bytes),offset)
            return if (count < 0) byteArrayOf() else bytes.copyOf(count)
        }
        /** Close the owned channel and remove only an exclusively created staging file. */
        override fun close() {
            var first: kotlin.Throwable? = null
            try { channel.close() } catch (error: kotlin.Throwable) { first = error }
            try { temporary?.let { java.nio.file.Files.deleteIfExists(it) } } catch (cleanup: kotlin.Throwable) { if (first == null) first = cleanup else first.addSuppressed(cleanup) }
            first?.let { throw it }
        }
    }
)packagekotlin"
