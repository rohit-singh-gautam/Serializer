// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"htmljava(
/** Immutable admission for ordinary HTML fragments, independent of XML providers and wire limits. */
public static final class HtmlLimits {
  public final int maxInputBytes,maxOutputBytes,maxEvents,maxDepth,maxAttributesPerElement,maxTextBytes;
  /** Derive defaults from the shared approved HTML contract. */
  public HtmlLimits(){this(SRL_HTML_MAX_INPUT_BYTES,SRL_HTML_MAX_OUTPUT_BYTES,SRL_HTML_MAX_EVENTS,SRL_HTML_MAX_DEPTH,SRL_HTML_MAX_ATTRIBUTES_PER_ELEMENT,SRL_HTML_MAX_TEXT_BYTES);}
  /** Reject negative limits before scanning input or retaining authored events. */
  public HtmlLimits(int input,int output,int events,int depth,int attributes,int text){if(input<0||output<0||events<0||depth<0||attributes<0||text<0)throw new IllegalArgumentException("Negative HTML limit");maxInputBytes=input;maxOutputBytes=output;maxEvents=events;maxDepth=depth;maxAttributesPerElement=attributes;maxTextBytes=text;}
}
/** One owning lowercase attribute; hidden uses explicit Boolean presence. */
public static final class HtmlAttribute {
  public final String name,value;public final boolean booleanValue;
  /** Retain immutable strings without applying URL or application security policy. */
  public HtmlAttribute(String name,String value,boolean booleanValue){this.name=java.util.Objects.requireNonNull(name);this.value=java.util.Objects.requireNonNull(value);this.booleanValue=booleanValue;}
}
/** Owning semantic fragment/start/text/end event, without XML namespaces or metadata. */
public static final class HtmlEvent {
  public final String kind,name,text;public final java.util.List<HtmlAttribute> attributes;
  /** Construct an explicit fragment boundary. */
  public HtmlEvent(String kind){this(kind,"",java.util.List.of(),"");}
  /** Construct a payload-free element boundary. */
  public HtmlEvent(String kind,String name){this(kind,name,java.util.List.of(),"");}
  /** Copy the complete attribute sequence while retaining immutable string values. */
  public HtmlEvent(String kind,String name,java.util.List<HtmlAttribute> attributes,String text){this.kind=java.util.Objects.requireNonNull(kind);this.name=java.util.Objects.requireNonNull(name);this.attributes=java.util.List.copyOf(attributes);this.text=java.util.Objects.requireNonNull(text);}
}
/** Explicitly balanced approved HTML only; browser recovery and URL sanitization are outside this contract. */
public static final class Html {
  private static final java.util.Set<String> TAGS=java.util.Set.of("div","p","span","b","i","strong","em","a","ul","ol","li","section","article","h1","h2","h3","h4","h5","h6","br","hr","img","small","s","u","code","blockquote","nav");
  private static final java.util.Set<String> VOIDS=java.util.Set.of("br","hr","img"),PHRASING=java.util.Set.of("span","b","i","strong","em","a","br","img","small","s","u","code");
  private static final String SPACE=" \t\r\n\f";
  /** Describe the precise supported subset without advertising general HTML5 parsing. */
  public static String supportedSubset(){return "UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; br/hr/img void tags; ASCII case-insensitive names; quoted/unquoted and hidden Boolean attributes; amp/lt/gt/quot/apos/nbsp and numeric references; comments omitted; no doctype, implied closes, raw text, foreign content or browser recovery";}
  /** Reject malformed Unicode and unsupported controls, including C1 browser substitution characters. */
  private static String text(String value){if(value==null)throw new IllegalArgumentException("HTML requires text");for(int offset=0;offset<value.length();){int scalar=value.codePointAt(offset);offset+=Character.charCount(scalar);if(scalar==0||scalar>=0xd800&&scalar<=0xdfff||scalar>=0x7f&&scalar<=0x9f||scalar<32&&scalar!=9&&scalar!=10&&scalar!=13&&scalar!=12)throw new IllegalArgumentException("Unsupported HTML character");}return value;}
  /** Count UTF8 before allocating or retaining another output fragment. */
  private static int bytes(String value){int count=0;for(int offset=0;offset<value.length();){int scalar=value.codePointAt(offset);offset+=Character.charCount(scalar);int width=scalar<128?1:scalar<2048?2:scalar<65536?3:4;if(width>Integer.MAX_VALUE-count)throw new IllegalArgumentException("HTML byte overflow");count+=width;}return count;}
  /** Decode only approved terminated references; numeric CR values retain their literal identity. */
  private static String decode(String input){
    String value=input.replace("\r\n","\n").replace('\r','\n');var result=new StringBuilder();
    for(int position=0;position<value.length();){if(value.charAt(position)!='&'){result.append(value.charAt(position++));continue;}int end=value.indexOf(';',position+1);if(end<0||end-position>12)throw new IllegalArgumentException("HTML references require supported terminated spelling");String entity=value.substring(position+1,end);String decoded=switch(entity){case "amp"->"&";case "lt"->"<";case "gt"->">";case "quot"->"\"";case "apos"->"'";case "nbsp"->"\u00a0";default->null;};
      if(decoded==null){if(!entity.startsWith("#"))throw new IllegalArgumentException("Unsupported HTML entity");boolean hex=entity.length()>1&&(entity.charAt(1)=='x'||entity.charAt(1)=='X');int start=hex?2:1;if(start==entity.length())throw new IllegalArgumentException("Empty HTML numeric reference");int scalar=0,base=hex?16:10;for(int index=start;index<entity.length();++index){char item=entity.charAt(index);int digit=item>='0'&&item<='9'?item-'0':hex&&item>='a'&&item<='f'?item-'a'+10:hex&&item>='A'&&item<='F'?item-'A'+10:-1;if(digit<0||scalar>(0x10ffff-digit)/base)throw new IllegalArgumentException("Unsupported HTML numeric reference");scalar=scalar*base+digit;}decoded=new String(Character.toChars(scalar));}
      result.append(text(decoded));position=end+1;
    }return text(result.toString());
  }
  /** Reject relationships that independent HTML parsers would implicitly close or rearrange. */
  private static void child(java.util.List<String> open,String child){if(!TAGS.contains(child))throw new IllegalArgumentException("HTML tag outside approved subset");String parent=open.isEmpty()?"":open.get(open.size()-1);if(child.equals("li")&&!parent.equals("ul")&&!parent.equals("ol"))throw new IllegalArgumentException("HTML list item requires direct list parent");if(VOIDS.contains(parent)||(parent.equals("ul")||parent.equals("ol"))&&!child.equals("li")||(PHRASING.contains(parent)||parent.equals("p")||parent.length()==2&&parent.charAt(0)=='h'&&parent.charAt(1)>='1'&&parent.charAt(1)<='6')&&!PHRASING.contains(child)||child.equals("a")&&open.contains("a"))throw new IllegalArgumentException("HTML child requires unsupported browser recovery");}
  /** Check one approved attribute without normalizing application values or URL schemes. */
  private static boolean attribute(String tag,String key){return java.util.Set.of("id","class","title","lang","dir","hidden").contains(key)||tag.equals("a")&&(key.equals("href")||key.equals("rel"))||tag.equals("img")&&java.util.Set.of("src","alt","width","height").contains(key);}
  private static final class Output {
    final HtmlLimits limits;final StringBuilder result=new StringBuilder();int count;
    /** Own one bounded output operation without publishing partially checked text. */
    Output(HtmlLimits limits){this.limits=limits;}
    /** Charge encoded bytes before the owned builder retains the fragment. */
    void append(String value){int size=bytes(value);if(size>limits.maxOutputBytes-count)throw new IllegalArgumentException("HTML output byte limit");count+=size;result.append(value);}
    /** Escape syntax and raw CR, accounting complete Unicode scalars in bounded batches. */
    void escape(String value,boolean attribute){text(value);var batch=new StringBuilder();for(int offset=0;offset<value.length();){int scalar=value.codePointAt(offset);offset+=Character.charCount(scalar);switch(scalar){case '&':batch.append("&amp;");break;case '<':batch.append("&lt;");break;case '>':batch.append("&gt;");break;case '\r':batch.append("&#13;");break;case '"':batch.append(attribute?"&quot;":"\"");break;default:batch.appendCodePoint(scalar);break;}if(batch.length()>=4096){append(batch.toString());batch.setLength(0);}}if(batch.length()!=0)append(batch.toString());}
  }
  /** Validate an owning trace using default admission and return ordinary deterministic HTML UTF8. */
  public static byte[] writeFragment(java.util.List<HtmlEvent> events){return writeFragment(events,new HtmlLimits());}
  /** Validate every boundary/attribute and all resource bounds before returning complete HTML. */
  public static byte[] writeFragment(java.util.List<HtmlEvent> events,HtmlLimits limits){
    java.util.Objects.requireNonNull(limits);if(events==null||events.size()>limits.maxEvents||events.size()<2||!events.get(0).kind.equals("start_fragment")||!events.get(events.size()-1).kind.equals("end_fragment"))throw new IllegalArgumentException("HTML requires bounded explicit fragment boundaries");var output=new Output(limits);var open=new java.util.ArrayList<String>();int textBytes=0;
    for(int index=0;index<events.size();++index){var event=events.get(index);if(event==null)throw new IllegalArgumentException("Null HTML event");switch(event.kind){
      case "start_fragment":case "end_fragment":if(index!=0&&index!=events.size()-1||!open.isEmpty()||!event.name.isEmpty()||!event.attributes.isEmpty()||!event.text.isEmpty())throw new IllegalArgumentException("Malformed HTML fragment boundary");break;
      case "start_element":child(open,event.name);if(!event.text.isEmpty()||open.size()>=limits.maxDepth||event.attributes.size()>limits.maxAttributesPerElement)throw new IllegalArgumentException("HTML element payload/depth/attribute limit");output.append("<"+event.name);var names=new java.util.HashSet<String>();for(var item:event.attributes){String key=item.name;if(!attribute(event.name,key)||!names.add(key)||item.booleanValue!=key.equals("hidden")||item.booleanValue&&!item.value.isEmpty())throw new IllegalArgumentException("Unsupported or duplicate HTML attribute");text(item.value);output.append(" "+key);if(!item.booleanValue){output.append("=\"");output.escape(item.value,true);output.append("\"");}}output.append(">");open.add(event.name);break;
      case "end_element":if(open.isEmpty()||!open.get(open.size()-1).equals(event.name)||!event.attributes.isEmpty()||!event.text.isEmpty())throw new IllegalArgumentException("Unbalanced HTML close");if(!VOIDS.contains(event.name))output.append("</"+event.name+">");open.remove(open.size()-1);break;
      case "text":int count=bytes(event.text);String parent=open.isEmpty()?"":open.get(open.size()-1);if(!event.name.isEmpty()||!event.attributes.isEmpty()||VOIDS.contains(parent)||count>limits.maxTextBytes-textBytes)throw new IllegalArgumentException("Invalid HTML text event or cumulative byte limit");if(parent.equals("ul")||parent.equals("ol"))for(int item=0;item<event.text.length();++item)if(SPACE.indexOf(event.text.charAt(item))<0)throw new IllegalArgumentException("HTML list text requires browser recovery");textBytes+=count;output.escape(event.text,false);break;
      default:throw new IllegalArgumentException("Unknown HTML event kind");
    }}return output.result.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);
  }
)htmljava"
R"htmljava(
  private static final class Parser {
    final String source;final HtmlLimits limits;final java.util.ArrayList<HtmlEvent> events=new java.util.ArrayList<>();final java.util.ArrayList<String> open=new java.util.ArrayList<>();int position,textBytes;
    /** Borrow one fatal-decoded bounded source and own only the candidate event trace. */
    Parser(String source,HtmlLimits limits){this.source=source;this.limits=limits;}
    /** Admit every event/text run before retaining its owning event payload. */
    void add(HtmlEvent event){if(events.size()>=limits.maxEvents)throw new IllegalArgumentException("HTML event count limit");if(event.kind.equals("text")){int count=bytes(event.text);if(count>limits.maxTextBytes-textBytes)throw new IllegalArgumentException("HTML text byte limit");textBytes+=count;}events.add(event);}
    /** Skip only shared HTML whitespace, including form feed. */
    int space(int offset){while(offset<source.length()&&SPACE.indexOf(source.charAt(offset))>=0)++offset;return offset;}
    /** Scan ASCII tag/attribute syntax without locale-sensitive Unicode case folding. */
    String name(int offset){int end=offset;while(end<source.length()){char value=source.charAt(end);if(!(value>='A'&&value<='Z'||value>='a'&&value<='z'||value>='0'&&value<='9'||value=='-'))break;++end;}if(end==offset)throw new IllegalArgumentException("Unsupported HTML name syntax");return source.substring(offset,end);}
    /** Observe a source character without indexing truncated input. */
    char current(){return position<source.length()?source.charAt(position):0;}
    /** Parse explicit nesting and validate complete EOF before returning a private trace. */
    java.util.List<HtmlEvent> parse(){
      add(new HtmlEvent("start_fragment"));while(position<source.length()){
        if(current()!='<'){int end=source.indexOf('<',position);if(end<0)end=source.length();add(new HtmlEvent("text","",java.util.List.of(),decode(source.substring(position,end))));position=end;continue;}
        if(source.startsWith("<!--",position)){int end=source.indexOf("-->",position+4);if(end<0||source.substring(position+4,end).contains("--"))throw new IllegalArgumentException("Malformed HTML comment");position=end+3;continue;}
        ++position;boolean closing=current()=='/';if(closing)++position;String original=name(position),tag=original.toLowerCase(java.util.Locale.ROOT);position+=original.length();
        if(closing){position=space(position);if(current()!='>'||VOIDS.contains(tag)||open.isEmpty()||!open.get(open.size()-1).equals(tag))throw new IllegalArgumentException("HTML close requires matching explicit nonvoid start");++position;open.remove(open.size()-1);add(new HtmlEvent("end_element",tag));continue;}
        child(open,tag);if(open.size()>=limits.maxDepth)throw new IllegalArgumentException("HTML nesting limit");var attributes=new java.util.ArrayList<HtmlAttribute>();boolean selfClosing=false;
        while(true){int separator=position;position=space(position);if(position>=source.length())throw new IllegalArgumentException("Truncated HTML start tag");if(current()=='>'){++position;break;}if(source.startsWith("/>",position)){selfClosing=true;position+=2;break;}if(separator==position||attributes.size()>=limits.maxAttributesPerElement)throw new IllegalArgumentException("HTML attributes require separation and bounded cardinality");String originalKey=name(position),key=originalKey.toLowerCase(java.util.Locale.ROOT);position+=originalKey.length();int keyEnd=position;position=space(position);boolean assigned=current()=='=';String value="";
          if(assigned){position=space(position+1);if(position>=source.length())throw new IllegalArgumentException("Truncated HTML attribute");char quote=current()=='\"'||current()=='\''?source.charAt(position++):0;int end=quote!=0?source.indexOf(quote,position):position;if(quote!=0){if(end<0)throw new IllegalArgumentException("Unterminated HTML attribute");}else{while(end<source.length()&&SPACE.indexOf(source.charAt(end))<0&&source.charAt(end)!='>'){if("\"'< =`".indexOf(source.charAt(end))>=0)throw new IllegalArgumentException("Invalid unquoted HTML attribute");++end;}if(end==position)throw new IllegalArgumentException("Empty unquoted HTML attribute");}value=decode(source.substring(position,end));position=end+(quote!=0?1:0);}else position=keyEnd;
          if(key.equals("hidden")){if(assigned&&!value.isEmpty()&&!value.equalsIgnoreCase("hidden"))throw new IllegalArgumentException("HTML hidden requires Boolean spelling");value="";}else if(!assigned)throw new IllegalArgumentException("Only hidden may omit HTML attribute value");attributes.add(new HtmlAttribute(key,value,key.equals("hidden")));
        }
        if(selfClosing&&!VOIDS.contains(tag))throw new IllegalArgumentException("Self-closing nonvoid HTML requires browser recovery");add(new HtmlEvent("start_element",tag,attributes,""));if(VOIDS.contains(tag))add(new HtmlEvent("end_element",tag));else open.add(tag);
      }if(!open.isEmpty())throw new IllegalArgumentException("Unclosed HTML fragment");add(new HtmlEvent("end_fragment"));writeFragment(events,limits);return java.util.List.copyOf(events);
    }
  }
  /** Parse approved ordinary UTF8 HTML with default bounds and no XML parser. */
  public static java.util.List<HtmlEvent> parseFragment(byte[] input){return parseFragment(input,new HtmlLimits());}
  /** Fatal-decode Unicode and admit the whole candidate only after checked balanced EOF. */
  public static java.util.List<HtmlEvent> parseFragment(byte[] input,HtmlLimits limits){java.util.Objects.requireNonNull(limits);if(input==null||input.length>limits.maxInputBytes)throw new IllegalArgumentException("HTML input byte limit");try{String source=java.nio.charset.StandardCharsets.UTF_8.newDecoder().onMalformedInput(java.nio.charset.CodingErrorAction.REPORT).onUnmappableCharacter(java.nio.charset.CodingErrorAction.REPORT).decode(java.nio.ByteBuffer.wrap(input)).toString();return new Parser(text(source),limits).parse();}catch(java.nio.charset.CharacterCodingException failure){throw new IllegalArgumentException("Invalid HTML UTF8",failure);}}
}
)htmljava"
