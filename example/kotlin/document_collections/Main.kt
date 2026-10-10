@file:OptIn(kotlin.ExperimentalUnsignedTypes::class)

import java.io.File

// Read an owning model, update a typed field, and check all four protocols.
fun main(args: Array<String>) {
    val defaults = ExampleModel()
    check(defaults.pages.size == 2 && defaults.columns.size == 3 && defaults.headings.size == 2)
    defaults.pages[0].title = "First"
    check(defaults.pages[1].title.isEmpty()) { "Aliased fixed elements" }
    val value = ExampleModel.decode(File(args[0]).readBytes(), Protocol.JSON)
    value.lookupCache = 77uL
    value.revision += 1u
    val canonical = value.encode(Protocol.BINARY_NONE)
    for (protocol in Protocol.entries) {
        val copy = ExampleModel.decode(value.encode(protocol), protocol)
        check(copy.lookupCache == 0uL)
        check(copy.encode(Protocol.BINARY_NONE).contentEquals(canonical)) { "Value mismatch" }
    }
    File(args[1]).writeBytes(value.encode(Protocol.JSON))
    println("Four protocols passed")
}
