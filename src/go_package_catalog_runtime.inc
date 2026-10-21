// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
type srlPackageOPC struct {
  parts map[string]SerializerPackagePartMetadata
  order []SerializerPackagePartMetadata
  relationships []SerializerPackageRelationship
  held int64
  state *srlPackageState
}

// reserve admits owning OPC catalog estimates independently of the temporary metadata trees.
func (opc *srlPackageOPC) reserve(amount int64,part string) error {
  if err:=opc.state.consume(SerializerPackageDecoderAllocationBytes,amount,part);err!=nil { return err }
  if err:=opc.state.budget.hold(amount,part);err!=nil { return err };opc.held+=amount;return nil
}
// close releases only private catalog reservations after nested transport borrows have finished.
func (opc *srlPackageOPC) close() {
  opc.state.budget.release(opc.held);opc.held=0;opc.parts=nil;opc.order=nil;opc.relationships=nil
}
// srlPackageDeclarations validates a complete infrastructure tree before interpreting declarations.
func srlPackageDeclarations(data []byte,namespace,local,part string,state *srlPackageState,compatibility bool) (*srlPackageMetadataTree,error) {
  tree,err:=srlPackageMetadata(data,part,state);if err!=nil { return nil,err };success:=false
  defer func(){if !success { tree.close() }}()
  if compatibility { processor:=srlPackageCompatibility{tree:tree};if err=processor.apply();err!=nil { return nil,err } }
  root:=tree.root
  if !srlPackageNamed(root,namespace,local)||len(root.attributes)!=0 { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid OPC infrastructure root",part) }
  if err=srlPackageElementContent(root,part);err!=nil { return nil,err }
  for _,child:=range root.children {
    if len(child.children)!=0 { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Nested OPC metadata declaration",part) }
    if err=srlPackageElementContent(child,part);err!=nil { return nil,err }
  }
  success=true;return tree,nil
}
// srlPackageExtension checks the exact OPC default-extension token alphabet including complete escapes.
func srlPackageExtension(value string) bool {
  if value=="" { return false }
  for index:=0;index<len(value);index++ {
    item:=value[index]
    if item=='%' { if len(value)-index<3||srlPackageHex(value[index+1])<0||srlPackageHex(value[index+2])<0 { return false };index+=2 } else if !(item>='A'&&item<='Z'||item>='a'&&item<='z'||item>='0'&&item<='9'||srlPackageStrings.ContainsRune("_~!$&'()*+,:=@-",rune(item))) { return false }
  }
  return true
}
// srlPackageContentTypeCatalog indexes defaults/overrides without collapsing MIME parameter values.
func srlPackageContentTypeCatalog(archive *SerializerPackageZipReader,manifest SerializerPackageEntry,opc *srlPackageOPC) (map[string]string,map[string]string,error) {
  bytes,err:=archive.Read(manifest.Name);if err!=nil { return nil,nil,err }
  tree,err:=srlPackageDeclarations(bytes,srlPackageContentTypes,"Types","/[Content_Types].xml",opc.state,false);if err!=nil { return nil,nil,err };defer tree.close()
  defaults,overrides:=map[string]string{},map[string]string{}
  for _,child:=range tree.root.children {
    if err=opc.state.consume(SerializerPackageWorkUnits,1,manifest.Name);err!=nil { return nil,nil,err }
    if srlPackageNamed(child,srlPackageContentTypes,"Default") {
      if err=srlPackageAttributes(child,manifest.Name,"Extension","ContentType");err!=nil { return nil,nil,err }
      extension,failure:=srlPackageAttribute(child,"Extension",manifest.Name);if failure!=nil { return nil,nil,failure };extension=srlPackageFold(extension)
      if _,exists:=defaults[extension];exists||!srlPackageExtension(extension) { return nil,nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid or duplicate default extension",manifest.Name) }
      media,failure:=srlPackageRawAttribute(child,"ContentType",manifest.Name);if failure!=nil { return nil,nil,failure };if failure=srlPackageWithMIME(media,opc.state,manifest.Name,nil);failure!=nil { return nil,nil,failure }
      if err=opc.reserve(64+int64(len(extension)+len(media)),manifest.Name);err!=nil { return nil,nil,err };defaults[extension]=media
    } else if srlPackageNamed(child,srlPackageContentTypes,"Override") {
      if err=srlPackageAttributes(child,manifest.Name,"PartName","ContentType");err!=nil { return nil,nil,err }
      name,failure:=srlPackageAttribute(child,"PartName",manifest.Name);if failure!=nil { return nil,nil,failure };if _,failure=srlPackagePartName(name,true);failure!=nil { return nil,nil,failure };name=srlPackageFold(name)
      if _,exists:=overrides[name];exists { return nil,nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Duplicate content type override",name) }
      media,failure:=srlPackageRawAttribute(child,"ContentType",manifest.Name);if failure!=nil { return nil,nil,failure };if failure=srlPackageWithMIME(media,opc.state,manifest.Name,nil);failure!=nil { return nil,nil,failure }
      if err=opc.reserve(64+int64(len(name)+len(media)),name);err!=nil { return nil,nil,err };overrides[name]=media
    } else { return nil,nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Unknown content type declaration",manifest.Name) }
  }
  if len(defaults)==0&&len(overrides)==0 { return nil,nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Empty content type manifest",manifest.Name) };return defaults,overrides,nil
}
// srlPackageValidateLinks checks source-local IDs, inert URI syntax and complete internal part closure.
func srlPackageValidateLinks(links []SerializerPackageRelationship,parts map[string]SerializerPackagePartMetadata,state *srlPackageState) ([]SerializerPackageRelationship,error) {
  seen:=map[string]map[string]bool{};result:=make([]SerializerPackageRelationship,0,len(links))
  for _,link:=range links {
    for _,text:=range []string{link.Source,link.ID,link.Type,link.Target,link.Mode} { if err:=state.consume(SerializerPackageWorkUnits,int64(len(text))+1,link.Source);err!=nil { return nil,err } }
    for _,resource:=range []SerializerPackageResource{SerializerPackageRelationships,SerializerPackageReferences}{if err:=state.consume(resource,1,link.Source);err!=nil{if failure,ok:=err.(*SerializerPackageError);ok&&state.first!=err&&state.budget.first!=err{copy:=*failure;copy.Part=link.Source;copy.FieldPath="relationships";copy.Reference=link.Target;copy.RelationshipID=link.ID;return nil,&copy};return nil,err}}
    if !srlPackageNCName(link.ID) { return nil,srlPackageFailure(SerializerPackageInvalidRelationship,"Invalid relationship ID",link.Source) }
    source:=link.Source
    if source!="/" { if _,err:=srlPackagePartName(source,false);err!=nil { return nil,err };if _,exists:=parts[srlPackageFold(source)];!exists { return nil,srlPackageFailure(SerializerPackageMissingPart,"Relationship source is missing",source) } }
    key:=srlPackageFold(source);if seen[key]==nil { seen[key]=map[string]bool{} };if seen[key][link.ID] { return nil,srlPackageFailure(SerializerPackageInvalidRelationship,"Duplicate source-local relationship ID",source) };seen[key][link.ID]=true
    if _,err:=srlPackageParseURI(link.Type,true);err!=nil { return nil,err }
    if link.Mode=="internal" {
      destination,err:=srlPackageTarget(source,link.Target);if err!=nil { return nil,err }
      if _,found:=parts[srlPackageFold(destination)];!found { return nil,&SerializerPackageError{Code:SerializerPackageMissingPart,Message:"Relationship target is missing",Part:source,Reference:destination,RelationshipID:link.ID} }
    } else if link.Mode=="external" { if _,err:=srlPackageParseURI(link.Target,false);err!=nil { return nil,err } } else { return nil,srlPackageFailure(SerializerPackageInvalidRelationship,"Invalid relationship target mode",source) }
    result=append(result,link)
  }
  return result,state.poll()
}
// srlPackageReadOPC validates all standard metadata and excluded-entry integrity before catalog publication.
func srlPackageReadOPC(archive *SerializerPackageZipReader,state *srlPackageState) (result *srlPackageOPC,err error) {
  opc:=&srlPackageOPC{parts:map[string]SerializerPackagePartMetadata{},state:state};success:=false
  defer func(){if !success { opc.close() }}()
  var manifest SerializerPackageEntry;found:=false
  for _,entry:=range archive.entries {
    if err=state.consume(SerializerPackageWorkUnits,1,entry.Name);err!=nil { return nil,err }
    if srlPackageFold(entry.Name)=="[content_types].xml" {
      if found { return nil,srlPackageFailure(SerializerPackageDuplicateName,"Duplicate content type manifest","") }
      if !entry.RegularFile() { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Content type manifest is not a regular file",entry.Name) };manifest=entry;found=true
    }
  }
  if !found { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Content type manifest is missing","") }
  defaults,overrides,err:=srlPackageContentTypeCatalog(archive,manifest,opc);if err!=nil { return nil,err }
  mapped,identities:=map[string]bool{},map[string]bool{};relationshipParts:=[]struct{name,source string}{}
  for _,entry:=range archive.entries {
    if entry.Name==manifest.Name { continue }
    if err=state.consume(SerializerPackageWorkUnits,int64(len(entry.Name))+1,entry.Name);err!=nil { return nil,err }
    if err=opc.reserve(128+int64(len(entry.Name))*2,entry.Name);err!=nil { return nil,err }
    name:="/"+entry.Name;accepted:=false
    if entry.RegularFile() { accepted,err=srlPackageMapsToPart(name,state);if err!=nil { return nil,err } }
    if !accepted { if _,err=archive.Read(entry.Name);err!=nil { return nil,err };continue }
    key,failure:=srlPackageUniquePart(identities,name,state);if failure!=nil { return nil,failure };mapped[key]=true
    media,exists:=overrides[key]
    if !exists { dot,slash:=srlPackageStrings.LastIndexByte(name,'.'),srlPackageStrings.LastIndexByte(name,'/');if dot>slash { media,exists=defaults[srlPackageFold(name[dot+1:])] } }
    if !exists { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Part has no content type",name) }
    source,relationship,failure:=srlPackageRelationshipSource(name);if failure!=nil { return nil,failure };relationshipMedia:=false
    if failure=srlPackageWithMIME(media,state,name,func(parsed srlPackageMIME)error{relationshipMedia=parsed.media==srlPackageRelationshipMIME;return nil});failure!=nil { return nil,failure }
    if relationship {
      if !relationshipMedia { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Relationship part has wrong content type",name) }
      relationshipParts=append(relationshipParts,struct{name,source string}{name,source})
    } else {
      if _,err=srlPackagePartName(name,false);err!=nil { return nil,err }
      if relationshipMedia { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Relationship media type requires infrastructure",name) }
      metadata:=SerializerPackagePartMetadata{Name:name,ContentType:media,entry:entry};opc.parts[key]=metadata;opc.order=append(opc.order,metadata)
    }
  }
  for name:=range overrides { if !mapped[name] { return nil,srlPackageFailure(SerializerPackageInvalidMetadata,"Content type override names a missing or reserved part",name) } }
  links:=[]SerializerPackageRelationship{}
  for _,item:=range relationshipParts {
    if item.source!="/" { if _,found:=opc.parts[srlPackageFold(item.source)];!found { return nil,srlPackageFailure(SerializerPackageMissingPart,"Relationship-part source is missing",item.name) } }
    parse:=func()error {
      bytes,failure:=archive.Read(item.name[1:]);if failure!=nil { return failure }
      tree,failure:=srlPackageDeclarations(bytes,srlPackageRelationshipNamespace,"Relationships",item.name,state,true);if failure!=nil { return failure };defer tree.close()
      for _,child:=range tree.root.children {
        if !srlPackageNamed(child,srlPackageRelationshipNamespace,"Relationship") { return srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid relationship declaration",item.name) }
        if failure=srlPackageAttributes(child,item.name,"Id","Type","Target","TargetMode");failure!=nil { return failure }
        mode:="Internal";if text,found:=child.attributes[srlPackageXML.Name{Local:"TargetMode"}];found { mode=text }
        if mode!="Internal"&&mode!="External" { return srlPackageFailure(SerializerPackageInvalidRelationship,"Invalid TargetMode",item.name) }
        id,failure:=srlPackageAttribute(child,"Id",item.name);if failure!=nil { return failure };kind,failure:=srlPackageAttribute(child,"Type",item.name);if failure!=nil { return failure };target,failure:=srlPackageAttribute(child,"Target",item.name);if failure!=nil { return failure }
        if failure=opc.reserve(256+int64(len(item.source)+len(id)+len(kind)+len(target)),item.name);failure!=nil { return failure }
        links=append(links,SerializerPackageRelationship{Source:item.source,ID:id,Type:kind,Target:target,Mode:srlPackageFold(mode)})
      }
      return nil
    }
    if err=parse();err!=nil { return nil,err }
  }
  opc.relationships,err=srlPackageValidateLinks(links,opc.parts,state);if err!=nil { return nil,err }
  if err=state.poll();err!=nil { return nil,err };success=true;return opc,nil
}

// SerializerPackageReader owns validated OPC metadata and only the ZIP source adapter it creates.
// Borrowed application transports remain open; wrapper access and callbacks require sequential calls.
type SerializerPackageReader struct { state srlPackageState; archive *SerializerPackageZipReader; opc *srlPackageOPC }
// SerializerPackageOpenOPC publishes a catalog only after complete standard metadata and closure checks.
func SerializerPackageOpenOPC(source any,budgets ...*SerializerPackageBudget) (result *SerializerPackageReader,err error) {
  budget,err:=srlPackageBudget(budgets);if err!=nil { return nil,err }
  reader:=&SerializerPackageReader{state:srlPackageState{budget:budget}};if err=reader.state.begin();err!=nil { return nil,err };complete:=false
  defer func(){reader.state.end(&err,complete);if reader.state.closed { reader.cleanup() }}()
  reader.archive,err=srlPackageOpen(source,budget,&reader.state);if err!=nil { return nil,err }
  reader.opc,err=srlPackageReadOPC(reader.archive,&reader.state);if err!=nil { return nil,err }
  if err=reader.state.poll();err!=nil { return nil,err };complete=true;return reader,nil
}
// Parts returns an independent logical catalog slice while private physical identities remain immutable.
func (reader *SerializerPackageReader) Parts() []SerializerPackagePartMetadata { if reader.opc==nil { return nil };return append([]SerializerPackagePartMetadata(nil),reader.opc.order...) }
// Relationships returns a detached source-local link slice, preserving exact external target spelling.
func (reader *SerializerPackageReader) Relationships() []SerializerPackageRelationship { if reader.opc==nil { return nil };return append([]SerializerPackageRelationship(nil),reader.opc.relationships...) }
// Read returns independent whole-part bytes after exact ZIP integrity checks under the shared ledger.
func (reader *SerializerPackageReader) Read(name string) (data []byte,err error) {
  if err=reader.state.begin();err!=nil { return nil,err };complete:=false
  defer func(){reader.state.end(&err,complete);if reader.state.closed { reader.cleanup() }}()
)packagego"
R"packagego(
  if _,err=srlPackagePartName(name,false);err!=nil { return nil,err };part,found:=reader.opc.parts[srlPackageFold(name)]
  if !found { return nil,srlPackageFailure(SerializerPackageMissingPart,"Package part is missing",name) }
  data,err=reader.archive.Read(part.entry.Name);if err!=nil { return nil,err };if err=reader.state.poll();err!=nil { return nil,err };complete=true;return data,nil
}
// cleanup releases owned private resources only after the active borrowed operation has returned.
func (reader *SerializerPackageReader) cleanup() {
  if reader.archive!=nil { reader.archive.Close();reader.archive=nil };if reader.opc!=nil { reader.opc.close();reader.opc=nil }
}
// Close fences immediately and leaves application-owned input open, including during callback abandonment.
func (reader *SerializerPackageReader) Close() error { reader.state.close();if !reader.state.busy { reader.cleanup() };return nil }
)packagego"
