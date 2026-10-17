// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// XmlLimits bounds one complete typed XML operation; zero limits are not defaults.
type XmlLimits struct { MaxBytes, MaxTextBytes, MaxNodes, MaxAttributes, MaxDepth int }
// DefaultXmlLimits returns an independent whole-document resource policy.
func DefaultXmlLimits() XmlLimits { return XmlLimits{16<<20, 8<<20, 100000, 100000, 128} }
// XmlValidator is a caller-prepared trusted XSD provider; it must never fetch resources.
type XmlValidator interface { ValidateXML([]byte) error }
// XmlOptions selects limits and an optional runtime provider for this complete operation.
type XmlOptions struct { Limits XmlLimits; Validator XmlValidator }
// DefaultXmlOptions enables generated checks without a runtime validator.
func DefaultXmlOptions() XmlOptions { return XmlOptions{Limits:DefaultXmlLimits()} }
type srlXmlNode struct { name srlxml.Name; attrs []srlxml.Attr; children []*srlXmlNode; text string }
type srlXmlBudget struct { limits XmlLimits; nodes, attrs, text, depth int }
type srlXmlBindingChange struct { prefix, previous string; existed bool }
type srlXmlFrame struct { node *srlXmlNode; rawName srlxml.Name; changes []srlXmlBindingChange }
var srlXmlDeclaration=regexp.MustCompile(`^[ \r\n\t]*version[ \r\n\t]*=[ \r\n\t]*(?:"1\.0"|'1\.0')(?:[ \r\n\t]+encoding[ \r\n\t]*=[ \r\n\t]*(?:"(?i:UTF-8)"|'(?i:UTF-8)'))?(?:[ \r\n\t]+standalone[ \r\n\t]*=[ \r\n\t]*(?:"(?:yes|no)"|'(?:yes|no)'))?[ \r\n\t]*$`)
// Charge before retaining another node or scalar in the whole-document tree.
func (b *srlXmlBudget) node(name srlxml.Name) (*srlXmlNode,error) {
  if b.nodes >= b.limits.MaxNodes { return nil, fmt.Errorf("XML node limit exceeded") }
  if err:=b.textValue(name.Space+name.Local);err!=nil{return nil,err}
  b.nodes++; return &srlXmlNode{name:name},nil
}
// Charge cumulative text/name storage with overflow-safe subtraction.
func (b *srlXmlBudget) textValue(text string) error {
  if len(text)>b.limits.MaxTextBytes-b.text { return fmt.Errorf("XML text limit exceeded") }
  if !utf8.ValidString(text) { return fmt.Errorf("Invalid XML UTF-8") }
  for _,c:=range text { if !(c==9||c==10||c==13||c>=32&&c<=0xd7ff||c>=0xe000&&c<=0xfffd||c>=0x10000&&c<=0x10ffff) { return fmt.Errorf("Invalid XML character") } }
  b.text+=len(text); return nil
}
// Bind one explicit policy and reject nonsensical resource bounds.
func srlXmlOptions(values []XmlOptions) (XmlOptions,error) {
  if len(values)>1 { return XmlOptions{},fmt.Errorf("Expected at most one XML policy") }
  value:=DefaultXmlOptions(); if len(values)==1 { value=values[0] }
  l:=value.Limits; if l.MaxBytes<0||l.MaxTextBytes<0||l.MaxNodes<0||l.MaxAttributes<0||l.MaxDepth<0 { return value,fmt.Errorf("Negative XML limit") }
  return value,nil
}
// Parse a complete document using namespace-aware strict tokens and bounded retention.
func srlXmlParse(data []byte, limits XmlLimits) (*srlXmlNode,error) {
  if len(data)>limits.MaxBytes { return nil,fmt.Errorf("XML input limit exceeded") }
  if !utf8.Valid(data) { return nil,fmt.Errorf("Invalid XML UTF-8") }
  if len(data)>=3&&data[0]==0xef&&data[1]==0xbb&&data[2]==0xbf { data=data[3:] }
  input:=srlxml.NewDecoder(strings.NewReader(string(data))); input.Strict=true
  budget:=srlXmlBudget{limits:limits}; var root *srlXmlNode; stack:=[]srlXmlFrame{}
  namespaces:=map[string]string{"xml":"http://www.w3.org/XML/1998/namespace"}; seenToken:=false
  resolve:=func(raw srlxml.Name,attribute bool)(srlxml.Name,error){
    if strings.Contains(raw.Local,":")||strings.Contains(raw.Space,":")||raw.Local=="" { return raw,fmt.Errorf("Invalid XML qualified name") }
    if raw.Space==""&&attribute { return raw,nil }
    uri,found:=namespaces[raw.Space];if raw.Space!=""&&!found{return raw,fmt.Errorf("Undeclared XML namespace prefix")}
    return srlxml.Name{Space:uri,Local:raw.Local},nil
  }
  for { token,err:=input.RawToken(); if err==srlio.EOF { break }; if err!=nil { return nil,fmt.Errorf("Malformed XML") }
    switch value:=token.(type) {
    case srlxml.StartElement:
      if len(stack)>=limits.MaxDepth { return nil,fmt.Errorf("XML depth limit exceeded") }
      changes:=[]srlXmlBindingChange{};seenRaw:=map[srlxml.Name]bool{}
      for _,a:=range value.Attr {
        if budget.attrs>=limits.MaxAttributes { return nil,fmt.Errorf("XML attribute limit exceeded") }; budget.attrs++
        if err=budget.textValue(a.Name.Space+a.Name.Local+a.Value); err!=nil { return nil,err }
        if seenRaw[a.Name]{return nil,fmt.Errorf("Duplicate XML attribute")};seenRaw[a.Name]=true
        if a.Name.Space=="xmlns"||a.Name.Space==""&&a.Name.Local=="xmlns" {
          prefix:=a.Name.Local;if a.Name.Space==""{prefix=""}
          if prefix=="xmlns"||a.Value=="http://www.w3.org/2000/xmlns/"||prefix=="xml"&&a.Value!="http://www.w3.org/XML/1998/namespace"||prefix!="xml"&&a.Value=="http://www.w3.org/XML/1998/namespace"||prefix!=""&&a.Value=="" { return nil,fmt.Errorf("Invalid XML namespace declaration") }
          previous,existed:=namespaces[prefix];changes=append(changes,srlXmlBindingChange{prefix,previous,existed});namespaces[prefix]=a.Value
        }
      }
      expanded,err:=resolve(value.Name,false);if err!=nil{return nil,err}
      node,err:=budget.node(expanded); if err!=nil { return nil,err }
      seen:=map[srlxml.Name]bool{}
      for _,a:=range value.Attr {
        if a.Name.Space=="xmlns"||a.Name.Space==""&&a.Name.Local=="xmlns" { continue }
        a.Name,err=resolve(a.Name,true);if err!=nil{return nil,err}
        if seen[a.Name] { return nil,fmt.Errorf("Duplicate XML attribute") }; seen[a.Name]=true; node.attrs=append(node.attrs,a)
      }
      if len(stack)==0 { if root!=nil { return nil,fmt.Errorf("Multiple XML roots") }; root=node } else { parent:=stack[len(stack)-1].node; parent.children=append(parent.children,node) }
      stack=append(stack,srlXmlFrame{node,value.Name,changes})
    case srlxml.EndElement:
      if len(stack)==0||stack[len(stack)-1].rawName!=value.Name { return nil,fmt.Errorf("Unexpected XML end") }
      for _,change:=range stack[len(stack)-1].changes {if change.existed{namespaces[change.prefix]=change.previous}else{delete(namespaces,change.prefix)}}
      stack=stack[:len(stack)-1]
    case srlxml.CharData:
      text:=string(value); if err:=budget.textValue(text); err!=nil { return nil,err }
      if len(stack)==0 { if strings.Trim(text," \r\n\t")!="" { return nil,fmt.Errorf("Text outside XML root") } } else { node:=stack[len(stack)-1].node; node.text+=text }
    case srlxml.Directive: return nil,fmt.Errorf("XML DTDs and directives are prohibited")
    case srlxml.ProcInst:
      if strings.EqualFold(value.Target,"xml")&&(seenToken||value.Target!="xml"||!srlXmlDeclaration.Match(value.Inst)) { return nil,fmt.Errorf("Invalid XML 1.0 UTF-8 declaration") }
    }
    seenToken=true
  }
  if root==nil||len(stack)!=0 { return nil,fmt.Errorf("Incomplete XML document") }; return root,nil
}
type srlXmlSink struct { text strings.Builder; maximum int }
// Reject oversized serialized output before growing its byte storage.
func (w *srlXmlSink) Write(data []byte)(int,error) { if len(data)>w.maximum-w.text.Len() { return 0,fmt.Errorf("XML output limit exceeded") }; return w.text.Write(data) }
// Emit deterministic data XML without a private envelope or external resources.
func srlXmlWrite(node *srlXmlNode, limits XmlLimits)([]byte,error) {
  sink:=srlXmlSink{maximum:limits.MaxBytes}; output:=srlxml.NewEncoder(&sink)
  var visit func(*srlXmlNode,int,string)error
  visit=func(n *srlXmlNode,depth int,inheritedNamespace string)error {
    if depth>limits.MaxDepth { return fmt.Errorf("XML depth limit exceeded") }
    start:=srlxml.StartElement{Name:n.name,Attr:n.attrs}
    // encoding/xml does not reset an inherited default namespace for empty Name.Space.
    if n.name.Space==""&&inheritedNamespace!="" { start.Attr=append(start.Attr,srlxml.Attr{Name:srlxml.Name{Local:"xmlns"},Value:""}) }
    if err:=output.EncodeToken(start); err!=nil { return err }
    if err:=output.EncodeToken(srlxml.CharData(n.text)); err!=nil { return err }
    for _,child:=range n.children { if err:=visit(child,depth+1,n.name.Space); err!=nil { return err } }
    return output.EncodeToken(start.End())
  }
  if err:=visit(node,1,""); err!=nil { return nil,err }; if err:=output.Flush(); err!=nil { return nil,err }; return []byte(sink.text.String()),nil
}
type srlXmlFields struct { node *srlXmlNode; next int; attrs map[srlxml.Name]bool }
// Require element-only record content before any field is decoded.
func srlXmlReader(node *srlXmlNode,hasText ...bool)(*srlXmlFields,error) { if (len(hasText)==0||!hasText[0])&&strings.Trim(node.text," \r\n\t")!="" { return nil,fmt.Errorf("Unexpected XML record text") }; return &srlXmlFields{node:node,attrs:map[srlxml.Name]bool{}},nil }
// Read simple content while rejecting element children and retaining mapped attributes.
func (r *srlXmlFields) text()([]*srlXmlNode,error) { if len(r.node.children)!=0{return nil,fmt.Errorf("XML simple content contains children")};return []*srlXmlNode{{text:r.node.text}},nil }
// Select an exact expanded-name attribute, keeping absent optionals distinct.
func (r *srlXmlFields) attribute(name srlxml.Name,required bool)([]*srlXmlNode,error) {
  for _,a:=range r.node.attrs { if a.Name==name { if r.attrs[name] { return nil,fmt.Errorf("Duplicate XML field mapping") }; r.attrs[name]=true; return []*srlXmlNode{{name:name,text:a.Value}},nil } }
  if required { return nil,fmt.Errorf("Required XML attribute missing") }; return nil,nil
}
// Consume one contiguous declared child sequence with generated cardinality bounds.
func (r *srlXmlFields) children(name srlxml.Name,minimum,maximum int)([]*srlXmlNode,error) {
  start:=r.next; for r.next<len(r.node.children)&&r.node.children[r.next].name==name { r.next++ }
  count:=r.next-start; if count<minimum||maximum>=0&&count>maximum { return nil,fmt.Errorf("XML child order/cardinality mismatch") }; return r.node.children[start:r.next],nil
}
// Reject all unknown attributes and leftover duplicate, misplaced or unknown children.
func (r *srlXmlFields) finish()error { if len(r.attrs)!=len(r.node.attrs)||r.next!=len(r.node.children) { return fmt.Errorf("Unexpected XML field") }; return nil }
// Require one wrapper and exhaust its exact item contract.
func (r *srlXmlFields) wrapped(wrapper,name srlxml.Name,minimum,maximum int)([]*srlXmlNode,error) { nodes,err:=r.children(wrapper,1,1);if err!=nil{return nil,err};fields,err:=srlXmlReader(nodes[0]);if err!=nil{return nil,err};result,err:=fields.children(name,minimum,maximum);if err!=nil{return nil,err};if err=fields.finish();err!=nil{return nil,err};return result,nil }
// Scalars have no attribute or nested-element representation in this mapping.
func srlXmlText(node *srlXmlNode)(string,error) { if len(node.attrs)!=0||len(node.children)!=0 { return "",fmt.Errorf("XML scalar has markup") }; return node.text,nil }
var srlXmlInteger=regexp.MustCompile(`^[+-]?[0-9]+$`)
var srlXmlFloat=regexp.MustCompile(`^[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?$`)
// Parse XML integer lexemes exactly and retain the declared integer width.
func srlXmlInt(text string,bits int)(int64,error) { text=strings.Trim(text," \r\n\t"); if !srlXmlInteger.MatchString(text) { return 0,fmt.Errorf("Invalid XML integer") }; value,err:=strconv.ParseInt(text,10,bits); if err!=nil { return 0,fmt.Errorf("XML integer out of range") }; return value,nil }
// Parse nonnegative XML integer lexemes without narrowing or signed overflow.
func srlXmlUint(text string,bits int)(uint64,error) { text=strings.Trim(text," \r\n\t"); if !srlXmlInteger.MatchString(text) { return 0,fmt.Errorf("Invalid XML unsigned integer") };if strings.HasPrefix(text,"-"){if strings.TrimLeft(text[1:],"0")==""{return 0,nil};return 0,fmt.Errorf("Negative XML unsigned integer")}; value,err:=strconv.ParseUint(strings.TrimPrefix(text,"+"),10,bits); if err!=nil { return 0,fmt.Errorf("XML integer out of range") }; return value,nil }
// Accept only the XML Schema Boolean spellings.
func srlXmlBool(text string)(bool,error) { switch strings.Trim(text," \r\n\t") { case "true","1":return true,nil;case "false","0":return false,nil };return false,fmt.Errorf("Invalid XML Boolean") }
// Parse XML float spellings and reject permissive host-library extensions.
func srlXmlDouble(text string,bits int)(float64,error) { text=strings.Trim(text," \r\n\t"); switch text { case "INF":return math.Inf(1),nil;case "-INF":return math.Inf(-1),nil;case "NaN":return math.NaN(),nil };if !srlXmlFloat.MatchString(text) { return 0,fmt.Errorf("Invalid XML float") };value,err:=strconv.ParseFloat(text,bits);if err!=nil{return 0,fmt.Errorf("XML float out of range")};return value,nil }
// Emit canonical XML nonfinite values and shortest round-trippable finite values.
func srlXmlFormatFloat(value float64,bits int)string { if math.IsNaN(value){return "NaN"};if math.IsInf(value,1){return "INF"};if math.IsInf(value,-1){return "-INF"};return strconv.FormatFloat(value,'g',-1,bits) }
// Charge complete generated attributes before retaining their scalar contents.
func srlXmlAttribute(node *srlXmlNode,name srlxml.Name,text string,b *srlXmlBudget)error { if b.attrs>=b.limits.MaxAttributes{return fmt.Errorf("XML attribute limit exceeded")};if err:=b.textValue(name.Space+name.Local+text);err!=nil{return err};b.attrs++;node.attrs=append(node.attrs,srlxml.Attr{Name:name,Value:text});return nil }
)serializer"
