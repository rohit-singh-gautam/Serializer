using System;
using System.IO;
using static SerializerGenerated.Schema;

internal static class Program {
  // Edit an owning model and compare complete values after each protocol round trip.
  private static void Main(string[] args) {
    var integer = new IntegerNumber();
    var floating = new FloatingNumber();
    integer.Value = -7;
    floating.Value = 12.5;
    if (integer.Value != -7 || floating.Value != 12.5) throw new Exception("Constrained generic mismatch");
    var value = ExampleModel.Decode(File.ReadAllBytes(args[0]), Protocol.JSON);
    value.Revision += 1;
    // Edit the uint32 key inside envelope<box<pair<uint32, string>>>.
    value.Payload.Value.Key = 7;
    var canonical = value.Encode(Protocol.BINARY_NONE);
    foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
      var copy = ExampleModel.Decode(value.Encode(protocol), protocol);
      if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(canonical)) throw new Exception("Value mismatch");
    }
    File.WriteAllBytes(args[1], value.Encode(Protocol.JSON));
    Console.WriteLine("Generic model: four protocols passed");
  }
}
