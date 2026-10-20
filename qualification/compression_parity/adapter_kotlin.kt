// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.nio.file.Files
import java.nio.file.Path
import java.util.Locale
import SerializerCompression as C

/** Require the same selected frames, policies and owning native models without skips. */
fun main(args: Array<String>) {
    val root = Path.of(args[1])
    for (format in C.Format.entries) check(C.available(format)) { "Missing Kotlin provider $format" }
    if (args[0] == "produce") {
        val value = AccountRecord(); value.accountId = 42u; value.label = "shared \u03bb \ud83d\ude42"
        for (protocol in Protocol.entries) for (format in C.Format.entries) {
            val stem = "kotlin-$protocol-$format"
            val level = if (format == C.Format.NONE || format == C.Format.LZ4) 0 else 3
            val plain = value.encode(protocol); val frame = C.encode({ value.encode(protocol) }, C.EncodeOptions(format, level = level))
            check(frame.contentEquals(C.encode({ value.encode(protocol) }, C.EncodeOptions(format, plain.size, frame.size, level))))
            for (bounds in arrayOf(intArrayOf(plain.size-1, frame.size), intArrayOf(plain.size, frame.size-1))) {
                try { C.encode({ value.encode(protocol) }, C.EncodeOptions(format, bounds[0], bounds[1], level)); error("One-tight encode admission") }
                catch (failure: C.Failure) { check(failure.code == C.Code.RESOURCE_LIMIT) }
            }
            Files.write(root.resolve("$stem.frame"), frame)
            Files.write(root.resolve("$stem.plain"), plain)
        }
        println("{\"produced\":24,\"encode_bounds\":72}"); return
    }
    var decisions = 0; var exchanges = 0
    for (line in Files.readAllLines(root.resolve("cases.tsv"))) {
        val row = line.split('\t'); var observed = "ok"
        try {
            val plain = C.decompress(Files.readAllBytes(root.resolve(row[2])), C.DecodeOptions(C.Format.valueOf(row[1]), row[5].toInt(), row[6].toInt(), row[7].toInt()))
            if (row[4] == "ok") check(plain.contentEquals(Files.readAllBytes(root.resolve(row[3])))) { row[0] }
        } catch (failure: C.Failure) { observed = failure.code.name.lowercase(Locale.ROOT) }
        check(observed == row[4]) { "${row[0]}: $observed != ${row[4]}" }; ++decisions
    }
    for (line in Files.readAllLines(root.resolve("exchange.tsv"))) {
        val row = line.split('\t'); val protocol = Protocol.valueOf(row[1]); val options = C.DecodeOptions(C.Format.valueOf(row[2]))
        val frame = Files.readAllBytes(root.resolve(row[3])); val value = C.decode(frame, { AccountRecord.decode(it, protocol) }, options)
        check(value.accountId == 42u && value.label == "shared \u03bb \ud83d\ude42") { row[0] }
        check(C.decompress(frame, options).contentEquals(Files.readAllBytes(root.resolve(row[4])))) { row[0] }; ++exchanges
    }
    println("{\"decisions\":$decisions,\"exchanges\":$exchanges}")
}
