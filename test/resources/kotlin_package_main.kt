// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.nio.file.Files
import java.nio.file.Path

private var passed = 0
private var failed = 0
private lateinit var root: Path
private var filter = ""

/** Complete every independent decision while retaining individual failures for diagnosis. */
private fun test(name: String, action: () -> Unit) {
    if (!name.contains(filter)) return
    try { action(); passed++ } catch (error: Throwable) { failed++; System.err.println("FAIL $name: $error"); error.printStackTrace() }
}
/** Capture an expected failure without accepting an accidentally successful operation. */
private fun failure(action: () -> Unit): Throwable { try { action() } catch (error: Throwable) { return error }; error("Operation unexpectedly succeeded") }
/** Compare the public error category rather than implementation-specific messages. */
private fun code(error: Throwable, expected: String) {
    val actual = when (error) { is SerializerPackage.PackageError -> error.code.name; is parameterized.SerializerPackage.PackageError -> error.code.name; is identity.SerializerPackage.PackageError -> error.code.name; else -> throw AssertionError("Missing package diagnostic: $error") }
    check(actual in expected.split(',')) { "Expected $expected; got $actual: $error" }
}
/** Relax only logical large-offset/count/metadata limits for independent ZIP64 boundaries. */
private fun generous(): SerializerPackage.Limits = SerializerPackage.Limits(maxArchiveBytes=1L shl 34,maxEntries=100000,maxMetadataBytes=128L shl 20,maxBufferBytes=256L shl 20,maxDecoderAllocationBytes=512L shl 20)
/** Retain a source that independently checks bounded, legal short random reads and borrowed ownership. */
private class Source(val bytes: ByteArray) : SerializerPackage.RandomSource, java.io.Closeable {
    var calls = 0; var closed = false; var callback: (() -> Unit)? = null; var sentinel: Throwable? = null
    override fun size(): Long = bytes.size.toLong()
    override fun readAt(offset: Long, maximum: Int): ByteArray {
        check(!closed && maximum in 1..32768 && offset >= 0 && offset <= bytes.size && maximum <= bytes.size-offset)
        calls++; sentinel?.let { throw it }; callback?.invoke()
        return bytes.copyOfRange(offset.toInt(),offset.toInt()+minOf(17,maximum))
    }
    override fun close() { closed = true }
}
/** Materialize only tiny independently authored wire segments in a multi-gigabyte logical source. */
private class Segments(path: Path) : SerializerPackage.RandomSource {
    private val extent: Long
    private val segments = mutableListOf<Pair<Long,ByteArray>>()
    var actualBytes = 0L
    init {
        java.io.DataInputStream(Files.newInputStream(path)).use { input ->
            check(input.readNBytes(8).contentEquals(byteArrayOf(83,90,73,80,54,52,0,1)))
            extent = java.lang.Long.reverseBytes(input.readLong()); val count = Integer.reverseBytes(input.readInt()); check(count in 0..15)
            repeat(count) { val offset = java.lang.Long.reverseBytes(input.readLong()); val size = java.lang.Long.reverseBytes(input.readLong()); check(size in 0..1048575); val bytes = input.readNBytes(size.toInt()); check(bytes.size.toLong() == size); segments.add(offset to bytes) }
            check(input.read() == -1)
        }
    }
    override fun size(): Long = extent
    override fun readAt(offset: Long, maximum: Int): ByteArray {
        check(offset >= 0 && offset <= extent && maximum in 1..32768 && maximum <= extent-offset)
        val count = minOf(maximum,17); actualBytes += count; check(actualBytes < 1048576)
        val result = ByteArray(count)
        for ((start,bytes) in segments) { val first = maxOf(offset,start); val last = minOf(offset+count,start+bytes.size); if (first < last) bytes.copyInto(result,(first-offset).toInt(),(first-start).toInt(),(last-start).toInt()) }
        return result
    }
}
/** Read all physical entries so an accepted index cannot hide payload integrity failures. */
private fun readZip(source: Any, name: String, hex: String) {
    SerializerPackage.ZipReader.open(source,SerializerPackage.Budget(generous())).use { archive ->
        for (entry in archive.entries()) check(archive.read(entry.name).size.toLong() == entry.expandedBytes)
        if (name.isNotEmpty()) check(archive.read(name).contentEquals(java.util.HexFormat.of().parseHex(hex)))
    }
}
/** Execute one independent ZIP, OPC, Unicode, descriptor or malformed-boundary corpus decision. */
private fun row(values: List<String>) {
    val path = root.resolve("fixtures").resolve(values[2])
    val action = {
        when (values[1]) {
            "segment" -> { val source = Segments(path); readZip(source,values[4],values[5]); check(source.actualBytes < 1048576) }
            "zip" -> readZip(Files.readAllBytes(path),values[4],values[5])
            "count" -> SerializerPackage.ZipReader.open(path,SerializerPackage.Budget(generous())).use { archive -> val count = values[4].toInt(); val entries = archive.entries(); check(entries.size == count); for (index in entries.indices) check(entries[index].name == "part$index"); check(archive.read("part${count-1}").isEmpty()) }
            "collection" -> { val value = ProfileCollectionArchive.load(path).value; check(value.models.size == 3 && value.models[2].value.name == "item2") }
            "parameterized" -> { check(parameterized.ProfileArchive.load(path).value.main!!.name == values[5]) }
            else -> {
                val mode = when (values[1]) { "preserve" -> SerializerPackage.Unknown.PRESERVE; "skip" -> SerializerPackage.Unknown.SKIP; else -> SerializerPackage.Unknown.REJECT }
                val result = ProfileArchive.load(path,policy=SerializerPackage.ExtensionPolicy(mode,mode))
                check(result.value.main!!.name == values[5] && result.value.attachments.size == 1)
                check(result.value.attachments[0].data.contentEquals(byteArrayOf(0,1,254.toByte(),255.toByte())))
            }
        }
    }
    if (values[3] == "OK") action() else code(failure(action),values[3])
}
/** Author actual generated native values for independent ZIP/XML output inspection. */
private fun model(): ProfileArchive.Input = ProfileArchive.Input().also { value ->
    value.main = ProfileModel().also { it.name = "generated & checked"; it.points = mutableListOf(ProfilePoint().also { point -> point.x=1.0; point.y=2.0; point.z=3.0 }) }
    value.attachments = listOf(SerializerPackage.BinaryPart(byteArrayOf(0,1,254.toByte(),255.toByte()),"/Textures/raw.bin","application/octet-stream"))
}
/** Track forbidden borrowed close/flush and late sink cancellation boundaries. */
private class Sink : java.io.ByteArrayOutputStream() {
    var closed = false; var flushed = false; var callback: (() -> Unit)? = null
    override fun write(bytes: ByteArray, offset: Int, count: Int) { check(count <= 32768); super.write(bytes,offset,count); callback?.invoke() }
    override fun close() { closed = true }
    override fun flush() { flushed = true }
}
/** Map public cumulative resources without reaching into runtime-owned counters. */
private fun maximum(limits: SerializerPackage.Limits, resource: SerializerPackage.Resource): Long = when (resource) {
    SerializerPackage.Resource.IO_BYTES -> limits.maxIoBytes; SerializerPackage.Resource.METADATA_BYTES -> limits.maxMetadataBytes
    SerializerPackage.Resource.EXPANDED_BYTES -> limits.maxTotalExpandedBytes; SerializerPackage.Resource.ENTRIES -> limits.maxEntries
    SerializerPackage.Resource.RELATIONSHIPS -> limits.maxRelationships; SerializerPackage.Resource.DECODER_ALLOCATION_BYTES -> limits.maxDecoderAllocationBytes
    SerializerPackage.Resource.WORK_UNITS -> limits.maxWorkUnits; SerializerPackage.Resource.REFERENCES -> limits.maxReferences; SerializerPackage.Resource.PRESERVED_BYTES -> limits.maxPreservedBytes
}
/** Change exactly one ceiling for an independently measured exact/one-tight admission pair. */
private fun setMaximum(limits: SerializerPackage.Limits, resource: SerializerPackage.Resource, value: Long) {
    when (resource) {
        SerializerPackage.Resource.IO_BYTES -> limits.maxIoBytes=value; SerializerPackage.Resource.METADATA_BYTES -> limits.maxMetadataBytes=value
        SerializerPackage.Resource.EXPANDED_BYTES -> limits.maxTotalExpandedBytes=value; SerializerPackage.Resource.ENTRIES -> limits.maxEntries=value
        SerializerPackage.Resource.RELATIONSHIPS -> limits.maxRelationships=value; SerializerPackage.Resource.DECODER_ALLOCATION_BYTES -> limits.maxDecoderAllocationBytes=value
        SerializerPackage.Resource.WORK_UNITS -> limits.maxWorkUnits=value; SerializerPackage.Resource.REFERENCES -> limits.maxReferences=value; SerializerPackage.Resource.PRESERVED_BYTES -> limits.maxPreservedBytes=value
    }
}
/** Verify lifecycle, cumulative shared budgets, exact caller failures and full-Save publication. */
private fun contracts() {
    val bytes = Files.readAllBytes(root.resolve("fixtures/profile-descriptor-0-wide-0.zip"))
    test("borrowed short source") { val source = Source(bytes); check(ProfileArchive.load(source).value.main!!.name == "independent"); check(!source.closed && source.calls > 10) }
    test("snapshot bytes") { val mutable = bytes.copyOf(); SerializerPackage.ZipReader.open(mutable).use { archive -> mutable.fill(0); check(archive.read("Textures/raw.bin").size == 4) } }
    test("invalid policy before I/O") { val source = Source(bytes); code(failure { SerializerPackage.ZipReader.open(source,SerializerPackage.Budget(SerializerPackage.Limits(maxArchiveBytes=-1))) },"INVALID_OPTIONS"); check(source.calls == 0) }
    test("source archive admission") { val source = Source(bytes); code(failure { SerializerPackage.ZipReader.open(source,SerializerPackage.Budget(SerializerPackage.Limits(maxArchiveBytes=bytes.size-1L))) },"RESOURCE_LIMIT"); check(source.calls == 0) }
    test("policy snapshot") { val limits = SerializerPackage.Limits(maxArchiveBytes=bytes.size-1L); val budget = SerializerPackage.Budget(limits); limits.maxArchiveBytes=Long.MAX_VALUE; budget.configured().maxArchiveBytes=Long.MAX_VALUE; code(failure { SerializerPackage.ZipReader.open(bytes,budget) },"RESOURCE_LIMIT") }
    test("immutable metadata") { SerializerPackage.ZipReader.open(bytes).use { reader -> failure { (reader.entries() as MutableList).clear() }; check(reader.entries().size == 5) } }
    test("exact cumulative package budgets") {
        val path=root.resolve("fixtures/extension.zip"); val preserve=SerializerPackage.ExtensionPolicy(SerializerPackage.Unknown.PRESERVE,SerializerPackage.Unknown.PRESERVE)
        val measured=SerializerPackage.Budget(); ProfileArchive.load(path,measured,preserve)
        for (resource in SerializerPackage.Resource.entries) {
            val used=maximum(measured.configured(),resource)-measured.remaining(resource); if (used==0L) continue
            val limits=SerializerPackage.Limits(); setMaximum(limits,resource,used); ProfileArchive.load(path,SerializerPackage.Budget(limits),preserve)
            setMaximum(limits,resource,used-1); code(failure { ProfileArchive.load(path,SerializerPackage.Budget(limits),preserve) },"RESOURCE_LIMIT")
        }
    }
    test("source exact caller failure") { val source=Source(bytes); val expected=java.io.IOException("source-marker"); source.sentinel=expected; check(failure { SerializerPackage.ZipReader.open(source) } === expected); check(!source.closed) }
    test("reader sticky failure") { val source=Source(bytes); val reader=SerializerPackage.ZipReader.open(source); val expected=java.io.IOException("payload-marker"); source.sentinel=expected; check(failure { reader.read("Textures/raw.bin") } === expected); val calls=source.calls; check(failure { reader.read("Textures/raw.bin") } === expected); check(calls==source.calls); reader.close() }
    test("located transport preserves caller PackageError identity") {
        val source=Source(bytes); val reader=SerializerPackage.ZipReader.open(source)
        val expected=SerializerPackage.PackageError(SerializerPackage.Code.RESOURCE_LIMIT,"caller-marker","caller-part")
        source.sentinel=expected
        check(failure { reader.readContext("Textures/raw.bin","/Textures/raw.bin","attachments","edge") } === expected)
        check(expected.part=="caller-part" && expected.fieldPath.isEmpty()); reader.close()
    }
    for (method in listOf("stored","deflate")) test("writer cumulative expansion $method") {
        SerializerPackage.ZipWriter(SerializerPackage.Budget(SerializerPackage.Limits(maxTotalExpandedBytes=0))).use { writer ->
            code(failure { writer.add("nonempty.bin",byteArrayOf(1),method) },"RESOURCE_LIMIT"); failure { writer.finish() }
        }
    }
    test("write rejects internal target root escape") {
        val opaque=SerializerPackage.BinaryPart(byteArrayOf(1),"/Model.xml","application/octet-stream")
        val links=listOf(SerializerPackage.Relationship("/","escape","urn:serializer:opaque","../Model.xml"))
        code(failure { SerializerPackage.Composer().finish(SerializerPackage.Extensions(listOf(opaque),links)) },"INVALID_RELATIONSHIP")
    }
    for (resource in listOf(SerializerPackage.Resource.DECODER_ALLOCATION_BYTES,SerializerPackage.Resource.WORK_UNITS)) test("selected entry admission retains role $resource") {
        val budget=SerializerPackage.Budget(); SerializerPackage.ZipReader.open(bytes,budget).use { reader ->
            budget.consume(resource,budget.remaining(resource))
            val observed=failure { reader.readContext("Alternate/root.xml","/Alternate/root.xml","main","main-edge") }
            code(observed,"RESOURCE_LIMIT"); observed as SerializerPackage.PackageError
            check(observed.part=="/Alternate/root.xml" && observed.fieldPath=="main" && observed.relationshipId=="main-edge")
        }
    }
    test("reader callback close") { val source=Source(bytes); val reader=SerializerPackage.ZipReader.open(source); source.callback={reader.close()}; failure { reader.read("Textures/raw.bin") }; val calls=source.calls; failure { reader.read("Textures/raw.bin") }; check(calls==source.calls && !source.closed) }
    test("reader swallowed reentry") { val source=Source(bytes); val reader=SerializerPackage.ZipReader.open(source); source.callback={failure { reader.read("Textures/raw.bin") }}; failure { reader.read("Textures/raw.bin") }; val calls=source.calls; failure { reader.read("Textures/raw.bin") }; check(calls==source.calls); reader.close() }
    test("zero random progress") { val source=object: SerializerPackage.RandomSource { override fun size()=bytes.size.toLong(); override fun readAt(offset: Long,maximum: Int)=byteArrayOf() }; failure { SerializerPackage.ZipReader.open(source) } }
    test("overlong random read") { val source=object: SerializerPackage.RandomSource { override fun size()=bytes.size.toLong(); override fun readAt(offset: Long,maximum: Int)=ByteArray(maximum+1) }; failure { SerializerPackage.ZipReader.open(source) } }
    test("cancellation identity") { val expected=IllegalStateException("cancel-marker"); check(failure { SerializerPackage.Budget(SerializerPackage.Limits(checkCancelled={throw expected})) } === expected) }
    test("expired deadline") { code(failure { SerializerPackage.Budget(SerializerPackage.Limits(deadlineNanos=System.nanoTime()-1)) },"DEADLINE_EXCEEDED") }
    test("sink ownership") { val sink=Sink(); ProfileArchive.write(sink,model()); check(sink.size()>0 && !sink.closed && !sink.flushed) }
    test("final sink cancellation") { val expected=IllegalStateException("late-cancel"); var cancelled=false; val limits=SerializerPackage.Limits(checkCancelled={if(cancelled)throw expected}); val sink=Sink(); sink.callback={cancelled=true}; check(failure { ProfileArchive.write(sink,model(),SerializerPackage.Budget(limits)) } === expected); check(sink.size()>0 && !sink.closed) }
    test("writer callback close") { val writer=SerializerPackage.ZipWriter(); writer.add("data",ByteArray(100000)); val sink=Sink(); sink.callback={writer.close()}; failure { writer.writeTo(sink) }; check(sink.size() in 1..32768); failure { writer.finish() } }
    test("spool explicit admission and cleanup") {
        val directory=root.resolve("spool"); Files.createDirectories(directory)
        val source=object:java.io.ByteArrayInputStream(bytes) { var closed=false; override fun close(){closed=true} }
        code(failure { SerializerPackage.ZipReader.open(source) },"INVALID_OPTIONS"); check(source.available()==bytes.size)
        val limits=SerializerPackage.Limits(allowSpool=true,spoolDirectory=directory,maxSpoolBytes=bytes.size.toLong())
        SerializerPackage.ZipReader.open(source,SerializerPackage.Budget(limits)).use { check(it.read("Textures/raw.bin").size==4) }; check(!source.closed)
        Files.list(directory).use { check(it.findAny().isEmpty) }
        limits.maxSpoolBytes=bytes.size-1L; code(failure { SerializerPackage.ZipReader.open(java.io.ByteArrayInputStream(bytes),SerializerPackage.Budget(limits)) },"RESOURCE_LIMIT")
        Files.list(directory).use { check(it.findAny().isEmpty) }
    }
    test("spool cancellation cleanup") {
        val directory=root.resolve("spool-cancel"); Files.createDirectories(directory); val expected=IllegalStateException("spool-marker"); var cancel=false
        val input=object:java.io.ByteArrayInputStream(bytes) { override fun read(data:ByteArray,offset:Int,count:Int):Int { val read=super.read(data,offset,minOf(count,17)); cancel=true; return read } }
        val limits=SerializerPackage.Limits(allowSpool=true,spoolDirectory=directory,checkCancelled={if(cancel)throw expected})
        check(failure { SerializerPackage.ZipReader.open(input,SerializerPackage.Budget(limits)) } === expected); Files.list(directory).use { check(it.findAny().isEmpty) }
    }
    test("private spool creation and owned cleanup") {
        val directory=root.resolve("spool-private"); Files.createDirectories(directory)
        val caller=directory.resolve("caller-owned.bin"); Files.write(caller,byteArrayOf(7)); var observed=false
        val input=object:java.io.ByteArrayInputStream(bytes) {
            /** Observe only the exclusively created spool while the caller's bounded read is active. */
            override fun read(data:ByteArray,offset:Int,count:Int):Int {
                Files.list(directory).use { files ->
                    val privateFiles=files.filter { it.fileName.toString().startsWith(".serializer-zip-") }.toList()
                    check(privateFiles.size==1); observed=true
                    if(Files.getFileStore(directory).supportsFileAttributeView("posix"))
                        check(Files.getPosixFilePermissions(privateFiles.single())==java.nio.file.attribute.PosixFilePermissions.fromString("rw-------"))
                }
                return super.read(data,offset,count)
            }
        }
        SerializerPackage.ZipReader.open(input,SerializerPackage.Budget(SerializerPackage.Limits(allowSpool=true,spoolDirectory=directory))).use { check(it.entries().isNotEmpty()) }
        check(observed && Files.readAllBytes(caller).contentEquals(byteArrayOf(7)))
        Files.list(directory).use { check(it.toList()==listOf(caller)) }; Files.delete(caller)
    }
    test("spool scratch allocation precedes source and private creation") {
        val directory=root.resolve("spool-scratch"); Files.createDirectories(directory); var reads=0
        val input=object:java.io.ByteArrayInputStream(bytes) { override fun read(data:ByteArray,offset:Int,count:Int):Int { reads++; return super.read(data,offset,count) } }
        val limits=SerializerPackage.Limits(allowSpool=true,spoolDirectory=directory,maxDecoderAllocationBytes=0)
        code(failure { SerializerPackage.ZipReader.open(input,SerializerPackage.Budget(limits)) },"RESOURCE_LIMIT")
        check(reads==0); Files.list(directory).use { check(it.findAny().isEmpty) }
    }
    for (capacity in listOf(0L,bytes.size-1L)) test("spool pre-I/O admission $capacity") {
        val directory=root.resolve("spool-admission"); Files.createDirectories(directory); var consumed=0L
        val input=object:java.io.ByteArrayInputStream(bytes) { override fun read(data:ByteArray,offset:Int,count:Int):Int { check(count in 1..32768); val actual=super.read(data,offset,minOf(count,17)); if(actual>0)consumed+=actual; return actual } }
        val limits=SerializerPackage.Limits(allowSpool=true,spoolDirectory=directory,maxIoBytes=capacity,maxSpoolBytes=bytes.size.toLong())
        code(failure { SerializerPackage.ZipReader.open(input,SerializerPackage.Budget(limits)) },"RESOURCE_LIMIT"); check(consumed<=capacity)
        Files.list(directory).use { check(it.findAny().isEmpty) }
    }
    test("required opaque capabilities override optional skip") { val policy=SerializerPackage.ExtensionPolicy(SerializerPackage.Unknown.SKIP,SerializerPackage.Unknown.SKIP,requiredPart={true}); code(failure { ProfileArchive.load(root.resolve("fixtures/extension.zip"),policy=policy) },"UNSUPPORTED_FEATURE") }
    test("extension policy matrix") {
        val path=root.resolve("fixtures/extension.zip")
        for (mode in SerializerPackage.Unknown.entries) if (mode==SerializerPackage.Unknown.REJECT) code(failure { ProfileArchive.load(path) },"UNSUPPORTED_FEATURE") else { val result=ProfileArchive.load(path,policy=SerializerPackage.ExtensionPolicy(mode,mode)); check(result.extensions.parts.size==if(mode==SerializerPackage.Unknown.PRESERVE)1 else 0) }
    }
    test("extensions visible before domain publication") { var calls=0; val result=ProfileArchive.load(root.resolve("fixtures/extension.zip"),policy=SerializerPackage.ExtensionPolicy(SerializerPackage.Unknown.PRESERVE,SerializerPackage.Unknown.PRESERVE),validate={value,_->calls++;check(value.main!=null && value.extensions.parts.size==1 && value.extensions.relationships.size==1)}); check(calls==1 && result.extensions===result.value.extensions) }
    test("external target spelling") { val loaded=ProfileArchive.load(root.resolve("fixtures/external-iris.zip"),policy=SerializerPackage.ExtensionPolicy(SerializerPackage.Unknown.PRESERVE,SerializerPackage.Unknown.PRESERVE)); check(loaded.extensions.relationships.map{it.target}==Files.readAllLines(root.resolve("fixtures/external-targets.txt"))); Files.write(root.resolve("external-roundtrip.opc"),ProfileArchive.encode(loaded.value)) }
    test("preserved namespace bytes and edited graph rejection") {
        val result=ProfileArchive.load(root.resolve("fixtures/extension.zip"),policy=SerializerPackage.ExtensionPolicy(SerializerPackage.Unknown.PRESERVE,SerializerPackage.Unknown.PRESERVE))
        val opaque=result.extensions.parts.single(); val encoded=ProfileArchive.encode(result.value)
        SerializerPackage.ZipReader.open(encoded).use { check(it.read(opaque.name.substring(1)).contentEquals(opaque.data)) }
        result.value.extensions=SerializerPackage.Extensions(result.extensions.parts,result.extensions.relationships.map{it.copy(target="missing.bin")})
        code(failure { ProfileArchive.encode(result.value) },"MISSING_PART")
    }
    test("typed middle failure publishes nothing") { var published=false; code(failure { ProfileCollectionArchive.load(root.resolve("fixtures/collection-middle-invalid.zip"),validate={_,_->published=true}) },"CODEC"); check(!published) }
    test("domain exception identity") { val expected=java.io.IOException("domain-marker"); check(failure { ProfileArchive.load(bytes,validate={_,_->throw expected}) } === expected) }
    for (boundary in listOf("before_temporary","after_temporary","before_file_sync","after_file_sync","before_replace","after_replace","after_directory_sync")) test("atomic Save $boundary") {
        val path=root.resolve("save-$boundary.opc"); val old=byteArrayOf(7,8,9); Files.write(path,old); val expected=java.io.IOException("save-marker")
        val observed=failure { ProfileArchive.save(path,model(),fault={if(it==boundary)throw expected}) }
        if (boundary in listOf("after_replace","after_directory_sync")) { code(observed,"PUBLICATION_UNCERTAIN"); check(observed.cause===expected); check(ProfileArchive.load(path).value.main!!.name=="generated & checked") } else { check(observed===expected); check(Files.readAllBytes(path).contentEquals(old)) }
        Files.list(root).use { paths -> check(paths.noneMatch{it.fileName.toString().startsWith(".serializer-save-")}) }
    }
    test("semantic Save rejection precedes filesystem") { val path=root.resolve("semantic.opc"); val old=byteArrayOf(4,5); Files.write(path,old); val expected=java.io.IOException("semantic-marker"); var hooks=0; check(failure { ProfileArchive.save(path,model(),validate={_,_->throw expected},fault={hooks++}) } === expected); check(hooks==0 && Files.readAllBytes(path).contentEquals(old)) }
    for (wide in listOf(false,true)) for (descriptor in listOf(false,true)) test("independent output $wide $descriptor") {
        val limits=SerializerPackage.Limits(forceZip64=wide,descriptor=descriptor)
        Files.write(root.resolve("produced-$wide-$descriptor.opc"),ProfileArchive.encode(model(),SerializerPackage.Budget(limits)))
        SerializerPackage.ZipWriter(SerializerPackage.Budget(limits)).use { writer -> writer.add("stored.bin",byteArrayOf(0,1,2)); writer.add("empty.bin",byteArrayOf(),"deflate"); Files.write(root.resolve("produced-$wide-$descriptor.zip"),writer.finish()) }
    }
    test("real ZIP64 directory count output") {
        SerializerPackage.ZipWriter(SerializerPackage.Budget(generous().also { it.descriptor=false })).use { writer ->
            repeat(65536) { writer.add("part$it",byteArrayOf()) }
            Files.write(root.resolve("large-count.zip"),writer.finish())
        }
    }
    test("automatic member/provider output admission") {
        val role=SerializerPackage.Role("main","/3D/model.xml","application/vnd.serializer.test-model+xml","/","urn:serializer:main-model","main-link","stored",1,1)
        val composer=SerializerPackage.Composer()
        val observed=failure { composer.single(role,model().main!!) { value,context ->
            val limits=XmlLimits(); limits.srlPackageContext=SrlXmlBudget.Operation(context::check) { kind,count,field -> context.consume(kind,if(kind=="allocation" && field=="name")Long.MAX_VALUE else count,field) }
            value.encodeXml(limits)
        } }
        code(observed,"RESOURCE_LIMIT"); observed as SerializerPackage.PackageError
        check(observed.part=="/3D/model.xml" && observed.fieldPath=="main.name" && observed.relationshipId=="main-link")
        failure { composer.finish() }
    }
    test("deferred document buffer retains owner attribution") {
        val value=model().main!!; val size=value.encodeXml().size.toLong()
        val role=SerializerPackage.Role("main","/3D/model.xml","application/vnd.serializer.test-model+xml","/","urn:serializer:main-model","main-link","stored",1,1)
        val observed=failure { SerializerPackage.Composer().single(role,value) { candidate,context ->
            var sawMember=false; val limits=XmlLimits()
            limits.srlPackageContext=SrlXmlBudget.Operation(context::check) { kind,count,field ->
                if(field.isNotEmpty())sawMember=true
                context.consume(kind,if(sawMember && kind=="allocation" && field.isEmpty() && count==size)Long.MAX_VALUE else count,field)
            }
            candidate.encodeXml(limits)
        } }
        code(observed,"RESOURCE_LIMIT"); observed as SerializerPackage.PackageError
        check(observed.part=="/3D/model.xml" && observed.fieldPath=="main" && observed.relationshipId=="main-link")
    }
    test("provider input attribute retains schema member attribution") {
        val role=SerializerPackage.Role("main","/3D/model.xml","application/vnd.serializer.test-model+xml","/","urn:serializer:main-model",null,"stored",1,1)
        SerializerPackage.Loader(bytes).use { loader ->
            val observed=failure { loader.single(role) { data,context ->
                val limits=XmlLimits(); limits.srlPackageContext=SrlXmlBudget.Operation(context::check) { kind,count,field -> context.consume(kind,if(kind=="allocation" && field=="name")Long.MAX_VALUE else count,field) }
                ProfileModel.decodeXml(data,limits)
            } }
            code(observed,"RESOURCE_LIMIT"); observed as SerializerPackage.PackageError
            check(observed.part=="/Alternate/root.xml" && observed.fieldPath=="main.name" && observed.relationshipId=="main")
        }
    }
    test("native policy exception identity") {
        val expected=IllegalArgumentException("native-policy-marker"); var context:SerializerPackage.CodecContext?=null
        val budget=SerializerPackage.Budget(SerializerPackage.Limits(checkCancelled={if(context?.fieldPath=="main.name")throw expected}))
        val role=SerializerPackage.Role("main","/model.xml","application/test+xml","/","urn:test:main","main-link","stored",1,1)
        check(failure { SerializerPackage.Composer(budget).single(role,model().main!!) { value,current -> context=current; val limits=XmlLimits(); limits.srlPackageContext=SrlXmlBudget.Operation(current::check,current::consume); value.encodeXml(limits) } } === expected)
    }
    test("typed QName child scopes survive complete package roundtrip") {
        val value=ScopedArchive.load(root.resolve("fixtures/scoped.opc")).value
        check(value.main!!.first=="p:item" && value.main!!.second=="p:item")
        Files.write(root.resolve("scoped-roundtrip.opc"),ScopedArchive.encode(value))
    }
    test("retained QName bindings share output allocation admission") {
        val value=ScopedArchive.load(root.resolve("fixtures/scoped.opc")).value.main!!
        val retainedOwnerAllocation=64L+"unused".length*2L+checkNotNull(value.srlXmlNamespaces.owner["unused"]).length*2L
        val role=SerializerPackage.Role("main","/model.xml","application/vnd.scoped+xml","/","urn:scoped:main","scope","stored",1,1)
        var admitted=false
        val observed=failure { SerializerPackage.Composer().single(role,value) { candidate,context ->
            val limits=XmlLimits()
            limits.srlPackageContext=SrlXmlBudget.Operation(context::check) { kind,count,field ->
                val retainedOwner=kind=="allocation" && field.isEmpty() && count==retainedOwnerAllocation
                if(retainedOwner)admitted=true
                context.consume(kind,if(retainedOwner)Long.MAX_VALUE else count,field)
            }
            candidate.encodeXml(limits)
        } }
        code(observed,"RESOURCE_LIMIT"); observed as SerializerPackage.PackageError
        check(admitted && observed.part=="/model.xml" && observed.fieldPath=="main" && observed.relationshipId=="scope")
    }
    test("QName retained input copies share cumulative package ledger") {
        val path=root.resolve("fixtures/scoped.opc"); val measured=SerializerPackage.Budget()
        ScopedArchive.load(path,measured)
        val used=measured.configured().maxDecoderAllocationBytes-measured.remaining(SerializerPackage.Resource.DECODER_ALLOCATION_BYTES)
        check(used>Files.size(path))
        ScopedArchive.load(path,SerializerPackage.Budget(SerializerPackage.Limits(maxDecoderAllocationBytes=used)))
        code(failure { ScopedArchive.load(path,SerializerPackage.Budget(SerializerPackage.Limits(maxDecoderAllocationBytes=used-1))) },"RESOURCE_LIMIT")
    }
    test("identity cumulative reference admission includes repeated equal keyrefs") {
        val path=root.resolve("identity.opc")
        val exact=identity.SerializerPackage.Budget(identity.SerializerPackage.Limits(maxReferences=15))
        check(identity.IdentityPackageArchive.load(path,exact).value.registries.size==3)
        check(exact.remaining(identity.SerializerPackage.Resource.REFERENCES)==0L)
        val observed=failure { identity.IdentityPackageArchive.load(path,identity.SerializerPackage.Budget(identity.SerializerPackage.Limits(maxReferences=14))) }
        code(observed,"RESOURCE_LIMIT"); observed as identity.SerializerPackage.PackageError
        check(observed.part=="/r2.xml" && observed.fieldPath=="registries" && observed.relationshipId=="r2")
    }
    test("identity middle failure is transactional and located") {
        var published=false
        val observed=failure { identity.IdentityPackageArchive.load(root.resolve("identity-invalid.opc"),validate={_,_->published=true}) }
        code(observed,"CODEC"); observed as identity.SerializerPackage.PackageError
        check(!published && observed.part=="/r2.xml" && observed.fieldPath=="registries" && observed.relationshipId=="r2")
    }
    test("helper collision") { collisionProbe() }
    test("canonical input permutations") {
        val left=SerializerPackage.Composer(); val right=SerializerPackage.Composer(); val role=SerializerPackage.Role("items",null,null,"/","urn:serializer:part",null,"stored",0,-1)
        val parts=listOf(SerializerPackage.BinaryPart(byteArrayOf(1),"/a.bin","application/octet-stream"),SerializerPackage.BinaryPart(byteArrayOf(2),"/b.bin","application/octet-stream"))
        left.binaries(role,parts); right.binaries(role,parts.reversed())
        // Stable authored relationship IDs are necessary when occurrence order has semantic identity.
        val extensions=SerializerPackage.Extensions(parts,listOf(SerializerPackage.Relationship("/","a","urn:serializer:part","/a.bin"),SerializerPackage.Relationship("/","b","urn:serializer:part","/b.bin")))
        check(SerializerPackage.Composer().finish(extensions).contentEquals(SerializerPackage.Composer().finish(SerializerPackage.Extensions(parts.reversed(),extensions.relationships.reversed()))))
    }
}
/** Exercise every maintained independent corpus case and native lifecycle contract. */
fun main(arguments: Array<String>) {
    root=Path.of(arguments[0]); filter=arguments.getOrElse(1){""}
    for (line in Files.readAllLines(root.resolve("fixtures/cases.tsv"))) { val values=line.split('\t'); test(values[0]){row(values)} }
    contracts(); println("{\"passed\":$passed,\"failed\":$failed}")
    check(failed==0) { "$failed Kotlin package checks failed" }
}
