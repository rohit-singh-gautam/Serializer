// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import("bytes";"encoding/json";"errors";"fmt";"os";"path/filepath";m "serializer-relocation/model")

var checks int

// check keeps native SDK assertions enabled independently of compiler optimization.
func check(condition bool,message string){if !condition{panic(message)};checks++}
// snapshot retains all exported model and preservation bytes independently of subsequent native calls.
func snapshot(value any)string{data,err:=json.Marshal(value);if err!=nil{panic(err)};return string(data)}
// budget selects an independent public ledger with no private implementation access.
func budget(limits ...m.@p@Limits)*m.@p@Budget{value,err:=m.@p@NewBudget(limits...);if err!=nil{panic(err)};return value}
// bounds exercises exact measured public admission and transactional one-tight failures.
func bounds(value *m.RelocationArchive){
    measured:=budget();_,err:=m.RelocationArchiveEncode(value,measured);if err!=nil{panic(err)};defaults:=m.@p@DefaultLimits();before:=snapshot(value)
    for _,resource:=range []m.@p@Resource{m.@p@WorkUnits,m.@p@DecoderAllocationBytes}{for tight:=int64(0);tight<2;tight++{
        limits:=defaults;if resource==m.@p@WorkUnits{limits.MaxWorkUnits=defaults.MaxWorkUnits-measured.Remaining(resource)-tight}else{limits.MaxDecoderAllocationBytes=defaults.MaxDecoderAllocationBytes-measured.Remaining(resource)-tight}
        _,err=m.RelocationArchiveEncode(value,budget(limits));check((err==nil)==(tight==0),"exact and one-tight canonical output admission");if err!=nil{var owned *m.@p@Error;check(errors.As(err,&owned)&&owned.Code==m.@p@ResourceLimit,"owned resource classification")};check(snapshot(value)==before,"bounded encode changed caller state")
    }}
}
// main exercises explicit canonical publication through the real external Go package API.
func main(){
    check(len(os.Args)==5,"fixture arguments");directory,name,action,expected:=os.Args[1],os.Args[2],os.Args[3],os.Args[4];source,err:=os.ReadFile(filepath.Join(directory,name+".opc"));if err!=nil{panic(err)}
    policy:=m.@p@ExtensionPolicy{Parts:m.@p@Preserve,Relationships:m.@p@Preserve};options:=m.RelocationArchiveLoadOptions{nil,policy,nil}
    _=m.@p@Extensions{nil,nil};_=m.RelocationArchive{nil,nil,nil,m.@p@Extensions{nil,nil}}
    saved:=filepath.Join(directory,name+"-saved.opc");previous:=[]byte("previous destination");if err=os.WriteFile(saved,previous,0600);err!=nil{panic(err)}
    legacy,err:=m.RelocationArchiveLoad(source,options);if err!=nil{check(expected=="LOAD_ERROR","unexpected original load rejection");fmt.Printf("{\"case\":%q,\"decision\":%q,\"checks\":%d,\"failure_stage\":\"load\"}\n",name,expected,checks);return};check(expected!="LOAD_ERROR","invalid duplicate origin published")
    original:=snapshot(legacy);value,err:=m.@selected@LoadCanonical(source,options)
    if err!=nil{check(expected=="REJECT","unexpected canonical load rejection");after,_:=os.ReadFile(saved);check(bytes.Equal(after,previous)&&snapshot(legacy)==original,"canonical load failure changed prior state");fmt.Printf("{\"case\":%q,\"decision\":%q,\"checks\":%d,\"failure_stage\":\"canonical_load\"}\n",name,expected,checks);return}
    check(snapshot(legacy)==original,"explicit canonical load mutated separately loaded extensions");check(value.First.Id==11&&value.First.Label=="first"&&len(value.Children)==1&&value.Children[0].Value.Id==33&&value.Children[0].SourceIndex==0,"typed identities and child routing")
    if action=="remove_second"{value.Second=nil};before:=snapshot(value)
    if expected=="REJECT"{_,err=m.RelocationArchiveEncode(value);check(err!=nil,"invalid canonical encode published");err=m.RelocationArchiveSave(saved,value);check(err!=nil,"invalid canonical Save published");after,_:=os.ReadFile(saved);check(bytes.Equal(after,previous),"rejected Save changed destination")}else{
        check(expected=="OK","unknown decision");encoded,failure:=m.RelocationArchiveEncode(value);if failure!=nil{panic(failure)};if err=os.WriteFile(filepath.Join(directory,name+"-encoded.opc"),encoded,0600);err!=nil{panic(err)}
        repeated,failure:=m.RelocationArchiveLoad(encoded,options);if failure!=nil{panic(failure)};again,failure:=m.RelocationArchiveEncode(repeated);if failure!=nil{panic(failure)};check(bytes.Equal(encoded,again),"canonical output is not idempotent");if err=os.WriteFile(filepath.Join(directory,name+"-repeat.opc"),again,0600);err!=nil{panic(err)}
        if err=m.RelocationArchiveSave(saved,value);err!=nil{panic(err)}
        if name=="alternate"{bounds(value);calls:=0;typed,failure:=m.@selected@LoadCanonicalWithCodecs(source,options,func(data []byte,xmlOptions m.XmlOptions,part,id string)(*m.RelocationPayload,error){calls++;check(part=="/alternate/first.xml"&&id=="authored_first","selected decoder origin or ID changed");return m.DecodeRelocationPayloadXML(data,xmlOptions)},nil,nil);if failure!=nil{panic(failure)};check(calls==1&&snapshot(typed)==snapshot(value),"canonical selected decoder changed candidate");var written bytes.Buffer;_,failure=m.RelocationArchiveWrite(&written,value);check(failure==nil&&bytes.Equal(written.Bytes(),encoded),"borrowed canonical writer changed bytes")
            marker:=errors.New("caller cancellation identity");failed,failure:=m.@selected@LoadCanonicalWithCodecs(source,options,func([]byte,m.XmlOptions,string,string)(*m.RelocationPayload,error){return nil,marker},nil,nil);check(failed==nil&&failure==marker,"selected canonical decoder failure identity or publication");polls:=0;limits:=m.@p@DefaultLimits();limits.CheckCancelled=func()error{polls++;if polls>=7{return marker};return nil};if err=os.WriteFile(saved,previous,0600);err!=nil{panic(err)}
            err=m.RelocationArchiveSave(saved,value,m.RelocationArchiveSaveOptions{Budget:budget(limits)});after,_:=os.ReadFile(saved);check(err==marker&&bytes.Equal(after,previous),"cancelled Save lost exact caller error or destination")
            err=m.RelocationArchiveSave(saved,value,m.RelocationArchiveSaveOptions{Fault:func(point string)error{if point=="before_replace"{return marker};return nil}});after,_=os.ReadFile(saved);check(err==marker&&bytes.Equal(after,previous),"faulted Save lost caller identity or old destination");if err=m.RelocationArchiveSave(saved,value);err!=nil{panic(err)}
        }
    };check(snapshot(value)==before,"canonical output mutated caller model or retained records");fmt.Printf("{\"case\":%q,\"decision\":%q,\"checks\":%d}\n",name,expected,checks)
}
