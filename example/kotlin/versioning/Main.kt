import java.io.File

// Read an owning model, update a typed field, and check all four protocols.
fun main(args: Array<String>) {
    run {
        val typed = Uint16Version()
        check(typed.encode(Protocol.BINARY_NONE).contentEquals(byteArrayOf(44, 1)))
        for (protocol in Protocol.entries) {
            val copy = Uint16Version.decode(typed.encode(protocol), protocol)
            check(copy.encode(Protocol.BINARY_NONE).contentEquals(typed.encode(Protocol.BINARY_NONE)))
        }
    }
    run {
        val typed = Uint32Version()
        check(typed.encode(Protocol.BINARY_NONE).contentEquals(byteArrayOf(112, 17, 1, 0)))
        for (protocol in Protocol.entries) {
            val copy = Uint32Version.decode(typed.encode(protocol), protocol)
            check(copy.encode(Protocol.BINARY_NONE).contentEquals(typed.encode(Protocol.BINARY_NONE)))
        }
    }
    run {
        val typed = Uint64Version()
        check(typed.encode(Protocol.BINARY_NONE).contentEquals(byteArrayOf(-1, -1, -1, -1, -1, -1, -1, -1)))
        for (protocol in Protocol.entries) {
            val copy = Uint64Version.decode(typed.encode(protocol), protocol)
            check(copy.encode(Protocol.BINARY_NONE).contentEquals(typed.encode(Protocol.BINARY_NONE)))
        }
    }
    run {
        val typed = FloatVersion()
        check(typed.encode(Protocol.BINARY_NONE).contentEquals(byteArrayOf(-51, -52, -52, 61)))
        for (protocol in Protocol.entries) {
            val copy = FloatVersion.decode(typed.encode(protocol), protocol)
            check(copy.encode(Protocol.BINARY_NONE).contentEquals(typed.encode(Protocol.BINARY_NONE)))
        }
    }
    run {
        val typed = DoubleVersion()
        check(typed.encode(Protocol.BINARY_NONE).contentEquals(byteArrayOf(0, 0, 0, 0, 0, 0, 4, 64)))
        for (protocol in Protocol.entries) {
            val copy = DoubleVersion.decode(typed.encode(protocol), protocol)
            check(copy.encode(Protocol.BINARY_NONE).contentEquals(typed.encode(Protocol.BINARY_NONE)))
        }
    }
    run {
        val typed = Dotted2Version()
        check(typed.encode(Protocol.BINARY_NONE).contentEquals(byteArrayOf(1, 0, 10, 0)))
        for (protocol in Protocol.entries) {
            val copy = Dotted2Version.decode(typed.encode(protocol), protocol)
            check(copy.encode(Protocol.BINARY_NONE).contentEquals(typed.encode(Protocol.BINARY_NONE)))
        }
    }
    run {
        val typed = Dotted3Version()
        check(typed.encode(Protocol.BINARY_NONE).contentEquals(byteArrayOf(1, 0, 10, 0, 0, 0)))
        for (protocol in Protocol.entries) {
            val copy = Dotted3Version.decode(typed.encode(protocol), protocol)
            check(copy.encode(Protocol.BINARY_NONE).contentEquals(typed.encode(Protocol.BINARY_NONE)))
        }
    }
    run {
        val typed = Dotted4Version()
        check(typed.encode(Protocol.BINARY_NONE).contentEquals(byteArrayOf(1, 0, 10, 0, 0, 0, 4, 0)))
        for (protocol in Protocol.entries) {
            val copy = Dotted4Version.decode(typed.encode(protocol), protocol)
            check(copy.encode(Protocol.BINARY_NONE).contentEquals(typed.encode(Protocol.BINARY_NONE)))
        }
    }
    val value = ExampleModel.decode(File(args[0]).readBytes(), Protocol.JSON, Limits(readPolicy = ReadPolicy.COMPATIBLE))
    val old = ExampleModel.decode(value.encode(Protocol.BINARY_NONE), Protocol.BINARY_NONE, Limits(readPolicy = ReadPolicy.COMPATIBLE))
    check(old.version == 8.toUByte() && old.oldName == "Ada")
    value.name = value.oldName
    value.version = 10u
    val canonical = value.encode(Protocol.BINARY_NONE)
    for (protocol in Protocol.entries) {
        val copy = ExampleModel.decode(value.encode(protocol), protocol)
        check(copy.encode(Protocol.BINARY_NONE).contentEquals(canonical)) { "Value mismatch" }
    }
    File(args[1]).writeBytes(value.encode(Protocol.JSON))
    println("Version migration and four protocols passed")
}
