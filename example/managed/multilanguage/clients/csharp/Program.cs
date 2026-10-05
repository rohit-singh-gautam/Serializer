using System;
using System.IO;
using static SerializerGenerated.Schema;

internal static class Program {
  // Edit an owning model and compare complete values after each protocol round trip.
  private static void Main(string[] args) {
    var value = ExampleModel.Decode(File.ReadAllBytes(args[0]), Protocol.BINARY_INTEGER);
    value.Revision += 1;
    var canonical = value.Encode(Protocol.BINARY_NONE);
    foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
      var copy = ExampleModel.Decode(value.Encode(protocol), protocol);
      if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(canonical)) throw new Exception("Value mismatch");
    }
    File.WriteAllBytes(args[1], value.Encode(Protocol.BINARY_INTEGER));
    Console.WriteLine("Four protocols passed");
  }
}
