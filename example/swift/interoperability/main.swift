import Foundation

// Construct expected fields independently, including all three union choices.
func fixture(_ variant: Int) -> InteropMessage {
    var value = InteropMessage()
    value.text = "Ada \"Lovelace\" 🚀\n" + String(repeating: "x", count: 64)
    value.numbers = [Int32.min, -1, 0, Int32.max]
    value.decimals = [-0.0, 1.5, -2.25]
    value.flags = [false, true]; value.labels = ["", "é", "🚀"]
    var child = InteropDetail(); child.code = -9; child.note = "child"
    value.children = [InteropDetail(), child]
    value.states = [.Paused, .Ready]
    value.counts = ["🚀": UInt64.max, "é": 7, "a": 0]
    value.indexed = [UInt64.max: child, 0: InteropDetail()]
    value.toggles = [true: "yes", false: "no"]
    value.enums = [.Paused: -2, .Ready: 1]
    value.payloadIndex = variant; value.payloadNumber = -1234567890123456789
    value.payloadRatio = -3.5; value.payloadState = .Paused
    return value
}

// Exchange all native protocols and compare complete decoded data and binary bytes.
func exchange(_ directory: URL, _ mode: String) throws {
    try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
    let languages = try String(contentsOf: directory.appendingPathComponent("producers.txt"), encoding: .utf8).split(whereSeparator: { $0.isWhitespace })
    for variant in 0..<3 {
        let expected = fixture(variant)
        let canonical = try expected.encode(.BINARY_NONE)
        for protocolValue in Protocol.allCases {
            let name = String(describing: protocolValue)
            let bytes = try expected.encode(protocolValue)
            if mode == "emit" {
                try Data(bytes).write(to: directory.appendingPathComponent("swift_\(name)_\(variant).bin"))
            } else {
                for language in languages {
                    let data = try Data(contentsOf: directory.appendingPathComponent("\(language)_\(name)_\(variant).bin"))
                    let actual = try InteropMessage.decode(Array(data), protocolValue)
                    guard try actual.encode(.BINARY_NONE) == canonical else { throw SerializerError.invalid("\(language) -> swift \(name)/\(variant)") }
                    if protocolValue != .JSON && Array(data) != bytes { throw SerializerError.invalid("Canonical binary mismatch") }
                }
            }
        }
    }
    print("swift \(mode) passed")
}

// Measure complete owning encode/decode calls, including allocation costs.
func benchmark() throws {
    let value = fixture(0), iterations = 10000
    for protocolValue in Protocol.allCases {
        let bytes = try value.encode(protocolValue)
        for _ in 0..<1000 { _ = try InteropMessage.decode(value.encode(protocolValue), protocolValue) }
        var start = Date.timeIntervalSinceReferenceDate
        for _ in 0..<iterations { _ = try value.encode(protocolValue) }
        let encode = Date.timeIntervalSinceReferenceDate-start; start = Date.timeIntervalSinceReferenceDate
        for _ in 0..<iterations { _ = try InteropMessage.decode(bytes, protocolValue) }
        print("swift \(protocolValue): \(bytes.count) bytes; encode \(Int(Double(iterations)/encode))/s; decode \(Int(Double(iterations)/(Date.timeIntervalSinceReferenceDate-start)))/s")
    }
}

guard CommandLine.arguments.count == 3 else { fatalError("Usage: main <fixtures-directory> emit|verify|benchmark") }
let mode = CommandLine.arguments[2]
if mode == "benchmark" { try benchmark() }
else if mode == "emit" || mode == "verify" { try exchange(URL(fileURLWithPath: CommandLine.arguments[1]), mode) }
else { fatalError("Unknown mode") }
