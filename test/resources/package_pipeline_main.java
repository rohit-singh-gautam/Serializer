// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package consumer;

import generated.Generated.Pipeline.Archive;
import generated.Generated.SerializerPackage;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Map;

/** Drive actual generated Java part pipelines against independently authored OPC fixtures. */
public final class PipelineMain {
  private static int checks;
  /** Keep runtime assertions enabled for every actual SDK invocation. */
  private static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);++checks;}
  /** Consume one complete fixture and independently staged output without partial publication. */
  public static void main(String[] arguments)throws Exception{
    Path directory=Path.of(arguments[0]);String name=arguments[1],codec=arguments[2],decision=arguments[3];
    var profiles=Map.of("main",new SerializerPackage.PipelineProfile(codec,null));
    byte[] source=Files.readAllBytes(directory.resolve(name+".opc"));var decoded=new SerializerPackage.Budget();
    Archive.Value value;
    try{value=Archive.loadWithPipelineProfiles(source,decoded,null,null,profiles).value;}
    catch(SerializerPackage.PackageError failure){check(decision.equals("LOAD_ERROR"),"unexpected load rejection: "+failure+" cause "+failure.getCause());System.out.println("{\"decision\":\"LOAD_ERROR\",\"checks\":"+checks+"}");return;}
    check(decision.equals("OK"),"invalid fixture published");
    check(value.main.id==17&&value.main.label.equals("pipeline")&&value.main.values.length==3,"native model values");
    check(value.links.size()==1&&value.links.get(0).contentType.isEmpty()&&value.links.get(0).data.length==0,"external role remains inert");
    var measured=new SerializerPackage.Budget();byte[] output=Archive.encodeWithPipelineProfiles(value,measured,profiles);
    Files.write(directory.resolve(name+"-encoded.opc"),output);
    Archive.saveWithPipelineProfiles(directory.resolve(name+"-saved.opc"),value,new SerializerPackage.Budget(),null,null,profiles);
    for(boolean decode:new boolean[]{false,true})for(var resource:new SerializerPackage.Resource[]{SerializerPackage.Resource.WORK_UNITS,SerializerPackage.Resource.DECODER_ALLOCATION_BYTES}){
      long maximum=resource==SerializerPackage.Resource.WORK_UNITS?new SerializerPackage.Limits().maxWorkUnits:new SerializerPackage.Limits().maxDecoderAllocationBytes;
      long used=maximum-(decode?decoded:measured).remaining(resource);check(used>0,"native operation admission");
      for(int tight=0;tight<2;++tight){var limits=new SerializerPackage.Limits();if(resource==SerializerPackage.Resource.WORK_UNITS)limits.maxWorkUnits=used-tight;else limits.maxDecoderAllocationBytes=used-tight;
        try{if(decode)Archive.loadWithPipelineProfiles(source,new SerializerPackage.Budget(limits),null,null,profiles);else Archive.encodeWithPipelineProfiles(value,new SerializerPackage.Budget(limits),profiles);check(tight==0,"one-tight quota accepted");}
        catch(SerializerPackage.PackageError failure){check(tight==1&&failure.code==SerializerPackage.Code.RESOURCE_LIMIT,"exact quota or failure classification");}
      }
    }
    var marker=new UnsupportedOperationException("caller cancellation");var interrupted=new SerializerPackage.Limits();interrupted.checkCancelled=()->{throw marker;};
    for(boolean decode:new boolean[]{false,true})try{if(decode)Archive.loadWithPipelineProfiles(source,new SerializerPackage.Budget(interrupted),null,null,profiles);else Archive.encodeWithPipelineProfiles(value,new SerializerPackage.Budget(interrupted),profiles);throw new AssertionError("cancellation ignored");}catch(UnsupportedOperationException failure){check(failure==marker,"caller cancellation identity");}
    for(var invalid:java.util.List.of(Map.of("fixed",new SerializerPackage.PipelineProfile("binary_none",null)),Map.of("fixed",new SerializerPackage.PipelineProfile(null,"deflate")),Map.of("missing",new SerializerPackage.PipelineProfile(null,"stored")))){
      try{Archive.encodeWithPipelineProfiles(value,new SerializerPackage.Budget(),invalid);throw new AssertionError("invalid profile accepted");}catch(SerializerPackage.PackageError expected){++checks;}
    }
    Path destination=directory.resolve(name+"-atomic.opc");byte[] previous="previous destination".getBytes(java.nio.charset.StandardCharsets.UTF_8);Files.write(destination,previous);
    var external=value.links.get(0);value.links=java.util.List.of(new SerializerPackage.BinaryPart(new byte[]{1},external.name,"",external.sourceIndex,external.compression));
    try{Archive.saveWithPipelineProfiles(destination,value,new SerializerPackage.Budget(),null,null,profiles);throw new AssertionError("external payload published");}
    catch(SerializerPackage.PackageError expected){check(Arrays.equals(Files.readAllBytes(destination),previous),"failed Save replaced destination");}
    System.out.println("{\"decision\":\"OK\",\"checks\":"+checks+"}");
  }
}
