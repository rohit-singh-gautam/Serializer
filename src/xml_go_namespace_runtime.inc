// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlnamespace(
// Bound the namespace-only preflight while borrowing the same unchanged native owner graph.
func (output *srlXmlOutput) reserveNamespaceDepth(depth int)error{if depth>=output.budget.limits.MaxDepth{return srlXmlLimitError("XML namespace reservation depth limit exceeded")};if err:=output.budget.charge("work",1);err!=nil{return err};if output.namespaceReserved==nil{if err:=output.budget.charge("allocation",128);err!=nil{return err};output.namespaceReserved=make(map[string]struct{})};return nil}
// Reserve lexical prefixes without copying native payload strings or constructing XML descendants.
func (output *srlXmlOutput) reserveNamespaceText(text string)error{
  if len(text)>output.budget.limits.MaxTextBytes{return srlXmlLimitError("XML namespace reservation text limit exceeded")};if err:=output.budget.charge("work",int64(len(text)));err!=nil{return err}
  start:=0;for end:=0;end<=len(text);end++{if end!=len(text)&&!srlXmlSpace(rune(text[end])){continue};token:=text[start:end];start=end+1;prefix,_,_:=strings.Cut(token,":");if !srlXmlExtensionNcName(prefix){continue};if _,exists:=output.namespaceReserved[prefix];exists{continue};if len(prefix)>output.budget.limits.MaxTextBytes-output.namespaceReservedBytes{return srlXmlLimitError("XML namespace reservation byte limit exceeded")};if err:=output.budget.charge("allocation",int64(128+len(prefix)));err!=nil{return err};output.namespaceReserved[prefix]=struct{}{};output.namespaceReservedBytes+=len(prefix)};return nil
}
// Plan the owner default only when clearing it would change the selected union member.
func (output *srlXmlOutput) namespacePreferredDefault(text string,scope map[string]string,rule *srlXmlRule)(string,bool,error){
  lookup:=func(prefix string)(string,bool){if prefix=="xml"{return "http://www.w3.org/XML/1998/namespace",true};uri,found:=scope[prefix];return uri,found||prefix==""}
  _,selected,err:=rule.selectNamespace(text,lookup,output.budget.limits.MaxNodes,&output.budget);if err!=nil{return "",false,err};if selected.kind=="QName"||selected.listKind=="QName"{if rule.keepNamespaceLexical{tokens,err:=srlXmlTokens(text,output.budget.limits.MaxNodes);if err!=nil{return "",false,err};for _,token:=range tokens{if !strings.Contains(token,":"){return scope[""],true,nil}}};return "",false,nil}
  empty:=func(prefix string)(string,bool){if prefix==""{return "",true};return lookup(prefix)}
  _,after,err:=rule.selectNamespace(text,empty,output.budget.limits.MaxNodes,&output.budget);if err!=nil{return "",false,err};return scope[""],after!=selected,nil
}
// Retain a selected string's original default vocabulary on its pending scalar tag.
func (output *srlXmlOutput) namespaceStringDefault(uri string)error{
  frame:=&output.stack[len(output.stack)-1];current,_:=output.identityNamespace("");if current==uri{return nil};if !frame.pending||frame.unqualifiedQName||frame.expanded.Space==""&&uri!=""{return fmt.Errorf("XML QName output would change selected string member")}
  if frame.expanded.Space!=""&&!strings.Contains(frame.start.Name.Local,":"){prefix:=output.extensionPrefix("srl",frame.expanded.Space,nil);frame.start.Name=srlxml.Name{Local:prefix+":"+frame.expanded.Local};if err:=output.extensionDeclare(prefix,frame.expanded.Space);err!=nil{return err}}
  for index:=range frame.start.Attr{attribute:=&frame.start.Attr[index];if attribute.Name.Space==""&&attribute.Name.Local=="xmlns"{return fmt.Errorf("XML QName output would change selected string member")}};return output.extensionDeclare("",uri)
}
// Resolve complete XML1.0 QName tokens before a provider namespace scope leaves the scalar.
func srlXmlNamespaceNames(text string,mode,maximum int,lookup func(string)(string,bool))([]srlxml.Name,error){
  if !utf8.ValidString(text){return nil,fmt.Errorf("Invalid XML QName UTF8")}
  tokens,err:=srlXmlTokens(text,maximum);if err!=nil{return nil,err};if mode==0&&len(tokens)!=1{return nil,fmt.Errorf("XML QName requires one name")}
  result:=make([]srlxml.Name,0,len(tokens));for _,token:=range tokens{
    prefix,local:="",token
    if mode==2{prefix=token;local="";if !srlXmlExtensionNcName(prefix)||prefix=="xmlns"{return nil,fmt.Errorf("Invalid XML namespace prefix")}}else{
      if before,after,found:=strings.Cut(token,":");found{prefix,local=before,after;if !srlXmlExtensionNcName(prefix)||prefix=="xmlns"{return nil,fmt.Errorf("Invalid XML QName prefix")}}
      if !srlXmlExtensionNcName(local){return nil,fmt.Errorf("Invalid XML QName local name")}
    }
    uri,found:=lookup(prefix);if prefix!=""&&(!found||uri==""){return nil,fmt.Errorf("Unbound XML QName prefix")};result=append(result,srlxml.Name{Space:uri,Local:local})
  };return result,nil
}
// Retain only referenced bindings, with cumulative admission before allocating each owning scope.
func (input *srlXmlInput) namespaceScope(text string,mode int,closed map[string]string,rules ...*srlXmlRule)(map[string]string,error){
  lookup:=func(prefix string)(string,bool){if uri,found:=closed[prefix];found{return uri,true};return input.identityNamespace(prefix)}
  if mode==3{if len(rules)==0{return nil,fmt.Errorf("Missing XML QName union rule")};_,selected,err:=rules[0].selectNamespace(text,lookup,input.budget.limits.MaxNodes,&input.budget);if err!=nil{return nil,err};if selected.listKind=="QName"{mode=1}else if selected.kind=="QName"{mode=0}else{mode=-1}}
  maximum:=input.budget.limits.MaxNodes;if mode<0{maximum=len(text)}else if _,err:=srlXmlNamespaceNames(text,mode,maximum,lookup);err!=nil{return nil,err}
  tokens,err:=srlXmlTokens(text,maximum);if err!=nil{return nil,err};scope:=map[string]string{}
  for _,token:=range tokens{prefix:="";if mode==2{prefix=token}else if before,_,found:=strings.Cut(token,":");found{prefix=before};if mode<0{local:=token;if before,after,found:=strings.Cut(token,":");found{if !srlXmlExtensionNcName(before)||before=="xmlns"{continue};local=after};if !srlXmlExtensionNcName(local){continue}};if _,exists:=scope[prefix];exists{continue};uri,_:=lookup(prefix)
    remaining:=input.budget.limits.MaxTextBytes-input.namespaceBytes;if len(prefix)>remaining||len(uri)>remaining-len(prefix){return nil,srlXmlLimitError("XML QName scope byte limit exceeded")};if err:=input.budget.charge("allocation",int64(128+len(prefix)+len(uri)));err!=nil{return nil,err};input.namespaceBytes+=len(prefix)+len(uri);scope[prefix]=uri
  };return scope,nil
}
// Preserve child-local overrides through its end token without copying complete inherited scopes.
func (input *srlXmlInput) namespaceText(mode int,rules ...*srlXmlRule)(string,map[string]string,error){
  input.captureNamespaces=true;defer func(){input.captureNamespaces=false}();var text strings.Builder
  for{event:=input.look;input.look=nil;if event==nil{var err error;event,err=input.read();if err!=nil{return "",nil,err}};if event.kind=='e'{lexical:=text.String();closed:=event.closed;if lexical==""&&len(rules)!=0&&rules[0].hasDefault{lexical=rules[0].defaultText;closed=rules[0].defaultScope};scope,err:=input.namespaceScope(lexical,mode,closed,rules...);return lexical,scope,err};if event.kind!='t'{return "",nil,fmt.Errorf("XML QName scalar has markup")};text.WriteString(event.text)}
}
// Reject scalar markup before interpreting QName values in their own active namespace scope.
func (input *srlXmlInput) namespaceScalar(name srlxml.Name,mode int,rules ...*srlXmlRule)(string,map[string]string,error){fields,err:=input.open(name);if err!=nil{return "",nil,err};if len(fields.attrs)!=0{return "",nil,fmt.Errorf("XML QName scalar has attributes")};return input.namespaceText(mode,rules...)}
// Regenerate declarations from expanded identity rather than reusing aliases from another scope.
func (output *srlXmlOutput) namespaceValue(text string,mode int,scope map[string]string,rules ...*srlXmlRule)(string,error){
  lookup:=func(prefix string)(string,bool){if prefix=="xml"{return "http://www.w3.org/XML/1998/namespace",true};uri,found:=scope[prefix];return uri,found||prefix==""}
  var selected *srlXmlRule
  if len(rules)!=0{checked,branch,err:=rules[0].selectNamespace(text,lookup,output.budget.limits.MaxNodes,&output.budget);if err!=nil{return "",err};text=checked;selected=branch}
  if mode==3{if selected==nil{return "",fmt.Errorf("Missing XML QName union rule")};if selected.listKind=="QName"{mode=1}else if selected.kind=="QName"{mode=0}else{_,after,err:=rules[0].selectNamespace(text,output.identityNamespace,output.budget.limits.MaxNodes,&output.budget);if err!=nil{return "",err};if after!=selected{if err=output.namespaceStringDefault(scope[""]);err!=nil{return "",err};_,after,err=rules[0].selectNamespace(text,output.identityNamespace,output.budget.limits.MaxNodes,&output.budget);if err!=nil{return "",err};if after!=selected{return "",fmt.Errorf("XML QName output would change selected string member")}};return text,nil}}
  names,err:=srlXmlNamespaceNames(text,mode,output.budget.limits.MaxNodes,lookup);if err!=nil{return "",err};var lexical strings.Builder
  if len(rules)!=0&&rules[0].defaultAttribute&&len(output.identities)==0{current,_:=output.identityNamespace("");empty:=false;for _,name:=range names{if name.Space==""{empty=true;break}};if current!=""&&empty&&len(names)==len(rules[0].defaultNames){matched:=true;for index,name:=range names{if err:=output.budget.charge("work",int64(len(name.Space)+len(name.Local)+1));err!=nil{return "",err};if name!=rules[0].defaultNames[index]{matched=false;break}};if matched{output.skipNamespaceAttribute=true;return text,nil}}}
  if len(rules)!=0&&rules[0].keepNamespaceLexical{
    tokens,err:=srlXmlTokens(text,output.budget.limits.MaxNodes);if err!=nil{return "",err}
    for index,token:=range tokens{prefix:="";if mode==2{prefix=token}else if before,_,found:=strings.Cut(token,":");found{prefix=before};if prefix==""{if err:=output.namespaceStringDefault(names[index].Space);err!=nil{return "",err};output.stack[len(output.stack)-1].unqualifiedQName=true}else if err:=output.extensionDeclare(prefix,names[index].Space);err!=nil{return "",err}}
    if _,after,err:=rules[0].selectNamespace(text,output.identityNamespace,output.budget.limits.MaxNodes,&output.budget);err!=nil{return "",err}else if after!=selected{return "",fmt.Errorf("XML QName output changed selected union member")};return text,nil
  }
  for _,name:=range names{prefix:="";if name.Space!=""{var exists bool;prefix,exists=output.extensionMatchingPrefix(name.Space,nil,true);if !exists{prefix=output.extensionPrefix("q",name.Space,nil)};if err=output.extensionDeclare(prefix,name.Space);err!=nil{return "",err}}else if mode==2{return "",fmt.Errorf("XML prefix list requires a namespace")}else if current,_:=output.identityNamespace("");current!=""{return "",fmt.Errorf("Unqualified XML QName conflicts with the output default namespace")}
    if name.Space==""{output.stack[len(output.stack)-1].unqualifiedQName=true};if lexical.Len()!=0{lexical.WriteByte(' ')};if prefix!=""{lexical.WriteString(prefix);if mode!=2{lexical.WriteByte(':')}};if mode!=2{lexical.WriteString(name.Local)}
  };result:=lexical.String();if selected!=nil{_,after,err:=rules[0].selectNamespace(result,output.identityNamespace,output.budget.limits.MaxNodes,&output.budget);if err!=nil{return "",err};if after!=selected{return "",fmt.Errorf("XML QName output changed selected union member")}};return result,nil
}
// Own explicit native-authoring bindings only after validating reserved names and XML characters.
func srlXmlNamespaceBindings(bindings map[string]string)(map[string]string,error){
  for prefix,uri:=range bindings{if !(prefix==""||srlXmlExtensionNcName(prefix))||prefix=="xmlns"||(prefix=="xml")!=(uri=="http://www.w3.org/XML/1998/namespace")||prefix!=""&&uri==""||uri=="http://www.w3.org/2000/xmlns/"{return nil,fmt.Errorf("Invalid XML QName namespace binding")};if _,err:=srlXmlExtensionSize(prefix);err!=nil{return nil,err};if _,err:=srlXmlExtensionSize(uri);err!=nil{return nil,err}}
  result:=make(map[string]string,len(bindings));for prefix,uri:=range bindings{result[prefix]=uri};return result,nil
}
)xmlnamespace"
