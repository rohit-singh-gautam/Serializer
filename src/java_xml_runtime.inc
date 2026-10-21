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
  String field="";
  /** Admit native work before retaining parser/model state. */
  void charge(String resource,long amount){if(limits.xmlOperation!=null)limits.xmlOperation.consume(resource,amount,field);}
  /** Keep expanded-name local context without retaining payload text. */
  void select(String name){int end=name.lastIndexOf('}');field=end<0?name:name.substring(end+1);charge("work",1);}
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
    if (size > limits.maxStringBytes - textBytes) throw SrlXmlRule.quota("XML text limit exceeded");
    charge("work",size);charge("allocation",size+2L*text.length());textBytes += size; return text;
  }
  /** Charge a node before allocating its descendants. */
  SrlXmlNode node(String name, int depth) {
    if (depth > limits.maxDepth || nodes >= limits.maxElements) throw SrlXmlRule.quota("XML node or depth limit exceeded");
    select(name);charge("allocation",128);++nodes; return new SrlXmlNode(text(name));
  }
  /** Charge attributes and their names/values before adding them to the tree. */
  void attribute(SrlXmlNode node, String name, String value) {
    if (attributes >= limits.maxElements) throw SrlXmlRule.quota("XML attribute limit exceeded");
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
  if (bytes.length > limits.maxBytes) throw SrlXmlRule.quota("XML byte limit exceeded");
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
        if (budget.attributes >= limits.maxElements) throw SrlXmlRule.quota("XML namespace limit exceeded");
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
  SrlXmlFields(SrlXmlNode node) { this(node, false); }
  /** Bind simple content only when explicitly selected by the fixed mapping. */
  SrlXmlFields(SrlXmlNode node, boolean hasText) { this.node = node; if (!hasText && !srlXmlTrim(node.text.toString()).isEmpty()) throw new IllegalArgumentException("Unexpected XML record text"); }
  /** Read simple text without accepting element children. */
  String text() { if (!node.children.isEmpty()) throw new IllegalArgumentException("XML simple content contains children"); return node.text.toString(); }
  /** Read a required or optional attribute without losing absence. */
  String attribute(String name, boolean optional) { String value = node.attributes.get(name); if (value == null && !optional) throw new IllegalArgumentException("Required XML attribute missing"); if (value != null && !used.add(name)) throw new IllegalArgumentException("Repeated XML attribute mapping"); return value; }
  /** Consume only contiguous children under statically selected cardinality. */
  java.util.List<SrlXmlNode> children(String name, int minimum, int maximum) { int begin = next; while (next < node.children.size() && node.children.get(next).name.equals(name)) ++next; int count = next - begin; if (count < minimum || maximum >= 0 && count > maximum) throw new IllegalArgumentException("XML field occurrence mismatch"); return node.children.subList(begin, next); }
  /** Reject unknown, duplicate and misplaced members before model publication. */
  void finish() { if (used.size() != node.attributes.size() || next != node.children.size()) throw new IllegalArgumentException("Unexpected XML field"); }
  /** Require one collection wrapper and consume all of its attributes and children. */
)xmljava"
R"xmljava(  java.util.List<SrlXmlNode> wrapped(String wrapper, String name, int minimum, int maximum) { var fields = new SrlXmlFields(children(wrapper, 1, 1).get(0)); var result = fields.children(name, minimum, maximum); fields.finish(); return result; }
}
/** Pull one bounded input event at a time; completed subtrees are never retained. */
private static final class SrlXmlInput implements AutoCloseable {
  final SrlXmlBudget budget;
  final XmlMapLimits mapLimits;
  final XmlMixedLimits mixedLimits;
  final XmlIdentityLimits identityLimits;
  final XmlDigestLimits digestLimits;
  final XmlExtensionOptions extensionOptions;
  final java.util.ArrayList<SrlXmlIdentity> identities = new java.util.ArrayList<>();
  final javax.xml.stream.XMLStreamReader reader;
  final java.util.ArrayList<java.util.Map<String,String>> namespaceDeclarations=new java.util.ArrayList<>();
  int depth;
  int namespaceBytes;
  char kind;
  boolean acceptedType;
  boolean acceptedNil;
  /** Create a non-resolving operation-local XML1.0 UTF8 token provider. */
  SrlXmlInput(byte[] bytes, Limits limits) {
    this(bytes, limits, new XmlMapLimits());
  }
  /** Carry one immutable map policy through every nested typed decoder. */
  SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps) {
    this(bytes, limits, maps, new XmlMixedLimits());
  }
  /** Share immutable mixed admission policy while retaining independent sequence counters. */
  SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps, XmlMixedLimits mixed) { this(bytes, limits, maps, mixed, new XmlIdentityLimits()); }
  /** Carry explicit owner-local identity admission through every descendant. */
  SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity) {
    this(bytes,limits,maps,mixed,identity,new XmlDigestLimits());
  }
  /** Share immutable per-digest admission through every nested owning decoder. */
  SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests) {
    this(bytes,limits,maps,mixed,identity,digests,new XmlExtensionOptions());
  }
  /** Carry explicit opaque-vocabulary policies independently from earlier overloads. */
  SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests, XmlExtensionOptions extensions) {
    extensionOptions=java.util.Objects.requireNonNull(extensions);
    if (bytes.length > limits.maxBytes) throw SrlXmlRule.quota("XML byte limit exceeded");
    utf8(bytes, 0, bytes.length); budget = new SrlXmlBudget(limits); mapLimits = java.util.Objects.requireNonNull(maps); mixedLimits = java.util.Objects.requireNonNull(mixed); identityLimits = java.util.Objects.requireNonNull(identity); digestLimits=java.util.Objects.requireNonNull(digests);
    try {
      var factory = javax.xml.stream.XMLInputFactory.newFactory();
      factory.setProperty(javax.xml.stream.XMLInputFactory.SUPPORT_DTD, false);
      factory.setProperty(javax.xml.stream.XMLInputFactory.IS_SUPPORTING_EXTERNAL_ENTITIES, false);
      factory.setProperty(javax.xml.stream.XMLInputFactory.IS_NAMESPACE_AWARE, true);
      factory.setXMLResolver((publicId, systemId, base, namespace) -> { throw new javax.xml.stream.XMLStreamException("External XML resolution prohibited"); });
      reader = factory.createXMLStreamReader(new java.io.ByteArrayInputStream(bytes));
      String version = reader.getVersion(), encoding = reader.getCharacterEncodingScheme();
      if (version != null && !version.equals("1.0")) throw new IllegalArgumentException("XML1.0 required");
      if (encoding != null && !encoding.replace("-", "").equalsIgnoreCase("UTF8")) throw new IllegalArgumentException("XML UTF8 required");
      advance();
    } catch (javax.xml.stream.XMLStreamException failure) { throw new IllegalArgumentException("Malformed or unsupported XML document", failure); }
  }
  /** Expose a prefix-independent expanded identity for the current start token. */
  String name() { return srlXmlName(reader.getNamespaceURI(), reader.getLocalName()); }
  /** Materialize a complete scope only for an explicitly preserved owner or foreign node. */
  java.util.Map<String,String> namespaceScope(){var result=new java.util.LinkedHashMap<String,String>();result.put("xml","http://www.w3.org/XML/1998/namespace");for(var declared:namespaceDeclarations)if(declared!=null)result.putAll(declared);return result;}
  /** Expose native collection admission without exposing mutable operation counters. */
  int listLimit() { return budget.limits.maxElements; }
  /** Snapshot only the active start tag for synchronous observers, never descendants. */
  java.util.Map<String,String> identityAttributes() { var result=new java.util.LinkedHashMap<String,String>();for(int index=0;index<reader.getAttributeCount();++index)result.put(srlXmlName(reader.getAttributeNamespace(index),reader.getAttributeLocalName(index)),reader.getAttributeValue(index));return result; }
  /** Begin one nested owner scope before its already-current start token advances. */
  SrlXmlIdentity identityScope(SrlXmlIdentityScope scope) { if(kind!='s')throw new IllegalArgumentException("Missing XML identity owner");var result=new SrlXmlIdentity(scope,identityLimits,budget.limits.xmlOperation,budget.field);result.start(name(),identityAttributes(),reader::getNamespaceURI);identities.add(result);return result; }
  /** Require local forward-reference completion before publishing its native candidate. */
  void identityFinish(SrlXmlIdentity identity) { identity.finish();if(!identities.remove(identity))throw new IllegalArgumentException("Unknown XML identity scope"); }
  /** Deliver each provider event synchronously while its namespace context is still active. */
  void identityEvent() { if(identities.isEmpty())return;java.util.Map<String,String> attributes=kind=='s'?identityAttributes():null;for(var identity:identities)if(!identity.complete){if(kind=='s')identity.start(name(),attributes,reader::getNamespaceURI);else if(kind=='t')identity.text(reader.getText());else if(kind=='e')identity.end(reader::getNamespaceURI);} }
  /** Admit text, names, declarations and attributes before exposing the next event. */
  void advance() {
    if(kind=='e')namespaceDeclarations.remove(namespaceDeclarations.size()-1);
    try {
      while (reader.hasNext()) {
        int event = reader.next();
        if (event == javax.xml.stream.XMLStreamConstants.START_ELEMENT) {
          if (++depth > budget.limits.maxDepth || budget.nodes >= budget.limits.maxElements) throw SrlXmlRule.quota("XML node or depth limit exceeded");
          budget.select(name());budget.charge("allocation",128);++budget.nodes; budget.text(name());
          java.util.Map<String,String> declared=null;if(reader.getNamespaceCount()!=0){declared=new java.util.LinkedHashMap<>();for(int index=0;index<reader.getNamespaceCount();++index){String prefix=reader.getNamespacePrefix(index),uri=reader.getNamespaceURI(index);declared.put(prefix==null?"":prefix,uri==null?"":uri);}}namespaceDeclarations.add(declared);
          for (int index = 0; index < reader.getNamespaceCount(); ++index) { if (budget.attributes >= budget.limits.maxElements) throw SrlXmlRule.quota("XML namespace limit exceeded"); ++budget.attributes; String prefix = reader.getNamespacePrefix(index), uri = reader.getNamespaceURI(index); budget.text(prefix == null ? "" : prefix); budget.text(uri == null ? "" : uri); }
          for (int index = 0; index < reader.getAttributeCount(); ++index) { if (budget.attributes >= budget.limits.maxElements) throw SrlXmlRule.quota("XML attribute limit exceeded"); ++budget.attributes; budget.text(srlXmlName(reader.getAttributeNamespace(index), reader.getAttributeLocalName(index))); budget.text(reader.getAttributeValue(index)); }
          kind = 's'; identityEvent(); return;
        }
        if (event == javax.xml.stream.XMLStreamConstants.END_ELEMENT) { --depth; kind = 'e'; identityEvent(); return; }
        if (event == javax.xml.stream.XMLStreamConstants.CHARACTERS || event == javax.xml.stream.XMLStreamConstants.CDATA || event == javax.xml.stream.XMLStreamConstants.SPACE) { budget.text(reader.getText()); kind = 't'; identityEvent(); return; }
        if (event == javax.xml.stream.XMLStreamConstants.DTD || event == javax.xml.stream.XMLStreamConstants.ENTITY_REFERENCE) throw new IllegalArgumentException("XML entities and DTDs are prohibited");
      }
      kind = 0;
    } catch (javax.xml.stream.XMLStreamException failure) { throw new IllegalArgumentException("Malformed or unsupported XML document", failure); }
  }
  /** Skip only XML indentation between generated record fields. */
  void skipSpace() { while (kind == 't') { if (!srlXmlTrim(reader.getText()).isEmpty()) throw new IllegalArgumentException("Unexpected XML record text"); advance(); } }
  /** Require the exact next field identity without accepting unknown children. */
  boolean has(String name) { skipSpace(); return kind == 's' && name().equals(name); }
  /** Retain only this record's attribute strings while traversing descendants. */
  SrlXmlInputFields open(String name) { if (!has(name)) throw new IllegalArgumentException("Unexpected XML element"); var fields = new SrlXmlInputFields(this); acceptedType = false; acceptedNil = false; advance(); return fields; }
  /** Check optional QName metadata in the payload's current namespace scope before advancing. */
  void expectType(String expected, boolean required) {
    String selected = typeIdentity(required);
    if (selected != null && !selected.equals(expected)) throw new IllegalArgumentException("Conflicting XML variant type");
  }
  /** Resolve one type identity while its payload start tag still owns the namespace scope. */
  String typeIdentity(boolean required) {
    skipSpace(); if (kind != 's') throw new IllegalArgumentException("Missing XML variant payload");
    String text = reader.getAttributeValue("http://www.w3.org/2001/XMLSchema-instance", "type");
    if (text == null) { if (required) throw new IllegalArgumentException("Required XML variant type missing"); return null; }
    text = srlXmlTrim(text); String[] parts = text.split(":", -1); String prefix = "", local = text;
    if (parts.length == 2 && !parts[0].isEmpty() && !parts[1].isEmpty()) { prefix = parts[0]; local = parts[1]; }
    else if (parts.length != 1 || text.isEmpty()) throw new IllegalArgumentException("Invalid XML variant type QName");
    String uri = reader.getNamespaceURI(prefix);
    if (!prefix.isEmpty() && uri == null) throw new IllegalArgumentException("Undeclared XML variant type prefix");
    acceptedType = true;
    return srlXmlName(uri, local);
  }
  /** Distinguish absent, nil and present content before rebuilding native Optional layers. */
  char presence(String name, boolean absent, boolean nilAllowed) {
    if (!has(name)) { if (absent) return 'a'; throw new IllegalArgumentException("Required XML presence element missing"); }
    String marker = reader.getAttributeValue("http://www.w3.org/2001/XMLSchema-instance", "nil");
    if (marker == null) return 'v';
    if (!nilAllowed) throw new IllegalArgumentException("XML xsi:nil is prohibited by presence policy");
    marker = srlXmlTrim(marker);
    if (!marker.equals("true") && !marker.equals("1") && !marker.equals("false") && !marker.equals("0")) throw new IllegalArgumentException("Invalid XML xsi:nil Boolean");
    acceptedNil = true;
    if (marker.equals("false") || marker.equals("0")) return 'v';
    if (!open(name).text().isEmpty()) throw new IllegalArgumentException("Nil XML value contains text");
    return 'n';
  }
  /** Concatenate only the active scalar, including split entity and CDATA tokens. */
  String text() { var value = new StringBuilder(); while (kind == 't') { value.append(reader.getText()); advance(); } if (kind != 'e') throw new IllegalArgumentException("XML scalar contains nested content"); advance(); return value.toString(); }
  /** Reject scalar attributes before consuming text and the matching end token. */
  String scalar(String name) { var fields = open(name); if (!fields.attributes.isEmpty()) throw new IllegalArgumentException("XML scalar contains attributes or children"); return text(); }
  /** Preserve one logical mixed text run across provider chunk/entity/CDATA/comment boundaries. */
  String mixedText(SrlXmlMixedBudget mixed) {
    var value = new StringBuilder(); boolean charged = false;
    while (kind == 't') {
      String chunk = reader.getText();
      if (!chunk.isEmpty()) { if (!charged) { mixed.fragment(); charged = true; } mixed.text(chunk); value.append(chunk); }
      advance();
    }
    return value.toString();
  }
  /** Require the complete document before publishing a candidate model. */
  void finish() { skipSpace(); if (kind != 0 || !identities.isEmpty()) throw new IllegalArgumentException("Unexpected XML trailing content or incomplete identity scope"); }
  /** Release the provider even when lexical, resource or shape checks fail. */
  @Override public void close() { try { reader.close(); } catch (javax.xml.stream.XMLStreamException failure) { throw new IllegalArgumentException("XML provider close failed", failure); } }
}
/** Keep only the current record's attributes until exact membership is checked. */
private static final class SrlXmlInputFields {
  final SrlXmlInput input;
  final java.util.Map<String, String> attributes = new java.util.LinkedHashMap<>();
  boolean closed;
  /** Snapshot only current start-tag attributes before the provider advances. */
  SrlXmlInputFields(SrlXmlInput input) { this.input = input; for (int index = 0; index < input.reader.getAttributeCount(); ++index) { String name = srlXmlName(input.reader.getAttributeNamespace(index), input.reader.getAttributeLocalName(index)); if ((input.acceptedType && name.equals("{http://www.w3.org/2001/XMLSchema-instance}type")) || (input.acceptedNil && name.equals("{http://www.w3.org/2001/XMLSchema-instance}nil"))) continue; if (attributes.put(name, input.reader.getAttributeValue(index)) != null) throw new IllegalArgumentException("Duplicate XML attribute"); } }
  /** Consume a declared required or optional expanded-name attribute exactly once. */
  String attribute(String name, boolean optional) { String text = attributes.remove(name); if (text == null && !optional) throw new IllegalArgumentException("Required XML attribute missing"); return text; }
  /** Decode simple content after checking that all attributes were consumed. */
  String text() { if (!attributes.isEmpty()) throw new IllegalArgumentException("Unexpected XML attribute"); String text = input.text(); closed = true; return text; }
  /** Reject unknown attributes, duplicate children and out-of-order content before publication. */
  void finish() { if (!attributes.isEmpty()) throw new IllegalArgumentException("Unexpected XML attribute"); if (!closed) { input.skipSpace(); if (input.kind != 'e') throw new IllegalArgumentException("Unexpected XML field"); input.advance(); } }
}
)xmljava"
R"xmljava(
/** Write tokens directly while retaining only names of active ancestors. */
private static final class SrlXmlOutput {
  final SrlXmlBudget budget;
  final XmlMapLimits mapLimits;
  final XmlMixedLimits mixedLimits;
  final XmlIdentityLimits identityLimits;
  final XmlDigestLimits digestLimits;
  final XmlExtensionOptions extensionOptions;
  final java.util.ArrayList<SrlXmlIdentity> identities = new java.util.ArrayList<>();
  final java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
  final java.util.ArrayList<Frame> stack = new java.util.ArrayList<>();
  /** Bound native list items with the same explicit policy used by ordinary collections. */
  int listLimit() { return budget.limits.maxElements; }
  /** Admit joined UTF8 list storage before the builder retains another token or separator. */
  int listItem(String text, StringBuilder joined, int used) {
    int remaining = budget.limits.maxStringBytes - budget.textBytes;
    int separator = joined.length() == 0 ? 0 : 1;
    if (separator > remaining - used) throw SrlXmlRule.quota("XML list text limit exceeded");
    int count = separator;
    for (int offset = 0; offset < text.length();) {
      int value = text.codePointAt(offset); offset += Character.charCount(value);
      if (!(value == 9 || value == 10 || value == 13 || value >= 32 && value <= 0xd7ff || value >= 0xe000 && value <= 0xfffd || value >= 0x10000 && value <= 0x10ffff)) throw new IllegalArgumentException("Invalid XML character");
      int width = value < 0x80 ? 1 : value < 0x800 ? 2 : value < 0x10000 ? 3 : 4;
      if (width > remaining - used - count) throw SrlXmlRule.quota("XML list text limit exceeded"); count += width;
    }
    if (separator != 0) joined.append(' '); joined.append(text); return used + count;
  }
  /** One pending start tag carries only its qualified name and attribute identities. */
  static final class Frame {
    final String name, namespace, expanded;
    final java.util.HashSet<String> attributes = new java.util.HashSet<>();
    java.util.Map<String,String> identityAttributes, identityNamespaces;
    boolean scoped;
    boolean pending = true;
    /** Bind the already validated generated element name. */
    Frame(String name, String namespace, String expanded) { this.name = name; this.namespace = namespace; this.expanded=expanded; }
  }
  /** Isolate all budgets and output state within this complete document. */
  SrlXmlOutput(Limits limits) { this(limits, new XmlMapLimits()); }
  /** Carry one immutable map policy through every nested typed encoder. */
  SrlXmlOutput(Limits limits, XmlMapLimits maps) { this(limits, maps, new XmlMixedLimits()); }
  /** Carry explicit mixed/map policies through all descendants without mutable global state. */
  SrlXmlOutput(Limits limits, XmlMapLimits maps, XmlMixedLimits mixed) { this(limits,maps,mixed,new XmlIdentityLimits()); }
  /** Share explicit identity limits while all owner indexes remain independent. */
  SrlXmlOutput(Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity) { this(limits,maps,mixed,identity,new XmlDigestLimits()); }
  /** Keep digest byte admission independent from ordinary document and collection limits. */
  SrlXmlOutput(Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests) {this(limits,maps,mixed,identity,digests,new XmlExtensionOptions());}
  /** Share explicit extension policies without modifying earlier constructor contracts. */
  SrlXmlOutput(Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests, XmlExtensionOptions extensions) { budget = new SrlXmlBudget(limits); mapLimits = java.util.Objects.requireNonNull(maps); mixedLimits = java.util.Objects.requireNonNull(mixed); identityLimits=java.util.Objects.requireNonNull(identity); digestLimits=java.util.Objects.requireNonNull(digests);extensionOptions=java.util.Objects.requireNonNull(extensions); }
  /** Attach metadata after opening its still-pending owner start tag. */
  SrlXmlIdentity identityScope(SrlXmlIdentityScope scope) { var result=new SrlXmlIdentity(scope,identityLimits,budget.limits.xmlOperation,budget.field);identities.add(result);stack.get(stack.size()-1).identityAttributes=new java.util.LinkedHashMap<>();return result; }
  /** Resolve only namespace declarations the direct writer actually emits. */
  String identityNamespace(String prefix) {if(prefix.equals("xml"))return "http://www.w3.org/XML/1998/namespace";for(int index=stack.size()-1;index>=0;--index){var frame=stack.get(index);if(frame.identityNamespaces!=null&&frame.identityNamespaces.containsKey(prefix))return frame.identityNamespaces.get(prefix);int colon=frame.name.indexOf(':');if(!frame.scoped&&colon>=0&&frame.name.substring(0,colon).equals(prefix))return frame.namespace;}return prefix.isEmpty()?"":null;}
  /** Observe one pending start tag after all attributes have been validated. */
  void identityStart(Frame frame) { for(var identity:identities)if(!identity.complete)identity.start(frame.expanded,frame.identityAttributes==null?java.util.Map.of():frame.identityAttributes,this::identityNamespace); }
  /** Require complete local references before releasing an owner index. */
  void identityFinish(SrlXmlIdentity identity) { identity.finish();if(!identities.remove(identity))throw new IllegalArgumentException("Unknown XML identity scope"); }
  /** Split a compiler-owned expanded name. */
  String[] parts(String value) { int close = value.startsWith("{") ? value.indexOf('}') : -1; return close < 0 ? new String[]{"", value} : new String[]{value.substring(1, close), value.substring(close + 1)}; }
  /** Check encoded length before appending the next fragment. */
  void append(String text) { budget.charge("work",text.length());budget.charge("allocation",2L*text.length());byte[] chunk = utf8(text); if (chunk.length > budget.limits.maxBytes - bytes.size()) throw SrlXmlRule.quota("XML output byte limit exceeded"); bytes.writeBytes(chunk); }
  /** Escape syntax and normalized whitespace while batching ordinary text runs. */
  void escaped(String text, boolean attribute) { int begin = 0; for (int i = 0; i < text.length(); ++i) { String replacement = switch (text.charAt(i)) { case '&' -> "&amp;"; case '<' -> "&lt;"; case '>' -> "&gt;"; case '\r' -> "&#13;"; case '"' -> attribute ? "&quot;" : null; case '\n' -> attribute ? "&#10;" : null; case '\t' -> attribute ? "&#9;" : null; default -> null; }; if (replacement != null) { append(text.substring(begin, i)); append(replacement); begin = i + 1; } } append(text.substring(begin)); }
  /** Finish the pending start tag before content is emitted. */
  void seal() { if (!stack.isEmpty()) { var frame = stack.get(stack.size() - 1); if (frame.pending) { identityStart(frame); append(">"); frame.pending = false; } } }
  /** Admit and emit an element before traversing any of its descendants. */
  void open(String name) { if(!stack.isEmpty()&&stack.get(stack.size()-1).scoped){open(name,null);return;} if (stack.size() >= budget.limits.maxDepth || budget.nodes >= budget.limits.maxElements) throw SrlXmlRule.quota("XML node or depth limit exceeded"); budget.select(name);budget.charge("allocation",128);budget.text(name); ++budget.nodes; seal(); var parts = parts(name); String prefix = parts[0].equals("http://www.w3.org/XML/1998/namespace") ? "xml" : "srl"; String qualified = parts[0].isEmpty() ? parts[1] : prefix + ":" + parts[1]; append("<" + qualified); if (!parts[0].isEmpty() && !prefix.equals("xml") && (stack.isEmpty() || !stack.get(stack.size() - 1).namespace.equals(parts[0]))) { namespaceBudget(prefix,parts[0]); append(" xmlns:srl=\""); escaped(parts[0], true); append("\""); } stack.add(new Frame(qualified, parts[0], name)); }
  /** Admit each emitted namespace declaration before growing its output. */
  void namespaceBudget(String prefix,String uri) { if(budget.attributes>=budget.limits.maxElements)throw SrlXmlRule.quota("XML namespace limit exceeded");++budget.attributes;budget.text(prefix);budget.text(uri); }
  /** Choose a deterministic prefix without shadowing an opaque lexical QName binding. */
  String freshPrefix(String base,String uri){for(int index=0;;++index){String prefix=base+(index==0?"":Integer.toString(index));String bound=identityNamespace(prefix);if(bound==null||bound.equals(uri))return prefix;}}
  /** Reuse an existing binding to keep retained namespace storage stable across roundtrips. */
  String matchingPrefix(String uri,boolean attribute){if(!attribute&&uri.equals(identityNamespace("")))return "";for(int index=stack.size()-1;index>=0;--index){var frame=stack.get(index);if(frame.identityNamespaces!=null)for(var entry:frame.identityNamespaces.entrySet())if((!attribute||!entry.getKey().isEmpty())&&entry.getValue().equals(uri)&&uri.equals(identityNamespace(entry.getKey())))return entry.getKey();if(!frame.scoped&&!frame.namespace.isEmpty()&&frame.namespace.equals(uri)){int colon=frame.name.indexOf(':');if(colon>=0){String prefix=frame.name.substring(0,colon);if(uri.equals(identityNamespace(prefix)))return prefix;}}}return null;}
  /** Charge and emit only namespace bindings which differ from their inherited scope. */
  void declare(String prefix,String uri){String current=identityNamespace(prefix);if(uri.equals(current))return;namespaceBudget(prefix,uri);var frame=stack.get(stack.size()-1);if(frame.identityNamespaces==null)frame.identityNamespaces=new java.util.LinkedHashMap<>();frame.identityNamespaces.put(prefix,uri);append(prefix.isEmpty()?" xmlns=\"":" xmlns:"+prefix+"=\"");escaped(uri,true);append("\"");}
  /** Pin every preserved binding before selecting the known or foreign element's prefix. */
  void open(String name,java.util.Map<String,String> scope){
    if(stack.size()>=budget.limits.maxDepth||budget.nodes>=budget.limits.maxElements)throw SrlXmlRule.quota("XML node or depth limit exceeded");budget.select(name);budget.charge("allocation",128);budget.text(name);++budget.nodes;seal();var selected=parts(name);String prefix="";
    java.util.Map<String,String> old=null;var temporary=new Frame("",selected[0],name);temporary.scoped=true;stack.add(temporary);
    if(scope!=null){old=temporary.identityNamespaces;temporary.identityNamespaces=scope;}
    if(!selected[0].isEmpty()){prefix=matchingPrefix(selected[0],false);if(prefix==null)prefix=selected[0].equals("http://www.w3.org/XML/1998/namespace")?"xml":freshPrefix("srl",selected[0]);}
    temporary.identityNamespaces=old;stack.remove(stack.size()-1);String qualified=prefix.isEmpty()?selected[1]:prefix+":"+selected[1];append("<"+qualified);var frame=new Frame(qualified,selected[0],name);frame.scoped=true;stack.add(frame);
    if(scope!=null)for(var entry:scope.entrySet())declare(entry.getKey(),entry.getValue());
    if(selected[0].isEmpty()){if(!identityNamespace("").isEmpty())declare("","");}else declare(prefix,selected[0]);
  }
  /** Admit attributes in a pinned scope while preserving their expanded duplicate identity. */
  void scopedAttribute(String name,String text){var frame=stack.get(stack.size()-1);if(!frame.pending||!frame.attributes.add(name))throw new IllegalArgumentException("Invalid XML attribute position or duplicate");if(budget.attributes>=budget.limits.maxElements)throw SrlXmlRule.quota("XML attribute limit exceeded");++budget.attributes;budget.text(name);budget.text(text);var selected=parts(name);String prefix="";if(!selected[0].isEmpty()){prefix=matchingPrefix(selected[0],true);if(prefix==null)prefix=selected[0].equals("http://www.w3.org/XML/1998/namespace")?"xml":freshPrefix("a",selected[0]);declare(prefix,selected[0]);}append(" "+(prefix.isEmpty()?selected[1]:prefix+":"+selected[1])+"=\"");escaped(text,true);append("\"");if(!identities.isEmpty()){if(frame.identityAttributes==null)frame.identityAttributes=new java.util.LinkedHashMap<>();frame.identityAttributes.put(name,text);}}
)xmljava"
R"xmljava(
  /** Emit each attribute exactly once before text or child elements. */
  void attribute(String name, String text) { if(stack.get(stack.size()-1).scoped){scopedAttribute(name,text);return;} var frame = stack.get(stack.size() - 1); if (!frame.pending || !frame.attributes.add(name)) throw new IllegalArgumentException("Invalid XML attribute position or duplicate"); if (budget.attributes >= budget.limits.maxElements) throw SrlXmlRule.quota("XML attribute limit exceeded"); ++budget.attributes; budget.text(name); budget.text(text); var parts = parts(name); String prefix = parts[0].equals("http://www.w3.org/XML/1998/namespace") ? "xml" : "a" + frame.attributes.size(); if (!parts[0].isEmpty() && !prefix.equals("xml")) { namespaceBudget(prefix,parts[0]); append(" xmlns:" + prefix + "=\""); escaped(parts[0], true); append("\""); } append(" " + (parts[0].isEmpty() ? parts[1] : prefix + ":" + parts[1]) + "=\""); escaped(text, true); append("\""); if(!identities.isEmpty()){if(frame.identityAttributes==null)frame.identityAttributes=new java.util.LinkedHashMap<>();frame.identityAttributes.put(name,text);if(!parts[0].isEmpty()&&!prefix.equals("xml")){if(frame.identityNamespaces==null)frame.identityNamespaces=new java.util.LinkedHashMap<>();frame.identityNamespaces.put(prefix,parts[0]);}} }
  /** Declare a QName prefix on this pending payload before emitting its xsi:type attribute. */
  void typeName(String identity) {
    var frame = stack.get(stack.size() - 1);
    if (!frame.pending) throw new IllegalArgumentException("XML variant type follows content");
    var selected = parts(identity); String lexical = selected[1];
    if(frame.scoped){if(!selected[0].isEmpty()){String prefix=selected[0].equals("http://www.w3.org/XML/1998/namespace")?"xml":freshPrefix("t",selected[0]);declare(prefix,selected[0]);lexical=prefix+":"+lexical;}else if(!identityNamespace("").isEmpty())throw new IllegalArgumentException("Unqualified XML type conflicts with preserved default namespace");attribute("{http://www.w3.org/2001/XMLSchema-instance}type",lexical);return;}
    if (selected[0].equals("http://www.w3.org/XML/1998/namespace")) lexical = "xml:" + lexical;
    else if (!selected[0].isEmpty()) {
      if (budget.attributes >= budget.limits.maxElements) throw SrlXmlRule.quota("XML namespace limit exceeded");
      ++budget.attributes; budget.text("t" + selected[0]); if(!identities.isEmpty()){if(frame.identityNamespaces==null)frame.identityNamespaces=new java.util.LinkedHashMap<>();frame.identityNamespaces.put("t",selected[0]);} append(" xmlns:t=\""); escaped(selected[0], true); append("\""); lexical = "t:" + lexical;
    }
    attribute("{http://www.w3.org/2001/XMLSchema-instance}type", lexical);
  }
  /** Validate and emit one scalar directly without a retained node. */
  void text(String text) { budget.text(text); seal(); for(var identity:identities)if(!identity.complete)identity.text(text); escaped(text, false); }
  /** Release the completed ancestor name after its matching end token. */
  void close() { var frame = stack.get(stack.size() - 1);if(frame.pending)identityStart(frame);for(var identity:identities)if(!identity.complete)identity.end(this::identityNamespace);stack.remove(stack.size()-1);append(frame.pending ? "/>" : "</" + frame.name + ">"); }
  /** Publish only a completed output document. */
  byte[] finish() { if (!stack.isEmpty() || !identities.isEmpty()) throw new IllegalArgumentException("Incomplete XML output or identity scope"); return bytes.toByteArray(); }
}
/** Optional prepared trusted standalone runtime XSD1.0 validator using the Java platform. */
public static final class XmlSchema {
  private final javax.xml.validation.Schema schema;
)xmljava"
R"xmljava(  /** Compile once with external schemas and DTDs prohibited. */
  public XmlSchema(byte[] xsd) {
    var tree = srlXmlParse(xsd, Limits.defaults());
    var pending = new java.util.ArrayList<SrlXmlNode>(); pending.add(tree);
    for (int i = 0; i < pending.size(); ++i) { var node = pending.get(i); if (java.util.Set.of("include", "import", "redefine", "override").stream().anyMatch(kind -> node.name.equals(srlXmlName("http://www.w3.org/2001/XMLSchema", kind)))) throw new IllegalArgumentException("Runtime XSD external references are unsupported"); pending.addAll(node.children); }
    try { var factory = javax.xml.validation.SchemaFactory.newInstance(javax.xml.XMLConstants.W3C_XML_SCHEMA_NS_URI); factory.setFeature(javax.xml.XMLConstants.FEATURE_SECURE_PROCESSING, true); factory.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, ""); factory.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, ""); schema = factory.newSchema(new javax.xml.transform.stream.StreamSource(new java.io.ByteArrayInputStream(xsd))); }
    catch (org.xml.sax.SAXException failure) { throw new IllegalArgumentException("Invalid or unsupported XSD", failure); }
  }
  /** Reuse the immutable schema with a separate bounded per-call validator. */
  public void validate(byte[] bytes, Limits limits) {
    try (var input = new SrlXmlInput(bytes, limits)) { while (input.kind != 0) input.advance(); input.finish(); }
    try { var validator = schema.newValidator(); validator.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, ""); validator.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, ""); validator.validate(new javax.xml.transform.stream.StreamSource(new java.io.ByteArrayInputStream(bytes))); }
    catch (org.xml.sax.SAXException | java.io.IOException failure) { throw new IllegalArgumentException("XML does not satisfy the selected XSD", failure); }
  }
}
)xmljava"
