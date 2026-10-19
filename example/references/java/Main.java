import java.util.Arrays;

/** Export existing payload arrays without constructing another payload. */
public final class Main {
  /** Check live storage identity, every wire format and independent decoded values. */
  public static void main(String[] args) {
    var value = new Schema.Payload(); value.values = new float[]{7, 8};
    var a = new Schema.SharedOwner(); a.b.bind(value);
    var c = new Schema.SharedValueConsumer(); c.d.bind(value);
    if (a.b.readValue() != value || c.d.readValue() != value) throw new AssertionError("Independent sharing");
    value = null; a.b.unbind();
    var independentExpected = new Schema.OwningConsumer(); independentExpected.d = c.d.readValue();
    for (var protocol : Schema.Protocol.values()) {
      if (!Arrays.equals(c.encode(protocol), independentExpected.encode(protocol))) throw new AssertionError("Shared value wire mismatch");
    }
    var owner = new Schema.Owner();
    owner.b.values = new float[]{1, 2, 3}; owner.b.codes = new int[]{0, 1, 2};
    var borrowed = new Schema.BorrowedConsumer(); borrowed.d.bind(owner);
    var shared = new Schema.SharedConsumer(); shared.d.bind(owner);
    var weak = new Schema.WeakConsumer(); weak.d.bind(owner);
    if (shared.d.readValue() != owner.b || shared.d.readValue().values != owner.b.values) throw new AssertionError("Copied payload");
    owner.b.values[0] = 4;
    var expected = new Schema.OwningConsumer(); expected.d = owner.b;
    for (var protocol : Schema.Protocol.values()) {
      var encoded = expected.encode(protocol);
      if (!Arrays.equals(borrowed.encode(protocol), encoded) || !Arrays.equals(shared.encode(protocol), encoded) || !Arrays.equals(weak.encode(protocol), encoded)) throw new AssertionError("Wire mismatch");
      owner.withAsConsumer(projected -> { if (!Arrays.equals(projected.encode(protocol), encoded)) throw new AssertionError("Translator mismatch"); return null; });
      var loaded = Schema.OwningConsumer.decode(encoded, protocol);
      if (!Arrays.equals(loaded.encode(protocol), encoded)) throw new AssertionError("Owned decode");
      loaded.d.values[0] = 99;
      if (owner.b.values[0] != 4) throw new AssertionError("Decode changed source");
    }
    System.out.println(new String(shared.encode(Schema.Protocol.JSON), java.nio.charset.StandardCharsets.UTF_8));
  }
}
