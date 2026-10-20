// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.nio.file.Files
import java.nio.file.Path

/** Verify actual C++ composition through generated Kotlin values and return an independent native-readable archive. */
fun main(arguments: Array<String>) {
    val result = examplemodel.OpcExampleDocument.load(Path.of(arguments[0]))
    val catalog = checkNotNull(result.value.main)
    check(catalog.title == "Parts & tools")
    check(catalog.items.map { it.id.toLong() to it.label } == listOf(7L to "Bolt <M6>",8L to "Nut"))
    val attachment = result.value.attachments.single()
    check(attachment.name == "/Attachments/sample.bin" && attachment.data.contentEquals(byteArrayOf(0,1,254.toByte(),255.toByte())))
    Files.write(Path.of(arguments[1]),examplemodel.OpcExampleDocument.encode(result.value))
}
