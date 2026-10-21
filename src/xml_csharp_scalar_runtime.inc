// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlscalar(
  private readonly string kind, whitespace;
  private readonly string[] bounds, enumeration;
  private readonly long length, minimum, maximum;
  private readonly bool finite;
  private readonly Pattern[][] groups;
  private readonly string listKind;
  private readonly string? fixedValue;
  private readonly SrlXmlRule[] members;
  private string[][]? qnames, fixedNames, defaultNames;
  private string[][][]? qnameLists;
  internal string? DefaultNamespaceText;
  internal bool DefaultNamespaceAttribute;
  internal System.Collections.Generic.Dictionary<string,string> DefaultNamespaceScope=new(System.StringComparer.Ordinal);
  // Bind compiler-owned expanded identities before publishing the prepared rule.
  internal SrlXmlRule NamespaceValues(string[][] names,string[][][] lists){qnames=names;qnameLists=lists;return this;}
  // Keep aliases observed by lexical patterns and prefix-list string enumerations.
  internal SrlXmlRule NamespaceLexical(){KeepNamespaceLexical=true;return this;}
  internal bool KeepNamespaceLexical;
  // Retain schema aliases separately from the instance vocabulary used by an explicit value.
  internal SrlXmlRule NamespaceDefault(string text,string[][] bindings,string[][] names,bool fixedValue,bool attribute){DefaultNamespaceText=text;defaultNames=names;DefaultNamespaceAttribute=attribute;fixedNames=fixedValue?names:null;foreach(var binding in bindings)DefaultNamespaceScope.Add(binding[0],binding[1]);return this;}
  // Preserve ordinary string alternatives without assigning QName semantics to their spelling.
  internal int NamespaceMode()=>listKind=="QName"?1:kind=="QName"?0:-1;
  // Compare the complete schema default before omitting an optional namespace-valued attribute.
  internal bool MatchesNamespaceDefault(string text,System.Collections.Generic.Dictionary<string,string> scope,int maximum,Limits.IOperation? operation){if(!DefaultNamespaceAttribute||defaultNames==null)return false;var work=new Work(MAX_WORK_UNITS,maximum,operation);work.Allocate(128L+text.Length*4L);string[] tokens=Tokens(text,maximum);var names=new (string Prefix,string Uri,string Local)[tokens.Length];bool empty=false;for(int index=0;index<tokens.Length;++index){names[index]=SrlXmlNamespaceValues.Resolve(tokens[index],NamespaceMode(),scope);empty|=names[index].Uri.Length==0;}return empty&&EqualNames(names,defaultNames,work);}
  // Retain compiler-owned immutable scalar metadata once per generated field.
  internal SrlXmlRule(string kind, string whitespace, int bits, string[] bounds, long length,
      long minimum, long maximum, string[] enumeration, bool finite, Pattern[][] groups,
      string listKind, SrlXmlRule[] members, string? fixedValue) {
    this.kind = kind; this.whitespace = whitespace; this.bounds = bounds;
    this.length = length; this.minimum = minimum; this.maximum = maximum;
    this.enumeration = enumeration; this.finite = finite; this.groups = groups;
    this.listKind = listKind; this.members = members; this.fixedValue = fixedValue;
  }
  // Normalize XML whitespace without collapsing unrelated Unicode separators.
  private string Normalize(string text) {
    if (whitespace != "replace" && whitespace != "collapse") return text;
    text = text.Replace('\t', ' ').Replace('\r', ' ').Replace('\n', ' ');
    if (whitespace != "collapse") return text;
    var output = new System.Text.StringBuilder(); bool pending = false;
    foreach (char value in text) {
      if (value == ' ') { pending = output.Length != 0; continue; }
      if (pending) output.Append(' '); output.Append(value); pending = false;
    }
    return output.ToString();
  }
  // Compare exact integer and width-rounded floating value spaces; null means unordered.
  private int? Compare(string left, string right) {
    if (listKind.Length != 0) {
      string[] first = Tokens(left, int.MaxValue), second = Tokens(right, int.MaxValue);
      if (first.Length != second.Length) return 1;
      var item = ItemRule(listKind);
      for (int index = 0; index < first.Length; ++index) if (item.Compare(first[index], second[index]) != 0) return 1;
      return 0;
    }
    if (kind == "float" || kind == "double") {
      double first = SrlXmlFloat(left, kind == "float"), second = SrlXmlFloat(right, kind == "float");
      if (double.IsNaN(first) || double.IsNaN(second)) return double.IsNaN(first) && double.IsNaN(second) ? 0 : null;
      return first < second ? -1 : first > second ? 1 : 0;
    }
    if (kind.StartsWith("unsigned", System.StringComparison.Ordinal) || kind == "byte" || kind == "short" || kind == "int" || kind == "long") {
      bool unsigned = kind.StartsWith("unsigned", System.StringComparison.Ordinal);
      int width = IntegerWidth(kind);
      return SrlXmlInteger(left, width, unsigned).CompareTo(SrlXmlInteger(right, width, unsigned));
    }
    if (kind == "boolean") return SrlXmlBoolean(left) == SrlXmlBoolean(right) ? 0 : 1;
    if (kind == "decimal" || IntegerKind(kind)) return CompareDecimal(left, right);
    return string.CompareOrdinal(left, right);
  }
  // Split only XML whitespace; an empty lexical list contains zero items.
  internal static string[] Tokens(string text) { return Tokens(text, MAX_LIST_ITEMS); }
  // Retain the ordinary default list policy while attaching package-only resource classification.
  internal static string[] Tokens(string text,Limits.IOperation? operation) {
    try{return Tokens(text);}catch(LimitFailure failure){operation?.ResourceLimit(failure.Message);throw;}
  }
  // Admit the complete item count before Split allocates a token array and its item strings.
  internal static string[] Tokens(string text, int maximum) {
    int count = 0; bool item = false;
    foreach (char value in text) {
      bool space = value == ' ' || value == '\t' || value == '\r' || value == '\n';
      if (!space && !item) { if (count >= maximum) throw new LimitFailure("XML list item limit exceeded"); ++count; }
      item = !space;
    }
    return text.Split(new char[]{' ', '\t', '\r', '\n'}, System.StringSplitOptions.RemoveEmptyEntries);
  }
  // Locate bounded list failures only while a package operation is attached.
  internal static string[] Tokens(string text,int maximum,Limits.IOperation? operation) {
    try{return Tokens(text,maximum);}catch(LimitFailure failure){operation?.ResourceLimit(failure.Message);throw;}
  }
  // Materialize XSD defaults without treating an existing empty attribute as missing.
  internal static string DefaultText(string? text, string fallback, bool attribute) { return text == null || !attribute && text.Length == 0 ? fallback : text; }
  // Build one immutable unrestricted builtin list-item contract.
  private static SrlXmlRule ItemRule(string kind) { return new SrlXmlRule(kind, "collapse", 0, new string[]{"", "", "", ""}, -1, -1, -1, System.Array.Empty<string>(), false, System.Array.Empty<Pattern[]>(), "", System.Array.Empty<SrlXmlRule>(), null); }
  // Derive builtin integer width independently of native string storage.
  private static int IntegerWidth(string kind) { return kind switch { "byte" or "unsignedByte" => 8, "short" or "unsignedShort" => 16, "int" or "unsignedInt" => 32, "long" or "unsignedLong" => 64, _ => 0 }; }
  // Recognize the unbounded signed integer family.
  private static bool IntegerKind(string kind) { return kind == "integer" || kind == "positiveInteger" || kind == "negativeInteger" || kind == "nonPositiveInteger" || kind == "nonNegativeInteger"; }
  // Compare arbitrarily large finite decimal values without host decimal rounding or huge padding.
  private static int CompareDecimal(string left, string right) {
    static (bool Negative, string Whole, string Fraction) Parts(string value) {
      bool negative = value.StartsWith("-", System.StringComparison.Ordinal);
      if (value.StartsWith("+", System.StringComparison.Ordinal) || negative) value = value.Substring(1);
      int point = value.IndexOf('.'); string whole = (point < 0 ? value : value.Substring(0, point)).TrimStart('0');
      string fraction = point < 0 ? "" : value.Substring(point + 1).TrimEnd('0');
      return (negative && (whole.Length != 0 || fraction.Length != 0), whole, fraction);
    }
    var first = Parts(left); var second = Parts(right);
    if (first.Negative != second.Negative) return first.Negative ? -1 : 1;
    int order = first.Whole.Length.CompareTo(second.Whole.Length);
    if (order == 0) order = string.CompareOrdinal(first.Whole, second.Whole);
    if (order == 0) for (int index = 0; index < System.Math.Max(first.Fraction.Length, second.Fraction.Length); ++index) {
      char a = index < first.Fraction.Length ? first.Fraction[index] : '0', b = index < second.Fraction.Length ? second.Fraction[index] : '0';
      if (a != b) { order = a < b ? -1 : 1; break; }
    }
    return first.Negative ? -System.Math.Sign(order) : System.Math.Sign(order);
  }
  // Check builtin lexical and value-space rules before facets compare candidates.
  private void Lexical(string text) {
    int width = IntegerWidth(kind);
    if (width != 0) { SrlXmlInteger(text, width, kind.StartsWith("unsigned", System.StringComparison.Ordinal)); return; }
    if (IntegerKind(kind)) {
      if (!System.Text.RegularExpressions.Regex.IsMatch(text, @"\A[+-]?[0-9]+\z", System.Text.RegularExpressions.RegexOptions.CultureInvariant)) throw new System.FormatException("Invalid XML integer");
      int sign = System.Numerics.BigInteger.Parse(text, System.Globalization.CultureInfo.InvariantCulture).Sign;
      if (kind == "positiveInteger" && sign <= 0 || kind == "negativeInteger" && sign >= 0 || kind == "nonPositiveInteger" && sign > 0 || kind == "nonNegativeInteger" && sign < 0) throw new System.FormatException("XML integer sign failed");
    } else if (kind == "decimal") {
      if (!System.Text.RegularExpressions.Regex.IsMatch(text, @"\A[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\z", System.Text.RegularExpressions.RegexOptions.CultureInvariant)) throw new System.FormatException("Invalid XML decimal");
)xmlscalar"
R"xmlscalar(    } else if (kind == "boolean") SrlXmlBoolean(text);
    else if (kind == "float" || kind == "double") SrlXmlFloat(text, kind == "float");
    else if (kind == "language") { if (!System.Text.RegularExpressions.Regex.IsMatch(text, @"\A[A-Za-z]{1,8}(?:-[A-Za-z0-9]{1,8})*\z", System.Text.RegularExpressions.RegexOptions.CultureInvariant)) throw new System.FormatException("Invalid XML language"); }
    else if (kind == "Name" || kind == "NCName" || kind == "NMTOKEN" || kind == "ID" || kind == "IDREF" || kind == "ENTITY") {
      if (text.Length == 0) throw new System.FormatException("Empty XML name token");
      try { if (kind == "NMTOKEN") System.Xml.XmlConvert.VerifyNMTOKEN(text); else if (kind == "Name") System.Xml.XmlConvert.VerifyName(text); else System.Xml.XmlConvert.VerifyNCName(text); }
      catch (System.Xml.XmlException failure) { throw new System.FormatException("Invalid XML name token", failure); }
    }
  }
  // Read one XML scalar without accepting an isolated UTF16 surrogate.
  private static int Scalar(string text, ref int offset) {
    char first = text[offset++]; int scalar = first;
    if (char.IsHighSurrogate(first)) {
      if (offset >= text.Length || !char.IsLowSurrogate(text[offset])) throw new System.FormatException("Invalid XML Unicode scalar");
      scalar = char.ConvertToUtf32(first, text[offset++]);
    } else if (char.IsLowSurrogate(first)) throw new System.FormatException("Invalid XML Unicode scalar");
    if (!(scalar == 9 || scalar == 10 || scalar == 13 || scalar >= 32 && scalar <= 0xd7ff || scalar >= 0xe000 && scalar <= 0xfffd || scalar >= 0x10000 && scalar <= 0x10ffff)) throw new System.FormatException("Invalid XML character");
    return scalar;
  }
  // Apply one shared check before native input conversion or complete output publication.
  internal string Check(string text) {
    return Check(text, MAX_LIST_ITEMS);
  }
  // Carry operation-specific collection admission through each recursive list/union branch.
  internal string Check(string text, int maximumItems) {
    if (text is null) throw new System.FormatException("Null XML scalar value");
    return Check(text, new Work(MAX_WORK_UNITS, maximumItems));
  }
  // Preserve ordinary scalar-limit exceptions while packages retain their selected native field.
  internal string Check(string text,int maximumItems,Limits.IOperation? operation) {
    try{return Check(text,maximumItems);}catch(LimitFailure failure){operation?.ResourceLimit(failure.Message);throw;}
  }
  // Preserve present normalized empty tokens without activating an element's nonempty default.
  internal string Output(string text, string fallback, int maximumItems) {
    text = Check(text, maximumItems);
    if (text.Length == 0 && Normalize(fallback).Length != 0) {
      if (whitespace == "collapse") return " ";
      throw new System.FormatException("Empty XML value would activate a different XSD default");
    }
    return text;
  }
  // Apply the same package-only classification when a default-preserving output rule exhausts work.
  internal string Output(string text,string fallback,int maximumItems,Limits.IOperation? operation) {
    try{return Output(text,fallback,maximumItems);}catch(LimitFailure failure){operation?.ResourceLimit(failure.Message);throw;}
  }
  // Share work across recursive union alternatives; never swallow a resource failure.
  private string Check(string text, Work work) {
    work.Spend(text.Length); text = Normalize(text); Lexical(text);
    SrlXmlRule selected=this;
    if (members.Length != 0) {
      bool matched = false;
      foreach (SrlXmlRule member in members) {
        try { text=member.Check(text, work);selected=work.Selected;matched = true; break; }
        catch (LimitFailure) { throw; }
        catch (System.FormatException) { work.CallbackFailure?.Throw(); /* Another complete union branch may match. */ }
      }
      if (!matched) throw new System.FormatException("XML union has no matching member");
    }
    long count = 0;
    for (int offset = 0; offset < text.Length; ++count) Scalar(text, ref offset);
    if (listKind.Length != 0) { string[] items = Tokens(text, work.MaximumItems); count = items.Length; var item = ItemRule(listKind); foreach (string value in items) item.Check(value, work); }
    if (length >= 0 && count != length || minimum >= 0 && count < minimum || maximum >= 0 && count > maximum) throw new System.FormatException("XML length facet failed");
    for (int index = 0; index < bounds.Length; ++index) {
      if (bounds[index].Length == 0) continue;
      int? order = Compare(text, bounds[index]);
      if (order == null || index == 0 && order.Value < 0 || index == 1 && order.Value > 0 || index == 2 && order.Value <= 0 || index == 3 && order.Value >= 0) throw new System.FormatException("XML numeric facet failed");
    }
    if (enumeration.Length != 0 && (qnames==null||qnames.Length==0) && (qnameLists==null||qnameLists.Length==0)) {
      bool matched = false;
      foreach (string allowed in enumeration) if (Compare(text, Normalize(allowed)) == 0) { matched = true; break; }
      if (!matched) throw new System.FormatException("XML enumeration facet failed");
    }
    if (fixedValue != null && fixedNames==null && Compare(text, Normalize(fixedValue)) != 0) throw new System.FormatException("XML fixed value differs");
    if(work.Scope!=null&&NamespaceMode()>=0)CheckNames(text,work);
    if (finite && !double.IsFinite(SrlXmlFloat(text, kind == "float"))) throw new System.FormatException("XML finite facet failed");
    if (groups.Length != 0) {
      foreach (Pattern[] group in groups) {
        bool matched = false;
        foreach (Pattern pattern in group) if (pattern.Matches(text, work)) { matched = true; break; }
        if (!matched) throw new System.FormatException("XML portable pattern facet failed");
      }
    }
    work.Selected=selected;return text;
  }
  // Validate one complete value in its retained scope, preserving the first successful member.
  internal Work CheckNamespace(string text,System.Collections.Generic.Dictionary<string,string> scope,int maximum,Limits.IOperation? operation){var work=new Work(MAX_WORK_UNITS,maximum,operation){Scope=scope};try{work.Checked=Check(text,work);return work;}catch(LimitFailure failure){operation?.ResourceLimit(failure.Message);throw;}}
  // Admit temporary identities before resolving or comparing any instance aliases.
  private void CheckNames(string text,Work work){
    const int identityEntryBytes=64;int count=0;bool inside=false;foreach(char character in text){bool space=character==' '||character=='\t'||character=='\r'||character=='\n';if(!space&&!inside)++count;inside=!space;}
    work.Allocate((long)identityEntryBytes*count+text.Length*4L);string[] tokens=Tokens(text,work.MaximumItems);int mode=NamespaceMode();if(mode==0&&tokens.Length!=1)throw new System.FormatException("XML QName requires one name");
    var names=new (string Prefix,string Uri,string Local)[tokens.Length];for(int index=0;index<tokens.Length;++index)names[index]=SrlXmlNamespaceValues.Resolve(tokens[index],mode,work.Scope!);
    if(fixedNames!=null&&!EqualNames(names,fixedNames,work))throw new System.FormatException("XML fixed QName value differs");
    if(qnames!=null&&qnames.Length!=0){foreach(var allowed in qnames)if(EqualNames(names,new[]{allowed},work))return;throw new System.FormatException("XML QName enumeration facet failed");}
    if(qnameLists!=null&&qnameLists.Length!=0){foreach(var allowed in qnameLists)if(EqualNames(names,allowed,work))return;throw new System.FormatException("XML QName enumeration facet failed");}
  }
  // Compare ordered expanded values while excluding lexical prefixes from equality.
  private static bool EqualNames((string Prefix,string Uri,string Local)[] actual,string[][] expected,Work work){work.Spend(1);if(actual.Length!=expected.Length)return false;for(int index=0;index<actual.Length;++index){var name=actual[index];work.Spend(name.Uri.Length+name.Local.Length+1);if(name.Uri!=expected[index][0]||name.Local!=expected[index][1])return false;}return true;}
  // Preserve bounded evaluation failures while union branches probe ordinary lexical failures.
  private sealed class LimitFailure : System.FormatException {
    // Retain the precise resource diagnostic used by ordinary pattern checks.
    internal LimitFailure(string message) : base(message) { }
  }
)xmlscalar"
R"xmlscalar(
  // Bound all alternatives and inherited restriction groups within one scalar match.
  internal sealed class Work {
    private int remaining;
    internal readonly int MaximumItems;
    private readonly Limits.IOperation? operation;
    internal System.Collections.Generic.Dictionary<string,string>? Scope;
    internal SrlXmlRule Selected=null!;
    internal string Checked=string.Empty;
    internal System.Runtime.ExceptionServices.ExceptionDispatchInfo? CallbackFailure;
    // Retain one private finite allowance for this match operation.
    internal Work(int remaining, int maximumItems,Limits.IOperation? operation=null) { this.remaining = remaining; MaximumItems = maximumItems;this.operation=operation; }
    // Preserve caller exception identity rather than treating an admission failure as a union mismatch.
    internal void Allocate(long bytes){try{operation?.Consume(Limits.OperationResource.AllocationBytes,bytes);}catch(System.Exception failure){CallbackFailure=System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure);throw;}}
    // Charge before clearing state storage, following edges or inspecting characters.
    internal void Spend(int units) {try{operation?.Consume(Limits.OperationResource.WorkUnits,units);}catch(System.Exception failure){CallbackFailure=System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure);throw;} if (units < 0 || units > remaining) throw new LimitFailure("Portable XML regex work limit exceeded"); remaining -= units; }
  }
  // Execute only compiler-lowered Thompson instructions, without host-regex semantics.
  internal sealed class Pattern {
    private const int CONSUMES = 1, INVERTED = 2, DOT = 4, ACCEPTING = 8;
    private readonly int start;
    private readonly State[] states;
    // Retain immutable instructions prepared during generated type initialization.
    private Pattern(int start, State[] states) { this.start = start; this.states = states; }
    // Retain one character predicate or epsilon instruction and its checked successors.
    private sealed class State {
      internal readonly int[] Epsilon, Ranges; internal readonly int Next, Flags;
      // Bind private compiler-owned arrays that are never returned to callers.
      internal State(int[] epsilon, int[] ranges, int next, int flags) { Epsilon = epsilon; Ranges = ranges; Next = next; Flags = flags; }
    }
    // Parse one bounded compiler-owned integer list with invariant culture.
    private static int[] Integers(string text) {
      if (text.Length == 0) return System.Array.Empty<int>();
      string[] pieces = text.Split(','); int[] result = new int[pieces.Length];
      for (int index = 0; index < result.Length; ++index) result[index] = int.Parse(pieces[index], System.Globalization.NumberStyles.None, System.Globalization.CultureInfo.InvariantCulture);
      return result;
    }
    // Split ASCII literals so the same generated programs also fit JVM constant limits.
    internal static Pattern Prepare(string[] fragments) {
      string[] records = string.Concat(fragments).Split(';'); int count = records.Length - 1;
      if (count <= 0 || count > MAX_STATES) throw new System.FormatException("Portable XML regex state limit exceeded");
      int start = int.Parse(records[0], System.Globalization.CultureInfo.InvariantCulture);
      if (start < 0 || start >= count) throw new System.FormatException("Invalid portable XML regex start");
      var states = new State[count];
      for (int index = 0; index < count; ++index) {
        string[] fields = records[index + 1].Split('|');
        if (fields.Length != 4) throw new System.FormatException("Invalid portable XML regex instruction");
        int[] epsilon = Integers(fields[0]), ranges = Integers(fields[1]);
        int next = int.Parse(fields[2], System.Globalization.CultureInfo.InvariantCulture), flags = int.Parse(fields[3], System.Globalization.CultureInfo.InvariantCulture);
        if ((ranges.Length & 1) != 0 || next < 0 || next >= count || flags < 0 || flags > (CONSUMES | INVERTED | DOT | ACCEPTING)) throw new System.FormatException("Invalid portable XML regex instruction");
        foreach (int target in epsilon) if (target < 0 || target >= count) throw new System.FormatException("Invalid portable XML regex edge");
        for (int range = 0; range < ranges.Length; range += 2) if (ranges[range] < 0 || ranges[range] > ranges[range + 1] || ranges[range + 1] > 0x10ffff) throw new System.FormatException("Invalid portable XML regex range");
        states[index] = new State(epsilon, ranges, next, flags);
      }
      return new Pattern(start, states);
    }
    // Mark states on insertion so nullable loops cannot overflow the fixed stack.
    private int Closure(int[] selected, int count, bool[] visited, int[] stack, Work work) {
      work.Spend(visited.Length); System.Array.Fill(visited, false); int pending = 0, kept = 0;
      for (int index = 0; index < count; ++index) { int target = selected[index]; work.Spend(1); if (!visited[target]) { visited[target] = true; stack[pending++] = target; } }
      while (pending != 0) {
        work.Spend(1); int current = stack[--pending]; State state = states[current];
        if ((state.Flags & (CONSUMES | ACCEPTING)) != 0) selected[kept++] = current;
        foreach (int target in state.Epsilon) { work.Spend(1); if (!visited[target]) { visited[target] = true; stack[pending++] = target; } }
      }
      return kept;
    }
    // Match Unicode scalars with fixed state arrays and deterministic operation bounds.
    internal bool Matches(string text, Work work) {
      int bytes = 0;
      for (int offset = 0; offset < text.Length;) {
        work.Spend(1); int scalar = Scalar(text, ref offset);
        int width = scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
        if (width > MAX_INPUT_BYTES - bytes) throw new LimitFailure("Portable XML regex input limit exceeded"); bytes += width;
      }
      var visited = new bool[states.Length]; var active = new int[states.Length]; var next = new int[states.Length]; var stack = new int[states.Length];
      active[0] = start; int count = Closure(active, 1, visited, stack, work);
      for (int offset = 0; offset < text.Length;) {
        work.Spend(1); int scalar = Scalar(text, ref offset); int following = 0;
        for (int index = 0; index < count; ++index) {
          work.Spend(1); State state = states[active[index]]; if ((state.Flags & CONSUMES) == 0) continue;
          bool matched = false;
          if ((state.Flags & DOT) != 0) matched = scalar != '\n' && scalar != '\r';
          else {
            for (int range = 0; range < state.Ranges.Length; range += 2) { work.Spend(1); matched |= scalar >= state.Ranges[range] && scalar <= state.Ranges[range + 1]; }
            if ((state.Flags & INVERTED) != 0) matched = !matched;
          }
          if (matched) next[following++] = state.Next;
        }
        count = Closure(next, following, visited, stack, work); int[] previous = active; active = next; next = previous;
      }
      for (int index = 0; index < count; ++index) if ((states[active[index]].Flags & ACCEPTING) != 0) return true;
      return false;
    }
  }
}
)xmlscalar"
