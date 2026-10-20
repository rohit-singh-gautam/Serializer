// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagekotlin(
    /** Grow an owning result only after cumulative and simultaneous byte admission. */
    private class Bytes(private val state: State, private val part: String = "") : java.io.Closeable {
        private val output = java.io.ByteArrayOutputStream()
        private var held = 0L
        /** Report the checked logical output extent without copying the current buffer. */
        val size: Long get() = held
        /** Append a bounded slice without retaining application buffers. */
        fun append(bytes: ByteArray, offset: Int = 0, count: Int = bytes.size) {
            if (offset < 0 || count < 0 || offset > bytes.size || count > bytes.size-offset || count > state.budget.limits.maxArchiveBytes-held) throw PackageError(Code.RESOURCE_LIMIT,"Package archive byte limit",part)
            arraySize(held+count,state.budget.limits.maxBufferBytes,part)
            state.consume(Resource.DECODER_ALLOCATION_BYTES,count.toLong(),part); state.budget.hold(count.toLong(),part)
            try { output.write(bytes,offset,count); held += count } catch (error: kotlin.Throwable) { state.budget.release(count.toLong()); throw error }
        }
        /** Write a specified unsigned little-endian field. */
        fun number(value: Long, count: Int) { val bytes = ByteArray(count) { (value ushr (it*8)).toByte() }; append(bytes) }
        /** Return a detached result while accounting for simultaneous output copies. */
        fun finish(): ByteArray {
            state.consume(Resource.DECODER_ALLOCATION_BYTES,held,part); state.budget.hold(held,part)
            try { state.poll(); return output.toByteArray() } finally { state.budget.release(held) }
        }
        /** Release only explicitly reserved internal output bytes. */
        override fun close() { state.budget.release(held); held = 0; output.reset() }
    }
    /** Complete owning ZIP writer with deterministic metadata and explicit finish/failed-state fencing. */
    class ZipWriter private constructor(private val state: State) : java.io.Closeable {
        /** Start an independent sequential writer. */
        constructor(budget: Budget = Budget()) : this(State(budget))
        /** Nested OPC composition observes its enclosing wrapper between every provider call. */
        internal constructor(budget: Budget, parent: State) : this(State(budget,parent))
        private class Part(val name: String, val encoded: ByteArray, val data: ByteArray, val method: Int, val expanded: Long, val crc: Long, val field: String, val id: String, val logicalName: String) { var offset = 0L }
        private val parts = linkedMapOf<String,Part>()
        private var retained = 0L
        /** Propagate a selected role through physical writer preparation and later directory rendering. */
        internal fun addContext(name: String, bytes: ByteArray, compression: String, logicalName: String, field: String, id: String) {
            state.contextPart=logicalName; state.contextField=field; state.contextId=id
            try { add(name,bytes,compression) } finally { state.contextPart=""; state.contextField=""; state.contextId="" }
        }
        /** Snapshot one caller value before deflation; failures never admit another write. */
        fun add(name: String, bytes: ByteArray, compression: String = "stored") {
            state.begin()
            try {
                physicalName(name)
                if (parts.containsKey(name)) throw PackageError(Code.DUPLICATE_NAME,"Duplicate ZIP item",name)
                if (compression != "stored" && compression != "deflate") throw PackageError(Code.INVALID_OPTIONS,"Unknown ZIP compression",name)
                arraySize(bytes.size.toLong(),state.budget.limits.maxPartBytes,name)
                state.consume(Resource.EXPANDED_BYTES,bytes.size.toLong(),name)
                val encoded = srlUtf8(name)
                if (encoded.size > 65535) throw PackageError(Code.INVALID_NAME,"ZIP filename too long",name)
                state.consume(Resource.ENTRIES,1,name); state.consume(Resource.METADATA_BYTES,46L+encoded.size,name)
                state.consume(Resource.DECODER_ALLOCATION_BYTES,bytes.size.toLong()+encoded.size,name)
                state.budget.hold(bytes.size.toLong()+encoded.size,name); retained += bytes.size.toLong()+encoded.size
                val snapshot = bytes.copyOf(); val crc = java.util.zip.CRC32(); var checked = 0
                while (checked < snapshot.size) { val batch = minOf(CHUNK,snapshot.size-checked); state.consume(Resource.WORK_UNITS,batch.toLong(),name); crc.update(snapshot,checked,batch); checked += batch }
                val data = if (compression == "stored") snapshot else {
                    val deflater = java.util.zip.Deflater(java.util.zip.Deflater.DEFAULT_COMPRESSION,true)
                    try {
                        deflater.setInput(snapshot); deflater.finish()
                        Bytes(state,name).use { output ->
                            val scratch = arraySize(minOf(CHUNK.toLong(),state.budget.limits.maxBufferBytes-state.budget.held),CHUNK.toLong(),name)
                            if (scratch == 0) throw PackageError(Code.RESOURCE_LIMIT,"DEFLATE scratch capacity required",name)
                            state.consume(Resource.DECODER_ALLOCATION_BYTES,scratch.toLong(),name); state.budget.hold(scratch.toLong(),name)
                            try {
                            val block = ByteArray(scratch)
                            while (!deflater.finished()) {
                                state.poll(); val count = deflater.deflate(block); state.consume(Resource.WORK_UNITS,count.toLong()+1,name)
                                if (count <= 0 && !deflater.finished()) throw PackageError(Code.CODEC,"DEFLATE made no progress",name)
                                output.append(block,0,count)
                            }
                            output.finish()
                            } finally { state.budget.release(scratch.toLong()) }
                        }
                    } finally { deflater.end() }
                }
                if (compression == "deflate") {
                    state.budget.release(snapshot.size.toLong()); retained -= snapshot.size
                    state.budget.hold(data.size.toLong(),name); retained += data.size
                }
                parts[name] = Part(name,encoded,data,if (compression == "deflate") 8 else 0,bytes.size.toLong(),crc.value,state.contextField,state.contextId,state.contextPart)
                state.poll()
            } catch (error: kotlin.Throwable) { state.closed = true; state.own(error) }
            finally { state.busy = false; if (state.closed) cleanup() }
        }
        /** Encode one canonical ZIP directory after every payload has been checked and retained. */
        private fun archive(): ByteArray = Bytes(state).use { output ->
            val ordered = parts.values.sortedBy { it.name }
            val descriptor = state.budget.limits.descriptor; val wide = state.budget.limits.forceZip64
            for (part in ordered) {
                state.contextPart=part.logicalName; state.contextField=part.field; state.contextId=part.id
                state.poll(); part.offset = output.size
                val flags = 0x800L or if (descriptor) 8 else 0
                output.number(0x04034b50,4); output.number(if (wide) 45 else 20,2); output.number(flags,2)
                output.number(part.method.toLong(),2); output.number(0,2); output.number(33,2)
                output.number(if (descriptor) 0 else part.crc,4)
                output.number(if (wide) U32 else if (descriptor) 0 else part.data.size.toLong(),4)
                output.number(if (wide) U32 else if (descriptor) 0 else part.expanded,4)
                output.number(part.encoded.size.toLong(),2); output.number(if (wide) 20 else 0,2); output.append(part.encoded)
                if (wide) { output.number(1,2); output.number(16,2); output.number(part.expanded,8); output.number(part.data.size.toLong(),8) }
                output.append(part.data)
                if (descriptor) { output.number(0x08074b50,4); output.number(part.crc,4); output.number(part.data.size.toLong(),if (wide) 8 else 4); output.number(part.expanded,if (wide) 8 else 4) }
            }
            state.contextPart=""; state.contextField=""; state.contextId=""
            val directory = output.size
            for (part in ordered) {
                state.contextPart=part.logicalName; state.contextField=part.field; state.contextId=part.id
                state.consume(Resource.WORK_UNITS,1,part.name)
                output.number(0x02014b50,4); output.number(if (wide) 45 else 20,2); output.number(if (wide) 45 else 20,2)
                output.number(0x800L or if (descriptor) 8 else 0,2); output.number(part.method.toLong(),2)
                output.number(0,2); output.number(33,2); output.number(part.crc,4)
                output.number(if (wide) U32 else part.data.size.toLong(),4); output.number(if (wide) U32 else part.expanded,4)
                output.number(part.encoded.size.toLong(),2); output.number(if (wide) 28 else 0,2)
                output.number(0,2); output.number(0,2); output.number(0,2); output.number(0,4); output.number(if (wide) U32 else part.offset,4)
                output.append(part.encoded)
                if (wide) { output.number(1,2); output.number(24,2); output.number(part.expanded,8); output.number(part.data.size.toLong(),8); output.number(part.offset,8) }
            }
            state.contextPart=""; state.contextField=""; state.contextId=""
            val size = output.size-directory
            val directoryWide = wide || ordered.size >= 65535 || directory >= U32 || size >= U32
            if (directoryWide) {
                val location = output.size
                output.number(0x06064b50,4); output.number(44,8); output.number(45,2); output.number(45,2)
                output.number(0,4); output.number(0,4); output.number(ordered.size.toLong(),8); output.number(ordered.size.toLong(),8)
                output.number(size,8); output.number(directory,8)
                output.number(0x07064b50,4); output.number(0,4); output.number(location,8); output.number(1,4)
            }
            output.number(0x06054b50,4); output.number(0,2); output.number(0,2)
            output.number(if (directoryWide) 65535 else ordered.size.toLong(),2); output.number(if (directoryWide) 65535 else ordered.size.toLong(),2)
            output.number(if (directoryWide) U32 else size,4); output.number(if (directoryWide) U32 else directory,4); output.number(0,2)
            output.finish()
        }
        /** Return a complete owning archive; finish succeeds only once. */
        fun finish(): ByteArray {
            state.begin()
            try { val result = archive(); state.poll(); state.closed = true; return result }
            catch (error: kotlin.Throwable) { state.closed = true; state.own(error) }
            finally { state.busy = false; cleanup() }
        }
        /** Borrow a sink after preparation; a transport failure may retain an external prefix. */
        fun writeTo(destination: java.io.OutputStream) {
            state.begin()
            try {
                val bytes = archive(); var offset = 0
                while (offset < bytes.size) { state.poll(); val count = minOf(CHUNK,bytes.size-offset); state.consume(Resource.IO_BYTES,count.toLong()); destination.write(bytes,offset,count); state.poll(); offset += count }
                state.closed = true
            } catch (error: kotlin.Throwable) { state.closed = true; state.own(error) }
            finally { state.busy = false; cleanup() }
        }
        /** Release owned payload snapshots after success, failure or inactive close. */
        private fun cleanup() { state.budget.release(retained); retained = 0; parts.clear() }
        /** Never implicitly finish an archive. */
        override fun close() { state.close(); if (!state.busy) cleanup() }
    }
    /** Write complete staged bytes in bounded calls without closing or flushing a borrowed sink. */
    fun write(destination: java.io.OutputStream, bytes: ByteArray, budget: Budget = Budget()) {
        val state = State(budget); state.begin()
        try {
            arraySize(bytes.size.toLong(),budget.limits.maxArchiveBytes)
            var offset = 0
            while (offset < bytes.size) { state.poll(); val count = minOf(CHUNK,bytes.size-offset); state.consume(Resource.IO_BYTES,count.toLong()); destination.write(bytes,offset,count); state.poll(); offset += count }
            state.poll()
        } catch (error: kotlin.Throwable) { state.fail(error) } finally { state.busy = false }
    }
    /** Stage an exclusively owned sibling and require atomic replacement of the final path. */
    fun save(path: java.nio.file.Path, bytes: ByteArray, budget: Budget = Budget(), fault: ((String) -> Unit)? = null) {
        val state = State(budget); state.begin(); var temporary: java.nio.file.Path? = null; var published = false
        /** Poll around each caller fault hook while retaining the original publication boundary. */
        fun point(name: String) { state.poll(); fault?.invoke(name); state.poll() }
        try {
            arraySize(bytes.size.toLong(),budget.limits.maxArchiveBytes)
            val target = path.toAbsolutePath(); val directory = target.parent
            point("before_temporary"); temporary = privateFile(directory,".serializer-save-"); point("after_temporary")
            java.nio.channels.FileChannel.open(temporary,java.nio.file.StandardOpenOption.WRITE).use { output ->
                var offset = 0
)packagekotlin"
R"packagekotlin(                while (offset < bytes.size) { state.poll(); val count = minOf(CHUNK,bytes.size-offset); state.consume(Resource.IO_BYTES,count.toLong()); val block = java.nio.ByteBuffer.wrap(bytes,offset,count); while (block.hasRemaining()) { state.poll(); if (output.write(block) <= 0) throw java.io.IOException("Private Save made no progress") }; offset += count }
                point("before_file_sync"); output.force(true); point("after_file_sync")
            }
            point("before_replace"); java.nio.file.Files.move(temporary,target,java.nio.file.StandardCopyOption.ATOMIC_MOVE,java.nio.file.StandardCopyOption.REPLACE_EXISTING)
            published = true; temporary = null; point("after_replace")
            if (java.nio.file.Files.getFileStore(directory).supportsFileAttributeView("posix")) java.nio.channels.FileChannel.open(directory,java.nio.file.StandardOpenOption.READ).use { it.force(true) }
            point("after_directory_sync")
        } catch (error: kotlin.Throwable) {
            if (published) { val failure = PackageError(Code.PUBLICATION_UNCERTAIN,"Final package replaced; durability acknowledgement failed"); failure.initCause(error); state.first = failure; state.fail(failure) }
            state.fail(error)
        } finally { state.busy = false; temporary?.let { try { java.nio.file.Files.deleteIfExists(it) } catch (cleanup: kotlin.Throwable) { state.first?.addSuppressed(cleanup) ?: throw cleanup } } }
    }
)packagekotlin"
