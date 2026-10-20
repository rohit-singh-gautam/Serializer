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
    // Keep ordinary XML failures exact while an attached package classifies local quota exhaustion.
    internal static System.FormatException Limit(string message,Limits.IOperation? operation=null) { operation?.ResourceLimit(message);return new System.FormatException(message); }
    internal readonly Limits Limits;
    internal Limits.IOperation? Operation => Limits.Operation;
    internal int Nodes, Attributes, TextBytes;
    // Retain a validated per-operation policy without global mutable counters.
    internal SrlXmlBudget(Limits limits) { Limits = limits ?? throw new System.ArgumentNullException(nameof(limits)); }
    // Validate XML1.0 characters and bound cumulative UTF8 text and expanded names.
    internal string Text(string value) { System.Xml.XmlConvert.VerifyXmlChars(value); int count = SrlUtf8.GetByteCount(value); Operation?.Consume(Limits.OperationResource.WorkUnits,count);Operation?.Consume(Limits.OperationResource.AllocationBytes,(long)count*2);if (count > Limits.MaxStringBytes - TextBytes) throw SrlXmlBudget.Limit("XML text limit exceeded",Operation); TextBytes += count; return value; }
    // Bound depth and nodes before creating a child record.
    internal SrlXmlNode Node(string name, int depth) { if (depth > Limits.MaxDepth || Nodes >= Limits.MaxElements) throw SrlXmlBudget.Limit("XML node or depth limit exceeded",Operation);Operation?.Consume(Limits.OperationResource.WorkUnits,1);Operation?.Consume(Limits.OperationResource.AllocationBytes,64); ++Nodes; return new SrlXmlNode(Text(name)); }
    // Bound attribute metadata before inserting it into the current element.
    internal void Attribute(SrlXmlNode node, string name, string value) { if (Attributes >= Limits.MaxElements) throw SrlXmlBudget.Limit("XML attribute limit exceeded",Operation); ++Attributes; if (!node.Attributes.TryAdd(Text(name), Text(value))) throw new System.FormatException("Duplicate XML attribute"); }
  }
  // Build a namespace identity independently of a provider's prefix spelling.
  private static string SrlXmlName(string uri, string local) { return uri.Length == 0 ? local : "{" + uri + "}" + local; }
  // Trim exactly XML whitespace around numeric values and record indentation.
  private static string SrlXmlTrim(string value) { return value.Trim(' ', '\t', '\r', '\n'); }
  // Read a complete bounded UTF8 XML1.0 document; DTDs, entities and network resolution are disabled.
  private static SrlXmlNode SrlXmlParse(byte[] bytes, Limits limits) {
    if (bytes.Length > limits.MaxBytes) throw SrlXmlBudget.Limit("XML byte limit exceeded",limits.Operation);
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
            if (reader.NamespaceURI == "http://www.w3.org/2000/xmlns/") { if (budget.Attributes >= limits.MaxElements) throw SrlXmlBudget.Limit("XML namespace limit exceeded",budget.Operation); ++budget.Attributes; budget.Text(reader.LocalName); budget.Text(reader.Value); }
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
  // Pull bounded provider tokens without retaining completed XML subtrees.
  internal sealed class SrlXmlInput : System.IDisposable {
    private readonly SrlXmlBudget budget;
    internal readonly XmlMapLimits MapLimits;
    internal readonly XmlMixedLimits MixedLimits;
    internal readonly XmlIdentityLimits IdentityLimits;
    internal readonly XmlDigestLimits DigestLimits;
    internal readonly XmlExtensionOptions ExtensionOptions;
    private readonly System.Collections.Generic.List<SrlXmlIdentity> identities = new();
    private readonly System.Xml.XmlReader reader;
    private bool emptyEnd;
    private bool acceptedType;
    private bool acceptedNil;
    internal char Kind;
    // Expose native collection admission without exposing mutable counters.
    internal int ListLimit => budget.Limits.MaxElements;
    // Borrow the current native owner only when a package supplies shared accounting.
    internal global::System.IDisposable? FieldOwner()=>budget.Operation?.EnterOwner();
    internal Limits.IOperation? Operation=>budget.Operation;
    // Select a native sibling before its scalar or descendant work is admitted.
    internal void Field(string member)=>budget.Operation?.SelectField(member);
    internal int NamespaceBytes;
    // Bound retained QName bindings independently of provider scope restoration.
    internal int NamespaceByteLimit => budget.Limits.MaxStringBytes;
    // Bind an independent no-fetch provider after complete byte and UTF8 checks.
    internal SrlXmlInput(byte[] bytes, Limits limits) : this(bytes, limits, new XmlMapLimits()) { }
    // Carry one immutable map policy through every nested typed decoder.
    internal SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps) : this(bytes, limits, maps, new XmlMixedLimits()) { }
    // Carry explicit mixed/map policies without sharing mutable sequence counters.
    internal SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps, XmlMixedLimits mixed) : this(bytes, limits, maps, mixed, new XmlIdentityLimits()) { }
    // Carry explicit owner-local identity bounds through every nested candidate.
    internal SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity) : this(bytes,limits,maps,mixed,identity,new XmlDigestLimits()) { }
    // Carry immutable per-digest admission through every nested owning candidate.
)xmlcs"
R"xmlcs(    internal SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests) : this(bytes,limits,maps,mixed,identity,digests,new XmlExtensionOptions()) { }
    // Share explicit opaque-vocabulary policies through every descendant.
    internal SrlXmlInput(byte[] bytes, Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests, XmlExtensionOptions extensions) {
      ExtensionOptions=extensions??throw new System.ArgumentNullException(nameof(extensions));
      if (bytes.Length > limits.MaxBytes) throw SrlXmlBudget.Limit("XML byte limit exceeded",limits.Operation);
      limits.Operation?.Consume(Limits.OperationResource.WorkUnits,bytes.Length);
      limits.Operation?.Consume(Limits.OperationResource.AllocationBytes,(long)bytes.Length*2);
      SrlUtf8.GetString(bytes); budget = new SrlXmlBudget(limits); MapLimits = maps ?? throw new System.ArgumentNullException(nameof(maps)); MixedLimits = mixed ?? throw new System.ArgumentNullException(nameof(mixed)); IdentityLimits = identity ?? throw new System.ArgumentNullException(nameof(identity)); DigestLimits=digests ?? throw new System.ArgumentNullException(nameof(digests));
      reader = System.Xml.XmlReader.Create(new System.IO.MemoryStream(bytes, false), new System.Xml.XmlReaderSettings { DtdProcessing = System.Xml.DtdProcessing.Prohibit, XmlResolver = null, MaxCharactersInDocument = limits.MaxBytes, CloseInput = true });
      Advance();
    }
    // Derive current element identity independently of prefixes.
    internal string Name => SrlXmlName(reader.NamespaceURI, reader.LocalName);
    // Expose the active secure provider only to bounded foreign-subtree capture.
    internal System.Xml.XmlReader ExtensionReader=>reader;
    // Snapshot namespace context only for explicit preservation, including unused QName prefixes.
    internal System.Collections.Generic.Dictionary<string,string> NamespaceScope(){if(reader is not System.Xml.IXmlNamespaceResolver resolver)throw new System.FormatException("XML provider cannot expose namespace scope");var bindings=resolver.GetNamespacesInScope(System.Xml.XmlNamespaceScope.All);if(budget.Operation!=null){budget.Operation.Consume(Limits.OperationResource.WorkUnits,bindings.Count);long size=64L+bindings.Count*64L;foreach(var entry in bindings){int count=SrlUtf8.GetByteCount(entry.Key)+SrlUtf8.GetByteCount(entry.Value);budget.Operation.Consume(Limits.OperationResource.WorkUnits,count);size+=count*2L;}budget.Operation.Consume(Limits.OperationResource.AllocationBytes,size);}return new(bindings,System.StringComparer.Ordinal);}
    // Admit retained used bindings before QName sidecars grow beyond the provider lifetime.
    internal void NamespaceRetention(long size){budget.Operation?.Consume(Limits.OperationResource.WorkUnits,size+1);budget.Operation?.Consume(Limits.OperationResource.AllocationBytes,64L+size*2);}
    // Charge names, attributes and text before exposing the next provider event.
    internal void Advance() {
      if (emptyEnd) { emptyEnd = false; Kind = 'e'; IdentityEvent(); return; }
      try {
        while (reader.Read()) {
          if (reader.NodeType == System.Xml.XmlNodeType.XmlDeclaration) { if (reader.GetAttribute("version") != "1.0") throw new System.FormatException("XML1.0 required"); string? encoding = reader.GetAttribute("encoding"); if (encoding != null && !encoding.Replace("-", "").Equals("UTF8", System.StringComparison.OrdinalIgnoreCase)) throw new System.FormatException("XML UTF8 required"); }
)xmlcs"
R"xmlcs(          else if (reader.NodeType == System.Xml.XmlNodeType.Element) {
            if (reader.Depth >= budget.Limits.MaxDepth || budget.Nodes >= budget.Limits.MaxElements) throw SrlXmlBudget.Limit("XML node or depth limit exceeded",budget.Operation);
            ++budget.Nodes; budget.Text(Name);
            if (reader.MoveToFirstAttribute()) { do { if (budget.Attributes >= budget.Limits.MaxElements) throw SrlXmlBudget.Limit("XML attribute limit exceeded",budget.Operation); ++budget.Attributes; budget.Text(SrlXmlName(reader.NamespaceURI, reader.LocalName)); budget.Text(reader.Value); } while (reader.MoveToNextAttribute()); reader.MoveToElement(); }
            Kind = 's'; emptyEnd = reader.IsEmptyElement; IdentityEvent(); return;
          } else if (reader.NodeType == System.Xml.XmlNodeType.EndElement) { Kind = 'e'; IdentityEvent(); return; }
          else if (reader.NodeType == System.Xml.XmlNodeType.Text || reader.NodeType == System.Xml.XmlNodeType.CDATA || reader.NodeType == System.Xml.XmlNodeType.Whitespace || reader.NodeType == System.Xml.XmlNodeType.SignificantWhitespace) { budget.Text(reader.Value); Kind = 't'; IdentityEvent(); return; }
          else if (reader.NodeType == System.Xml.XmlNodeType.EntityReference || reader.NodeType == System.Xml.XmlNodeType.DocumentType) throw new System.FormatException("XML entities and DTDs are prohibited");
        }
        Kind = '\0';
      } catch (System.Xml.XmlException failure) { budget.Operation?.RethrowFailure();throw new System.FormatException("Malformed or unsupported XML document", failure); }
    }
    // Snapshot only active start-tag attributes for synchronous observers; descendants stay unretained.
    private System.Collections.Generic.Dictionary<string,string> IdentityAttributes() { var result=new System.Collections.Generic.Dictionary<string,string>();if(reader.MoveToFirstAttribute()){do{if(reader.NamespaceURI!="http://www.w3.org/2000/xmlns/")result.Add(SrlXmlName(reader.NamespaceURI,reader.LocalName),reader.Value);}while(reader.MoveToNextAttribute());reader.MoveToElement();}return result; }
    // Attach a nested owner before its already-current start token advances.
    internal SrlXmlIdentity IdentityScope(SrlXmlIdentityScope scope) { if(Kind!='s')throw new System.FormatException("Missing XML identity owner");var result=new SrlXmlIdentity(scope,IdentityLimits,budget.Operation?.Capture());result.Start(Name,IdentityAttributes(),reader.LookupNamespace);identities.Add(result);return result; }
    // Require local forward references before publishing the complete native candidate.
    internal void IdentityFinish(SrlXmlIdentity identity) { identity.Finish();if(!identities.Remove(identity))throw new System.FormatException("Unknown XML identity scope"); }
    // Deliver each active provider event while its exact namespace scope still exists.
    private void IdentityEvent() { if(identities.Count==0)return;var attributes=Kind=='s'?IdentityAttributes():null;foreach(var identity in identities)if(!identity.Complete){if(Kind=='s')identity.Start(Name,attributes!,reader.LookupNamespace);else if(Kind=='t')identity.Text(reader.Value);else if(Kind=='e')identity.End(reader.LookupNamespace);} }
    // Admit only XML indentation between generated record fields.
    internal void SkipSpace() { while (Kind == 't') { if (SrlXmlTrim(reader.Value).Length != 0) throw new System.FormatException("Unexpected XML record text"); Advance(); } }
    // Inspect a child identity without retaining or consuming its subtree.
    internal bool Has(string name) { SkipSpace(); return Kind == 's' && Name == name; }
    // Admit native scalar normalization after deferred attributes regain their actual member context.
    internal void AdmitScalar(string value){if(budget.Operation!=null){int count=SrlUtf8.GetByteCount(value);budget.Operation.Consume(Limits.OperationResource.WorkUnits,count);budget.Operation.Consume(Limits.OperationResource.AllocationBytes,count*2L);}}
    // Retain only active start-tag attributes across descendant traversal.
    internal SrlXmlInputFields Open(string name) {
      if (!Has(name)) throw new System.FormatException("Unexpected XML element");
      var attributes = new System.Collections.Generic.Dictionary<string, string>();
      if (reader.MoveToFirstAttribute()) { do { if (reader.NamespaceURI != "http://www.w3.org/2000/xmlns/" && !((acceptedType && reader.LocalName == "type" || acceptedNil && reader.LocalName == "nil") && reader.NamespaceURI == "http://www.w3.org/2001/XMLSchema-instance") && !attributes.TryAdd(SrlXmlName(reader.NamespaceURI, reader.LocalName), reader.Value)) throw new System.FormatException("Duplicate XML attribute"); } while (reader.MoveToNextAttribute()); reader.MoveToElement(); }
      acceptedType = false; acceptedNil = false; Advance(); return new SrlXmlInputFields(this, attributes);
    }
    // Check QName metadata in the payload's current namespace scope before advancing.
    internal void ExpectType(string expected, bool required) {
      string? selected = TypeIdentity(required);
      if (selected != null && selected != expected) throw new System.FormatException("Conflicting XML variant type");
    }
    // Resolve a type identity while the payload still owns its namespace scope.
    internal string? TypeIdentity(bool required) {
      SkipSpace(); if (Kind != 's') throw new System.FormatException("Missing XML variant payload");
      string? text = reader.GetAttribute("type", "http://www.w3.org/2001/XMLSchema-instance");
      if (text == null) { if (required) throw new System.FormatException("Required XML variant type missing"); return null; }
      text = SrlXmlTrim(text); string[] parts = text.Split(':'); string prefix = "", local = text;
      if (parts.Length == 2 && parts[0].Length != 0 && parts[1].Length != 0) { prefix = parts[0]; local = parts[1]; }
      else if (parts.Length != 1 || text.Length == 0) throw new System.FormatException("Invalid XML variant type QName");
      string? uri = reader.LookupNamespace(prefix);
      if (prefix.Length != 0 && uri == null) throw new System.FormatException("Undeclared XML variant type prefix");
      acceptedType = true;
      return SrlXmlName(uri ?? "", local);
    }
    // Distinguish absent, nil and present content before rebuilding native optional tuples.
    internal char Presence(string name, bool absent, bool nilAllowed) {
      if (!Has(name)) { if (absent) return 'a'; throw new System.FormatException("Required XML presence element missing"); }
      string? marker = reader.GetAttribute("nil", "http://www.w3.org/2001/XMLSchema-instance");
      if (marker == null) return 'v';
      if (!nilAllowed) throw new System.FormatException("XML xsi:nil is prohibited by presence policy");
      marker = SrlXmlTrim(marker);
      if (marker != "true" && marker != "1" && marker != "false" && marker != "0") throw new System.FormatException("Invalid XML xsi:nil Boolean");
      acceptedNil = true;
      if (marker == "false" || marker == "0") return 'v';
      if (Open(name).Text().Length != 0) throw new System.FormatException("Nil XML value contains text");
      return 'n';
    }
    // Retain only one scalar across adjacent entity, CDATA and text tokens.
    internal string Text() { var text = new System.Text.StringBuilder(); while (Kind == 't') { text.Append(reader.Value); Advance(); } if (Kind != 'e') throw new System.FormatException("XML scalar has nested content"); Advance(); return text.ToString(); }
    // Reject scalar attributes before consuming its complete text and end token.
    internal string Scalar(string name) { var fields = Open(name); if (fields.Attributes.Count != 0) throw new System.FormatException("XML scalar has nested content"); return Text(); }
    // Coalesce one logical mixed run across provider text/entity/CDATA/comment boundaries.
    internal string MixedText(SrlXmlMixedBudget mixed) {
      var text = new System.Text.StringBuilder(); bool charged = false;
      while (Kind == 't') {
        string chunk = reader.Value;
        if (chunk.Length != 0) { if (!charged) { mixed.Fragment(); charged = true; } mixed.Text(chunk); text.Append(chunk); }
        Advance();
      }
      return text.ToString();
    }
    // Require complete-message exhaustion before publishing the model candidate.
    internal void Finish() { SkipSpace(); if (Kind != '\0' || identities.Count != 0) throw new System.FormatException("Unexpected XML trailing content or incomplete identity scope"); }
    // Dispose the input provider and its owned stream on every success or failure path.
    public void Dispose() { reader.Dispose(); }
  }
  // Store only active-record attribute strings; child values go directly into the candidate.
  internal sealed class SrlXmlInputFields {
    private readonly SrlXmlInput input;
    internal readonly System.Collections.Generic.Dictionary<string, string> Attributes;
    private bool closed;
    // Bind one current start tag before recursive traversal.
    internal SrlXmlInputFields(SrlXmlInput input, System.Collections.Generic.Dictionary<string, string> attributes) { this.input = input; Attributes = attributes; }
    // Consume one exact required or optional attribute identity.
    internal string? Attribute(string name, bool optional) { if (Attributes.Remove(name, out string? value)) { input.AdmitScalar(value);return value; } if (!optional) throw new System.FormatException("Required XML attribute missing"); return null; }
    // Consume simple content after all mapped attributes have been checked.
    internal string Text() { if (Attributes.Count != 0) throw new System.FormatException("Unexpected XML attribute"); string text = input.Text(); closed = true; return text; }
    // Exhaust unknown attributes and require the matching record or wrapper end token.
    internal void Finish() { if (Attributes.Count != 0) throw new System.FormatException("Unexpected XML attribute"); if (!closed) { input.SkipSpace(); if (input.Kind != 'e') throw new System.FormatException("Unexpected XML field"); input.Advance(); } }
  }
  // Write escaped tokens while retaining only active ancestor names and attribute identities.
)xmlcs"
R"xmlcs(
  internal sealed class SrlXmlOutput {
    // Bound lexical list items through the operation's ordinary collection policy.
    internal int ListLimit => budget.Limits.MaxElements;
    // Admit joined UTF8 list storage before a builder retains another token or separator.
    internal int ListItem(string text, System.Text.StringBuilder joined, int used) {
      try { System.Xml.XmlConvert.VerifyXmlChars(text); }
      catch (System.Xml.XmlException failure) { throw new System.FormatException("Invalid XML character", failure); }
      int remaining = budget.Limits.MaxStringBytes - budget.TextBytes;
      int separator = joined.Length == 0 ? 0 : 1;
      if (separator > remaining - used) throw SrlXmlBudget.Limit("XML list text limit exceeded",budget.Operation);
      int count = SrlUtf8.GetByteCount(text);
      if (count > remaining - used - separator) throw SrlXmlBudget.Limit("XML list text limit exceeded",budget.Operation);
      if (separator != 0) joined.Append(' '); joined.Append(text); return used + separator + count;
    }
    private readonly SrlXmlBudget budget;
    internal readonly XmlMapLimits MapLimits;
    internal readonly XmlMixedLimits MixedLimits;
    internal readonly XmlIdentityLimits IdentityLimits;
    internal readonly XmlDigestLimits DigestLimits;
    internal readonly XmlExtensionOptions ExtensionOptions;
    private readonly System.Collections.Generic.List<SrlXmlIdentity> identities = new();
    private readonly System.IO.MemoryStream bytes = new();
    private readonly System.Collections.Generic.List<Frame> stack = new();
    // Track a pending start tag without retaining scalar values or descendants.
    private sealed class Frame {
      internal readonly string Name, Namespace, Expanded;
      internal readonly System.Collections.Generic.HashSet<string> Attributes = new();
      internal System.Collections.Generic.Dictionary<string,string>? IdentityAttributes, IdentityNamespaces;
      internal bool Pending = true, Scoped;
      // Bind the generated qualified name used by its end tag.
      internal Frame(string name, string xmlNamespace, string expanded) { Name = name; Namespace = xmlNamespace; Expanded=expanded; }
    }
    // Create independent output and budgets for one complete operation.
    internal SrlXmlOutput(Limits limits) : this(limits, new XmlMapLimits()) { }
    // Borrow the current native owner only when a package supplies shared accounting.
    internal global::System.IDisposable? FieldOwner()=>budget.Operation?.EnterOwner();
    internal Limits.IOperation? Operation=>budget.Operation;
    // Select a native sibling before its scalar or descendant work is admitted.
    internal void Field(string member)=>budget.Operation?.SelectField(member);
    // Carry one immutable map policy through every nested typed encoder.
    internal SrlXmlOutput(Limits limits, XmlMapLimits maps) : this(limits, maps, new XmlMixedLimits()) { }
    // Share immutable mixed/map policies while each sequence keeps independent counters.
    internal SrlXmlOutput(Limits limits, XmlMapLimits maps, XmlMixedLimits mixed) : this(limits,maps,mixed,new XmlIdentityLimits()) { }
    // Share explicit identity admission while owner indexes retain independent mutable state.
    internal SrlXmlOutput(Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity) : this(limits,maps,mixed,identity,new XmlDigestLimits()) { }
    // Bound exact owning digest bytes independently from ordinary document limits.
    internal SrlXmlOutput(Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests) : this(limits,maps,mixed,identity,digests,new XmlExtensionOptions()) { }
    // Preserve earlier constructor contracts while carrying independent extension policies.
    internal SrlXmlOutput(Limits limits, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests, XmlExtensionOptions extensions) { ExtensionOptions=extensions??throw new System.ArgumentNullException(nameof(extensions)); budget = new SrlXmlBudget(limits); MapLimits = maps ?? throw new System.ArgumentNullException(nameof(maps)); MixedLimits = mixed ?? throw new System.ArgumentNullException(nameof(mixed)); IdentityLimits=identity ?? throw new System.ArgumentNullException(nameof(identity)); DigestLimits=digests ?? throw new System.ArgumentNullException(nameof(digests)); }
    // Attach metadata after opening its still-pending owner start tag.
    internal SrlXmlIdentity IdentityScope(SrlXmlIdentityScope scope) { var result=new SrlXmlIdentity(scope,IdentityLimits,budget.Operation?.Capture());identities.Add(result);stack[^1].IdentityAttributes=new();return result; }
    // Resolve only declarations actually emitted by the direct writer.
    internal string? IdentityNamespace(string prefix) {if(prefix=="xml")return "http://www.w3.org/XML/1998/namespace";for(int index=stack.Count-1;index>=0;--index){var frame=stack[index];if(frame.IdentityNamespaces!=null&&frame.IdentityNamespaces.TryGetValue(prefix,out string? value))return value;int colon=frame.Name.IndexOf(':');if(!frame.Scoped&&colon>=0&&frame.Name[..colon]==prefix)return frame.Namespace;}return prefix.Length==0?"":null;}
    // Observe a complete pending start tag after native attributes pass all checks.
    private void IdentityStart(Frame frame) { foreach(var identity in identities)if(!identity.Complete)identity.Start(frame.Expanded,frame.IdentityAttributes??new(),IdentityNamespace); }
    // Resolve every local forward reference before releasing its owner index.
    internal void IdentityFinish(SrlXmlIdentity identity) { identity.Finish();if(!identities.Remove(identity))throw new System.FormatException("Unknown XML identity scope"); }
    // Split compiler-owned expanded-name spelling.
    private static (string Uri, string Local) Parts(string value) { int close = value.StartsWith("{", System.StringComparison.Ordinal) ? value.IndexOf('}') : -1; return close < 0 ? ("", value) : (value[1..close], value[(close + 1)..]); }
    // Charge encoded bytes before growing owned output storage.
    private void Append(string value) { int count = SrlUtf8.GetByteCount(value);budget.Operation?.Consume(Limits.OperationResource.WorkUnits,count);budget.Operation?.Consume(Limits.OperationResource.AllocationBytes,count); if (count > budget.Limits.MaxBytes - bytes.Length) throw SrlXmlBudget.Limit("XML output byte limit exceeded",budget.Operation); bytes.Write(SrlUtf8.GetBytes(value)); }
    // Escape only XML syntax and whitespace requiring lexical preservation.
    private void Escaped(string value, bool attribute) { int begin = 0; for (int index = 0; index < value.Length; ++index) { string? replacement = value[index] switch { '&' => "&amp;", '<' => "&lt;", '>' => "&gt;", '\r' => "&#13;", '"' => attribute ? "&quot;" : null, '\n' => attribute ? "&#10;" : null, '\t' => attribute ? "&#9;" : null, _ => null }; if (replacement != null) { Append(value[begin..index]); Append(replacement); begin = index + 1; } } Append(value[begin..]); }
    // Complete a pending start tag before writing content.
    private void Seal() { if (stack.Count != 0 && stack[^1].Pending) { IdentityStart(stack[^1]); Append(">"); stack[^1].Pending = false; } }
    // Admit and emit an element before visiting any model descendants.
    internal void Open(string name) { if(stack.Count!=0&&stack[^1].Scoped){Open(name,null);return;} if (stack.Count >= budget.Limits.MaxDepth || budget.Nodes >= budget.Limits.MaxElements) throw SrlXmlBudget.Limit("XML node or depth limit exceeded",budget.Operation); budget.Text(name); ++budget.Nodes; Seal(); var (uri, local) = Parts(name); string prefix = uri == "http://www.w3.org/XML/1998/namespace" ? "xml" : "srl"; string qualified = uri.Length == 0 ? local : prefix + ":" + local; Append("<" + qualified); if (uri.Length != 0 && prefix != "xml" && (stack.Count == 0 || stack[^1].Namespace != uri)) { Append(" xmlns:srl=\""); Escaped(uri, true); Append("\""); } stack.Add(new Frame(qualified, uri, name)); }
    // Choose a deterministic prefix without shadowing an opaque lexical QName binding.
    internal string FreshPrefix(string basis,string uri){for(int index=0;;++index){string prefix=basis+(index==0?"":index.ToString(System.Globalization.CultureInfo.InvariantCulture));string? bound=IdentityNamespace(prefix);if(bound==null||bound==uri)return prefix;}}
    // Admit the private rewritten QName lexical builder before retaining its tokens.
    internal void NamespaceRetention(long size){budget.Operation?.Consume(Limits.OperationResource.WorkUnits,size+1);budget.Operation?.Consume(Limits.OperationResource.AllocationBytes,64L+size*2);}
    // Reuse existing bindings so retained semantic scope charges stay stable across roundtrips.
    internal string? MatchingPrefix(string uri,bool attribute){if(!attribute&&IdentityNamespace("")==uri)return "";for(int index=stack.Count-1;index>=0;--index){var frame=stack[index];if(frame.IdentityNamespaces!=null)foreach(var entry in frame.IdentityNamespaces)if((!attribute||entry.Key.Length!=0)&&entry.Value==uri&&IdentityNamespace(entry.Key)==uri)return entry.Key;if(!frame.Scoped&&frame.Namespace==uri){int colon=frame.Name.IndexOf(':');if(colon>=0){string prefix=frame.Name[..colon];if(IdentityNamespace(prefix)==uri)return prefix;}}}return null;}
    // Charge and emit only declarations which differ from their inherited scope.
    internal void Declare(string prefix,string uri){if(IdentityNamespace(prefix)==uri)return;if(budget.Attributes>=budget.Limits.MaxElements)throw SrlXmlBudget.Limit("XML namespace limit exceeded",budget.Operation);++budget.Attributes;budget.Text(prefix);budget.Text(uri);var frame=stack[^1];frame.IdentityNamespaces??=new();frame.IdentityNamespaces[prefix]=uri;Append(prefix.Length==0?" xmlns=\"":" xmlns:"+prefix+"=\"");Escaped(uri,true);Append("\"");}
    // Pin every preserved binding before selecting a known or opaque element prefix.
    internal void Open(string name,System.Collections.Generic.Dictionary<string,string>? scope){
      if(stack.Count>=budget.Limits.MaxDepth||budget.Nodes>=budget.Limits.MaxElements)throw SrlXmlBudget.Limit("XML node or depth limit exceeded",budget.Operation);budget.Text(name);++budget.Nodes;Seal();var (uri,local)=Parts(name);string prefix="";
      var temporary=new Frame("",uri,name){Scoped=true,IdentityNamespaces=scope};stack.Add(temporary);if(uri.Length!=0)prefix=MatchingPrefix(uri,false)??(uri=="http://www.w3.org/XML/1998/namespace"?"xml":FreshPrefix("srl",uri));stack.RemoveAt(stack.Count-1);
      string qualified=prefix.Length==0?local:prefix+":"+local;Append("<"+qualified);var frame=new Frame(qualified,uri,name){Scoped=true};stack.Add(frame);if(scope!=null)foreach(var entry in scope)Declare(entry.Key,entry.Value);
      if(uri.Length==0){if(IdentityNamespace("")!.Length!=0)Declare("","");}else Declare(prefix,uri);
    }
    // Preserve expanded duplicate identity while avoiding all pinned namespace prefixes.
    private void ScopedAttribute(string name,string text){var frame=stack[^1];if(!frame.Pending||!frame.Attributes.Add(name))throw new System.FormatException("Invalid XML attribute position or duplicate");if(budget.Attributes>=budget.Limits.MaxElements)throw SrlXmlBudget.Limit("XML attribute limit exceeded",budget.Operation);++budget.Attributes;budget.Text(name);budget.Text(text);var (uri,local)=Parts(name);string prefix="";if(uri.Length!=0){prefix=MatchingPrefix(uri,true)??(uri=="http://www.w3.org/XML/1998/namespace"?"xml":FreshPrefix("a",uri));Declare(prefix,uri);}Append(" "+(prefix.Length==0?local:prefix+":"+local)+"=\"");Escaped(text,true);Append("\"");if(identities.Count!=0){frame.IdentityAttributes??=new();frame.IdentityAttributes.Add(name,text);}}
)xmlcs"
R"xmlcs(
    // Add each attribute before the first content token.
    internal void Attribute(string name, string text) { if(stack[^1].Scoped){ScopedAttribute(name,text);return;} var frame = stack[^1]; if (!frame.Pending || !frame.Attributes.Add(name)) throw new System.FormatException("Invalid XML attribute position or duplicate"); if (budget.Attributes >= budget.Limits.MaxElements) throw SrlXmlBudget.Limit("XML attribute limit exceeded",budget.Operation); ++budget.Attributes; budget.Text(name); budget.Text(text); var (uri, local) = Parts(name); string prefix = uri == "http://www.w3.org/XML/1998/namespace" ? "xml" : "a" + frame.Attributes.Count; if (uri.Length != 0 && prefix != "xml") { Append(" xmlns:" + prefix + "=\""); Escaped(uri, true); Append("\""); } Append(" " + (uri.Length == 0 ? local : prefix + ":" + local) + "=\""); Escaped(text, true); Append("\""); if(identities.Count!=0){frame.IdentityAttributes??=new();frame.IdentityAttributes.Add(name,text);if(uri.Length!=0&&prefix!="xml"){frame.IdentityNamespaces??=new();frame.IdentityNamespaces[prefix]=uri;}} }
    // Declare a QName prefix on this pending payload before emitting its xsi:type attribute.
    internal void TypeName(string identity) {
      var frame = stack[^1]; if (!frame.Pending) throw new System.FormatException("XML variant type follows content");
      var (uri, local) = Parts(identity); string lexical = local;
      if(frame.Scoped){if(uri.Length!=0){string prefix=uri=="http://www.w3.org/XML/1998/namespace"?"xml":FreshPrefix("t",uri);Declare(prefix,uri);lexical=prefix+":"+local;}else if(IdentityNamespace("")!.Length!=0)throw new System.FormatException("Unqualified XML type conflicts with preserved default namespace");Attribute("{http://www.w3.org/2001/XMLSchema-instance}type",lexical);return;}
      if (uri == "http://www.w3.org/XML/1998/namespace") lexical = "xml:" + lexical;
      else if (uri.Length != 0) {
        if (budget.Attributes >= budget.Limits.MaxElements) throw SrlXmlBudget.Limit("XML namespace limit exceeded",budget.Operation);
        ++budget.Attributes; budget.Text("t" + uri); if(identities.Count!=0){frame.IdentityNamespaces??=new();frame.IdentityNamespaces["t"]=uri;} Append(" xmlns:t=\""); Escaped(uri, true); Append("\""); lexical = "t:" + lexical;
      }
      Attribute("{http://www.w3.org/2001/XMLSchema-instance}type", lexical);
    }
    // Validate and emit one scalar without a retained XML node.
    internal void Text(string text) { budget.Text(text); Seal(); foreach(var identity in identities)if(!identity.Complete)identity.Text(text); Escaped(text, false); }
    // Release each completed ancestor after its matching end token.
    internal void Close() { var frame = stack[^1];if(frame.Pending)IdentityStart(frame);foreach(var identity in identities)if(!identity.Complete)identity.End(IdentityNamespace);stack.RemoveAt(stack.Count - 1);Append(frame.Pending ? "/>" : "</" + frame.Name + ">"); }
    // Publish bytes only after the generated traversal completes.
    internal byte[] Finish() { if (stack.Count != 0 || identities.Count != 0) throw new System.FormatException("Incomplete XML output or identity scope");budget.Operation?.Consume(Limits.OperationResource.AllocationBytes,bytes.Length);return bytes.ToArray(); }
  }
  // Optional immutable prepared XSD1.0 bundle; validation contexts are created per call.
  public sealed class XmlSchema {
    private readonly System.Xml.Schema.XmlSchemaSet schemas;
    // Compile a trusted standalone schema with all external resolution prohibited.
    public XmlSchema(byte[] xsd) { var tree = SrlXmlParse(xsd, new Limits()); var pending = new System.Collections.Generic.List<SrlXmlNode> { tree }; for (int index = 0; index < pending.Count; ++index) { var node = pending[index]; foreach (var kind in new[] { "include", "import", "redefine", "override" }) if (node.Name == SrlXmlName("http://www.w3.org/2001/XMLSchema", kind)) throw new System.FormatException("Runtime XSD external references are unsupported"); pending.AddRange(node.Children); } schemas = new System.Xml.Schema.XmlSchemaSet { XmlResolver = null }; using var stream = new System.IO.MemoryStream(xsd, false); using var reader = System.Xml.XmlReader.Create(stream, new System.Xml.XmlReaderSettings { DtdProcessing = System.Xml.DtdProcessing.Prohibit, XmlResolver = null }); schemas.Add(null, reader); schemas.Compile(); }
    // Validate current document values after complete bounded and secure parsing.
    public void Validate(byte[] bytes, Limits limits) {
      using (var input = new SrlXmlInput(bytes, limits)) { while (input.Kind != '\0') input.Advance(); input.Finish(); }
      var settings = new System.Xml.XmlReaderSettings { DtdProcessing = System.Xml.DtdProcessing.Prohibit, XmlResolver = null, Schemas = schemas, ValidationType = System.Xml.ValidationType.Schema, MaxCharactersInDocument = limits.MaxBytes };
      settings.ValidationFlags = System.Xml.Schema.XmlSchemaValidationFlags.ProcessIdentityConstraints;
      settings.ValidationEventHandler += (_, args) => throw new System.FormatException("XML does not satisfy the selected XSD", args.Exception);
      using var stream = new System.IO.MemoryStream(bytes, false);
      using var reader = System.Xml.XmlReader.Create(stream, settings);
      bool rootSeen = false;
      while (reader.Read()) {
        if (!rootSeen && reader.NodeType == System.Xml.XmlNodeType.Element) {
          // System.Xml otherwise reports an undeclared root only as an optional warning.
          rootSeen = true;
          if (!schemas.GlobalElements.Contains(new System.Xml.XmlQualifiedName(reader.LocalName, reader.NamespaceURI))) throw new System.FormatException("XML root is not declared in the selected XSD");
        }
      }
    }
  }
)xmlcs"
