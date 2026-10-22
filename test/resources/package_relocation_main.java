// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package consumer;

import generated.Generated.Relocation.Archive;
import generated.Generated.SerializerPackage;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HexFormat;

/** Exercise only public owning package APIs against independently authored relocation inputs. */
public final class RelocationMain {
  private static int checks;
  private static final byte[] PREVIOUS = "previous destination".getBytes(StandardCharsets.UTF_8);

  /** Keep assertions active under the actual optimized SDK invocation. */
  private static void check(boolean condition,String message) {
    if(!condition)throw new AssertionError(message);
    ++checks;
  }

  /** Snapshot public extension values without reading private canonicalization provenance. */
  private static String snapshot(SerializerPackage.Extensions extensions) {
    var text=new StringBuilder();
    for(var part:extensions.parts)text.append(part.name).append('|').append(part.contentType).append('|').append(part.sourceIndex).append('|').append(part.compression).append('|').append(HexFormat.of().formatHex(part.data)).append('\n');
    for(var link:extensions.relationships)text.append(link.source).append('|').append(link.id).append('|').append(link.type).append('|').append(link.target).append('|').append(link.mode).append('\n');
    return text.toString();
  }

  /** Admit exactly the independently observed cumulative work/allocation and reject one less. */
  private static void bounds(Archive.Input input) throws Exception {
    var observed=new SerializerPackage.Budget();Archive.encode(input,observed);
    for(var resource:new SerializerPackage.Resource[]{SerializerPackage.Resource.WORK_UNITS,SerializerPackage.Resource.DECODER_ALLOCATION_BYTES}) {
      var defaults=observed.configured();long maximum=resource==SerializerPackage.Resource.WORK_UNITS?defaults.maxWorkUnits:defaults.maxDecoderAllocationBytes;
      long used=maximum-observed.remaining(resource);check(used>0,"positive resource inventory");
      for(long allowance:new long[]{used,used-1}) {
        var limits=new SerializerPackage.Limits();if(resource==SerializerPackage.Resource.WORK_UNITS)limits.maxWorkUnits=allowance;else limits.maxDecoderAllocationBytes=allowance;
        boolean rejected=false;
        try{Archive.encode(input,new SerializerPackage.Budget(limits));}
        catch(SerializerPackage.PackageError error){check(error.code==SerializerPackage.Code.RESOURCE_LIMIT,"bounded failure classification");rejected=true;}
        check(rejected==(allowance<used),"exact and one-tight cumulative bound");
      }
    }
  }

  /** Preserve the first caller exception and the old destination on cancelled Save. */
  private static void cancellation(Archive.Input input,Path path) throws Exception {
    Files.write(path,PREVIOUS);var original=new IllegalStateException("caller cancellation identity");boolean[] armed={false};
    var limits=new SerializerPackage.Limits();limits.checkCancelled=()->{if(armed[0])throw original;};var budget=new SerializerPackage.Budget(limits);armed[0]=true;
    try{Archive.save(path,input,budget,null,null);throw new AssertionError("cancelled Save accepted");}
    catch(IllegalStateException error){check(error==original,"exact caller exception identity");}
    check(Arrays.equals(Files.readAllBytes(path),PREVIOUS),"cancelled Save retains destination");
  }

  /** Load one complete value, reuse its extension provenance and publish only accepted archives. */
  public static void main(String[] args) throws Exception {
    Path directory=Path.of(args[0]);String name=args[1],action=args[2],expected=args[3];
    var policy=new SerializerPackage.ExtensionPolicy(SerializerPackage.Unknown.PRESERVE,SerializerPackage.Unknown.PRESERVE,null,null);
    SerializerPackage.Result<Archive.Value> loaded;
    try{loaded=Archive.load(Files.readAllBytes(directory.resolve(name+".opc")),new SerializerPackage.Budget(),policy,null);}
    catch(SerializerPackage.PackageError error){check(expected.equals("LOAD_ERROR"),"unexpected load rejection");System.out.println("{\"case\":\""+name+"\",\"decision\":\""+expected+"\",\"checks\":"+checks+",\"failure_stage\":\"load\"}");return;}
    check(!expected.equals("LOAD_ERROR"),"invalid load published");check(loaded.value.extensions==loaded.extensions,"owning result extension identity");
    check(loaded.value.first.id==11&&loaded.value.first.label.equals("first"),"first typed payload");
    check(loaded.value.second.id==22&&loaded.value.second.label.equals("second"),"second typed payload");
    check(loaded.value.children.size()>=1&&loaded.value.children.get(0).value.id==33&&loaded.value.children.get(0).sourceIndex==0,"dynamic child routing");
    var input=new Archive.Input();input.first=loaded.value.first;input.second=action.equals("remove_second")?null:loaded.value.second;input.children=loaded.value.children;input.extensions=loaded.extensions;
    if(name.startsWith("dynamic-")){
      String original="/children/child.xml";var names=new java.util.HashMap<String,String>();if(action.equals("swap_children")){names.put(original,"/children/other.xml");names.put("/children/other.xml",original);}else if(!action.equals("remove_unreferenced_child")){names.put(original,action.equals("collision_child")?"/assets/blob.bin":"/renamed/child.xml");}
      input.extensions=loaded.extensions.withRenames(names);var children=new java.util.ArrayList<SerializerPackage.ModelPart<generated.Generated.Relocation.Payload>>();for(var item:loaded.value.children)children.add(new SerializerPackage.ModelPart<>(item.value,names.getOrDefault(item.name,item.name),item.contentType,item.sourceIndex));if(action.equals("remove_child")||action.equals("remove_unreferenced_child"))children.remove(0);if(action.equals("swap_children"))java.util.Collections.reverse(children);input.children=children;
    }
    String before=snapshot(input.extensions);String childName=input.children.get(0).name;
    Path saved=directory.resolve(name+"-saved.opc");Files.write(saved,PREVIOUS);
    if(expected.equals("REJECT")) {
      boolean rejected=false;try{Archive.encode(input);}catch(SerializerPackage.PackageError error){rejected=true;}check(rejected,"invalid relocation encoded");
      rejected=false;try{Archive.save(saved,input,new SerializerPackage.Budget(),null,null);}catch(SerializerPackage.PackageError error){rejected=true;}check(rejected,"invalid relocation saved");
      check(Arrays.equals(Files.readAllBytes(saved),PREVIOUS),"failed Save retains destination");
    } else {
      check(expected.equals("OK"),"unknown decision");Files.write(directory.resolve(name+"-encoded.opc"),Archive.encode(input));Files.write(directory.resolve(name+"-repeat.opc"),Archive.encode(input));
      Archive.save(saved,input,new SerializerPackage.Budget(),null,null);
      if(name.equals("alternate")){bounds(input);cancellation(input,directory.resolve(name+"-cancel.opc"));}
    }
    check(before.equals(snapshot(input.extensions)),"extension input mutated");check(childName.equals(input.children.get(0).name),"dynamic child renamed");check(input.first.id==11&&input.first.label.equals("first"),"typed model mutated");
    System.out.println("{\"case\":\""+name+"\",\"decision\":\""+expected+"\",\"checks\":"+checks+"}");
  }
}
