// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compresskt(
/** Bounded whole-message compression; generated helper name: @compression_name@. */
object @compression_name@ {
    private const val DEFAULT_BYTES = 64 * 1024 * 1024
    private const val DEFAULT_WINDOW_BYTES = 8 * 1024 * 1024
    private const val DEFLATE_WINDOW_BYTES = 32768
    private const val BATCH_BYTES = 32768
    /** Framing is explicit and independent of the ordinary native protocol. */
    enum class Format { NONE, GZIP, ZLIB, DEFLATE, ZSTD, LZ4 }
    /** Compression failures contain a stable category and no payload excerpts. */
    enum class Code { UNAVAILABLE, INVALID_OPTIONS, INVALID_DATA, RESOURCE_LIMIT }
    /** Native codec, allocation, cancellation and transport exceptions retain their own types. */
    class Failure(val code: Code, message: String) : java.lang.IllegalArgumentException(message)
    /** Immutable bounds; optional formats choose Zstandard level three or LZ4 fast mode by default. */
    data class EncodeOptions(val format: Format = Format.NONE, val maxInputBytes: Int = DEFAULT_BYTES,
        val maxOutputBytes: Int = DEFAULT_BYTES, val level: Int = if (format == Format.ZSTD) 3 else if (format == Format.LZ4) 0 else 6, val checkCancelled: (() -> Unit)? = null) {
        init {
            if (maxInputBytes < 0 || maxOutputBytes < 0 || (format != Format.ZSTD && level !in 0..9))
                throw Failure(Code.INVALID_OPTIONS, "Invalid compression encoding options")
        }
    }
    /** Compressed input, actual output and window bounds are independent of native codec limits. */
    data class DecodeOptions(val format: Format = Format.NONE, val maxInputBytes: Int = DEFAULT_BYTES,
        val maxOutputBytes: Int = DEFAULT_BYTES, val maxWindowBytes: Int = DEFAULT_WINDOW_BYTES,
        val checkCancelled: (() -> Unit)? = null) {
        init {
            if (maxInputBytes < 0 || maxOutputBytes < 0 || maxWindowBytes < 0)
                throw Failure(Code.INVALID_OPTIONS, "Invalid compression decoding options")
        }
    }
    /** Probe optional deployment without invoking application callbacks or borrowed I/O. */
    fun available(format: Format): Boolean {
        try { when (format) { Format.ZSTD -> zstdProvider(); Format.LZ4 -> lz4Provider(); else -> return true }; return true }
        catch (failure: Failure) { if (failure.code == Code.UNAVAILABLE) return false; throw failure }
    }
    // Admit deployment and the full provider-specific level interval before any application effects.
    private fun admit(options: EncodeOptions) {
        if (!available(options.format)) throw Failure(Code.UNAVAILABLE, "Selected compression provider is unavailable")
        if (options.format == Format.ZSTD) zstdProvider().validateLevel(options.level)
        options.checkCancelled?.invoke()
    }
    // Admit conservative mandatory histories before source or codec callbacks.
    private fun admit(options: DecodeOptions) {
        if (!available(options.format)) throw Failure(Code.UNAVAILABLE, "Selected compression provider is unavailable")
        if ((options.format == Format.GZIP || options.format == Format.ZLIB || options.format == Format.DEFLATE) && options.maxWindowBytes < DEFLATE_WINDOW_BYTES ||
            options.format == Format.ZSTD && options.maxWindowBytes < 1024 || options.format == Format.LZ4 && options.maxWindowBytes < 65536)
            throw Failure(Code.RESOURCE_LIMIT, "Compression history limit exceeded")
        options.checkCancelled?.invoke()
    }
    // Check a span before indexing, subtraction or provider calls.
    private fun requireSpan(data: ByteArray, position: Int, count: Int) {
        if (position < 0 || count < 0 || position > data.size || count > data.size - position)
            throw Failure(Code.INVALID_DATA, "Truncated compression framing")
    }
    // Preserve unsigned wire bytes while arithmetic remains in bounded signed integers.
    private fun byte(data: ByteArray, position: Int): Int = data[position].toInt() and 255
    // Read a checked unsigned little-endian word into an exact wider value.
    private fun little(data: ByteArray, position: Int, count: Int): kotlin.Long {
        requireSpan(data, position, count); var result = 0L
        for (index in 0 until count) result = result or (byte(data, position + index).toLong() shl (index * 8))
        return result
    }
    // Compute CRC-32 through bounded standard-provider batches.
    private fun crc(data: ByteArray, start: Int, count: Int, check: (() -> Unit)?): kotlin.Long {
        val result = java.util.zip.CRC32(); var offset = 0
        while (offset < count) {
            check?.invoke(); val size = kotlin.math.min(BATCH_BYTES, count - offset)
            result.update(data, start + offset, size); offset += size
        }
        check?.invoke(); return result.value
    }
    // Compute mandatory zlib Adler-32 independently of the raw DEFLATE provider.
    private fun adler(data: ByteArray, check: (() -> Unit)?): kotlin.Long {
        val result = java.util.zip.Adler32(); var offset = 0
        while (offset < data.size) {
            check?.invoke(); val size = kotlin.math.min(BATCH_BYTES, data.size - offset)
            result.update(data, offset, size); offset += size
        }
        check?.invoke(); return result.value
    }
    // Copy admitted bytes through bounded cancellation intervals into independent storage.
    private fun copy(data: ByteArray, maximum: Int, check: (() -> Unit)?): ByteArray {
        if (data.size > maximum) throw Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded")
        val result = ByteArray(data.size); var offset = 0
        while (offset < data.size) {
            check?.invoke(); val size = kotlin.math.min(BATCH_BYTES, data.size - offset)
            java.lang.System.arraycopy(data, offset, result, offset, size); offset += size
        }
        check?.invoke(); return result
    }
    // Grow only after logical output admission; capacity/RSS are not the public byte accounting unit.
    private class Buffer(private val maximum: Int) : java.io.ByteArrayOutputStream() {
        override fun write(value: Int) {
            if (size() == maximum) throw Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded")
            super.write(value)
        }
        override fun write(data: ByteArray, offset: Int, length: Int) {
            if (length > maximum - size()) throw Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded")
            super.write(data, offset, length)
        }
    }
    // Append an externally specified little-endian trailer word.
    private fun writeLittle(output: Buffer, value: kotlin.Long) {
        for (index in 0 until java.lang.Integer.BYTES) output.write((value ushr (index * 8)).toInt())
    }
    // Check optional gzip fields and FHCRC without interpreting opaque name/comment bytes.
    private fun gzipStart(data: ByteArray, check: (() -> Unit)?): Int {
        requireSpan(data, 0, 10)
        if (byte(data, 0) != 31 || byte(data, 1) != 139 || byte(data, 2) != 8 || byte(data, 3) and 224 != 0)
            throw Failure(Code.INVALID_DATA, "Invalid gzip header")
        val flags = byte(data, 3); var position = 10
        if (flags and 4 != 0) {
            val size = little(data, position, 2).toInt(); position += 2
            requireSpan(data, position, size); position += size
        }
        for (flag in intArrayOf(8, 16)) {
            if (flags and flag == 0) continue
            do {
                if (position and (BATCH_BYTES - 1) == 0) check?.invoke()
                requireSpan(data, position, 1)
            } while (data[position++].toInt() != 0)
        }
        if (flags and 2 != 0) {
            if (little(data, position, 2) != crc(data, 0, position, check) and 65535L)
                throw Failure(Code.INVALID_DATA, "Invalid gzip header checksum")
            position += 2
        }
        check?.invoke(); return position
    }
)compresskt"
R"compresskt(
    // Read RFC little-bit-endian fields from bounded memory, never one transport call per byte.
    private class Bits(private val data: ByteArray, var position: Int, private val check: (() -> Unit)?) {
        private var value = 0
        private var bits = 0
        private var work = 0
        fun read(count: Int): Int {
            if (++work and 4095 == 0) check?.invoke()
            while (bits < count) {
                if (position == data.size) throw Failure(Code.INVALID_DATA, "Truncated DEFLATE data")
                value = value or (byte(data, position++) shl bits); bits += 8
            }
            val result = value and ((1 shl count) - 1); value = value ushr count; bits -= count
            return result
        }
        // Stored payloads begin on the next byte boundary.
        fun align() { value = 0; bits = 0 }
        // Skip stored data only after its complete span is present.
        fun skip(count: Int) { requireSpan(data, position, count); position += count }
    }
    // Fixed small canonical tables never allocate from payload-controlled code counts.
    private class Huffman {
        private val counts = IntArray(16)
        private val first = IntArray(16)
        private val offsets = IntArray(16)
        private val symbols = IntArray(288)
        private var maximum = 0
        fun build(lengths: IntArray, start: Int, count: Int, codeLengths: Boolean = false, allowEmpty: Boolean = false) {
            counts.fill(0); maximum = 0; var used = 0
            for (index in 0 until count) {
                val length = lengths[start + index]
                if (length !in 0..15) throw Failure(Code.INVALID_DATA, "Invalid DEFLATE code length")
                if (length != 0) { ++counts[length]; ++used; maximum = kotlin.math.max(maximum, length) }
            }
            if (used == 0) {
                if (allowEmpty) return
                throw Failure(Code.INVALID_DATA, "Empty DEFLATE Huffman tree")
            }
            var left = 1
            for (length in 1..15) {
                left = (left shl 1) - counts[length]
                if (left < 0) throw Failure(Code.INVALID_DATA, "Oversubscribed DEFLATE Huffman tree")
            }
            if (left != 0 && (codeLengths || used != 1 || maximum != 1))
                throw Failure(Code.INVALID_DATA, "Incomplete DEFLATE Huffman tree")
            var code = 0; var offset = 0
            for (length in 1..15) {
                code = (code + counts[length - 1]) shl 1; first[length] = code
                offsets[length] = offset; offset += counts[length]
            }
            val next = offsets.copyOf()
            for (symbol in 0 until count) {
                val length = lengths[start + symbol]
                if (length != 0) symbols[next[length]++] = symbol
            }
        }
        // Canonical Huffman code bits arrive most-significant-code-bit first.
        fun decode(bits: Bits): Int {
            var code = 0
            for (length in 1..maximum) {
                code = (code shl 1) or bits.read(1); val index = code - first[length]
                if (index >= 0 && index < counts[length]) return symbols[offsets[length] + index]
            }
            throw Failure(Code.INVALID_DATA, "Invalid DEFLATE Huffman code")
        }
    }
    private val lengthBase = intArrayOf(3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258)
    private val lengthExtra = intArrayOf(0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0)
    private val distanceBase = intArrayOf(1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577)
    private val distanceExtra = intArrayOf(0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13)
    private val codeOrder = intArrayOf(16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15)
    // Fixed alphabets are initialized once and never exposed or mutated afterward.
    private fun fixed(distance: Boolean): Huffman {
        val lengths = IntArray(if (distance) 32 else 288) { if (distance) 5 else if (it < 144) 8 else if (it < 256) 9 else if (it < 280) 7 else 8 }
        return Huffman().also { it.build(lengths, 0, lengths.size) }
    }
    private val fixedLiteral = fixed(false)
    private val fixedDistance = fixed(true)
    private data class Extent(val end: Int, val size: Int)
    // Validate exact extent, tree validity, available history and actual expanded count before allocation.
    private fun extent(data: ByteArray, start: Int, maximum: Int, window: Int, check: (() -> Unit)?): Extent {
        val bits = Bits(data, start, check); val literal = Huffman(); val distance = Huffman(); val codes = Huffman()
        val lengths = IntArray(320); val codeLengths = IntArray(19); var output = 0
        // Reject before output-count overflow, using only actual grammar-derived lengths.
        fun add(count: Int) {
            if (count > maximum - output) throw Failure(Code.RESOURCE_LIMIT, "Expanded output limit exceeded")
            output += count
        }
        var final: Int
        do {
            check?.invoke(); final = bits.read(1); val kind = bits.read(2)
            if (kind == 0) {
                bits.align(); val size = bits.read(16); val complement = bits.read(16)
                if (size xor 65535 != complement) throw Failure(Code.INVALID_DATA, "Invalid DEFLATE stored length")
                bits.skip(size); add(size); continue
            }
            if (kind == 3) throw Failure(Code.INVALID_DATA, "Reserved DEFLATE block type")
            var selectedLiteral = fixedLiteral; var selectedDistance = fixedDistance
            if (kind == 2) {
                val literalCount = bits.read(5) + 257; val distanceCount = bits.read(5) + 1; val codeCount = bits.read(4) + 4
                if (literalCount > 286) throw Failure(Code.INVALID_DATA, "Invalid DEFLATE literal alphabet")
                codeLengths.fill(0)
                for (index in 0 until codeCount) codeLengths[codeOrder[index]] = bits.read(3)
                codes.build(codeLengths, 0, codeLengths.size, true)
                val total = literalCount + distanceCount; var position = 0
                while (position < total) {
                    val symbol = codes.decode(bits)
                    if (symbol < 16) lengths[position++] = symbol
                    else {
                        val count = if (symbol == 16) bits.read(2) + 3 else if (symbol == 17) bits.read(3) + 3 else bits.read(7) + 11
                        if (symbol == 16 && position == 0 || count > total - position)
                            throw Failure(Code.INVALID_DATA, "Invalid DEFLATE code-length repeat")
                        val value = if (symbol == 16) lengths[position - 1] else 0
                        repeat(count) { lengths[position++] = value }
                    }
                }
                if (lengths[256] == 0) throw Failure(Code.INVALID_DATA, "DEFLATE end-of-block code is absent")
                literal.build(lengths, 0, literalCount); distance.build(lengths, literalCount, distanceCount, allowEmpty = true)
                selectedLiteral = literal; selectedDistance = distance
            }
            while (true) {
                val symbol = selectedLiteral.decode(bits)
                if (symbol < 256) { add(1); continue }
                if (symbol == 256) break
                val lengthCode = symbol - 257
                if (lengthCode !in lengthBase.indices) throw Failure(Code.INVALID_DATA, "Reserved DEFLATE length code")
                val count = lengthBase[lengthCode] + bits.read(lengthExtra[lengthCode]); val distanceCode = selectedDistance.decode(bits)
                if (distanceCode !in distanceBase.indices) throw Failure(Code.INVALID_DATA, "Reserved DEFLATE distance code")
                val back = distanceBase[distanceCode] + bits.read(distanceExtra[distanceCode])
                if (back > output || back > window) throw Failure(Code.INVALID_DATA, "DEFLATE distance exceeds available history")
                add(count)
            }
        } while (final == 0)
        check?.invoke(); return Extent(bits.position, output)
    }
)compresskt"
R"compresskt(
    /** Compress complete owning native bytes; standard framing adds no Serializer envelope. */
    fun compress(input: ByteArray, options: EncodeOptions = EncodeOptions()): ByteArray {
        admit(options)
        if (input.size > options.maxInputBytes) throw Failure(Code.RESOURCE_LIMIT, "Serialized input limit exceeded")
        if (options.format == Format.NONE) return copy(input, options.maxOutputBytes, options.checkCancelled)
        if (options.format == Format.ZSTD) return zstdCompress(input, options)
        if (options.format == Format.LZ4) return lz4Compress(input, options)
        val overhead = if (options.format == Format.GZIP) 18 else if (options.format == Format.ZLIB) 6 else 0
        if (options.maxOutputBytes < overhead) throw Failure(Code.RESOURCE_LIMIT, "Compression framing exceeds output limit")
        val output = Buffer(options.maxOutputBytes)
        if (options.format == Format.GZIP) output.write(byteArrayOf(31,-117,8,0,0,0,0,0,0,-1), 0, 10)
        else if (options.format == Format.ZLIB) { output.write(0x78); output.write(0x9c) }
        val provider = java.util.zip.Deflater(options.level, true); val batch = ByteArray(BATCH_BYTES); var position = 0
        try {
            while (position < input.size) {
                options.checkCancelled?.invoke(); val size = kotlin.math.min(BATCH_BYTES, input.size - position)
                provider.setInput(input, position, size); position += size
                while (!provider.needsInput()) {
                    options.checkCancelled?.invoke(); val before = provider.bytesRead
                    val produced = provider.deflate(batch); options.checkCancelled?.invoke(); output.write(batch, 0, produced)
                    if (produced == 0 && provider.bytesRead == before && !provider.needsInput())
                        throw Failure(Code.INVALID_DATA, "Compression provider made no progress")
                }
            }
            provider.finish()
            while (!provider.finished()) {
                options.checkCancelled?.invoke(); val produced = provider.deflate(batch)
                options.checkCancelled?.invoke(); output.write(batch, 0, produced)
                if (produced == 0 && !provider.finished()) throw Failure(Code.INVALID_DATA, "Compression provider made no progress")
            }
        } finally { provider.end() }
        if (options.format == Format.GZIP) { writeLittle(output, crc(input, 0, input.size, options.checkCancelled)); writeLittle(output, input.size.toLong()) }
        else if (options.format == Format.ZLIB) {
            val checksum = adler(input, options.checkCancelled)
            for (shift in 24 downTo 0 step 8) output.write((checksum ushr shift).toInt())
        }
        options.checkCancelled?.invoke(); return output.toByteArray()
    }
    /** Validate one exact frame before allocating expanded bytes or invoking native model decoding. */
    fun decompress(input: ByteArray, options: DecodeOptions = DecodeOptions()): ByteArray {
        admit(options)
        if (input.size > options.maxInputBytes) throw Failure(Code.RESOURCE_LIMIT, "Compressed input limit exceeded")
        if (options.format == Format.NONE) return copy(input, options.maxOutputBytes, options.checkCancelled)
        if (options.format == Format.ZSTD) return zstdDecompress(input, options)
        if (options.format == Format.LZ4) return lz4Decompress(input, options)
        var start = 0; var trailer = 0; var window = DEFLATE_WINDOW_BYTES
        if (options.format == Format.GZIP) { start = gzipStart(input, options.checkCancelled); trailer = 8 }
        else if (options.format == Format.ZLIB) {
            requireSpan(input, 0, 2); val cmf = byte(input, 0); val flags = byte(input, 1)
            if (cmf and 15 != 8 || cmf shr 4 > 7 || ((cmf shl 8) or flags) % 31 != 0 || flags and 32 != 0)
                throw Failure(Code.INVALID_DATA, "Invalid or dictionary-dependent zlib header")
            window = 1 shl ((cmf shr 4) + 8); start = 2; trailer = 4
        }
        val measured = extent(input, start, options.maxOutputBytes, window, options.checkCancelled)
        requireSpan(input, measured.end, trailer)
        if (input.size - measured.end != trailer) throw Failure(Code.INVALID_DATA, "Trailing or concatenated compression data")
        options.checkCancelled?.invoke(); val output = ByteArray(measured.size); val batch = ByteArray(BATCH_BYTES)
        val provider = java.util.zip.Inflater(true); var position = start; var written = 0; var dummy = false
        try {
            while (!provider.finished()) {
                options.checkCancelled?.invoke()
                if (provider.needsInput()) {
                    if (position < measured.end) {
                        val size = kotlin.math.min(BATCH_BYTES, measured.end - position)
                        provider.setInput(input, position, size); position += size
                    } else if (!dummy) { provider.setInput(ByteArray(1)); dummy = true }
                    else throw Failure(Code.INVALID_DATA, "Truncated expanded data")
                }
                val before = provider.bytesRead
                val count = try { provider.inflate(batch, 0, kotlin.math.min(BATCH_BYTES.toLong(), measured.size.toLong() - written + 1).toInt()) }
                    catch (_: java.util.zip.DataFormatException) { throw Failure(Code.INVALID_DATA, "Invalid DEFLATE provider data") }
                options.checkCancelled?.invoke()
                if (count > measured.size - written) throw Failure(Code.INVALID_DATA, "Unexpected expanded byte count")
                java.lang.System.arraycopy(batch, 0, output, written, count); written += count
                if (provider.needsDictionary()) throw Failure(Code.INVALID_DATA, "Compression dictionaries are unsupported")
                if (count == 0 && provider.bytesRead == before && !provider.needsInput() && !provider.finished())
                    throw Failure(Code.INVALID_DATA, "Compression provider made no progress")
            }
        } finally { provider.end() }
        if (written != measured.size) throw Failure(Code.INVALID_DATA, "Unexpected expanded byte count")
        if (options.format == Format.GZIP) {
            if (little(input, measured.end, 4) != crc(output, 0, output.size, options.checkCancelled) || little(input, measured.end + 4, 4) != output.size.toLong())
                throw Failure(Code.INVALID_DATA, "Invalid gzip trailer")
        } else if (options.format == Format.ZLIB) {
            var checksum = 0L
            for (index in 0 until 4) checksum = (checksum shl 8) or byte(input, measured.end + index).toLong()
            if (checksum != adler(output, options.checkCancelled)) throw Failure(Code.INVALID_DATA, "Invalid zlib checksum")
        }
        options.checkCancelled?.invoke(); return output
    }
    /** Read a borrowed EOF-delimited source with bounded batches and a one-byte overflow probe. */
    fun decompress(source: java.io.InputStream, options: DecodeOptions = DecodeOptions()): ByteArray {
        admit(options); val staged = Buffer(options.maxInputBytes); val batch = ByteArray(BATCH_BYTES)
        while (true) {
            options.checkCancelled?.invoke()
            val requested = kotlin.math.min(BATCH_BYTES.toLong(), options.maxInputBytes.toLong() - staged.size() + 1).toInt()
            val count = source.read(batch, 0, requested); options.checkCancelled?.invoke()
            if (count == -1) break
            if (count <= 0 || count > requested) throw java.io.IOException("Invalid borrowed source read count")
            staged.write(batch, 0, count)
        }
        options.checkCancelled?.invoke(); return decompress(staged.toByteArray(), options)
    }
    /** Admit compression before invoking a complete owning encoder callback. */
    fun encode(encoder: () -> ByteArray, options: EncodeOptions = EncodeOptions()): ByteArray {
        admit(options); val plain = encoder(); options.checkCancelled?.invoke(); return compress(plain, options)
    }
    /** Publish a fresh candidate only after exact framing, native decoding and final cancellation. */
    fun <T> decode(input: ByteArray, decoder: (ByteArray) -> T, options: DecodeOptions = DecodeOptions()): T {
        admit(options); val plain = decompress(input, options); val candidate = decoder(plain)
        options.checkCancelled?.invoke(); return candidate
    }
)compresskt"
R"compresskt(
    // Preserve the first failure across synchronous callback reentry, close and later callback exceptions.
    private class Lifecycle(private val check: (() -> Unit)?) {
        private var failed = false
        private var finished = false
        private var closed = false
        private var busy = false
        private var failure: kotlin.Throwable? = null
        fun begin() {
            if (failed || finished || closed || busy) {
                val cause = java.lang.IllegalStateException("Compression wrapper is busy, finished, closed or failed")
                if (busy && !failed) { failed = true; failure = cause }
                throw cause
            }
            busy = true
        }
        // A swallowed nested-operation error still aborts its active outer operation.
        fun active() {
            if (failed) throw failure!!
            if (finished || closed) throw java.lang.IllegalStateException("Compression wrapper is finished or closed")
        }
        // Policy callbacks are external boundaries even when they return normally.
        fun poll() { active(); check?.invoke(); active() }
        fun fail(cause: kotlin.Throwable): kotlin.Throwable {
            if (!failed) { failed = true; failure = cause }
            return failure!!
        }
        fun end() { busy = false }
        fun finish() { finished = true }
        fun close() { closed = true }
    }
    /** Stage one message; finish is explicit, and close never finishes or closes/flushes the borrowed sink. */
    class Output(private val sink: java.io.OutputStream, private val options: EncodeOptions = EncodeOptions()) : java.io.OutputStream() {
        private val state: Lifecycle
        private val runOptions: EncodeOptions
        private var staged: Buffer?
        init {
            admit(options); state = Lifecycle(options.checkCancelled)
            runOptions = options.copy(checkCancelled = state::poll); staged = Buffer(options.maxInputBytes)
        }
        // Stage a single byte through the same failure and admission rules.
        override fun write(value: Int) {
            state.begin()
            try { state.poll(); staged!!.write(value); state.poll() }
            catch (cause: kotlin.Throwable) { staged = null; throw state.fail(cause) }
            finally { state.end() }
        }
        // Stage codec bytes without publishing any compressed prefix.
        override fun write(data: ByteArray, offset: Int, length: Int) {
            state.begin()
            try {
                java.util.Objects.checkFromIndexSize(offset, length, data.size); state.poll()
                if (length > options.maxInputBytes - staged!!.size()) throw Failure(Code.RESOURCE_LIMIT, "Serialized input limit exceeded")
                var position = 0
                while (position < length) {
                    state.poll(); val size = kotlin.math.min(BATCH_BYTES, length - position)
                    staged!!.write(data, offset + position, size); position += size
                }
                state.poll()
            } catch (cause: kotlin.Throwable) { staged = null; throw state.fail(cause) }
            finally { state.end() }
        }
        /** Publish bounded batches after validation; an external failure may retain a prefix. */
        fun finish() {
            state.begin()
            try {
                state.poll(); val wire = compress(staged!!.toByteArray(), runOptions); state.poll(); var offset = 0
                while (offset < wire.size) {
                    state.poll(); val size = kotlin.math.min(BATCH_BYTES, wire.size - offset)
                    sink.write(wire, offset, size); offset += size; state.poll()
                }
                state.poll(); state.finish(); staged = null
            } catch (cause: kotlin.Throwable) { staged = null; throw state.fail(cause) }
            finally { state.end() }
        }
        // Flush performs policy checks but never publishes, finishes or forwards a borrowed flush.
        override fun flush() {
            state.begin()
            try { state.poll() } catch (cause: kotlin.Throwable) { staged = null; throw state.fail(cause) }
            finally { state.end() }
        }
        override fun close() { state.close(); staged = null }
    }
    /** Validate borrowed source EOF before exposing bytes; finish requires complete expanded consumption. */
    class Input(private val source: java.io.InputStream, private val options: DecodeOptions = DecodeOptions()) : java.io.InputStream() {
        private val state: Lifecycle
        private val runOptions: DecodeOptions
        private var staged: ByteArray? = null
        private var position = 0
        init { admit(options); state = Lifecycle(options.checkCancelled); runOptions = options.copy(checkCancelled = state::poll) }
        // Retain the original immutable cancellation policy after staging completes.
        private fun prepare() {
            state.poll(); if (staged == null) staged = decompress(source, runOptions); state.poll()
        }
        override fun read(): Int {
            state.begin()
            try { prepare(); val result = if (position == staged!!.size) -1 else byte(staged!!, position++); state.poll(); return result }
            catch (cause: kotlin.Throwable) { staged = null; throw state.fail(cause) }
            finally { state.end() }
        }
        // Return bounded short reads; zero-length requests perform no borrowed source I/O.
        override fun read(destination: ByteArray, offset: Int, length: Int): Int {
            state.begin()
            try {
                java.util.Objects.checkFromIndexSize(offset, length, destination.size); state.poll()
                if (length == 0) return 0
                prepare()
                if (position == staged!!.size) { state.poll(); return -1 }
                val count = kotlin.math.min(kotlin.math.min(length, BATCH_BYTES), staged!!.size - position)
                java.lang.System.arraycopy(staged!!, position, destination, offset, count); state.poll(); position += count; return count
            } catch (cause: kotlin.Throwable) { staged = null; throw state.fail(cause) }
            finally { state.end() }
        }
        /** Require exact expanded consumption and permanently fence unread-message failures. */
        fun finish() {
            state.begin()
            try {
                prepare()
                if (position != staged!!.size) throw Failure(Code.INVALID_DATA, "Unread expanded compression data")
                state.poll(); state.finish(); staged = null
            } catch (cause: kotlin.Throwable) { staged = null; throw state.fail(cause) }
            finally { state.end() }
        }
        override fun close() { state.close(); staged = null }
    }
}
)compresskt"
