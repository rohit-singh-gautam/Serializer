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

  // Verify scalar split tokens, exact shape errors and transactional resource failures.
  private static void CheckTokens(ShapesRecord value, byte[] encoded) {
    string document = Encoding.UTF8.GetString(encoded);
    if (ShapesRecord.DecodeXml(Encoding.UTF8.GetBytes(document.Replace(">1.25<", ">1<![CDATA[.]]>&#50;5<"))).Small != 1.25f) throw new Exception("Split scalar tokens");
    foreach (var failure in new[] {
        (Document: document.Replace("</record>", "<unexpected/></record>"), Message: "Unexpected XML field"),
        (Document: document.Replace(">1.25<", ">broken<"), Message: "Invalid XML floating value")}) {
      try { ShapesRecord.DecodeXml(Encoding.UTF8.GetBytes(failure.Document)); throw new Exception("Invalid token candidate published"); }
      catch (FormatException expected) { if (expected.Message != failure.Message) throw new Exception("Changed token diagnostic", expected); }
    }
    if (!value.EncodeXml(new Limits(maxBytes: encoded.Length)).AsSpan().SequenceEqual(encoded)) throw new Exception("Exact output byte boundary");
    foreach (var limits in new[] {new Limits(maxBytes: encoded.Length - 1), new Limits(maxStringBytes: 1), new Limits(maxElements: 1), new Limits(maxDepth: 1)}) {
      try { value.EncodeXml(limits); throw new Exception("Oversized token output published"); } catch (FormatException) { }
      try { ShapesRecord.DecodeXml(encoded, limits); throw new Exception("Oversized token input published"); } catch (FormatException) { }
    }
    Rejected("<!DOCTYPE record [<!ENTITY local 'expanded'>]>" + document);
    Rejected(document + "<second/>");
  }

  // Check complete model values and selected prepared runtime XSD enforcement.
  private static void Main() {
    string namesXml = "<r:names xmlns:r='urn:root' xmlns:a='urn:attribute' xml:lang='en' a:qualified='A&amp;B'><unqualified>plain</unqualified><f:foreign xmlns:f='urn:foreign'>foreign</f:foreign></r:names>";
    var names = ShapesNames.DecodeXml(Encoding.UTF8.GetBytes(namesXml));
    if (names.Language != "en" || names.Qualified != "A&B" || names.Unqualified != "plain" || names.Foreign != "foreign") throw new Exception("Expanded namespace input");
    if (!names.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(ShapesNames.DecodeXml(names.EncodeXml()).Encode(Protocol.BINARY_NONE))) throw new Exception("Expanded namespace output");
    var integerBox = ShapesIntegerBox.DecodeXml(Encoding.UTF8.GetBytes("<integer_box><value>4294967295</value></integer_box>"));
    if (integerBox.Value != uint.MaxValue) throw new Exception("Concrete numeric XML generic");
    var textBox = ShapesTextBox.DecodeXml(Encoding.UTF8.GetBytes("<text_box><value>A&amp;B</value></text_box>"));
    if (textBox.Value != "A&B" || ShapesTextBox.DecodeXml(textBox.EncodeXml()).Value != textBox.Value) throw new Exception("Concrete text XML generic");
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
    CheckTokens(value, encoded);
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
