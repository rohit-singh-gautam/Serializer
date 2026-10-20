// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
type srlPackageComposer struct {
  state srlPackageState
  sources map[string][]string
  parts []SerializerPackageBinaryPart
  links []SerializerPackageRelationship
  held int64
  finished bool
}
type srlPackageModelInput struct { value any; name,contentType string; parent int }

// srlPackageNewComposer creates private schema-role staging without I/O or application ownership transfer.
func srlPackageNewComposer(budget *SerializerPackageBudget) *srlPackageComposer { return &srlPackageComposer{state:srlPackageState{budget:budget},sources:map[string][]string{}} }
// begin fences a completed composer before entering its sequential callback guard.
func (composer *srlPackageComposer) begin() error {
  if composer.finished { return &SerializerPackageError{Code:SerializerPackageInvalidState,Message:"Finished package composer",Cause:composer.state.first} };return composer.state.begin()
}
// cleanup abandons only private payload/descriptor snapshots after nested activity has returned.
func (composer *srlPackageComposer) cleanup() {
  composer.state.budget.release(composer.held);composer.held=0;composer.parts=nil;composer.links=nil;composer.sources=nil
}
// leave latches the first failure and releases abandoned or successfully transferred staging.
func (composer *srlPackageComposer) leave(err *error,complete bool) {
  composer.state.end(err,complete);if composer.state.closed||composer.finished { composer.cleanup() }
}
// close immediately fences active callbacks while deferring cleanup until the current borrowed call ends.
func (composer *srlPackageComposer) close() { composer.state.close();if !composer.state.busy { composer.cleanup() } }
// reserve admits cumulative byte/descriptor allocation and live private ownership before copying.
func (composer *srlPackageComposer) reserve(amount int64,name string) error {
  if err:=composer.state.consume(SerializerPackageDecoderAllocationBytes,amount,name);err!=nil { return err }
  if err:=composer.state.budget.hold(amount,name);err!=nil { return err };composer.held+=amount;return nil
}
// inputs admits generated occurrence scratch before allocation and snapshots its exact count/capacity.
func (composer *srlPackageComposer) inputs(role srlPackageRole,count int) (values []srlPackageModelInput,err error) {
  if err=composer.begin();err!=nil { return nil,err };complete:=false;defer func(){composer.leave(&err,complete)}()
  if err=srlPackageOccurs(role,count);err!=nil { return nil,err }
  const descriptorBytes=int64(64)
  if int64(count)>composer.state.budget.Remaining(SerializerPackageEntries)||int64(count)>int64(^uint64(0)>>1)/descriptorBytes { return nil,srlPackageFailure(SerializerPackageResourceLimit,"Typed occurrence scratch exceeds limit","") }
  if err=composer.reserve(int64(count)*descriptorBytes,"");err!=nil { return nil,err };complete=true;return make([]srlPackageModelInput,0,count),nil
}
// parent resolves one source role occurrence before invoking any model codec.
func (composer *srlPackageComposer) parent(role srlPackageRole,index int,name string) (string,error) {
  parents:=[]string{"/"};if role.source!="/" { var found bool;parents,found=composer.sources[role.source];if !found { return "",srlPackageFailure(SerializerPackageInvalidRelationship,"Role source occurrence is missing",name) } }
  if index<0||index>=len(parents) { return "",srlPackageFailure(SerializerPackageInvalidRelationship,"Role source occurrence is missing",name) };return parents[index],nil
}
// metadata validates a complete occurrence's name/media/source before native model work or retaining bytes.
func (composer *srlPackageComposer) metadata(role srlPackageRole,dynamicName,dynamicType string,parent int) (string,string,string,error) {
  name,media:=role.name,role.contentType;if name=="" { name=dynamicName };if media=="" { media=dynamicType }
  if err:=composer.state.consume(SerializerPackageWorkUnits,int64(len(name))+1,name);err!=nil { return "","","",err }
  if err:=composer.state.consume(SerializerPackageWorkUnits,int64(len(media))+1,name);err!=nil { return "","","",err }
  if _,err:=srlPackagePartName(name,false);err!=nil { return "","","",err };if err:=srlPackageWithMIME(media,&composer.state,name,nil);err!=nil { return "","","",err }
  source,err:=composer.parent(role,parent,name);if err!=nil { return "","","",err };return name,media,source,nil
}
// add retains a checked privately owned payload and deterministic schema-role relationship occurrence.
func (composer *srlPackageComposer) add(role srlPackageRole,data []byte,name,media,source string,parent,index int) error {
  if err:=composer.reserve(256+int64(len(name)+len(media)),name);err!=nil { return err }
  id:=role.id;if id=="" { id="srl_"+role.field+"_"+srlPackageStrconv.Itoa(index) }
  composer.parts=append(composer.parts,SerializerPackageBinaryPart{Data:data,Name:name,ContentType:media,SourceIndex:parent,Compression:role.compression})
  composer.links=append(composer.links,SerializerPackageRelationship{Source:source,ID:id,Type:role.relationship,Target:name,Mode:"internal"});return nil
}
// models encodes one complete typed role while fencing swallowed callback closure/reentry and codec errors.
func (composer *srlPackageComposer) models(role srlPackageRole,values []srlPackageModelInput,encode any) (err error) {
  if err=composer.begin();err!=nil { return err };complete:=false;defer func(){composer.leave(&err,complete)}()
  if err=srlPackageOccurs(role,len(values));err!=nil { return err };discovered:=[]string{}
  for index,item:=range values {
    if err=composer.state.poll();err!=nil { return err };if item.value==nil { return srlPackageCodec(role,role.name,"",nil) }
    name,media,source,failure:=composer.metadata(role,item.name,item.contentType,item.parent);if failure!=nil { return failure }
    id:=role.id;if id==""{id="srl_"+role.field+"_"+srlPackageStrconv.Itoa(index)}
    data,failure:=srlPackageEncodeNative(encode,item.value,&composer.state,role,name,id)
    if failure!=nil { if composer.state.first!=nil { return composer.state.first };if composer.state.budget.first!=nil { return composer.state.fail(composer.state.budget.first) };return srlPackageCodec(role,name,id,failure) }
    if err=composer.state.poll();err!=nil { return err };if data==nil { return srlPackageCodec(role,name,id,nil) }
    if int64(len(data))>composer.state.budget.limits.MaxPartBytes { return srlPackageFailure(SerializerPackageResourceLimit,"Native XML output exceeds part limit",name) }
    if err=composer.reserve(int64(len(data)),name);err!=nil { return err }
    if err=composer.add(role,data,name,media,source,item.parent,index);err!=nil { return err };discovered=append(discovered,name)
  }
  composer.sources[role.field]=discovered;if err=composer.state.poll();err!=nil { return err };complete=true;return nil
}
// binaries snapshots opaque occurrence bytes before later aggregate callbacks can mutate caller input.
func (composer *srlPackageComposer) binaries(role srlPackageRole,values []SerializerPackageBinaryPart) (err error) {
  if err=composer.begin();err!=nil { return err };complete:=false;defer func(){composer.leave(&err,complete)}()
  if err=srlPackageOccurs(role,len(values));err!=nil { return err };discovered:=[]string{}
  for index,item:=range values {
    if err=composer.state.poll();err!=nil { return err };name,media,source,failure:=composer.metadata(role,item.Name,item.ContentType,item.SourceIndex);if failure!=nil { return failure }
    if int64(len(item.Data))>composer.state.budget.limits.MaxPartBytes { return srlPackageFailure(SerializerPackageResourceLimit,"Binary role exceeds part limit",name) }
    if err=composer.reserve(int64(len(item.Data)),name);err!=nil { return err };data:=append([]byte(nil),item.Data...)
    if err=composer.add(role,data,name,media,source,item.SourceIndex,index);err!=nil { return err };discovered=append(discovered,name)
  }
  composer.sources[role.field]=discovered;if err=composer.state.poll();err!=nil { return err };complete=true;return nil
}
// finish snapshots independently retained extensions and validates their edited graph before canonical publication.
func (composer *srlPackageComposer) finish(extensions SerializerPackageExtensions) (data []byte,err error) {
  if err=composer.begin();err!=nil { return nil,err };complete:=false;defer func(){composer.leave(&err,complete)}()
  for _,part:=range extensions.Parts {
    if err=composer.state.poll();err!=nil { return nil,err };if int64(len(part.Data))>composer.state.budget.limits.MaxPartBytes { return nil,srlPackageFailure(SerializerPackageResourceLimit,"Preserved part exceeds limit",part.Name) }
    if err=composer.reserve(256+int64(len(part.Data)),part.Name);err!=nil { return nil,err };part.Data=append([]byte(nil),part.Data...);composer.parts=append(composer.parts,part)
  }
  for _,link:=range extensions.Relationships { if err=composer.reserve(256,link.Source);err!=nil { return nil,err };composer.links=append(composer.links,link) }
  data,err=srlPackageWriteOPC(composer.parts,composer.links,&composer.state);if err!=nil { return nil,err }
  if err=composer.state.poll();err!=nil { return nil,err };composer.finished=true;complete=true;return data,nil
}
)packagego"
