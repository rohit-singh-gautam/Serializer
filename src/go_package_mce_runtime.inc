// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
type srlPackageCompatibilityScope struct {
  parent *srlPackageCompatibilityScope
  ignorable map[string]bool
  process map[srlPackageXML.Name]bool
}
type srlPackageCompatibility struct { tree *srlPackageMetadataTree }

// tokens splits only XML whitespace after admitting retained compatibility token estimates.
func (compatibility *srlPackageCompatibility) tokens(value string) ([]string,error) {
  tree:=compatibility.tree
  if err:=tree.state.consume(SerializerPackageWorkUnits,int64(len(value))+1,tree.part);err!=nil { return nil,err }
  if err:=tree.reserve(int64(len(value))*8);err!=nil { return nil,err }
  return srlPackageStrings.FieldsFunc(value,func(item rune)bool{return item==' '||item=='\t'||item=='\r'||item=='\n'}),nil
}
// prefix resolves the original element's scope; later rebinding never changes accumulated capability identity.
func (compatibility *srlPackageCompatibility) prefix(node *srlPackageNode,value string) (string,error) {
  uri,found:=node.namespaces[value]
  if !srlPackageNCName(value)||!found||uri==""||uri==srlPackageCompatibilityNamespace { return "",srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid compatibility namespace prefix",compatibility.tree.part) };return uri,nil
}
// ignorable searches immutable parent scopes without copying every accumulated URI.
func (compatibility *srlPackageCompatibility) ignorable(scope *srlPackageCompatibilityScope,uri string) (bool,error) {
  for ;scope!=nil;scope=scope.parent {
    if err:=compatibility.tree.state.consume(SerializerPackageWorkUnits,1,compatibility.tree.part);err!=nil { return false,err }
    if scope.ignorable[uri] { return true,nil }
  }
  return false,nil
}
// process matches expanded ProcessContent names or namespace wildcards in original declaration scopes.
func (compatibility *srlPackageCompatibility) process(scope *srlPackageCompatibilityScope,name srlPackageXML.Name) (bool,error) {
  for ;scope!=nil;scope=scope.parent {
    if err:=compatibility.tree.state.consume(SerializerPackageWorkUnits,1,compatibility.tree.part);err!=nil { return false,err }
    if scope.process[name]||scope.process[srlPackageXML.Name{Space:name.Space,Local:"*"}] { return true,nil }
  }
  return false,nil
}
// declarations validates compatibility attributes even on wrappers whose content is not selected.
func (compatibility *srlPackageCompatibility) declarations(node *srlPackageNode,parent *srlPackageCompatibilityScope) (*srlPackageCompatibilityScope,error) {
  tree:=compatibility.tree
  if err:=tree.reserve(128);err!=nil { return nil,err }
  scope:=&srlPackageCompatibilityScope{parent:parent,ignorable:map[string]bool{},process:map[srlPackageXML.Name]bool{}}
  if value,found:=node.attributes[srlPackageXML.Name{Space:srlPackageCompatibilityNamespace,Local:"Ignorable"}];found {
    tokens,err:=compatibility.tokens(value);if err!=nil { return nil,err }
    for _,token:=range tokens { uri,err:=compatibility.prefix(node,token);if err!=nil { return nil,err };scope.ignorable[uri]=true }
  }
  for name,value:=range node.attributes {
    if name.Space!=srlPackageCompatibilityNamespace { continue }
    switch name.Local {
    case "ProcessContent":
      tokens,err:=compatibility.tokens(value);if err!=nil { return nil,err }
      for _,token:=range tokens {
        colon:=srlPackageStrings.IndexByte(token,':')
        if colon<1||colon!=srlPackageStrings.LastIndexByte(token,':')||!(token[colon+1:]=="*"||srlPackageNCName(token[colon+1:])) { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid ProcessContent name",tree.part) }
        uri,err:=compatibility.prefix(node,token[:colon]);if err!=nil { return nil,err }
        allowed,err:=compatibility.ignorable(scope,uri);if err!=nil { return nil,err };if !allowed { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"ProcessContent namespace is not ignorable",tree.part) }
        scope.process[srlPackageXML.Name{Space:uri,Local:token[colon+1:]}]=true
      }
    case "MustUnderstand":
      tokens,err:=compatibility.tokens(value);if err!=nil { return nil,err };for _,token:=range tokens { if _,err:=compatibility.prefix(node,token);err!=nil { return nil,err } }
    case "Ignorable":
    default:return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Unknown compatibility attribute",tree.part)
    }
  }
  return scope,nil
}
// must checks only retained, selected or explicitly unwrapped content against the selected application profile.
func (compatibility *srlPackageCompatibility) must(node *srlPackageNode) error {
  if value,found:=node.attributes[srlPackageXML.Name{Space:srlPackageCompatibilityNamespace,Local:"MustUnderstand"}];found {
    tokens,err:=compatibility.tokens(value);if err!=nil { return err }
    for _,token:=range tokens { uri,err:=compatibility.prefix(node,token);if err!=nil { return err };if uri!=srlPackageRelationshipNamespace { return srlPackageFailure(SerializerPackageInvalidMetadata,"Unsupported required metadata namespace",compatibility.tree.part) } }
  }
  return nil
}
// wrapper checks complete Choice/Fallback attributes before selecting the first understood branch.
func (compatibility *srlPackageCompatibility) wrapper(node *srlPackageNode,scope *srlPackageCompatibilityScope,choice bool) (bool,error) {
  requires:="";found:=false
  for name,value:=range node.attributes {
    if name.Space=="" { if !choice||name.Local!="Requires" { return false,srlPackageFailure(SerializerPackageInvalidMetadata,"Unexpected compatibility wrapper attribute",compatibility.tree.part) };requires=value;found=true } else if name.Space!=srlPackageCompatibilityNamespace {
      ignored,err:=compatibility.ignorable(scope,name.Space);if err!=nil { return false,err }
      if name.Space==srlPackageXMLNamespace||!ignored { return false,srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid qualified wrapper attribute",compatibility.tree.part) }
    }
  }
  if !choice { return false,nil }
  if !found||srlPackageStrings.Trim(requires," \t\r\n")=="" { return false,srlPackageFailure(SerializerPackageInvalidMetadata,"Choice requires namespace prefixes",compatibility.tree.part) }
  supported:=true;tokens,err:=compatibility.tokens(requires);if err!=nil { return false,err }
  for _,token:=range tokens { uri,err:=compatibility.prefix(node,token);if err!=nil { return false,err };if uri!=srlPackageRelationshipNamespace { supported=false } };return supported,nil
}
// children flattens selected wrappers while retaining text that violates OPC element-only grammar.
func (compatibility *srlPackageCompatibility) children(node *srlPackageNode,scope *srlPackageCompatibilityScope) error {
  tree:=compatibility.tree
  // Pointer slots use a conservative logical descriptor estimate, independent of Go heap layout.
  const childSlotBytes=int64(16)
  if err:=tree.reserve(int64(node.text.Len()));err!=nil { return err }
  result:=[]*srlPackageNode{};var text srlPackageStrings.Builder;text.WriteString(node.text.String())
  for _,child:=range node.children {
    action,err:=compatibility.visit(child,scope,false);if err!=nil { return err }
    if action==0 {
      if err=tree.reserve(childSlotBytes);err!=nil { return err };result=append(result,child)
    } else if action==1 {
      for _,nested:=range child.children { if err=tree.reserve(childSlotBytes);err!=nil { return err };result=append(result,nested) }
      if err=tree.reserve(int64(child.text.Len()));err!=nil { return err };text.WriteString(child.text.String())
    }
  }
  // Reset does not transfer the builder's backing store; admit the distinct final text copy.
  if err:=tree.reserve(int64(text.Len()));err!=nil { return err }
  node.children=result;node.text.Reset();node.text.WriteString(text.String());return nil
}
// visit retains zero, unwraps one or discards two; discarded payload content cannot demand capabilities.
func (compatibility *srlPackageCompatibility) visit(node *srlPackageNode,parent *srlPackageCompatibilityScope,selectedBranch bool) (int,error) {
  tree:=compatibility.tree;if err:=tree.state.consume(SerializerPackageWorkUnits,1,tree.part);err!=nil { return 0,err }
  scope,err:=compatibility.declarations(node,parent);if err!=nil { return 0,err }
  uri,local:=node.name.Space,node.name.Local;ignored,err:=compatibility.ignorable(scope,uri);if err!=nil { return 0,err }
  if uri!=srlPackageRelationshipNamespace&&ignored {
    process,err:=compatibility.process(scope,node.name);if err!=nil { return 0,err };if !process { return 2,nil }
    for _,name:=range []string{"base","lang","space"} { if _,found:=node.attributes[srlPackageXML.Name{Space:srlPackageXMLNamespace,Local:name}];found { return 0,srlPackageFailure(SerializerPackageInvalidMetadata,"XML context on unwrapped metadata",tree.part) } }
    if err:=compatibility.must(node);err!=nil { return 0,err };if err:=compatibility.children(node,scope);err!=nil { return 0,err };return 1,nil
  }
  if uri==srlPackageCompatibilityNamespace {
    if local=="AlternateContent" {
      if _,err:=compatibility.wrapper(node,scope,false);err!=nil { return 0,err };if err:=srlPackageElementContent(node,tree.part);err!=nil { return 0,err }
      choices:=0;fallback:=false;var selected *srlPackageNode
      for _,child:=range node.children {
        childScope,err:=compatibility.declarations(child,scope);if err!=nil { return 0,err }
        ignored,err:=compatibility.ignorable(childScope,child.name.Space);if err!=nil { return 0,err };process,err:=compatibility.process(childScope,child.name);if err!=nil { return 0,err }
        if child.name.Space!=srlPackageRelationshipNamespace&&ignored&&!process { continue }
        if child.name.Space!=srlPackageCompatibilityNamespace { return 0,srlPackageFailure(SerializerPackageInvalidMetadata,"Unexpected AlternateContent child",tree.part) }
        if child.name.Local=="Choice"&&!fallback { choices++;supported,err:=compatibility.wrapper(child,childScope,true);if err!=nil { return 0,err };if selected==nil&&supported { selected=child } } else if child.name.Local=="Fallback"&&choices>0&&!fallback { fallback=true;if _,err:=compatibility.wrapper(child,childScope,false);err!=nil { return 0,err };if selected==nil { selected=child } } else { return 0,srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid Choice/Fallback order",tree.part) }
      }
      if choices==0 { return 0,srlPackageFailure(SerializerPackageInvalidMetadata,"AlternateContent has no Choice",tree.part) };if err:=compatibility.must(node);err!=nil { return 0,err }
      node.children=nil;node.text.Reset()
      if selected!=nil {
        if _,err:=compatibility.visit(selected,scope,true);err!=nil { return 0,err }
        if err:=tree.reserve(int64(selected.text.Len()));err!=nil { return 0,err };node.children=selected.children;node.text.WriteString(selected.text.String())
      }
      return 1,nil
    }
    if !selectedBranch||(local!="Choice"&&local!="Fallback") { return 0,srlPackageFailure(SerializerPackageInvalidMetadata,"Unexpected compatibility element",tree.part) }
    if _,err:=compatibility.wrapper(node,scope,local=="Choice");err!=nil { return 0,err };if err:=compatibility.must(node);err!=nil { return 0,err };if err:=compatibility.children(node,scope);err!=nil { return 0,err };return 1,nil
  }
  if err:=compatibility.must(node);err!=nil { return 0,err };if err:=compatibility.children(node,scope);err!=nil { return 0,err }
  for name:=range node.attributes { ignored,err:=compatibility.ignorable(scope,name.Space);if err!=nil { return 0,err };if name.Space==srlPackageCompatibilityNamespace||name.Space!=srlPackageRelationshipNamespace&&ignored { delete(node.attributes,name) } }
  return 0,nil
}
// apply requires one logical root after capability selection and no text outside that root.
func (compatibility *srlPackageCompatibility) apply() error {
  // Admit the synthetic root record and its initial child slot before allocating either.
  const syntheticRootBytes=int64(144)
  if err:=compatibility.tree.reserve(syntheticRootBytes);err!=nil { return err }
  root:=&srlPackageNode{children:[]*srlPackageNode{compatibility.tree.root}}
  if err:=compatibility.children(root,nil);err!=nil { return err }
  if len(root.children)!=1||srlPackageStrings.Trim(root.text.String()," \t\r\n")!="" { return srlPackageFailure(SerializerPackageInvalidMetadata,"Compatibility output requires one root",compatibility.tree.part) }
  compatibility.tree.root=root.children[0];return nil
}
)packagego"
