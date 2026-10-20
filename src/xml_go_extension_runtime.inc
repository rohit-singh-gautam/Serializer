// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlextension(
// XmlExtensionLimits bounds foreign roots and retained semantic storage independently of document limits.
type XmlExtensionLimits struct{MaxSlots,MaxItems,MaxPreservedBytes int}
// DefaultXmlExtensionLimits derives ordinary admission from the shared extension contract.
func DefaultXmlExtensionLimits()XmlExtensionLimits{return XmlExtensionLimits{srlXmlExtensionMaxSlots,srlXmlExtensionMaxItems,srlXmlExtensionMaxPreservedBytes}}
// XmlExtensionOptions declares understood vocabulary separately from permission to retain opaque XML.
type XmlExtensionOptions struct{UnderstoodNamespaces []string;RequiredPrefixes string;Limits XmlExtensionLimits}
// DefaultXmlExtensionOptions does not claim understanding of any foreign vocabulary.
func DefaultXmlExtensionOptions()XmlExtensionOptions{return XmlExtensionOptions{nil,"",DefaultXmlExtensionLimits()}}
// Validate exact XML scalars without retaining an encoded copy of an owning text value.
func srlXmlExtensionSize(text string)(int,error){if !utf8.ValidString(text){return 0,fmt.Errorf("Invalid XML extension UTF8")};for _,value:=range text{if !(value==9||value==10||value==13||value>=32&&value<=0xd7ff||value>=0xe000&&value<=0xfffd||value>=0x10000&&value<=0x10ffff){return 0,fmt.Errorf("Invalid XML extension character")}};return len(text),nil}
// Apply the shared XML1.0 NCName ranges to namespace prefixes and opaque local names.
func srlXmlExtensionNcName(text string)bool{if text==""{return false};for offset,value:=range text{if !srlXmlNameCharacter(value,offset==0,false){return false}};return true}
// Own a bounded capability inventory once per complete operation, preserving exact zero policies.
func srlXmlExtensionOptions(value XmlExtensionOptions)(XmlExtensionOptions,error){
  limits:=value.Limits;if limits.MaxSlots<0||limits.MaxItems<0||limits.MaxPreservedBytes<0{return value,fmt.Errorf("Negative XML extension limit")};if len(value.UnderstoodNamespaces)>limits.MaxItems{return value,srlXmlLimitError("XML capability item limit")}
  bytes,err:=srlXmlExtensionSize(value.RequiredPrefixes);if err!=nil{return value,err};if bytes>srlXmlExtensionMaxPreservedBytes{return value,srlXmlLimitError("XML capability metadata byte limit")};for _,uri:=range value.UnderstoodNamespaces{size,err:=srlXmlExtensionSize(uri);if err!=nil{return value,err};if size>srlXmlExtensionMaxPreservedBytes-bytes{return value,srlXmlLimitError("XML capability metadata byte limit")};bytes+=size}
  value.UnderstoodNamespaces=append([]string(nil),value.UnderstoodNamespaces...);return value,nil
}
// Validate expanded semantic names before entering the token writer's trusted-name path.
func srlXmlExtensionName(name srlxml.Name)error{if !srlXmlExtensionNcName(name.Local)||name.Space=="http://www.w3.org/2000/xmlns/"{return fmt.Errorf("Invalid XML extension expanded name")};_,err:=srlXmlExtensionSize(name.Space);return err}
type srlXmlExtensionSlot struct{id string;attribute bool;position int;namespaces map[string]bool;policy,minimum,maximum int}
type srlXmlExtensionPosition struct{uri string;position int}
type srlXmlExtensionContract struct{core string;known map[srlxml.Name]bool;required []string;slots []srlXmlExtensionSlot;attributeSlots map[string]int;childSlots map[srlXmlExtensionPosition]int}
// Prepare exact slot lookup indexes once beside immutable compiler-owned metadata.
func (value srlXmlExtensionContract)prepared()srlXmlExtensionContract{value.attributeSlots=make(map[string]int);value.childSlots=make(map[srlXmlExtensionPosition]int);for index,slot:=range value.slots{for uri:=range slot.namespaces{if slot.attribute{value.attributeSlots[uri]=index}else{value.childSlots[srlXmlExtensionPosition{uri,slot.position}]=index}}};return value}
type srlXmlForeignNode struct{name srlxml.Name;attributes []srlxml.Attr;scope map[string]string;content []any}
type srlXmlForeignChild struct{position,item int;node *srlXmlForeignNode}
type srlXmlPreserved struct{scope map[string]string;attributes []srlxml.Attr;children []srlXmlForeignChild}
type srlXmlExtensions struct{meta *srlXmlExtensionContract;scope map[string]string;options XmlExtensionOptions;counts []int;items,bytes,cursor int;state *srlXmlPreserved}
const(srlXmlExtensionOwnerBytes=64;srlXmlExtensionNodeBytes=64;srlXmlExtensionScopeEntryBytes=64;srlXmlExtensionAttributeBytes=48;srlXmlExtensionContentEntryBytes=24)
// Check every partial measurement before arithmetic or allocation of corresponding retained storage.
func (value *srlXmlExtensions)add(first,second int)(int,error){if first<0||second<0||first>value.options.Limits.MaxPreservedBytes||second>value.options.Limits.MaxPreservedBytes-first{return 0,srlXmlLimitError("XML extension preserved-byte limit")};return first+second,nil}
// Admit logical bytes before retaining another owning node, map, text run or content cell.
func (value *srlXmlExtensions)charge(count int)error{next,err:=value.add(value.bytes,count);if err!=nil{return err};value.bytes=next;return nil}
// Validate complete retained namespace context, including unused opaque lexical QName bindings.
func (value *srlXmlExtensions)scopeBytes(bindings map[string]string)(int,error){size:=0;for prefix,uri:=range bindings{if !(prefix==""||srlXmlExtensionNcName(prefix))||prefix=="xmlns"||(prefix=="xml")!=(uri=="http://www.w3.org/XML/1998/namespace")||prefix!=""&&uri==""||uri=="http://www.w3.org/2000/xmlns/"{return 0,fmt.Errorf("Invalid preserved XML namespace binding")};first,err:=srlXmlExtensionSize(prefix);if err!=nil{return 0,err};second,err:=srlXmlExtensionSize(uri);if err!=nil{return 0,err};for _,count:=range []int{srlXmlExtensionScopeEntryBytes,first,second}{size,err=value.add(size,count);if err!=nil{return 0,err}}};return size,nil}
// Measure exact attribute identity and text without interpreting vocabulary-specific lexical values.
func (value *srlXmlExtensions)attributeBytes(attribute srlxml.Attr)(int,error){if err:=srlXmlExtensionName(attribute.Name);err!=nil{return 0,err};size:=srlXmlExtensionAttributeBytes;for _,text:=range []string{attribute.Name.Space,attribute.Name.Local,attribute.Value}{count,err:=srlXmlExtensionSize(text);if err!=nil{return 0,err};size,err=value.add(size,count);if err!=nil{return 0,err}};return size,nil}
// Require capability understanding before constructing a native candidate or opening an output head.
func srlXmlExtensionTracker(meta *srlXmlExtensionContract,scope map[string]string,options XmlExtensionOptions,state *srlXmlPreserved)(*srlXmlExtensions,error){
  bounds:=options.Limits;if len(meta.slots)>bounds.MaxSlots||len(meta.known)>bounds.MaxItems||len(meta.required)>bounds.MaxItems{return nil,srlXmlLimitError("XML extension slot limit")}
  ownerScope:=make(map[string]string,len(scope));for prefix,uri:=range scope{ownerScope[prefix]=uri};value:=&srlXmlExtensions{meta:meta,scope:ownerScope,options:options,counts:make([]int,len(meta.slots)),state:state};understood:=make(map[string]bool,len(options.UnderstoodNamespaces));for _,uri:=range options.UnderstoodNamespaces{understood[uri]=true};for _,uri:=range meta.required{if uri!=meta.core&&!understood[uri]{return nil,fmt.Errorf("Required XML capability is not understood")}}
  remaining:=options.RequiredPrefixes;required:=0;for{remaining=strings.TrimLeft(remaining," \t\r\n");if remaining==""{break};end:=strings.IndexAny(remaining," \t\r\n");prefix:=remaining;if end>=0{prefix=remaining[:end];remaining=remaining[end:]}else{remaining=""};if required>=bounds.MaxItems||!srlXmlExtensionNcName(prefix)||prefix=="xmlns"{return nil,fmt.Errorf("Invalid required XML capability prefix")};required++;uri:=scope[prefix];if uri==""{return nil,fmt.Errorf("Unbound required XML capability prefix")};if uri!=meta.core&&!understood[uri]{return nil,fmt.Errorf("Required XML capability is not understood")}}
  if state!=nil{size,err:=value.scopeBytes(scope);if err!=nil{return nil,err};size,err=value.add(srlXmlExtensionOwnerBytes,size);if err!=nil{return nil,err};if err=value.charge(size);err!=nil{return nil,err}};return value,nil
}
// Keep unqualified, core, known and reserved model names closed before consulting foreign slots.
func (value *srlXmlExtensions)slot(name srlxml.Name,attribute bool,position int)int{if name.Space==""||name.Space==value.meta.core||name.Space=="urn:serializer:xml:model"||value.meta.known[name]{return -1};var index int;var exists bool;if attribute{index,exists=value.meta.attributeSlots[name.Space]}else{index,exists=value.meta.childSlots[srlXmlExtensionPosition{name.Space,position}]};if exists{return index};return -1}
// Enforce policy and exact item/slot occurrences before retaining or skipping one foreign root.
func (value *srlXmlExtensions)admit(name srlxml.Name,attribute bool,position int)(bool,error){if err:=srlXmlExtensionName(name);err!=nil{return false,err};index:=value.slot(name,attribute,position);if index<0{return false,fmt.Errorf("XML extension has no permitted slot")};slot:=value.meta.slots[index];if slot.policy==srlXmlExtensionReject{return false,fmt.Errorf("XML extension slot rejects foreign vocabulary")};if value.items>=value.options.Limits.MaxItems{return false,srlXmlLimitError("XML extension item/occurrence limit")};if slot.maximum>=0&&value.counts[index]>=slot.maximum{return false,fmt.Errorf("XML extension item/occurrence limit")};value.items++;value.counts[index]++;return slot.policy==srlXmlExtensionPreserve,nil}
// Clone only a fully measured namespace scope; no preserved state is allocated for skip-only traversal.
func (value *srlXmlExtensions)storage()(*srlXmlPreserved,error){if value.state==nil{size,err:=value.scopeBytes(value.scope);if err!=nil{return nil,err};size,err=value.add(srlXmlExtensionOwnerBytes,size);if err!=nil{return nil,err};if err=value.charge(size);err!=nil{return nil,err};scope:=make(map[string]string,len(value.scope));for prefix,uri:=range value.scope{scope[prefix]=uri};value.state=&srlXmlPreserved{scope:scope}};return value.state,nil}
// Retain a foreign expanded attribute only after its preserve slot and byte charge are admitted.
func (value *srlXmlExtensions)attribute(attribute srlxml.Attr)error{preserve,err:=value.admit(attribute.Name,true,0);if err!=nil||!preserve{return err};size,err:=value.attributeBytes(attribute);if err!=nil{return err};if err=value.charge(size);err!=nil{return err};state,err:=value.storage();if err!=nil{return err};state.attributes=append(state.attributes,attribute);return nil}
// Measure every node, attribute and namespace entry before retaining or replaying its owning payload.
func (value *srlXmlExtensions)measure(name srlxml.Name,attributes []srlxml.Attr,bindings map[string]string)error{if err:=srlXmlExtensionName(name);err!=nil{return err};scope,err:=value.scopeBytes(bindings);if err!=nil{return err};size,err:=value.add(srlXmlExtensionNodeBytes,scope);if err!=nil{return err};for _,text:=range []string{name.Space,name.Local}{count,err:=srlXmlExtensionSize(text);if err!=nil{return err};size,err=value.add(size,count);if err!=nil{return err}};for _,attribute:=range attributes{count,err:=value.attributeBytes(attribute);if err!=nil{return err};size,err=value.add(size,count);if err!=nil{return err}};return value.charge(size)}
)xmlextension"
R"xmlextension(
// Choose a deterministic prefix without shadowing any preserved lexical QName binding.
func (output *srlXmlOutput)extensionPrefix(base,uri string,pinned map[string]string)string{for index:=0;;index++{prefix:=base;if index!=0{prefix+=strconv.Itoa(index)};bound,exists:=pinned[prefix];if !exists{bound,exists=output.identityNamespace(prefix)};if !exists||bound==uri{return prefix}}}
// Reuse a nonshadowed existing binding to stabilize opaque retained storage across repeated replay.
func (output *srlXmlOutput)extensionMatchingPrefix(uri string,pinned map[string]string,attribute bool)(string,bool){
  lookup:=func(prefix string)(string,bool){if value,exists:=pinned[prefix];exists{return value,true};return output.identityNamespace(prefix)}
  if !attribute{if value,exists:=lookup("");exists&&value==uri{return "",true}}
  if uri=="http://www.w3.org/XML/1998/namespace"{return "xml",true};keys:=make([]string,0,len(pinned));for key:=range pinned{keys=append(keys,key)};sort.Strings(keys);for _,prefix:=range keys{if (!attribute||prefix!="")&&pinned[prefix]==uri{return prefix,true}}
  for index:=len(output.stack)-1;index>=0;index--{frame:=&output.stack[index];for _,item:=range frame.start.Attr{prefix:="";if item.Name.Local=="xmlns"{}else if strings.HasPrefix(item.Name.Local,"xmlns:"){prefix=strings.TrimPrefix(item.Name.Local,"xmlns:")}else{continue};if (!attribute||prefix!="")&&item.Value==uri{if actual,exists:=lookup(prefix);exists&&actual==uri{return prefix,true}}}}
  return "",false
}
// Charge every emitted namespace declaration before retaining it on a pending head.
func (output *srlXmlOutput)extensionDeclare(prefix,uri string)error{if current,exists:=output.identityNamespace(prefix);exists&&current==uri{return nil};if output.budget.attrs>=output.budget.limits.MaxAttributes{return srlXmlLimitError("XML namespace limit exceeded")};if err:=output.budget.textValue(prefix+uri);err!=nil{return err};output.budget.attrs++;name:="xmlns";if prefix!=""{name+=":"+prefix};frame:=&output.stack[len(output.stack)-1];frame.start.Attr=append(frame.start.Attr,srlxml.Attr{Name:srlxml.Name{Local:name},Value:uri});return nil}
// Pin original in-scope bindings while keeping the ordinary writer's fast path unchanged.
func (output *srlXmlOutput)openScoped(name srlxml.Name,scope map[string]string)error{
  if len(output.stack)>=output.budget.limits.MaxDepth||output.budget.nodes>=output.budget.limits.MaxNodes{return srlXmlLimitError("XML node or depth limit exceeded")};if err:=output.budget.textValue(name.Space+name.Local);err!=nil{return err};if err:=output.seal();err!=nil{return err};output.budget.nodes++
  prefix:="";if name.Space!=""{var exists bool;prefix,exists=output.extensionMatchingPrefix(name.Space,scope,false);if !exists{prefix=output.extensionPrefix("srl",name.Space,scope)}};local:=name.Local;if prefix!=""{local=prefix+":"+local};output.stack=append(output.stack,srlXmlOutputFrame{start:srlxml.StartElement{Name:srlxml.Name{Local:local}},expanded:name,pending:true,manual:true,scoped:true})
  if scope!=nil{keys:=make([]string,0,len(scope));for key:=range scope{keys=append(keys,key)};sort.Strings(keys);for _,key:=range keys{if err:=output.extensionDeclare(key,scope[key]);err!=nil{return err}}}
  if name.Space==""{if current,_:=output.identityNamespace("");current!=""{return output.extensionDeclare("","")}}else{return output.extensionDeclare(prefix,name.Space)};return nil
}
)xmlextension"
R"xmlextension(
// Read one unfiltered token so significant foreign text is never mistaken for record indentation.
func srlXmlForeignEvent(input *srlXmlInput)(*srlXmlInputToken,error){if input.look!=nil{event:=input.look;input.look=nil;return event,nil};return input.read()}
// Capture only explicitly preserved semantic subtrees; skipped payload never acquires owning storage.
func (value *srlXmlExtensions)foreign(input *srlXmlInput,preserve bool)(*srlXmlForeignNode,error){event,err:=srlXmlForeignEvent(input);if err!=nil{return nil,err};if event.kind!='s'{return nil,fmt.Errorf("Missing foreign XML root")};var node *srlXmlForeignNode;var pending strings.Builder
  if preserve{if err=value.measure(event.name,event.attrs,input.namespaces);err!=nil{return nil,err};scope:=make(map[string]string,len(input.namespaces));for prefix,uri:=range input.namespaces{scope[prefix]=uri};node=&srlXmlForeignNode{name:event.name,attributes:append([]srlxml.Attr(nil),event.attrs...),scope:scope}}
  flush:=func()error{if preserve&&pending.Len()!=0{if err:=value.charge(srlXmlExtensionContentEntryBytes);err!=nil{return err};node.content=append(node.content,pending.String());pending=strings.Builder{}};return nil}
  for{event,err=srlXmlForeignEvent(input);if err!=nil{return nil,err};switch event.kind{case 't':if preserve{if err=value.charge(len(event.text));err!=nil{return nil,err};pending.WriteString(event.text)};case 's':if err=flush();err!=nil{return nil,err};input.look=event;child,err:=value.foreign(input,preserve);if err!=nil{return nil,err};if preserve{if err=value.charge(srlXmlExtensionContentEntryBytes);err!=nil{return nil,err};node.content=append(node.content,child)};case 'e':if err=flush();err!=nil{return nil,err};return node,nil;default:return nil,fmt.Errorf("Incomplete foreign XML subtree")}}
}
// Drain only foreign roots admitted at the exact native field and repeated-item boundary.
func (value *srlXmlExtensions)readAt(input *srlXmlInput,position,item int)error{for{event,err:=input.peek();if err!=nil{return err};if event.kind!='s'||value.slot(event.name,false,position)<0{return nil};preserve,err:=value.admit(event.name,false,position);if err!=nil{return err};node,err:=value.foreign(input,preserve);if err!=nil{return err};if node!=nil{if err=value.charge(srlXmlExtensionContentEntryBytes);err!=nil{return err};state,err:=value.storage();if err!=nil{return err};state.children=append(state.children,srlXmlForeignChild{position,item,node})}}}
// Require every explicit slot minimum before publishing a complete native owner.
func (value *srlXmlExtensions)finish()error{for index,count:=range value.counts{if count<value.meta.slots[index].minimum{return fmt.Errorf("Required XML extension occurrence missing")}};return nil}
// Emit retained attributes only while the complete known head remains pending.
func (value *srlXmlExtensions)emitAttributes(output *srlXmlOutput)error{if value.state!=nil{for _,attribute:=range value.state.attributes{preserve,err:=value.admit(attribute.Name,true,0);if err!=nil{return err};if !preserve{return fmt.Errorf("Retained XML attribute no longer has a preserve slot")};size,err:=value.attributeBytes(attribute);if err!=nil{return err};if err=value.charge(size);err!=nil{return err};if err=output.attribute(attribute.Name,attribute.Value);err!=nil{return err}}};return nil}
// Replay bounded opaque semantic nodes while known native models stay on their direct typed path.
func (value *srlXmlExtensions)emitNode(output *srlXmlOutput,node *srlXmlForeignNode)error{if node==nil{return fmt.Errorf("Missing retained XML extension node")};if err:=value.measure(node.name,node.attributes,node.scope);err!=nil{return err};if err:=output.openScoped(node.name,node.scope);err!=nil{return err};for _,attribute:=range node.attributes{if err:=output.attribute(attribute.Name,attribute.Value);err!=nil{return err}};for _,content:=range node.content{if err:=value.charge(srlXmlExtensionContentEntryBytes);err!=nil{return err};switch selected:=content.(type){case string:size,err:=srlXmlExtensionSize(selected);if err!=nil{return err};if err=value.charge(size);err!=nil{return err};if err=output.text(selected);err!=nil{return err};case *srlXmlForeignNode:if err:=value.emitNode(output,selected);err!=nil{return err};default:return fmt.Errorf("Invalid retained XML extension content")}};return output.close()}
// Preserve unknown-root order around the exact native repeated-item boundaries after ordinary edits.
func (value *srlXmlExtensions)emitAt(output *srlXmlOutput,position,item int)error{if value.state==nil{return nil};for value.cursor<len(value.state.children){child:=value.state.children[value.cursor];if child.position<position||child.position==position&&child.item<item{return fmt.Errorf("Preserved XML extension no longer fits edited field position")};if child.position!=position||child.item!=item{return nil};if child.node==nil{return fmt.Errorf("Missing retained XML extension node")};preserve,err:=value.admit(child.node.name,false,position);if err!=nil{return err};if !preserve{return fmt.Errorf("Retained XML child no longer has a preserve slot")};if err=value.charge(srlXmlExtensionContentEntryBytes);err!=nil{return err};if err=value.emitNode(output,child.node);err!=nil{return err};value.cursor++};return nil}
// Reject preserved positions lost when native repeated collections were shortened or removed.
func (value *srlXmlExtensions)endPosition(position int)error{if value.state!=nil&&value.cursor<len(value.state.children)&&value.state.children[value.cursor].position==position{return fmt.Errorf("Preserved XML extension no longer fits edited field position")};return nil}
// Require complete replay and every slot minimum before publishing complete owning output bytes.
func (value *srlXmlExtensions)finishOutput()error{if value.state!=nil&&value.cursor!=len(value.state.children){return fmt.Errorf("Unreplayed XML extension position")};return value.finish()}
)xmlextension"
