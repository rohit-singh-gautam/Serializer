// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlcs(
  // Own one bounded data XML node with expanded names and ordered children.
  internal sealed class SrlXmlNode {
    internal readonly string Name;
    internal readonly System.Collections.Generic.Dictionary<string, string> Attributes = new();
    internal readonly System.Collections.Generic.List<SrlXmlNode> Children = new();
    internal readonly System.Text.StringBuilder Text = new();
    // Retain only the name selected by the fixed document contract.
    internal SrlXmlNode(string name) { Name = name; }
  }
  // Charge the complete generated/parsed document before retaining its logical tree storage.
  internal sealed class SrlXmlBudget {
    internal readonly Limits Limits;
    internal int Nodes, Attributes, TextBytes;
    // Retain a validated per-operation policy without global mutable counters.
    internal SrlXmlBudget(Limits limits) { Limits = limits ?? throw new System.ArgumentNullException(nameof(limits)); }
    // Validate XML1.0 characters and bound cumulative UTF8 text and expanded names.
    internal string Text(string value) { System.Xml.XmlConvert.VerifyXmlChars(value); int count = SrlUtf8.GetByteCount(value); if (count > Limits.MaxStringBytes - TextBytes) throw new System.FormatException("XML text limit exceeded"); TextBytes += count; return value; }
    // Bound depth and nodes before creating a child record.
    internal SrlXmlNode Node(string name, int depth) { if (depth > Limits.MaxDepth || Nodes >= Limits.MaxElements) throw new System.FormatException("XML node or depth limit exceeded"); ++Nodes; return new SrlXmlNode(Text(name)); }
    // Bound attribute metadata before inserting it into the current element.
    internal void Attribute(SrlXmlNode node, string name, string value) { if (Attributes >= Limits.MaxElements) throw new System.FormatException("XML attribute limit exceeded"); ++Attributes; if (!node.Attributes.TryAdd(Text(name), Text(value))) throw new System.FormatException("Duplicate XML attribute"); }
  }
  // Build a namespace identity independently of a provider's prefix spelling.
  private static string SrlXmlName(string uri, string local) { return uri.Length == 0 ? local : "{" + uri + "}" + local; }
  // Trim exactly XML whitespace around numeric values and record indentation.
  private static string SrlXmlTrim(string value) { return value.Trim(' ', '\t', '\r', '\n'); }
  // Read a complete bounded UTF8 XML1.0 document; DTDs, entities and network resolution are disabled.
  private static SrlXmlNode SrlXmlParse(byte[] bytes, Limits limits) {
    if (bytes.Length > limits.MaxBytes) throw new System.FormatException("XML byte limit exceeded");
    SrlUtf8.GetString(bytes);
    var settings = new System.Xml.XmlReaderSettings { DtdProcessing = System.Xml.DtdProcessing.Prohibit, XmlResolver = null, MaxCharactersInDocument = limits.MaxBytes, IgnoreComments = true, IgnoreProcessingInstructions = false };
    using var stream = new System.IO.MemoryStream(bytes, false);
    using var reader = System.Xml.XmlReader.Create(stream, settings);
    var budget = new SrlXmlBudget(limits);
    var stack = new System.Collections.Generic.List<SrlXmlNode>(); SrlXmlNode? root = null;
    while (reader.Read()) {
      if (reader.NodeType == System.Xml.XmlNodeType.XmlDeclaration) { if (reader.GetAttribute("version") != "1.0") throw new System.FormatException("XML1.0 required"); string? encoding = reader.GetAttribute("encoding"); if (encoding != null && !encoding.Replace("-", "").Equals("UTF8", System.StringComparison.OrdinalIgnoreCase)) throw new System.FormatException("XML UTF8 required"); }
      else if (reader.NodeType == System.Xml.XmlNodeType.Element) {
        var node = budget.Node(SrlXmlName(reader.NamespaceURI, reader.LocalName), stack.Count + 1);
        if (reader.MoveToFirstAttribute()) {
          do {
            if (reader.NamespaceURI == "http://www.w3.org/2000/xmlns/") { if (budget.Attributes >= limits.MaxElements) throw new System.FormatException("XML namespace limit exceeded"); ++budget.Attributes; budget.Text(reader.LocalName); budget.Text(reader.Value); }
            else budget.Attribute(node, SrlXmlName(reader.NamespaceURI, reader.LocalName), reader.Value);
          } while (reader.MoveToNextAttribute()); reader.MoveToElement();
        }
        if (stack.Count == 0) { if (root != null) throw new System.FormatException("Multiple XML roots"); root = node; } else stack[^1].Children.Add(node);
        if (!reader.IsEmptyElement) stack.Add(node);
      } else if (reader.NodeType == System.Xml.XmlNodeType.EndElement) { var node = stack[^1]; stack.RemoveAt(stack.Count - 1); if (node.Children.Count != 0 && SrlXmlTrim(node.Text.ToString()).Length != 0) throw new System.FormatException("XML mixed content is unsupported"); }
      else if (reader.NodeType == System.Xml.XmlNodeType.Text || reader.NodeType == System.Xml.XmlNodeType.CDATA || reader.NodeType == System.Xml.XmlNodeType.Whitespace || reader.NodeType == System.Xml.XmlNodeType.SignificantWhitespace) { string text = budget.Text(reader.Value); if (stack.Count != 0) stack[^1].Text.Append(text); else if (SrlXmlTrim(text).Length != 0) throw new System.FormatException("XML text outside root"); }
      else if (reader.NodeType == System.Xml.XmlNodeType.EntityReference || reader.NodeType == System.Xml.XmlNodeType.DocumentType) throw new System.FormatException("XML entities and DTDs are prohibited");
    }
    if (root == null || stack.Count != 0) throw new System.FormatException("Incomplete XML document"); return root;
  }
  // Scalar mappings cannot silently discard nested elements or attributes.
  private static string SrlXmlScalarText(SrlXmlNode node) { if (node.Attributes.Count != 0 || node.Children.Count != 0) throw new System.FormatException("XML scalar has nested content"); return node.Text.ToString(); }
  // Strict integer lexical and range checks preserve every signed and unsigned model width.
  private static System.Numerics.BigInteger SrlXmlInteger(string text, int bits, bool unsigned) { text = SrlXmlTrim(text); if (!System.Text.RegularExpressions.Regex.IsMatch(text, @"\A[+-]?[0-9]+\z")) throw new System.FormatException("Invalid XML integer"); var value = System.Numerics.BigInteger.Parse(text, System.Globalization.CultureInfo.InvariantCulture); var minimum = unsigned ? System.Numerics.BigInteger.Zero : -(System.Numerics.BigInteger.One << (bits - 1)); var maximum = (System.Numerics.BigInteger.One << (unsigned ? bits : bits - 1)) - 1; if (value < minimum || value > maximum) throw new System.FormatException("XML integer out of range"); return value; }
  // Recognize only the XML Boolean lexical alternatives.
  private static bool SrlXmlBoolean(string text) { return SrlXmlTrim(text) switch { "true" or "1" => true, "false" or "0" => false, _ => throw new System.FormatException("Invalid XML Boolean") }; }
  // Preserve XML/XSD floating special values without accepting provider-specific spellings.
  private static double SrlXmlFloat(string text, bool single) { text = SrlXmlTrim(text); if (text == "NaN") return double.NaN; if (text == "INF") return double.PositiveInfinity; if (text == "-INF") return double.NegativeInfinity; if (!System.Text.RegularExpressions.Regex.IsMatch(text, @"\A[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\z")) throw new System.FormatException("Invalid XML floating value"); double value = single ? float.Parse(text, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture) : double.Parse(text, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture); if (!double.IsFinite(value)) throw new System.FormatException("XML floating value overflows model"); return value; }
  // Emit invariant roundtrip lexical values, including XML's named special values.
  private static string SrlXmlFloatText(double value, bool single) { return double.IsNaN(value) ? "NaN" : double.IsPositiveInfinity(value) ? "INF" : double.IsNegativeInfinity(value) ? "-INF" : single ? ((float)value).ToString("R", System.Globalization.CultureInfo.InvariantCulture) : value.ToString("R", System.Globalization.CultureInfo.InvariantCulture); }
  // Consume exact attribute membership and ordered child occurrences.
  private sealed class SrlXmlFields {
    private readonly SrlXmlNode node; private readonly System.Collections.Generic.HashSet<string> used = new(); private int next;
    // Allow indentation but reject scalar text where record fields are required.
    internal SrlXmlFields(SrlXmlNode node, bool hasText = false) { this.node = node; if (!hasText && SrlXmlTrim(node.Text.ToString()).Length != 0) throw new System.FormatException("Unexpected XML record text"); }
    // Read simple content without accepting element children.
    internal string Text() { if (node.Children.Count != 0) throw new System.FormatException("XML simple content contains children"); return node.Text.ToString(); }
    // Preserve required/optional attribute presence without defaulting missing values.
    internal string? Attribute(string name, bool optional) { if (!node.Attributes.TryGetValue(name, out var value)) { if (optional) return null; throw new System.FormatException("Required XML attribute missing"); } if (!used.Add(name)) throw new System.FormatException("Duplicate XML attribute mapping"); return value; }
    // Consume only adjacent children with the statically selected expanded name.
    internal System.Collections.Generic.List<SrlXmlNode> Children(string name, int minimum, int maximum) { int begin = next; while (next < node.Children.Count && node.Children[next].Name == name) ++next; int count = next - begin; if (count < minimum || maximum >= 0 && count > maximum) throw new System.FormatException("XML occurrence mismatch"); return node.Children.GetRange(begin, count); }
    // Reject unknown or reordered fields before publishing the fresh model.
    internal void Finish() { if (used.Count != node.Attributes.Count || next != node.Children.Count) throw new System.FormatException("Unexpected XML field"); }
    // Require a collection wrapper and validate its complete member sequence.
    internal System.Collections.Generic.List<SrlXmlNode> Wrapped(string wrapper, string name, int minimum, int maximum) { var fields = new SrlXmlFields(Children(wrapper, 1, 1)[0]); var result = fields.Children(name, minimum, maximum); fields.Finish(); return result; }
  }
  // Encode deterministic escaped XML through a bounded whole-document byte sink.
  private static byte[] SrlXmlWrite(SrlXmlNode root, Limits limits) {
    var prefixes = new System.Collections.Generic.Dictionary<string, string>(); using var output = new System.IO.MemoryStream();
    // Resolve only compiler-owned expanded-name syntax.
    (string Uri, string Local) Name(string value) { int close = value.StartsWith("{", System.StringComparison.Ordinal) ? value.IndexOf('}') : -1; return close < 0 ? ("", value) : (value[1..close], value[(close + 1)..]); }
    // Collect namespace identities before emitting the root declaration list.
    void Inspect(SrlXmlNode node) { foreach (var selected in node.Attributes.Keys) Register(selected); Register(node.Name); foreach (var child in node.Children) Inspect(child); }
    // Assign a deterministic prefix, preserving the predefined xml namespace.
    void Register(string selected) { var (uri, local) = Name(selected); if (uri.Length != 0 && !prefixes.ContainsKey(uri)) prefixes.Add(uri, uri == "http://www.w3.org/XML/1998/namespace" ? "xml" : "n" + prefixes.Count); }
    // Bound bytes before appending the next escaped output fragment.
    void Append(string value) { int count = SrlUtf8.GetByteCount(value); if (count > limits.MaxBytes - output.Length) throw new System.FormatException("XML output byte limit exceeded"); output.Write(SrlUtf8.GetBytes(value)); }
    // Produce a presentation name with the same expanded identity.
    string Qualified(string value) { var (uri, local) = Name(value); return uri.Length == 0 ? local : prefixes[uri] + ":" + local; }
    // Preserve application whitespace and batch ordinary UTF16 runs.
    void Escaped(string value, bool attribute) { int begin = 0; for (int index = 0; index < value.Length; ++index) { string? replacement = value[index] switch { '&' => "&amp;", '<' => "&lt;", '>' => "&gt;", '\r' => "&#13;", '"' => attribute ? "&quot;" : null, '\n' => attribute ? "&#10;" : null, '\t' => attribute ? "&#9;" : null, _ => null }; if (replacement != null) { Append(value[begin..index]); Append(replacement); begin = index + 1; } } Append(value[begin..]); }
    // Emit one already bounded data record in its fixed field order.
    void Emit(SrlXmlNode node, bool first) { Append("<" + Qualified(node.Name)); if (first) foreach (var pair in prefixes) if (pair.Value != "xml") { Append(" xmlns:" + pair.Value + "=\""); Escaped(pair.Key, true); Append("\""); } foreach (var pair in node.Attributes) { Append(" " + Qualified(pair.Key) + "=\""); Escaped(pair.Value, true); Append("\""); } if (node.Children.Count == 0 && node.Text.Length == 0) { Append("/>"); return; } Append(">"); Escaped(node.Text.ToString(), false); foreach (var child in node.Children) Emit(child, false); Append("</" + Qualified(node.Name) + ">"); }
    Inspect(root); Emit(root, true); return output.ToArray();
  }
  // Optional immutable prepared XSD1.0 bundle; validation contexts are created per call.
  public sealed class XmlSchema {
    private readonly System.Xml.Schema.XmlSchemaSet schemas;
    // Compile a trusted standalone schema with all external resolution prohibited.
    public XmlSchema(byte[] xsd) { var tree = SrlXmlParse(xsd, new Limits()); var pending = new System.Collections.Generic.List<SrlXmlNode> { tree }; for (int index = 0; index < pending.Count; ++index) { var node = pending[index]; foreach (var kind in new[] { "include", "import", "redefine", "override" }) if (node.Name == SrlXmlName("http://www.w3.org/2001/XMLSchema", kind)) throw new System.FormatException("Runtime XSD external references are unsupported"); pending.AddRange(node.Children); } schemas = new System.Xml.Schema.XmlSchemaSet { XmlResolver = null }; using var stream = new System.IO.MemoryStream(xsd, false); using var reader = System.Xml.XmlReader.Create(stream, new System.Xml.XmlReaderSettings { DtdProcessing = System.Xml.DtdProcessing.Prohibit, XmlResolver = null }); schemas.Add(null, reader); schemas.Compile(); }
    // Validate current document values after complete bounded and secure parsing.
    public void Validate(byte[] bytes, Limits limits) { SrlXmlParse(bytes, limits); var settings = new System.Xml.XmlReaderSettings { DtdProcessing = System.Xml.DtdProcessing.Prohibit, XmlResolver = null, Schemas = schemas, ValidationType = System.Xml.ValidationType.Schema, MaxCharactersInDocument = limits.MaxBytes }; settings.ValidationFlags = System.Xml.Schema.XmlSchemaValidationFlags.ProcessIdentityConstraints; settings.ValidationEventHandler += (_, args) => throw new System.FormatException("XML does not satisfy the selected XSD", args.Exception); using var stream = new System.IO.MemoryStream(bytes, false); using var reader = System.Xml.XmlReader.Create(stream, settings); while (reader.Read()) {} }
  }
)xmlcs"
