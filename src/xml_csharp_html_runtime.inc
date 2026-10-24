// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"htmlcs(
// Immutable bounds for approved ordinary HTML, independent of XML providers and native wire policies.
public sealed class HtmlLimits {
  public int MaxInputBytes {get;}public int MaxOutputBytes {get;}public int MaxEvents {get;}public int MaxDepth {get;}public int MaxAttributesPerElement {get;}public int MaxTextBytes {get;}
  // Derive default admission from the shared approved HTML contract.
  public HtmlLimits():this(SrlHtmlMaxInputBytes,SrlHtmlMaxOutputBytes,SrlHtmlMaxEvents,SrlHtmlMaxDepth,SrlHtmlMaxAttributesPerElement,SrlHtmlMaxTextBytes){}
  // Reject negative policies before parsing or traversing authored event storage.
  public HtmlLimits(int input,int output,int events,int depth,int attributes,int text){if(input<0||output<0||events<0||depth<0||attributes<0||text<0)throw new System.ArgumentOutOfRangeException(nameof(input));MaxInputBytes=input;MaxOutputBytes=output;MaxEvents=events;MaxDepth=depth;MaxAttributesPerElement=attributes;MaxTextBytes=text;}
}
// Own one lowercase attribute; hidden is represented by Boolean presence.
public sealed class HtmlAttribute {
  public string Name {get;}public string Value {get;}public bool Boolean {get;}
  // Retain immutable application text without applying URL or application security policy.
  public HtmlAttribute(string name,string value="",bool boolean=false){Name=name??throw new System.ArgumentNullException(nameof(name));Value=value??throw new System.ArgumentNullException(nameof(value));Boolean=boolean;}
}
// Own semantic fragment/start/text/end events without XML namespaces or xsi metadata.
public sealed class HtmlEvent {
  public string Kind {get;}public string Name {get;}public string Text {get;}public System.Collections.Generic.IReadOnlyList<HtmlAttribute> Attributes {get;}
  // Copy the complete attribute sequence and retain immutable strings.
  public HtmlEvent(string kind,string name="",System.Collections.Generic.IEnumerable<HtmlAttribute>? attributes=null,string text=""){Kind=kind??throw new System.ArgumentNullException(nameof(kind));Name=name??throw new System.ArgumentNullException(nameof(name));Text=text??throw new System.ArgumentNullException(nameof(text));Attributes=System.Array.AsReadOnly(new System.Collections.Generic.List<HtmlAttribute>(attributes??System.Array.Empty<HtmlAttribute>()).ToArray());}
}
// Parse explicitly balanced approved HTML only; this contract supplies neither browser recovery nor URL sanitization.
public static class Html {
  private static readonly System.Collections.Generic.HashSet<string> Tags=new("div p span b i strong em a ul ol li section article h1 h2 h3 h4 h5 h6 br hr img small s u code blockquote nav sub sup".Split(' '),System.StringComparer.Ordinal);
  private static readonly System.Collections.Generic.HashSet<string> Voids=new(new[]{"br","hr","img"},System.StringComparer.Ordinal),Phrasing=new("span b i strong em a br img small s u code sub sup".Split(' '),System.StringComparer.Ordinal);
  private static readonly System.Text.UTF8Encoding Encoding=new(false,true);
  private const string Space=" \t\r\n\f";
  // Describe exact supported syntax without advertising general HTML5 parsing.
  public static string SupportedSubset(){return "UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; br/hr/img void tags; ASCII case-insensitive names; quoted/unquoted and hidden Boolean attributes; amp/lt/gt/quot/apos/nbsp and numeric references; comments omitted; no doctype, implied closes, raw text, foreign content or browser recovery";}
  // Reject malformed Unicode and unsupported controls before the encoder can substitute values.
  private static string Text(string value){if(value==null)throw new System.FormatException("HTML requires text");for(int offset=0;offset<value.Length;){int scalar=value[offset++];if(scalar>=0xd800&&scalar<=0xdbff){if(offset>=value.Length||!char.IsLowSurrogate(value[offset]))throw new System.FormatException("Unsupported HTML Unicode scalar");scalar=char.ConvertToUtf32((char)scalar,value[offset++]);}else if(scalar>=0xdc00&&scalar<=0xdfff)throw new System.FormatException("Unsupported HTML Unicode scalar");if(scalar==0||scalar>=0x7f&&scalar<=0x9f||scalar<32&&scalar!=9&&scalar!=10&&scalar!=13&&scalar!=12)throw new System.FormatException("Unsupported HTML character");}return value;}
  // Count UTF8 without allocating another retained encoded buffer.
  private static int Bytes(string value){return Encoding.GetByteCount(value);}
  // Decode approved terminated references; source CR normalization does not alter numeric CR references.
  private static string Decode(string input){
    string value=input.Replace("\r\n","\n",System.StringComparison.Ordinal).Replace('\r','\n');var result=new System.Text.StringBuilder();
    for(int position=0;position<value.Length;){if(value[position]!='&'){result.Append(value[position++]);continue;}int end=value.IndexOf(';',position+1);if(end<0||end-position>12)throw new System.FormatException("HTML references require supported terminated spelling");string entity=value.Substring(position+1,end-position-1);string? decoded=entity switch{"amp"=>"&","lt"=>"<","gt"=>">","quot"=>"\"","apos"=>"'","nbsp"=>"\u00a0",_=>null};
      if(decoded==null){if(!entity.StartsWith("#",System.StringComparison.Ordinal))throw new System.FormatException("Unsupported HTML entity");bool hex=entity.Length>1&&(entity[1]=='x'||entity[1]=='X');int start=hex?2:1;if(start==entity.Length)throw new System.FormatException("Empty HTML numeric reference");int scalar=0,radix=hex?16:10;for(int index=start;index<entity.Length;++index){char item=entity[index];int digit=item>='0'&&item<='9'?item-'0':hex&&item>='a'&&item<='f'?item-'a'+10:hex&&item>='A'&&item<='F'?item-'A'+10:-1;if(digit<0||scalar>(0x10ffff-digit)/radix)throw new System.FormatException("Unsupported HTML numeric reference");scalar=scalar*radix+digit;}if(scalar>=0xd800&&scalar<=0xdfff)throw new System.FormatException("Unsupported HTML character reference");decoded=char.ConvertFromUtf32(scalar);}
      result.Append(Text(decoded));position=end+1;
    }return Text(result.ToString());
  }
  // Reject relationships that an independent HTML parser would implicitly close or rearrange.
  private static void Child(System.Collections.Generic.IReadOnlyList<string> open,string child){if(!Tags.Contains(child))throw new System.FormatException("HTML tag outside approved subset");string parent=open.Count==0?"":open[^1];if(child=="li"&&parent!="ul"&&parent!="ol")throw new System.FormatException("HTML list item requires direct list parent");if(Voids.Contains(parent)||(parent=="ul"||parent=="ol")&&child!="li"||(Phrasing.Contains(parent)||parent=="p"||parent.Length==2&&parent[0]=='h'&&parent[1]>='1'&&parent[1]<='6')&&!Phrasing.Contains(child))throw new System.FormatException("HTML child requires unsupported browser recovery");if(child=="a")foreach(string ancestor in open)if(ancestor=="a")throw new System.FormatException("Nested HTML anchor outside approved subset");}
  // Check approved attribute identities without interpreting application values or URL schemes.
  private static bool Attribute(string tag,string key){return key is "id" or "class" or "title" or "lang" or "dir" or "hidden"||tag=="a"&&(key is "href" or "rel")||tag=="img"&&(key is "src" or "alt" or "width" or "height");}
  private sealed class Output {
    private readonly HtmlLimits limits;internal readonly System.Text.StringBuilder Result=new();private int count;
    // Own independent output state; errors discard the complete unpublished candidate.
    internal Output(HtmlLimits limits){this.limits=limits;}
    // Charge encoded length before appending another fragment to owned storage.
    internal void Append(string value){int size=Bytes(value);if(size>limits.MaxOutputBytes-count)throw new System.FormatException("HTML output byte limit");count+=size;Result.Append(value);}
    // Escape syntax and raw CR in batches that never split a UTF16 surrogate pair.
    internal void Escape(string value,bool attribute){Text(value);var batch=new System.Text.StringBuilder();foreach(char item in value){switch(item){case '&':batch.Append("&amp;");break;case '<':batch.Append("&lt;");break;case '>':batch.Append("&gt;");break;case '\r':batch.Append("&#13;");break;case '"':batch.Append(attribute?"&quot;":"\"");break;default:batch.Append(item);break;}if(batch.Length>=4096&&!char.IsHighSurrogate(item)){Append(batch.ToString());batch.Clear();}}if(batch.Length!=0)Append(batch.ToString());}
  }
  // Validate a complete owning event trace and publish deterministic ordinary HTML UTF8 only on success.
  public static byte[] WriteFragment(System.Collections.Generic.IReadOnlyList<HtmlEvent> events,HtmlLimits? policy=null){
    var limits=policy??new HtmlLimits();if(events==null||events.Count>limits.MaxEvents||events.Count<2||events[0].Kind!="start_fragment"||events[^1].Kind!="end_fragment")throw new System.FormatException("HTML requires bounded explicit fragment boundaries");var output=new Output(limits);var open=new System.Collections.Generic.List<string>();int textBytes=0;
    for(int index=0;index<events.Count;++index){var entry=events[index];if(entry==null)throw new System.FormatException("Null HTML event");switch(entry.Kind){
      case "start_fragment":case "end_fragment":if(index!=0&&index!=events.Count-1||open.Count!=0||entry.Name.Length!=0||entry.Attributes.Count!=0||entry.Text.Length!=0)throw new System.FormatException("Malformed HTML fragment boundary");break;
      case "start_element":Child(open,entry.Name);if(entry.Text.Length!=0||open.Count>=limits.MaxDepth||entry.Attributes.Count>limits.MaxAttributesPerElement)throw new System.FormatException("HTML element payload/depth/attribute limit");output.Append("<"+entry.Name);var names=new System.Collections.Generic.HashSet<string>(System.StringComparer.Ordinal);foreach(var item in entry.Attributes){string key=item.Name;if(!Attribute(entry.Name,key)||!names.Add(key)||item.Boolean!=(key=="hidden")||item.Boolean&&item.Value.Length!=0)throw new System.FormatException("Unsupported or duplicate HTML attribute");Text(item.Value);output.Append(" "+key);if(!item.Boolean){output.Append("=\"");output.Escape(item.Value,true);output.Append("\"");}}output.Append(">");open.Add(entry.Name);break;
      case "end_element":if(open.Count==0||open[^1]!=entry.Name||entry.Attributes.Count!=0||entry.Text.Length!=0)throw new System.FormatException("Unbalanced HTML close");if(!Voids.Contains(entry.Name))output.Append("</"+entry.Name+">");open.RemoveAt(open.Count-1);break;
      case "text":int count=Bytes(Text(entry.Text));string parent=open.Count==0?"":open[^1];if(entry.Name.Length!=0||entry.Attributes.Count!=0||Voids.Contains(parent)||count>limits.MaxTextBytes-textBytes)throw new System.FormatException("Invalid HTML text event or cumulative byte limit");if(parent=="ul"||parent=="ol")foreach(char item in entry.Text)if(Space.IndexOf(item)<0)throw new System.FormatException("HTML list text requires browser recovery");textBytes+=count;output.Escape(entry.Text,false);break;
      default:throw new System.FormatException("Unknown HTML event kind");
    }}return Encoding.GetBytes(output.Result.ToString());
  }
)htmlcs"
R"htmlcs(
  private sealed class Parser {
    private readonly string source;private readonly HtmlLimits limits;private readonly System.Collections.Generic.List<HtmlEvent> events=new();private readonly System.Collections.Generic.List<string> open=new();private int position,textBytes;
    // Borrow bounded fatal-decoded source and own only the candidate event trace.
    internal Parser(string source,HtmlLimits limits){this.source=source;this.limits=limits;}
    // Admit every event and cumulative text run before retaining the native trace payload.
    private void Add(HtmlEvent entry){if(events.Count>=limits.MaxEvents)throw new System.FormatException("HTML event count limit");if(entry.Kind=="text"){int count=Bytes(entry.Text);if(count>limits.MaxTextBytes-textBytes)throw new System.FormatException("HTML text byte limit");textBytes+=count;}events.Add(entry);}
    // Skip exactly the shared HTML whitespace set, including form feed.
    private int SkipSpace(int offset){while(offset<source.Length&&Space.IndexOf(source[offset])>=0)++offset;return offset;}
    // Scan ASCII tag/attribute names without locale-sensitive Unicode folding.
    private string Name(int offset){int end=offset;while(end<source.Length){char value=source[end];if(!(value>='A'&&value<='Z'||value>='a'&&value<='z'||value>='0'&&value<='9'||value=='-'))break;++end;}if(end==offset)throw new System.FormatException("Unsupported HTML name syntax");return source.Substring(offset,end-offset);}
    // Observe one source character without indexing truncated input.
    private char Current(){return position<source.Length?source[position]:'\0';}
    // Parse explicit nesting and require checked balanced EOF before returning an owning candidate.
    internal System.Collections.Generic.IReadOnlyList<HtmlEvent> Parse(){
      Add(new HtmlEvent("start_fragment"));while(position<source.Length){
        if(Current()!='<'){int end=source.IndexOf('<',position);if(end<0)end=source.Length;Add(new HtmlEvent("text",text:Decode(source.Substring(position,end-position))));position=end;continue;}
        if(source.Length-position>=4&&string.CompareOrdinal(source,position,"<!--",0,4)==0){int end=source.IndexOf("-->",position+4,System.StringComparison.Ordinal);if(end<0||source.Substring(position+4,end-position-4).Contains("--",System.StringComparison.Ordinal))throw new System.FormatException("Malformed HTML comment");position=end+3;continue;}
        ++position;bool closing=Current()=='/';if(closing)++position;string original=Name(position),tag=original.ToLowerInvariant();position+=original.Length;
        if(closing){position=SkipSpace(position);if(Current()!='>'||Voids.Contains(tag)||open.Count==0||open[^1]!=tag)throw new System.FormatException("HTML close requires matching explicit nonvoid start");++position;open.RemoveAt(open.Count-1);Add(new HtmlEvent("end_element",tag));continue;}
        Child(open,tag);if(open.Count>=limits.MaxDepth)throw new System.FormatException("HTML nesting limit");var attributes=new System.Collections.Generic.List<HtmlAttribute>();bool selfClosing=false;
        while(true){int separator=position;position=SkipSpace(position);if(position>=source.Length)throw new System.FormatException("Truncated HTML start tag");if(Current()=='>'){++position;break;}if(source.Length-position>=2&&string.CompareOrdinal(source,position,"/>",0,2)==0){selfClosing=true;position+=2;break;}if(separator==position||attributes.Count>=limits.MaxAttributesPerElement)throw new System.FormatException("HTML attributes require separation and bounded cardinality");string originalKey=Name(position),key=originalKey.ToLowerInvariant();position+=originalKey.Length;int keyEnd=position;position=SkipSpace(position);bool assigned=Current()=='=';string value="";
          if(assigned){position=SkipSpace(position+1);if(position>=source.Length)throw new System.FormatException("Truncated HTML attribute");char quote=Current()=='\"'||Current()=='\''?source[position++]:'\0';int end=quote!='\0'?source.IndexOf(quote,position):position;if(quote!='\0'){if(end<0)throw new System.FormatException("Unterminated HTML attribute");}else{while(end<source.Length&&Space.IndexOf(source[end])<0&&source[end]!='>'){if("\"'< =`".IndexOf(source[end])>=0)throw new System.FormatException("Invalid unquoted HTML attribute");++end;}if(end==position)throw new System.FormatException("Empty unquoted HTML attribute");}value=Decode(source.Substring(position,end-position));position=end+(quote!='\0'?1:0);}else position=keyEnd;
          if(key=="hidden"){if(assigned&&value.Length!=0&&!value.Equals("hidden",System.StringComparison.OrdinalIgnoreCase))throw new System.FormatException("HTML hidden requires Boolean spelling");value="";}else if(!assigned)throw new System.FormatException("Only hidden may omit HTML attribute value");attributes.Add(new HtmlAttribute(key,value,key=="hidden"));
        }
        if(selfClosing&&!Voids.Contains(tag))throw new System.FormatException("Self-closing nonvoid HTML requires browser recovery");Add(new HtmlEvent("start_element",tag,attributes));if(Voids.Contains(tag))Add(new HtmlEvent("end_element",tag));else open.Add(tag);
      }if(open.Count!=0)throw new System.FormatException("Unclosed HTML fragment");Add(new HtmlEvent("end_fragment"));WriteFragment(events,limits);return events.AsReadOnly();
    }
  }
  // Fatal-decode ordinary UTF8 HTML without XML parsing, publishing only a complete approved trace.
  public static System.Collections.Generic.IReadOnlyList<HtmlEvent> ParseFragment(byte[] input,HtmlLimits? policy=null){var limits=policy??new HtmlLimits();if(input==null||input.Length>limits.MaxInputBytes)throw new System.FormatException("HTML input byte limit");try{return new Parser(Text(Encoding.GetString(input)),limits).Parse();}catch(System.Text.DecoderFallbackException failure){throw new System.FormatException("Invalid HTML UTF8",failure);}}
}
)htmlcs"
