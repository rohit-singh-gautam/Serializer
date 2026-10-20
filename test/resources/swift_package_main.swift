// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

typealias P = SerializerPackage
enum Injected: Error { case transport, cancelled }
var checkLabel = ""
// Fail checks independently of compiler assertions or optimization settings.
func require(_ condition: Bool, _ message: String) {
    if !condition { fatalError(message) }
}
// Read a complete independently prepared fixture without borrowing the ZIP consumer's storage.
func bytes(_ path: String) throws -> [UInt8] { Array(try Data(contentsOf: URL(fileURLWithPath:path))) }
// Exercise transport ownership and exact synchronous read boundaries.
final class Source: P.Source {
    let size: Int
    let data: [UInt8]
    var mode = "normal"
    var calls = 0
    init(_ data: [UInt8], size: Int? = nil) { self.data = data; self.size = size ?? data.count }
    func read(offset: Int, count: Int) throws -> [UInt8] {
        calls += 1
        if mode == "throw" { throw Injected.transport }
        let result = Array(data[offset..<(offset+count)])
        return mode == "short" ? Array(result.dropLast()) : mode == "large" ? result+[0] : result
    }
}
// An unknown-length source exposes bounded chunks and never receives implicit close calls.
final class Sequential: P.SequentialSource {
    var data: [UInt8]
    var fail = false
    init(_ data: [UInt8]) { self.data = data }
    func read(maximumBytes: Int) throws -> [UInt8] {
        if fail { throw Injected.transport }
        let count = min(17,min(maximumBytes,data.count)); let output = Array(data.prefix(count)); data.removeFirst(count); return output
    }
}
// A caller-owned sink records a valid prefix before an injected original transport error.
final class Sink: P.Sink {
    var data: [UInt8] = []
    var fail = false
    func write(_ bytes: [UInt8]) throws { data += bytes; if fail { throw Injected.transport } }
}
// Match structured package categories and preserve original transport/cancellation errors.
func rejected(_ code: P.Code? = nil, _ work: () throws -> Void) {
    do { try work(); fatalError("Invalid package operation succeeded: "+checkLabel) }
    catch let failure as P.Failure { if let code { require(failure.code == code,"Unexpected package failure: \(failure.message)") } }
    catch Injected.transport {} catch Injected.cancelled {} catch { fatalError("Unexpected error: \(error)") }
}
// Check complete native ZIP/provider/resource/storage behavior against externally generated fixtures.
func zipChecks(_ root: String) throws {
    let directory = URL(fileURLWithPath:root,isDirectory:true)
    let cases = try String(contentsOf:directory.appendingPathComponent("cases.tsv"),encoding:.utf8)
    for row in cases.split(whereSeparator: \.isNewline) {
        let fields = row.split(separator:"\t").map(String.init), path = directory.appendingPathComponent(fields[0]).path
        checkLabel = fields[0]
        let data = try bytes(path)
        if fields[1] == "valid" {
            let reader = try P.ZipReader.open(data); defer { reader.close() }
            for (index,entry) in reader.entries.enumerated() { let decoded = try reader.read(entry.name); try Data(decoded).write(to:URL(fileURLWithPath:path+".\(index).decoded")) }
        } else {
            rejected { let reader = try P.ZipReader.open(data); defer { reader.close() }; for entry in reader.entries { _ = try reader.read(entry.name) } }
        }
    }
    let payload = Array((0..<10000).map { UInt8(truncatingIfNeeded:$0*13) })
    for index in 0..<8 {
        checkLabel = "writer-finished-\(index)"
        var limits = P.Limits(); limits.forceZip64Entries = index & 1 != 0; limits.forceZip64 = index & 2 != 0; limits.descriptor = index & 4 != 0
        let writer = try P.ZipWriter(budget:P.Budget(limits)); var mutable = payload
        try writer.add("z/data.bin",mutable,compression:.deflate); mutable[0] ^= 255
        try writer.add("a.bin",Array("stored".utf8)); try writer.add("empty.bin",[])
        let output = try writer.finish(); rejected(.invalidState) { _ = try writer.finish() }
        try Data(output).write(to:directory.appendingPathComponent("swift-\(index).zip"))
        let copy = try P.ZipReader.open(output); defer { copy.close() }; require(try copy.read("z/data.bin") == payload,"Caller mutation changed writer snapshot")
        let second = try P.ZipWriter(budget:P.Budget(limits)); try second.add("empty.bin",[]); try second.add("a.bin",Array("stored".utf8)); try second.add("z/data.bin",payload,compression:.deflate)
        require(try second.finish() == output,"Canonical ZIP bytes depend on input insertion order")
    }
    let valid = try bytes(directory.appendingPathComponent("stored.zip").path)
    let unicode = try P.ZipWriter(); try unicode.add("\u{e9}.bin",Array("composed".utf8)); try unicode.add("e\u{301}.bin",Array("decomposed".utf8)); try Data(unicode.finish()).write(to:directory.appendingPathComponent("swift-unicode.zip"))
    for mode in ["short","large","throw"] { checkLabel = "source-"+mode; let source = Source(valid); source.mode = mode; rejected { _ = try P.ZipReader.open(source) }; require(source.calls == 1,"Invalid source read resumed") }
    let negative = Source([],size:-1); rejected(.resourceLimit) { _ = try P.ZipReader.open(negative) }; require(negative.calls == 0,"Invalid source extent touched transport")
    var options = P.Limits(); options.maxArchiveBytes = valid.count-1; let source = Source(valid); rejected(.resourceLimit) { _ = try P.ZipReader.open(source,budget:P.Budget(options)) }; require(source.calls == 0,"Archive admission ran after transport")
    for field in 0..<7 {
        checkLabel = "resource-\(field)"
        var limits = P.Limits()
        switch field { case 0: limits.maxEntries = 0; case 1: limits.maxMetadataBytes = 0; case 2: limits.maxIoBytes = 0; case 3: limits.maxBufferBytes = 0; case 4: limits.maxDecoderAllocationBytes = 0; case 5: limits.maxTotalExpandedBytes = 0; default: limits.maxWorkUnits = 0 }
        rejected(.resourceLimit) { let reader = try P.ZipReader.open(valid,budget:P.Budget(limits)); defer { reader.close() }; _ = try reader.read("data.bin") }
    }
    var limits = P.Limits(); limits.checkCancelled = { throw Injected.cancelled }; rejected { _ = try P.ZipReader.open(valid,budget:P.Budget(limits)) }
    limits = P.Limits(); limits.maxPartBytes = payload.count; limits.maxTotalExpandedBytes = payload.count
    let cumulative = try P.ZipReader.open(valid,budget:P.Budget(limits)); require(try cumulative.read("data.bin") == payload,"Exact expanded-byte limit rejected"); rejected(.resourceLimit) { _ = try cumulative.read("data.bin") }; cumulative.close()
    limits.maxPartBytes -= 1; rejected(.resourceLimit) { _ = try P.ZipReader.open(valid,budget:P.Budget(limits)) }
    limits = P.Limits(); limits.deadlineUptimeNanoseconds = 0; rejected(.deadlineExceeded) { _ = try P.Budget(limits) }
    var recursive: P.Budget?; limits = P.Limits(); limits.checkCancelled = { if let recursive { do { try recursive.check() } catch {} } }
    recursive = try P.Budget(limits); rejected(.invalidState) { try recursive!.check() }
    let sink = Sink(); sink.fail = true; let writer = try P.ZipWriter(); try writer.add("data.bin",payload); rejected { try writer.write(to:sink) }; require(!sink.data.isEmpty,"Transport fault wrote no injected prefix"); rejected(.invalidState) { _ = try writer.finish() }
    let spool = directory.appendingPathComponent("spool",isDirectory:true); try FileManager.default.createDirectory(at:spool,withIntermediateDirectories:true)
    limits = P.Limits(); limits.allowSpool = true; limits.spoolDirectory = spool
    let reader = try P.ZipReader.open(Sequential(valid),budget:P.Budget(limits)); reader.close()
    require(try FileManager.default.contentsOfDirectory(atPath:spool.path).isEmpty,"Successful spool leaked private files")
    limits.maxSpoolBytes = 1; rejected(.resourceLimit) { _ = try P.ZipReader.open(Sequential(valid),budget:P.Budget(limits)) }
    let broken = Sequential(valid); broken.fail = true; limits.maxSpoolBytes = valid.count; rejected { _ = try P.ZipReader.open(broken,budget:P.Budget(limits)) }
    require(try FileManager.default.contentsOfDirectory(atPath:spool.path).isEmpty,"Failed spool leaked private files")
    let destination = directory.appendingPathComponent("save.zip"); let old = Data("old".utf8); try old.write(to:destination)
    var armed = false; limits = P.Limits(); limits.checkCancelled = { if armed { throw Injected.cancelled } }
    let save = try P.ZipWriter(budget:P.Budget(limits)); try save.add("data.bin",payload); armed = true; rejected { try save.save(to:destination) }; require(try Data(contentsOf:destination) == old,"Failed Save replaced old file")
    let complete = try P.ZipWriter(); try complete.add("data.bin",payload); try complete.save(to:destination)
    let saved = try P.ZipReader.open(P.ZipReader.FileSource(destination)); defer { saved.close() }; require(try saved.read("data.bin") == payload,"Atomic Save did not publish complete file")
    print("Swift ZIP source/sink, ZIP64, descriptors, CRC, cancellation, seven resource ledgers, spool cleanup and full-Save pass")
}

try zipChecks(CommandLine.arguments[1])
