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
// srlPackageEncodeNative preserves older callback controls while new facades receive the located XML policy.
func srlPackageEncodeNative(encode any,value any,state *srlPackageState,role srlPackageRole,part,id string)([]byte,error){switch action:=encode.(type){case func(any,XmlOptions)([]byte,error):return action(value,srlPackageNativeXMLAt(state,role,part,id));case func(any,*SerializerPackageBudget)([]byte,error):return action(value,state.budget);default:return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Invalid typed package encoder",part)}}
// srlPackageDecodeNative keeps the former callback probes while production facades share located admission.
func srlPackageDecodeNative(decode any,data []byte,state *srlPackageState,role srlPackageRole,part,id string)(any,error){switch action:=decode.(type){case func([]byte,XmlOptions)(any,error):return action(data,srlPackageNativeXMLAt(state,role,part,id));case func([]byte,*SerializerPackageBudget)(any,error):return action(data,state.budget);default:return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Invalid typed package decoder",part)}}
)packagego"
