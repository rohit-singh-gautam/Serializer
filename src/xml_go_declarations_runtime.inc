// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"godecl(
// Legal pinned derivations select complete generated codecs, including their own nil attributes.
type srlXmlDeclarationType struct{name string;index,nilIndex int;abstract bool}
// A declaration owns only immutable names, flags and private codec indexes.
type srlXmlDeclaredValue struct{name,root srlxml.Name;attribute,nillable,abstract,fixed,abstractType bool;index,nilIndex int;declaredType string;types []srlXmlDeclarationType}
type srlXmlDeclarations struct{values []srlXmlDeclaredValue}
// Distinguish absent declarations from invalid declared values before temporary subtree capture.
func srlXmlDeclarationFind(inventory int,name srlxml.Name,attribute bool,budget *srlXmlBudget)(*srlXmlDeclaredValue,error){
 if inventory<0{return nil,nil};if inventory>=len(srlXmlDeclarationInventories){return nil,fmt.Errorf("Unknown XML declaration inventory")};values:=srlXmlDeclarationInventories[inventory].values
 if budget!=nil{if err:=budget.charge("work",int64(len(values)));err!=nil{return nil,err}};for index:=range values{if values[index].name==name&&values[index].attribute==attribute{return &values[index],nil}};return nil,nil
}
// Borrow the same native operation and independent policies, omitting all package-root authorizations.
type srlXmlDeclarationPolicy struct{limits XmlLimits;maps XmlMapLimits;mixed XmlMixedLimits;identity XmlIdentityLimits;digests XmlDigestLimits;extensions XmlExtensionOptions;operation func(string,int64,string)error;field string;temporary bool}
// Preserve operation field attribution and the validation marker through nested private codecs.
func srlXmlDeclarationInputPolicy(input *srlXmlInput)srlXmlDeclarationPolicy{return srlXmlDeclarationPolicy{limits:input.budget.limits,maps:input.mapLimits,mixed:input.mixedLimits,identity:input.identityLimits,digests:input.digestLimits,extensions:input.extensionOptions,operation:input.budget.operation,field:input.budget.field,temporary:input.declarationValidation}}
// Borrow output policy without copying any native model or retained XML state.
func srlXmlDeclarationOutputPolicy(output *srlXmlOutput)srlXmlDeclarationPolicy{return srlXmlDeclarationPolicy{limits:output.budget.limits,maps:output.mapLimits,mixed:output.mixedLimits,identity:output.identityLimits,digests:output.digestLimits,extensions:output.extensionOptions,operation:output.budget.operation,field:output.budget.field,temporary:output.declarationValidation}}
// Admit actual temporary owning storage before constructing a transport or private codec context.
func(p srlXmlDeclarationPolicy)charge(count int64)error{if p.operation!=nil{if err:=p.operation("work",count,p.field);err!=nil{return err};return p.operation("allocation",count,p.field)};return nil}
// Bound canonical aliases independently after the original XML has passed its complete logical policy.
func(p srlXmlDeclarationPolicy)transport()XmlLimits{const framingExpansion=16;result:=p.limits;maximum:=int(^uint(0)>>1);if result.MaxBytes>maximum/framingExpansion{result.MaxBytes=maximum}else{result.MaxBytes*=framingExpansion};if result.MaxTextBytes<result.MaxBytes{result.MaxTextBytes=result.MaxBytes};return result}
// Begin only a bounded XML workspace, with the original operation attached before any token visits.
func(p srlXmlDeclarationPolicy)writer()(*srlXmlOutput,error){if err:=p.charge(256);err!=nil{return nil,err};output:=srlXmlOutputStart(p.transport(),p.maps);output.budget.operation=p.operation;output.budget.field=p.field;output.mixedLimits=p.mixed;output.identityLimits=p.identity;output.digestLimits=p.digests;output.extensionOptions=p.extensions;output.declarationValidation=true;return output,nil}
// Parse the private transport without carrying expected package root/type/capability flags.
func(p srlXmlDeclarationPolicy)read(data []byte,inventory,index int,name srlxml.Name)error{
 if err:=p.charge(int64(len(data))+256);err!=nil{return err};input,err:=srlXmlInputStart(data,p.transport(),p.maps);if err!=nil{return err};input.budget.operation=p.operation;input.budget.field=p.field;input.mixedLimits=p.mixed;input.identityLimits=p.identity;input.digestLimits=p.digests;input.extensionOptions=p.extensions;input.declarationValidation=true
 if err=srlXmlDeclarationRead(inventory,index,input,name);err!=nil{return err};return input.finish()
}
// Replay immutable opaque nodes; root xsi:type/nil are removed only after declaration validation.
func srlXmlDeclarationEmit(output *srlXmlOutput,node *srlXmlForeignNode,strip bool)error{
 if node==nil{return fmt.Errorf("Missing declared wildcard node")};if err:=output.budget.charge("allocation",128);err!=nil{return err};if err:=output.openScoped(node.name,node.scope);err!=nil{return err}
 for _,attribute:=range node.attributes{if strip&&attribute.Name.Space=="http://www.w3.org/2001/XMLSchema-instance"&&(attribute.Name.Local=="type"||attribute.Name.Local=="nil"){continue};if err:=output.attribute(attribute.Name,attribute.Value);err!=nil{return err}}
 for _,content:=range node.content{if err:=output.budget.charge("work",1);err!=nil{return err};switch value:=content.(type){case string:if err:=output.text(value);err!=nil{return err};case *srlXmlForeignNode:if err:=srlXmlDeclarationEmit(output,value,false);err!=nil{return err};default:return fmt.Errorf("Invalid declared wildcard content")}};return output.close()
}
// Select only a statically legal expanded xsi:type; abstract or unknown targets never become lax fallback.
func(p srlXmlDeclarationPolicy)selected(declaration *srlXmlDeclaredValue,node *srlXmlForeignNode)(int,int,error){
 for _,attribute:=range node.attributes{if attribute.Name.Space!="http://www.w3.org/2001/XMLSchema-instance"||attribute.Name.Local!="type"{continue};lexical:=strings.Trim(attribute.Value," \t\r\n");prefix,local:="",lexical;if before,after,found:=strings.Cut(lexical,":");found{prefix,local=before,after;if !srlXmlExtensionNcName(prefix)||prefix=="xmlns"{return 0,0,fmt.Errorf("Invalid declared wildcard type prefix")}}
  if !srlXmlExtensionNcName(local){return 0,0,fmt.Errorf("Invalid declared wildcard type local name")};uri:=node.scope[prefix];if prefix=="xml"{uri="http://www.w3.org/XML/1998/namespace"};if prefix!=""&&uri==""{return 0,0,fmt.Errorf("Unbound declared wildcard type")};if err:=p.charge(int64(len(uri))+int64(len(local))+2);err!=nil{return 0,0,err};actual:=srlXmlIdentityName(srlxml.Name{Space:uri,Local:local})
  if actual!=declaration.declaredType{for _,target:=range declaration.types{if p.operation!=nil{if err:=p.operation("work",1,p.field);err!=nil{return 0,0,err}};if target.name==actual{if target.abstract{return 0,0,fmt.Errorf("Abstract declared wildcard type")};return target.index,target.nilIndex,nil}};return 0,0,fmt.Errorf("Declared wildcard xsi:type mismatch")};break
 };if declaration.abstractType{return 0,0,fmt.Errorf("Abstract wildcard declared type")};return declaration.index,declaration.nilIndex,nil
}
// Validate the complete original subtree through its private DTO before retaining or replaying it.
func(p srlXmlDeclarationPolicy)validate(inventory int,declaration *srlXmlDeclaredValue,node *srlXmlForeignNode)error{
 if declaration.abstract{return fmt.Errorf("Abstract wildcard declaration")};index,nilIndex,err:=p.selected(declaration,node);if err!=nil{return err};nilValue:=false
 for _,attribute:=range node.attributes{if attribute.Name.Space!="http://www.w3.org/2001/XMLSchema-instance"||attribute.Name.Local!="nil"{continue};if !declaration.nillable{return fmt.Errorf("Wildcard declaration is not nillable")};marker:=strings.Trim(attribute.Value," \t\r\n");if marker!="true"&&marker!="1"&&marker!="false"&&marker!="0"{return fmt.Errorf("Invalid wildcard xsi:nil")};nilValue=marker=="true"||marker=="1";if nilValue{if declaration.fixed||nilIndex<0{return fmt.Errorf("Fixed wildcard cannot be nil")};for _,content:=range node.content{if text,ok:=content.(string);!ok||text!=""{return fmt.Errorf("Nil wildcard has content")}}};break
 };if nilValue{index=nilIndex};output,err:=p.writer();if err!=nil{return err};if err=srlXmlDeclarationEmit(output,node,true);err!=nil{return err};data,err:=output.finish();if err!=nil{return err};return p.read(data,inventory,index,node.name)
}
// A namespaced synthetic wrapper preserves an unqualified attribute's original default QName scope.
func(p srlXmlDeclarationPolicy)attribute(inventory int,declaration *srlXmlDeclaredValue,value string,scope map[string]string)error{
 output,err:=p.writer();if err!=nil{return err};if err=output.openScoped(declaration.root,scope);err!=nil{return err};if err=output.attribute(declaration.name,value);err!=nil{return err};if err=output.close();err!=nil{return err};data,err:=output.finish();if err!=nil{return err};return p.read(data,inventory,declaration.index,declaration.root)
}
)godecl"
