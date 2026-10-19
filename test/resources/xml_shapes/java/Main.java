import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Optional;

/** Qualify generated nested, optional, numeric, enum and fixed-array XML mappings. */
public final class Main {
  /** Require invalid scalar lexical values to fail before publishing a model. */
  private static void rejected(String document) {
    try {
      Schema.Shapes.Record.decodeXml(document.getBytes(StandardCharsets.UTF_8));
    } catch (IllegalArgumentException expected) {
      return;
    }
    throw new AssertionError("Invalid XML scalar accepted");
  }

  /** Check split tokens, exact shape failures and publication at every resource boundary. */
  private static void checkTokens(Schema.Shapes.Record value, byte[] encoded) {
    String document = new String(encoded, StandardCharsets.UTF_8);
    if (Schema.Shapes.Record.decodeXml(document.replace(">1.25<", ">1<![CDATA[.]]>&#50;5<").getBytes(StandardCharsets.UTF_8)).small != 1.25f) throw new AssertionError("Split scalar tokens");
    String[][] failures = {
      {document.replace("</record>", "<unexpected/></record>"), "Unexpected XML field"},
      {document.replace(">1.25<", ">broken<"), "Invalid XML floating value"}
    };
    for (var failure : failures) {
      try { Schema.Shapes.Record.decodeXml(failure[0].getBytes(StandardCharsets.UTF_8)); throw new AssertionError("Invalid token candidate published"); }
      catch (IllegalArgumentException expected) { if (!expected.getMessage().equals(failure[1])) throw new AssertionError("Changed token diagnostic", expected); }
    }
    var defaults = Schema.Limits.defaults();
    var exact = new Schema.Limits(encoded.length, defaults.maxStringBytes, defaults.maxElements, defaults.maxDepth);
    if (!Arrays.equals(value.encodeXml(exact, null), encoded)) throw new AssertionError("Exact output byte boundary");
    for (var limits : new Schema.Limits[]{
        new Schema.Limits(encoded.length - 1, defaults.maxStringBytes, defaults.maxElements, defaults.maxDepth),
        new Schema.Limits(defaults.maxBytes, 1, defaults.maxElements, defaults.maxDepth),
        new Schema.Limits(defaults.maxBytes, defaults.maxStringBytes, 1, defaults.maxDepth),
        new Schema.Limits(defaults.maxBytes, defaults.maxStringBytes, defaults.maxElements, 1)}) {
      try { value.encodeXml(limits, null); throw new AssertionError("Oversized token output published"); } catch (IllegalArgumentException expected) { }
      try { Schema.Shapes.Record.decodeXml(encoded, limits, null); throw new AssertionError("Oversized token input published"); } catch (IllegalArgumentException expected) { }
    }
    rejected("<!DOCTYPE record [<!ENTITY local 'expanded'>]>" + document);
    rejected(document + "<second/>");
  }

  /** Check complete model values and selected prepared runtime XSD enforcement. */
  public static void main(String[] args) {
    String namesXml = "<r:names xmlns:r='urn:root' xmlns:a='urn:attribute' xml:lang='en' a:qualified='A&amp;B'><unqualified>plain</unqualified><f:foreign xmlns:f='urn:foreign'>foreign</f:foreign></r:names>";
    var names = Schema.Shapes.Names.decodeXml(namesXml.getBytes(StandardCharsets.UTF_8));
    if (!names.language.equals("en") || !names.qualified.equals("A&B") || !names.unqualified.equals("plain") || !names.foreign.equals("foreign")) throw new AssertionError("Expanded namespace input");
    if (!Arrays.equals(names.encode(Schema.Protocol.BINARY_NONE), Schema.Shapes.Names.decodeXml(names.encodeXml()).encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Expanded namespace output");
    var integerBox = Schema.Shapes.IntegerBox.decodeXml("<integer_box><value>4294967295</value></integer_box>".getBytes(StandardCharsets.UTF_8));
    if (integerBox.value != -1) throw new AssertionError("Concrete numeric XML generic");
    var textBox = Schema.Shapes.TextBox.decodeXml("<text_box><value>A&amp;B</value></text_box>".getBytes(StandardCharsets.UTF_8));
    if (!textBox.value.equals("A&B") || !Schema.Shapes.TextBox.decodeXml(textBox.encodeXml()).value.equals(textBox.value)) throw new AssertionError("Concrete text XML generic");
    String catalogXml = "<catalog xmlns='urn:serializer:vocabulary'><prices><price currency='USD'>12.5</price></prices><labels><label>A&amp;B</label><label/></labels></catalog>";
    var catalog = Schema.Shapes.Catalog.decodeXml(catalogXml.getBytes(StandardCharsets.UTF_8));
    if (catalog.prices.length != 1 || catalog.prices[0].value != 12.5 || !catalog.prices[0].currency.equals("USD") || !catalog.labels[0].equals("A&B")) throw new AssertionError("Simple content/wrappers");
    if (!Arrays.equals(catalog.encode(Schema.Protocol.BINARY_NONE), Schema.Shapes.Catalog.decodeXml(catalog.encodeXml()).encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Vocabulary roundtrip");
    if (Schema.Shapes.Catalog.decodeXml(new Schema.Shapes.Catalog().encodeXml()).prices.length != 0) throw new AssertionError("Empty wrappers");
    try { Schema.Shapes.Catalog.decodeXml(catalogXml.replace("<prices>", "<prices extra='1'>").getBytes(StandardCharsets.UTF_8)); throw new AssertionError("Unknown wrapper attribute"); } catch (IllegalArgumentException expected) { /* Expected contract rejection. */ }
    var value = new Schema.Shapes.Record();
    value.big = -1L;
    value.label = "\tX\r\n\ud83d\ude00";
    value.flag = Optional.of(true);
    value.nested.value = -32768;
    var child = new Schema.Shapes.Child();
    child.value = 32767;
    value.children = new Schema.Shapes.Child[]{child};
    value.states = new Schema.Shapes.State[]{Schema.Shapes.State.ZERO, Schema.Shapes.State.ONE};
    value.amount = Double.POSITIVE_INFINITY;
    value.small = 1.25f;
    byte[] encoded = value.encodeXml();
    checkTokens(value, encoded);
    var copy = Schema.Shapes.Record.decodeXml(encoded);
    if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), value.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Typed roundtrip");
    value.flag = Optional.of(false);
    var presentFalse = Schema.Shapes.Record.decodeXml(value.encodeXml()).flag;
    if (presentFalse.isEmpty() || presentFalse.get()) throw new AssertionError("Optional false presence");
    value.flag = Optional.empty();
    if (Schema.Shapes.Record.decodeXml(value.encodeXml()).flag.isPresent()) throw new AssertionError("Optional absence");
    byte[] zero = new String(encoded, StandardCharsets.UTF_8).replace("big=\"18446744073709551615\"", "big=\"-000\"").getBytes(StandardCharsets.UTF_8);
    if (Schema.Shapes.Record.decodeXml(zero).big != 0) throw new AssertionError("Unsigned negative zero");
    String document = new String(encoded, StandardCharsets.UTF_8);
    for (String invalid : new String[]{"-", "18446744073709551616", "\u00a07"}) {
      String changed = document.replace("big=\"18446744073709551615\"", "big=\"" + invalid + "\"");
      if (changed.equals(document)) throw new AssertionError("Unsigned probe did not change XML");
      rejected(changed);
    }
    rejected(document.replace("flag=\"true\"", "flag=\"TRUE\""));
    rejected(document.replace(">1.25<", ">1e39<"));
    String xsd = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='record' type='xs:string'/></xs:schema>";
    var validator = new Schema.XmlSchema(xsd.getBytes(StandardCharsets.UTF_8));
    try {
      Schema.Shapes.Record.decodeXml(encoded, Schema.Limits.defaults(), validator);
      throw new AssertionError("Input runtime XSD was ignored");
    } catch (IllegalArgumentException expected) { }
    try {
      value.encodeXml(Schema.Limits.defaults(), validator);
      throw new AssertionError("Output runtime XSD was ignored");
    } catch (IllegalArgumentException expected) { }
    System.out.println("Java generated typed mapping and runtime XSD checks passed");
  }
}
