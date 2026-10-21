// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
const (
  srlPackageContentTypes = "http://schemas.openxmlformats.org/package/2006/content-types"
  srlPackageRelationshipNamespace = "http://schemas.openxmlformats.org/package/2006/relationships"
  srlPackageRelationshipMIME = "application/vnd.openxmlformats-package.relationships+xml"
  srlPackageCompatibilityNamespace = "http://schemas.openxmlformats.org/markup-compatibility/2006"
  srlPackageXMLNamespace = "http://www.w3.org/XML/1998/namespace"
)

// SerializerPackageBinaryPart owns a whole logical part; names never become extraction paths.
type SerializerPackageBinaryPart struct {
  Data []byte
  Name, ContentType string
  SourceIndex int
  Compression string
}
// SerializerPackageModelPart binds a typed owning value to a discovered dynamic role occurrence.
type SerializerPackageModelPart[T any] struct { Value T; Name, ContentType string; SourceIndex int }
// SerializerPackageRelationship retains authored inert URI spelling and source-local identity.
type SerializerPackageRelationship struct { Source, ID, Type, Target, Mode string }
// SerializerPackageExtensions preserves whole-part bytes and source-local links independently.
type SerializerPackageExtensions struct { Parts []SerializerPackageBinaryPart; Relationships []SerializerPackageRelationship }
// SerializerPackageUnknown selects optional capability policy; required capabilities always reject.
type SerializerPackageUnknown uint8
const (
  SerializerPackageReject SerializerPackageUnknown = iota
  SerializerPackageSkip
  SerializerPackagePreserve
)
// SerializerPackagePartMetadata is a detached logical catalog item without transport authority.
type SerializerPackagePartMetadata struct { Name, ContentType string; entry SerializerPackageEntry }
// SerializerPackageExtensionPolicy snapshots optional behavior and sequential trusted callbacks.
// Callbacks return promptly; errors retain their exact identity and no external target is opened.
type SerializerPackageExtensionPolicy struct {
  Parts, Relationships SerializerPackageUnknown
  RequiredPart func(SerializerPackagePartMetadata)(bool,error)
  RequiredRelationship func(SerializerPackageRelationship)(bool,error)
}

// srlPackageFold folds only ASCII identity bytes, preserving all original authored Unicode.
func srlPackageFold(value string) string {
  var result srlPackageStrings.Builder
  result.Grow(len(value))
  for index:=0;index<len(value);index++ { item:=value[index];if item>='A'&&item<='Z' { item+='a'-'A' };result.WriteByte(item) }
  return result.String()
}
// srlPackageUnreserved recognizes RFC3986's exact ASCII unreserved alphabet.
func srlPackageUnreserved(value rune) bool {
  return value>='A'&&value<='Z'||value>='a'&&value<='z'||value>='0'&&value<='9'||srlPackageStrings.ContainsRune("-._~",value)
}
// srlPackageHex parses one exact ASCII hexadecimal digit without locale rules.
func srlPackageHex(value byte) int {
  if value>='0'&&value<='9' { return int(value-'0') };if value>='a'&&value<='f' { return int(value-'a')+10 };if value>='A'&&value<='F' { return int(value-'A')+10 };return -1
}
// srlPackagePartName validates the selected ASCII OPC URI subset without normalization.
func srlPackagePartName(name string,infrastructure bool) (string,error) {
  if len(name)<2||name[0]!='/'||len(name)-1>srlPackageMaxCommentBytes||srlPackageStrings.ContainsAny(name,"?#") {
    return "",srlPackageFailure(SerializerPackageInvalidName,"Invalid OPC part URI",name)
  }
  if name=="/[Content_Types].xml"&&infrastructure { return name,nil }
  for _,piece:=range srlPackageStrings.Split(name[1:],"/") {
    if piece==""||piece[len(piece)-1]=='.' { return "",srlPackageFailure(SerializerPackageInvalidName,"Invalid OPC URI segment",name) }
    for index:=0;index<len(piece);index++ {
      value:=piece[index]
      if value=='%' {
        if len(piece)-index<3||srlPackageHex(piece[index+1])<0||srlPackageHex(piece[index+2])<0 { return "",srlPackageFailure(SerializerPackageInvalidName,"Malformed OPC URI escape",name) }
        value=byte(srlPackageHex(piece[index+1])*16+srlPackageHex(piece[index+2]));index+=2
        if value>=128||srlPackageUnreserved(rune(value))||value=='/'||value=='\\' { return "",srlPackageFailure(SerializerPackageInvalidName,"Forbidden OPC URI escape",name) }
      } else if value>=128||!(srlPackageUnreserved(rune(value))||srlPackageStrings.ContainsRune("!$&'()*+,;=:@",rune(value))) {
        return "",srlPackageFailure(SerializerPackageInvalidName,"Invalid OPC URI character",name)
      }
    }
  }
  if !infrastructure {
    source,found,err:=srlPackageRelationshipSource(name);if err!=nil { return "",err };_ = source
    if srlPackageFold(name)=="/[content_types].xml"||found { return "",srlPackageFailure(SerializerPackageInvalidName,"Reserved OPC infrastructure URI",name) }
  }
  return name,nil
}
// srlPackageRelationshipSource identifies infrastructure using ASCII case-insensitive identity.
func srlPackageRelationshipSource(name string) (string,bool,error) {
  key:=srlPackageFold(name);if key=="/_rels/.rels" { return "/",true,nil }
  basename:=srlPackageStrings.LastIndexByte(key,'/');if basename<0 { return "",false,nil }
  separator:=srlPackageStrings.LastIndexByte(key[:basename],'/')
  if separator<0||key[separator+1:basename]!="_rels"||!srlPackageStrings.HasSuffix(key,".rels") { return "",false,nil }
  file:=name[basename+1:len(name)-len(".rels")]
  if file=="" { return "",false,srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid relationship part name",name) }
  source,err:=srlPackagePartName(name[:separator+1]+file,false);return source,true,err
}
// srlPackageMapsToPart excludes nonparts but fails closed on valid-shaped unsupported Unicode URIs.
func srlPackageMapsToPart(name string,state *srlPackageState) (bool,error) {
  unsupported:=false
  for index:=0;index<len(name);index++ { if name[index]>=128||name[index]=='%'&&len(name)-index>=3&&srlPackageHex(name[index+1])>=8&&srlPackageHex(name[index+2])>=0 { unsupported=true;break } }
  probe:=name
  if unsupported {
    if err:=state.consume(SerializerPackageDecoderAllocationBytes,int64(len(name)),name);err!=nil { return false,err }
    var text srlPackageStrings.Builder;text.Grow(len(name))
    for index:=0;index<len(name);index++ {
      value:=name[index]
      if value>=128 { text.WriteByte('_') } else if value=='%'&&len(name)-index>=3&&srlPackageHex(name[index+1])>=8&&srlPackageHex(name[index+2])>=0 { text.WriteString("%20");index+=2 } else { text.WriteByte(value) }
    }
    probe=text.String()
  }
  if _,err:=srlPackagePartName(probe,true);err!=nil { if failure,ok:=err.(*SerializerPackageError);ok&&failure.Code==SerializerPackageInvalidName { return false,nil };return false,err }
  if unsupported { return false,srlPackageFailure(SerializerPackageUnsupportedFeature,"Non-ASCII OPC part names are unsupported",name) };return true,nil
}
// srlPackageUniquePart rejects aliases and file/descendant ambiguity before recording identity.
func srlPackageUniquePart(names map[string]bool,name string,state *srlPackageState) (string,error) {
  key:=srlPackageFold(name)
  if names[key] { return "",srlPackageFailure(SerializerPackageDuplicateName,"Duplicate OPC part identity",name) }
  for previous:=range names {
    if err:=state.consume(SerializerPackageWorkUnits,1,name);err!=nil { return "",err }
    if srlPackageStrings.HasPrefix(key,previous+"/")||srlPackageStrings.HasPrefix(previous,key+"/") { return "",srlPackageFailure(SerializerPackageInvalidName,"Conflicting OPC part ancestry",name) }
  }
  names[key]=true;return key,nil
}
// srlPackageMIMEToken recognizes MIME tokens independently of URI unreserved punctuation.
func srlPackageMIMEToken(value byte) bool {
  return value>='A'&&value<='Z'||value>='a'&&value<='z'||value>='0'&&value<='9'||srlPackageStrings.ContainsRune("!#$%&'*+-.^_`|~",rune(value))
}
type srlPackageMIME struct { media string; parameters map[string]string }
// srlPackageParseMIME checks full consumption, significant quoted whitespace and duplicate parameters.
func srlPackageParseMIME(value string) (srlPackageMIME,error) {
  failure:=srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid OPC content type","")
  if !srlPackageUTF8.ValidString(value) { return srlPackageMIME{},failure }
  position:=0;for position<len(value)&&srlPackageMIMEToken(value[position]) { position++ }
  if position==0||position==len(value)||value[position]!='/' { return srlPackageMIME{},failure }
  position++;start:=position;for position<len(value)&&srlPackageMIMEToken(value[position]) { position++ }
  if start==position { return srlPackageMIME{},failure }
  result:=srlPackageMIME{media:srlPackageFold(value[:position]),parameters:map[string]string{}}
  for position<len(value) {
    for position<len(value)&&(value[position]==' '||value[position]=='\t') { position++ }
    if position==len(value)||value[position]!=';' { return srlPackageMIME{},failure };position++
    for position<len(value)&&(value[position]==' '||value[position]=='\t') { position++ }
    start=position;for position<len(value)&&srlPackageMIMEToken(value[position]) { position++ }
    if start==position||position==len(value)||value[position]!='=' { return srlPackageMIME{},failure }
    name:=srlPackageFold(value[start:position]);position++;var text srlPackageStrings.Builder
    if position<len(value)&&value[position]=='"' {
      position++;ended:=false
      for position<len(value) {
        current,width:=srlPackageUTF8.DecodeRuneInString(value[position:]);position+=width
        if current=='"' { ended=true;break }
        if current=='\\' { if position==len(value) { break };current,width=srlPackageUTF8.DecodeRuneInString(value[position:]);position+=width }
        if current!='\t'&&(current<32||current==127||current>255) { return srlPackageMIME{},failure };text.WriteRune(current)
      }
      if !ended { return srlPackageMIME{},failure }
    } else { start=position;for position<len(value)&&srlPackageMIMEToken(value[position]) { position++ };if start==position { return srlPackageMIME{},failure };text.WriteString(value[start:position]) }
    if _,exists:=result.parameters[name];exists { return srlPackageMIME{},failure };result.parameters[name]=text.String()
  }
  if len(result.parameters)!=0&&(result.media==srlPackageRelationshipMIME||result.media=="application/vnd.openxmlformats-package.core-properties+xml") { return srlPackageMIME{},failure }
  return result,nil
}
// srlPackageWithMIME admits temporary parsed byte copies and conservative parameter-record estimates.
// The trusted internal callback must not retain the parsed value after its scratch reservation is released.
func srlPackageWithMIME(value string,state *srlPackageState,part string,use func(srlPackageMIME)error) error {
  const recordBytes=int64(128)
  const parameterBytes=int64(64)
  const maximum=int64(^uint64(0)>>1)
  size:=int64(len(value))
  if size>(maximum-recordBytes)/2 { return srlPackageFailure(SerializerPackageResourceLimit,"MIME scratch extent exceeds range",part) }
  if err:=state.consume(SerializerPackageWorkUnits,size+1,part);err!=nil { return err }
  parameters:=int64(srlPackageStrings.Count(value,";"));amount:=recordBytes+size*2
  if parameters>(maximum-amount)/parameterBytes { return srlPackageFailure(SerializerPackageResourceLimit,"MIME parameter scratch exceeds range",part) };amount+=parameters*parameterBytes
  if err:=state.consume(SerializerPackageDecoderAllocationBytes,amount,part);err!=nil { return err }
  if err:=state.budget.hold(amount,part);err!=nil { return err };defer state.budget.release(amount)
  parsed,err:=srlPackageParseMIME(value);if err!=nil { return err };if use!=nil { return use(parsed) };return nil
}
// equal compares normalized media/parameter names while preserving parameter value case and spacing.
func (value srlPackageMIME) equal(other srlPackageMIME) bool {
  if value.media!=other.media||len(value.parameters)!=len(other.parameters) { return false }
  for name,text:=range value.parameters { if otherText,exists:=other.parameters[name];!exists||text!=otherText { return false } };return true
}

type srlPackageURI struct { scheme,authority,path,query,fragment string; hasScheme,hasAuthority,hasQuery,hasFragment bool }
// srlPackageIRI validates the RFC3987 character set with private-use characters only in queries.
func srlPackageIRI(value,extra string,query bool) error {
  if !srlPackageUTF8.ValidString(value) { return srlPackageFailure(SerializerPackageInvalidRelationship,"Invalid IRI UTF-8","") }
  for position:=0;position<len(value); {
    code,width:=srlPackageUTF8.DecodeRuneInString(value[position:]);position+=width
    if code=='%' { if len(value)-position<2||srlPackageHex(value[position])<0||srlPackageHex(value[position+1])<0 { return srlPackageFailure(SerializerPackageInvalidRelationship,"Malformed IRI escape","") };position+=2;continue }
    ordinary:=code>=0xa0&&code<=0xd7ff||code>=0xf900&&code<=0xfdcf||code>=0xfdf0&&code<=0xffef||code>=0x10000&&code<=0xdfffd&&(code&65535)<=0xfffd||code>=0xe1000&&code<=0xefffd
    private:=query&&(code>=0xe000&&code<=0xf8ff||code>=0xf0000&&code<=0xffffd||code>=0x100000&&code<=0x10fffd)
    if !ordinary&&!private&&!(code<128&&(srlPackageUnreserved(code)||srlPackageStrings.ContainsRune("!$&'()*+,;="+extra,code))) { return srlPackageFailure(SerializerPackageInvalidRelationship,"Invalid relationship IRI character","") }
  }
  return nil
}
// srlPackageDigits admits an empty port but no signs, whitespace, delimiters or normalization.
func srlPackageDigits(value string) bool { for index:=0;index<len(value);index++ { if value[index]<'0'||value[index]>'9' { return false } };return true }
// srlPackageScheme checks the ASCII scheme grammar before any authority split.
func srlPackageScheme(value string) bool {
  if value==""||!(value[0]>='A'&&value[0]<='Z'||value[0]>='a'&&value[0]<='z') { return false }
  for index:=1;index<len(value);index++ { item:=value[index];if !(item>='A'&&item<='Z'||item>='a'&&item<='z'||item>='0'&&item<='9'||item=='+'||item=='-'||item=='.') { return false } };return true
}
// srlPackageIPLiteral checks IPv6 without zone IDs and RFC3986 IPvFuture without network access.
func srlPackageIPLiteral(value string) bool {
  if parsed,err:=srlPackageIP.ParseAddr(value);err==nil&&parsed.Is6()&&parsed.Zone()=="" { return true }
  if len(value)<4||(value[0]!='v'&&value[0]!='V') { return false }
  position:=1;for position<len(value)&&srlPackageHex(value[position])>=0 { position++ }
  if position==1||position==len(value)||value[position]!='.' { return false };position++;if position==len(value) { return false }
  for ;position<len(value);position++ { item:=value[position];if item>=128||!(srlPackageUnreserved(rune(item))||srlPackageStrings.ContainsRune("!$&'()*+,;=:",rune(item))) { return false } };return true
}
// srlPackageParseURI splits literal ASCII delimiters and retains exact Unicode authority spelling.
func srlPackageParseURI(value string,absolute bool) (srlPackageURI,error) {
)packagego"
R"packagego(
  result:=srlPackageURI{};failure:=srlPackageFailure(SerializerPackageInvalidRelationship,"Invalid relationship URI","")
  if at:=srlPackageStrings.IndexByte(value,'#');at>=0 { result.hasFragment=true;result.fragment=value[at+1:];value=value[:at] }
  if at:=srlPackageStrings.IndexByte(value,'?');at>=0 { result.hasQuery=true;result.query=value[at+1:];value=value[:at] }
  colon,slash:=srlPackageStrings.IndexByte(value,':'),srlPackageStrings.IndexByte(value,'/')
  if colon>=0&&(slash<0||colon<slash) { result.hasScheme=true;result.scheme=value[:colon];value=value[colon+1:];if !srlPackageScheme(result.scheme) { return result,failure } }
  if absolute&&(!result.hasScheme||result.hasFragment) { return result,failure }
  if srlPackageStrings.HasPrefix(value,"//") {
    result.hasAuthority=true;at:=srlPackageStrings.IndexByte(value[2:],'/');if at<0 { result.authority=value[2:];value="" } else { at+=2;result.authority=value[2:at];value=value[at:] }
  }
  if result.hasAuthority {
    host:=result.authority
    if at:=srlPackageStrings.IndexByte(host,'@');at>=0 { if err:=srlPackageIRI(host[:at],":",false);err!=nil { return result,err };host=host[at+1:] }
    if srlPackageStrings.HasPrefix(host,"[") {
      end:=srlPackageStrings.IndexByte(host,']');if end<0||!srlPackageIPLiteral(host[1:end]) { return result,failure }
      tail:=host[end+1:];if tail!=""&&(tail[0]!=':'||!srlPackageDigits(tail[1:])) { return result,failure }
    } else {
      at:=srlPackageStrings.IndexByte(host,':');name:=host;if at>=0 { name=host[:at];if !srlPackageDigits(host[at+1:]) { return result,failure } }
      if err:=srlPackageIRI(name,"",false);err!=nil { return result,err }
    }
  }
  result.path=value
  for _,item:=range []struct{value,extra string;query bool}{{value,":@/",false},{result.query,":@/?",true},{result.fragment,":@/?",false}} {
    if err:=srlPackageIRI(item.value,item.extra,item.query);err!=nil { return result,err }
  }
  return result,nil
}
// srlPackageTarget resolves internal dot segments while keeping authored target text separately.
func srlPackageTarget(source,text string) (string,error) {
  value,err:=srlPackageParseURI(text,false);if err!=nil { return "",err }
  if value.hasScheme||value.hasAuthority { return "",srlPackageFailure(SerializerPackageInvalidRelationship,"Internal target has scheme or authority",source) }
  if value.path=="" { return srlPackagePartName(source,false) }
  if srlPackageStrings.Contains(value.path,"//")||srlPackageStrings.HasSuffix(value.path,"/")||value.path=="."||value.path==".."||srlPackageStrings.HasSuffix(value.path,"/.")||srlPackageStrings.HasSuffix(value.path,"/..") { return "",srlPackageFailure(SerializerPackageInvalidRelationship,"Relationship targets a directory",source) }
  pieces:=[]string{}
  if value.path[0]!='/' { base:=source[1:srlPackageStrings.LastIndexByte(source,'/')+1];for _,piece:=range srlPackageStrings.Split(base,"/") { if piece!="" { pieces=append(pieces,piece) } } }
  for _,piece:=range srlPackageStrings.Split(value.path,"/") { if piece==""||piece=="." { continue };if piece==".." { if len(pieces)>0 { pieces=pieces[:len(pieces)-1] } } else { pieces=append(pieces,piece) } }
  return srlPackagePartName("/"+srlPackageStrings.Join(pieces,"/"),false)
}
// srlPackageNameStart follows XML1.0's explicit NCName start ranges.
func srlPackageNameStart(value rune) bool {
  return value=='_'||value>='A'&&value<='Z'||value>='a'&&value<='z'||value>=0xc0&&value<=0xd6||value>=0xd8&&value<=0xf6||value>=0xf8&&value<=0x2ff||value>=0x370&&value<=0x37d||value>=0x37f&&value<=0x1fff||value>=0x200c&&value<=0x200d||value>=0x2070&&value<=0x218f||value>=0x2c00&&value<=0x2fef||value>=0x3001&&value<=0xd7ff||value>=0xf900&&value<=0xfdcf||value>=0xfdf0&&value<=0xfffd||value>=0x10000&&value<=0xeffff
}
// srlPackageNCName rejects empty, malformed UTF8 and colon-containing relationship IDs.
func srlPackageNCName(value string) bool {
  if value==""||!srlPackageUTF8.ValidString(value) { return false };first:=true
  for _,item:=range value { if !srlPackageNameStart(item)&&(first||!(item=='-'||item=='.'||item>='0'&&item<='9'||item==0xb7||item>=0x300&&item<=0x36f||item>=0x203f&&item<=0x2040)) { return false };first=false };return true
}
// srlPackageCollapse applies exactly XML whitespace rules to metadata token values.
func srlPackageCollapse(value string) string { return srlPackageStrings.Join(srlPackageStrings.FieldsFunc(value,func(item rune)bool{return item==' '||item=='\t'||item=='\r'||item=='\n'})," ") }
)packagego"
