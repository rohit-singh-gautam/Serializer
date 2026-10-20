// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"identitycs(
// Immutable limits for one owner-local identity index, separate from ordinary XML admission.
public sealed class XmlIdentityLimits {
  public int MaxConstraints { get; } public int MaxFields { get; } public int MaxDepth { get; }
  public int MaxSelectedNodes { get; } public int MaxTuples { get; } public int MaxTupleBytes { get; } public int MaxWorkUnits { get; }
  // Derive defaults from the compiler's shared XML identity contract.
  public XmlIdentityLimits() : this(SrlXmlIdentityMaxConstraints,SrlXmlIdentityMaxFields,SrlXmlIdentityMaxDepth,SrlXmlIdentityMaxSelectedNodes,SrlXmlIdentityMaxTuples,SrlXmlIdentityMaxTupleBytes,SrlXmlIdentityMaxWorkUnits) { }
  // Reject negative limits before parsing input or traversing native state; zero is exact.
  public XmlIdentityLimits(int constraints,int fields,int depth,int selected,int tuples,int bytes,int work) {
    if(constraints<0||fields<0||depth<0||selected<0||tuples<0||bytes<0||work<0)throw new System.ArgumentOutOfRangeException(nameof(constraints));
    MaxConstraints=constraints;MaxFields=fields;MaxDepth=depth;MaxSelectedNodes=selected;MaxTuples=tuples;MaxTupleBytes=bytes;MaxWorkUnits=work;
  }
}
internal sealed class SrlXmlIdentityPath {
  internal readonly string[] Steps;internal readonly bool Descendants;internal readonly string? Attribute;
  // Retain compiler-owned expanded paths without runtime XPath interpretation.
  internal SrlXmlIdentityPath(string[] steps,bool descendants,string? attribute){Steps=steps;Descendants=descendants;Attribute=attribute;}
}
internal sealed class SrlXmlIdentityField {
  internal readonly SrlXmlIdentityPath Path;internal readonly string Kind;internal readonly bool Nillable;private readonly SrlXmlRule rule;
  // Keep builtin scalar value spaces independent of native payload spellings.
  internal SrlXmlIdentityField(SrlXmlIdentityPath path,string kind,bool nillable){Path=path;Kind=kind;Nillable=nillable;rule=new SrlXmlRule(kind,kind=="string"?"preserve":kind=="normalizedString"?"replace":"collapse",0,new string[]{"","","",""},-1,-1,-1,System.Array.Empty<string>(),false,System.Array.Empty<SrlXmlRule.Pattern[]>(),"",System.Array.Empty<SrlXmlRule>(),null);}
  // Reuse prepared scalar normalization without exposing its private implementation type.
  internal string Check(string lexical){return rule.Check(lexical);}
}
internal sealed class SrlXmlIdentityConstraint {
  internal readonly string Kind;internal readonly SrlXmlIdentityPath Selector;internal readonly SrlXmlIdentityField[] Fields;internal readonly int Refer;internal readonly string Name;
  // Bind the checked owner-local target index and ordered tuple fields.
  internal SrlXmlIdentityConstraint(string kind,SrlXmlIdentityPath selector,SrlXmlIdentityField[] fields,int refer):this(kind,selector,fields,refer,kind){}
  // Preserve the schema identity name for shared package-budget diagnostics.
  internal SrlXmlIdentityConstraint(string kind,SrlXmlIdentityPath selector,SrlXmlIdentityField[] fields,int refer,string name){Kind=kind;Selector=selector;Fields=fields;Refer=refer;Name=name;}
}
internal sealed class SrlXmlIdentityScope {
  internal readonly SrlXmlIdentityConstraint[] Constraints;
  // Own prepared metadata only; each operation binds its actual owner element name.
  internal SrlXmlIdentityScope(SrlXmlIdentityConstraint[] constraints){Constraints=constraints;}
}
// Incremental bounded paths, open scalar captures and typed tuples; no element tree is retained.
internal sealed class SrlXmlIdentity {
  private readonly SrlXmlIdentityScope scope;private readonly XmlIdentityLimits limits;private readonly Limits.IOperation? operation;
  private readonly System.Collections.Generic.List<string> path=new();
  private readonly System.Collections.Generic.List<Candidate> active=new();
  private readonly System.Collections.Generic.List<System.Collections.Generic.HashSet<string>> tables=new();
  private int selected,tuples,tupleBytes;private long workUnits;internal bool Complete { get; private set; }private bool failed;
  private sealed class Capture {
    internal readonly int Depth;internal readonly bool Nil;internal readonly System.Text.StringBuilder Text=new();internal int Bytes;
    // Capture one selected scalar at its original namespace depth.
    internal Capture(int depth,bool nil){Depth=depth;Nil=nil;}
  }
  private sealed class Candidate {
    internal readonly int Constraint,Depth;internal readonly string?[] Values;internal readonly bool[] Seen;internal readonly Capture?[] Captures;internal int Bytes;
    // Allocate a bounded candidate only after selection admission succeeds.
    internal Candidate(int constraint,int depth,int fields){Constraint=constraint;Depth=depth;Values=new string?[fields];Seen=new bool[fields];Captures=new Capture?[fields];}
  }
  // Allocate checked static tables for this independently scoped operation.
  internal SrlXmlIdentity(SrlXmlIdentityScope scope,XmlIdentityLimits limits):this(scope,limits,null){}
  // An immutable owner context follows identity work beyond the original native field scope.
  internal SrlXmlIdentity(SrlXmlIdentityScope scope,XmlIdentityLimits limits,Limits.IOperation? operation){this.scope=scope;this.limits=limits;this.operation=operation;Allocate(128L+scope.Constraints.Length*64L);if(scope.Constraints.Length>limits.MaxConstraints)throw new System.FormatException("XML identity constraint limit");foreach(var item in scope.Constraints){if(item.Fields.Length==0||item.Fields.Length>limits.MaxFields)throw new System.FormatException("XML identity field limit");tables.Add(new(System.StringComparer.Ordinal));}}
  // Count UTF8 without allocating another encoded field buffer.
  private static int Bytes(string value){return SrlUtf8.GetByteCount(value);}
  // Charge before selector or tuple work; resource failure remains terminal.
  private void Allocate(long count){operation?.Consume(Limits.OperationResource.AllocationBytes,count);}
  // Preserve cumulative package accounting alongside local XML limits.
  private void Work(long count){operation?.Consume(Limits.OperationResource.WorkUnits,count);if(count>limits.MaxWorkUnits-workUnits)throw new System.FormatException("XML identity work limit");workUnits+=count;}
  // Compare compiled expanded paths directly, without suffix arrays or XPath evaluation.
  private bool Matches(int first,SrlXmlIdentityPath expected){int count=path.Count-first;Work((long)count+expected.Steps.Length+1);if(expected.Descendants){if(count<expected.Steps.Length)return false;first=path.Count-expected.Steps.Length;}else if(count!=expected.Steps.Length)return false;for(int index=0;index<expected.Steps.Length;++index){string actual=path[first+index],wanted=expected.Steps[index];Work((long)System.Math.Min(Bytes(actual),Bytes(wanted))+1);if(actual!=wanted)return false;}return true;}
  // Recognize the integer/decimal identity value-space family without floating conversion.
  private static bool Numeric(string kind){return kind is "decimal" or "integer" or "positiveInteger" or "negativeInteger" or "nonPositiveInteger" or "nonNegativeInteger" or "byte" or "short" or "int" or "long" or "unsignedByte" or "unsignedShort" or "unsignedInt" or "unsignedLong";}
  // Canonicalize arbitrarily long finite decimal digits without host decimal rounding.
  private static string Decimal(string value){bool negative=value.StartsWith("-",System.StringComparison.Ordinal);if(negative||value.StartsWith("+",System.StringComparison.Ordinal))value=value[1..];int point=value.IndexOf('.');string whole=(point<0?value:value[..point]).TrimStart('0'),fraction=point<0?"":value[(point+1)..].TrimEnd('0');if(whole.Length==0)whole="0";if(whole=="0"&&fraction.Length==0)negative=false;return (negative?"-":"")+whole+(fraction.Length==0?"":"."+fraction);}
  // Normalize exact typed values before equality, including namespace-expanded QName fields.
  private string Key(string lexical,SrlXmlIdentityField field,System.Func<string,string?> resolve){
    string kind=field.Kind;
    int size=Bytes(lexical);Allocate(64L+size*6L);if(size>limits.MaxTupleBytes)throw new System.FormatException("XML identity field-byte limit");Work((long)size+1);
    if(kind=="QName"){
      string value=SrlXmlTrim(lexical);string[] parts=value.Split(':');string prefix="",local=value;
      if(parts.Length==2&&parts[0].Length!=0&&parts[1].Length!=0){prefix=parts[0];local=parts[1];}else if(parts.Length!=1||value.Length==0)throw new System.FormatException("Invalid XML identity QName");
      if(local.Length==0)throw new System.FormatException("Empty XML identity QName");try{System.Xml.XmlConvert.VerifyNCName(local);if(prefix.Length!=0)System.Xml.XmlConvert.VerifyNCName(prefix);}catch(System.Xml.XmlException failure){throw new System.FormatException("Invalid XML identity QName",failure);}string? uri=resolve(prefix);if(prefix.Length!=0&&uri==null)throw new System.FormatException("Undeclared XML identity QName prefix");return "q"+(uri??"")+'\0'+local;
    }
    string normalized=field.Check(lexical);
    if(kind=="boolean")return SrlXmlBoolean(normalized)?"btrue":"bfalse";if(Numeric(kind))return "d"+Decimal(normalized);if(kind is "float" or "double")throw new System.FormatException("Floating XML identity fields are unsupported");return (kind=="anyURI"?"u":"s")+normalized;
  }
  // Bound partial canonical tuple bytes before retaining another field value.
  private void Store(Candidate candidate,int index,string value){int count=Bytes(value)-(value.StartsWith("q",System.StringComparison.Ordinal)?2:1);if(count>limits.MaxTupleBytes-candidate.Bytes)throw new System.FormatException("XML identity candidate tuple-byte limit");candidate.Bytes+=count;candidate.Values[index]=value;}
  // Observe a complete active start tag while QName attributes still own their namespace scope.
  internal void Start(string name,System.Collections.Generic.IReadOnlyDictionary<string,string> attributes,System.Func<string,string?> resolve){
    try{
      if(Complete||failed)throw new System.FormatException("XML identity observer inactive");Work((long)Bytes(name)+1);if(path.Count>=limits.MaxDepth)throw new System.FormatException("XML identity depth limit");foreach(var candidate in active)foreach(var capture in candidate.Captures)if(capture!=null)throw new System.FormatException("XML identity scalar has complex child content");Allocate(16L+name.Length*2L);path.Add(name);
      for(int index=0;index<scope.Constraints.Length;++index){var constraint=scope.Constraints[index];if(Matches(1,constraint.Selector)){if(selected>=limits.MaxSelectedNodes)throw new System.FormatException("XML identity selected-node limit");++selected;Allocate(96L+constraint.Fields.Length*24L);active.Add(new Candidate(index,path.Count,constraint.Fields.Length));}}
      foreach(var candidate in active){var constraint=scope.Constraints[candidate.Constraint];for(int index=0;index<constraint.Fields.Length;++index){var field=constraint.Fields[index];if(!Matches(candidate.Depth,field.Path)||field.Path.Attribute!=null&&!attributes.ContainsKey(field.Path.Attribute))continue;if(constraint.Kind=="key"&&field.Nillable)throw new System.FormatException("XSD key field selects an element declared nillable");if(candidate.Seen[index])throw new System.FormatException("XML identity field selects multiple values");candidate.Seen[index]=true;
        if(field.Path.Attribute!=null)Store(candidate,index,Key(attributes[field.Path.Attribute],field,resolve));
        else{string nil=SrlXmlTrim(attributes.TryGetValue("{http://www.w3.org/2001/XMLSchema-instance}nil",out string? marker)?marker:"false");if(nil!="true"&&nil!="1"&&nil!="false"&&nil!="0")throw new System.FormatException("Malformed XML identity nil");Allocate(64);candidate.Captures[index]=new Capture(path.Count,nil=="true"||nil=="1");}
      }}
    }catch{failed=true;throw;}
  }
  // Charge logical UTF8 text independent of parser chunk, entity or CDATA boundaries.
  internal void Text(string value){try{if(Complete||failed||path.Count==0)throw new System.FormatException("XML identity observer inactive");int count=Bytes(value);Work(count);foreach(var candidate in active)foreach(var capture in candidate.Captures)if(capture!=null){Work(count);if(capture.Nil&&value.Length!=0)throw new System.FormatException("Nil XML identity scalar has text");if(count>limits.MaxTupleBytes-capture.Bytes)throw new System.FormatException("XML identity field-byte limit");capture.Bytes+=count;Allocate(count*4L);capture.Text.Append(value);}}catch{failed=true;throw;}}
)identitycs"
R"identitycs(
  // Close captures before leaving their namespace scope and index only complete checked tuples.
  internal void End(System.Func<string,string?> resolve){
    try{
      if(Complete||failed||path.Count==0)throw new System.FormatException("Unbalanced XML identity end");Work(1);
      foreach(var candidate in active){var constraint=scope.Constraints[candidate.Constraint];for(int index=0;index<candidate.Captures.Length;++index){Work(1);var capture=candidate.Captures[index];if(capture!=null&&capture.Depth==path.Count){if(!capture.Nil)Store(candidate,index,Key(capture.Text.ToString(),constraint.Fields[index],resolve));candidate.Captures[index]=null;}}}
      for(int item=active.Count-1;item>=0;--item){var candidate=active[item];if(candidate.Depth!=path.Count)continue;active.RemoveAt(item);var constraint=scope.Constraints[candidate.Constraint];bool missing=false;foreach(var value in candidate.Values)if(value==null)missing=true;
        if(missing){if(constraint.Kind=="key")throw new System.FormatException("XSD key tuple is incomplete or nil");continue;}if(tuples>=limits.MaxTuples)throw new System.FormatException("XML identity tuple-count limit");if(candidate.Bytes>limits.MaxTupleBytes-tupleBytes)throw new System.FormatException("XML identity tuple-byte limit");++tuples;tupleBytes+=candidate.Bytes;Work((long)candidate.Bytes+1);
        if(operation!=null){using var owner=operation.EnterOwner();operation.SelectField(constraint.Name);operation.Consume(Limits.OperationResource.References,1);operation.Consume(Limits.OperationResource.AllocationBytes,128L+candidate.Bytes*4L+candidate.Values.Length*24L);}
        var tuple=new System.Text.StringBuilder();foreach(var value in candidate.Values)tuple.Append(value!.Length).Append(':').Append(value);bool added=tables[candidate.Constraint].Add(tuple.ToString());if(!added&&constraint.Kind!="keyref")throw new System.FormatException("Duplicate XSD key/unique tuple");
      }
      path.RemoveAt(path.Count-1);if(path.Count==0)Complete=true;
    }catch{failed=true;throw;}
  }
  // Resolve all forward references only after the complete owner-local target tables exist.
  internal void Finish(){if(failed||!Complete||path.Count!=0||active.Count!=0)throw new System.FormatException("Incomplete or failed XML identity scope");try{Work(1);for(int index=0;index<scope.Constraints.Length;++index){var constraint=scope.Constraints[index];if(constraint.Kind!="keyref")continue;var target=tables[constraint.Refer];foreach(string tuple in tables[index]){Work((long)Bytes(tuple)+1);if(!target.Contains(tuple))throw new System.FormatException("Unresolved XSD keyref tuple");}}}catch{failed=true;throw;}}
}
)identitycs"
