// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using P=SerializerGenerated.Schema.SerializerPackage;
using A=SerializerGenerated.Schema.PipelineArchive;

// Consume independent OPC fixtures through actual generated .NET pipeline APIs.
internal static class PipelineMain {
  private static int checks;
  // Keep SDK assertions enabled in Release builds.
  private static void Check(bool condition,string message){if(!condition)throw new InvalidOperationException(message);++checks;}
  // Load complete values, preserve inert external links and validate private Save publication.
  public static void Main(string[] arguments){
    string directory=arguments[0],name=arguments[1],codec=arguments[2],decision=arguments[3];
    var profiles=new Dictionary<string,P.PipelineProfile>{{"main",new(Codec:codec)}};
    byte[] source=File.ReadAllBytes(Path.Combine(directory,name+".opc"));var decoded=new P.Budget();
    A.Value value;
    try{value=A.LoadWithPipelineProfiles(source,profiles,decoded).Value;}
    catch(P.PackageError failure){Check(decision=="LOAD_ERROR","unexpected load rejection: "+failure);Console.WriteLine("{\"decision\":\"LOAD_ERROR\",\"checks\":"+checks+"}");return;}
    Check(decision=="OK","invalid fixture published");
    Check(value.Main?.Id==17&&value.Main.Label=="pipeline"&&value.Main.Values.Count==3,"native model values");
    Check(value.Links.Count==1&&value.Links[0].ContentType==""&&value.Links[0].Data.Length==0,"external role remains inert");
    value.PipelineOverrides=profiles;
    var measured=new P.Budget();byte[] output=A.Encode(value,measured);
    File.WriteAllBytes(Path.Combine(directory,name+"-encoded.opc"),output);
    A.Save(Path.Combine(directory,name+"-saved.opc"),value);
    foreach(bool decode in new[]{false,true})foreach(var resource in new[]{P.Resource.WorkUnits,P.Resource.DecoderAllocationBytes}){
      long used=(decode?decoded:measured).Used(resource);Check(used>0,"native operation admission");
      for(int tight=0;tight<2;++tight){var limits=resource==P.Resource.WorkUnits?new P.PackageLimits{MaxWorkUnits=used-tight}:new P.PackageLimits{MaxDecoderAllocationBytes=used-tight};
        try{if(decode)A.LoadWithPipelineProfiles(source,profiles,new P.Budget(limits));else A.Encode(value,new P.Budget(limits));Check(tight==0,"one-tight quota accepted");}
        catch(P.PackageError failure){Check(tight==1&&failure.Code==P.Code.ResourceLimit,"exact quota or failure classification");}
      }
    }
    var marker=new NotSupportedException("caller cancellation");var interrupted=new P.PackageLimits{IsCancelled=()=>throw marker};
    foreach(bool decode in new[]{false,true})try{if(decode)A.LoadWithPipelineProfiles(source,profiles,new P.Budget(interrupted));else A.Encode(value,new P.Budget(interrupted));throw new InvalidOperationException("cancellation ignored");}catch(NotSupportedException failure){Check(ReferenceEquals(failure,marker),"caller cancellation identity");}
    foreach(var invalid in new[]{new Dictionary<string,P.PipelineProfile>{{"fixed",new(Codec:"binary_none")}},new Dictionary<string,P.PipelineProfile>{{"fixed",new(Compression:"deflate")}},new Dictionary<string,P.PipelineProfile>{{"missing",new(Compression:"stored")}}}){
      try{A.SelectProfiles(null,invalid);throw new InvalidOperationException("invalid profile accepted");}catch(P.PackageError){++checks;}
    }
    string destination=Path.Combine(directory,name+"-atomic.opc");byte[] previous=System.Text.Encoding.UTF8.GetBytes("previous destination");File.WriteAllBytes(destination,previous);
    value.Links=new[]{value.Links[0] with{Data=new byte[]{1}}};
    try{A.Save(destination,value);throw new InvalidOperationException("external payload published");}
    catch(P.PackageError){Check(File.ReadAllBytes(destination).SequenceEqual(previous),"failed Save replaced destination");}
    Console.WriteLine("{\"decision\":\"OK\",\"checks\":"+checks+"}");
  }
}
