// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

// Check durable values and ensure the readonly callable cannot change native bytes.
func check(_ value: EvolutionOrder, customer: String, note: String?) throws {
    guard value.id == 7 && value.customer == customer && value.note == note else {
        throw SerializerError.invalid("Unexpected authored order state")
    }
    let before = try value.encode(.BINARY_NONE)
    guard try value.hasId(7) && !value.hasId(8) && before == value.encode(.BINARY_NONE) else {
        throw SerializerError.invalid("Readonly order behavior changed durable state")
    }
}

// Construct one native authored value independently of the selected XML mapping.
func authored() -> EvolutionOrder {
    var value = EvolutionOrder()
    value.id = 7
    value.customer = "R&D"
    value.note = "memo"
    return value
}

// Emit native baselines only after the default, callable, native and XML checks have succeeded.
func emit(_ variant: String, directory: String) throws {
    let defaults = EvolutionOrder()
    try check(defaults, customer: "native default", note: nil)
    let defaultBytes = try defaults.encode(.BINARY_NONE)
    try check(EvolutionOrder.decode(defaultBytes, .BINARY_NONE), customer: "native default", note: nil)
    let value = authored()
    try check(value, customer: "R&D", note: "memo")
    let native = try value.encode(.BINARY_NONE)
    let integer = try value.encode(.BINARY_INTEGER)
    let names = try value.encode(.BINARY_STRING)
    try check(EvolutionOrder.decode(native, .BINARY_NONE), customer: "R&D", note: "memo")
    try check(EvolutionOrder.decode(integer, .BINARY_INTEGER), customer: "R&D", note: "memo")
    try check(EvolutionOrder.decode(names, .BINARY_STRING), customer: "R&D", note: "memo")
    let xml = try value.encodeXml()
    let projection = authored()
    try check(EvolutionOrder.decodeXml(xml), customer: "R&D", note: projection.note)
    try check(value, customer: "R&D", note: "memo")
    let destination = URL(fileURLWithPath: directory, isDirectory: true)
    try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)
    let outputs: [(String, [UInt8])] = [
        ("authored.xml", xml), ("authored.bin", native),
        ("authored-integer.bin", integer), ("authored-string.bin", names),
        ("default.bin", defaultBytes), ("xml-projection.bin", try projection.encode(.BINARY_NONE))
    ]
    for (name, bytes) in outputs { try Data(bytes).write(to: destination.appendingPathComponent(name)) }
}

// Decode and validate a private candidate before creating an externally visible output file.
func run(_ arguments: [String]) throws {
    guard arguments.count >= 3 && ["v1", "v2"].contains(arguments[0]) else {
        throw SerializerError.invalid("Expected VARIANT emit OUTDIR or VARIANT check-xml/check-native INPUT OUTPUT")
    }
    if arguments[1] == "emit" && arguments.count == 3 {
        try emit(arguments[0], directory: arguments[2])
        return
    }
    guard arguments.count == 4 else { throw SerializerError.invalid("Expected one input and one output") }
    let input = [UInt8](try Data(contentsOf: URL(fileURLWithPath: arguments[2])))
    let result: [UInt8]
    switch arguments[1] {
    case "check-xml":
        let value = try EvolutionOrder.decodeXml(input)
        try check(value, customer: "R&D", note: "memo")
        result = try value.encode(.BINARY_NONE)
    case "check-native":
        let value = try EvolutionOrder.decode(input, .BINARY_NONE)
        try check(value, customer: "R&D", note: "memo")
        result = try value.encodeXml()
        try check(value, customer: "R&D", note: "memo")
    default: throw SerializerError.invalid("Unknown evolution command")
    }
    try Data(result).write(to: URL(fileURLWithPath: arguments[3]))
}

// Fail cleanly on rejected XML without publishing a partial native result or invoking a crash reporter.
do { try run(Array(CommandLine.arguments.dropFirst())) }
catch {
    FileHandle.standardError.write(Data((String(describing: error) + "\n").utf8))
    exit(1)
}
