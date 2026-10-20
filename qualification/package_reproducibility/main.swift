// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

typealias P = SerializerPackage
enum ProducerError: Error { case invalidArguments, invalidTable, invalidHex }
struct Part { let name: String; let media: String; let bytes: [UInt8] }

// Bind the self-contained compressor version to the actual generated implementation bytes.
func provider() -> [String:Any] {
    ["implementation":"Serializer Swift bounded-LZ77 fixed-Huffman DEFLATE",
     "version":"@provider_version@",
     "options":["level":6,"window_bytes":32768,"strategy":"fixed-Huffman","wrapper":"raw-deflate"],
     "metadata_method":0]
}

// Retain literal UTF8 fields and an empty final hexadecimal payload cell.
func rows(_ path: URL, _ fields: Int) throws -> [[String]] {
    let text = try String(contentsOf:path,encoding:.utf8)
    return try text.split(separator:"\n").map { line in
        let row = line.split(separator:"\t",omittingEmptySubsequences:false).map(String.init)
        guard row.count == fields else { throw ProducerError.invalidTable }
        return row
    }
}

// Decode authored binary bytes without using generated serialization or compression helpers.
func hexadecimal(_ text: String) throws -> [UInt8] {
    let bytes = Array(text.utf8)
    guard bytes.count%2 == 0 else { throw ProducerError.invalidHex }
    var result: [UInt8] = []
    for index in stride(from:0,to:bytes.count,by:2) {
        guard let value = UInt8(String(decoding:bytes[index..<(index+2)],as:UTF8.self),radix:16) else { throw ProducerError.invalidHex }
        result.append(value)
    }
    return result
}

// Exercise all six caller-order permutations without pre-sorting either inventory.
func permute<T>(_ values: [T], _ permutation: Int) -> [T] {
    var indexes = Array(values.indices)
    if permutation == 1 { indexes.reverse() }
    else if permutation == 2 || permutation == 3 {
        let offset = permutation == 2 ? 1 : values.count/2
        indexes = Array(indexes.dropFirst(offset))+Array(indexes.prefix(offset))
    } else if permutation == 4 { indexes = indexes.filter { $0%2 == 0 }+indexes.filter { $0%2 != 0 } }
    else if permutation == 5 { indexes = Array(indexes.filter { $0%2 != 0 }.reversed())+Array(indexes.filter { $0%2 == 0 }.reversed()) }
    return indexes.map { values[$0] }
}

// Emit complete owning bytes through public native writers and the same-ledger full file Save.
func produce(_ arguments: [String]) throws {
    guard arguments.count == 4 || arguments.count == 5,
        ["zip-stored","zip-deflate","opc-stored","opc-deflate"].contains(arguments[1]),
        let permutation = Int(arguments[2]), (0..<6).contains(permutation),
        arguments.count == 4 || arguments[4] == "0" || arguments[4] == "1" else { throw ProducerError.invalidArguments }
    let profile = arguments[1], opc = profile.hasPrefix("opc-"), force = arguments.count == 5 && arguments[4] == "1"
    let storage: P.Compression = profile.hasSuffix("-stored") ? .stored : .deflate
    let directory = URL(fileURLWithPath:arguments[0],isDirectory:true).appendingPathComponent(opc ? "opc":"zip",isDirectory:true)
    let parts = try permute(rows(directory.appendingPathComponent("parts.tsv"),4).map { row in
        Part(name:row[0],media:row[1],bytes:try hexadecimal(row[3]))
    },permutation)
    let links = try permute(rows(directory.appendingPathComponent("links.tsv"),5),permutation)
    var limits = P.Limits(); limits.forceZip64 = force
    let budget = try P.Budget(limits)
    let bytes: [UInt8]
    if opc {
        let payloads = parts.map { P.BinaryPart($0.bytes,name:$0.name,contentType:$0.media,compression:storage) }
        let relationships = links.map { P.Relationship(source:$0[0],id:$0[1],type:$0[2],target:$0[3],external:$0[4] == "external") }
        bytes = try P.opcWrite(payloads,relationships:relationships,budget:budget)
    } else {
        let writer = try P.ZipWriter(budget:budget); defer { writer.close() }
        for part in parts { try writer.add(part.name,part.bytes,compression:storage) }
        bytes = try writer.finish()
    }
    try P.save(bytes,to:URL(fileURLWithPath:arguments[3]),budget:budget)
    try report(["profile":profile,"permutation":permutation,"force_zip64":force,"parts":parts.count,
                "relationships":links.count,"provider":provider()])
}

// Print one stable JSON record while keeping binary output out of the process streams.
func report(_ value: [String:Any]) throws {
    let encoded = try JSONSerialization.data(withJSONObject:value,options:.sortedKeys)
    print(String(decoding:encoded,as:UTF8.self))
}

// Report failures without an uncaught exception or platform error-reporting dialog.
do {
    let arguments = Array(CommandLine.arguments.dropFirst())
    if arguments == ["--provider"] { try report(provider()) }
    else { try produce(arguments) }
} catch {
    FileHandle.standardError.write(Data((String(describing:error)+"\n").utf8))
    exit(1)
}
