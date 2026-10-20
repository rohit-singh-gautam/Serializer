// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.io.File
import java.lang.management.ManagementFactory

@Volatile private var retained: Any? = null

// Measure complete public owning codecs with setup, result validation and output outside timing.
@Suppress("DEPRECATION")
fun main(arguments: Array<String>) {
    require(arguments.size == 4) { "Usage: benchmark INPUT MODE OUTPUT REPETITIONS" }
    val input = File(arguments[0]).readBytes()
    val mode = arguments[1]
    val repetitions = arguments[3].toInt()
    require(repetitions > 0)
    // BENCHMARK_SETUP
    val canonical = value.encode(Protocol.BINARY_NONE)
    val operation: () -> Any = when (mode) {
        "native-encode" -> { { value.encode(Protocol.BINARY_NONE) } }
        "native-decode" -> { { XmlExampleOrder.decode(canonical, Protocol.BINARY_NONE) } }
        // BENCHMARK_XML_OPERATIONS
        else -> error("Unknown benchmark operation")
    }
    repeat(3) { repeat(repetitions) { retained = operation() } }
    val samples = LongArray(5)
    for (index in samples.indices) {
        val start = System.nanoTime()
        repeat(repetitions) { retained = operation() }
        samples[index] = System.nanoTime() - start
    }
    val allocations = ManagementFactory.getThreadMXBean() as com.sun.management.ThreadMXBean
    check(allocations.isThreadAllocatedMemorySupported)
    allocations.isThreadAllocatedMemoryEnabled = true
    val thread = Thread.currentThread().id
    val before = allocations.getThreadAllocatedBytes(thread)
    retained = operation()
    val allocated = allocations.getThreadAllocatedBytes(thread) - before
    val result = when (val selected = retained) {
        is XmlExampleOrder -> selected
        is ByteArray -> {
            // BENCHMARK_RESULT
        }
        else -> error("Unexpected retained result")
    }
    check(result.encode(Protocol.BINARY_NONE).contentEquals(canonical)) { "Measured result changed native values" }
    File(arguments[2]).writeBytes(canonical)
    // BENCHMARK_XML_OUTPUT
    println("{\"wall_ns\":" + samples.joinToString(prefix="[", postfix="]") +
        ",\"repetitions\":" + repetitions + ",\"allocation_bytes\":" + allocated +
        ",\"allocation_method\":\"ThreadMXBean current-thread allocation delta for one separate call; native provider storage may be excluded\"}")
}
