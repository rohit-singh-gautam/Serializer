// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmljava(
/** One bounded data-XML node; generated models never use reflection. */
private static final class SrlXmlNode {
  final String name;
  final java.util.Map<String, String> attributes = new java.util.LinkedHashMap<>();
  final java.util.List<SrlXmlNode> children = new java.util.ArrayList<>();
  final StringBuilder text = new StringBuilder();
  /** Retain the statically selected expanded name. */
  SrlXmlNode(String name) { this.name = name; }
}
/** Shared complete-document limits for parsing and generated tree construction. */
private static final class SrlXmlBudget {
  final Limits limits;
  int nodes, attributes, textBytes;
  /** Require a validated policy before accepting XML bytes or model fields. */
  SrlXmlBudget(Limits limits) { this.limits = java.util.Objects.requireNonNull(limits); }
  /** Reject non-XML Unicode and cumulatively bound UTF-8 text before retention. */
  String text(String text) {
    java.util.Objects.requireNonNull(text);
    for (int offset = 0; offset < text.length();) {
      int cp = text.codePointAt(offset); offset += Character.charCount(cp);
      if (!(cp == 9 || cp == 10 || cp == 13 || cp >= 0x20 && cp <= 0xd7ff || cp >= 0xe000 && cp <= 0xfffd || cp >= 0x10000 && cp <= 0x10ffff)) throw new IllegalArgumentException("Invalid XML1.0 character");
    }
    int size = utf8(text).length;
    if (size > limits.maxStringBytes - textBytes) throw new IllegalArgumentException("XML text limit exceeded");
    textBytes += size; return text;
  }
  /** Charge a node before allocating its descendants. */
  SrlXmlNode node(String name, int depth) {
    if (depth > limits.maxDepth || nodes >= limits.maxElements) throw new IllegalArgumentException("XML node or depth limit exceeded");
    ++nodes; return new SrlXmlNode(text(name));
  }
  /** Charge attributes and their names/values before adding them to the tree. */
  void attribute(SrlXmlNode node, String name, String value) {
    if (attributes >= limits.maxElements) throw new IllegalArgumentException("XML attribute limit exceeded");
    ++attributes;
    if (node.attributes.put(text(name), text(value)) != null) throw new IllegalArgumentException("Duplicate XML attribute");
  }
}
/** Construct an expanded name without depending on document prefix spelling. */
private static String srlXmlName(String uri, String local) { return uri == null || uri.isEmpty() ? local : "{" + uri + "}" + local; }
/** Accept only XML whitespace around scalar lexical values and record indentation. */
private static String srlXmlTrim(String text) { return text.replaceAll("^[ \\t\\r\\n]+|[ \\t\\r\\n]+$", ""); }
/** Parse one exact UTF-8 XML1.0 document with bounded SAX callbacks and no DTDs or entities. */
private static SrlXmlNode srlXmlParse(byte[] bytes, Limits limits) {
  if (bytes.length > limits.maxBytes) throw new IllegalArgumentException("XML byte limit exceeded");
  String decoded = utf8(bytes, 0, bytes.length);
  var encoding = java.util.regex.Pattern.compile("(?s)^\\uFEFF?<\\?xml\\s+[^?]*\\bencoding\\s*=\\s*(['\"])([^'\"]+)\\1").matcher(decoded);
  if (encoding.find() && !encoding.group(2).replace("-", "").equalsIgnoreCase("UTF8")) throw new IllegalArgumentException("XML UTF8 required");
  try {
    var factory = javax.xml.parsers.SAXParserFactory.newInstance(); factory.setNamespaceAware(true);
    factory.setFeature(javax.xml.XMLConstants.FEATURE_SECURE_PROCESSING, true);
    factory.setFeature("http://apache.org/xml/features/disallow-doctype-decl", true);
    factory.setFeature("http://xml.org/sax/features/external-general-entities", false);
    factory.setFeature("http://xml.org/sax/features/external-parameter-entities", false);
    factory.setFeature("http://apache.org/xml/features/nonvalidating/load-external-dtd", false);
    var reader = factory.newSAXParser().getXMLReader();
    reader.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, "");
    reader.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, "");
    var budget = new SrlXmlBudget(limits);
    var stack = new java.util.ArrayList<SrlXmlNode>();
    var roots = new java.util.ArrayList<SrlXmlNode>();
    var handler = new org.xml.sax.helpers.DefaultHandler() {
      org.xml.sax.Locator locator;
      /** Retain provider version metadata without global parser state. */
      @Override public void setDocumentLocator(org.xml.sax.Locator value) { locator = value; }
      /** Charge namespace declarations although fixed mappings retain only expanded names. */
      @Override public void startPrefixMapping(String prefix, String uri) {
        if (budget.attributes >= limits.maxElements) throw new IllegalArgumentException("XML namespace limit exceeded");
        ++budget.attributes; budget.text(prefix); budget.text(uri);
      }
      /** Bound each element and attribute before linking the new node. */
      @Override public void startElement(String uri, String local, String raw, org.xml.sax.Attributes attrs) {
        if (locator instanceof org.xml.sax.ext.Locator2 details && details.getXMLVersion() != null && !details.getXMLVersion().equals("1.0")) throw new IllegalArgumentException("XML1.0 required");
        var node = budget.node(srlXmlName(uri, local), stack.size() + 1);
        for (int index = 0; index < attrs.getLength(); ++index) budget.attribute(node, srlXmlName(attrs.getURI(index), attrs.getLocalName(index)), attrs.getValue(index));
        if (stack.isEmpty()) roots.add(node); else stack.get(stack.size() - 1).children.add(node);
        stack.add(node);
      }
      /** Retain only cumulatively bounded character data. */
      @Override public void characters(char[] value, int offset, int length) {
        String text = budget.text(new String(value, offset, length));
        if (!stack.isEmpty()) stack.get(stack.size() - 1).text.append(text);
        else if (!srlXmlTrim(text).isEmpty()) throw new IllegalArgumentException("XML text outside root");
      }
      /** Reject mixed text before returning the parsed tree. */
      @Override public void endElement(String uri, String local, String raw) {
        var node = stack.remove(stack.size() - 1);
        if (!node.children.isEmpty() && !srlXmlTrim(node.text.toString()).isEmpty()) throw new IllegalArgumentException("XML mixed content is unsupported");
      }
      /** External references are rejected even if a provider changes its default feature policy. */
      @Override public org.xml.sax.InputSource resolveEntity(String publicId, String systemId) { throw new IllegalArgumentException("External XML resolution is prohibited"); }
      /** Keep provider diagnostics from reaching stderr or including document contents. */
      @Override public void error(org.xml.sax.SAXParseException error) throws org.xml.sax.SAXException { throw error; }
      /** Treat all fatal syntax diagnostics as failure of the unpublished candidate. */
      @Override public void fatalError(org.xml.sax.SAXParseException error) throws org.xml.sax.SAXException { throw error; }
    };
    reader.setContentHandler(handler); reader.setEntityResolver(handler); reader.setErrorHandler(handler);
    var input = new org.xml.sax.InputSource(new java.io.ByteArrayInputStream(bytes)); input.setEncoding("UTF-8"); reader.parse(input);
    if (roots.size() != 1 || !stack.isEmpty()) throw new IllegalArgumentException("XML requires exactly one root");
    return roots.get(0);
  } catch (java.io.IOException | org.xml.sax.SAXException | javax.xml.parsers.ParserConfigurationException failure) { throw new IllegalArgumentException("Malformed or unsupported XML document", failure); }
}
/** A scalar field cannot discard nested elements or attributes. */
private static String srlXmlScalarText(SrlXmlNode node) { if (!node.attributes.isEmpty() || !node.children.isEmpty()) throw new IllegalArgumentException("XML scalar contains attributes or children"); return node.text.toString(); }
/** Strict XML integer parsing retains unsigned model bits without narrowing validation. */
private static long srlXmlInteger(String text, int bits, boolean unsigned) {
  text = srlXmlTrim(text); if (!text.matches("[+-]?[0-9]+")) throw new IllegalArgumentException("Invalid XML integer");
  var value = new java.math.BigInteger(text);
  var minimum = unsigned ? java.math.BigInteger.ZERO : java.math.BigInteger.ONE.shiftLeft(bits - 1).negate();
  var maximum = java.math.BigInteger.ONE.shiftLeft(unsigned ? bits : bits - 1).subtract(java.math.BigInteger.ONE);
  if (value.compareTo(minimum) < 0 || value.compareTo(maximum) > 0) throw new IllegalArgumentException("XML integer out of range"); return value.longValue();
}
/** Strict XML Boolean parsing does not coerce unknown lexical values. */
private static boolean srlXmlBoolean(String text) { return switch (srlXmlTrim(text)) { case "true", "1" -> true; case "false", "0" -> false; default -> throw new IllegalArgumentException("Invalid XML Boolean"); }; }
/** Preserve XSD special floating values while rejecting permissive Java spellings and overflow. */
private static double srlXmlFloat(String text, boolean single) {
  text = srlXmlTrim(text);
  if (text.equals("NaN")) return Double.NaN;
  if (text.equals("INF")) return Double.POSITIVE_INFINITY;
  if (text.equals("-INF")) return Double.NEGATIVE_INFINITY;
  if (!text.matches("[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?")) throw new IllegalArgumentException("Invalid XML floating value");
  double value = single ? Float.parseFloat(text) : Double.parseDouble(text);
  if (!Double.isFinite(value)) throw new IllegalArgumentException("XML floating value overflows model"); return value;
}
/** Emit XML/XSD floating lexical forms independent of locale. */
private static String srlXmlFloatText(double value, boolean single) { return Double.isNaN(value) ? "NaN" : value == Double.POSITIVE_INFINITY ? "INF" : value == Double.NEGATIVE_INFINITY ? "-INF" : single ? Float.toString((float)value) : Double.toString(value); }
/** Enforce exact declared attributes and ordered child occurrences. */
private static final class SrlXmlFields {
  final SrlXmlNode node;
  final java.util.Set<String> used = new java.util.HashSet<>();
  int next;
  /** Reject text where a generated record expects fields. */
  SrlXmlFields(SrlXmlNode node) { this.node = node; if (!srlXmlTrim(node.text.toString()).isEmpty()) throw new IllegalArgumentException("Unexpected XML record text"); }
  /** Read a required or optional attribute without losing absence. */
  String attribute(String name, boolean optional) { String value = node.attributes.get(name); if (value == null && !optional) throw new IllegalArgumentException("Required XML attribute missing"); if (value != null && !used.add(name)) throw new IllegalArgumentException("Repeated XML attribute mapping"); return value; }
  /** Consume only contiguous children under statically selected cardinality. */
  java.util.List<SrlXmlNode> children(String name, int minimum, int maximum) { int begin = next; while (next < node.children.size() && node.children.get(next).name.equals(name)) ++next; int count = next - begin; if (count < minimum || maximum >= 0 && count > maximum) throw new IllegalArgumentException("XML field occurrence mismatch"); return node.children.subList(begin, next); }
  /** Reject unknown, duplicate and misplaced members before model publication. */
  void finish() { if (used.size() != node.attributes.size() || next != node.children.size()) throw new IllegalArgumentException("Unexpected XML field"); }
}
/** Emit deterministic XML1.0 bytes with bounded escaped output. */
private static byte[] srlXmlWrite(SrlXmlNode root, Limits limits) {
  final class Emitter {
    final java.util.Map<String, String> prefixes = new java.util.LinkedHashMap<>();
    final java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
    /** Split a compiler-owned expanded name. */
)xmljava"
R"xmljava(    String[] name(String value) { int close = value.startsWith("{") ? value.indexOf('}') : -1; return close < 0 ? new String[]{"", value} : new String[]{value.substring(1, close), value.substring(close + 1)}; }
    /** Collect namespace identities used by statically generated nodes. */
    void inspect(SrlXmlNode node) { var names = new java.util.ArrayList<>(node.attributes.keySet()); names.add(node.name); for (var selected : names) { String uri = name(selected)[0]; if (!uri.isEmpty()) prefixes.computeIfAbsent(uri, key -> key.equals("http://www.w3.org/XML/1998/namespace") ? "xml" : "n" + prefixes.size()); } for (var child : node.children) inspect(child); }
    /** Check encoded length before publishing the next fragment to owned storage. */
    void append(String text) { byte[] chunk = utf8(text); if (chunk.length > limits.maxBytes - bytes.size()) throw new IllegalArgumentException("XML output byte limit exceeded"); bytes.writeBytes(chunk); }
    /** Resolve a presentation name while preserving its expanded identity. */
    String qualified(String value) { var parts = name(value); return parts[0].isEmpty() ? parts[1] : prefixes.get(parts[0]) + ":" + parts[1]; }
    /** Escape only XML syntax and normalized whitespace, keeping ordinary runs batched. */
    void escaped(String text, boolean attribute) { int begin = 0; for (int i = 0; i < text.length(); ++i) { String replacement = switch (text.charAt(i)) { case '&' -> "&amp;"; case '<' -> "&lt;"; case '>' -> "&gt;"; case '\r' -> "&#13;"; case '"' -> attribute ? "&quot;" : null; case '\n' -> attribute ? "&#10;" : null; case '\t' -> attribute ? "&#9;" : null; default -> null; }; if (replacement != null) { append(text.substring(begin, i)); append(replacement); begin = i + 1; } } append(text.substring(begin)); }
    /** Emit one already bounded tree in generated field order. */
    void emit(SrlXmlNode node, boolean root) { append("<" + qualified(node.name)); if (root) for (var item : prefixes.entrySet()) if (!item.getValue().equals("xml")) { append(" xmlns:" + item.getValue() + "=\""); escaped(item.getKey(), true); append("\""); } for (var item : node.attributes.entrySet()) { append(" " + qualified(item.getKey()) + "=\""); escaped(item.getValue(), true); append("\""); } if (node.children.isEmpty() && node.text.isEmpty()) { append("/>"); return; } append(">"); escaped(node.text.toString(), false); for (var child : node.children) emit(child, false); append("</" + qualified(node.name) + ">"); }
  }
  var output = new Emitter(); output.inspect(root); output.emit(root, true); return output.bytes.toByteArray();
}
/** Optional prepared trusted standalone runtime XSD1.0 validator using the Java platform. */
public static final class XmlSchema {
  private final javax.xml.validation.Schema schema;
  /** Compile once with external schemas and DTDs prohibited. */
  public XmlSchema(byte[] xsd) {
    var tree = srlXmlParse(xsd, Limits.defaults());
    var pending = new java.util.ArrayList<SrlXmlNode>(); pending.add(tree);
    for (int i = 0; i < pending.size(); ++i) { var node = pending.get(i); if (java.util.Set.of("include", "import", "redefine", "override").stream().anyMatch(kind -> node.name.equals(srlXmlName("http://www.w3.org/2001/XMLSchema", kind)))) throw new IllegalArgumentException("Runtime XSD external references are unsupported"); pending.addAll(node.children); }
    try { var factory = javax.xml.validation.SchemaFactory.newInstance(javax.xml.XMLConstants.W3C_XML_SCHEMA_NS_URI); factory.setFeature(javax.xml.XMLConstants.FEATURE_SECURE_PROCESSING, true); factory.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, ""); factory.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, ""); schema = factory.newSchema(new javax.xml.transform.stream.StreamSource(new java.io.ByteArrayInputStream(xsd))); }
    catch (org.xml.sax.SAXException failure) { throw new IllegalArgumentException("Invalid or unsupported XSD", failure); }
  }
  /** Reuse the immutable schema with a separate bounded per-call validator. */
  public void validate(byte[] bytes, Limits limits) {
    srlXmlParse(bytes, limits);
    try { var validator = schema.newValidator(); validator.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, ""); validator.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, ""); validator.validate(new javax.xml.transform.stream.StreamSource(new java.io.ByteArrayInputStream(bytes))); }
    catch (org.xml.sax.SAXException | java.io.IOException failure) { throw new IllegalArgumentException("XML does not satisfy the selected XSD", failure); }
  }
}
)xmljava"
