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
// @xml_operation_options@ adds operation-owned admission without changing positional XmlOptions literals.
type @xml_operation_options@ struct { Options XmlOptions; Operation func(string,int64,string)error; ExpectedRoot,ExpectedType string }
type srlXmlOperationValidator struct { validator XmlValidator; operation func(string,int64,string)error; root,expectedType string }
type srlXmlSelectedOptions struct { XmlOptions; Operation func(string,int64,string)error; ExpectedRoot,ExpectedType string }
// ValidateXML preserves the caller provider when a policy is inspected outside generated entry points.
func (v srlXmlOperationValidator) ValidateXML(data []byte)error{if v.validator!=nil{return v.validator.ValidateXML(data)};return nil}
// Policy snapshots the additional context while preserving ordinary provider ownership and legacy options.
func (v @xml_operation_options@) Policy()XmlOptions{return srlXmlWithOperation(v.Options,v.Operation,v.ExpectedRoot,v.ExpectedType)}
// srlXmlWithOperation carries package-local context through the existing optional provider slot.
func srlXmlWithOperation(options XmlOptions,operation func(string,int64,string)error,root,expectedType string)XmlOptions{validator:=options.Validator;if previous,ok:=validator.(srlXmlOperationValidator);ok{validator=previous.validator};options.Validator=srlXmlOperationValidator{validator:validator,operation:operation,root:root,expectedType:expectedType};return options}
// charge admits native work before retaining any new scalar/event storage.
func (b *srlXmlBudget) charge(resource string,count int64)error{if b.operation!=nil{return b.operation(resource,count,b.field)};return nil}
// DefaultXmlOptions enables generated checks without a runtime validator.
func DefaultXmlOptions() XmlOptions { return XmlOptions{Limits:DefaultXmlLimits()} }
type srlXmlNode struct { name srlxml.Name; attrs []srlxml.Attr; children []*srlXmlNode; text string }
type srlXmlBudget struct { limits XmlLimits; nodes, attrs, text, depth int; operation func(string,int64,string)error;field string }
type srlXmlBindingChange struct { prefix, previous string; existed bool }
type srlXmlFrame struct { node *srlXmlNode; rawName srlxml.Name; changes []srlXmlBindingChange }
var srlXmlDeclaration=regexp.MustCompile(`^[ \r\n\t]*version[ \r\n\t]*=[ \r\n\t]*(?:"1\.0"|'1\.0')(?:[ \r\n\t]+encoding[ \r\n\t]*=[ \r\n\t]*(?:"(?i:UTF-8)"|'(?i:UTF-8)'))?(?:[ \r\n\t]+standalone[ \r\n\t]*=[ \r\n\t]*(?:"(?:yes|no)"|'(?:yes|no)'))?[ \r\n\t]*$`)
// Charge before retaining another node or scalar in the whole-document tree.
func (b *srlXmlBudget) node(name srlxml.Name) (*srlXmlNode,error) {
  if b.nodes >= b.limits.MaxNodes { return nil, srlXmlLimitError("XML node limit exceeded") }
  if err:=b.textValue(name.Space+name.Local);err!=nil{return nil,err}
  b.nodes++; return &srlXmlNode{name:name},nil
}
// Charge cumulative text/name storage with overflow-safe subtraction.
func (b *srlXmlBudget) textValue(text string) error {
  if len(text)>b.limits.MaxTextBytes-b.text { return srlXmlLimitError("XML text limit exceeded") }
  if !utf8.ValidString(text) { return fmt.Errorf("Invalid XML UTF-8") }
  for _,c:=range text { if !(c==9||c==10||c==13||c>=32&&c<=0xd7ff||c>=0xe000&&c<=0xfffd||c>=0x10000&&c<=0x10ffff) { return fmt.Errorf("Invalid XML character") } }
  if err:=b.charge("work",int64(len(text)));err!=nil{return err};if err:=b.charge("allocation",int64(len(text)));err!=nil{return err};b.text+=len(text); return nil
}
// Bind one explicit policy and reject nonsensical resource bounds.
func srlXmlOptions(values []XmlOptions) (srlXmlSelectedOptions,error) {
  if len(values)>1 { return srlXmlSelectedOptions{},fmt.Errorf("Expected at most one XML policy") }
  selected:=DefaultXmlOptions();if len(values)==1{selected=values[0]};value:=srlXmlSelectedOptions{XmlOptions:selected}
  if context,ok:=selected.Validator.(srlXmlOperationValidator);ok{value.Validator=context.validator;value.Operation=context.operation;value.ExpectedRoot=context.root;value.ExpectedType=context.expectedType}
  l:=value.Limits; if l.MaxBytes<0||l.MaxTextBytes<0||l.MaxNodes<0||l.MaxAttributes<0||l.MaxDepth<0 { return value,fmt.Errorf("Negative XML limit") }
  return value,nil
}
// Parse a complete document using namespace-aware strict tokens and bounded retention.
func srlXmlParse(data []byte, limits XmlLimits) (*srlXmlNode,error) {
  if len(data)>limits.MaxBytes { return nil,srlXmlLimitError("XML input limit exceeded") }
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
      if len(stack)>=limits.MaxDepth { return nil,srlXmlLimitError("XML depth limit exceeded") }
      changes:=[]srlXmlBindingChange{};seenRaw:=map[srlxml.Name]bool{}
      for _,a:=range value.Attr {
        if budget.attrs>=limits.MaxAttributes { return nil,srlXmlLimitError("XML attribute limit exceeded") }; budget.attrs++
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
// A single lookahead event replaces retained input trees for generated readers.
type srlXmlInputToken struct { kind byte; name srlxml.Name; attrs []srlxml.Attr; text string; closed map[string]string }
type srlXmlInputFrame struct { raw srlxml.Name; changes []srlXmlBindingChange }
type srlXmlInput struct { decoder *srlxml.Decoder; budget srlXmlBudget; mapLimits XmlMapLimits; mixedLimits XmlMixedLimits; identityLimits XmlIdentityLimits; digestLimits XmlDigestLimits; extensionOptions XmlExtensionOptions; identities []*srlXmlIdentity; stack []srlXmlInputFrame; namespaces map[string]string; look *srlXmlInputToken; seen, root, captureNamespaces bool; namespaceBytes int }
// Bind the original byte slice after complete-message byte and UTF-8 admission checks.
func srlXmlInputStart(data []byte,limits XmlLimits,maps ...XmlMapLimits)(*srlXmlInput,error) {
  if len(data)>limits.MaxBytes{return nil,srlXmlLimitError("XML input limit exceeded")};if !utf8.Valid(data){return nil,fmt.Errorf("Invalid XML UTF-8")}
  if len(data)>=3&&data[0]==0xef&&data[1]==0xbb&&data[2]==0xbf{data=data[3:]}
  decoder:=srlxml.NewDecoder(strings.NewReader(string(data)));decoder.Strict=true
  mapLimits:=DefaultXmlMapLimits();if len(maps)!=0{mapLimits=maps[0]}
  return &srlXmlInput{decoder:decoder,budget:srlXmlBudget{limits:limits},mapLimits:mapLimits,mixedLimits:DefaultXmlMixedLimits(),identityLimits:DefaultXmlIdentityLimits(),digestLimits:DefaultXmlDigestLimits(),extensionOptions:DefaultXmlExtensionOptions(),namespaces:map[string]string{"xml":"http://www.w3.org/XML/1998/namespace"}},nil
}
// Resolve a raw name while preserving the distinct unqualified attribute namespace.
func (r *srlXmlInput) resolve(name srlxml.Name,attribute bool)(srlxml.Name,error) {
  if name.Local==""||strings.Contains(name.Local,":")||strings.Contains(name.Space,":"){return name,fmt.Errorf("Invalid XML qualified name")}
  if name.Space==""&&attribute{return name,nil};uri,exists:=r.namespaces[name.Space];if name.Space!=""&&!exists{return name,fmt.Errorf("Undeclared XML namespace prefix")};return srlxml.Name{Space:uri,Local:name.Local},nil
}
// Advance one strict namespace-aware token and charge all input before exposing it.
func (r *srlXmlInput) read()(*srlXmlInputToken,error) {
  for {
    raw,err:=r.decoder.RawToken();if err==srlio.EOF{if len(r.stack)!=0||!r.root{return nil,fmt.Errorf("Incomplete XML document")};return &srlXmlInputToken{},nil};if err!=nil{return nil,fmt.Errorf("Malformed XML")}
    first:=!r.seen;r.seen=true
    switch value:=raw.(type){
    case srlxml.StartElement:
      if len(r.stack)>=r.budget.limits.MaxDepth{return nil,srlXmlLimitError("XML depth limit exceeded")}
      if len(r.stack)==0{if r.root{return nil,fmt.Errorf("Multiple XML roots")};r.root=true}
      r.budget.field=value.Name.Local;changes:=[]srlXmlBindingChange{};seenRaw:=map[srlxml.Name]bool{}
      for _,a:=range value.Attr{
        r.budget.field=a.Name.Local;if r.budget.attrs>=r.budget.limits.MaxAttributes{return nil,srlXmlLimitError("XML attribute limit exceeded")};r.budget.attrs++
        if err:=r.budget.textValue(a.Name.Space+a.Name.Local+a.Value);err!=nil{return nil,err};if seenRaw[a.Name]{return nil,fmt.Errorf("Duplicate XML attribute")};seenRaw[a.Name]=true
        if a.Name.Space=="xmlns"||a.Name.Space==""&&a.Name.Local=="xmlns"{
          prefix:=a.Name.Local;if a.Name.Space==""{prefix=""}
          if prefix=="xmlns"||a.Value=="http://www.w3.org/2000/xmlns/"||prefix=="xml"&&a.Value!="http://www.w3.org/XML/1998/namespace"||prefix!="xml"&&a.Value=="http://www.w3.org/XML/1998/namespace"||prefix!=""&&a.Value==""{return nil,fmt.Errorf("Invalid XML namespace declaration")}
          previous,exists:=r.namespaces[prefix];changes=append(changes,srlXmlBindingChange{prefix,previous,exists});r.namespaces[prefix]=a.Value
        }
      }
      expanded,err:=r.resolve(value.Name,false);if err!=nil{return nil,err};if r.budget.nodes>=r.budget.limits.MaxNodes{return nil,srlXmlLimitError("XML node limit exceeded")};if err=r.budget.textValue(expanded.Space+expanded.Local);err!=nil{return nil,err};r.budget.nodes++
      r.budget.field=expanded.Local;if err:=r.budget.charge("allocation",128);err!=nil{return nil,err};event:=&srlXmlInputToken{kind:'s',name:expanded};seen:=map[srlxml.Name]bool{}
      for _,a:=range value.Attr{if a.Name.Space=="xmlns"||a.Name.Space==""&&a.Name.Local=="xmlns"{continue};a.Name,err=r.resolve(a.Name,true);if err!=nil{return nil,err};if seen[a.Name]{return nil,fmt.Errorf("Duplicate XML attribute")};seen[a.Name]=true;event.attrs=append(event.attrs,a)}
      r.stack=append(r.stack,srlXmlInputFrame{value.Name,changes});if err:=r.identityEvent(event);err!=nil{return nil,err};return event,nil
    case srlxml.EndElement:
      if len(r.stack)==0||r.stack[len(r.stack)-1].raw!=value.Name{return nil,fmt.Errorf("Unexpected XML end")};frame:=r.stack[len(r.stack)-1];if err:=r.identityEvent(&srlXmlInputToken{kind:'e'});err!=nil{return nil,err};r.stack=r.stack[:len(r.stack)-1]
      event:=&srlXmlInputToken{kind:'e'};if r.captureNamespaces{event.closed=make(map[string]string,len(frame.changes));for _,change:=range frame.changes{event.closed[change.prefix]=r.namespaces[change.prefix]}}
      for _,change:=range frame.changes{if change.existed{r.namespaces[change.prefix]=change.previous}else{delete(r.namespaces,change.prefix)}};return event,nil
    case srlxml.CharData:
      text:=string(value);if err:=r.budget.textValue(text);err!=nil{return nil,err};if len(r.stack)==0{if strings.Trim(text," \r\n\t")!=""{return nil,fmt.Errorf("Text outside XML root")};continue};event:=&srlXmlInputToken{kind:'t',text:text};if err:=r.identityEvent(event);err!=nil{return nil,err};return event,nil
    case srlxml.Directive:return nil,fmt.Errorf("XML DTDs and directives are prohibited")
    case srlxml.ProcInst:
      if strings.EqualFold(value.Target,"xml")&&(!first||value.Target!="xml"||!srlXmlDeclaration.Match(value.Inst)){return nil,fmt.Errorf("Invalid XML 1.0 UTF-8 declaration")}
    }
  }
}
// Inspect the next structural event, rejecting non-whitespace record text.
)serializer"
R"serializer(func (r *srlXmlInput) peek()(*srlXmlInputToken,error){for{if r.look==nil{event,err:=r.read();if err!=nil{return nil,err};r.look=event};if r.look.kind!='t'{return r.look,nil};if strings.Trim(r.look.text," \r\n\t")!=""{return nil,fmt.Errorf("Unexpected XML record text")};r.look=nil}}
// Require a specific expanded-name start event without accepting unknown fields.
func (r *srlXmlInput) open(name srlxml.Name)(*srlXmlInputFields,error){event,err:=r.peek();if err!=nil{return nil,err};if event.kind!='s'||event.name!=name{return nil,fmt.Errorf("Unexpected XML element")};r.look=nil;return &srlXmlInputFields{input:r,attrs:event.attrs},nil}
// Test a contiguous generated child sequence using one bounded lookahead event.
func (r *srlXmlInput) has(name srlxml.Name)(bool,error){event,err:=r.peek();return err==nil&&event.kind=='s'&&event.name==name,err}
// Check QName metadata in its active namespace scope, consuming only the recognized type attribute.
func (r *srlXmlInput) expectType(expected srlxml.Name,required bool)error{
  selected,err:=r.typeIdentity(required);if err!=nil{return err};if selected.Local!=""&&selected!=expected{return fmt.Errorf("Conflicting XML variant type")};return nil
}
// Resolve one explicit type identity before advancing outside the payload namespace scope.
func (r *srlXmlInput) typeIdentity(required bool)(srlxml.Name,error){
  event,err:=r.peek();if err!=nil{return srlxml.Name{},err};if event.kind!='s'{return srlxml.Name{},fmt.Errorf("Missing XML variant payload")}
  for index,attribute:=range event.attrs{if attribute.Name.Space=="http://www.w3.org/2001/XMLSchema-instance"&&attribute.Name.Local=="type"{
    text:=strings.Trim(attribute.Value," \r\n\t");parts:=strings.Split(text,":");raw:=srlxml.Name{Local:text}
    if len(parts)==2&&parts[0]!=""&&parts[1]!=""{raw=srlxml.Name{Space:parts[0],Local:parts[1]}}else if len(parts)!=1||text==""{return srlxml.Name{},fmt.Errorf("Invalid XML variant type QName")}
    identity,err:=r.resolve(raw,false);if err!=nil{return srlxml.Name{},err}
    event.attrs=append(event.attrs[:index],event.attrs[index+1:]...);return identity,nil
  }}
  if required{return srlxml.Name{},fmt.Errorf("Required XML variant type missing")};return srlxml.Name{},nil
}
// Distinguish absent, nil and present content before constructing native optional layers.
func (r *srlXmlInput) presence(name srlxml.Name,absent,nilAllowed bool)(byte,error){
  present,err:=r.has(name);if err!=nil{return 0,err};if !present{if absent{return 'a',nil};return 0,fmt.Errorf("Required XML presence element missing")}
  event:=r.look
  for index,attribute:=range event.attrs{if attribute.Name.Space=="http://www.w3.org/2001/XMLSchema-instance"&&attribute.Name.Local=="nil"{
    if !nilAllowed{return 0,fmt.Errorf("XML xsi:nil is prohibited by presence policy")}
    marker:=strings.Trim(attribute.Value," \r\n\t");if marker!="true"&&marker!="1"&&marker!="false"&&marker!="0"{return 0,fmt.Errorf("Invalid XML xsi:nil Boolean")}
    event.attrs=append(event.attrs[:index],event.attrs[index+1:]...);if marker=="false"||marker=="0"{return 'v',nil}
    fields,err:=r.open(name);if err!=nil{return 0,err};if len(fields.attrs)!=0{return 0,fmt.Errorf("Unexpected nil XML attribute")};text,err:=r.text();if err!=nil{return 0,err};if text!=""{return 0,fmt.Errorf("Nil XML value contains text")};return 'n',nil
  }}
  return 'v',nil
}
)serializer"
R"serializer(
// Collect only one scalar's text, including split CDATA/entity token boundaries.
func (r *srlXmlInput) text()(string,error){var value strings.Builder;for{var event *srlXmlInputToken;if r.look!=nil{event=r.look;r.look=nil}else{var err error;event,err=r.read();if err!=nil{return "",err}};if event.kind=='e'{return value.String(),nil};if event.kind!='t'{return "",fmt.Errorf("XML scalar has markup")};value.WriteString(event.text)}}
// Decode scalar content and reject all attributes before consuming its end token.
func (r *srlXmlInput) scalar(name srlxml.Name)(string,error){fields,err:=r.open(name);if err!=nil{return "",err};if len(fields.attrs)!=0{return "",fmt.Errorf("XML scalar has markup")};return r.text()}
// Confirm the complete document after the root candidate has been checked.
func (r *srlXmlInput) finish()error{event,err:=r.peek();if err!=nil{return err};if event.kind!=0||len(r.identities)!=0{return fmt.Errorf("Unexpected XML trailing content or incomplete identity scope")};return nil}
type srlXmlInputFields struct { input *srlXmlInput; attrs []srlxml.Attr; used int }
// Read one exact attribute; clear consumed entries so duplicate mappings reject.
func (f *srlXmlInputFields) attribute(name srlxml.Name,required bool)(string,bool,error){for i,a:=range f.attrs{if a.Name==name{f.attrs[i].Name=srlxml.Name{};f.used++;return a.Value,true,nil}};if required{return "",false,fmt.Errorf("Required XML attribute missing")};return "",false,nil}
// Exhaust every attribute and close exactly one record or wrapper.
func (f *srlXmlInputFields) finish()error{if f.used!=len(f.attrs){return fmt.Errorf("Unexpected XML attribute")};event,err:=f.input.peek();if err!=nil{return err};if event.kind!='e'{return fmt.Errorf("Unexpected XML field")};f.input.look=nil;return nil}
// Reject oversized serialized output before growing its byte storage.
func (w *srlXmlSink) Write(data []byte)(int,error) { if len(data)>w.maximum-w.text.Len() { return 0,srlXmlLimitError("XML output limit exceeded") }; return w.text.Write(data) }
// Retain only the pending start tag and namespace of each active ancestor.
type srlXmlOutputFrame struct { start srlxml.StartElement; expanded srlxml.Name; pending bool; inherited string; manual,scoped bool; attributes map[srlxml.Name]bool; identityAttributes map[string]string }
type srlXmlOutput struct { budget srlXmlBudget; mapLimits XmlMapLimits; mixedLimits XmlMixedLimits; identityLimits XmlIdentityLimits; digestLimits XmlDigestLimits; extensionOptions XmlExtensionOptions; identities []*srlXmlIdentity; sink srlXmlSink; encoder *srlxml.Encoder; stack []srlXmlOutputFrame }
// Create operation-local output; no model value or descendant node is retained.
func srlXmlOutputStart(limits XmlLimits,maps ...XmlMapLimits)*srlXmlOutput { mapLimits:=DefaultXmlMapLimits();if len(maps)!=0{mapLimits=maps[0]};result:=&srlXmlOutput{budget:srlXmlBudget{limits:limits},mapLimits:mapLimits,mixedLimits:DefaultXmlMixedLimits(),identityLimits:DefaultXmlIdentityLimits(),digestLimits:DefaultXmlDigestLimits(),extensionOptions:DefaultXmlExtensionOptions(),sink:srlXmlSink{maximum:limits.MaxBytes}};result.encoder=srlxml.NewEncoder(&result.sink);return result }
// Flush attributes before the first child or scalar text token.
func (w *srlXmlOutput) seal()error { if len(w.stack)==0{return nil};frame:=&w.stack[len(w.stack)-1];if !frame.pending{return nil};if frame.start.Name.Space==""&&frame.inherited!=""{frame.start.Attr=append(frame.start.Attr,srlxml.Attr{Name:srlxml.Name{Local:"xmlns"},Value:""})};if err:=w.identityStart(frame);err!=nil{return err};frame.pending=false;return w.encoder.EncodeToken(frame.start) }
// Admit one start token before retaining its bounded pending tag.
func (w *srlXmlOutput) open(name srlxml.Name)error { w.budget.field=name.Local;if err:=w.budget.charge("allocation",128);err!=nil{return err}; if len(w.stack)!=0&&w.stack[len(w.stack)-1].scoped{return w.openScoped(name,nil)}; if len(w.stack)>=w.budget.limits.MaxDepth{return srlXmlLimitError("XML depth limit exceeded")};if w.budget.nodes>=w.budget.limits.MaxNodes{return srlXmlLimitError("XML node limit exceeded")};if err:=w.budget.textValue(name.Space+name.Local);err!=nil{return err};if err:=w.seal();err!=nil{return err};inherited:="";manual:=false;if len(w.stack)!=0{parent:=&w.stack[len(w.stack)-1];inherited=parent.start.Name.Space;manual=parent.manual};w.budget.nodes++;w.stack=append(w.stack,srlXmlOutputFrame{start:srlxml.StartElement{Name:name},expanded:name,pending:true,inherited:inherited,manual:manual});return nil }
// Add a statically bound attribute while its start token remains pending.
func (w *srlXmlOutput) attribute(name srlxml.Name,text string)error {
  w.budget.field=name.Local;if err:=w.budget.charge("allocation",96);err!=nil{return err}
  if len(w.stack)==0||!w.stack[len(w.stack)-1].pending{return fmt.Errorf("XML attribute follows content")};if w.budget.attrs>=w.budget.limits.MaxAttributes{return srlXmlLimitError("XML attribute limit exceeded")};if err:=w.budget.textValue(name.Space+name.Local+text);err!=nil{return err};frame:=&w.stack[len(w.stack)-1]
  if len(w.identities)!=0&&name.Space!="xmlns"&&!strings.HasPrefix(name.Local,"xmlns:")&&name.Local!="xmlns"{if frame.identityAttributes==nil{frame.identityAttributes=make(map[string]string)};frame.identityAttributes[srlXmlIdentityName(name)]=text}
  if frame.manual{
    if frame.attributes==nil{frame.attributes=make(map[srlxml.Name]bool)};if frame.attributes[name]{return fmt.Errorf("Duplicate XML attribute")};frame.attributes[name]=true
    if name.Space!=""{
      prefix:="xml"
      if name.Space!="http://www.w3.org/XML/1998/namespace"{
        if frame.scoped{var exists bool;prefix,exists=w.extensionMatchingPrefix(name.Space,nil,true);if !exists{prefix=w.extensionPrefix("a",name.Space,nil)};if err:=w.extensionDeclare(prefix,name.Space);err!=nil{return err}}else{
        prefix="a"+strconv.Itoa(len(frame.start.Attr));if w.budget.attrs+1>=w.budget.limits.MaxAttributes{return srlXmlLimitError("XML namespace limit exceeded")};if err:=w.budget.textValue(prefix+name.Space);err!=nil{return err};w.budget.attrs++
        frame.start.Attr=append(frame.start.Attr,srlxml.Attr{Name:srlxml.Name{Local:"xmlns:"+prefix},Value:name.Space})
        }
      }
      name=srlxml.Name{Local:prefix+":"+name.Local}
    }
  }else{for _,attribute:=range frame.start.Attr{if attribute.Name==name{return fmt.Errorf("Duplicate XML attribute")}}}
  w.budget.attrs++;frame.start.Attr=append(frame.start.Attr,srlxml.Attr{Name:name,Value:text});return nil
}
// Keep typed-subgraph namespace prefixes under one owner so encoder aliases cannot shadow QName values.
func (w *srlXmlOutput) typeName(identity srlxml.Name)error{
  if len(w.stack)==0{return fmt.Errorf("Missing XML variant payload")};frame:=&w.stack[len(w.stack)-1]
  if frame.scoped{if !frame.pending{return fmt.Errorf("XML variant type follows content")};lexical:=identity.Local;if identity.Space!=""{prefix:="xml";if identity.Space!="http://www.w3.org/XML/1998/namespace"{prefix=w.extensionPrefix("srltype",identity.Space,nil)};if err:=w.extensionDeclare(prefix,identity.Space);err!=nil{return err};lexical=prefix+":"+lexical}else if uri,_:=w.identityNamespace("");uri!=""{return fmt.Errorf("Unqualified XML type conflicts with preserved default namespace")};return w.attribute(srlxml.Name{Space:"http://www.w3.org/2001/XMLSchema-instance",Local:"type"},lexical)}
  if !frame.pending||len(frame.start.Attr)!=0{return fmt.Errorf("XML variant type must precede payload attributes")};frame.manual=true
  lexical:=identity.Local
  if identity.Space==""&&frame.start.Name.Space!=""{
    uri:=frame.start.Name.Space;frame.start.Name=srlxml.Name{Local:"srlrecord:"+frame.start.Name.Local}
    if err:=w.attribute(srlxml.Name{Local:"xmlns:srlrecord"},uri);err!=nil{return err}
    if err:=w.attribute(srlxml.Name{Local:"xmlns"},"");err!=nil{return err};frame.inherited=""
  }else if identity.Space=="http://www.w3.org/XML/1998/namespace"{lexical="xml:"+lexical
  }else if identity.Space!=""{if err:=w.attribute(srlxml.Name{Local:"xmlns:srltype"},identity.Space);err!=nil{return err};lexical="srltype:"+lexical}
  return w.attribute(srlxml.Name{Space:"http://www.w3.org/2001/XMLSchema-instance",Local:"type"},lexical)
}
// Validate and escape a scalar directly into bounded output storage.
func (w *srlXmlOutput) text(text string)error { if err:=w.budget.textValue(text);err!=nil{return err};if err:=w.seal();err!=nil{return err};for _,identity:=range w.identities{if !identity.complete{if err:=identity.text(text);err!=nil{return err}}};return w.encoder.EncodeToken(srlxml.CharData(text)) }
// Complete one active element without retaining its finished contents.
func (w *srlXmlOutput) close()error { if len(w.stack)==0{return fmt.Errorf("Unexpected XML end")};if err:=w.seal();err!=nil{return err};last:=len(w.stack)-1;for _,identity:=range w.identities{if !identity.complete{if err:=identity.end(w.identityNamespace);err!=nil{return err}}};end:=w.stack[last].start.End();w.stack=w.stack[:last];return w.encoder.EncodeToken(end) }
// Publish bytes only after every generated element and output bound succeeds.
func (w *srlXmlOutput) finish()([]byte,error) { if len(w.stack)!=0||len(w.identities)!=0{return nil,fmt.Errorf("Incomplete XML output or identity scope")};if err:=w.encoder.Flush();err!=nil{return nil,err};return []byte(w.sink.text.String()),nil }
// Emit deterministic data XML without a private envelope or external resources.
func srlXmlWrite(node *srlXmlNode, limits XmlLimits)([]byte,error) {
  sink:=srlXmlSink{maximum:limits.MaxBytes}; output:=srlxml.NewEncoder(&sink)
  var visit func(*srlXmlNode,int,string)error
  visit=func(n *srlXmlNode,depth int,inheritedNamespace string)error {
    if depth>limits.MaxDepth { return srlXmlLimitError("XML depth limit exceeded") }
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
// Prepared instructions are lowered from the shared compiler's portable Unicode program.
type srlXmlPatternState struct { epsilon []int; ranges [][2]rune; next int; consumes,inverted,dot,accepting bool }
)serializer"
R"serializer(type srlXmlPattern struct { start int; states []srlXmlPatternState }
type srlXmlPatternWork struct { remaining int; budget *srlXmlBudget }
// Charge deterministic matching work before clearing storage or following an edge.
func (w *srlXmlPatternWork) spend(units int)error { if w.budget!=nil{if err:=w.budget.charge("work",int64(units));err!=nil{return err}};if units<0||units>w.remaining{return srlXmlLimitError("Portable XML regex work limit exceeded")};w.remaining-=units;return nil }
// Execute a prepared Thompson automaton without backtracking or host-regex semantics.
func (p *srlXmlPattern) matches(text string,work *srlXmlPatternWork)(bool,error){
  if len(text)>srlXmlPatternMaxInputBytes||len(p.states)>srlXmlPatternMaxStates{return false,srlXmlLimitError("Portable XML regex input/state limit exceeded")}
  if !utf8.ValidString(text){return false,fmt.Errorf("Invalid XML UTF-8")}
  visited:=make([]bool,len(p.states));active:=make([]int,0,len(p.states));next:=make([]int,0,len(p.states));stack:=make([]int,0,len(p.states))
  // Each epsilon closure visits a state once, including loops from nullable repeats.
  closure:=func(selected []int)([]int,error){
    if err:=work.spend(len(visited));err!=nil{return nil,err};clear(visited);stack=append(stack[:0],selected...);selected=selected[:0]
    for len(stack)!=0{if err:=work.spend(1);err!=nil{return nil,err};last:=len(stack)-1;index:=stack[last];stack=stack[:last];if index<0||index>=len(p.states){return nil,fmt.Errorf("Invalid portable XML regex state")};if visited[index]{continue};visited[index]=true;state:=&p.states[index];if state.consumes||state.accepting{selected=append(selected,index)};stack=append(stack,state.epsilon...)}
    return selected,nil
  }
  var err error;active=append(active,p.start);active,err=closure(active);if err!=nil{return false,err}
  for _,scalar:=range text{
    if err:=work.spend(1);err!=nil{return false,err};if !(scalar==9||scalar==10||scalar==13||scalar>=32&&scalar<=0xd7ff||scalar>=0xe000&&scalar<=0xfffd||scalar>=0x10000&&scalar<=0x10ffff){return false,fmt.Errorf("Invalid XML character")};next=next[:0]
    for _,index:=range active{if err:=work.spend(1);err!=nil{return false,err};state:=&p.states[index];if !state.consumes{continue};matched:=false
      if state.dot{matched=scalar!='\n'&&scalar!='\r'}else{for _,interval:=range state.ranges{if err:=work.spend(1);err!=nil{return false,err};matched=matched||scalar>=interval[0]&&scalar<=interval[1]};if state.inverted{matched=!matched}}
      if matched{next=append(next,state.next)}
    }
    next,err=closure(next);if err!=nil{return false,err};active,next=next,active
  }
  for _,index:=range active{if p.states[index].accepting{return true,nil}};return false,nil
}
// Intersect restriction groups; alternatives within one group share a bounded work allowance.
func srlXmlPatterns(text string,groups [][]srlXmlPattern)error{
  work:=srlXmlPatternWork{remaining:srlXmlPatternMaxWorkUnits}
  for _,group:=range groups{matched:=false;for index:=range group{accept,err:=group[index].matches(text,&work);if err!=nil{return err};if accept{matched=true;break}};if !matched{return fmt.Errorf("XML portable pattern facet failed")}}
  return nil
}
// Normalize XML Schema whitespace without treating other Unicode separators as XML whitespace.
func srlXmlWhitespace(text,policy string)string {
  if policy=="replace"||policy=="collapse"{text=strings.NewReplacer("\t"," ","\r"," ","\n"," ").Replace(text)}
  if policy=="collapse"{parts:=strings.Split(text," ");kept:=parts[:0];for _,part:=range parts{if part!=""{kept=append(kept,part)}};text=strings.Join(kept," ")};return text
}
// Compare scalar value spaces with exact native widths and unsigned 64-bit boundaries.
func srlXmlCompare(left,right,kind string,bits int)(int,bool,error){
  if kind=="float"||kind=="double"{a,err:=srlXmlDouble(left,bits);if err!=nil{return 0,false,err};b,err:=srlXmlDouble(right,bits);if err!=nil{return 0,false,err};if math.IsNaN(a)||math.IsNaN(b){return 0,math.IsNaN(a)&&math.IsNaN(b),nil};if a<b{return -1,true,nil};if a>b{return 1,true,nil};return 0,true,nil}
  if strings.HasPrefix(kind,"unsigned"){a,err:=srlXmlUint(left,bits);if err!=nil{return 0,false,err};b,err:=srlXmlUint(right,bits);if err!=nil{return 0,false,err};if a<b{return -1,true,nil};if a>b{return 1,true,nil};return 0,true,nil}
  if kind=="byte"||kind=="short"||kind=="int"||kind=="long"{a,err:=srlXmlInt(left,bits);if err!=nil{return 0,false,err};b,err:=srlXmlInt(right,bits);if err!=nil{return 0,false,err};if a<b{return -1,true,nil};if a>b{return 1,true,nil};return 0,true,nil}
  if kind=="boolean"{a,err:=srlXmlBool(left);if err!=nil{return 0,false,err};b,err:=srlXmlBool(right);if err!=nil{return 0,false,err};if a==b{return 0,true,nil};return 1,true,nil}
  return strings.Compare(left,right),true,nil
}
// Apply generated numeric, Unicode length, enumeration and whitespace facets to one scalar.
func srlXmlFacets(text,kind,whitespace string,bits int,bounds [4]string,length,minimum,maximum int,enumeration []string,finite bool)(string,error){
  text=srlXmlWhitespace(text,whitespace)
  count:=utf8.RuneCountInString(text);if length>=0&&count!=length||minimum>=0&&count<minimum||maximum>=0&&count>maximum{return "",fmt.Errorf("XML length facet failed")}
  for index,bound:=range bounds{if bound==""{continue};order,comparable,err:=srlXmlCompare(text,bound,kind,bits);if err!=nil{return "",err};if !comparable||index==0&&order<0||index==1&&order>0||index==2&&order<=0||index==3&&order>=0{return "",fmt.Errorf("XML numeric facet failed")}}
  if len(enumeration)!=0{matched:=false;for _,allowed:=range enumeration{order,comparable,err:=srlXmlCompare(text,srlXmlWhitespace(allowed,whitespace),kind,bits);if err!=nil{return "",err};if comparable&&order==0{matched=true;break}};if !matched{return "",fmt.Errorf("XML enumeration facet failed")}}
  if finite{value,err:=srlXmlDouble(text,bits);if err!=nil{return "",err};if math.IsNaN(value)||math.IsInf(value,0){return "",fmt.Errorf("XML finite facet failed")}}
  return text,nil
}
// Charge complete generated attributes before retaining their scalar contents.
func srlXmlAttribute(node *srlXmlNode,name srlxml.Name,text string,b *srlXmlBudget)error { if b.attrs>=b.limits.MaxAttributes{return srlXmlLimitError("XML attribute limit exceeded")};if err:=b.textValue(name.Space+name.Local+text);err!=nil{return err};b.attrs++;node.attrs=append(node.attrs,srlxml.Attr{Name:name,Value:text});return nil }
)serializer"
