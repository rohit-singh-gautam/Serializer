// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.IO;
using System.Linq;
using System.Text;
using P=SerializerGenerated.Schema.SerializerPackage;
using A=SerializerGenerated.Schema.RelocationArchive;

// Exercise public generated package APIs against independently authored relocation records.
internal static class RelocationMain {
  private static int checks;
  private static readonly byte[] Previous=Encoding.UTF8.GetBytes("previous destination");

  // Keep assertions active in the optimized SDK build.
  private static void Check(bool value,string message){if(!value)throw new InvalidOperationException(message);++checks;}

  // Snapshot public records and opaque bytes without inspecting private provenance.
  private static string Snapshot(P.Extensions extensions){
    var text=new StringBuilder();
    foreach(var part in extensions.Parts)text.Append(part.Name).Append('|').Append(part.ContentType).Append('|').Append(part.SourceIndex).Append('|').Append(part.Compression).Append('|').Append(Convert.ToHexString(part.Data)).Append('\n');
    foreach(var link in extensions.Relationships)text.Append(link.Source).Append('|').Append(link.Id).Append('|').Append(link.Type).Append('|').Append(link.Target).Append('|').Append(link.Mode).Append('\n');
    return text.ToString();
  }

  // Require a package rejection rather than treating an arbitrary assertion failure as success.
  private static void Rejected(Action action){try{action();}catch(P.PackageError){++checks;return;}throw new InvalidOperationException("Invalid relocation accepted");}

  // Replay independently observed cumulative admissions at exact and one-tight policies.
  private static void Bounds(A.Input input){
    var observed=new P.Budget();A.Encode(input,observed);
    foreach(var resource in new[]{P.Resource.WorkUnits,P.Resource.DecoderAllocationBytes}){
      long maximum=resource==P.Resource.WorkUnits?observed.Limits.MaxWorkUnits:observed.Limits.MaxDecoderAllocationBytes;
      long used=maximum-observed.Remaining(resource);Check(used>0,"Positive relocation admission");
      foreach(long allowance in new[]{used,used-1}){
        var limits=resource==P.Resource.WorkUnits?new P.PackageLimits{MaxWorkUnits=allowance}:new P.PackageLimits{MaxDecoderAllocationBytes=allowance};
        bool failed=false;
        try{A.Encode(input,new P.Budget(limits));}catch(P.PackageError error){Check(error.Code==P.Code.ResourceLimit,"Quota classification");failed=true;}
        Check(failed==(allowance<used),"Exact and one-tight cumulative policy");
      }
    }
  }

  // Cancellation must retain the exact caller object and leave the old file intact.
  private static void Cancellation(A.Input input,string path){
    File.WriteAllBytes(path,Previous);var marker=new InvalidOperationException("caller identity");bool armed=false;
    var limits=new P.PackageLimits{IsCancelled=()=>{if(armed)throw marker;return false;}};
    var budget=new P.Budget(limits);armed=true;
    try{A.Save(path,input,budget);throw new Exception("Cancelled Save accepted");}
    catch(InvalidOperationException error){Check(ReferenceEquals(error,marker),"Exact cancellation object");}
    Check(File.ReadAllBytes(path).SequenceEqual(Previous),"Cancelled Save retained destination");
  }

  // Report actual public load/encode/Save decisions without assuming a backend's failure phase.
  private static void Main(string[] args){
    string directory=args[0],name=args[1],action=args[2],expected=args[3];
    string destination=Path.Combine(directory,name+"-saved.opc");File.WriteAllBytes(destination,Previous);
    P.Result<A.Value> loaded;
    try{loaded=A.Load(Path.Combine(directory,name+".opc"),policy:new(P.Unknown.Preserve,P.Unknown.Preserve));}
    catch(P.PackageError){
      Check(expected=="LOAD_ERROR"||expected=="REJECT","Unexpected load failure");
      Check(File.ReadAllBytes(destination).SequenceEqual(Previous),"Rejected load retained destination");
      Console.WriteLine(System.Text.Json.JsonSerializer.Serialize(new{@case=name,decision=expected,checks,failure_stage="load"}));return;
    }
    Check(expected!="LOAD_ERROR","Invalid load published");
    Check(loaded.Value.First?.Id==11&&loaded.Value.First.Label=="first","First payload");
    Check(loaded.Value.Second?.Id==22&&loaded.Value.Second.Label=="second","Second payload");
    Check(loaded.Value.Children.Count==1&&loaded.Value.Children[0].Value.Id==33&&loaded.Value.Children[0].SourceIndex==0,"Dynamic child routing");
    var input=new A.Input{First=loaded.Value.First,Second=action=="remove_second"?null:loaded.Value.Second,Children=loaded.Value.Children,Extensions=loaded.Extensions};
    string before=Snapshot(input.Extensions),child=input.Children[0].Name;
    if(expected=="REJECT"){
      Rejected(()=>A.Encode(input));Rejected(()=>A.Save(destination,input));
      Check(File.ReadAllBytes(destination).SequenceEqual(Previous),"Rejected Save retained destination");
    }else{
      Check(expected=="OK","Unknown expectation");
      File.WriteAllBytes(Path.Combine(directory,name+"-encoded.opc"),A.Encode(input));
      File.WriteAllBytes(Path.Combine(directory,name+"-repeat.opc"),A.Encode(input));A.Save(destination,input);
      if(name=="alternate"){Bounds(input);Cancellation(input,Path.Combine(directory,name+"-cancel.opc"));}
    }
    Check(Snapshot(input.Extensions)==before,"Preserved inputs mutated");Check(input.Children[0].Name==child,"Dynamic child renamed");
    Check(input.First?.Id==11&&input.First.Label=="first","Typed payload mutated");
    Console.WriteLine(System.Text.Json.JsonSerializer.Serialize(new{@case=name,decision=expected,checks}));
  }
}
