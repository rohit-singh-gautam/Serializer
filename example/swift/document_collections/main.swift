import Foundation

// Read a fresh value, edit a typed field, and verify complete protocol round trips.
var defaults = ExampleModel()
guard defaults.pages.count == 2 && defaults.columns.count == 3 && defaults.headings.count == 2
  else { throw SerializerError.invalid("Incorrect fixed defaults") }
defaults.pages[0].title = "First"
guard defaults.pages[1].title.isEmpty else { throw SerializerError.invalid("Aliased fixed elements") }
let args = CommandLine.arguments
var value = try ExampleModel.decode(Array(Data(contentsOf: URL(fileURLWithPath: args[1]))), .JSON)
value.lookupCache = 77
value.revision += 1
let canonical = try value.encode(.BINARY_NONE)
for protocolValue in Protocol.allCases {
    let copy = try ExampleModel.decode(value.encode(protocolValue), protocolValue)
    guard copy.lookupCache == 0 else { throw SerializerError.invalid("Transient cache persisted") }
  guard try copy.encode(.BINARY_NONE) == canonical else { throw SerializerError.invalid("Value mismatch") }
}
try Data(value.encode(.JSON)).write(to: URL(fileURLWithPath: args[2]))
print("Four protocols passed")
