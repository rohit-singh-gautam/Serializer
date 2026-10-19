using System;
using System.Linq;
using static SerializerGenerated.Schema;

// Export the original payload arrays through transparent value references.
internal static class Program {
  // Check storage identity, wire equivalence and independent decoded values.
  private static void Main() {
    Payload? value = new Payload(); value.Values.AddRange(new float[]{7, 8});
    var a = new SharedOwner(); a.B.Bind(value);
    var c = new SharedValueConsumer(); c.D.Bind(value);
    if (!ReferenceEquals(a.B.ReadValue(), value) || !ReferenceEquals(c.D.ReadValue(), value)) throw new Exception("Independent sharing");
    value = null; a.B.Unbind();
    var independentExpected = new OwningConsumer(); independentExpected.D = c.D.ReadValue();
    foreach (Protocol protocol in Enum.GetValues(typeof(Protocol))) {
      if (!c.Encode(protocol).SequenceEqual(independentExpected.Encode(protocol))) throw new Exception("Shared value wire mismatch");
    }
    var owner = new Owner();
    owner.B.Values.AddRange(new float[]{1, 2, 3}); owner.B.Codes.AddRange(new uint[]{0, 1, 2});
    var borrowed = new BorrowedConsumer(); borrowed.D.Bind(owner);
    var shared = new SharedConsumer(); shared.D.Bind(owner);
    var weak = new WeakConsumer(); weak.D.Bind(owner);
    if (!ReferenceEquals(shared.D.ReadValue(), owner.B) || !ReferenceEquals(shared.D.ReadValue().Values, owner.B.Values)) throw new Exception("Copied payload");
    owner.B.Values[0] = 4;
    var expected = new OwningConsumer(); expected.D = owner.B;
    foreach (Protocol protocol in Enum.GetValues(typeof(Protocol))) {
      var encoded = expected.Encode(protocol);
      if (!borrowed.Encode(protocol).SequenceEqual(encoded) || !shared.Encode(protocol).SequenceEqual(encoded) || !weak.Encode(protocol).SequenceEqual(encoded)) throw new Exception("Wire mismatch");
      owner.WithAsConsumer(projected => { if (!projected.Encode(protocol).SequenceEqual(encoded)) throw new Exception("Translator mismatch"); return 0; });
      var loaded = OwningConsumer.Decode(encoded, protocol);
      if (!loaded.Encode(protocol).SequenceEqual(encoded)) throw new Exception("Owned decode");
      loaded.D.Values[0] = 99;
      if (owner.B.Values[0] != 4) throw new Exception("Decode changed source");
    }
    Console.WriteLine(System.Text.Encoding.UTF8.GetString(shared.Encode(Protocol.JSON)));
  }
}
