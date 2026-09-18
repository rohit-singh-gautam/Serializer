import Foundation

// Read a fresh value, edit a typed field, and verify complete protocol round trips.
let args = CommandLine.arguments
var value = try ExampleModel.decode(Array(Data(contentsOf: URL(fileURLWithPath: args[1]))), .JSON)
value.revision += 1
let canonical = try value.encode(.BINARY_NONE)
for protocolValue in Protocol.allCases {
    let copy = try ExampleModel.decode(value.encode(protocolValue), protocolValue)
    guard try copy.encode(.BINARY_NONE) == canonical else { throw SerializerError.invalid("Value mismatch") }
}
try Data(value.encode(.JSON)).write(to: URL(fileURLWithPath: args[2]))
print("Four protocols passed")
