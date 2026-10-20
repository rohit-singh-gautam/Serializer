// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
// metadataMethodMismatch compares fixed schema compression before payload interpretation.
func metadataMethodMismatch(opc *srlPackageOPC,source string,link SerializerPackageRelationship,role srlPackageRole)bool{target,err:=srlPackageTarget(source,link.Target);if err!=nil{return false};part,ok:=opc.parts[srlPackageFold(target)];if !ok{return false};expected:=uint16(8);if role.compression=="stored"{expected=0};return part.entry.Method!=expected}
// srlPackageSelectProfile applies a closed override without changing expected model/XML identity.
func srlPackageSelectProfile(role srlPackageRole,profiles map[string]string)(srlPackageRole,error){if selected,ok:=profiles[role.role];ok{if selected!="stored"&&selected!="deflate"||role.policy=="fixed"&&selected!=role.compression{return role,srlPackageFailure(SerializerPackageInvalidOptions,"Conflicting package profile override",role.field)};role.compression=selected};return role,nil}
type srlPackageReferences struct{budget *SerializerPackageBudget;keys map[string]string;pending [][5]string;part,id string}
// consume preserves caller error identity and locates only runtime-owned admission failures.
func(r *srlPackageReferences)consume(resource SerializerPackageResource,amount int64,field string)error{err:=r.budget.Consume(resource,amount,r.part);if failure,ok:=err.(*SerializerPackageError);ok&&r.budget.first!=err{copy:=*failure;copy.Part=r.part;copy.FieldPath=field;copy.RelationshipID=r.id;return &copy};return err}
// key indexes exact scoped scalar identities with the expected native model type.
func(r *srlPackageReferences)key(scope,value,role,field,modelType string)error{if r.keys==nil{r.keys=map[string]string{}};path:=role+"."+field;slot:=scope+"\x00"+value;if err:=r.consume(SerializerPackageReferences,1,path);err!=nil{return err};if err:=r.consume(SerializerPackageWorkUnits,int64(len(slot)+1),path);err!=nil{return err};if err:=r.consume(SerializerPackageDecoderAllocationBytes,int64(128+len(slot)),path);err!=nil{return err};if _,ok:=r.keys[slot];ok{return &SerializerPackageError{Code:SerializerPackageSemantic,Message:"Duplicate application key",Part:r.part,FieldPath:path,Reference:scope,RelationshipID:r.id}};r.keys[slot]=modelType;return nil}
// reference retains a forward edge with actual loaded part/link provenance.
func(r *srlPackageReferences)reference(scope,value,target,role,field string)error{path:=role+"."+field;if err:=r.consume(SerializerPackageReferences,1,path);err!=nil{return err};if err:=r.consume(SerializerPackageDecoderAllocationBytes,int64(160+len(value)),path);err!=nil{return err};r.pending=append(r.pending,[5]string{scope+"\x00"+value,target,path,r.part,r.id});return nil}
// finish resolves every request before aggregate publication, preserving its original source.
func(r *srlPackageReferences)finish()error{for _,item:=range r.pending{r.part,r.id=item[3],item[4];if err:=r.consume(SerializerPackageWorkUnits,int64(len(item[0])+1),item[2]);err!=nil{return err};if r.keys[item[0]]!=item[1]{return &SerializerPackageError{Code:SerializerPackageSemantic,Message:"Missing or wrong-type application key",Part:r.part,FieldPath:item[2],RelationshipID:r.id}}};return nil}
// srlPackageNativeXML narrows existing per-document XML defaults before invoking generated native codecs.
// Native events, scalar work, namespace retention and identity tuples share the operation ledger.
func srlPackageNativeXMLAt(state *srlPackageState,role srlPackageRole,part,id string) XmlOptions {
  budget:=state.budget
  options:=DefaultXmlOptions();limits:=&options.Limits
  narrow:=func(current int,maximum int64)int{if maximum<int64(current){return int(maximum)};return current}
  limits.MaxBytes=narrow(limits.MaxBytes,budget.limits.MaxPartBytes)
  limits.MaxTextBytes=narrow(limits.MaxTextBytes,budget.limits.MaxPartBytes)
  limits.MaxNodes=narrow(limits.MaxNodes,budget.Remaining(SerializerPackageWorkUnits))
  limits.MaxAttributes=narrow(limits.MaxAttributes,budget.Remaining(SerializerPackageWorkUnits))
  operation:=func(resource string,count int64,field string)error{
    selected:=SerializerPackageWorkUnits;if resource=="allocation"{selected=SerializerPackageDecoderAllocationBytes}else if resource=="reference"{selected=SerializerPackageReferences}
    err:=state.consume(selected,count,part);if failure,ok:=err.(*SerializerPackageError);ok&&state.first!=err&&budget.first!=err{copy:=*failure;copy.Part=part;copy.FieldPath=role.field;if field!=""{copy.FieldPath+="."+field};copy.RelationshipID=id;err=&copy};if err!=nil{return state.fail(err)};return nil
  }
  return srlXmlWithOperation(options,operation,role.expectedRoot,role.expectedType)
}
// srlPackageNativeXML preserves the existing internal adapter for independent provider probes.
func srlPackageNativeXML(budget *SerializerPackageBudget)XmlOptions{return srlPackageNativeXMLAt(&srlPackageState{budget:budget},srlPackageRole{},"","")}
// srlPackageTypedHeader checks immutable expanded dispatch before selected callbacks; native codecs own XML counts.
func srlPackageTypedHeader(data []byte,state *srlPackageState,role srlPackageRole,part,id string)error{
  if int64(len(data))>state.budget.limits.MaxPartBytes{return &SerializerPackageError{Code:SerializerPackageResourceLimit,Message:"Payload exceeds part limit",Part:part,FieldPath:role.field,RelationshipID:id}}
  if err:=state.consume(SerializerPackageWorkUnits,int64(len(data)),part);err!=nil{return err};if err:=state.consume(SerializerPackageDecoderAllocationBytes,int64(len(data)),part);err!=nil{return err}
  maximum:=int(^uint(0)>>1);limits:=DefaultXmlLimits();limits.MaxBytes=len(data);limits.MaxTextBytes=maximum;limits.MaxAttributes=maximum;limits.MaxNodes=1;limits.MaxDepth=1
  input,err:=srlXmlInputStart(data,limits);if err!=nil{return srlPackageCodec(role,part,id,err)}
  // Expanded namespace vocabulary can exceed raw header bytes; charge its actual extra work/storage.
  bridge:=srlPackageNativeXMLAt(state,role,part,id);if context,ok:=bridge.Validator.(srlXmlOperationValidator);ok{input.budget.operation=context.operation}
  root,err:=input.peek();if err!=nil{return srlPackageCodec(role,part,id,err)}
  expanded:=func(name srlxml.Name)string{if name.Space==""{return name.Local};return "{"+name.Space+"}"+name.Local}
  if root.kind!='s'||expanded(root.name)!=role.expectedRoot{return srlPackageCodec(role,part,id,nil)}
  selected,err:=input.typeIdentity(false);if err!=nil{return srlPackageCodec(role,part,id,err)};if selected.Local!=""&&expanded(selected)!=role.expectedType{return srlPackageCodec(role,part,id,nil)};return state.poll()
}
// srlPackageEncodeNative preserves older callback controls while new facades receive the located XML policy.
func srlPackageEncodeNative(encode any,value any,state *srlPackageState,role srlPackageRole,part,id string)([]byte,error){switch action:=encode.(type){case func(any,XmlOptions,string,string)([]byte,error):data,err:=action(value,srlPackageNativeXMLAt(state,role,part,id),part,id);if err!=nil{if _,limited:=err.(srlXmlLimitError);!limited{return nil,state.fail(err)}}else{if err=srlPackageTypedHeader(data,state,role,part,id);err!=nil{return nil,state.fail(err)}};return data,err;case func(any,XmlOptions)([]byte,error):return action(value,srlPackageNativeXMLAt(state,role,part,id));case func(any,*SerializerPackageBudget)([]byte,error):return action(value,state.budget);default:return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Invalid typed package encoder",part)}}
// srlPackageDecodeNative keeps the former callback probes while production facades share located admission.
func srlPackageDecodeNative(decode any,data []byte,state *srlPackageState,role srlPackageRole,part,id string)(any,error){switch action:=decode.(type){case func([]byte,XmlOptions,string,string)(any,error):if err:=srlPackageTypedHeader(data,state,role,part,id);err!=nil{return nil,state.fail(err)};value,err:=action(data,srlPackageNativeXMLAt(state,role,part,id),part,id);if err!=nil{if _,limited:=err.(srlXmlLimitError);!limited{return nil,state.fail(err)}};return value,err;case func([]byte,XmlOptions)(any,error):return action(data,srlPackageNativeXMLAt(state,role,part,id));case func([]byte,*SerializerPackageBudget)(any,error):return action(data,state.budget);default:return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Invalid typed package decoder",part)}}
)packagego"
R"packagego(
// srlPackageCanonicalExtensions explicitly rebinds a fresh loaded candidate without extending old public structs.
func srlPackageCanonicalExtensions(loader *srlPackageLoader,extensions SerializerPackageExtensions,roles []srlPackageRole)(SerializerPackageExtensions,error){
  const relocationDescriptorBytes int64=96
  state:=&loader.state
  reserve:=func(amount int64,part string)error{if err:=state.consume(SerializerPackageDecoderAllocationBytes,amount,part);err!=nil{return err};return loader.hold(amount,part)}
  var aliases map[string]string
  for _,role:=range roles{if role.name==""{continue};for _,item:=range loader.referenceLocations[role.field]{original:=item.metadata.Name;if original==role.name{continue}
    if err:=state.consume(SerializerPackageWorkUnits,int64(len(original))+int64(len(role.name)),original);err!=nil{return SerializerPackageExtensions{},err}
    if err:=reserve(relocationDescriptorBytes+int64(len(original))+int64(len(role.name)),original);err!=nil{return SerializerPackageExtensions{},err}
    key:=srlPackageFold(original);if previous,found:=aliases[key];found&&previous!=role.name{return SerializerPackageExtensions{},srlPackageFailure(SerializerPackageInvalidRelationship,"Conflicting canonical role origins",original)};if aliases==nil{aliases=make(map[string]string)};aliases[key]=role.name
  }}
  if len(aliases)==0{return extensions,nil}
  destination:=func(original string)string{if moved,found:=aliases[srlPackageFold(original)];found{return moved};return original}
  var parts map[string]bool
  addPart:=func(name string)error{if err:=state.consume(SerializerPackageWorkUnits,int64(len(name))+1,name);err!=nil{return err};if err:=reserve(relocationDescriptorBytes+int64(len(name)),name);err!=nil{return err};validated,err:=srlPackagePartName(name,false);if err!=nil{return err};key:=srlPackageFold(validated);if parts[key]{return srlPackageFailure(SerializerPackageInvalidRelationship,"Canonical role destination is occupied",name)};if parts==nil{parts=make(map[string]bool)};parts[key]=true;return nil}
  for _,metadata:=range loader.opc.order{if loader.decoded[srlPackageFold(metadata.Name)]{if err:=addPart(destination(metadata.Name));err!=nil{return SerializerPackageExtensions{},err}}}
  for _,part:=range extensions.Parts{if err:=addPart(part.Name);err!=nil{return SerializerPackageExtensions{},err}}
  var links map[string]bool
  check:=func(link SerializerPackageRelationship)error{
    if err:=state.consume(SerializerPackageWorkUnits,int64(len(link.Source))+int64(len(link.Target))+int64(len(link.ID))+1,link.Source);err!=nil{return err}
    if err:=reserve(relocationDescriptorBytes+int64(len(link.Source))+int64(len(link.Target))+int64(len(link.ID)),link.Source);err!=nil{return err}
    if link.Source!="/"&&!parts[srlPackageFold(link.Source)]{return srlPackageFailure(SerializerPackageMissingPart,"Canonical relationship source missing",link.Source)}
    key:=srlPackageFold(link.Source)+"\x00"+link.ID;if links[key]{return &SerializerPackageError{Code:SerializerPackageInvalidRelationship,Message:"Canonical source-local relationship ID collision",Part:link.Source,RelationshipID:link.ID}};if links==nil{links=make(map[string]bool)};links[key]=true
    if link.Mode=="internal"{target,err:=srlPackageTarget(link.Source,link.Target);if err!=nil{return err};if !parts[srlPackageFold(target)]{return &SerializerPackageError{Code:SerializerPackageMissingPart,Message:"Canonical relationship target missing",Part:link.Source,RelationshipID:link.ID}}};return state.poll()
  }
  for _,role:=range roles{for index,item:=range loader.referenceLocations[role.field]{source:="/";if role.source!="/"{parents:=loader.sources[role.source];if item.parent<0||item.parent>=len(parents){return SerializerPackageExtensions{},srlPackageFailure(SerializerPackageMissingPart,"Canonical source occurrence missing",item.metadata.Name)};source=destination(parents[item.parent])};id:=role.id;if id==""{id="srl_"+role.field+"_"+srlPackageStrconv.Itoa(index)};if err:=check(SerializerPackageRelationship{Source:source,ID:id,Type:role.relationship,Target:destination(item.metadata.Name),Mode:"internal"});err!=nil{return SerializerPackageExtensions{},err}}}
  if int64(len(extensions.Relationships))>int64(^uint64(0)>>1)/relocationDescriptorBytes{return SerializerPackageExtensions{},srlPackageFailure(SerializerPackageResourceLimit,"Canonical relationship inventory overflow","")};if err:=reserve(int64(len(extensions.Relationships))*relocationDescriptorBytes,"");err!=nil{return SerializerPackageExtensions{},err}
  result:=SerializerPackageExtensions{Parts:extensions.Parts,Relationships:make([]SerializerPackageRelationship,0,len(extensions.Relationships))}
  for _,original:=range extensions.Relationships{if err:=state.poll();err!=nil{return SerializerPackageExtensions{},err};link:=original;link.Source=destination(original.Source)
    if original.Mode=="internal"{if err:=state.consume(SerializerPackageWorkUnits,int64(len(original.Source))+int64(len(original.Target)),original.Source);err!=nil{return SerializerPackageExtensions{},err};if err:=reserve(int64(len(original.Source))+int64(len(original.Target)),original.Source);err!=nil{return SerializerPackageExtensions{},err};resolved,err:=srlPackageTarget(original.Source,original.Target);if err!=nil{return SerializerPackageExtensions{},err};moved:=destination(resolved)
      if link.Source!=original.Source||moved!=resolved{suffix:=srlPackageStrings.IndexAny(original.Target,"?#");if suffix<0{suffix=len(original.Target)};if err:=reserve(int64(len(moved))+int64(len(original.Target)-suffix),original.Source);err!=nil{return SerializerPackageExtensions{},err};link.Target=moved+original.Target[suffix:]}}
    if err:=check(link);err!=nil{return SerializerPackageExtensions{},err};result.Relationships=append(result.Relationships,link)
  };if err:=state.poll();err!=nil{return SerializerPackageExtensions{},err};return result,nil
}
)packagego"
