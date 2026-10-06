import Foundation

// Read a fresh value, edit a typed field, and verify complete protocol round trips.
do {
    let typed = Uint16Version()
    guard try typed.encode(.BINARY_NONE) == [44, 1] else { throw SerializerError.invalid("Revision width") }
    for protocolValue in Protocol.allCases {
        let copy = try Uint16Version.decode(typed.encode(protocolValue), protocolValue)
        guard try copy.encode(.BINARY_NONE) == typed.encode(.BINARY_NONE) else { throw SerializerError.invalid("Revision mismatch") }
    }
}
do {
    let typed = Uint32Version()
    guard try typed.encode(.BINARY_NONE) == [112, 17, 1, 0] else { throw SerializerError.invalid("Revision width") }
    for protocolValue in Protocol.allCases {
        let copy = try Uint32Version.decode(typed.encode(protocolValue), protocolValue)
        guard try copy.encode(.BINARY_NONE) == typed.encode(.BINARY_NONE) else { throw SerializerError.invalid("Revision mismatch") }
    }
}
do {
    let typed = Uint64Version()
    guard try typed.encode(.BINARY_NONE) == [255, 255, 255, 255, 255, 255, 255, 255] else { throw SerializerError.invalid("Revision width") }
    for protocolValue in Protocol.allCases {
        let copy = try Uint64Version.decode(typed.encode(protocolValue), protocolValue)
        guard try copy.encode(.BINARY_NONE) == typed.encode(.BINARY_NONE) else { throw SerializerError.invalid("Revision mismatch") }
    }
}
do {
    let typed = FloatVersion()
    guard try typed.encode(.BINARY_NONE) == [205, 204, 204, 61] else { throw SerializerError.invalid("Revision width") }
    for protocolValue in Protocol.allCases {
        let copy = try FloatVersion.decode(typed.encode(protocolValue), protocolValue)
        guard try copy.encode(.BINARY_NONE) == typed.encode(.BINARY_NONE) else { throw SerializerError.invalid("Revision mismatch") }
    }
}
do {
    let typed = DoubleVersion()
    guard try typed.encode(.BINARY_NONE) == [0, 0, 0, 0, 0, 0, 4, 64] else { throw SerializerError.invalid("Revision width") }
    for protocolValue in Protocol.allCases {
        let copy = try DoubleVersion.decode(typed.encode(protocolValue), protocolValue)
        guard try copy.encode(.BINARY_NONE) == typed.encode(.BINARY_NONE) else { throw SerializerError.invalid("Revision mismatch") }
    }
}
do {
    let typed = Dotted2Version()
    guard try typed.encode(.BINARY_NONE) == [1, 0, 10, 0] else { throw SerializerError.invalid("Revision width") }
    for protocolValue in Protocol.allCases {
        let copy = try Dotted2Version.decode(typed.encode(protocolValue), protocolValue)
        guard try copy.encode(.BINARY_NONE) == typed.encode(.BINARY_NONE) else { throw SerializerError.invalid("Revision mismatch") }
    }
}
do {
    let typed = Dotted3Version()
    guard try typed.encode(.BINARY_NONE) == [1, 0, 10, 0, 0, 0] else { throw SerializerError.invalid("Revision width") }
    for protocolValue in Protocol.allCases {
        let copy = try Dotted3Version.decode(typed.encode(protocolValue), protocolValue)
        guard try copy.encode(.BINARY_NONE) == typed.encode(.BINARY_NONE) else { throw SerializerError.invalid("Revision mismatch") }
    }
}
do {
    let typed = Dotted4Version()
    guard try typed.encode(.BINARY_NONE) == [1, 0, 10, 0, 0, 0, 4, 0] else { throw SerializerError.invalid("Revision width") }
    for protocolValue in Protocol.allCases {
        let copy = try Dotted4Version.decode(typed.encode(protocolValue), protocolValue)
        guard try copy.encode(.BINARY_NONE) == typed.encode(.BINARY_NONE) else { throw SerializerError.invalid("Revision mismatch") }
    }
}
let args = CommandLine.arguments
var value = try ExampleModel.decode(Array(Data(contentsOf: URL(fileURLWithPath: args[1]))), .JSON, Limits(readPolicy: .COMPATIBLE))
let old = try ExampleModel.decode(value.encode(.BINARY_NONE), .BINARY_NONE, Limits(readPolicy: .COMPATIBLE))
guard old.version == 8 && old.oldName == "Ada" else { throw SerializerError.invalid("Historical mismatch") }
value.name = value.oldName
value.version = 10
let canonical = try value.encode(.BINARY_NONE)
for protocolValue in Protocol.allCases {
    let copy = try ExampleModel.decode(value.encode(protocolValue), protocolValue)
    guard try copy.encode(.BINARY_NONE) == canonical else { throw SerializerError.invalid("Value mismatch") }
}
try Data(value.encode(.JSON)).write(to: URL(fileURLWithPath: args[2]))
print("Version migration and four protocols passed")
