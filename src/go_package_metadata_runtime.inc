// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
type srlPackageNode struct {
  name srlPackageXML.Name
  attributes map[srlPackageXML.Name]string
  namespaces map[string]string
  children []*srlPackageNode
  text srlPackageStrings.Builder
}
type srlPackageMetadataTree struct { root *srlPackageNode; held int64; state *srlPackageState; part string }
type srlPackageMetadataFrame struct { node *srlPackageNode; raw srlPackageXML.Name }

// reserve admits cumulative allocation and live logical metadata scratch before retaining it.
func (tree *srlPackageMetadataTree) reserve(amount int64) error {
  if err:=tree.state.consume(SerializerPackageDecoderAllocationBytes,amount,tree.part);err!=nil { return err }
  if err:=tree.state.budget.hold(amount,tree.part);err!=nil { return err };tree.held+=amount;return nil
}
// close releases the private tree's logical scratch once its declarations have been consumed.
func (tree *srlPackageMetadataTree) close() { tree.root=nil;tree.state.budget.release(tree.held);tree.held=0 }

// srlPackageXMLCharacter recognizes the exact XML1.0 legal character ranges.
func srlPackageXMLCharacter(item rune) bool { return item==9||item==10||item==13||item>=32&&item<=0xd7ff||item>=0xe000&&item<=0xfffd||item>=0x10000&&item<=0x10ffff }
// srlPackageMetadataText admits strict UTF8 or BOM-marked UTF16 with no replacement decoding.
func srlPackageMetadataText(data []byte,tree *srlPackageMetadataTree) (string,string,error) {
  encoding:="UTF-8";offset:=0;little:=len(data)>=2&&data[0]==255&&data[1]==254;big:=len(data)>=2&&data[0]==254&&data[1]==255
  fail:=srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid OPC metadata encoding",tree.part)
  if little||big {
    encoding="UTF-16";offset=2;if (len(data)-offset)%2!=0 { return "","",fail }
    if err:=tree.reserve(int64(len(data))*2);err!=nil { return "","",err };var text srlPackageStrings.Builder
    text.Grow(len(data)*2)
    unit:=func(at int)uint16 { if little { return uint16(data[at])|uint16(data[at+1])<<8 };return uint16(data[at])<<8|uint16(data[at+1]) }
    for offset<len(data) {
      first:=unit(offset);offset+=2;value:=rune(first)
      if first>=0xd800&&first<=0xdbff {
        if len(data)-offset<2 { return "","",fail };second:=unit(offset);offset+=2
        if second<0xdc00||second>0xdfff { return "","",fail };value=0x10000+(rune(first)-0xd800)*1024+rune(second)-0xdc00
      } else if first>=0xdc00&&first<=0xdfff { return "","",fail }
      if !srlPackageXMLCharacter(value) { return "","",fail };text.WriteRune(value)
    }
    return text.String(),encoding,nil
  }
  if len(data)>=3&&data[0]==239&&data[1]==187&&data[2]==191 { offset=3 }
  if !srlPackageUTF8.Valid(data[offset:]) { return "","",fail }
  for _,item:=range string(data[offset:]) { if !srlPackageXMLCharacter(item) { return "","",fail } }
  if err:=tree.reserve(int64(len(data)-offset));err!=nil { return "","",err };return string(data[offset:]),encoding,nil
}

var srlPackageDeclaration=srlPackageRegexp.MustCompile(`^[ \r\n\t]*version[ \r\n\t]*=[ \r\n\t]*(?:"1\.0"|'1\.0')(?:[ \r\n\t]+encoding[ \r\n\t]*=[ \r\n\t]*(?:"(?i:UTF-8|UTF-16)"|'(?i:UTF-8|UTF-16)'))?(?:[ \r\n\t]+standalone[ \r\n\t]*=[ \r\n\t]*(?:"(?:yes|no)"|'(?:yes|no)'))?[ \r\n\t]*$`)
var srlPackageDeclaredEncoding=srlPackageRegexp.MustCompile(`encoding[ \r\n\t]*=[ \r\n\t]*["']([^"']*)["']`)

// srlPackageResolveXML resolves an original raw name against one checked namespace scope.
func srlPackageResolveXML(raw srlPackageXML.Name,scope map[string]string,attribute bool) (srlPackageXML.Name,error) {
  if !srlPackageNCName(raw.Local)||(raw.Space!=""&&!srlPackageNCName(raw.Space)) { return raw,srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid XML qualified name","") }
  if attribute&&raw.Space=="" { return raw,nil }
  uri,found:=scope[raw.Space];if raw.Space!=""&&!found { return raw,srlPackageFailure(SerializerPackageInvalidMetadata,"Undeclared XML namespace prefix","") }
  return srlPackageXML.Name{Space:uri,Local:raw.Local},nil
}

// srlPackageMetadata parses bounded namespace-aware XML with DTD/entity/network access unavailable.
// Its caller closes the returned private tree after consuming declarations; no tree escapes an aggregate.
func srlPackageMetadata(data []byte,part string,state *srlPackageState) (result *srlPackageMetadataTree,err error) {
  tree:=&srlPackageMetadataTree{state:state,part:part};success:=false
  defer func(){if !success { tree.close() }}()
  if err=state.consume(SerializerPackageMetadataBytes,int64(len(data)),part);err!=nil { return nil,err }
  if err=state.consume(SerializerPackageWorkUnits,int64(len(data))*2,part);err!=nil { return nil,err }
  text,encoding,err:=srlPackageMetadataText(data,tree);if err!=nil { return nil,err }
  input:=srlPackageXML.NewDecoder(srlPackageStrings.NewReader(text));input.Strict=true
  input.CharsetReader=func(name string,reader srlPackageIO.Reader)(srlPackageIO.Reader,error) {
    if !srlPackageStrings.EqualFold(name,encoding) { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Metadata encoding declaration mismatch",part) };return reader,nil
  }
  stack:=[]srlPackageMetadataFrame{};seenToken:=false
  invalid:=func()error{return srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid or unsupported OPC XML metadata",part)}
  for {
    if err=state.poll();err!=nil { return nil,err };token,failure:=input.RawToken()
    if failure==srlPackageIO.EOF { break };if failure!=nil { return nil,invalid() }
    switch value:=token.(type) {
    case srlPackageXML.StartElement:
      const maximumDepth=128
      if len(stack)>=maximumDepth { return nil,srlPackageFailure(SerializerPackageResourceLimit,"Metadata depth limit exceeded",part) }
      inherited:=map[string]string{"xml":srlPackageXMLNamespace};if len(stack)!=0 { inherited=stack[len(stack)-1].node.namespaces }
      reservation:=int64(128)+int64(len(value.Attr))*128+int64(len(inherited)+len(value.Attr))*64
      if err=tree.reserve(reservation);err!=nil { return nil,err }
      scope:=make(map[string]string,len(inherited)+len(value.Attr));for prefix,uri:=range inherited { scope[prefix]=uri }
      seenRaw:=map[srlPackageXML.Name]bool{}
      for _,attribute:=range value.Attr {
        if seenRaw[attribute.Name] { return nil,invalid() };seenRaw[attribute.Name]=true
        if err=tree.reserve(int64(len(attribute.Name.Space)+len(attribute.Name.Local)+len(attribute.Value)));err!=nil { return nil,err }
        if attribute.Name.Space=="xmlns"||attribute.Name.Space==""&&attribute.Name.Local=="xmlns" {
          prefix:=attribute.Name.Local;if attribute.Name.Space=="" { prefix="" }
          if prefix!=""&&!srlPackageNCName(prefix)||prefix=="xmlns"||attribute.Value=="http://www.w3.org/2000/xmlns/"||prefix=="xml"&&attribute.Value!=srlPackageXMLNamespace||prefix!="xml"&&attribute.Value==srlPackageXMLNamespace||prefix!=""&&attribute.Value=="" { return nil,invalid() }
          scope[prefix]=attribute.Value
        }
      }
      expanded,failure:=srlPackageResolveXML(value.Name,scope,false);if failure!=nil { return nil,invalid() }
      if err=tree.reserve(int64(len(expanded.Space)+len(expanded.Local)));err!=nil { return nil,err }
      node:=&srlPackageNode{name:expanded,namespaces:scope,attributes:map[srlPackageXML.Name]string{}}
      for _,attribute:=range value.Attr {
        if attribute.Name.Space=="xmlns"||attribute.Name.Space==""&&attribute.Name.Local=="xmlns" { continue }
        name,failure:=srlPackageResolveXML(attribute.Name,scope,true);if failure!=nil { return nil,invalid() }
        if _,found:=node.attributes[name];found { return nil,invalid() };node.attributes[name]=attribute.Value
      }
      if len(stack)==0 { if tree.root!=nil { return nil,invalid() };tree.root=node } else { parent:=stack[len(stack)-1].node;parent.children=append(parent.children,node) }
      stack=append(stack,srlPackageMetadataFrame{node:node,raw:value.Name})
    case srlPackageXML.EndElement:
      if len(stack)==0||stack[len(stack)-1].raw!=value.Name { return nil,invalid() };stack=stack[:len(stack)-1]
    case srlPackageXML.CharData:
      if len(stack)==0 { if srlPackageStrings.Trim(string(value)," \t\r\n")!="" { return nil,invalid() } } else {
        if err=tree.reserve(int64(len(value)));err!=nil { return nil,err };stack[len(stack)-1].node.text.Write(value)
      }
    case srlPackageXML.Directive:return nil,invalid()
    case srlPackageXML.ProcInst:
      if srlPackageStrings.EqualFold(value.Target,"xml") {
        if seenToken||value.Target!="xml"||!srlPackageDeclaration.Match(value.Inst) { return nil,invalid() }
        declared:=srlPackageDeclaredEncoding.FindSubmatch(value.Inst);if len(declared)!=0&&!srlPackageStrings.EqualFold(string(declared[1]),encoding) { return nil,invalid() }
      }
    }
    seenToken=true
  }
  if tree.root==nil||len(stack)!=0 { return nil,invalid() }
  if err=state.poll();err!=nil { return nil,err };success=true;return tree,nil
}

// srlPackageNamed compares exact expanded names independently of prefix spelling.
func srlPackageNamed(node *srlPackageNode,namespace,local string) bool { return node.name.Space==namespace&&node.name.Local==local }
// srlPackageElementContent enforces element-only OPC declarations after compatibility processing.
func srlPackageElementContent(node *srlPackageNode,part string) error {
  if srlPackageStrings.Trim(node.text.String()," \t\r\n")!="" { return srlPackageFailure(SerializerPackageInvalidMetadata,"Unexpected OPC metadata text",part) };return nil
}
// srlPackageAttributes validates the complete attribute set before interpreting any declaration.
func srlPackageAttributes(node *srlPackageNode,part string,allowed ...string) error {
  for name:=range node.attributes {
    found:=false;for _,local:=range allowed { if name.Space==""&&name.Local==local { found=true;break } }
    if !found { return srlPackageFailure(SerializerPackageInvalidMetadata,"Unexpected OPC metadata attribute",part) }
  }
  return nil
}
// srlPackageRawAttribute reads a required value without changing quoted MIME parameter whitespace.
func srlPackageRawAttribute(node *srlPackageNode,name,part string) (string,error) {
  value,found:=node.attributes[srlPackageXML.Name{Local:name}];if !found { return "",srlPackageFailure(SerializerPackageInvalidMetadata,"Missing OPC metadata attribute",part) };return value,nil
}
// srlPackageAttribute applies XML token collapse only to token-valued metadata attributes.
func srlPackageAttribute(node *srlPackageNode,name,part string) (string,error) {
  value,err:=srlPackageRawAttribute(node,name,part);if err!=nil { return "",err };return srlPackageCollapse(value),nil
}
)packagego"
