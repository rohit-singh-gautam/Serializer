import Foundation

// Dispatch concrete value types; successful return always contains a complete fresh model.
func decode(_ kind: Int, _ bytes: [UInt8], _ protocolValue: Protocol, _ limits: Limits) throws -> [UInt8] {
    switch kind {
    case 0: return try InteropMessage.decode(bytes, protocolValue, limits).encode(.JSON)
    case 1: return try CheckTextValue.decode(bytes, protocolValue, limits).encode(.JSON)
    case 2: return try CheckIntegerValue.decode(bytes, protocolValue, limits).encode(.JSON)
    case 3: return try CheckBoolValue.decode(bytes, protocolValue, limits).encode(.JSON)
    case 4: return try CheckFloatValue.decode(bytes, protocolValue, limits).encode(.JSON)
    case 5: return try CheckBytesValue.decode(bytes, protocolValue, limits).encode(.JSON)
    case 6: return try CheckEmpty.decode(bytes, protocolValue, limits).encode(.JSON)
    case 7: return try CheckRecursive.decode(bytes, protocolValue, limits).encode(.JSON)
    case 8: return try CheckVisibility.decode(bytes, protocolValue, limits).encode(.JSON)
    case 9: return try CheckDefaultValue.decode(bytes, protocolValue, limits).encode(.JSON)
    default: fatalError("Unknown test type")
    }
}

let directory = URL(fileURLWithPath: CommandLine.arguments[1])
let output = URL(fileURLWithPath: CommandLine.arguments[2])
var statuses = ""
for line in try String(contentsOf: directory.appendingPathComponent("manifest.tsv"), encoding: .utf8).split(whereSeparator: { $0.isNewline }) {
    let fields = line.split(whereSeparator: { $0.isWhitespace }).map { Int($0)! }
    let bytes = Array(try Data(contentsOf: directory.appendingPathComponent("\(fields[0]).bin")))
    let limits = Limits(maxBytes: fields[3], maxStringBytes: fields[4], maxElements: fields[5], maxDepth: fields[6])
    let json: [UInt8]
    do { json = try decode(fields[1], bytes, Protocol(rawValue: fields[2])!, limits) }
    catch { statuses += "ERR\n"; continue }
    try Data(json).write(to: output.appendingPathComponent("\(fields[0]).json"))
    statuses += "OK\n"
}
try statuses.write(to: output.appendingPathComponent("results.txt"), atomically: true, encoding: .utf8)
