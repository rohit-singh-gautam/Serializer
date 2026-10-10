using System;
using System.IO;
using static SerializerGenerated.Schema;

internal static class Program {
  // Compare complete versioned values and return a clean failure when input is rejected.
  private static int Main(string[] args) {
    try {
      var value = ExampleModel.Decode(File.ReadAllBytes(args[0]), Protocol.JSON);
      var canonical = value.Encode(Protocol.BINARY_NONE);
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var copy = ExampleModel.Decode(value.Encode(protocol), protocol);
        if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(canonical)) throw new Exception("Value mismatch");
      }
      File.WriteAllBytes(args[1], value.Encode(Protocol.JSON));
      Console.WriteLine("Four protocols passed");
      return 0;
    } catch (Exception error) {
      Console.Error.WriteLine(error.Message);
      return 1;
    }
  }
}
