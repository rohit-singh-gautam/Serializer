// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import("bytes";"encoding/json";"errors";"fmt";"os";"path/filepath";m "serializer-pipeline/model")

var checks int
// check keeps public SDK assertions enabled in optimized consumers.
func check(condition bool,message string){if !condition{panic(message)};checks++}
// main exercises complete generated native/XML profile pipelines and atomic publication failure.
func main(){directory,name,codec,decision:=os.Args[1],os.Args[2],os.Args[3],os.Args[4];input,err:=os.ReadFile(filepath.Join(directory,name+".opc"));if err!=nil{panic(err)};source:=func(data []byte)m.@p@Source{return m.@p@Source{Reader:bytes.NewReader(data),SizeBytes:int64(len(data))}};profiles:=map[string]m.@p@PipelineProfile{"main":{Codec:codec}}
  value,err:=m.PipelineArchiveLoadWithPipelineProfiles(source(input),profiles);if err!=nil{check(decision=="LOAD_ERROR","unexpected load rejection: "+fmt.Sprint(err,errors.Unwrap(err)));fmt.Println(`{"decision":"LOAD_ERROR","checks":1}`);return};check(decision=="OK","invalid fixture published");check(value.Main.Id==17&&value.Main.Label=="pipeline"&&len(value.Main.Values)==3,"native model values");check(len(value.Links)==1&&value.Links[0].ContentType==""&&len(value.Links[0].Data)==0,"external role remains inert")
  measured,err:=m.@p@NewBudget();if err!=nil{panic(err)};output,err:=m.PipelineArchiveEncodeWithPipelineProfiles(value,profiles,measured);if err!=nil{panic(err)};if err=os.WriteFile(filepath.Join(directory,name+"-encoded.opc"),output,0600);err!=nil{panic(err)};if err=m.PipelineArchiveSaveWithPipelineProfiles(filepath.Join(directory,name+"-saved.opc"),value,profiles,m.PipelineArchiveSaveOptions{});err!=nil{panic(err)}
  defaults:=m.@p@DefaultLimits();for _,resource:=range []m.@p@Resource{m.@p@WorkUnits,m.@p@DecoderAllocationBytes}{for tight:=int64(0);tight<2;tight++{limits:=defaults;if resource==m.@p@WorkUnits{limits.MaxWorkUnits-=measured.Remaining(resource)+tight}else{limits.MaxDecoderAllocationBytes-=measured.Remaining(resource)+tight};budget,err:=m.@p@NewBudget(limits);if err!=nil{panic(err)};_,err=m.PipelineArchiveEncodeWithPipelineProfiles(value,profiles,budget);check((err==nil)==(tight==0),"exact/one-tight native operation admission");if err!=nil{var failure *m.@p@Error;check(errors.As(err,&failure)&&failure.Code==m.@p@ResourceLimit,"native resource classification")}}}
  for _,invalid:=range []map[string]m.@p@PipelineProfile{{"fixed":{Codec:"binary_none"}},{"fixed":{Compression:"deflate"}},{"missing":{Compression:"stored"}}}{_,err=m.PipelineArchiveEncodeWithPipelineProfiles(value,invalid);check(err!=nil,"closed fixed/default profile")}
  destination:=filepath.Join(directory,name+"-atomic.opc");previous:=[]byte("previous destination");if err=os.WriteFile(destination,previous,0600);err!=nil{panic(err)};value.Links[0].Data=[]byte{1};err=m.PipelineArchiveSaveWithPipelineProfiles(destination,value,profiles,m.PipelineArchiveSaveOptions{});check(err!=nil,"external payload published");actual,err:=os.ReadFile(destination);check(err==nil&&bytes.Equal(actual,previous),"failed Save replaced destination");result,_:=json.Marshal(map[string]any{"decision":"OK","checks":checks});fmt.Println(string(result))
}
