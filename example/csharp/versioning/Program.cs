using System;
using System.IO;
using static SerializerGenerated.Schema;

internal static class Program {
  // Edit an owning model and compare complete values after each protocol round trip.
  private static void Main(string[] args) {
    {
      var typed = new Uint16Version();
      if (!typed.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(new byte[] {44, 1})) throw new Exception("Revision width");
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var copy = Uint16Version.Decode(typed.Encode(protocol), protocol);
        if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(typed.Encode(Protocol.BINARY_NONE))) throw new Exception("Revision mismatch");
      }
    }
    {
      var typed = new Uint32Version();
      if (!typed.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(new byte[] {112, 17, 1, 0})) throw new Exception("Revision width");
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var copy = Uint32Version.Decode(typed.Encode(protocol), protocol);
        if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(typed.Encode(Protocol.BINARY_NONE))) throw new Exception("Revision mismatch");
      }
    }
    {
      var typed = new Uint64Version();
      if (!typed.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(new byte[] {255, 255, 255, 255, 255, 255, 255, 255})) throw new Exception("Revision width");
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var copy = Uint64Version.Decode(typed.Encode(protocol), protocol);
        if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(typed.Encode(Protocol.BINARY_NONE))) throw new Exception("Revision mismatch");
      }
    }
    {
      var typed = new FloatVersion();
      if (!typed.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(new byte[] {205, 204, 204, 61})) throw new Exception("Revision width");
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var copy = FloatVersion.Decode(typed.Encode(protocol), protocol);
        if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(typed.Encode(Protocol.BINARY_NONE))) throw new Exception("Revision mismatch");
      }
    }
    {
      var typed = new DoubleVersion();
      if (!typed.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(new byte[] {0, 0, 0, 0, 0, 0, 4, 64})) throw new Exception("Revision width");
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var copy = DoubleVersion.Decode(typed.Encode(protocol), protocol);
        if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(typed.Encode(Protocol.BINARY_NONE))) throw new Exception("Revision mismatch");
      }
    }
    {
      var typed = new Dotted2Version();
      if (!typed.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(new byte[] {1, 0, 10, 0})) throw new Exception("Revision width");
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var copy = Dotted2Version.Decode(typed.Encode(protocol), protocol);
        if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(typed.Encode(Protocol.BINARY_NONE))) throw new Exception("Revision mismatch");
      }
    }
    {
      var typed = new Dotted3Version();
      if (!typed.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(new byte[] {1, 0, 10, 0, 0, 0})) throw new Exception("Revision width");
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var copy = Dotted3Version.Decode(typed.Encode(protocol), protocol);
        if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(typed.Encode(Protocol.BINARY_NONE))) throw new Exception("Revision mismatch");
      }
    }
    {
      var typed = new Dotted4Version();
      if (!typed.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(new byte[] {1, 0, 10, 0, 0, 0, 4, 0})) throw new Exception("Revision width");
      foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
        var copy = Dotted4Version.Decode(typed.Encode(protocol), protocol);
        if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(typed.Encode(Protocol.BINARY_NONE))) throw new Exception("Revision mismatch");
      }
    }
    var value = ExampleModel.Decode(File.ReadAllBytes(args[0]), Protocol.JSON, new Limits(readPolicy: ReadPolicy.COMPATIBLE));
    var old = ExampleModel.Decode(value.Encode(Protocol.BINARY_NONE), Protocol.BINARY_NONE, new Limits(readPolicy: ReadPolicy.COMPATIBLE));
    if (old.Version != 8 || old.OldName != "Ada") throw new Exception("Historical mismatch");
    value.Name = value.OldName;
    value.Version = 10;
    var canonical = value.Encode(Protocol.BINARY_NONE);
    foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
      var copy = ExampleModel.Decode(value.Encode(protocol), protocol);
      if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(canonical)) throw new Exception("Value mismatch");
    }
    File.WriteAllBytes(args[1], value.Encode(Protocol.JSON));
    Console.WriteLine("Version migration and four protocols passed");
  }
}
