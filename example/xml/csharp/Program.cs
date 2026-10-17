using System;
using System.IO;
using System.Linq;
using System.Text;
using static SerializerGenerated.Schema;

// Use generated XML and binary codecs with the same owning C# model.
internal static class Program {
  // Require malformed or over-budget input to return no owning candidate.
  private static void Rejected(byte[] bytes, Limits limits) {
    try {
      XmlExampleOrder.DecodeXml(bytes, limits);
    } catch (Exception error) when (error is FormatException || error is ArgumentException || error is System.Xml.XmlException) {
      return;
    }
    throw new Exception("Invalid XML was accepted");
  }

  // Check the shared XML contract, complete generated roundtrip, and binary side channels.
  private static void Main(string[] args) {
    if (args.Length != 2) throw new ArgumentException("Expected INPUT OUTPUT");
    if (new FileInfo(args[0]).Length > 1024 * 1024) throw new ArgumentException("Example input exceeds one MiB");
    byte[] bytes = File.ReadAllBytes(args[0]);
    var value = XmlExampleOrder.DecodeXml(bytes);
    if (value.Id != 7 || value.Customer != "R&D" || value.Count != 3 || !value.Tags.SequenceEqual(new[] { "A", "B" })) throw new Exception("Fixture mismatch");
    byte[] canonical = value.Encode(Protocol.BINARY_NONE);
    byte[] encoded = value.EncodeXml();
    if (!XmlExampleOrder.DecodeXml(encoded).Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(canonical)) throw new Exception("XML roundtrip");
    if (!XmlExampleOrder.Decode(canonical, Protocol.BINARY_NONE).EncodeXml().AsSpan().SequenceEqual(encoded)) throw new Exception("Binary side channel");
    string text = "<order xmlns=\"urn:serializer:xml-example\" id=\"7\"><customer>R&amp;D</customer><count>3</count><tag>A</tag><tag>B</tag></order>";
    string[] invalid = {
      text.Replace("urn:serializer:xml-example", "urn:wrong"),
      text.Replace("<order ", "<wrong ").Replace("</order>", "</wrong>"),
      text.Replace("<count>3</count>", "<count>2147483648</count>"),
      text.Replace("<count>3</count>", "<count>+-1</count>"),
      "<!DOCTYPE order [<!ENTITY attack 'bad'>]><order>&attack;</order>",
      Encoding.UTF8.GetString(encoded) + "<extra/>"
    };
    foreach (string document in invalid) Rejected(Encoding.UTF8.GetBytes(document), new Limits());
    Rejected(bytes, new Limits(maxBytes: 1));
    File.WriteAllBytes(args[1], encoded);
    Console.WriteLine("Generated C# XML and binary side-channel checks passed");
  }
}
