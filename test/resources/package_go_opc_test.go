// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
  "bytes"
  "errors"
  "os"
  "path/filepath"
  "strings"
  "testing"
)

// opcCode classifies structured errors without accepting an unrelated runtime panic or transport failure.
func opcCode(err error) string { var failure *SerializerPackageError;if errors.As(err,&failure) { return strings.ToUpper(string(failure.Code)) };if err==nil { return "OK" };return err.Error() }

// TestOPCIndependent consumes wire/metadata fixtures authored independently of the generated implementation.
func TestOPCIndependent(t *testing.T) {
  table,err:=os.ReadFile("fixtures/opc_cases.tsv");if err!=nil { t.Fatal(err) }
  for _,line:=range strings.Split(strings.TrimSpace(string(table)),"\n") {
    fields:=strings.Split(line,"\t")
    t.Run(fields[0],func(t *testing.T){
      data,err:=os.ReadFile(filepath.Join("fixtures",fields[2]));if err!=nil { t.Fatal(err) }
      budget,err:=SerializerPackageNewBudget();if err!=nil { t.Fatal(err) }
      reader,err:=SerializerPackageOpenOPC(data,budget)
      if err==nil {
        parts:=reader.Parts();for _,part:=range parts { if _,err=reader.Read(part.Name);err!=nil { break } }
        if len(parts)!=0 { original:=parts[0].Name;parts[0].Name="changed";if reader.Parts()[0].Name!=original { t.Fatal("catalog slice aliases retained state") } }
        reader.Close()
      }
      accepted:=fields[3]=="ANY"&&err!=nil;for _,code:=range strings.Split(fields[3],",") { if opcCode(err)==code { accepted=true } }
      if !accepted { t.Fatalf("expected %s, got %s",fields[3],opcCode(err)) }
      if budget.held!=0 { t.Fatalf("retained logical scratch after close/rejection: %d",budget.held) }
    })
  }
}

// TestOPCPartAndMIMEIdentity exercises valid ASCII names and complete MIME parameter identities.
func TestOPCPartAndMIMEIdentity(t *testing.T) {
  for _,name:=range []string{"/file.bin","/part%3Aname.bin","/path/A:b.xml"} { if _,err:=srlPackagePartName(name,false);err!=nil { t.Fatalf("valid name %q: %v",name,err) } }
  for _,name:=range []string{"", "/", "relative.bin", "/a//b", "/a/./b", "/a/../b", "/trailing.", "/x%41", "/x%2f", "/x%5C", "/caf%C3%A9.bin", "/caf\u00e9.bin", "/_rels/.rels", "/[Content_Types].xml"} {
    if _,err:=srlPackagePartName(name,false);err==nil { t.Fatalf("invalid part name accepted: %q",name) }
  }
  left,err:=srlPackageParseMIME("application/example+xml; profile=\"a  b\";mode=read");if err!=nil { t.Fatal(err) }
  right,err:=srlPackageParseMIME("APPLICATION/EXAMPLE+XML;MODE=read;PROFILE=\"a  b\"");if err!=nil||!left.equal(right) { t.Fatal("equivalent MIME rejected",err) }
  for _,text:=range []string{"application/example+xml;profile=\"a b\";mode=read", "application/example+xml;profile=\"a  b\";mode=READ"} { value,err:=srlPackageParseMIME(text);if err!=nil||left.equal(value) { t.Fatal("significant MIME value normalized",err) } }
  for _,text:=range []string{"", "text", "application/x; p=a;P=a", "application/x; p=\"unfinished", "application/x; p=\"\u2603\"", "application/vnd.openxmlformats-package.relationships+xml;p=a", "text/plain "} { if _,err:=srlPackageParseMIME(text);err==nil { t.Fatalf("invalid MIME accepted: %q",text) } }
}

// TestOPCLiteralIRI retains Unicode lookalikes as data while validating actual delimiters and IP syntax.
func TestOPCLiteralIRI(t *testing.T) {
  for _,text:=range []string{"https://left/right","https://left\uff0fright","https://left?right","https://left\uff1fright","https://left#right","https://left\uff03right","https://left@right","https://left\uff20right","https://left:80","https://left\uff1a80","https://user\uff20name@host.invalid/a","https://caf\u00e9.invalid/a","https://host%2Fname.invalid/a","https://[2001:db8::1]:443/a","https://[vF.name:tail]/a","https:///path"} {
    if _,err:=srlPackageParseURI(text,false);err!=nil { t.Fatalf("valid IRI %q: %v",text,err) }
  }
  for _,text:=range []string{"https://host:port/","https://host:\uff11/","https://[::1/","https://[::1]tail/","https://[not-ip]/","https://[1:2:3]/","https://[fe80::1%25zone]/","https://host]/","https://[::1]]/","https://user@@host/","https://host/%zz","https://host/%","https://host/a#one#two","1scheme://host/","https://host\\evil/","https://host\ue000.invalid/","https://host/\ue000","https://host/#\ue000"} { if _,err:=srlPackageParseURI(text,false);err==nil { t.Fatalf("invalid IRI accepted: %q",text) } }
  for _,text:=range []string{"./root.xml?view=2#section","root.xml?#","/Alternate/root.xml?next=a/b#value?with:punctuation"} { name,err:=srlPackageTarget("/Alternate/source.xml",text);if err!=nil||name!="/Alternate/root.xml" { t.Fatalf("target %q => %q: %v",text,name,err) } }
}

type opcBorrowed struct { data *bytes.Reader; reads,closes int; before func() }
// ReadAt exposes stable bytes while recording whether a callback fence allowed another borrowed call.
func (source *opcBorrowed) ReadAt(data []byte,offset int64)(int,error){source.reads++;if source.before!=nil { source.before() };return source.data.ReadAt(data,offset)}
// Close is deliberately available to prove the generated adapter never takes borrowed ownership.
func (source *opcBorrowed) Close()error{source.closes++;return nil}

// TestOPCOperationBoundaries checks exact metadata quotas, caller ownership and swallowed callback closure/reentry.
func TestOPCOperationBoundaries(t *testing.T) {
  data,err:=os.ReadFile("fixtures/profile-descriptor-0-wide-0.zip");if err!=nil { t.Fatal(err) }
  reference,err:=SerializerPackageNewBudget();if err!=nil { t.Fatal(err) };reader,err:=SerializerPackageOpenOPC(data,reference);if err!=nil { t.Fatal(err) };reader.Close()
  consumed:=reference.used[SerializerPackageMetadataBytes]
  for _,tight:=range []bool{false,true} {
    limits:=SerializerPackageDefaultLimits();limits.MaxMetadataBytes=consumed;if tight { limits.MaxMetadataBytes-- }
    budget,err:=SerializerPackageNewBudget(limits);if err!=nil { t.Fatal(err) };candidate,err:=SerializerPackageOpenOPC(data,budget)
    if tight { if candidate!=nil||opcCode(err)!="RESOURCE_LIMIT" { t.Fatal("tight metadata admitted",err) } } else if err!=nil { t.Fatal(err) } else { candidate.Close() }
    if budget.held!=0 { t.Fatal("quota rejection leaked held bytes",budget.held) }
  }
  for _,mode:=range []string{"close-policy","reenter-policy","close-source","reenter-source"} {
    t.Run(mode,func(t *testing.T){
      borrowed:=&opcBorrowed{data:bytes.NewReader(data)};limits:=SerializerPackageDefaultLimits();armed:=false;var active *SerializerPackageReader;calls:=0
      callback:=func(){calls++;if mode=="close-policy"||mode=="close-source" { active.Close() } else { if _,err:=active.Read("/Textures/raw.bin");opcCode(err)!="INVALID_STATE" { t.Fatalf("reentrant read result: %v",err) } }}
      limits.CheckCancelled=func()error{if armed&&strings.HasSuffix(mode,"policy") { armed=false;callback() };return nil}
      budget,err:=SerializerPackageNewBudget(limits);if err!=nil { t.Fatal(err) }
      active,err=SerializerPackageOpenOPC(SerializerPackageSource{Reader:borrowed,SizeBytes:int64(len(data))},budget);if err!=nil { t.Fatal(err) }
      before:=borrowed.reads;armed=true;if strings.HasSuffix(mode,"source") { borrowed.before=func(){borrowed.before=nil;callback()} }
      value,err:=active.Read("/Textures/raw.bin")
      if value!=nil||opcCode(err)!="INVALID_STATE"||calls!=1 { t.Fatal("callback did not fence candidate",value,err,calls) }
      allowed:=0;if strings.HasSuffix(mode,"source") { allowed=1 };if borrowed.reads-before!=allowed { t.Fatal("additional borrowed access after closure/reentry",borrowed.reads-before) }
      active.Close();if borrowed.closes!=0||budget.held!=0 { t.Fatal("borrow ownership or private cleanup changed",borrowed.closes,budget.held) }
      if value,err=active.Read("/Textures/raw.bin");value!=nil||opcCode(err)!="INVALID_STATE" { t.Fatal("failed wrapper resumed",err) }
    })
  }
}

// opcInput snapshots independently authored part bytes and metadata for writer-only decisions.
func opcInput(t *testing.T,path string)([]SerializerPackageBinaryPart,[]SerializerPackageRelationship){
  t.Helper();data,err:=os.ReadFile(path);if err!=nil { t.Fatal(err) };reader,err:=SerializerPackageOpenOPC(data);if err!=nil { t.Fatal(err) };defer reader.Close()
  parts:=[]SerializerPackageBinaryPart{}
  for _,metadata:=range reader.Parts(){payload,err:=reader.Read(metadata.Name);if err!=nil { t.Fatal(err) };parts=append(parts,SerializerPackageBinaryPart{Data:payload,Name:metadata.Name,ContentType:metadata.ContentType,Compression:"deflate"})}
  return parts,reader.Relationships()
}

// TestOPCIndependentOutput stages six complete archives for separate standard ZIP/XML inspection.
func TestOPCIndependentOutput(t *testing.T){
  parts,links:=opcInput(t,"fixtures/profile-descriptor-0-wide-0.zip")
  for _,wide:=range []bool{false,true}{
    for descriptor:=SerializerPackageDescriptorNone;descriptor<=SerializerPackageDescriptorUnsigned;descriptor++{
      limits:=SerializerPackageDefaultLimits();limits.ForceZIP64=wide;limits.Descriptor=descriptor;budget,err:=SerializerPackageNewBudget(limits);if err!=nil { t.Fatal(err) }
      first,err:=SerializerPackageEncodeOPC(parts,links,budget);if err!=nil { t.Fatal(err) };if budget.held!=0 { t.Fatal("writer left held scratch",budget.held) }
      backwardsParts:=append([]SerializerPackageBinaryPart(nil),parts...);backwardsLinks:=append([]SerializerPackageRelationship(nil),links...)
      for left,right:=0,len(backwardsParts)-1;left<right;left,right=left+1,right-1{backwardsParts[left],backwardsParts[right]=backwardsParts[right],backwardsParts[left]}
      for left,right:=0,len(backwardsLinks)-1;left<right;left,right=left+1,right-1{backwardsLinks[left],backwardsLinks[right]=backwardsLinks[right],backwardsLinks[left]}
      again,err:=SerializerPackageNewBudget(limits);if err!=nil { t.Fatal(err) };second,err:=SerializerPackageEncodeOPC(backwardsParts,backwardsLinks,again);if err!=nil||!bytes.Equal(first,second){t.Fatal("canonical metadata depended on input order",err)}
      prefix:="output-32-";if wide{prefix="output-64-"};path:=prefix+string(rune('0'+descriptor))+".opc";if err=os.WriteFile(path,first,0600);err!=nil{t.Fatal(err)}
    }
  }
}

// TestOPCWriterRejectsBeforeArchiveIO proves complete name/media/link admission precedes archive publication.
func TestOPCWriterRejectsBeforeArchiveIO(t *testing.T){
  parts,links:=opcInput(t,"fixtures/profile-descriptor-0-wide-0.zip")
  for _,mode:=range []string{"duplicate","ancestor","reserved","mime","dangling","duplicate-id"}{
    t.Run(mode,func(t *testing.T){
      input:=append([]SerializerPackageBinaryPart(nil),parts...);relations:=append([]SerializerPackageRelationship(nil),links...)
      switch mode{
      case "duplicate":input=append(input,input[0])
      case "ancestor":input=append(input,SerializerPackageBinaryPart{Name:input[0].Name+"/child.bin",ContentType:"application/octet-stream"})
      case "reserved":input[0].Name="/_rels/.rels"
      case "mime":input[0].ContentType=srlPackageRelationshipMIME
      case "dangling":relations[0].Target="/not-present.bin"
      case "duplicate-id":relations=append(relations,relations[0])
      }
      budget,err:=SerializerPackageNewBudget();if err!=nil{t.Fatal(err)};data,err:=SerializerPackageEncodeOPC(input,relations,budget)
      if data!=nil||err==nil||budget.used[SerializerPackageIOBytes]!=0||budget.held!=0{t.Fatal("invalid graph reached archive publication",len(data),err,budget.used[SerializerPackageIOBytes],budget.held)}
    })
  }
}

var opcMainRole=srlPackageRoleProfileArchive0
var opcAttachmentRole=srlPackageRoleProfileArchive1
var opcCollectionRole=srlPackageRoleProfileCollectionArchive0

type opcProfile = ProfileArchiveValue

// opcDecodeModel invokes the actual compiler-emitted model decoder with narrowed native XML bounds.
func opcDecodeModel(data []byte,budget *SerializerPackageBudget)(any,error){value,err:=DecodeProfileModelXML(data,srlPackageNativeXML(budget));if err!=nil{return nil,err};return value,nil}
// opcEncodeModel invokes the actual compiler-emitted model encoder without replacing ordinary model APIs.
func opcEncodeModel(value any,budget *SerializerPackageBudget)([]byte,error){return value.(*ProfileModel).EncodeXML(srlPackageNativeXML(budget))}

// opcProfileLoad exercises actual emitted facade bindings, including an independently declared MIME profile.
func opcProfileLoad(data []byte,role srlPackageRole,policy SerializerPackageExtensionPolicy,validate func(*opcProfile,*SerializerPackageBudget)error)(*opcProfile,error){
  if role.contentType!=opcMainRole.contentType {
    candidate,err:=ProfileParameterizedArchiveLoad(data,ProfileParameterizedArchiveLoadOptions{ExtensionPolicy:policy})
    if err!=nil{return nil,err};return &opcProfile{Main:candidate.Main,Attachments:candidate.Attachments,Extensions:candidate.Extensions},nil
  }
  return ProfileArchiveLoad(data,ProfileArchiveLoadOptions{ExtensionPolicy:policy,Validate:validate})
}

// TestTypedIndependent restores higher-layer role/policy expectations over the same independently authored corpus.
func TestTypedIndependent(t *testing.T){
  table,err:=os.ReadFile("fixtures/profile_cases.tsv");if err!=nil{t.Fatal(err)}
  for _,line:=range strings.Split(strings.TrimSpace(string(table)),"\n"){
    fields:=strings.Split(line,"\t");t.Run(fields[0],func(t *testing.T){
      data,err:=os.ReadFile(filepath.Join("fixtures",fields[2]));if err!=nil{t.Fatal(err)}
      policy:=SerializerPackageExtensionPolicy{};if fields[1]=="skip"{policy.Parts=SerializerPackageSkip;policy.Relationships=SerializerPackageSkip}else if fields[1]=="preserve"{policy.Parts=SerializerPackagePreserve;policy.Relationships=SerializerPackagePreserve}
      if fields[1]=="collection"{
        candidate,failure:=ProfileCollectionArchiveLoad(data,ProfileCollectionArchiveLoadOptions{ExtensionPolicy:policy})
        if failure==nil{if candidate==nil||len(candidate.Models)!=3{t.Fatal("collection cardinality changed")}}else if candidate!=nil{t.Fatal("middle failure returned partial typed aggregate")};err=failure
      }else{
        role:=opcMainRole;if fields[1]=="parameterized"{role.contentType+=";profile=\"a  b\";mode=read"}
        value,failure:=opcProfileLoad(data,role,policy,nil);err=failure
        if err==nil&&(value==nil||value.Main==nil||value.Main.Name!="independent"||len(value.Main.Points)!=1||value.Main.Points[0].X!=1){t.Fatal("native typed model values changed")}
      }
      accepted:=fields[3]=="ANY"&&err!=nil;for _,expected:=range strings.Split(fields[3],","){if opcCode(err)==expected{accepted=true}}
      if !accepted{t.Fatalf("expected %s, got %s",fields[3],opcCode(err))}
    })
  }
}

// TestTypedCanonicalExtensions checks complete extension visibility and validates edited graph closure before encoding.
func TestTypedCanonicalExtensions(t *testing.T){
  data,err:=os.ReadFile("fixtures/extension.zip");if err!=nil{t.Fatal(err)};seen:=false
  policy:=SerializerPackageExtensionPolicy{Parts:SerializerPackagePreserve,Relationships:SerializerPackagePreserve}
  value,err:=opcProfileLoad(data,opcMainRole,policy,func(candidate *opcProfile,budget *SerializerPackageBudget)error{
    seen=true;if candidate.Main==nil||len(candidate.Attachments)!=1||len(candidate.Extensions.Parts)!=1||len(candidate.Extensions.Relationships)!=1{return errors.New("validator observed incomplete aggregate")};return budget.Check()
  });if err!=nil||!seen{t.Fatal("complete extensions validator",err)}
  value.Extensions.Parts=append(value.Extensions.Parts,SerializerPackageBinaryPart{Data:[]byte("second whole opaque part"),Name:"/Extensions/second.bin",ContentType:"application/octet-stream"})
  value.Extensions.Relationships=append(value.Extensions.Relationships,SerializerPackageRelationship{Source:"/",ID:"second",Type:"urn:vendor:metadata",Target:"Extensions/second.bin",Mode:"internal"})
  encode:=func(input *opcProfile)([]byte,error){return ProfileArchiveEncode(input)}
  first,err:=encode(value);if err!=nil{t.Fatal(err)}
  value.Extensions.Parts[0],value.Extensions.Parts[1]=value.Extensions.Parts[1],value.Extensions.Parts[0]
  value.Extensions.Relationships[0],value.Extensions.Relationships[1]=value.Extensions.Relationships[1],value.Extensions.Relationships[0]
  permuted,err:=encode(value);if err!=nil||!bytes.Equal(first,permuted){t.Fatal("unordered explicit extension permutation changed canonical bytes",err)}
  again,err:=opcProfileLoad(first,opcMainRole,policy,nil);if err!=nil||len(again.Extensions.Parts)!=2{t.Fatal("preserved extension roundtrip failed",err)}
  if err=os.WriteFile("preserved.opc",first,0600);err!=nil{t.Fatal(err)}
  second,err:=encode(again);if err!=nil||!bytes.Equal(first,second){t.Fatal("canonical typed package changed on retained extension roundtrip",err)}
  value.Extensions.Parts=nil;if data,err:=encode(value);data!=nil||opcCode(err)!="MISSING_PART"{t.Fatal("edited package retained a dangling preserved link",err)}
}

// TestTypedRequiredExtensions rejects explicitly required unknown capabilities even under optional preservation.
func TestTypedRequiredExtensions(t *testing.T){
  data,err:=os.ReadFile("fixtures/extension.zip");if err!=nil{t.Fatal(err)};sentinel:=errors.New("required capability callback sentinel")
  for _,mode:=range []string{"part-required","relationship-required","part-error","relationship-error"}{
    t.Run(mode,func(t *testing.T){
      policy:=SerializerPackageExtensionPolicy{Parts:SerializerPackagePreserve,Relationships:SerializerPackagePreserve}
      if strings.HasPrefix(mode,"part-"){policy.RequiredPart=func(SerializerPackagePartMetadata)(bool,error){if strings.HasSuffix(mode,"error"){return false,sentinel};return true,nil}}else{policy.RequiredRelationship=func(SerializerPackageRelationship)(bool,error){if strings.HasSuffix(mode,"error"){return false,sentinel};return true,nil}}
      value,err:=opcProfileLoad(data,opcMainRole,policy,nil)
      if value!=nil{t.Fatal("required/errored capability published a candidate")}
      if strings.HasSuffix(mode,"error"){if err!=sentinel{t.Fatal("capability callback identity changed",err)}}else if opcCode(err)!="UNSUPPORTED_FEATURE"{t.Fatal("required unknown capability accepted",err)}
    })
  }
}

// TestTypedMiddleFailure proves two actual native decode calls cannot publish the first valid occurrence.
func TestTypedMiddleFailure(t *testing.T){
  data,err:=os.ReadFile("fixtures/collection-middle-invalid.zip");if err!=nil{t.Fatal(err)};budget,err:=SerializerPackageNewBudget();if err!=nil{t.Fatal(err)};loader,err:=srlPackageNewLoader(data,budget);if err!=nil{t.Fatal(err)}
  calls:=0;result,err:=loader.models(opcCollectionRole,func(data []byte,budget *SerializerPackageBudget)(any,error){calls++;return opcDecodeModel(data,budget)})
  if result!=nil||opcCode(err)!="CODEC"||calls!=2||budget.held!=0{t.Fatal("middle failure escaped transactional aggregate",result,err,calls,budget.held)}
  if values,failure:=loader.models(opcCollectionRole,opcDecodeModel);values!=nil||opcCode(failure)!="INVALID_STATE"{t.Fatal("failed loader resumed",failure)}
}

// TestTypedCallbackFirstFailure checks abandonment/panic fences and exact earliest domain/ledger errors.
func TestTypedCallbackFirstFailure(t *testing.T){
  data,err:=os.ReadFile("fixtures/profile-descriptor-0-wide-0.zip");if err!=nil{t.Fatal(err)}
  first,later:=errors.New("first domain/ledger sentinel"),errors.New("later callback sentinel")
  for _,mode:=range []string{"decode-close","decode-reentry","decode-panic","domain-close","domain-first","domain-ledger-first"}{
    t.Run(mode,func(t *testing.T){
      armed:=false;limits:=SerializerPackageDefaultLimits();limits.CheckCancelled=func()error{if armed{return first};return nil};budget,err:=SerializerPackageNewBudget(limits);if err!=nil{t.Fatal(err)};loader,err:=srlPackageNewLoader(data,budget);if err!=nil{t.Fatal(err)};defer loader.close()
      decode:=opcDecodeModel
      if strings.HasPrefix(mode,"decode-"){decode=func(data []byte,budget *SerializerPackageBudget)(any,error){
        if mode=="decode-panic"{panic(first)}else if mode=="decode-close"{loader.close()}else{if _,err:=loader.models(opcMainRole,opcDecodeModel);opcCode(err)!="INVALID_STATE"{t.Fatal("reentry did not fail",err)}};return nil,later
      }}
      var values []srlPackageModelOccurrence
      if mode=="decode-panic"{
        func(){defer func(){if recover()!=first{t.Fatal("native callback panic changed identity")}}();values,err=loader.models(opcMainRole,decode)}()
        if values!=nil||budget.held!=0{t.Fatal("panic abandoned private bytes",values,budget.held)}
      }else{
        values,err=loader.models(opcMainRole,decode)
        if strings.HasPrefix(mode,"decode-"){if values!=nil||opcCode(err)!="INVALID_STATE"{t.Fatal("decode callback failure replaced earlier abandonment",err)}}else{
          if err!=nil{t.Fatal(err)};if _,err=loader.binaries(opcAttachmentRole);err!=nil{t.Fatal(err)}
          err=loader.finish(SerializerPackageExtensionPolicy{},nil,func(budget *SerializerPackageBudget)error{
            if mode=="domain-close"{loader.close();return later};if mode=="domain-ledger-first"{armed=true;if budget.Check()!=first{t.Fatal("first ledger failure missing")};return later};return first
          })
          if mode=="domain-close"{if opcCode(err)!="INVALID_STATE"{t.Fatal("domain closure replaced",err)}}else if err!=first{t.Fatal("original domain/ledger error identity changed",err)}
        }
      }
      if values,failure:=loader.models(opcMainRole,opcDecodeModel);values!=nil||opcCode(failure)!="INVALID_STATE"{t.Fatal("abandoned loader resumed",failure)}
    })
  }
}

type opcSink struct { data []byte; maximum,closes,flushes int; failure error }
// Write records only accepted bytes and returns an optional exact application transport failure.
func (sink *opcSink) Write(data []byte)(int,error){count:=len(data);if sink.maximum!=0&&count>sink.maximum{count=sink.maximum};sink.data=append(sink.data,data[:count]...);return count,sink.failure}
// Close proves generated package writes never take ownership of an application sink.
func (sink *opcSink) Close()error{sink.closes++;return nil}
// Flush proves generated package writes do not commit unrelated application transport state.
func (sink *opcSink) Flush()error{sink.flushes++;return nil}

// TestTypedPublicFacade checks actual emitted owning/sink entry points and pre-I/O options validation.
func TestTypedPublicFacade(t *testing.T){
  data,err:=os.ReadFile("fixtures/profile-descriptor-0-wide-0.zip");if err!=nil{t.Fatal(err)};value,err:=ProfileArchiveLoad(data);if err!=nil{t.Fatal(err)}
  encoded,err:=ProfileArchiveEncode(value);if err!=nil{t.Fatal(err)};if err=os.WriteFile("typed.opc",encoded,0600);err!=nil{t.Fatal(err)}
  sink:=&opcSink{};n,err:=ProfileArchiveWrite(sink,value);if err!=nil||int(n)!=len(encoded)||!bytes.Equal(sink.data,encoded)||sink.closes!=0||sink.flushes!=0{t.Fatal("borrowed complete sink contract changed",n,err)}
  sentinel:=errors.New("public facade sink/domain sentinel")
  for _,failure:=range []error{nil,sentinel}{
    short:=&opcSink{maximum:1,failure:failure};n,err=ProfileArchiveWrite(short,value)
    if n!=1||len(short.data)!=1||err==nil||short.closes!=0||short.flushes!=0{t.Fatal("short/failing borrowed sink accepted",n,err)}
    if failure!=nil&&err!=failure{t.Fatal("sink original error changed",err)}
  }
  candidate,err:=ProfileArchiveLoad(data,ProfileArchiveLoadOptions{Validate:func(*ProfileArchiveValue,*SerializerPackageBudget)error{return sentinel}})
  if candidate!=nil||err!=sentinel{t.Fatal("domain failure published a candidate or changed cause",err)}
  source:=&opcBorrowed{data:bytes.NewReader(data)}
  _,err=ProfileArchiveLoad(SerializerPackageSource{Reader:source,SizeBytes:int64(len(data))},ProfileArchiveLoadOptions{ExtensionPolicy:SerializerPackageExtensionPolicy{Parts:SerializerPackageUnknown(255)}})
  if opcCode(err)!="INVALID_OPTIONS"||source.reads!=0{t.Fatal("invalid load policy reached input",err,source.reads)}
  _,err=ProfileArchiveLoad(SerializerPackageSource{Reader:source,SizeBytes:int64(len(data))},ProfileArchiveLoadOptions{},ProfileArchiveLoadOptions{})
  if opcCode(err)!="INVALID_OPTIONS"||source.reads!=0{t.Fatal("excess load options reached input",err,source.reads)}
}

// TestTypedSavePublication exercises actual generated Save validation and all named filesystem boundaries.
func TestTypedSavePublication(t *testing.T){
  data,err:=os.ReadFile("fixtures/profile-descriptor-0-wide-0.zip");if err!=nil{t.Fatal(err)};value,err:=ProfileArchiveLoad(data);if err!=nil{t.Fatal(err)}
  old:=[]byte("previous complete document");sentinel:=errors.New("typed publication boundary sentinel")
  for _,point:=range []string{"before_temporary","after_temporary","before_file_sync","after_file_sync","before_replace","after_replace","after_directory_sync"}{
    t.Run(point,func(t *testing.T){
      directory:=t.TempDir();path:=filepath.Join(directory,"complete.opc");if err=os.WriteFile(path,old,0600);err!=nil{t.Fatal(err)}
      err=ProfileArchiveSave(path,value,ProfileArchiveSaveOptions{Fault:func(actual string)error{if actual==point{return sentinel};return nil}})
      contents,readErr:=os.ReadFile(path);entries,listErr:=os.ReadDir(directory)
      if readErr!=nil||listErr!=nil||len(entries)!=1{t.Fatal("typed Save leaked staging/handles",readErr,listErr,len(entries))}
      if point=="after_replace"||point=="after_directory_sync"{
        if opcCode(err)!="PUBLICATION_UNCERTAIN"||!errors.Is(err,sentinel){t.Fatal("postreplace acknowledgement classification changed",err)}
        candidate,failure:=ProfileArchiveLoad(contents);if failure!=nil||candidate.Main.Name!="independent"{t.Fatal("postreplace file is not a complete generation",failure)}
      }else if err!=sentinel||!bytes.Equal(contents,old){t.Fatal("prereplace failure changed final document or original cause",err)}
    })
  }
  directory:=t.TempDir();path:=filepath.Join(directory,"complete.opc");if err=os.WriteFile(path,old,0600);err!=nil{t.Fatal(err)};faultCalls:=0
  err=ProfileArchiveSave(path,value,ProfileArchiveSaveOptions{Validate:func(*ProfileArchiveValue,*SerializerPackageBudget)error{return sentinel},Fault:func(string)error{faultCalls++;return nil}})
  contents,readErr:=os.ReadFile(path);entries,listErr:=os.ReadDir(directory)
  if err!=sentinel||readErr!=nil||listErr!=nil||!bytes.Equal(contents,old)||faultCalls!=0||len(entries)!=1{t.Fatal("domain failure reached filesystem publication",err,faultCalls)}
}

// TestMetadataScratchAdmission independently fences temporary MIME/MCE ownership before native work.
func TestMetadataScratchAdmission(t *testing.T){
  t.Run("mime-before-native-codec",func(t *testing.T){
    value:=&ProfileDynamicMimeArchiveInput{Main:[]SerializerPackageModelPart[*ProfileModel]{{Value:&ProfileModel{Name:string([]byte{255})},ContentType:"application/example+xml;profile=\""+strings.Repeat("a",4096)+"\""}}}
    for _,capacity:=range []int64{1024,1024*1024}{
      limits:=SerializerPackageDefaultLimits();limits.MaxBufferBytes=capacity;budget,err:=SerializerPackageNewBudget(limits);if err!=nil{t.Fatal(err)}
      data,err:=ProfileDynamicMimeArchiveEncode(value,budget);expected:="RESOURCE_LIMIT";if capacity>1024{expected="CODEC"}
      if data!=nil||opcCode(err)!=expected||budget.held!=0{t.Fatalf("MIME scratch capacity%d: expected%s before native codec, got%v held%d",capacity,expected,err,budget.held)}
    }
  })
  t.Run("mce-flatten-and-one-tight",func(t *testing.T){
    data,err:=os.ReadFile("fixtures/mce-buffer-large.zip");if err!=nil{t.Fatal(err)}
    accepts:=func(capacity int64)bool{
      limits:=SerializerPackageDefaultLimits();limits.MaxBufferBytes=capacity;budget,err:=SerializerPackageNewBudget(limits);if err!=nil{t.Fatal(err)}
      reader,err:=SerializerPackageOpenOPC(data,budget);if reader!=nil{reader.Close()}
      if budget.held!=0{t.Fatal("MCE scratch escaped close/rejection",budget.held)}
      if err!=nil&&opcCode(err)!="RESOURCE_LIMIT"{t.Fatal("MCE quota changed a framing/metadata decision",err)};return err==nil
    }
    if accepts(64*1024){t.Fatal("MCE copied live metadata outside the64KiB private-buffer grant")}
    lower,upper:=int64(64*1024),int64(256*1024);if !accepts(upper){t.Fatal("positive MCE buffer control rejected")}
    for upper-lower>1{middle:=lower+(upper-lower)/2;if accepts(middle){upper=middle}else{lower=middle}}
    if !accepts(upper)||accepts(upper-1){t.Fatal("exact/one-tight MCE scratch admission inconsistent",upper)}
  })
}
