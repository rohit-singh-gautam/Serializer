import java.io.File

// Read a managed record, update a typed field, and check all four protocols.
fun main(args: Array<String>) {
    val value = ExampleModel.decode(File(args[0]).readBytes(), Protocol.BINARY_INTEGER)
    value.revision += 1u
    val canonical = value.encode(Protocol.BINARY_NONE)
    for (protocol in Protocol.entries) {
        val copy = ExampleModel.decode(value.encode(protocol), protocol)
        check(copy.encode(Protocol.BINARY_NONE).contentEquals(canonical)) { "Value mismatch" }
    }
    File(args[1]).writeBytes(value.encode(Protocol.BINARY_INTEGER))
    println("Four protocols passed")
}
