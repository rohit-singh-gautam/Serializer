// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlextension(
  // Bound foreign roots and retained semantic payload independently from complete-document limits.
  public sealed class XmlExtensionLimits {
    public int MaxSlots { get; } public int MaxItems { get; } public long MaxPreservedBytes { get; }
    // Preserve exact zero admission and reject negative policies before allocating counters or payload.
    public XmlExtensionLimits(int maxSlots=SrlXmlExtensionMaxSlots,int maxItems=SrlXmlExtensionMaxItems,long maxPreservedBytes=SrlXmlExtensionMaxPreservedBytes){if(maxSlots<0||maxItems<0||maxPreservedBytes<0)throw new System.ArgumentOutOfRangeException(nameof(maxSlots));MaxSlots=maxSlots;MaxItems=maxItems;MaxPreservedBytes=maxPreservedBytes;}
  }
  // Semantic vocabulary understanding is independent from permission to retain opaque XML payload.
  public sealed class XmlExtensionOptions {
    public System.Collections.Generic.IReadOnlyList<string> UnderstoodNamespaces { get; } public string RequiredPrefixes { get; } public XmlExtensionLimits Limits { get; }
    // Own a bounded immutable copy rather than retaining the caller's mutable vocabulary inventory.
    public XmlExtensionOptions(System.Collections.Generic.IReadOnlyList<string>? understoodNamespaces=null,string requiredPrefixes="",XmlExtensionLimits? limits=null){
      Limits=limits??new XmlExtensionLimits();understoodNamespaces??=System.Array.Empty<string>();if(understoodNamespaces.Count>Limits.MaxItems)throw new System.FormatException("XML capability item limit");
      long bytes=SrlXmlExtensionSize(requiredPrefixes);if(bytes>SrlXmlExtensionMaxPreservedBytes)throw new System.FormatException("XML capability metadata byte limit");for(int index=0;index<understoodNamespaces.Count;++index){long size=SrlXmlExtensionSize(understoodNamespaces[index]);if(size>SrlXmlExtensionMaxPreservedBytes-bytes)throw new System.FormatException("XML capability metadata byte limit");bytes+=size;}
      var copy=new string[understoodNamespaces.Count];for(int index=0;index<copy.Length;++index)copy[index]=understoodNamespaces[index];UnderstoodNamespaces=System.Array.AsReadOnly(copy);RequiredPrefixes=requiredPrefixes;
    }
  }
  // Measure exact UTF8 storage and reject non-XML text before retaining it in extension state.
  private static long SrlXmlExtensionSize(string text){try{System.Xml.XmlConvert.VerifyXmlChars(text);return SrlUtf8.GetByteCount(text);}catch(System.Exception failure)when(failure is System.Xml.XmlException||failure is System.Text.EncoderFallbackException){throw new System.FormatException("Invalid XML extension character",failure);}}
  // Apply the platform's exact XML1.0 NCName contract to prefixes and opaque local names.
  private static bool SrlXmlExtensionNcName(string text){try{System.Xml.XmlConvert.VerifyNCName(text);return true;}catch(System.Xml.XmlException){return false;}}
  // Split a trusted expanded-name representation while revalidating any edited retained metadata.
  private static (string Uri,string Local) SrlXmlExtensionName(string name){int end=name.StartsWith("{",System.StringComparison.Ordinal)?name.IndexOf('}'):-1;string uri=end<0?"":name.Substring(1,end-1),local=end<0?name:name.Substring(end+1);if(!SrlXmlExtensionNcName(local)||end>=0&&uri.Length==0||uri=="http://www.w3.org/2000/xmlns/")throw new System.FormatException("Invalid XML extension expanded name");SrlXmlExtensionSize(uri);return(uri,local);}
  // Compiler-owned exact URI permissions never interpret the unknown vocabulary's lexical values.
  internal sealed class SrlXmlExtensionSlot {
    internal readonly string Id;internal readonly bool Attribute;internal readonly int Position,Policy,Minimum,Maximum;internal readonly string[] Namespaces;
    // Negative maximum denotes an unbounded explicit slot; all metadata is validated by the compiler.
    internal SrlXmlExtensionSlot(string id,bool attribute,int position,string[] namespaces,int policy,int minimum,int maximum){Id=id;Attribute=attribute;Position=position;Namespaces=namespaces;Policy=policy;Minimum=minimum;Maximum=maximum;}
  }
  // Share immutable expanded permissions independently from each native owner's counters.
  internal sealed class SrlXmlExtensionContract {
    internal readonly string Core;internal readonly string[] Known,Required;internal readonly SrlXmlExtensionSlot[] Slots;
    internal readonly System.Collections.Generic.HashSet<string> KnownNames;
    internal readonly System.Collections.Generic.Dictionary<string,int> AttributeSlots=new(System.StringComparer.Ordinal);
    internal readonly System.Collections.Generic.Dictionary<(string,int),int> ChildSlots=new();
    // Bind compiler-checked contract metadata without exposing a mutable public representation.
    internal SrlXmlExtensionContract(string core,string[] known,string[] required,SrlXmlExtensionSlot[] slots){Core=core;Known=known;Required=required;Slots=slots;KnownNames=new(known,System.StringComparer.Ordinal);for(int index=0;index<slots.Length;++index){var slot=slots[index];foreach(string uri in slot.Namespaces){if(slot.Attribute)AttributeSlots.Add(uri,index);else ChildSlots.Add((uri,slot.Position),index);}}}
  }
  // Own only explicitly preserved semantic foreign XML, including every in-scope QName binding.
  internal sealed class SrlXmlForeignNode {
    internal readonly string Name;internal readonly System.Collections.Generic.Dictionary<string,string> Attributes,Scope;internal readonly System.Collections.Generic.List<object> Content=new();
    // Take private admitted maps; known native model nodes are never copied into this representation.
    internal SrlXmlForeignNode(string name,System.Collections.Generic.Dictionary<string,string> attributes,System.Collections.Generic.Dictionary<string,string> scope){Name=name;Attributes=attributes;Scope=scope;}
  }
  // Position one opaque root around its original native field and repeated-item boundary.
  internal sealed class SrlXmlForeignChild {
    internal readonly int Position,Item;internal readonly SrlXmlForeignNode Node;
    // Retain a completely captured node only after its owning content cell was admitted.
    internal SrlXmlForeignChild(int position,int item,SrlXmlForeignNode node){Position=position;Item=item;Node=node;}
  }
  // XML-only state stays outside native fields and every ordinary wire codec.
  internal sealed class SrlXmlPreserved {
    internal readonly System.Collections.Generic.Dictionary<string,string> Scope,Attributes=new();internal readonly System.Collections.Generic.List<SrlXmlForeignChild> Children=new();
    // Take a complete private owner scope after logical storage admission.
    internal SrlXmlPreserved(System.Collections.Generic.Dictionary<string,string> scope){Scope=scope;}
  }
  // Enforce exact owner-local extension policies while retaining only explicitly preserved payload.
  internal sealed class SrlXmlExtensions {
    private const long OwnerBytes=64,NodeBytes=64,ScopeEntryBytes=64,AttributeBytes=48,ContentEntryBytes=24;
    private readonly SrlXmlExtensionContract meta;private readonly System.Collections.Generic.Dictionary<string,string> scope;private readonly XmlExtensionOptions options;private readonly int[] counts;
    private readonly Limits.IOperation? operation;private int items,cursor;private long bytes;internal SrlXmlPreserved? State;
    // Require vocabulary understanding before constructing a candidate or opening an output head.
    internal SrlXmlExtensions(SrlXmlExtensionContract meta,System.Collections.Generic.Dictionary<string,string> scope,XmlExtensionOptions options,SrlXmlPreserved? state,Limits.IOperation? operation=null){this.operation=operation;
      this.meta=meta;this.scope=scope;this.options=options;State=state;var bounds=options.Limits;if(meta.Slots.Length>bounds.MaxSlots||meta.Known.Length>bounds.MaxItems||meta.Required.Length>bounds.MaxItems)throw SrlXmlBudget.Limit("XML extension slot limit",operation);counts=new int[meta.Slots.Length];
      var understood=new System.Collections.Generic.HashSet<string>(options.UnderstoodNamespaces,System.StringComparer.Ordinal);foreach(string uri in meta.Required)if(uri!=meta.Core&&!understood.Contains(uri))throw new System.FormatException("Required XML capability is not understood");
      foreach(string prefix in SrlXmlRule.Tokens(options.RequiredPrefixes,bounds.MaxItems,operation)){if(!SrlXmlExtensionNcName(prefix)||prefix=="xmlns"||!scope.TryGetValue(prefix,out string? uri)||uri.Length==0)throw new System.FormatException("Unbound required XML capability prefix");if(uri!=meta.Core&&!understood.Contains(uri))throw new System.FormatException("Required XML capability is not understood");}
      if(state!=null)Charge(Add(OwnerBytes,ScopeBytes(scope)));
    }
    // Bound partial measurement before addition, independently from allocator capacity overhead.
    private long Add(long first,long second){if(first<0||second<0||second>options.Limits.MaxPreservedBytes-first)throw SrlXmlBudget.Limit("XML extension preserved-byte limit",operation);return first+second;}
    // Admit logical bytes before retaining another owning map, node, string or content cell.
    internal void Charge(long count){bytes=Add(bytes,count);}
    // Validate full namespace context, including unused prefixes needed by opaque lexical QName values.
    private long ScopeBytes(System.Collections.Generic.Dictionary<string,string> bindings){long size=0;foreach(var entry in bindings){string prefix=entry.Key,uri=entry.Value;if(!(prefix.Length==0||SrlXmlExtensionNcName(prefix))||prefix=="xmlns"||(prefix=="xml")!=(uri=="http://www.w3.org/XML/1998/namespace")||prefix.Length!=0&&uri.Length==0||uri=="http://www.w3.org/2000/xmlns/")throw new System.FormatException("Invalid preserved XML namespace binding");size=Add(size,Add(ScopeEntryBytes,Add(SrlXmlExtensionSize(prefix),SrlXmlExtensionSize(uri))));}return size;}
    // Measure exact retained attribute identity and text without changing its lexical vocabulary meaning.
    private long AttributeSize(string name,string value){SrlXmlExtensionName(name);return Add(AttributeBytes,Add(SrlXmlExtensionSize(name),SrlXmlExtensionSize(value)));}
    // Keep unqualified, core, known and reserved model metadata closed before consulting foreign slots.
    internal int Slot(string name,bool attribute,int position){string uri=SrlXmlExtensionName(name).Uri;if(uri.Length==0||uri==meta.Core||uri=="urn:serializer:xml:model"||meta.KnownNames.Contains(name))return -1;int index;return (attribute?meta.AttributeSlots.TryGetValue(uri,out index):meta.ChildSlots.TryGetValue((uri,position),out index))?index:-1;}
    // Enforce policy, total root items and per-slot occurrences before capturing or skipping a foreign root.
    private bool Admit(string name,bool attribute,int position){int index=Slot(name,attribute,position);if(index<0)throw new System.FormatException("XML extension has no permitted slot");var selected=meta.Slots[index];if(selected.Policy==SrlXmlExtensionReject)throw new System.FormatException("XML extension slot rejects foreign vocabulary");if(items>=options.Limits.MaxItems)throw SrlXmlBudget.Limit("XML extension item/occurrence limit",operation);if(selected.Maximum>=0&&counts[index]>=selected.Maximum)throw new System.FormatException("XML extension item/occurrence limit");++items;++counts[index];return selected.Policy==SrlXmlExtensionPreserve;}
    // Skip-only traversal allocates no retained XML sidecar state.
    private SrlXmlPreserved Storage(){if(State==null){Charge(Add(OwnerBytes,ScopeBytes(scope)));State=new SrlXmlPreserved(new System.Collections.Generic.Dictionary<string,string>(scope));}return State;}
    // Preserve an expanded foreign attribute only after admitting its exact slot and logical bytes.
    internal void Attribute(string name,string value){if(Admit(name,true,0)){Charge(AttributeSize(name,value));Storage().Attributes.Add(name,value);}}
    // Revalidate one captured or edited semantic node before retaining or replaying its storage.
    private void Measure(string name,System.Collections.Generic.Dictionary<string,string> attributes,System.Collections.Generic.Dictionary<string,string> bindings){SrlXmlExtensionName(name);long size=Add(NodeBytes,Add(SrlXmlExtensionSize(name),ScopeBytes(bindings)));foreach(var attribute in attributes)size=Add(size,AttributeSize(attribute.Key,attribute.Value));Charge(size);}
)xmlextension"
R"xmlextension(
    // Capture only explicit preserve roots; skipped subtrees allocate no namespace or payload candidates.
    private SrlXmlForeignNode? Foreign(SrlXmlInput input,bool preserve){
      SrlXmlForeignNode? node=null;System.Text.StringBuilder? pending=preserve?new():null;
      if(preserve){string name=input.Name;var bindings=input.NamespaceScope();long size=Add(NodeBytes,Add(SrlXmlExtensionSize(name),ScopeBytes(bindings)));var reader=input.ExtensionReader;
        if(reader.MoveToFirstAttribute()){do{if(reader.NamespaceURI!="http://www.w3.org/2000/xmlns/")size=Add(size,AttributeSize(SrlXmlName(reader.NamespaceURI,reader.LocalName),reader.Value));}while(reader.MoveToNextAttribute());reader.MoveToElement();}Charge(size);
        var attributes=new System.Collections.Generic.Dictionary<string,string>();if(reader.MoveToFirstAttribute()){do{if(reader.NamespaceURI!="http://www.w3.org/2000/xmlns/")attributes.Add(SrlXmlName(reader.NamespaceURI,reader.LocalName),reader.Value);}while(reader.MoveToNextAttribute());reader.MoveToElement();}node=new SrlXmlForeignNode(name,attributes,bindings);
      }
      input.Advance();while(input.Kind!='e'){
        if(input.Kind=='t'){if(preserve){string text=input.ExtensionReader.Value;Charge(SrlXmlExtensionSize(text));pending!.Append(text);}input.Advance();}
        else if(input.Kind=='s'){if(preserve&&pending!.Length!=0){Charge(ContentEntryBytes);node!.Content.Add(pending.ToString());pending.Clear();}var child=Foreign(input,preserve);if(preserve){Charge(ContentEntryBytes);node!.Content.Add(child!);}}
        else throw new System.FormatException("Incomplete foreign XML subtree");
      }
      if(preserve&&pending!.Length!=0){Charge(ContentEntryBytes);node!.Content.Add(pending.ToString());}input.Advance();return node;
    }
    // Consume only roots admitted at this exact known field and repeated-item boundary.
    internal void ReadAt(SrlXmlInput input,int position,int item){input.SkipSpace();while(input.Kind=='s'&&Slot(input.Name,false,position)>=0){bool preserve=Admit(input.Name,false,position);var node=Foreign(input,preserve);if(node!=null){Charge(ContentEntryBytes);Storage().Children.Add(new SrlXmlForeignChild(position,item,node));}input.SkipSpace();}}
    // Require every explicit slot minimum before publishing a fresh native owner.
    internal void Finish(){for(int index=0;index<counts.Length;++index)if(counts[index]<meta.Slots[index].Minimum)throw new System.FormatException("Required XML extension occurrence missing");}
    // Replay retained attributes only while the known owner's complete head is pending.
    internal void EmitAttributes(SrlXmlOutput output){if(State!=null)foreach(var entry in State.Attributes){if(!Admit(entry.Key,true,0))throw new System.FormatException("Retained XML attribute no longer has a preserve slot");Charge(AttributeSize(entry.Key,entry.Value));output.Attribute(entry.Key,entry.Value);}}
    // Replay bounded opaque semantic nodes while known native values continue to use direct typed traversal.
    private void EmitNode(SrlXmlOutput output,SrlXmlForeignNode node){Measure(node.Name,node.Attributes,node.Scope);output.Open(node.Name,node.Scope);foreach(var attribute in node.Attributes)output.Attribute(attribute.Key,attribute.Value);foreach(object content in node.Content){Charge(ContentEntryBytes);if(content is string text){Charge(SrlXmlExtensionSize(text));output.Text(text);}else if(content is SrlXmlForeignNode child)EmitNode(output,child);else throw new System.FormatException("Invalid retained XML extension content");}output.Close();}
    // Preserve unknown-root order around exact native item boundaries after known fields are edited.
    internal void EmitAt(SrlXmlOutput output,int position,int item){if(State==null)return;while(cursor<State.Children.Count){var child=State.Children[cursor];if(child.Position<position||child.Position==position&&child.Item<item)throw new System.FormatException("Preserved XML extension no longer fits edited field position");if(child.Position!=position||child.Item!=item)return;if(!Admit(child.Node.Name,false,position))throw new System.FormatException("Retained XML child no longer has a preserve slot");Charge(ContentEntryBytes);EmitNode(output,child.Node);++cursor;}}
    // Reject retained positions that disappeared when a native repeated collection was shortened or removed.
    internal void EndPosition(int position){if(State!=null&&cursor<State.Children.Count&&State.Children[cursor].Position==position)throw new System.FormatException("Preserved XML extension no longer fits edited field position");}
    // Require every retained root to replay and all slot minima to hold before publishing output bytes.
    internal void FinishOutput(){if(State!=null&&cursor!=State.Children.Count)throw new System.FormatException("Unreplayed XML extension position");Finish();}
  }
)xmlextension"
