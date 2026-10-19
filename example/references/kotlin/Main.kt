// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
@file:OptIn(ExperimentalUnsignedTypes::class)

/** Export live arrays and load ordinary values into an owning consumer. */
fun main() {
  // Unrelated parent records can retain the same independently shared payload object.
  val independent = Payload()
  independent.values = floatArrayOf(11f)
  val first = SharedOwner()
  val second = SharedValueConsumer()
  first.b.bind(independent)
  second.d.bind(independent)
  check(first.b.readValue() === second.d.readValue())

  val source = Owner()
  source.b.values = floatArrayOf(0f, 1f, 2f)
  source.b.codes = uintArrayOf(0u, 1u, 2u)
  val borrowed = BorrowedConsumer()
  borrowed.d.bind(source)
  val shared = SharedConsumer()
  shared.d.bind(source)
  val weak = WeakConsumer()
  weak.d.bind(source)
  source.b.values[0] = 9f
  shared.d.withValue { value -> check(value.values === source.b.values) }

  for (protocol in Protocol.entries) {
    // This owning copy is a verification baseline, not part of either export path.
    val expected = OwningConsumer()
    expected.d = Payload.decode(source.b.encode(protocol), protocol)
    val bytes = expected.encode(protocol)
    check(borrowed.encode(protocol).contentEquals(bytes))
    check(shared.encode(protocol).contentEquals(bytes))
    check(weak.encode(protocol).contentEquals(bytes))
    source.withAsConsumer { projected ->
      check(projected.getD() === source.b)
      check(projected.encode(protocol).contentEquals(bytes))
    }
    // Output-only references reject input; an ordinary target owns decoded buffers.
    check(runCatching { SharedConsumer.decode(bytes, protocol) }.isFailure)
    val decoded = OwningConsumer.decode(bytes, protocol)
    check(decoded.d.values.contentEquals(expected.d.values))
    check(decoded.d.values !== source.b.values)
    source.b.values[0] = 10f
    check(decoded.encode(protocol).contentEquals(bytes))
    source.b.values[0] = 9f
  }
  println(shared.encode(Protocol.JSON).decodeToString())
  // JVM reclamation is nondeterministic; explicit detachment demonstrates unavailable output.
  borrowed.d.unbind()
  check(!borrowed.d.isBound())
  check(runCatching { borrowed.encode(Protocol.JSON) }.isFailure)
}
