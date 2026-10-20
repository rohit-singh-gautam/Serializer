// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
// srlPackageEscapedSize measures exact XML1.0 attribute bytes before metadata allocation.
func srlPackageEscapedSize(value string) (int64,error) {
  if !srlPackageUTF8.ValidString(value) { return 0,srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid XML metadata UTF-8","") }
  var count int64
  for _,item:=range value {
    if !srlPackageXMLCharacter(item) { return 0,srlPackageFailure(SerializerPackageInvalidMetadata,"Invalid XML metadata character","") }
    switch item { case '&':count+=5;case '<':count+=4;case '"':count+=6;case 9:count+=4;case 10,13:count+=5;default:count+=int64(srlPackageUTF8.RuneLen(item)) }
  }
  return count,nil
}
// srlPackageAppendEscape appends already admitted text directly without an escaped temporary copy.
func srlPackageAppendEscape(output []byte,value string) []byte {
  for _,item:=range value {
    switch item {
    case '&':output=append(output,"&amp;"...)
    case '<':output=append(output,"&lt;"...)
    case '"':output=append(output,"&quot;"...)
    case 9:output=append(output,"&#9;"...)
    case 10:output=append(output,"&#10;"...)
    case 13:output=append(output,"&#13;"...)
    default:output=srlPackageUTF8.AppendRune(output,item)
    }
  }
  return output
}
// srlPackageWriteMetadata retains canonical generated infrastructure only while the ZIP writer snapshots it.
func srlPackageWriteMetadata(writer *SerializerPackageZipWriter,name,namespace,root,child string,declarations []map[string]string,state *srlPackageState) error {
  prefix:="<?xml version=\"1.0\" encoding=\"UTF-8\"?><"+root+" xmlns=\""+namespace+"\">"
  size:=int64(len(prefix)+len(root)+3)
  for _,attributes:=range declarations {
    if err:=state.consume(SerializerPackageWorkUnits,1,name);err!=nil { return err };size+=int64(len(child)+3)
    for key,value:=range attributes {
      if err:=state.consume(SerializerPackageWorkUnits,int64(len(value))+1,name);err!=nil { return err }
      count,err:=srlPackageEscapedSize(value);if err!=nil { return err };size+=int64(len(key)+4)+count
      if size>state.budget.limits.MaxPartBytes { return srlPackageFailure(SerializerPackageResourceLimit,"Generated metadata part exceeds limit",name) }
    }
  }
  length,err:=srlPackageArray(size,state.budget.limits.MaxPartBytes,name);if err!=nil { return err }
  if err=state.consume(SerializerPackageMetadataBytes,size,name);err!=nil { return err }
  if err=state.consume(SerializerPackageDecoderAllocationBytes,size,name);err!=nil { return err }
  if err=state.budget.hold(size,name);err!=nil { return err };defer state.budget.release(size)
  output:=make([]byte,0,length);output=append(output,prefix...)
  for _,attributes:=range declarations {
    if err=state.poll();err!=nil { return err };output=append(output,'<');output=append(output,child...)
    // Infrastructure uses at most four known attributes; bounded fixed scratch avoids a per-element sort slice.
    var keys [4]string;count:=0
    for key:=range attributes { if count==len(keys) { return srlPackageFailure(SerializerPackageInvalidState,"Unexpected metadata attribute count",name) };keys[count]=key;count++ }
    srlPackageSort.Strings(keys[:count])
    for _,key:=range keys[:count] { output=append(output,' ');output=append(output,key...);output=append(output,'=','"');output=srlPackageAppendEscape(output,attributes[key]);output=append(output,'"') }
    output=append(output,'/','>')
  }
  output=append(output,'<','/');output=append(output,root...);output=append(output,'>')
  if len(output)!=length { return srlPackageFailure(SerializerPackageInvalidState,"Generated metadata extent mismatch",name) }
  return writer.Add(name,output,"deflate")
}
// srlPackageWriteOPC validates full names/MIME/link closure before privately staging canonical infrastructure.
func srlPackageWriteOPC(parts []SerializerPackageBinaryPart,links []SerializerPackageRelationship,state *srlPackageState) (data []byte,err error) {
  // This is a conservative descriptor/map estimate, separate from actual payload and generated metadata bytes.
  const descriptorBytes=int64(256)
  if int64(len(parts))>state.budget.Remaining(SerializerPackageEntries)||int64(len(links))>state.budget.Remaining(SerializerPackageRelationships) { return nil,srlPackageFailure(SerializerPackageResourceLimit,"Package entry/relationship limit exceeded","") }
  scratch:=int64(0);defer func(){state.budget.release(scratch)}()
  for _,count:=range []int{len(parts),len(links)} {
    if int64(count)>int64(^uint64(0)>>1)/descriptorBytes { return nil,srlPackageFailure(SerializerPackageResourceLimit,"Package descriptor extent exceeds range","") }
    amount:=int64(count)*descriptorBytes
    if err=state.consume(SerializerPackageDecoderAllocationBytes,amount,"");err!=nil { return nil,err }
    if err=state.budget.hold(amount,"");err!=nil { return nil,err };scratch+=amount
  }
  ordered:=append([]SerializerPackageBinaryPart(nil),parts...);catalog:=map[string]SerializerPackagePartMetadata{};identities:=map[string]bool{}
  for _,part:=range ordered {
    if err=state.poll();err!=nil { return nil,err }
    if err=state.consume(SerializerPackageWorkUnits,int64(len(part.Name))+1,part.Name);err!=nil { return nil,err }
    if err=state.consume(SerializerPackageWorkUnits,int64(len(part.ContentType))+1,part.Name);err!=nil { return nil,err }
    if _,err=srlPackagePartName(part.Name,false);err!=nil { return nil,err }
    if err=srlPackageWithMIME(part.ContentType,state,part.Name,func(parsed srlPackageMIME)error{
      if parsed.media==srlPackageRelationshipMIME { return srlPackageFailure(SerializerPackageInvalidMetadata,"Relationship media type requires infrastructure",part.Name) };return nil
    });err!=nil { return nil,err }
    if part.Compression!=""&&part.Compression!="stored"&&part.Compression!="deflate" { return nil,srlPackageFailure(SerializerPackageInvalidOptions,"Invalid package part compression",part.Name) }
    key,failure:=srlPackageUniquePart(identities,part.Name,state);if failure!=nil { return nil,failure };catalog[key]=SerializerPackagePartMetadata{Name:part.Name,ContentType:part.ContentType}
  }
  validated,err:=srlPackageValidateLinks(links,catalog,state);if err!=nil { return nil,err }
  groups:=map[string][]SerializerPackageRelationship{}
  for _,link:=range validated {
    source:=link.Source;if source!="/" { source=catalog[srlPackageFold(source)].Name };link.Source=source;groups[source]=append(groups[source],link)
  }
  srlPackageSort.Slice(ordered,func(left,right int)bool{return ordered[left].Name<ordered[right].Name})
  writer:=&SerializerPackageZipWriter{state:srlPackageState{budget:state.budget,parent:state},names:map[string]bool{}};defer writer.Close()
  declarations:=[]map[string]string{}
  for _,part:=range ordered {
    compression:=part.Compression;if compression=="" { compression="stored" }
    if err=writer.Add(part.Name[1:],part.Data,compression);err!=nil { return nil,err }
    declarations=append(declarations,map[string]string{"PartName":part.Name,"ContentType":part.ContentType})
  }
  sources:=make([]string,0,len(groups));for source:=range groups { sources=append(sources,source) };srlPackageSort.Strings(sources)
  for _,source:=range sources {
    name:="/_rels/.rels"
    if source!="/" { separator:=srlPackageStrings.LastIndexByte(source,'/');name=source[:separator+1]+"_rels/"+source[separator+1:]+".rels" }
    if _,err=srlPackageUniquePart(identities,name,state);err!=nil { return nil,err }
    entries:=groups[source];srlPackageSort.Slice(entries,func(left,right int)bool{return entries[left].ID<entries[right].ID})
    children:=make([]map[string]string,0,len(entries))
    for _,link:=range entries {
      attributes:=map[string]string{"Id":link.ID,"Type":link.Type,"Target":link.Target};if link.Mode=="external" { attributes["TargetMode"]="External" };children=append(children,attributes)
    }
    if err=srlPackageWriteMetadata(writer,name[1:],srlPackageRelationshipNamespace,"Relationships","Relationship",children,state);err!=nil { return nil,err }
    declarations=append(declarations,map[string]string{"PartName":name,"ContentType":srlPackageRelationshipMIME})
  }
  // Relationships and payload declarations share one stable PartName order independently of input collection order.
  srlPackageSort.Slice(declarations,func(left,right int)bool{return declarations[left]["PartName"]<declarations[right]["PartName"]})
  if len(declarations)==0 { err=srlPackageWriteMetadata(writer,"[Content_Types].xml",srlPackageContentTypes,"Types","Default",[]map[string]string{{"Extension":"rels","ContentType":srlPackageRelationshipMIME}},state) } else { err=srlPackageWriteMetadata(writer,"[Content_Types].xml",srlPackageContentTypes,"Types","Override",declarations,state) }
  if err!=nil { return nil,err };data,err=writer.Finish();if err!=nil { return nil,err };if err=state.poll();err!=nil { return nil,err };return data,nil
}
// SerializerPackageEncodeOPC returns one canonical physical OPC package after whole-graph validation.
// It retains no application slices after returning; external URI targets remain inert metadata.
func SerializerPackageEncodeOPC(parts []SerializerPackageBinaryPart,links []SerializerPackageRelationship,budgets ...*SerializerPackageBudget) (data []byte,err error) {
  budget,err:=srlPackageBudget(budgets);if err!=nil { return nil,err };state:=srlPackageState{budget:budget}
  if err=state.begin();err!=nil { return nil,err };complete:=false;defer func(){state.end(&err,complete)}()
  data,err=srlPackageWriteOPC(parts,links,&state);if err!=nil { return nil,err };complete=true;return data,nil
}
)packagego"
