using System;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using static SerializerGenerated.Schema;

// Qualify immutable System.Xml schemas through the generated owning XML codec.
internal static class Program {
  private const string Xml = "<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>";
  private const string XsdStart = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>";

  // Require provider failure without accepting the assertion raised for a successful action.
  private static void Rejected(Action action) {
    try { action(); } catch (FormatException) { return; } catch (System.Xml.XmlException) { return; } catch (System.Xml.Schema.XmlSchemaException) { return; }
    throw new Exception("Provider accepted invalid input");
  }

  // Exercise complete bytes, selected directions, resource bounds and concurrent reuse.
  private static void Main(string[] args) {
    var schema = new XmlSchema(File.ReadAllBytes(args[0]));
    var limits = new Limits();
    byte[] bytes = Encoding.UTF8.GetBytes(Xml);
    var value = XmlProviderFixtureOrder.DecodeXml(bytes, limits, schema);
    if (value.Id != 7 || value.Customer != "R&D" || value.Values[0] != 3) throw new Exception("Generated values");
    schema.Validate(value.EncodeXml(limits, schema), limits);
    foreach (string bad in new[] {"<wrong xmlns='urn:serializer:xsd'/>",
        Xml.Replace("urn:serializer:xsd", "urn:wrong"), Xml + "<extra/>",
        Xml.Replace("<value>3</value>", ""), "<!DOCTYPE order SYSTEM 'file:///must-not-open'>" + Xml}) {
      Rejected(() => schema.Validate(Encoding.UTF8.GetBytes(bad), limits));
    }
    var mismatch = new XmlSchema(Encoding.UTF8.GetBytes("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:serializer:xsd'><xs:element name='order' type='xs:string'/></xs:schema>"));
    Rejected(() => value.EncodeXml(limits, mismatch));
    Rejected(() => XmlProviderFixtureOrder.DecodeXml(bytes, limits, mismatch));
    foreach (string directive in new[] {"include", "import", "redefine", "override"}) {
      foreach (string location in new[] {"file:///must-not-open.xsd", "https://invalid.example/must-not-open.xsd"}) {
        string source = XsdStart + "<xs:" + directive + " schemaLocation='" + location + "'/></xs:schema>";
        Rejected(() => new XmlSchema(Encoding.UTF8.GetBytes(source)));
      }
    }
    string hinted = Xml.Replace(" id=", " xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:schemaLocation='urn:serializer:xsd https://invalid.example/must-not-open.xsd' id=");
    schema.Validate(Encoding.UTF8.GetBytes(hinted), limits);
    foreach (var bounded in new[] {new Limits(maxBytes: bytes.Length - 1), new Limits(maxStringBytes: 1), new Limits(maxElements: 1), new Limits(maxDepth: 1)}) {
      Rejected(() => schema.Validate(bytes, bounded));
    }
    Parallel.For(0, 8, _ => {
      for (int index = 0; index < 16; ++index) {
        schema.Validate(bytes, limits);
        Rejected(() => schema.Validate(Encoding.UTF8.GetBytes("<wrong/>"), limits));
      }
    });
    Console.WriteLine("C# prepared XSD provider checks passed");
  }
}
