// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
// srlPackageRole retains checked immutable schema constants; maximum -1 denotes unbounded.
type srlPackageRole struct {
  field,role,name,contentType,source,relationship,id,compression,policy,expectedRoot,expectedType,codec,targetMode string
  minimum,maximum int64
  vocabularies []string
  requiresType bool
}
type srlPackageOccurrence struct { metadata SerializerPackagePartMetadata; parent int; id string; external bool }
type srlPackageModelOccurrence struct { value any; name,contentType string; parent int }
type srlPackageLoader struct {
  state srlPackageState
  archive *SerializerPackageZipReader
  opc *srlPackageOPC
  sources map[string][]string
  referenceLocations map[string][]srlPackageOccurrence
  decoded map[string]bool
  selected map[int]bool
  held int64
  finished bool
}

// srlPackageOccurs checks complete role cardinality before invoking any typed decoder.
func srlPackageOccurs(role srlPackageRole,count int) error {
  if int64(count)<role.minimum||role.maximum>=0&&int64(count)>role.maximum { return &SerializerPackageError{Code:SerializerPackageSemantic,Message:"Package role cardinality failed",FieldPath:role.field} };return nil
}
// srlPackageCodec attaches role context without retaining XML payload excerpts or masking prior callback failure.
func srlPackageCodec(role srlPackageRole,name,id string,cause error) error {
  code:=SerializerPackageCodec;if _,limited:=cause.(srlXmlLimitError);limited{code=SerializerPackageResourceLimit};return &SerializerPackageError{Code:code,Message:"Package typed XML codec rejected part",Part:name,FieldPath:role.field,RelationshipID:id,Cause:cause}
}
// srlPackageNewLoader owns an unpublished archive/catalog, sharing one parent-aware ledger with all nested reads.
func srlPackageNewLoader(source any,budget *SerializerPackageBudget) (result *srlPackageLoader,err error) {
  loader:=&srlPackageLoader{state:srlPackageState{budget:budget},sources:map[string][]string{},referenceLocations:map[string][]srlPackageOccurrence{},decoded:map[string]bool{},selected:map[int]bool{}}
  if err=loader.state.begin();err!=nil { return nil,err };complete:=false
  defer func(){loader.state.end(&err,complete);if loader.state.closed { loader.cleanup() }}()
  loader.archive,err=srlPackageOpen(source,budget,&loader.state);if err!=nil { return nil,err }
  loader.opc,err=srlPackageReadOPC(loader.archive,&loader.state);if err!=nil { return nil,err }
  if err=loader.state.poll();err!=nil { return nil,err };complete=true;return loader,nil
}
// begin fences finished profiles before entering the ordinary first-failure/reentry guard.
func (loader *srlPackageLoader) begin() error {
  if loader.finished { return &SerializerPackageError{Code:SerializerPackageInvalidState,Message:"Finished package loader",Cause:loader.state.first} };return loader.state.begin()
}
// cleanup releases private archive/catalog/opaque staging after the last active borrowed call returns.
func (loader *srlPackageLoader) cleanup() {
  if loader.archive!=nil { loader.archive.Close();loader.archive=nil };if loader.opc!=nil { loader.opc.close();loader.opc=nil }
  loader.state.budget.release(loader.held);loader.held=0;loader.sources=nil;loader.decoded=nil;loader.selected=nil
}
// leave completes one operation before releasing any resources abandoned by callbacks.
func (loader *srlPackageLoader) leave(err *error,complete bool) {
  loader.state.end(err,complete);if loader.state.closed||loader.finished { loader.cleanup() }
}
// close immediately fences enclosing work without closing a borrowed source in the middle of a callback.
func (loader *srlPackageLoader) close() { loader.state.close();if !loader.state.busy { loader.cleanup() } }
// hold accounts internal unpublished opaque bytes until aggregate publication or abandonment.
func (loader *srlPackageLoader) hold(amount int64,part string) error {
  if err:=loader.state.budget.hold(amount,part);err!=nil { return err };loader.held+=amount;return nil
}
// discover routes roles by source/type/media identity; fixed names are writer defaults, not load constraints.
func (loader *srlPackageLoader) discover(role srlPackageRole) ([]srlPackageOccurrence,error) {
  parents:=[]string{"/"};if role.source!="/" { var found bool;parents,found=loader.sources[role.source];if !found { return nil,srlPackageFailure(SerializerPackageInvalidRelationship,"Role source is not available","") } }
  values:=[]srlPackageOccurrence{};discovered:=[]string{}
  for parent,source:=range parents {
    for index,link:=range loader.opc.relationships {
      if err:=loader.state.consume(SerializerPackageWorkUnits,1,source);err!=nil { return nil,err }
      if srlPackageFold(link.Source)!=srlPackageFold(source)||link.Type!=role.relationship { continue }
      if link.Mode=="external" {
        if role.targetMode!="external"&&role.targetMode!="both"{return nil,&SerializerPackageError{Code:SerializerPackageInvalidRelationship,Message:"Declared role requires an internal target",Part:source,FieldPath:role.field,RelationshipID:link.ID}}
        if err:=loader.state.consume(SerializerPackageDecoderAllocationBytes,256+int64(len(link.Target)),source);err!=nil{return nil,err};if err:=loader.hold(256+int64(len(link.Target)),source);err!=nil{return nil,err};loader.selected[index]=true;values=append(values,srlPackageOccurrence{metadata:SerializerPackagePartMetadata{Name:link.Target},parent:parent,id:link.ID,external:true});continue
      }
      if role.targetMode=="external" {return nil,&SerializerPackageError{Code:SerializerPackageInvalidRelationship,Message:"Declared role requires an external target",Part:source,FieldPath:role.field,RelationshipID:link.ID}}
      if role.policy=="fixed"&&metadataMethodMismatch(loader.opc,source,link,role){return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Fixed package compression mismatch",source)}
      target,err:=srlPackageTarget(source,link.Target);if err!=nil { return nil,err };key:=srlPackageFold(target);metadata:=loader.opc.parts[key]
      if role.contentType!="" {
        matched:=false
        if err=srlPackageWithMIME(metadata.ContentType,&loader.state,metadata.Name,func(actual srlPackageMIME)error{
          return srlPackageWithMIME(role.contentType,&loader.state,metadata.Name,func(expected srlPackageMIME)error{matched=actual.equal(expected);return nil})
        });err!=nil { return nil,err }
        if !matched { return nil,srlPackageCodec(role,metadata.Name,link.ID,nil) }
      }
      if loader.decoded[key] { return nil,&SerializerPackageError{Code:SerializerPackageInvalidRelationship,Message:"Part is claimed by multiple role occurrences",Part:metadata.Name,FieldPath:role.field,RelationshipID:link.ID} }
      if err=loader.state.consume(SerializerPackageDecoderAllocationBytes,256+int64(len(metadata.Name)),metadata.Name);err!=nil { return nil,err }
      if err=loader.hold(256+int64(len(metadata.Name)),metadata.Name);err!=nil { return nil,err }
      loader.decoded[key]=true;loader.selected[index]=true;values=append(values,srlPackageOccurrence{metadata:metadata,parent:parent,id:link.ID});discovered=append(discovered,metadata.Name)
    }
  }
  if err:=srlPackageOccurs(role,len(values));err!=nil { return nil,err };if err:=loader.state.consume(SerializerPackageDecoderAllocationBytes,128,"");err!=nil{return nil,err};loader.referenceLocations[role.field]=values;loader.sources[role.field]=discovered;return values,nil
}
// models returns no partial role list if any middle native XML value fails or a callback abandons the wrapper.
// Native token/scalar/identity admission is cumulative; logical counters do not promise a host heap cap.
func (loader *srlPackageLoader) models(role srlPackageRole,decode any) (result []srlPackageModelOccurrence,err error) {
  if err=loader.begin();err!=nil { return nil,err };complete:=false;defer func(){loader.leave(&err,complete)}()
  occurrences,err:=loader.discover(role);if err!=nil { return nil,err };values:=[]srlPackageModelOccurrence{}
  for _,item:=range occurrences {
    data,failure:=loader.archive.Read(item.metadata.entry.Name);if failure!=nil { return nil,failure }
    if err=loader.state.consume(SerializerPackageWorkUnits,int64(len(data)),item.metadata.Name);err!=nil { return nil,err }
    if err=loader.state.budget.hold(int64(len(data)),item.metadata.Name);err!=nil { return nil,err }
    value,failure:=func()(any,error){defer loader.state.budget.release(int64(len(data)));return srlPackageDecodeNative(decode,data,&loader.state,role,item.metadata.Name,item.id)}()
    if failure!=nil { if loader.state.first!=nil { return nil,loader.state.first };if loader.state.budget.first!=nil { return nil,loader.state.fail(loader.state.budget.first) };return nil,srlPackageCodec(role,item.metadata.Name,item.id,failure) }
    if err=loader.state.poll();err!=nil { return nil,err };if value==nil { return nil,srlPackageCodec(role,item.metadata.Name,item.id,nil) }
    values=append(values,srlPackageModelOccurrence{value:value,name:item.metadata.Name,contentType:item.metadata.ContentType,parent:item.parent})
  }
  if err=loader.state.poll();err!=nil { return nil,err };complete=true;return values,nil
}
// binaries returns complete owning opaque occurrences while retaining their source occurrence and ZIP method.
func (loader *srlPackageLoader) binaries(role srlPackageRole) (result []SerializerPackageBinaryPart,err error) {
  if err=loader.begin();err!=nil { return nil,err };complete:=false;defer func(){loader.leave(&err,complete)}()
  occurrences,err:=loader.discover(role);if err!=nil { return nil,err };values:=[]SerializerPackageBinaryPart{}
  for _,item:=range occurrences {
    if item.external{values=append(values,SerializerPackageBinaryPart{Data:[]byte{},Name:item.metadata.Name,SourceIndex:item.parent,Compression:role.compression});continue}
    data,failure:=loader.archive.Read(item.metadata.entry.Name);if failure!=nil { return nil,failure }
    if err=loader.state.poll();err!=nil { return nil,err };if err=loader.hold(int64(len(data)),item.metadata.Name);err!=nil { return nil,err }
    compression:="stored";if item.metadata.entry.Method==8 { compression="deflate" };values=append(values,SerializerPackageBinaryPart{Data:data,Name:item.metadata.Name,ContentType:item.metadata.ContentType,SourceIndex:item.parent,Compression:compression})
  }
  if err=loader.state.poll();err!=nil { return nil,err };complete=true;return values,nil
}
// finish validates unclaimed whole parts/links and binds extensions before complete-aggregate domain validation.
// Domain/capability callbacks retain their exact errors and are polled on both sides before publication.
func (loader *srlPackageLoader) finish(policy SerializerPackageExtensionPolicy,bind func(SerializerPackageExtensions),validate func(*SerializerPackageBudget)error) (err error) {
  if err=loader.begin();err!=nil { return err };complete:=false;defer func(){loader.leave(&err,complete)}()
  if policy.Parts>SerializerPackagePreserve||policy.Relationships>SerializerPackagePreserve { return srlPackageFailure(SerializerPackageInvalidOptions,"Invalid package extension policy","") }
  extensions:=SerializerPackageExtensions{}
  for _,metadata:=range loader.opc.order {
    if loader.decoded[srlPackageFold(metadata.Name)] { continue }
    if err=loader.state.poll();err!=nil { return err };required:=false
    if policy.RequiredPart!=nil { required,err=policy.RequiredPart(metadata);if err!=nil { return srlPackageCallbackFailure(&loader.state,err) } }
    if err=loader.state.poll();err!=nil { return err }
    if required||policy.Parts==SerializerPackageReject { return srlPackageFailure(SerializerPackageUnsupportedFeature,"Unknown required or rejected package part",metadata.Name) }
    data,failure:=loader.archive.Read(metadata.entry.Name);if failure!=nil { return failure };if err=loader.state.poll();err!=nil { return err }
    if policy.Parts==SerializerPackagePreserve {
      if err=loader.state.consume(SerializerPackagePreservedBytes,int64(len(data)),metadata.Name);err!=nil { return err }
      if err=loader.hold(int64(len(data)),metadata.Name);err!=nil { return err };compression:="stored";if metadata.entry.Method==8 { compression="deflate" }
      extensions.Parts=append(extensions.Parts,SerializerPackageBinaryPart{Data:data,Name:metadata.Name,ContentType:metadata.ContentType,Compression:compression})
    }
  }
  for index,link:=range loader.opc.relationships {
    if loader.selected[index] { continue };if err=loader.state.poll();err!=nil { return err };required:=false
    if policy.RequiredRelationship!=nil { required,err=policy.RequiredRelationship(link);if err!=nil { return srlPackageCallbackFailure(&loader.state,err) } };if err=loader.state.poll();err!=nil { return err }
    if required||policy.Relationships==SerializerPackageReject { return srlPackageFailure(SerializerPackageUnsupportedFeature,"Unknown required or rejected package relationship",link.Source) }
    if policy.Relationships==SerializerPackagePreserve {
      for _,text:=range []string{link.Source,link.ID,link.Type,link.Target,link.Mode} { if err=loader.state.consume(SerializerPackagePreservedBytes,int64(len(text)),link.Source);err!=nil { return err } }
      extensions.Relationships=append(extensions.Relationships,link)
    }
  }
  if bind!=nil { if err=loader.state.poll();err!=nil { return err };bind(extensions);if err=loader.state.poll();err!=nil { return err } }
  if validate!=nil { if err=loader.state.poll();err!=nil { return err };if err=validate(loader.state.budget);err!=nil { return srlPackageCallbackFailure(&loader.state,err) };if err=loader.state.poll();err!=nil { return err } }
  if err=loader.state.poll();err!=nil { return err };loader.finished=true;complete=true;return nil
}

// srlPackageCallbackFailure preserves a swallowed earlier wrapper/ledger error before a callback's later return.
func srlPackageCallbackFailure(state *srlPackageState,cause error) error {
  if state.first!=nil { return state.first };if state.budget.first!=nil { return state.fail(state.budget.first) };return state.fail(cause)
}
)packagego"
