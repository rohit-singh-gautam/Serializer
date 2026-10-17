using System;
using System.Collections.Generic;
using System.Text;
using static SerializerGenerated.Schema;

// Qualify generated nested, optional, numeric, enum and fixed-array XML mappings.
internal static class Program {
  // Require invalid scalar lexical values to fail before publishing a model.
  private static void Rejected(string document) {
    try {
      ShapesRecord.DecodeXml(Encoding.UTF8.GetBytes(document));
    } catch (FormatException) {
      return;
    }
    throw new Exception("Invalid XML scalar accepted");
  }

  // Check complete model values and selected prepared runtime XSD enforcement.
  private static void Main() {
    string catalogXml = "<catalog xmlns='urn:serializer:vocabulary'><prices><price currency='USD'>12.5</price></prices><labels><label>A&amp;B</label><label/></labels></catalog>";
    var catalog = ShapesCatalog.DecodeXml(Encoding.UTF8.GetBytes(catalogXml));
    if (catalog.Prices.Count != 1 || catalog.Prices[0].Value != 12.5 || catalog.Prices[0].Currency != "USD" || catalog.Labels[0] != "A&B") throw new Exception("Simple content/wrappers");
    if (!catalog.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(ShapesCatalog.DecodeXml(catalog.EncodeXml()).Encode(Protocol.BINARY_NONE))) throw new Exception("Vocabulary roundtrip");
    if (ShapesCatalog.DecodeXml(new ShapesCatalog().EncodeXml()).Prices.Count != 0) throw new Exception("Empty wrappers");
    try { ShapesCatalog.DecodeXml(Encoding.UTF8.GetBytes(catalogXml.Replace("<prices>", "<prices extra='1'>"))); throw new Exception("Unknown wrapper attribute"); } catch (FormatException) { /* Expected contract rejection. */ }
    var value = new ShapesRecord {
      Big = ulong.MaxValue,
      Label = "\tX\r\n\U0001f600",
      Flag = new ValueTuple<bool>(true),
      Amount = double.PositiveInfinity,
      Small = 1.25f
    };
    value.Nested.Value = -32768;
    value.Children.Add(new ShapesChild { Value = 32767 });
    value.States = new List<ShapesState> { (ShapesState)0, (ShapesState)1 };
    byte[] encoded = value.EncodeXml();
    var copy = ShapesRecord.DecodeXml(encoded);
    if (!copy.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(value.Encode(Protocol.BINARY_NONE))) throw new Exception("Typed roundtrip");
    value.Flag = new ValueTuple<bool>(false);
    var presentFalse = ShapesRecord.DecodeXml(value.EncodeXml()).Flag;
    if (!presentFalse.HasValue || presentFalse.Value.Item1) throw new Exception("Optional false presence");
    value.Flag = null;
    if (ShapesRecord.DecodeXml(value.EncodeXml()).Flag.HasValue) throw new Exception("Optional absence");
    byte[] zero = Encoding.UTF8.GetBytes(Encoding.UTF8.GetString(encoded).Replace("big=\"18446744073709551615\"", "big=\"-000\""));
    if (ShapesRecord.DecodeXml(zero).Big != 0) throw new Exception("Unsigned negative zero");
    string document = Encoding.UTF8.GetString(encoded);
    foreach (string invalid in new[] { "-", "18446744073709551616", "\u00a07" }) {
      string changed = document.Replace("big=\"18446744073709551615\"", "big=\"" + invalid + "\"");
      if (changed == document) throw new Exception("Unsigned probe did not change XML");
      Rejected(changed);
    }
    Rejected(document.Replace("flag=\"true\"", "flag=\"TRUE\""));
    Rejected(document.Replace(">1.25<", ">1e39<"));
    string xsd = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='record' type='xs:string'/></xs:schema>";
    var validator = new XmlSchema(Encoding.UTF8.GetBytes(xsd));
    try {
      ShapesRecord.DecodeXml(encoded, new Limits(), validator);
      throw new Exception("Input runtime XSD was ignored");
    } catch (FormatException) { }
    try {
      value.EncodeXml(new Limits(), validator);
      throw new Exception("Output runtime XSD was ignored");
    } catch (FormatException) { }
    Console.WriteLine("C# generated typed mapping and runtime XSD checks passed");
  }
}
