@file:OptIn(ExperimentalUnsignedTypes::class)
import java.io.File
import kotlin.system.measureNanoTime

// Construct an independent expected value using native unsigned types and primitive arrays.
private fun fixture(variant: Int): InteropMessage {
    val value = InteropMessage()
    value.text = "Ada \"Lovelace\" 🚀\n" + "x".repeat(64)
    value.numbers = intArrayOf(Int.MIN_VALUE, -1, 0, Int.MAX_VALUE)
    value.decimals = floatArrayOf(-0.0f, 1.5f, -2.25f)
    value.flags = booleanArrayOf(false, true); value.labels = mutableListOf("", "é", "🚀")
    val child = InteropDetail(); child.code = -9; child.note = "child"
    value.children = mutableListOf(InteropDetail(), child)
    value.states = mutableListOf(InteropState.Paused, InteropState.Ready)
    value.counts = linkedMapOf("🚀" to ULong.MAX_VALUE, "é" to 7uL, "a" to 0uL)
    value.indexed = linkedMapOf(ULong.MAX_VALUE to child, 0uL to InteropDetail())
    value.toggles = linkedMapOf(true to "yes", false to "no")
    value.enums = linkedMapOf(InteropState.Paused to -2, InteropState.Ready to 1)
    value.payloadIndex = variant; value.payloadNumber = -1234567890123456789L
    value.payloadRatio = -3.5f; value.payloadState = InteropState.Paused
    return value
}

// Exchange every protocol with all producer languages selected by the runner.
private fun exchange(directory: File, mode: String) {
    directory.mkdirs()
    val languages = File(directory, "producers.txt").readLines().filter { it.isNotBlank() }
    for (variant in 0..2) {
        val expected = fixture(variant)
        val canonical = expected.encode(Protocol.BINARY_NONE)
        for (protocol in Protocol.entries) {
            val bytes = expected.encode(protocol)
            if (mode == "emit") File(directory, "kotlin_${protocol.name}_${variant}.bin").writeBytes(bytes)
            else for (language in languages) {
                val data = File(directory, "${language}_${protocol.name}_${variant}.bin").readBytes()
                val actual = InteropMessage.decode(data, protocol)
                check(actual.encode(Protocol.BINARY_NONE).contentEquals(canonical)) { "$language -> kotlin $protocol/$variant" }
                if (protocol != Protocol.JSON) check(data.contentEquals(bytes)) { "Canonical binary mismatch" }
            }
        }
    }
    println("kotlin $mode passed")
}

// Report warmed local timing samples, including complete output construction.
private fun benchmark() {
    val value = fixture(0); val iterations = 10000
    for (protocol in Protocol.entries) {
        val bytes = value.encode(protocol)
        repeat(1000) { InteropMessage.decode(value.encode(protocol), protocol) }
        val encode = measureNanoTime { repeat(iterations) { value.encode(protocol) } }
        val decode = measureNanoTime { repeat(iterations) { InteropMessage.decode(bytes, protocol) } }
        println("kotlin $protocol: ${bytes.size} bytes; encode ${iterations*1e9/encode}/s; decode ${iterations*1e9/decode}/s")
    }
}

// Run as a JVM application using the generated Kotlin module alone.
fun main(args: Array<String>) {
    require(args.size == 2) { "Usage: Main <fixtures-directory> emit|verify|benchmark" }
    when (args[1]) { "benchmark" -> benchmark(); "emit", "verify" -> exchange(File(args[0]), args[1]); else -> error("Unknown mode") }
}
