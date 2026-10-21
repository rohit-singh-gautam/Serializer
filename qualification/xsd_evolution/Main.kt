// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package evolution_consumer

import generated.EvolutionOrder
import generated.Protocol
import generated.decodeXml
import generated.encodeXml
import java.nio.file.Files
import java.nio.file.Path
import java.util.Optional

// Require the native model and readonly callable to retain their meaning in either XML variant.
private fun inspect(value: EvolutionOrder, customer: String, note: Optional<String>) {
  check(value.id == 7u) { "Native id changed" }
  check(value.customer == customer) { "Native customer changed" }
  check(value.note == note) { "Native note changed" }
  check(value.hasId(7u) && !value.hasId(8u)) { "Native readonly callable changed" }
}

// Author both schema versions through the same public property names and types.
private fun authored(note: Optional<String>): EvolutionOrder {
  val value = EvolutionOrder()
  value.id = 7u
  value.customer = "R&D"
  value.note = note
  inspect(value, "R&D", note)
  return value
}

// Validate every candidate before writing outputs through a separately compiled consumer module.
fun main(arguments: Array<String>) {
  require(arguments.size == 3 || arguments.size == 4) { "Expected VARIANT ACTION INPUT/OUTDIR [OUTPUT]" }
  val variant = arguments[0]
  val action = arguments[1]
  require(variant == "v1" || variant == "v2") { "Unknown variant" }
  val xmlNote = Optional.of("memo")
  if (action == "emit" && arguments.size == 3) {
    val fresh = EvolutionOrder()
    inspect(fresh, "native default", Optional.empty())
    val value = authored(Optional.of("memo"))
    val xml = value.encodeXml()
    val nativeBytes = value.encode(Protocol.BINARY_NONE)
    val integerBytes = value.encode(Protocol.BINARY_INTEGER)
    val stringBytes = value.encode(Protocol.BINARY_STRING)
    val defaultBytes = fresh.encode(Protocol.BINARY_NONE)
    val projected = authored(xmlNote).encode(Protocol.BINARY_NONE)
    val decoded = EvolutionOrder.decodeXml(xml)
    inspect(decoded, "R&D", xmlNote)
    check(decoded.encode(Protocol.BINARY_NONE).contentEquals(projected)) { "XML projection differs" }
    inspect(EvolutionOrder.decode(nativeBytes, Protocol.BINARY_NONE), "R&D", Optional.of("memo"))
    val directory = Path.of(arguments[2])
    Files.createDirectories(directory)
    for ((name, bytes) in listOf("authored.xml" to xml, "authored.bin" to nativeBytes,
      "authored-integer.bin" to integerBytes, "authored-string.bin" to stringBytes,
      "default.bin" to defaultBytes, "xml-projection.bin" to projected)) {
      Files.write(directory.resolve(name), bytes)
    }
    return
  }
  require(arguments.size == 4) { "Expected input and output paths" }
  val input = Files.readAllBytes(Path.of(arguments[2]))
  val output = when (action) {
    "check-xml" -> {
      val value = EvolutionOrder.decodeXml(input)
      inspect(value, "R&D", xmlNote)
      value.encode(Protocol.BINARY_NONE)
    }
    "check-native" -> {
      val value = EvolutionOrder.decode(input, Protocol.BINARY_NONE)
      inspect(value, "R&D", Optional.of("memo"))
      value.encodeXml()
    }
    else -> error("Unknown action")
  }
  Files.write(Path.of(arguments[3]), output)
}
