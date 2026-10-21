// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagekotlin(
    /** Checked random-access reader; only Path-opened channels/private spools are owned. */
    class ZipReader private constructor(private val state: State) : java.io.Closeable {
        private lateinit var source: RandomSource
        private var owned: java.io.Closeable? = null
        private var extent = 0L
        private val items = mutableListOf<Entry>()
        private val names = linkedMapOf<String,Entry>()
        companion object {
            /** Publish only a fully checked archive; byte input is snapshotted before parsing. */
            fun open(input: Any, budget: Budget = Budget()): ZipReader = open(input,budget,null)
            /** Nested operations retain the enclosing wrapper's activity guard. */
            internal fun open(input: Any, budget: Budget, parent: State?): ZipReader {
                val value = ZipReader(State(budget,parent)); value.state.begin()
                try { value.attach(input); value.parse(); value.state.poll(); return value }
                catch (error: kotlin.Throwable) { value.state.closed = true; value.state.fail(error) }
                finally { value.state.busy = false; if (value.state.closed) value.cleanup() }
            }
        }
        /** Return a detached immutable-view metadata list. */
        fun entries(): List<Entry> = java.util.Collections.unmodifiableList(items.toList())
        /** Borrow the cumulative operation ledger. */
        fun budget(): Budget = state.budget
        /** Attach stable bytes, files or borrowed sources; sequential sources require explicit spooling. */
        private fun attach(input: Any) {
            state.poll()
            when (input) {
                is ByteArray -> {
                    arraySize(input.size.toLong(),state.budget.limits.maxArchiveBytes)
                    state.budget.hold(input.size.toLong()); owned = java.io.Closeable { state.budget.release(input.size.toLong()) }
                    state.consume(Resource.DECODER_ALLOCATION_BYTES,input.size.toLong()); val snapshot = input.copyOf()
                    source = object : RandomSource {
                        /** Expose only the private admitted snapshot extent. */
                        override fun size(): Long = snapshot.size.toLong()
                        /** Copy a previously checked bounded slice from stable private storage. */
                        override fun readAt(offset: Long, maximum: Int): ByteArray = snapshot.copyOfRange(offset.toInt(),offset.toInt()+maximum)
                    }
                    extent = snapshot.size.toLong()
                }
                is java.nio.file.Path -> { val file = FileSource(input); owned = file; source = file; extent = file.size() }
                is RandomSource -> { source = input; extent = input.size(); state.poll() }
                is java.io.InputStream -> spool(input)
                else -> throw PackageError(Code.INVALID_OPTIONS,"Unsupported package source")
            }
            if (extent < 0 || extent > state.budget.limits.maxArchiveBytes) throw PackageError(Code.RESOURCE_LIMIT,"Archive byte limit exceeded")
            state.poll()
        }
        /** Stage borrowed sequential input privately, polling cancellation around every bounded callback. */
        private fun spool(input: java.io.InputStream) {
            val limits = state.budget.limits
            val directory = limits.spoolDirectory
            if (!limits.allowSpool || directory == null) throw PackageError(Code.INVALID_OPTIONS,"Sequential input requires explicit private spooling")
            val count = arraySize(minOf(CHUNK.toLong(),limits.maxBufferBytes-state.budget.held),CHUNK.toLong())
            if (count == 0 || state.budget.remaining(Resource.IO_BYTES) == 0L) throw PackageError(Code.RESOURCE_LIMIT,"Spool scratch and I/O capacity required")
            state.consume(Resource.DECODER_ALLOCATION_BYTES,count.toLong())
            val temporary = privateFile(directory,".serializer-zip-")
            try {
                java.nio.channels.FileChannel.open(temporary,java.nio.file.StandardOpenOption.WRITE).use { output ->
                    state.budget.hold(count.toLong())
                    try {
                        val bytes = ByteArray(count); var size = 0L
                        while (true) {
                            state.poll(); val remaining = minOf(limits.maxArchiveBytes,limits.maxSpoolBytes)-size
                            val io = state.budget.remaining(Resource.IO_BYTES)
                            if (io == 0L) throw PackageError(Code.RESOURCE_LIMIT,"Spool has no admitted EOF capacity")
                            val requested = minOf(io,if (remaining >= count) count.toLong() else remaining+1).toInt()
                            val read = input.read(bytes,0,requested); state.poll()
                            if (read == -1) break
                            if (read <= 0 || read > requested) throw java.io.IOException("Invalid or zero-progress package source")
                            if (read > remaining) throw PackageError(Code.RESOURCE_LIMIT,"Spool byte limit exceeded")
                            state.consume(Resource.IO_BYTES,read.toLong()); val buffer = java.nio.ByteBuffer.wrap(bytes,0,read)
                            while (buffer.hasRemaining()) { state.poll(); if (output.write(buffer) <= 0) throw java.io.IOException("Spool made no progress") }
                            size += read
                        }
                    } finally { state.budget.release(count.toLong()) }
                }
                val file = FileSource(temporary,temporary); owned = file; source = file; extent = file.size()
            } catch (error: kotlin.Throwable) { try { java.nio.file.Files.deleteIfExists(temporary) } catch (cleanup: kotlin.Throwable) { error.addSuppressed(cleanup) }; throw error }
        }
        /** Validate full extents first, then accumulate legal short random reads. */
        private fun readAt(offset: Long, length: Long): ByteArray {
            if (offset < 0 || length < 0 || offset > extent || length > extent-offset) throw PackageError(Code.INVALID_ARCHIVE,"ZIP extent outside archive")
            val count = arraySize(length,state.budget.limits.maxBufferBytes)
            state.consume(Resource.IO_BYTES,length); state.consume(Resource.DECODER_ALLOCATION_BYTES,length)
            state.budget.hold(length)
            try {
                val result = ByteArray(count); var position = 0
                while (position < count) {
                    state.poll(); val requested = minOf(CHUNK,count-position)
                    state.consume(Resource.DECODER_ALLOCATION_BYTES,requested*2L); state.budget.hold(requested*2L)
                    try {
                        val block = try { source.readAt(offset+position,requested) } catch (error: kotlin.Throwable) { state.fail(error) }; state.poll()
                        if (block.isEmpty() || block.size > requested) throw java.io.IOException("Invalid or truncated package random read")
                        block.copyInto(result,position); position += block.size
                    } finally { state.budget.release(requested*2L) }
                }
                return result
            } finally { state.budget.release(length) }
        }
        /** ZIP64 extra fields replace sentinel fields in their standard order. */
        private fun extra(bytes: ByteArray, vararg required: Boolean): LongArray {
            var position = 0; var selected: ByteArray? = null
            while (position < bytes.size) {
                val kind = number(bytes,position,2).toInt(); val size = number(bytes,position+2,2).toInt(); position += 4
                if (size > bytes.size-position) throw PackageError(Code.INVALID_ARCHIVE,"Truncated ZIP extra field")
                if (kind == 1) { if (selected != null) throw PackageError(Code.INVALID_ARCHIVE,"Duplicate ZIP64 extra"); selected = bytes.copyOfRange(position,position+size) }
                position += size
            }
            val count = required.count { it }
            if (count == 0) return longArrayOf()
            val values = selected ?: throw PackageError(Code.INVALID_ARCHIVE,"Missing ZIP64 extra")
            if (values.size < count*8) throw PackageError(Code.INVALID_ARCHIVE,"Truncated ZIP64 extra")
            return LongArray(count) { number(values,it*8,8) }
        }
        /** Validate exact end/directory/local boundaries and all descriptor forms before inflation. */
        private fun parse() {
            val tailOffset = maxOf(0,extent-65557); val tail = readAt(tailOffset,extent-tailOffset)
            state.budget.hold(tail.size.toLong()); var centralHeld = 0L
            try {
                var selected = -1
                for (position in tail.size-22 downTo 0) if (number(tail,position,4) == 0x06054b50L && number(tail,position+20,2) == (tail.size-position-22).toLong()) { selected = position; break }
                if (selected < 0) throw PackageError(Code.INVALID_ARCHIVE,"ZIP end directory missing")
                var count = number(tail,selected+10,2); var size = number(tail,selected+12,4); var offset = number(tail,selected+16,4)
                val end = tailOffset+selected; var boundary = end
                if (number(tail,selected+4,2) != 0L || number(tail,selected+6,2) != 0L || number(tail,selected+8,2) != count) throw PackageError(Code.UNSUPPORTED_FEATURE,"Multidisk ZIP unsupported")
                if (end >= 20) {
                    val locator = readAt(end-20,20)
                    if (number(locator,0,4) == 0x07064b50L) {
                        val location = number(locator,8,8)
                        if (number(locator,4,4) != 0L || number(locator,16,4) != 1L || location > end-20 || end-20-location < 56) throw PackageError(Code.INVALID_ARCHIVE,"Invalid ZIP64 locator")
                        val record = readAt(location,56); val wideCount = number(record,32,8); val wideSize = number(record,40,8); val wideOffset = number(record,48,8)
                        if (number(record,0,4) != 0x06064b50L || number(record,4,8) < 44 || number(record,4,8) != end-20-location-12 || number(record,16,4) != 0L || number(record,20,4) != 0L || number(record,24,8) != wideCount) throw PackageError(Code.INVALID_ARCHIVE,"Invalid ZIP64 directory")
                        if (number(record,14,2) > 45) throw PackageError(Code.UNSUPPORTED_FEATURE,"Unsupported ZIP64 extraction version")
                        if (count != 65535L && count != wideCount || size != U32 && size != wideSize || offset != U32 && offset != wideOffset) throw PackageError(Code.INVALID_ARCHIVE,"ZIP32 and ZIP64 disagree")
                        count = wideCount; size = wideSize; offset = wideOffset; boundary = location
                    }
                }
                if (offset > boundary || size != boundary-offset) throw PackageError(Code.INVALID_ARCHIVE,"ZIP directory boundary mismatch")
                state.consume(Resource.ENTRIES,count); state.consume(Resource.METADATA_BYTES,size)
                val central = readAt(offset,size); state.budget.hold(size); centralHeld = size; var position = 0
                repeat(arraySize(count,Int.MAX_VALUE.toLong())) {
                    state.consume(Resource.WORK_UNITS,1)
                    if (number(central,position,4) != 0x02014b50L) throw PackageError(Code.INVALID_ARCHIVE,"Invalid ZIP central entry")
                    val flags = number(central,position+8,2).toInt(); val method = number(central,position+10,2).toInt()
                    val crc = number(central,position+16,4); var compressed = number(central,position+20,4); var expanded = number(central,position+24,4)
                    val nameSize = number(central,position+28,2).toInt(); val extraSize = number(central,position+30,2).toInt(); val commentSize = number(central,position+32,2).toInt()
                    var local = number(central,position+42,4); val length = 46+nameSize+extraSize+commentSize
                    if (length > 65535 || length > central.size-position) throw PackageError(Code.INVALID_ARCHIVE,"Truncated ZIP central fields")
                    if (number(central,position+34,2) != 0L || flags and (0x800 or 8 or 6).inv() != 0 || method != 0 && method != 8) throw PackageError(Code.UNSUPPORTED_FEATURE,"Unsupported ZIP entry")
                    val encoded = central.copyOfRange(position+46,position+46+nameSize); val name = physicalName(text(encoded,flags and 0x800 != 0))
)packagekotlin"
R"packagekotlin(                    if (names.containsKey(name)) throw PackageError(Code.DUPLICATE_NAME,"Duplicate physical ZIP item",name)
                    var wide = expanded == U32 || compressed == U32
                    var values = extra(central.copyOfRange(position+46+nameSize,position+46+nameSize+extraSize),expanded == U32,compressed == U32,local == U32)
                    var next = 0; if (expanded == U32) expanded = values[next++]; if (compressed == U32) compressed = values[next++]; if (local == U32) local = values[next]
                    if (expanded > state.budget.limits.maxPartBytes) throw PackageError(Code.RESOURCE_LIMIT,"Expanded part exceeds limit",name)
                    if (local > offset || offset-local < 30) throw PackageError(Code.INVALID_ARCHIVE,"ZIP local header overlaps directory",name)
                    val header = readAt(local,30)
                    if (number(header,0,4) != 0x04034b50L || number(header,4,2) > 45 || number(central,position+6,2) > 45 || number(header,10,2) != number(central,position+12,2) || number(header,12,2) != number(central,position+14,2) || number(header,6,2).toInt() != flags || number(header,8,2).toInt() != method) throw PackageError(Code.INVALID_ARCHIVE,"ZIP headers disagree",name)
                    val localName = number(header,26,2); val localExtra = number(header,28,2); val data = local+30+localName+localExtra
                    if (data > offset || compressed > offset-data) throw PackageError(Code.INVALID_ARCHIVE,"ZIP payload overlaps directory",name)
                    if (!readAt(local+30,localName).contentEquals(encoded)) throw PackageError(Code.INVALID_ARCHIVE,"ZIP local filename mismatch",name)
                    var localExpanded = number(header,22,4); var localCompressed = number(header,18,4)
                    values = extra(readAt(local+30+localName,localExtra),localExpanded == U32,localCompressed == U32); next = 0
                    if (localExpanded == U32) localExpanded = values[next++]; if (localCompressed == U32) localCompressed = values[next]
                    var finish = data+compressed
                    if (flags and 8 != 0) {
                        wide = wide || number(header,18,4) == U32 || number(header,22,4) == U32
                        val descriptorSize = if (wide) 20L else 12L
                        var descriptor = readAt(finish,descriptorSize); var signed = false
                        if (!descriptor(descriptor,wide,crc,compressed,expanded) && number(descriptor,0,4) == 0x08074b50L) { descriptor = readAt(finish+4,descriptorSize); signed = true }
                        if (!descriptor(descriptor,wide,crc,compressed,expanded) || number(header,14,4) != 0L && number(header,14,4) != crc || localExpanded != 0L && localExpanded != expanded || localCompressed != 0L && localCompressed != compressed) throw PackageError(Code.INTEGRITY,"ZIP descriptor mismatch",name)
                        finish += descriptorSize+if (signed) 4 else 0
                    } else if (number(header,14,4) != crc || localExpanded != expanded || localCompressed != compressed) throw PackageError(Code.INTEGRITY,"ZIP local CRC/size mismatch",name)
                    if (finish > offset) throw PackageError(Code.INVALID_ARCHIVE,"ZIP record overlaps directory",name)
                    val entry = Entry(name,method,flags,crc,compressed,expanded,data,local,finish,(number(central,position+4,2) ushr 8).toInt(),number(central,position+38,4))
                    state.consume(Resource.DECODER_ALLOCATION_BYTES,256L+encoded.size,name); items.add(entry); names[name] = entry; position += length
                }
                if (position != central.size && (central.size-position < 6 || number(central,position,4) != 0x05054b50L || number(central,position+4,2) != (central.size-position-6).toLong())) throw PackageError(Code.INVALID_ARCHIVE,"Unexpected ZIP directory records")
                val intervals = items.sortedBy { it.localOffset }
                for (index in 1 until intervals.size) if (intervals[index-1].recordEnd > intervals[index].localOffset) throw PackageError(Code.INVALID_ARCHIVE,"Overlapping ZIP local records")
            } finally { state.budget.release(centralHeld+tail.size) }
        }
        /** An unsigned descriptor may itself start with the signature-valued CRC. */
        private fun descriptor(bytes: ByteArray, wide: Boolean, crc: Long, compressed: Long, expanded: Long): Boolean =
            number(bytes,0,4) == crc && number(bytes,4,if (wide) 8 else 4) == compressed && number(bytes,if (wide) 12 else 8,if (wide) 8 else 4) == expanded
        /** Carry selected role/edge context through entry transport, DEFLATE and CRC admission. */
        internal fun readContext(name: String, part: String, field: String, id: String): ByteArray {
            state.contextPart=part; state.contextField=field; state.contextId=id
            try { return read(name) }
            finally { state.contextPart=""; state.contextField=""; state.contextId="" }
        }
        /** Decode one complete selected payload and verify exact raw-DEFLATE extent, CRC and sizes. */
        fun read(name: String): ByteArray {
            state.begin()
            try {
                val entry = names[name] ?: throw PackageError(Code.MISSING_PART,"ZIP item missing",name)
                state.consume(Resource.EXPANDED_BYTES,entry.expandedBytes,name); state.consume(Resource.DECODER_ALLOCATION_BYTES,entry.expandedBytes,name)
                val count = arraySize(entry.expandedBytes,state.budget.limits.maxPartBytes,name)
                state.budget.hold(entry.expandedBytes,name)
                try {
                    val result = ByteArray(count); val crc = java.util.zip.CRC32(); var position = 0; var encoded = 0L
                    if (entry.method == 0) {
                        if (entry.compressedBytes != entry.expandedBytes) throw PackageError(Code.INTEGRITY,"Stored ZIP size mismatch",name)
                        while (position < count) { val block = readAt(entry.dataOffset+position,minOf(CHUNK,count-position).toLong()); block.copyInto(result,position); position += block.size }
                    } else {
                        state.consume(Resource.DECODER_ALLOCATION_BYTES,1,name); state.budget.hold(1,name)
                        val inflater = java.util.zip.Inflater(true)
                        try {
                            val overflow = ByteArray(1)
                            while (!inflater.finished()) {
                                state.poll()
                                if (inflater.needsInput() && encoded < entry.compressedBytes) { val block = readAt(entry.dataOffset+encoded,minOf(CHUNK.toLong(),entry.compressedBytes-encoded)); encoded += block.size; inflater.setInput(block) }
                                val read = if (position < count) inflater.inflate(result,position,minOf(CHUNK,count-position)) else inflater.inflate(overflow)
                                state.consume(Resource.WORK_UNITS,read.toLong()+1,name)
                                if (position == count && read != 0) throw PackageError(Code.INTEGRITY,"ZIP expansion exceeds declared size",name)
                                position += read
                                if (read == 0 && !inflater.finished() && (inflater.needsDictionary() || !inflater.needsInput() || encoded == entry.compressedBytes)) throw PackageError(Code.INTEGRITY,"Invalid or truncated ZIP DEFLATE stream",name)
                            }
                            if (encoded-inflater.remaining != entry.compressedBytes) throw PackageError(Code.INTEGRITY,"Trailing ZIP DEFLATE bytes",name)
                        } catch (error: java.util.zip.DataFormatException) { throw PackageError(Code.INTEGRITY,"Invalid ZIP DEFLATE stream",name) }
                        finally { inflater.end(); state.budget.release(1) }
                    }
                    var checked = 0
                    while (checked < result.size) { val batch = minOf(CHUNK,result.size-checked); state.consume(Resource.WORK_UNITS,batch.toLong(),name); crc.update(result,checked,batch); checked += batch }
                    if (position != count || crc.value != entry.crc32) throw PackageError(Code.INTEGRITY,"ZIP CRC/size mismatch",name)
                    state.poll(); return result
                } finally { state.budget.release(entry.expandedBytes) }
            } catch (error: kotlin.Throwable) {
                state.closed = true
                state.own(error)
            }
            finally { state.busy = false; if (state.closed) cleanup() }
        }
        /** Release only owned resources, deferring closure requested inside an active callback. */
        private fun cleanup() {
            val resource = owned; owned = null
            try { resource?.close() } catch (cleanup: kotlin.Throwable) { val first = state.first; if (first != null) first.addSuppressed(cleanup) else throw cleanup }
        }
        /** Never close application streams or random sources. */
        override fun close() { state.close(); if (!state.busy) cleanup() }
    }
)packagekotlin"
