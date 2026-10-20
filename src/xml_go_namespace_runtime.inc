// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlnamespace(
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
func (input *srlXmlInput) namespaceScope(text string,mode int,closed map[string]string)(map[string]string,error){
  lookup:=func(prefix string)(string,bool){if uri,found:=closed[prefix];found{return uri,true};return input.identityNamespace(prefix)}
  if _,err:=srlXmlNamespaceNames(text,mode,input.budget.limits.MaxNodes,lookup);err!=nil{return nil,err}
  tokens,err:=srlXmlTokens(text,input.budget.limits.MaxNodes);if err!=nil{return nil,err};scope:=map[string]string{}
  for _,token:=range tokens{prefix:="";if mode==2{prefix=token}else if before,_,found:=strings.Cut(token,":");found{prefix=before};if _,exists:=scope[prefix];exists{continue};uri,_:=lookup(prefix)
    remaining:=input.budget.limits.MaxTextBytes-input.namespaceBytes;if len(prefix)>remaining||len(uri)>remaining-len(prefix){return nil,fmt.Errorf("XML QName scope byte limit exceeded")};input.namespaceBytes+=len(prefix)+len(uri);scope[prefix]=uri
  };return scope,nil
}
// Preserve child-local overrides through its end token without copying complete inherited scopes.
func (input *srlXmlInput) namespaceText(mode int)(string,map[string]string,error){
  input.captureNamespaces=true;defer func(){input.captureNamespaces=false}();var text strings.Builder
  for{event:=input.look;input.look=nil;if event==nil{var err error;event,err=input.read();if err!=nil{return "",nil,err}};if event.kind=='e'{scope,err:=input.namespaceScope(text.String(),mode,event.closed);return text.String(),scope,err};if event.kind!='t'{return "",nil,fmt.Errorf("XML QName scalar has markup")};text.WriteString(event.text)}
}
// Reject scalar markup before interpreting QName values in their own active namespace scope.
func (input *srlXmlInput) namespaceScalar(name srlxml.Name,mode int)(string,map[string]string,error){fields,err:=input.open(name);if err!=nil{return "",nil,err};if len(fields.attrs)!=0{return "",nil,fmt.Errorf("XML QName scalar has attributes")};return input.namespaceText(mode)}
// Regenerate declarations from expanded identity rather than reusing aliases from another scope.
func (output *srlXmlOutput) namespaceValue(text string,mode int,scope map[string]string)(string,error){
  lookup:=func(prefix string)(string,bool){if prefix=="xml"{return "http://www.w3.org/XML/1998/namespace",true};uri,found:=scope[prefix];return uri,found||prefix==""}
  names,err:=srlXmlNamespaceNames(text,mode,output.budget.limits.MaxNodes,lookup);if err!=nil{return "",err};var lexical strings.Builder
  for _,name:=range names{prefix:="";if name.Space!=""{var exists bool;prefix,exists=output.extensionMatchingPrefix(name.Space,nil,true);if !exists{prefix=output.extensionPrefix("q",name.Space,nil)};if err=output.extensionDeclare(prefix,name.Space);err!=nil{return "",err}}else if mode==2{return "",fmt.Errorf("XML prefix list requires a namespace")}else if current,_:=output.identityNamespace("");current!=""{return "",fmt.Errorf("Unqualified XML QName conflicts with the output default namespace")}
    if lexical.Len()!=0{lexical.WriteByte(' ')};if prefix!=""{lexical.WriteString(prefix);if mode!=2{lexical.WriteByte(':')}};if mode!=2{lexical.WriteString(name.Local)}
  };return lexical.String(),nil
}
// Own explicit native-authoring bindings only after validating reserved names and XML characters.
func srlXmlNamespaceBindings(bindings map[string]string)(map[string]string,error){
  for prefix,uri:=range bindings{if !(prefix==""||srlXmlExtensionNcName(prefix))||prefix=="xmlns"||(prefix=="xml")!=(uri=="http://www.w3.org/XML/1998/namespace")||prefix!=""&&uri==""||uri=="http://www.w3.org/2000/xmlns/"{return nil,fmt.Errorf("Invalid XML QName namespace binding")};if _,err:=srlXmlExtensionSize(prefix);err!=nil{return nil,err};if _,err:=srlXmlExtensionSize(uri);err!=nil{return nil,err}}
  result:=make(map[string]string,len(bindings));for prefix,uri:=range bindings{result[prefix]=uri};return result,nil
}
)xmlnamespace"
