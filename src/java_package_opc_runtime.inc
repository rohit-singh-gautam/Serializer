// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejava(
  private static final String CT="http://schemas.openxmlformats.org/package/2006/content-types";
  private static final String RELS="http://schemas.openxmlformats.org/package/2006/relationships";
  private static final String RELS_MIME="application/vnd.openxmlformats-package.relationships+xml";
  private static final String MC="http://schemas.openxmlformats.org/markup-compatibility/2006";
  private static final String XML="http://www.w3.org/XML/1998/namespace";
  /** A whole opaque part borrows bytes for synchronous output and owns fresh bytes after load. */
  public static final class BinaryPart {
    public final byte[] data;public final String name,contentType,compression;public final int sourceIndex;
    /** Select one root-scoped stored occurrence. */
    public BinaryPart(byte[] data,String name,String contentType){this(data,name,contentType,0,"stored");}
    /** Retain explicit dynamic metadata and its zero-based parent role occurrence. */
    public BinaryPart(byte[] data,String name,String contentType,int sourceIndex,String compression){this.data=data;this.name=name;this.contentType=contentType;this.sourceIndex=sourceIndex;this.compression=compression;}
  }
  /** Dynamic typed occurrences retain discovered names/media types and the source occurrence. */
  public static final class ModelPart<T> {
    public final T value;public final String name,contentType;public final int sourceIndex;
    /** Bind an owning native value to one explicit package role occurrence. */
    public ModelPart(T value,String name,String contentType,int sourceIndex){this.value=value;this.name=name;this.contentType=contentType;this.sourceIndex=sourceIndex;}
  }
  /** Relationship targets remain authored inert URI references; external targets are never opened. */
  public static final class Relationship {
    public final String source,id,type,target,mode;
    /** Construct an internal link; IDs are scoped by the source part. */
    public Relationship(String source,String id,String type,String target){this(source,id,type,target,"internal");}
    /** Retain exact external spelling independently of URI identity comparisons. */
    public Relationship(String source,String id,String type,String target,String mode){this.source=source;this.id=id;this.type=type;this.target=target;this.mode=mode;}
  }
  /** Preserve exact whole-part bytes and source-local links independently. */
  public static final class Extensions {
    public final java.util.List<BinaryPart> parts;public final java.util.List<Relationship> relationships;
    private java.util.Map<String,java.util.List<String>> origins=java.util.Map.of();
    /** Own immutable list structure while byte arrays remain ordinary application values. */
    public Extensions(java.util.List<BinaryPart> parts,java.util.List<Relationship> relationships){this.parts=java.util.List.copyOf(parts);this.relationships=java.util.List.copyOf(relationships);}
    /** Construct empty retained extension state. */
    public Extensions(){this(java.util.List.of(),java.util.List.of());}
  }
  public enum Unknown { REJECT, SKIP, PRESERVE }
  /** Required capabilities reject regardless of optional skip/preserve selection. */
  public static final class ExtensionPolicy {
    public final Unknown parts,relationships;
    public final java.util.function.Predicate<PartMetadata> requiredPart;
    public final java.util.function.Predicate<Relationship> requiredRelationship;
    /** Reject unknown capabilities by default. */
    public ExtensionPolicy(){this(Unknown.REJECT,Unknown.REJECT,null,null);}
    /** Select independent optional part/link behavior and application capability predicates. */
    public ExtensionPolicy(Unknown parts,Unknown relationships,java.util.function.Predicate<PartMetadata> requiredPart,java.util.function.Predicate<Relationship> requiredRelationship){this.parts=java.util.Objects.requireNonNull(parts);this.relationships=java.util.Objects.requireNonNull(relationships);this.requiredPart=requiredPart;this.requiredRelationship=requiredRelationship;}
  }
  /** A result publishes only a complete fresh aggregate and its separately retained extensions. */
  public static final class Result<T> {
    public final T value;public final Extensions extensions;
    /** Bind accepted values after all required validations succeed. */
    public Result(T value,Extensions extensions){this.value=value;this.extensions=extensions;}
  }
  /** Owning logical part metadata keeps its original physical entry private. */
  public static final class PartMetadata {
    public final String name,contentType;private final Entry entry;
    /** Retain the checked physical/logical association. */
    private PartMetadata(String name,String contentType,Entry entry){this.name=name;this.contentType=contentType;this.entry=entry;}
  }
  /** ASCII identity folding does not normalize Unicode IRI data or percent-escaped reserved bytes. */
  private static String fold(String value){return value.toLowerCase(java.util.Locale.ROOT);}
  /** Recognize ASCII unreserved characters without locale-dependent classification. */
  private static boolean unreserved(int value){return value>='A'&&value<='Z'||value>='a'&&value<='z'||value>='0'&&value<='9'||"-._~".indexOf(value)>=0;}
  /** Convert exactly one ASCII hexadecimal digit. */
  private static int hex(char value){return value>='0'&&value<='9'?value-'0':value>='A'&&value<='F'?value-'A'+10:value>='a'&&value<='f'?value-'a'+10:-1;}
  /** Identify reserved relationship infrastructure without interpreting a filesystem path. */
  private static String relationshipSource(String name) throws java.io.IOException {
    String key=fold(name);if(key.equals("/_rels/.rels"))return "/";
    int basename=key.lastIndexOf('/'),separator=key.lastIndexOf('/',basename-1);
    if(separator<0 || !key.substring(separator+1,basename).equals("_rels") || !key.endsWith(".rels"))return null;
    String file=name.substring(basename+1,name.length()-5);if(file.isEmpty())throw error(Code.INVALID_METADATA,"Invalid relationship part name",name);
    return partName(name.substring(0,separator+1)+file,false);
  }
  /** Validate the selected ASCII OPC part-URI subset and reserved infrastructure boundaries. */
  private static String partName(String name,boolean infrastructure) throws java.io.IOException {
    if(name==null || !name.startsWith("/") || name.length()<2 || name.length()-1>65535 || name.indexOf('?')>=0 || name.indexOf('#')>=0)
      throw error(Code.INVALID_NAME,"Invalid OPC part URI",name==null?"":name);
    if(name.equals("/[Content_Types].xml") && infrastructure)return name;
    for(String piece:name.substring(1).split("/",-1)) {
      if(piece.isEmpty() || piece.endsWith("."))throw error(Code.INVALID_NAME,"Invalid OPC URI segment",name);
      for(int index=0;index<piece.length();++index) {
        int value=piece.charAt(index);
        if(value=='%') {
          if(index+2>=piece.length() || hex(piece.charAt(index+1))<0 || hex(piece.charAt(index+2))<0)throw error(Code.INVALID_NAME,"Malformed OPC URI escape",name);
          value=hex(piece.charAt(index+1))*16+hex(piece.charAt(index+2));index+=2;
          if(value>=128 || unreserved(value) || value=='/' || value=='\\')throw error(Code.INVALID_NAME,"Forbidden OPC URI escape",name);
        } else if(value>=128 || !(unreserved(value)||"!$&'()*+,;=:@".indexOf(value)>=0))throw error(Code.INVALID_NAME,"Invalid OPC URI character",name);
      }
    }
    if(!infrastructure && (fold(name).equals("/[content_types].xml") || relationshipSource(name)!=null))
      throw error(Code.INVALID_NAME,"Reserved OPC infrastructure URI",name);
    return name;
  }
  /** Exclude non-part physical names while failing closed on otherwise valid unsupported Unicode URIs. */
  private static boolean mapsToPart(String name,State state) throws java.io.IOException {
    boolean unsupported=false;for(int index=0;index<name.length();++index){char value=name.charAt(index);if(value>=128||value=='%'&&index+2<name.length()&&hex(name.charAt(index+1))>=8&&hex(name.charAt(index+2))>=0){unsupported=true;break;}}
    String probe=name;
    if(unsupported){state.consume(Resource.DECODER_ALLOCATION_BYTES,name.length()*2L,name);var text=new StringBuilder(name.length());for(int index=0;index<name.length();++index){char value=name.charAt(index);if(value>=128)text.append('_');else if(value=='%'&&index+2<name.length()&&hex(name.charAt(index+1))>=8&&hex(name.charAt(index+2))>=0){text.append("%20");index+=2;}else text.append(value);}probe=text.toString();}
    try{partName(probe,true);}catch(PackageError failure){if(failure.code!=Code.INVALID_NAME)throw failure;return false;}
    if(unsupported)throw error(Code.UNSUPPORTED_FEATURE,"Non-ASCII OPC part names are unsupported",name);return true;
  }
  /** Reject case/escape aliases and file/descendant collisions before retaining part identity. */
  private static String unique(java.util.Set<String> names,String name) throws java.io.IOException {
    String key=fold(name);if(names.contains(key))throw error(Code.DUPLICATE_NAME,"Duplicate OPC part identity",name);
    for(String old:names)if(key.startsWith(old+"/") || old.startsWith(key+"/"))throw error(Code.INVALID_NAME,"Conflicting OPC part ancestry",name);
    names.add(key);return key;
  }
  /** Parse a MIME identity without treating quoted semicolons as parameter separators. */
  private record Mime(String media,java.util.Map<String,String> parameters) {}
  /** Recognize the exact MIME token alphabet. */
  private static boolean mimeToken(char value){return unreserved(value)&&value!='~'||"!#$%&'*+.^_`|~-".indexOf(value)>=0;}
  /** Validate complete type/subtype/parameters, rejecting duplicate parameter names and reserved parameters. */
  private static Mime mime(String value) throws java.io.IOException {
    if(value==null)throw new PackageError(Code.INVALID_METADATA,"Missing OPC content type");
    int position=0;while(position<value.length()&&mimeToken(value.charAt(position)))++position;
    if(position==0 || position==value.length() || value.charAt(position)!='/')throw new PackageError(Code.INVALID_METADATA,"Invalid OPC media type");
    ++position;int subtype=position;while(position<value.length()&&mimeToken(value.charAt(position)))++position;
    if(position==subtype)throw new PackageError(Code.INVALID_METADATA,"Invalid OPC media subtype");
    String media=fold(value.substring(0,position));var parameters=new java.util.TreeMap<String,String>();
    while(position<value.length()) {
      while(position<value.length()&&(value.charAt(position)==' '||value.charAt(position)=='\t'))++position;
      if(position==value.length() || value.charAt(position++)!=';')throw new PackageError(Code.INVALID_METADATA,"Invalid MIME parameter delimiter");
      while(position<value.length()&&(value.charAt(position)==' '||value.charAt(position)=='\t'))++position;
      int start=position;while(position<value.length()&&mimeToken(value.charAt(position)))++position;
      if(start==position || position==value.length() || value.charAt(position++)!='=')throw new PackageError(Code.INVALID_METADATA,"Invalid MIME parameter");
      String name=fold(value.substring(start,position-1));var text=new StringBuilder();
      if(position<value.length()&&value.charAt(position)=='"') {
        ++position;boolean ended=false;
        while(position<value.length()) {
          char current=value.charAt(position++);if(current=='"'){ended=true;break;}
          if(current=='\\'){if(position==value.length())break;current=value.charAt(position++);}
          if(current!='\t'&&(current<32 || current==127 || current>255))throw new PackageError(Code.INVALID_METADATA,"Invalid quoted MIME parameter");
          text.append(current);
        }
        if(!ended)throw new PackageError(Code.INVALID_METADATA,"Unterminated MIME parameter");
      } else {start=position;while(position<value.length()&&mimeToken(value.charAt(position)))++position;if(start==position)throw new PackageError(Code.INVALID_METADATA,"Empty MIME parameter");text.append(value,start,position);}
      if(parameters.put(name,text.toString())!=null)throw new PackageError(Code.INVALID_METADATA,"Duplicate MIME parameter");
    }
    if(!parameters.isEmpty()&&(media.equals(RELS_MIME)||media.equals("application/vnd.openxmlformats-package.core-properties+xml")))
      throw new PackageError(Code.INVALID_METADATA,"Reserved OPC media type has parameters");
    return new Mime(media,java.util.Map.copyOf(parameters));
  }
)packagejava"
R"packagejava(
  /** URI components are parsed using literal ASCII delimiters, preserving Unicode authority spelling. */
  private record Uri(String scheme,String authority,String path,String query,String fragment) {}
  /** Validate a bounded IRI component without normalization or network resolution. */
  private static void iri(String value,String extra,boolean query) throws java.io.IOException {
    if(value==null)return;
    for(int position=0;position<value.length();) {
      int code=value.codePointAt(position);position+=Character.charCount(code);
      if(code=='%'){if(position+1>=value.length()||hex(value.charAt(position))<0||hex(value.charAt(position+1))<0)throw new PackageError(Code.INVALID_RELATIONSHIP,"Malformed IRI escape");position+=2;continue;}
      boolean ordinary=code>=0xa0&&code<=0xd7ff || code>=0xf900&&code<=0xfdcf || code>=0xfdf0&&code<=0xffef || code>=0x10000&&code<=0xdfffd&&(code&65535)<=0xfffd || code>=0xe1000&&code<=0xefffd;
      boolean privateUse=query&&(code>=0xe000&&code<=0xf8ff || code>=0xf0000&&code<=0xffffd || code>=0x100000&&code<=0x10fffd);
      if(!ordinary&&!privateUse&&!(code<128&&(unreserved(code)||"!$&'()*+,;=".indexOf(code)>=0||extra.indexOf(code)>=0)))throw new PackageError(Code.INVALID_RELATIONSHIP,"Invalid relationship IRI character");
    }
  }
  /** Count complete nonempty IPv6 groups, allowing embedded IPv4 only as the global final group. */
  private static int ipGroups(String value,boolean finalSide) {
    if(value.isEmpty())return 0;String[] groups=value.split(":",-1);int count=0;
    for(int index=0;index<groups.length;++index) {
      String group=groups[index];if(group.isEmpty())return -1;
      if(group.indexOf('.')>=0) {
        if(!finalSide || index+1!=groups.length)return -1;String[] pieces=group.split("\\.",-1);if(pieces.length!=4)return -1;
        for(String piece:pieces){if(!piece.matches("0|[1-9][0-9]{0,2}")||Integer.parseInt(piece)>255)return -1;}count+=2;
      } else {if(!group.matches("[0-9a-fA-F]{1,4}"))return -1;++count;}
    }
    return count;
  }
  /** Validate compression spelling exactly; triple colons, zones and multiple compressions reject. */
  private static boolean ipv6(String value) {
    if(value.indexOf('%')>=0)return false;int at=value.indexOf("::");
    if(at<0)return ipGroups(value,true)==8;
    if(value.indexOf("::",at+2)>=0)return false;
    int left=ipGroups(value.substring(0,at),false),right=ipGroups(value.substring(at+2),true);
    return left>=0&&right>=0&&left+right<8;
  }
  /** Validate inert absolute/relative IRI references without opening external resources. */
  private static Uri uri(String original,boolean absolute) throws java.io.IOException {
    if(original==null)throw new PackageError(Code.INVALID_RELATIONSHIP,"Missing relationship URI");
    String value=original,fragment=null,query=null,scheme=null,authority=null;int at=value.indexOf('#');
    if(at>=0){fragment=value.substring(at+1);value=value.substring(0,at);}at=value.indexOf('?');
    if(at>=0){query=value.substring(at+1);value=value.substring(0,at);}int colon=value.indexOf(':'),slash=value.indexOf('/');
    if(colon>=0&&(slash<0||colon<slash)){scheme=value.substring(0,colon);value=value.substring(colon+1);if(!scheme.matches("[A-Za-z][A-Za-z0-9+.-]*"))throw new PackageError(Code.INVALID_RELATIONSHIP,"Invalid URI scheme");}
    if(absolute&&(scheme==null||fragment!=null))throw new PackageError(Code.INVALID_RELATIONSHIP,"Relationship type requires absolute IRI");
    if(value.startsWith("//")){at=value.indexOf('/',2);authority=value.substring(2,at<0?value.length():at);value=at<0?"":value.substring(at);}
    if(authority!=null) {
      String host=authority;at=host.indexOf('@');if(at>=0){iri(host.substring(0,at),":",false);host=host.substring(at+1);}
      if(host.startsWith("[")) {
        int end=host.indexOf(']');if(end<0)throw new PackageError(Code.INVALID_RELATIONSHIP,"Unterminated IP authority");String literal=host.substring(1,end);
        if(!(ipv6(literal)||literal.matches("[vV][0-9a-fA-F]+\\.[A-Za-z0-9._~!$&'()*+,;=:-]+")) || !host.substring(end+1).matches("(?::[0-9]*)?"))
          throw new PackageError(Code.INVALID_RELATIONSHIP,"Invalid IP authority");
      } else {at=host.indexOf(':');iri(at<0?host:host.substring(0,at),"",false);if(at>=0&&!host.substring(at+1).matches("[0-9]*"))throw new PackageError(Code.INVALID_RELATIONSHIP,"Invalid URI port");}
    }
    iri(value,":@/",false);iri(query,":@/?",true);iri(fragment,":@/?",false);return new Uri(scheme,authority,value,query,fragment);
  }
  /** Resolve relative dot segments to exact logical part identity, keeping original target text separately. */
  private static String target(String source,String text) throws java.io.IOException {
    Uri value=uri(text,false);if(value.scheme!=null||value.authority!=null)throw error(Code.INVALID_RELATIONSHIP,"Internal target has scheme or authority",source);
    if(value.path.isEmpty())return partName(source,false);
    if(value.path.endsWith("/")||value.path.equals(".")||value.path.equals("..")||value.path.endsWith("/.")||value.path.endsWith("/.."))throw error(Code.INVALID_RELATIONSHIP,"Relationship targets a directory",source);
    var pieces=new java.util.ArrayList<String>();
    if(!value.path.startsWith("/")){String base=source.substring(1,source.lastIndexOf('/')+1);for(String piece:base.split("/"))if(!piece.isEmpty())pieces.add(piece);}
    for(String piece:value.path.split("/")){if(piece.isEmpty()||piece.equals("."))continue;if(piece.equals("..")){if(!pieces.isEmpty())pieces.remove(pieces.size()-1);}else pieces.add(piece);}
    return partName("/"+String.join("/",pieces),false);
  }
  /** Apply only XML's four whitespace characters to schema-derived metadata tokens. */
  private static String collapse(String value){return value.replaceAll("[ \\t\\r\\n]+"," ").replaceAll("^ | $","");}
  /** Recognize XML1.0 NCName start characters independently of the process locale. */
  private static boolean nameStart(int value) {
    return value=='_'||value>='A'&&value<='Z'||value>='a'&&value<='z'||value>=0xc0&&value<=0xd6||value>=0xd8&&value<=0xf6||value>=0xf8&&value<=0x2ff||value>=0x370&&value<=0x37d||value>=0x37f&&value<=0x1fff||value>=0x200c&&value<=0x200d||value>=0x2070&&value<=0x218f||value>=0x2c00&&value<=0x2fef||value>=0x3001&&value<=0xd7ff||value>=0xf900&&value<=0xfdcf||value>=0xfdf0&&value<=0xfffd||value>=0x10000&&value<=0xeffff;
  }
  /** Relationship IDs and compatibility prefixes preserve valid non-ASCII NCNames. */
  private static boolean ncname(String value) {
    if(value==null||value.isEmpty())return false;int position=0,code=value.codePointAt(0);if(!nameStart(code))return false;position+=Character.charCount(code);
    while(position<value.length()){code=value.codePointAt(position);position+=Character.charCount(code);if(!nameStart(code)&&!(code=='-'||code=='.'||code>='0'&&code<='9'||code==0xb7||code>=0x300&&code<=0x36f||code>=0x203f&&code<=0x2040))return false;}return true;
  }
)packagejava"
