// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compresskt(
    private const val LZ4_HISTORY_BYTES = 64 * 1024
    private const val LZ4_BLOCK_BYTES = 64 * 1024
    private const val LZ4_MAXIMUM_BLOCK_BYTES = 4 * 1024 * 1024
    private val lz4Lock = kotlin.Any()
    private var lz4State: Lz4Provider? = null
    // Resolve only safe pure-Java block/checksum APIs; no native library is required.
    private fun lz4Provider(): Lz4Provider = synchronized(lz4Lock) {
        lz4State ?: try { Lz4Provider().also { lz4State = it } }
        catch (_: java.lang.ReflectiveOperationException) { throw Failure(Code.UNAVAILABLE, "LZ4 JVM provider is unavailable") }
        catch (_: java.lang.NoClassDefFoundError) { throw Failure(Code.UNAVAILABLE, "LZ4 JVM provider is unavailable") }
    }
    // Cache immutable reflective handles while every content-hash context remains operation-owned.
    private class Lz4Provider {
        private val loader = @compression_name@::class.java.classLoader
        private val factoryType = java.lang.Class.forName("net.jpountz.lz4.LZ4Factory", true, loader)
        private val hashFactory = java.lang.Class.forName("net.jpountz.xxhash.XXHashFactory", true, loader)
        private val compressor = java.lang.Class.forName("net.jpountz.lz4.LZ4Compressor", false, loader)
        private val safe = java.lang.Class.forName("net.jpountz.lz4.LZ4SafeDecompressor", false, loader)
        private val hashType = java.lang.Class.forName("net.jpountz.xxhash.XXHash32", false, loader)
        private val streamingHash = java.lang.Class.forName("net.jpountz.xxhash.StreamingXXHash32", false, loader)
        val malformed: java.lang.Class<*> = java.lang.Class.forName("net.jpountz.lz4.LZ4Exception", false, loader)
        val factory: kotlin.Any? = providerInvoke(factoryType.getMethod("safeInstance"), null)
        val hashes: kotlin.Any? = providerInvoke(hashFactory.getMethod("safeInstance"), null)
        val decompressor: kotlin.Any? = providerInvoke(factoryType.getMethod("safeDecompressor"), factory)
        val hash: kotlin.Any? = providerInvoke(hashFactory.getMethod("hash32"), hashes)
        val fast: java.lang.reflect.Method = factoryType.getMethod("fastCompressor")
        val high: java.lang.reflect.Method = factoryType.getMethod("highCompressor", java.lang.Integer.TYPE)
        val bound: java.lang.reflect.Method = compressor.getMethod("maxCompressedLength", java.lang.Integer.TYPE)
        val compress: java.lang.reflect.Method = compressor.getMethod("compress", ByteArray::class.java, java.lang.Integer.TYPE, java.lang.Integer.TYPE, ByteArray::class.java, java.lang.Integer.TYPE, java.lang.Integer.TYPE)
        val decompress: java.lang.reflect.Method = safe.getMethod("decompress", ByteArray::class.java, java.lang.Integer.TYPE, java.lang.Integer.TYPE, ByteArray::class.java, java.lang.Integer.TYPE, java.lang.Integer.TYPE)
        val hash32: java.lang.reflect.Method = hashType.getMethod("hash", ByteArray::class.java, java.lang.Integer.TYPE, java.lang.Integer.TYPE, java.lang.Integer.TYPE)
        val newHash: java.lang.reflect.Method = hashFactory.getMethod("newStreamingHash32", java.lang.Integer.TYPE)
        val updateHash: java.lang.reflect.Method = streamingHash.getMethod("update", ByteArray::class.java, java.lang.Integer.TYPE, java.lang.Integer.TYPE)
        val hashValue: java.lang.reflect.Method = streamingHash.getMethod("getValue")
        init { if (!java.io.Closeable::class.java.isAssignableFrom(streamingHash)) throw java.lang.NoSuchMethodException("LZ4 checksum cleanup unavailable") }
        // Hash one bounded standard descriptor or admitted block using the required zero seed.
        fun hash(input: ByteArray, offset: Int, count: Int): Int = providerInvoke(hash32, hash, input, offset, count, 0) as Int
        // Poll between bounded content-hash updates independently of the block decoder's bound.
        fun update(context: kotlin.Any, input: ByteArray, offset: Int, count: Int, policy: ProviderCheck) {
            var position = 0
            while (position < count) { policy.poll(); val size = kotlin.math.min(BATCH_BYTES, count - position); providerInvoke(updateHash, context, input, offset + position, size); policy.poll(); position += size }
        }
    }
    // Encode one checked standard independent-block frame with no custom envelope.
    private fun lz4Compress(input: ByteArray, options: EncodeOptions): ByteArray {
        val state = lz4Provider(); val policy = ProviderCheck(options.checkCancelled); val output = Buffer(options.maxOutputBytes)
        var content: kotlin.Any? = null; var primary: kotlin.Throwable? = null
        try {
            policy.poll(); val compressor = if (options.level == 0) providerInvoke(state.fast, state.factory) else providerInvoke(state.high, state.factory, options.level)
            val capacity = providerInvoke(state.bound, compressor, LZ4_BLOCK_BYTES) as Int
            val batch = ByteArray(capacity); val header = byteArrayOf(4, 34, 77, 24, 100, 64, 0)
            policy.poll(); header[6] = (state.hash(header, 4, 2) ushr 8).toByte(); output.write(header, 0, header.size)
            content = providerInvoke(state.newHash, state.hashes, 0)
            var offset = 0
            while (offset < input.size) {
                policy.poll(); val count = kotlin.math.min(LZ4_BLOCK_BYTES, input.size - offset)
                val produced = providerInvoke(state.compress, compressor, input, offset, count, batch, 0, batch.size) as Int
                policy.poll(); if (produced <= 0 || produced > batch.size) throw Failure(Code.INVALID_DATA, "LZ4 provider made no progress")
                if (produced < count) { writeLittle(output, produced.toLong()); output.write(batch, 0, produced) }
                else { writeLittle(output, (count or java.lang.Integer.MIN_VALUE).toLong()); output.write(input, offset, count) }
                state.update(content!!, input, offset, count, policy); offset += count
            }
            writeLittle(output, 0); policy.poll(); writeLittle(output, (providerInvoke(state.hashValue, content) as Int).toLong())
            policy.poll(); return output.toByteArray()
        } catch (failure: kotlin.Throwable) { val observed = policy.first ?: failure; primary = observed; throw observed }
        finally { content?.let { owned -> providerClose(primary) { (owned as java.io.Closeable).close() } } }
    }
    // Parse extension lengths without overflow before materializing regenerated block storage.
    private fun lz4Length(input: ByteArray, cursor: IntArray, initial: Int, end: Int, maximum: Int, policy: ProviderCheck): Int {
        var length = initial
        if (initial == 15) {
            do {
                if (cursor[0] >= end) throw Failure(Code.INVALID_DATA, "Truncated LZ4 length")
                if (cursor[0] % BATCH_BYTES == 0) policy.poll()
                val extension = byte(input, cursor[0]++)
                if (extension > maximum - length) throw Failure(Code.INVALID_DATA, "Invalid LZ4 block length")
                length += extension
            } while (extension == 255)
        }
        return length
    }
    // Count exact expanded bytes and check every back reference before allocating block output.
    private fun lz4BlockSize(input: ByteArray, offset: Int, length: Int, history: Int, maximum: Int, policy: ProviderCheck): Int {
        val cursor = intArrayOf(offset); val end = offset + length; var produced = 0; var nextPoll = offset
        while (cursor[0] < end) {
            if (cursor[0] >= nextPoll) { policy.poll(); nextPoll = cursor[0] + BATCH_BYTES }
            val token = byte(input, cursor[0]++); val literals = lz4Length(input, cursor, token ushr 4, end, maximum, policy)
            if (literals > end - cursor[0] || literals > maximum - produced) throw Failure(Code.INVALID_DATA, "Invalid LZ4 literals")
            cursor[0] += literals; produced += literals; if (cursor[0] == end) return produced
            if (end - cursor[0] < 2) throw Failure(Code.INVALID_DATA, "Truncated LZ4 match")
            val distance = little(input, cursor[0], 2).toInt(); cursor[0] += 2
            if (distance == 0 || distance > produced + history) throw Failure(Code.INVALID_DATA, "Invalid LZ4 match distance")
            val match = lz4Length(input, cursor, token and 15, end, maximum, policy)
            if (maximum - produced < 4 || match > maximum - produced - 4) throw Failure(Code.INVALID_DATA, "Invalid LZ4 match length")
            produced += match + 4
        }
        throw Failure(Code.INVALID_DATA, "Missing LZ4 final literals")
    }
    // Merge <=64KiB history into the first literal sequence; original match offsets retain meaning.
    private fun lz4Prefix(input: ByteArray, offset: Int, length: Int, history: ByteArray, policy: ProviderCheck): ByteArray {
        val token = byte(input, offset); val cursor = intArrayOf(offset + 1)
        val literal = lz4Length(input, cursor, token ushr 4, offset + length, LZ4_MAXIMUM_BLOCK_BYTES, policy)
        val augmented = history.size + literal; val extensionBytes = if (augmented < 15) 0 else (augmented - 15) / 255 + 1
        val remainder = length - (cursor[0] - offset); val result = ByteArray(1 + extensionBytes + history.size + remainder)
        var position = 0; result[position++] = ((kotlin.math.min(15, augmented) shl 4) or (token and 15)).toByte()
        if (augmented >= 15) { var extension = augmented - 15; while (extension >= 255) { result[position++] = 255.toByte(); extension -= 255 }; result[position++] = extension.toByte() }
        java.lang.System.arraycopy(history, 0, result, position, history.size); position += history.size
        java.lang.System.arraycopy(input, cursor[0], result, position, remainder); policy.poll(); return result
    }
    // Retain exactly the final dictionary window, independently of total message size.
    private fun lz4History(history: ByteArray, block: ByteArray, offset: Int, length: Int): ByteArray {
        val count = kotlin.math.min(LZ4_HISTORY_BYTES, history.size + length); val result = ByteArray(count); val fromBlock = kotlin.math.min(count, length)
        if (count > fromBlock) java.lang.System.arraycopy(history, history.size - (count - fromBlock), result, 0, count - fromBlock)
        java.lang.System.arraycopy(block, offset + length - fromBlock, result, count - fromBlock, fromBlock); return result
    }
    // Validate standard descriptor/block/content checksums, exact frame extent and linked history.
    private fun lz4Decompress(input: ByteArray, options: DecodeOptions): ByteArray {
        val state = lz4Provider(); val policy = ProviderCheck(options.checkCancelled); var content: kotlin.Any? = null; var primary: kotlin.Throwable? = null
        try {
            if (little(input, 0, 4) != 0x184d2204L) throw Failure(Code.INVALID_DATA, "Expected one standard LZ4 frame")
            requireSpan(input, 4, 2); val flags = byte(input, 4); val descriptor = byte(input, 5)
            if (flags ushr 6 != 1 || flags and 2 != 0 || descriptor and 143 != 0) throw Failure(Code.INVALID_DATA, "Invalid LZ4 frame descriptor")
            if (flags and 1 != 0) throw Failure(Code.INVALID_DATA, "LZ4 dictionaries are unsupported")
            val blockId = (descriptor ushr 4) and 7
            if (blockId !in 4..7) throw Failure(Code.INVALID_DATA, "Invalid LZ4 block maximum")
            val maximum = 1 shl (8 + 2 * blockId); var offset = 6; var expected = -1L
            if (flags and 8 != 0) { expected = little(input, offset, 8); offset += 8; if (expected < 0 || expected > options.maxOutputBytes) throw Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded") }
            requireSpan(input, offset, 1); policy.poll()
            if (byte(input, offset) != ((state.hash(input, 4, offset - 4) ushr 8) and 255)) throw Failure(Code.INVALID_DATA, "Invalid LZ4 descriptor checksum")
            ++offset; if (maximum > options.maxWindowBytes) throw Failure(Code.RESOURCE_LIMIT, "Compression history limit exceeded")
            if (flags and 4 != 0) content = providerInvoke(state.newHash, state.hashes, 0)
            val output = Buffer(options.maxOutputBytes); var history = ByteArray(0)
            while (true) {
                policy.poll(); val block = little(input, offset, 4); offset += 4; if (block == 0L) break
                val raw = block and 0x80000000L != 0L; val count = (block and 0x7fffffffL).toInt()
                if (count > maximum) throw Failure(Code.INVALID_DATA, "Invalid LZ4 block size"); requireSpan(input, offset, count)
                if (flags and 16 != 0) { val checksum = little(input, offset + count, 4); policy.poll(); if (checksum != (state.hash(input, offset, count).toLong() and 0xffffffffL)) throw Failure(Code.INVALID_DATA, "Invalid LZ4 block checksum") }
                var plain = input; var plainOffset = offset; var plainCount = count
                if (!raw) {
                    plainCount = lz4BlockSize(input, offset, count, history.size, maximum, policy)
                    if (plainCount > options.maxOutputBytes - output.size()) throw Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded")
                    var packed = input; var packedOffset = offset; var packedCount = count
                    if (history.isNotEmpty()) { packed = lz4Prefix(input, offset, count, history, policy); packedOffset = 0; packedCount = packed.size }
                    plain = ByteArray(history.size + plainCount); plainOffset = history.size; policy.poll()
                    val decoded = try { providerInvoke(state.decompress, state.decompressor, packed, packedOffset, packedCount, plain, 0, plain.size) as Int }
                    catch (failure: kotlin.Throwable) { if (state.malformed.isInstance(failure)) throw Failure(Code.INVALID_DATA, "Invalid LZ4 block"); throw failure }
                    if (decoded != plain.size) throw Failure(Code.INVALID_DATA, "Invalid LZ4 regenerated block size")
                }
                policy.poll(); output.write(plain, plainOffset, plainCount); content?.let { state.update(it, plain, plainOffset, plainCount, policy) }
                if (flags and 32 == 0) history = lz4History(history, plain, plainOffset, plainCount)
                offset += count + if (flags and 16 != 0) 4 else 0
            }
            content?.let { val checksum = little(input, offset, 4); offset += 4; policy.poll(); if (checksum != ((providerInvoke(state.hashValue, it) as Int).toLong() and 0xffffffffL)) throw Failure(Code.INVALID_DATA, "Invalid LZ4 content checksum") }
            if (offset != input.size) throw Failure(Code.INVALID_DATA, "Trailing LZ4 input")
            if (expected != -1L && expected != output.size().toLong()) throw Failure(Code.INVALID_DATA, "Invalid LZ4 content size")
            policy.poll(); return output.toByteArray()
        } catch (failure: kotlin.Throwable) { val observed = policy.first ?: failure; primary = observed; throw observed }
        finally { content?.let { owned -> providerClose(primary) { (owned as java.io.Closeable).close() } } }
    }

)compresskt"
