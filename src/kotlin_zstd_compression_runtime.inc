// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compresskt(
    // Preserve callback provenance even when a native provider catches or wraps a checked exception.
    private class ProviderCheck(private val callback: (() -> Unit)?) {
        var first: kotlin.Throwable? = null
            private set
        // Reject retained callback failure before and after another policy invocation.
        fun poll() {
            first?.let { throw it }
            try { callback?.invoke() } catch (failure: kotlin.Throwable) { first = failure; throw failure }
            first?.let { throw it }
        }
    }
    // Invoke public reflected methods without replacing provider/application exception objects.
    private fun providerInvoke(method: java.lang.reflect.Method, target: kotlin.Any?, vararg arguments: kotlin.Any?): kotlin.Any? {
        try { return method.invoke(target, *arguments) }
        catch (failure: java.lang.reflect.InvocationTargetException) { throw (failure.cause ?: failure) }
        catch (failure: java.lang.IllegalAccessException) { throw java.lang.IllegalStateException("Inaccessible compression provider API", failure) }
    }
    // Construct one owned provider and unwrap only the reflection transport layer.
    private fun providerConstruct(constructor: java.lang.reflect.Constructor<*>, vararg arguments: kotlin.Any?): kotlin.Any {
        try { return constructor.newInstance(*arguments) }
        catch (failure: java.lang.reflect.InvocationTargetException) { throw (failure.cause ?: failure) }
        catch (failure: java.lang.ReflectiveOperationException) { throw java.lang.IllegalStateException("Inaccessible compression provider constructor", failure) }
    }
    // Cache immutable capability handles; mutable native contexts remain per operation.
    private val zstdLock = kotlin.Any()
    private var zstdState: ZstdProvider? = null
    // Resolve optional deployment once without caching a mutable compression/decompression context.
    private fun zstdProvider(): ZstdProvider = synchronized(zstdLock) {
        zstdState ?: try { ZstdProvider().also { zstdState = it } }
        catch (_: java.lang.ReflectiveOperationException) { throw Failure(Code.UNAVAILABLE, "Zstandard JVM provider is unavailable") }
        catch (_: java.lang.UnsatisfiedLinkError) { throw Failure(Code.UNAVAILABLE, "Zstandard native deployment is unavailable") }
        catch (_: java.lang.NoClassDefFoundError) { throw Failure(Code.UNAVAILABLE, "Zstandard JVM provider is unavailable") }
    }
    // Resolve the pinned public API and native deployment before invoking cancellation or borrowed I/O.
    private class ZstdProvider {
        private val loader = @compression_name@::class.java.classLoader
        private val api = java.lang.Class.forName("com.github.luben.zstd.Zstd", true, loader)
        private val output = java.lang.Class.forName("com.github.luben.zstd.ZstdCompressCtx", true, loader)
        private val input = java.lang.Class.forName("com.github.luben.zstd.ZstdInputStreamNoFinalizer", true, loader)
        private val directive = java.lang.Class.forName("com.github.luben.zstd.EndDirective", true, loader)
        val dataFailure: java.lang.Class<*> = java.lang.Class.forName("com.github.luben.zstd.ZstdIOException", false, loader)
        val encoder: java.lang.reflect.Constructor<*> = output.getConstructor()
        val decoder: java.lang.reflect.Constructor<*> = input.getConstructor(java.io.InputStream::class.java)
        val checksum: java.lang.reflect.Method = output.getMethod("setChecksum", java.lang.Boolean.TYPE)
        val level: java.lang.reflect.Method = output.getMethod("setLevel", java.lang.Integer.TYPE)
        val pledge: java.lang.reflect.Method = output.getMethod("setPledgedSrcSize", java.lang.Long.TYPE)
        val stream: java.lang.reflect.Method = output.getMethod("compressByteBufferStream", java.nio.ByteBuffer::class.java, java.nio.ByteBuffer::class.java, directive)
        val encoderClose: java.lang.reflect.Method = output.getMethod("close")
        val continueDirective: kotlin.Any = directive.getField("CONTINUE").get(null)
        val endDirective: kotlin.Any = directive.getField("END").get(null)
        val window: java.lang.reflect.Method = input.getMethod("setLongMax", java.lang.Integer.TYPE)
        val errorCode: java.lang.reflect.Method = dataFailure.getMethod("getErrorCode")
        val minimumLevel: Int = providerInvoke(api.getMethod("minCompressionLevel"), null) as Int
        val maximumLevel: Int = providerInvoke(api.getMethod("maxCompressionLevel"), null) as Int
        val windowFailure: Long = providerInvoke(api.getMethod("errFrameParameterWindowTooLarge"), null) as Long
        val malformed: kotlin.collections.Set<Long> = kotlin.arrayOf("errPrefixUnknown", "errVersionUnsupported", "errFrameParameterUnsupported",
            "errCorruptionDetected", "errChecksumWrong", "errDictionaryCorrupted", "errDictionaryWrong", "errSrcSizeWrong",
            "errTableLogTooLarge", "errMaxSymbolValueTooLarge", "errMaxSymbolValueTooSmall").map { providerInvoke(api.getMethod(it), null) as Long }.toSet()
        init { if (!java.io.InputStream::class.java.isAssignableFrom(input)) throw java.lang.NoSuchMethodException("Incompatible Zstandard stream API") }
        // Accept the full advertised compression level interval, including negative fast levels.
        fun validateLevel(value: Int) { if (value < minimumLevel || value > maximumLevel) throw Failure(Code.INVALID_OPTIONS, "Invalid Zstandard compression level") }
        // Map only known frame/window diagnostics, preserving unrelated provider failures exactly.
        fun failure(observed: java.io.IOException): kotlin.Throwable {
            if (dataFailure.isInstance(observed)) {
                val code = providerInvoke(errorCode, observed) as Long
                if (code == windowFailure) return Failure(Code.RESOURCE_LIMIT, "Compression history limit exceeded")
                if (code in malformed) return Failure(Code.INVALID_DATA, "Invalid Zstandard frame")
            }
            return observed
        }
    }
    // Limit each owned provider refill independently of complete staged input length.
    private class ZstdSource(bytes: ByteArray, private val policy: ProviderCheck) : java.io.ByteArrayInputStream(bytes) {
        // Limit provider bulk refills while preserving callback failures and true EOF.
        override fun read(bytes: ByteArray, offset: Int, length: Int): Int { policy.poll(); val count = super.read(bytes, offset, kotlin.math.min(length, BATCH_BYTES)); policy.poll(); return count }
        // Poll even when the provider reads an individual header byte.
        override fun read(): Int { policy.poll(); val value = super.read(); policy.poll(); return value }
    }
    // Bound provider output while retaining a one-byte exact-limit overflow probe.
    private fun outputCapacity(size: Int, maximum: Int): Int = kotlin.math.min(BATCH_BYTES.toLong(), maximum.toLong() - size + 1).toInt()
    // Validate one complete standard frame and its conservative history policy before native expansion.
    private fun zstdFrame(input: ByteArray, options: DecodeOptions, policy: ProviderCheck) {
        if (little(input, 0, 4) != 0xfd2fb528L) throw Failure(Code.INVALID_DATA, "Expected one standard Zstandard frame")
        requireSpan(input, 4, 1); val descriptor = byte(input, 4)
        if (descriptor and 8 != 0) throw Failure(Code.INVALID_DATA, "Reserved Zstandard frame descriptor")
        val single = descriptor and 32 != 0; var offset = 5; var history = 0L
        if (!single) { requireSpan(input, offset, 1); val window = byte(input, offset++); val base = 1L shl (10 + (window ushr 3)); history = base + (base ushr 3) * (window and 7) }
        val dictionaryBytes = if (descriptor and 3 == 3) 4 else descriptor and 3
        if (little(input, offset, dictionaryBytes) != 0L) throw Failure(Code.INVALID_DATA, "Zstandard dictionaries are unsupported")
        offset += dictionaryBytes; val sizeFlag = descriptor ushr 6
        val sizeBytes = if (sizeFlag == 0) { if (single) 1 else 0 } else 1 shl sizeFlag
        var contentSize = if (sizeBytes == 0) -1L else little(input, offset, sizeBytes)
        if (sizeBytes == 2) contentSize += 256
        if (sizeBytes != 0 && (contentSize < 0 || contentSize > options.maxOutputBytes)) throw Failure(Code.RESOURCE_LIMIT, "Compression byte limit exceeded")
        offset += sizeBytes; if (single) history = contentSize
        val effective = java.lang.Integer.highestOneBit(options.maxWindowBytes).toLong()
        if (history > effective) throw Failure(Code.RESOURCE_LIMIT, "Compression history limit exceeded")
        val maximumBlock = kotlin.math.min(128L * 1024, history)
        do {
            policy.poll(); val block = little(input, offset, 3).toInt(); offset += 3
            val last = block and 1 != 0; val type = (block ushr 1) and 3; val count = block ushr 3
            if (type == 3 || count > maximumBlock) throw Failure(Code.INVALID_DATA, "Invalid Zstandard block")
            val stored = if (type == 1) 1 else count; requireSpan(input, offset, stored); offset += stored
        } while (!last)
        if (descriptor and 4 != 0) { requireSpan(input, offset, 4); offset += 4 }
        if (offset != input.size) throw Failure(Code.INVALID_DATA, "Trailing Zstandard input")
        policy.poll()
    }
    // Preserve a primary failure while releasing one privately owned native/stream context.
    private fun providerClose(primary: kotlin.Throwable?, close: () -> Unit) {
        try { close() } catch (failure: kotlin.Throwable) { if (primary != null) { if (failure !== primary) primary.addSuppressed(failure) } else throw failure }
    }
    // Encode a checksummed standard frame with pledged size and bounded input/output provider calls.
    private fun zstdCompress(input: ByteArray, options: EncodeOptions): ByteArray {
        val state = zstdProvider(); val output = Buffer(options.maxOutputBytes); val policy = ProviderCheck(options.checkCancelled)
        var provider: kotlin.Any? = null; var primary: kotlin.Throwable? = null
        try {
            provider = providerConstruct(state.encoder); providerInvoke(state.level, provider, options.level)
            providerInvoke(state.checksum, provider, true); providerInvoke(state.pledge, provider, input.size.toLong())
            val batch = ByteArray(BATCH_BYTES); var offset = 0
            while (offset < input.size) {
                val count = kotlin.math.min(BATCH_BYTES, input.size - offset); val source = java.nio.ByteBuffer.wrap(input, offset, count)
                while (source.hasRemaining()) {
                    val destination = java.nio.ByteBuffer.wrap(batch, 0, outputCapacity(output.size(), options.maxOutputBytes)); val before = source.position()
                    policy.poll(); providerInvoke(state.stream, provider, destination, source, state.continueDirective); policy.poll(); output.write(batch, 0, destination.position())
                    if (source.position() == before && destination.position() == 0) throw Failure(Code.INVALID_DATA, "Compression provider made no progress")
                }
                offset += count
            }
            do {
                val destination = java.nio.ByteBuffer.wrap(batch, 0, outputCapacity(output.size(), options.maxOutputBytes))
                policy.poll(); val finished = providerInvoke(state.stream, provider, destination, java.nio.ByteBuffer.allocate(0), state.endDirective) as Boolean
                policy.poll(); output.write(batch, 0, destination.position())
                if (!finished && destination.position() == 0) throw Failure(Code.INVALID_DATA, "Compression provider made no progress")
            } while (!finished)
        } catch (failure: kotlin.Throwable) {
            val observed = policy.first ?: if (failure is java.io.IOException) state.failure(failure) else failure
            primary = observed; throw observed
        } finally { provider?.let { owned -> providerClose(primary) { providerInvoke(state.encoderClose, owned) } } }
        policy.poll(); return output.toByteArray()
    }
    // Apply the independently checked history ceiling to an owned native decompression stream.
    private fun zstdDecompress(input: ByteArray, options: DecodeOptions): ByteArray {
        val policy = ProviderCheck(options.checkCancelled); zstdFrame(input, options, policy)
        val state = zstdProvider(); val output = Buffer(options.maxOutputBytes)
        var provider: java.io.InputStream? = null; var primary: kotlin.Throwable? = null
        try {
            provider = providerConstruct(state.decoder, ZstdSource(input, policy)) as java.io.InputStream
            providerInvoke(state.window, provider, 31 - java.lang.Integer.numberOfLeadingZeros(options.maxWindowBytes))
            val batch = ByteArray(BATCH_BYTES)
            while (true) {
                policy.poll(); val count = provider.read(batch, 0, outputCapacity(output.size(), options.maxOutputBytes)); policy.poll()
                if (count < 0) break; if (count == 0) throw Failure(Code.INVALID_DATA, "Compression provider made no progress")
                output.write(batch, 0, count)
            }
        } catch (failure: kotlin.Throwable) {
            val observed = policy.first ?: if (failure is java.io.IOException) state.failure(failure) else failure
            primary = observed; throw observed
        } finally { provider?.let { owned -> providerClose(primary) { owned.close() } } }
        policy.poll(); return output.toByteArray()
    }

)compresskt"
