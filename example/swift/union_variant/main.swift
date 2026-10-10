import Foundation

// Read a fresh versioned value and verify complete protocol round trips.
do {
    let args = CommandLine.arguments
    let value = try ExampleModel.decode(Array(Data(contentsOf: URL(fileURLWithPath: args[1]))), .JSON)
    let canonical = try value.encode(.BINARY_NONE)
    for protocolValue in Protocol.allCases {
        let copy = try ExampleModel.decode(value.encode(protocolValue), protocolValue)
        guard try copy.encode(.BINARY_NONE) == canonical else { throw SerializerError.invalid("Value mismatch") }
    }
    try Data(value.encode(.JSON)).write(to: URL(fileURLWithPath: args[2]))
    print("Four protocols passed")
} catch {
    // Report expected decoder rejection without triggering a fatal-error crash.
    FileHandle.standardError.write(Data("\(error)\n".utf8))
    exit(1)
}
