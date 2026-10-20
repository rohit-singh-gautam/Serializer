// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"identitygo(
// XmlIdentityLimits bounds one owner-local key/unique/keyref index independently of ordinary XML.
type XmlIdentityLimits struct {MaxConstraints,MaxFields,MaxDepth,MaxSelectedNodes,MaxTuples,MaxTupleBytes,MaxWorkUnits int}
// DefaultXmlIdentityLimits derives immutable operation defaults from the shared identity contract.
func DefaultXmlIdentityLimits()XmlIdentityLimits{return XmlIdentityLimits{srlXmlIdentityMaxConstraints,srlXmlIdentityMaxFields,srlXmlIdentityMaxDepth,srlXmlIdentityMaxSelectedNodes,srlXmlIdentityMaxTuples,srlXmlIdentityMaxTupleBytes,srlXmlIdentityMaxWorkUnits}}
// Reject negative policies before touching source bytes or native values; zero is exact admission.
func srlXmlIdentityPolicy(limits XmlIdentityLimits)error{if limits.MaxConstraints<0||limits.MaxFields<0||limits.MaxDepth<0||limits.MaxSelectedNodes<0||limits.MaxTuples<0||limits.MaxTupleBytes<0||limits.MaxWorkUnits<0{return fmt.Errorf("Negative XML identity limit")};return nil}
type srlXmlIdentityPath struct{steps []string;descendants bool;attribute string}
type srlXmlIdentityField struct{path srlXmlIdentityPath;kind string;nillable bool}
type srlXmlIdentityConstraint struct{kind string;selector srlXmlIdentityPath;fields []srlXmlIdentityField;refer int}
type srlXmlIdentityScope struct{constraints []srlXmlIdentityConstraint}
type srlXmlIdentityCapture struct{depth,bytes int;nilValue bool;text strings.Builder}
type srlXmlIdentityCandidate struct{constraint,depth,bytes int;values []string;seen []bool;captures []*srlXmlIdentityCapture}
type srlXmlIdentity struct{scope *srlXmlIdentityScope;limits XmlIdentityLimits;path []string;active []*srlXmlIdentityCandidate;tables []map[string]bool;selected,tuples,tupleBytes,workUnits int;complete,failed bool;operation func(string,int64,string)error;packageField string}
// Convert one exact provider/compiler expanded name to the immutable path identity spelling.
func srlXmlIdentityName(name srlxml.Name)string{if name.Space==""{return name.Local};return "{"+name.Space+"}"+name.Local}
// Borrow the active namespace scope synchronously; captures resolve before its end token leaves scope.
func (r *srlXmlInput) identityNamespace(prefix string)(string,bool){value,exists:=r.namespaces[prefix];return value,exists}
// Retain only current start-tag attributes for synchronous observers.
func srlXmlIdentityAttributes(attrs []srlxml.Attr)map[string]string{result:=make(map[string]string,len(attrs));for _,attribute:=range attrs{result[srlXmlIdentityName(attribute.Name)]=attribute.Value};return result}
// Attach one nested owner before its current start event advances into descendants.
func (r *srlXmlInput) identityScope(scope *srlXmlIdentityScope)(*srlXmlIdentity,error){event,err:=r.peek();if err!=nil{return nil,err};if event.kind!='s'{return nil,fmt.Errorf("Missing XML identity owner")};result,err:=srlXmlIdentityStart(scope,r.identityLimits,r.budget.operation,r.budget.field);if err!=nil{return nil,err};if err=result.start(srlXmlIdentityName(event.name),srlXmlIdentityAttributes(event.attrs),r.identityNamespace);err!=nil{return nil,err};r.identities=append(r.identities,result);return result,nil}
// Resolve the completed owner index and release every retained tuple reference.
func (r *srlXmlInput) identityFinish(identity *srlXmlIdentity)error{if err:=identity.finish();err!=nil{return err};for index,item:=range r.identities{if item==identity{copy(r.identities[index:],r.identities[index+1:]);r.identities[len(r.identities)-1]=nil;r.identities=r.identities[:len(r.identities)-1];return nil}};return fmt.Errorf("Unknown XML identity scope")}
// Deliver provider events before namespace restoration, never retaining descendant XML.
func (r *srlXmlInput) identityEvent(event *srlXmlInputToken)error{if len(r.identities)==0{return nil};var attributes map[string]string;if event.kind=='s'{attributes=srlXmlIdentityAttributes(event.attrs)};for _,identity:=range r.identities{if identity.complete{continue};var err error;switch event.kind{case 's':err=identity.start(srlXmlIdentityName(event.name),attributes,r.identityNamespace);case 't':err=identity.text(event.text);case 'e':err=identity.end(r.identityNamespace)};if err!=nil{return err}};return nil}
// Attach metadata after its owner start tag is pending and before native attributes are added.
func (w *srlXmlOutput) identityScope(scope *srlXmlIdentityScope)(*srlXmlIdentity,error){result,err:=srlXmlIdentityStart(scope,w.identityLimits,w.budget.operation,w.budget.field);if err!=nil{return nil,err};w.identities=append(w.identities,result);return result,nil}
// Resolve only namespace declarations actually produced by this direct writer.
func (w *srlXmlOutput) identityNamespace(prefix string)(string,bool){if prefix=="xml"{return "http://www.w3.org/XML/1998/namespace",true};for index:=len(w.stack)-1;index>=0;index--{frame:=&w.stack[index];for item:=len(frame.start.Attr)-1;item>=0;item--{attribute:=frame.start.Attr[item];if attribute.Name.Space==""&&(prefix==""&&attribute.Name.Local=="xmlns"||prefix!=""&&attribute.Name.Local=="xmlns:"+prefix){return attribute.Value,true}};if prefix==""&&frame.start.Name.Space!=""{return frame.start.Name.Space,true}};return "",prefix==""}
// Observe a pending tag only after every native attribute has passed validation.
func (w *srlXmlOutput) identityStart(frame *srlXmlOutputFrame)error{for _,identity:=range w.identities{if !identity.complete{if err:=identity.start(srlXmlIdentityName(frame.expanded),frame.identityAttributes,w.identityNamespace);err!=nil{return err}}};return nil}
// Resolve local references and release all private tables before returning an owning encoding.
func (w *srlXmlOutput) identityFinish(identity *srlXmlIdentity)error{if err:=identity.finish();err!=nil{return err};for index,item:=range w.identities{if item==identity{copy(w.identities[index:],w.identities[index+1:]);w.identities[len(w.identities)-1]=nil;w.identities=w.identities[:len(w.identities)-1];return nil}};return fmt.Errorf("Unknown XML identity scope")}
// Allocate checked immutable constraint tables for one independent owner scope.
func srlXmlIdentityStart(scope *srlXmlIdentityScope,limits XmlIdentityLimits,operation func(string,int64,string)error,field string)(*srlXmlIdentity,error){
  if len(scope.constraints)>limits.MaxConstraints{return nil,srlXmlLimitError("XML identity constraint limit")};result:=&srlXmlIdentity{scope:scope,limits:limits,operation:operation,packageField:field,tables:make([]map[string]bool,len(scope.constraints))}
  for index,item:=range scope.constraints{if len(item.fields)==0||len(item.fields)>limits.MaxFields{return nil,srlXmlLimitError("XML identity field limit")};result.tables[index]=make(map[string]bool)};return result,nil
}
// Charge selector and tuple work before retaining or comparing another value.
func (r *srlXmlIdentity) work(count int)error{if r.operation!=nil{if err:=r.operation("work",int64(count),r.packageField);err!=nil{return err}};if count>r.limits.MaxWorkUnits-r.workUnits{return srlXmlLimitError("XML identity work limit")};r.workUnits+=count;return nil}
// Match compiler-owned expanded paths directly without suffix arrays or XPath interpretation.
func (r *srlXmlIdentity) matches(first int,expected srlXmlIdentityPath)(bool,error){
  count:=len(r.path)-first;if err:=r.work(count+len(expected.steps)+1);err!=nil{return false,err};if expected.descendants{if count<len(expected.steps){return false,nil};first=len(r.path)-len(expected.steps)}else if count!=len(expected.steps){return false,nil}
  for index,wanted:=range expected.steps{actual:=r.path[first+index];size:=len(actual);if len(wanted)<size{size=len(wanted)};if err:=r.work(size+1);err!=nil{return false,err};if actual!=wanted{return false,nil}};return true,nil
}
// Canonicalize arbitrary decimal digits without host integer widths or floating rounding.
func srlXmlIdentityDecimal(value string)string{negative:=strings.HasPrefix(value,"-");if negative||strings.HasPrefix(value,"+"){value=value[1:]};whole,fraction,found:=strings.Cut(value,".");if !found{fraction=""};whole=strings.TrimLeft(whole,"0");fraction=strings.TrimRight(fraction,"0");if whole==""{whole="0"};if whole=="0"&&fraction==""{negative=false};if fraction!=""{whole+="."+fraction};if negative{return "-"+whole};return whole}
// Compare typed value spaces rather than lexical prefixes, numeric aliases or native widths.
func (r *srlXmlIdentity) key(lexical,kind string,resolve func(string)(string,bool))(string,error){
  if len(lexical)>r.limits.MaxTupleBytes{return "",srlXmlLimitError("XML identity field-byte limit")};if err:=r.work(len(lexical)+1);err!=nil{return "",err}
  whitespace:="collapse";if kind=="string"{whitespace="preserve"}else if kind=="normalizedString"{whitespace="replace"}
  if kind=="QName"{
    value:=strings.Trim(lexical," \r\n\t");parts:=strings.Split(value,":");prefix,local:="",value
    if len(parts)==2&&parts[0]!=""&&parts[1]!=""{prefix,local=parts[0],parts[1]}else if len(parts)!=1||value==""{return "",fmt.Errorf("Invalid XML identity QName")}
    rule:=srlXmlRule{kind:"NCName",whitespace:"collapse",length:-1,minimum:-1,maximum:-1};if _,err:=rule.check(local,0);err!=nil{return "",err};if prefix!=""{if _,err:=rule.check(prefix,0);err!=nil{return "",err}};uri,exists:=resolve(prefix);if prefix!=""&&!exists{return "",fmt.Errorf("Undeclared XML identity QName prefix")};return "q"+uri+"\x00"+local,nil
  }
  rule:=srlXmlRule{kind:kind,whitespace:whitespace,length:-1,minimum:-1,maximum:-1};value,err:=rule.check(lexical,0);if err!=nil{return "",err}
  if kind=="boolean"{selected,err:=srlXmlBool(value);if err!=nil{return "",err};if selected{return "btrue",nil};return "bfalse",nil}
  if kind=="decimal"||srlXmlIntegerKind(kind)||srlXmlKindBits(kind)!=0&&kind!="float"&&kind!="double"{return "d"+srlXmlIdentityDecimal(value),nil};if kind=="float"||kind=="double"{return "",fmt.Errorf("Floating XML identity fields are unsupported")};if kind=="anyURI"{return "u"+value,nil};return "s"+value,nil
}
// Bound every partial canonical tuple before retaining the next selected scalar.
func (r *srlXmlIdentity) store(candidate *srlXmlIdentityCandidate,index int,value string)error{count:=len(value)-1;if strings.HasPrefix(value,"q"){count--};if count>r.limits.MaxTupleBytes-candidate.bytes{return srlXmlLimitError("XML identity candidate tuple-byte limit")};candidate.bytes+=count;candidate.values[index]=value;return nil}
// Observe a complete current start tag, while attributes retain their original namespace scope.
func (r *srlXmlIdentity) start(name string,attributes map[string]string,resolve func(string)(string,bool))(failure error){
  defer func(){if failure!=nil{r.failed=true}}();if r.complete||r.failed{return fmt.Errorf("XML identity observer inactive")};if failure=r.work(len(name)+1);failure!=nil{return failure};if len(r.path)>=r.limits.MaxDepth{return srlXmlLimitError("XML identity depth limit")}
  for _,candidate:=range r.active{for _,capture:=range candidate.captures{if capture!=nil{return fmt.Errorf("XML identity scalar has complex child content")}}};r.path=append(r.path,name)
  for index,constraint:=range r.scope.constraints{matches,err:=r.matches(1,constraint.selector);if err!=nil{return err};if matches{if r.selected>=r.limits.MaxSelectedNodes{return srlXmlLimitError("XML identity selected-node limit")};r.selected++;count:=len(constraint.fields);r.active=append(r.active,&srlXmlIdentityCandidate{constraint:index,depth:len(r.path),values:make([]string,count),seen:make([]bool,count),captures:make([]*srlXmlIdentityCapture,count)})}}
  for _,candidate:=range r.active{constraint:=r.scope.constraints[candidate.constraint];for index,field:=range constraint.fields{matches,err:=r.matches(candidate.depth,field.path);if err!=nil{return err};attribute,exists:=attributes[field.path.attribute];if !matches||field.path.attribute!=""&&!exists{continue};if constraint.kind=="key"&&field.nillable{return fmt.Errorf("XSD key field selects an element declared nillable")};if candidate.seen[index]{return fmt.Errorf("XML identity field selects multiple values")};candidate.seen[index]=true
    if field.path.attribute!=""{value,err:=r.key(attribute,field.kind,resolve);if err!=nil{return err};if failure=r.store(candidate,index,value);failure!=nil{return failure}}else{marker,exists:=attributes["{http://www.w3.org/2001/XMLSchema-instance}nil"];nilText:=strings.Trim(marker," \r\n\t");if !exists{nilText="false"};if nilText!="true"&&nilText!="1"&&nilText!="false"&&nilText!="0"{return fmt.Errorf("Malformed XML identity nil")};candidate.captures[index]=&srlXmlIdentityCapture{depth:len(r.path),nilValue:nilText=="true"||nilText=="1"}}
  }};return nil
}
// Charge logical UTF8 text independently of parser chunk, entity and CDATA boundaries.
func (r *srlXmlIdentity) text(value string)(failure error){
  defer func(){if failure!=nil{r.failed=true}}();if r.complete||r.failed||len(r.path)==0{return fmt.Errorf("XML identity observer inactive")};if failure=r.work(len(value));failure!=nil{return failure}
  for _,candidate:=range r.active{for _,capture:=range candidate.captures{if capture==nil{continue};if failure=r.work(len(value));failure!=nil{return failure};if capture.nilValue&&value!=""{return fmt.Errorf("Nil XML identity scalar has text")};if len(value)>r.limits.MaxTupleBytes-capture.bytes{return srlXmlLimitError("XML identity field-byte limit")};capture.bytes+=len(value);capture.text.WriteString(value)}};return nil
}
)identitygo"
R"identitygo(
// Close captures before leaving their namespace scope and index only complete checked tuples.
func (r *srlXmlIdentity) end(resolve func(string)(string,bool))(failure error){
  defer func(){if failure!=nil{r.failed=true}}();if r.complete||r.failed||len(r.path)==0{return fmt.Errorf("Unbalanced XML identity end")};if failure=r.work(1);failure!=nil{return failure}
  for _,candidate:=range r.active{constraint:=r.scope.constraints[candidate.constraint];for index,capture:=range candidate.captures{if failure=r.work(1);failure!=nil{return failure};if capture!=nil&&capture.depth==len(r.path){if !capture.nilValue{value,err:=r.key(capture.text.String(),constraint.fields[index].kind,resolve);if err!=nil{return err};if failure=r.store(candidate,index,value);failure!=nil{return failure}};candidate.captures[index]=nil}}}
  retained:=r.active[:0];for _,candidate:=range r.active{if candidate.depth!=len(r.path){retained=append(retained,candidate);continue};constraint:=r.scope.constraints[candidate.constraint];missing:=false;for _,value:=range candidate.values{if value==""{missing=true}}
    if missing{if constraint.kind=="key"{return fmt.Errorf("XSD key tuple is incomplete or nil")};continue};if r.tuples>=r.limits.MaxTuples{return srlXmlLimitError("XML identity tuple-count limit")};if candidate.bytes>r.limits.MaxTupleBytes-r.tupleBytes{return srlXmlLimitError("XML identity tuple-byte limit")};if r.operation!=nil{if failure=r.operation("reference",1,r.packageField);failure!=nil{return failure};if failure=r.operation("allocation",int64(128+candidate.bytes),r.packageField);failure!=nil{return failure}};r.tuples++;r.tupleBytes+=candidate.bytes;if failure=r.work(candidate.bytes+1);failure!=nil{return failure}
    var tuple strings.Builder;for _,value:=range candidate.values{tuple.WriteString(strconv.Itoa(len(value)));tuple.WriteByte(':');tuple.WriteString(value)};key:=tuple.String();if r.tables[candidate.constraint][key]&&constraint.kind!="keyref"{return fmt.Errorf("Duplicate XSD key/unique tuple")};r.tables[candidate.constraint][key]=true
  };for index:=len(retained);index<len(r.active);index++{r.active[index]=nil};r.active=retained;r.path=r.path[:len(r.path)-1];if len(r.path)==0{r.complete=true};return nil
}
// Resolve complete forward references only after all owner-local target tables exist.
func (r *srlXmlIdentity) finish()(failure error){
  defer func(){if failure!=nil{r.failed=true}}();if r.failed||!r.complete||len(r.path)!=0||len(r.active)!=0{return fmt.Errorf("Incomplete or failed XML identity scope")};if failure=r.work(1);failure!=nil{return failure}
  for index,constraint:=range r.scope.constraints{if constraint.kind!="keyref"{continue};target:=r.tables[constraint.refer];for tuple:=range r.tables[index]{if failure=r.work(len(tuple)+1);failure!=nil{return failure};if !target[tuple]{return fmt.Errorf("Unresolved XSD keyref tuple")}}};return nil
}
)identitygo"
