// Bind a shared owner field and write its target projection during one callback.
fun main() {
  val document = Owner()
  document.b.text = "initial"
  document.b.count = 1u
  val observer = Consumer()
  observer.d.bind(document, "document-1", 7uL, 1uL)

  // Binding retains the owner, and reads follow the current contents of b.
  document.b.text = "updated"
  document.b.count = 2u
  val output = observer.d.withValue { live ->
    live.withToWire { projected -> projected.encode(Protocol.JSON) }
  }
  val value = ProjectedValue.decode(output, Protocol.JSON)
  check(value.label == "updated" && value.count == 2u)

  // Persist owners once. A separately decoded address starts without a binding.
  val loadedDocument = Owner.decode(document.encode(Protocol.JSON), Protocol.JSON)
  val loadedObserver = Consumer.decode(observer.encode(Protocol.JSON), Protocol.JSON)
  check(!loadedObserver.d.isBound())
  check(loadedObserver.d.resolve(loadedDocument, "document-1", 7uL, 1uL))
  loadedObserver.d.withValue { live -> check(live.text == "updated") }

  println(output.toString(Charsets.UTF_8))
}
