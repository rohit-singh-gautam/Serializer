# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify Java/C#/Go live readonly XML exports with independent XSD and constructor tripwires."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
from lxml import etree
import mobile_xml_projection_test as shared
from native_xml_projection_test import XSD as BASE_XSD

ROOT, sdk = shared.ROOT, shared.sdk
NS='urn:serializer:projected'
MODEL=shared.MODEL.rsplit('}',1)[0]+'''
  class nested_envelope { public array<envelope> values; }
  class optional_envelope { public optional<envelope> value; }
  class base { public version uint16 ver { 3 } compatibility { 1 }; created(2) public uint32 code { 7 }; }
  class inherited : public base { public payload nested; public string fallback { "fresh" }; }
  class inherited_source { public payload child;
    public translator inherited as project { nested=child; };
  }
}'''
XSD=BASE_XSD.replace('</xs:schema>','''<xs:element name="larger"><xs:complexType><xs:sequence><xs:element name="values" type="xs:unsignedInt" minOccurs="0" maxOccurs="unbounded"/><xs:element name="nested" type="t:payload"/><xs:element name="fallback" type="xs:string"/></xs:sequence></xs:complexType></xs:element>
<xs:element name="inherited"><xs:complexType><xs:sequence><xs:element name="code" type="xs:unsignedInt"/><xs:element name="nested" type="t:payload"/><xs:element name="fallback" type="xs:string"/></xs:sequence><xs:anyAttribute namespace="urn:serializer:xml:model" processContents="skip"/></xs:complexType></xs:element></xs:schema>''')

JAVA=r'''import java.nio.file.*;import java.util.*;
/** Qualify native live-owner exports while owning target construction is forbidden. */
public final class Main {
  /** Check failure outside the catch so an accidentally successful operation cannot pass. */
  private static void rejected(Runnable operation){try{operation.run();}catch(RuntimeException expected){return;}throw new AssertionError("Invalid projection published");}
  /** Require the readonly graph barrier itself before any parser, provider or candidate constructor. */
  private static void input(Runnable operation){try{operation.run();}catch(IllegalArgumentException expected){if(expected.getMessage().contains("export-only"))return;throw expected;}throw new AssertionError("Readonly input published");}
  /** Keep the weak owner out of the caller's live stack roots before explicit collection. */
  private static Schema.Projected.WeakEnvelope weak(){var source=new Schema.Projected.Source();var value=new Schema.Projected.WeakEnvelope();value.value.bind(source);value.encodeXml();return value;}
  /** Borrow a large source through exact/one-tight output admission and real mid-traversal interruption. */
  private static void largeProjection(Path directory)throws Exception{
    var source=new Schema.Projected.LargerSource();for(int index=0;index<25000;++index)source.values.add(index);source.nested.number=17;
    var values=source.values;var nested=source.nested;var before=new ArrayList<byte[]>();for(var protocol:Schema.Protocol.values())before.add(source.encode(protocol));
    byte[] output;Schema.srlProjectionTripwire=true;
    try{
      output=source.withProject(view->{if(view.getValues()!=values||view.getNested()!=nested)throw new AssertionError("Large getter copied source");return view.encodeXml();});
      var defaults=Schema.Limits.defaults();same(output,source.withProject(view->view.encodeXml(new Schema.Limits(output.length,defaults.maxStringBytes,defaults.maxElements,defaults.maxDepth),null)));
      rejected(()->source.withProject(view->view.encodeXml(new Schema.Limits(output.length-1,defaults.maxStringBytes,defaults.maxElements,defaults.maxDepth),null)));
      var marker=new IllegalStateException("Large projection interrupted");int[] polls={0};var limits=new Schema.Limits(defaults.maxBytes,defaults.maxStringBytes,defaults.maxElements,defaults.maxDepth,defaults.readPolicy,(resource,amount,field)->{if(++polls[0]==100)throw marker;});
      byte[] sentinel={17};byte[][] published={sentinel};try{source.withProject(view->{published[0]=view.encodeXml(limits,null);return null;});throw new AssertionError("Large interruption ignored");}catch(IllegalStateException failure){if(failure!=marker||polls[0]!=100||published[0]!=sentinel)throw new AssertionError("Large interruption changed publication",failure);}
      same(output,source.withProject(view->view.encodeXml()));
    }finally{Schema.srlProjectionTripwire=false;}
    int index=0;for(var protocol:Schema.Protocol.values())same(before.get(index++),source.encode(protocol));Files.write(directory.resolve("large.java.xml"),output);
  }
  /** Compare logical arrays without relying on XML or reflection implementations. */
  private static void same(byte[] first,byte[] second){if(!Arrays.equals(first,second))throw new AssertionError("Native bytes changed");}
  /** Signal cancellation at the sealed prepared provider's measured final callback, then require the readonly completion fence. */
  private static void lateProvider(Path directory,Schema.Projected.Source source)throws Exception{
    var provider=new Schema.XmlSchema(Files.readAllBytes(directory.resolve("projection-provider.xsd")));
    byte[] bytes=source.current.encodeXml();var defaults=Schema.Limits.defaults();int[] providerPolls={0};
    provider.validate(bytes,new Schema.Limits(defaults.maxBytes,defaults.maxStringBytes,defaults.maxElements,defaults.maxDepth,defaults.readPolicy,(resource,amount,field)->++providerPolls[0]));
    if(providerPolls[0]==0)throw new AssertionError("Prepared provider never observed caller operation");
    var marker=new IllegalStateException("Readonly cancellation after real JAXP validation");int[] observed={0};boolean[] completed={false};
    var limits=new Schema.Limits(defaults.maxBytes,defaults.maxStringBytes,defaults.maxElements,defaults.maxDepth,defaults.readPolicy,(resource,amount,field)->{
      if(completed[0])throw marker;
      boolean providerFrame=StackWalker.getInstance().walk(frames->frames.anyMatch(frame->frame.getClassName().equals(Schema.XmlSchema.class.getName())&&frame.getMethodName().equals("validate")));
      if(providerFrame&&++observed[0]==providerPolls[0])completed[0]=true;
    });
    byte[] sentinel={17};byte[][] published={sentinel};RuntimeException failure=null;
    try{source.withProject(view->{published[0]=view.encodeXml(limits,provider);return null;});}catch(RuntimeException error){failure=error;}
    if(!completed[0]||observed[0]!=providerPolls[0]||failure!=marker||published[0]!=sentinel)throw new AssertionError("Readonly XML escaped late prepared-provider cancellation");
    same(bytes,source.withProject(view->view.encodeXml()));
    System.out.println("Java readonly real JAXP late-success cancellation preserves original error and publishes no XML");
  }
  /** Borrow native getters, preserve fresh defaults and verify all established wire protocols. */
  public static void main(String[] arguments)throws Exception{
    Path directory=Path.of(arguments[0]);var source=new Schema.Projected.Source();source.current.number=7;source.current.label="A<&\"";var owner=new Schema.Projected.Envelope();owner.value.bind(source);owner.label.bind(source);if(owner.value.readValue()!=source.current)throw new AssertionError("Selected target copied");
    var large=new Schema.Projected.LargerSource();large.values.add(1);large.values.add(2);large.nested.number=3;var target=new Schema.Projected.Larger();target.values=large.values;target.nested=large.nested;
    var inherited=new Schema.Projected.InheritedSource();inherited.child.number=11;var inheritedTarget=new Schema.Projected.Inherited();inheritedTarget.nested=inherited.child;
    var weak=weak();var before=new ArrayList<byte[]>();var expected=new ArrayList<byte[]>();var inheritedExpected=new ArrayList<byte[]>();for(var protocol:Schema.Protocol.values()){before.add(owner.encode(protocol));expected.add(target.encode(protocol));inheritedExpected.add(inheritedTarget.encode(protocol));}
    var schema=new Schema.XmlSchema("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='different' type='xs:string'/></xs:schema>".getBytes(java.nio.charset.StandardCharsets.UTF_8));Schema.srlProjectionTripwire=true;
    byte[] direct=owner.value.encodeXml();same(direct,source.current.encodeXml());Files.write(directory.resolve("reference.java.xml"),owner.encodeXml());
    for(int index=0;index<Schema.Protocol.values().length;++index){var protocol=Schema.Protocol.values()[index];same(before.get(index),owner.encode(protocol));same(source.current.encode(protocol),source.withProject(view->view.encode(protocol)));final int selected=index;large.withProject(view->{same(expected.get(selected),view.encode(protocol));return null;});inherited.withProject(view->{same(inheritedExpected.get(selected),view.encode(protocol));return null;});}
    same(source.current.encodeXml(),source.withProject(view->view.encodeXml()));byte[] output=large.withProject(view->{if(view.getValues()!=large.values||view.getNested()!=large.nested)throw new AssertionError("Readonly getter copied owned storage");return view.encodeXml();});Files.write(directory.resolve("translator.java.xml"),output);byte[] inheritedOutput=inherited.withProject(view->view.encodeXml());Files.write(directory.resolve("inherited.java.xml"),inheritedOutput);
    lateProvider(directory,source);
    input(()->Schema.Projected.Envelope.decodeXml(new byte[]{(byte)0xff},Schema.Limits.defaults(),schema));input(()->Schema.Projected.NestedEnvelope.decodeXml(new byte[]{(byte)0xff},Schema.Limits.defaults(),schema));input(()->Schema.Projected.OptionalEnvelope.decodeXml(new byte[]{(byte)0xff},Schema.Limits.defaults(),schema));input(()->Schema.Projected.EnvelopeValueValueReference.decodeXml(direct));rejected(()->owner.label.encodeXml());rejected(()->large.withProject(view->view.encodeXml(Schema.Limits.defaults(),schema)));
    source.current.number=0;source.current.label="updated";byte[] changed=owner.value.encodeXml();Schema.srlProjectionTripwire=false;if(Schema.Projected.Payload.decodeXml(changed).number!=0)throw new AssertionError("Live owner mutation hidden");Schema.srlProjectionTripwire=true;
    byte[] exact=source.withProject(view->view.encodeXml());same(exact,source.withProject(view->view.encodeXml(new Schema.Limits(exact.length,16*1024*1024,1000000,64),null)));rejected(()->source.withProject(view->view.encodeXml(new Schema.Limits(exact.length-1,16*1024*1024,1000000,64),null)));
    var saved=source.current;source.current=null;rejected(()->owner.value.encodeXml());source.current=saved;owner.value.unbind();rejected(()->owner.encodeXml());
    for(int iteration=0;iteration<40&&weak.value.isBound();++iteration){System.gc();Thread.sleep(10);}if(weak.value.isBound())throw new AssertionError("Weak owner unexpectedly retained");rejected(()->weak.encodeXml());Schema.srlProjectionTripwire=false;
    large.values.add(4);large.nested.number=5;var updated=Schema.Projected.Larger.decodeXml(large.withProject(view->view.encodeXml()));if(updated.values.size()!=3||updated.nested.number!=5||!updated.fallback.equals("default"))throw new AssertionError("Readonly live values/defaults changed");
    largeProjection(directory);System.out.println("Java live XML handles, recursive graph barriers, fresh base defaults, getters and constructor tripwires pass");
  }
}'''

CSHARP=r'''using System;using System.IO;using System.Collections.Generic;using System.Runtime.CompilerServices;using static SerializerGenerated.Schema;
// Check public native handles and scoped projections while target constructors are forbidden.
internal static class Program {
  // Require failure outside the catch, preserving the native owner's established exception behavior.
  private static void Rejected(Action operation){try{operation();}catch(Exception failure)when(failure is FormatException||failure is InvalidOperationException){return;}throw new Exception("Invalid projection published");}
  // Match the readonly graph barrier before provider invocation, parsing or native construction.
  private static void Input(Action operation){try{operation();}catch(FormatException failure){if(failure.Message.Contains("export-only",StringComparison.Ordinal))return;throw;}throw new Exception("Readonly input published");}
  // Ensure weak-owner qualification does not retain a caller-local strong reference through JIT lifetime.
  [MethodImpl(MethodImplOptions.NoInlining)]private static ProjectedWeakEnvelope Weak(){var source=new ProjectedSource();var value=new ProjectedWeakEnvelope();value.Value.Bind(source);value.EncodeXml();return value;}
  // Compare ordinary native protocol bytes independently from XML implementations.
  private static void Same(byte[] first,byte[] second){if(!first.AsSpan().SequenceEqual(second))throw new Exception("Native bytes changed");}
  // Preserve a large borrowed source through exact/one-tight output bounds and a fresh complete retry.
  private static void LargeProjection(string directory){var source=new ProjectedLargerSource();for(uint index=0;index<25000;++index)source.Values.Add(index);source.Nested.Number=17;var values=source.Values;var nested=source.Nested;var before=new List<byte[]>();foreach(var protocol in Enum.GetValues<Protocol>())before.Add(source.Encode(protocol));byte[] output;SerializerGenerated.Schema.SrlProjectionTripwire=true;
    try{output=source.WithProject(view=>{if(!ReferenceEquals(view.GetValues(),values)||!ReferenceEquals(view.GetNested(),nested))throw new Exception("Large getter copied source");return view.EncodeXml();});Same(output,source.WithProject(view=>view.EncodeXml(new Limits(maxBytes:output.Length),null)));Rejected(()=>source.WithProject(view=>view.EncodeXml(new Limits(maxBytes:output.Length-1),null)));var marker=new InvalidOperationException("Large projection interrupted");int polls=0;var canceled=new Limits().WithCancellation(()=>{if(++polls==100)throw marker;});byte[] sentinel={17},published=sentinel;try{published=source.WithProject(view=>view.EncodeXml(canceled,null));throw new Exception("Large interruption ignored");}catch(InvalidOperationException failure){if(!ReferenceEquals(failure,marker)||polls!=100||!ReferenceEquals(published,sentinel))throw new Exception("Large interruption changed publication",failure);}Same(output,source.WithProject(view=>view.EncodeXml()));}finally{SerializerGenerated.Schema.SrlProjectionTripwire=false;}
    int item=0;foreach(var protocol in Enum.GetValues<Protocol>())Same(before[item++],source.Encode(protocol));File.WriteAllBytes(Path.Combine(directory,"large.csharp.xml"),output);
  }
  // Exercise true getter identity, fresh defaults, historical metadata and complete operation limits.
  private static void Main(string[] arguments){string directory=arguments[0];var source=new ProjectedSource();source.Current.Number=7;source.Current.Label="A<&\"";var owner=new ProjectedEnvelope();owner.Value.Bind(source);owner.Label.Bind(source);if(!ReferenceEquals(owner.Value.ReadValue(),source.Current))throw new Exception("Selected record copied");
    var large=new ProjectedLargerSource();large.Values.AddRange(new uint[]{1,2});large.Nested.Number=3;var target=new ProjectedLarger();target.Values=large.Values;target.Nested=large.Nested;var inherited=new ProjectedInheritedSource();inherited.Child.Number=11;var inheritedTarget=new ProjectedInherited();inheritedTarget.Nested=inherited.Child;
    var weak=Weak();var before=new List<byte[]>();var expected=new List<byte[]>();var inheritedExpected=new List<byte[]>();foreach(var protocol in Enum.GetValues<Protocol>()){before.Add(owner.Encode(protocol));expected.Add(target.Encode(protocol));inheritedExpected.Add(inheritedTarget.Encode(protocol));}
    var schema=new XmlSchema(System.Text.Encoding.UTF8.GetBytes("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='different' type='xs:string'/></xs:schema>"));SerializerGenerated.Schema.SrlProjectionTripwire=true;
    byte[] direct=owner.Value.EncodeXml();Same(direct,source.Current.EncodeXml());File.WriteAllBytes(Path.Combine(directory,"reference.csharp.xml"),owner.EncodeXml());int index=0;foreach(var protocol in Enum.GetValues<Protocol>()){Same(before[index],owner.Encode(protocol));Same(source.Current.Encode(protocol),source.WithProject(view=>view.Encode(protocol)));Same(expected[index],large.WithProject(view=>view.Encode(protocol)));Same(inheritedExpected[index],inherited.WithProject(view=>view.Encode(protocol)));++index;}
    Same(source.Current.EncodeXml(),source.WithProject(view=>view.EncodeXml()));byte[] output=large.WithProject(view=>{if(!ReferenceEquals(view.GetValues(),large.Values)||!ReferenceEquals(view.GetNested(),large.Nested))throw new Exception("Getter copied owned storage");return view.EncodeXml();});File.WriteAllBytes(Path.Combine(directory,"translator.csharp.xml"),output);File.WriteAllBytes(Path.Combine(directory,"inherited.csharp.xml"),inherited.WithProject(view=>view.EncodeXml()));
    Input(()=>ProjectedEnvelope.DecodeXml(new byte[]{255},new Limits(),schema));Input(()=>ProjectedNestedEnvelope.DecodeXml(new byte[]{255},new Limits(),schema));Input(()=>ProjectedOptionalEnvelope.DecodeXml(new byte[]{255},new Limits(),schema));Input(()=>ProjectedEnvelopeValueValueReference.DecodeXml(direct));Rejected(()=>owner.Label.EncodeXml());Rejected(()=>large.WithProject(view=>view.EncodeXml(new Limits(),schema)));
    source.Current.Number=0;source.Current.Label="updated";byte[] changed=owner.Value.EncodeXml();SerializerGenerated.Schema.SrlProjectionTripwire=false;if(ProjectedPayload.DecodeXml(changed).Number!=0)throw new Exception("Live mutation hidden");SerializerGenerated.Schema.SrlProjectionTripwire=true;
    byte[] exact=source.WithProject(view=>view.EncodeXml());Same(exact,source.WithProject(view=>view.EncodeXml(new Limits(maxBytes:exact.Length),null)));Rejected(()=>source.WithProject(view=>view.EncodeXml(new Limits(maxBytes:exact.Length-1),null)));var saved=source.Current;source.Current=null!;Rejected(()=>owner.Value.EncodeXml());source.Current=saved;owner.Value.Unbind();Rejected(()=>owner.EncodeXml());
    for(int iteration=0;iteration<40&&weak.Value.IsBound();++iteration){GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();}if(weak.Value.IsBound())throw new Exception("Weak owner retained");Rejected(()=>weak.EncodeXml());SerializerGenerated.Schema.SrlProjectionTripwire=false;large.Values.Add(4);large.Nested.Number=5;var updated=ProjectedLarger.DecodeXml(large.WithProject(view=>view.EncodeXml()));if(updated.Values.Count!=3||updated.Nested.Number!=5||updated.Fallback!="default")throw new Exception("Live getters/defaults changed");
    LargeProjection(directory);Console.WriteLine("C# live XML handles, recursive graph preflight, fresh base getters, native protocols and constructor tripwires pass");
  }
}'''

GO=r'''package main
import("bytes";"fmt";"os";"path/filepath";"strings")
// Preserve exact caller callback failures at the complete-message boundary.
type projectionValidator struct{calls int;failure error}
func (value *projectionValidator)ValidateXML(data []byte)error{value.calls++;return value.failure}
// Require no owning bytes to be published after any policy, binding or provider failure.
func rejected(data []byte,err error){if data!=nil||err==nil{panic("Invalid projection published")}}
// Compare ordinary wire bytes without relying on XML equality implementations.
func same(first,second []byte){if !bytes.Equal(first,second){panic("Native bytes changed")}}
// Borrow a large array through output admission, exact callback failure identity and fresh recovery.
func largeProjection(directory string){source:=NewProjectedLargerSource();source.Values=make([]uint32,25000);for index:=range source.Values{source.Values[index]=uint32(index)};source.Nested.Number=17;values:=source.Values;nested:=source.Nested;protocols:=[]Protocol{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING};before:=make([][]byte,len(protocols));var err error;for index,protocol:=range protocols{before[index],err=source.Encode(protocol);if err!=nil{panic(err)}};srlProjectionTripwire=true;defer func(){srlProjectionTripwire=false}();var output []byte
  source.WithProject(func(view ProjectedLargerSourceProjectTranslator){if &view.GetValues()[0]!=&values[0]||view.GetNested()!=nested{panic("Large getter copied source")};output,err=view.EncodeXML();if err!=nil{panic(err)};limits:=DefaultXmlLimits();limits.MaxBytes=len(output);exact,err:=view.EncodeXML(XmlOptions{limits,nil});if err!=nil{panic(err)};same(output,exact);limits.MaxBytes--;rejected(view.EncodeXML(XmlOptions{limits,nil}));marker:=fmt.Errorf("Large projection interrupted");polls:=0;data,failure:=view.EncodeXMLWithOperation(XmlOperationOptions{Options:XmlOptions{DefaultXmlLimits(),nil},Operation:func(string,int64,string)error{polls++;if polls==100{return marker};return nil}});if data!=nil||failure!=marker||polls!=100{panic("Large interruption changed publication")};fresh,err:=view.EncodeXML();if err!=nil{panic(err)};same(output,fresh)})
  for index,protocol:=range protocols{after,err:=source.Encode(protocol);if err!=nil{panic(err)};same(before[index],after)};if err=os.WriteFile(filepath.Join(directory,"large.go.xml"),output,0600);err!=nil{panic(err)}
}
@prepared_provider_helpers@
// Exercise live source aliases, explicit borrowed/weak scopes and scoped target getters.
func main(){directory:=os.Args[1];source:=NewProjectedSource();source.Current.Number=7;source.Current.Label="A<&\"";owner:=NewProjectedEnvelope();if err:=owner.Value.Bind(source);err!=nil{panic(err)};scope:=NewProjectedEnvelopeLabelValueReferenceScope(source);if err:=owner.Label.Bind(scope);err!=nil{panic(err)};selected,err:=owner.Value.ReadValue();if err!=nil||selected!=source.Current{panic("Selected target copied")}
  large:=NewProjectedLargerSource();large.Values=[]uint32{1,2};large.Nested.Number=3;target:=NewProjectedLarger();target.Values=large.Values;target.Nested=large.Nested;inherited:=NewProjectedInheritedSource();inherited.Child.Number=11;inheritedTarget:=NewProjectedInherited();inheritedTarget.Nested=inherited.Child
  protocols:=[]Protocol{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING};before,expected,inheritedExpected:=[][]byte{},[][]byte{},[][]byte{};for _,protocol:=range protocols{first,err:=owner.Encode(protocol);if err!=nil{panic(err)};before=append(before,first);first,err=target.Encode(protocol);if err!=nil{panic(err)};expected=append(expected,first);first,err=inheritedTarget.Encode(protocol);if err!=nil{panic(err)};inheritedExpected=append(inheritedExpected,first)}
  weak:=NewProjectedWeakEnvelope();weakScope:=NewProjectedWeakEnvelopeValueValueReferenceScope(source);if err=weak.Value.Bind(weakScope);err!=nil{panic(err)};srlProjectionTripwire=true
  direct,err:=owner.Value.EncodeXML();if err!=nil{panic(err)};current,err:=source.Current.EncodeXML();if err!=nil{panic(err)};same(direct,current);output,err:=owner.EncodeXML();if err!=nil{panic(err)};if err=os.WriteFile(filepath.Join(directory,"reference.go.xml"),output,0600);err!=nil{panic(err)}
  for index,protocol:=range protocols{first,err:=owner.Encode(protocol);if err!=nil{panic(err)};same(first,before[index]);source.WithProject(func(view ProjectedSourceProjectTranslator){first,err:=view.Encode(protocol);if err!=nil{panic(err)};second,err:=source.Current.Encode(protocol);if err!=nil{panic(err)};same(first,second)});large.WithProject(func(view ProjectedLargerSourceProjectTranslator){first,err:=view.Encode(protocol);if err!=nil{panic(err)};same(first,expected[index])});inherited.WithProject(func(view ProjectedInheritedSourceProjectTranslator){first,err:=view.Encode(protocol);if err!=nil{panic(err)};same(first,inheritedExpected[index])})}
  large.WithProject(func(view ProjectedLargerSourceProjectTranslator){values:=view.GetValues();if &values[0]!=&large.Values[0]||view.GetNested()!=large.Nested{panic("Getter copied owned storage")};output,err=view.EncodeXML();if err!=nil{panic(err)}});if err=os.WriteFile(filepath.Join(directory,"translator.go.xml"),output,0600);err!=nil{panic(err)};inherited.WithProject(func(view ProjectedInheritedSourceProjectTranslator){output,err=view.EncodeXML();if err!=nil{panic(err)}});if err=os.WriteFile(filepath.Join(directory,"inherited.go.xml"),output,0600);err!=nil{panic(err)}
  marker:=fmt.Errorf("application projection provider marker");validator:=&projectionValidator{failure:marker};candidate,err:=DecodeProjectedEnvelopeXML([]byte{255},XmlOptions{DefaultXmlLimits(),validator});if candidate!=nil||err==nil||!strings.Contains(err.Error(),"export-only")||validator.calls!=0{panic("Input provider/candidate ran before readonly barrier")};nested,err:=DecodeProjectedNestedEnvelopeXML([]byte{255},XmlOptions{DefaultXmlLimits(),validator});if nested!=nil||err==nil||!strings.Contains(err.Error(),"export-only")||validator.calls!=0{panic("Nested graph input ran before readonly barrier")};optional,err:=DecodeProjectedOptionalEnvelopeXML([]byte{255},XmlOptions{DefaultXmlLimits(),validator});if optional!=nil||err==nil||!strings.Contains(err.Error(),"export-only")||validator.calls!=0{panic("Optional graph input ran before readonly barrier")};reference,err:=DecodeProjectedEnvelopeValueValueReferenceXML(direct);if reference!=nil||err==nil||!strings.Contains(err.Error(),"export-only"){panic("Readonly reference input published")};rejected(owner.Label.EncodeXML());large.WithProject(func(view ProjectedLargerSourceProjectTranslator){data,err:=view.EncodeXML(XmlOptions{DefaultXmlLimits(),validator});if data!=nil||err!=marker{panic("Projection changed provider error")}})
  source.Current.Number=0;source.Current.Label="updated";changed,err:=owner.Value.EncodeXML();if err!=nil{panic(err)};srlProjectionTripwire=false;copy,err:=DecodeProjectedPayloadXML(changed);if err!=nil||copy.Number!=0{panic("Live mutation hidden")};srlProjectionTripwire=true;source.WithProject(func(view ProjectedSourceProjectTranslator){data,err:=view.EncodeXML();if err!=nil{panic(err)};limits:=DefaultXmlLimits();limits.MaxBytes=len(data);exact,err:=view.EncodeXML(XmlOptions{limits,nil});if err!=nil{panic(err)};same(data,exact);limits.MaxBytes--;rejected(view.EncodeXML(XmlOptions{limits,nil}))});saved:=source.Current;source.Current=nil;rejected(owner.Value.EncodeXML());source.Current=saved;scope.Close();rejected(owner.EncodeXML());weakScope.Close();rejected(weak.EncodeXML());owner.Value.Unbind();rejected(owner.Value.EncodeXML());srlProjectionTripwire=false
  large.Values=append(large.Values,4);large.Nested.Number=5;large.WithProject(func(view ProjectedLargerSourceProjectTranslator){data,err:=view.EncodeXML();if err!=nil{panic(err)};copy,err:=DecodeProjectedLargerXML(data);if err!=nil||len(copy.Values)!=3||copy.Nested.Number!=5||copy.Fallback!="default"{panic("Live getters/defaults changed")}});@prepared_provider_call@;largeProjection(directory);fmt.Println("Go live readonly XML references, recursive input barriers, explicit owner scopes, fresh inherited bases and constructor tripwires pass")
}'''

GO_PREPARED=r'''
// Return the real provider's error before the operation's post-completion cancellation fence.
type projectionCompletingProvider struct{delegate XmlValidator;completed *bool;failure error}
func(value projectionCompletingProvider)ValidateXML(data []byte)error{if err:=value.delegate.ValidateXML(data);err!=nil{return err};*value.completed=true;return value.failure}
// Validate through a genuine prepared libxml2 bridge, then reject readonly output without replacing caller/provider errors.
func latePreparedProjection(directory string){
  bridge,err:=loadProviderBridge(os.Args[2]);if err!=nil{panic(err)};defer func(){if err:=bridge.Close();err!=nil{panic(err)}}()
  schema,err:=os.ReadFile(filepath.Join(directory,"projection-provider.xsd"));if err!=nil{panic(err)};provider,err:=newPreparedProvider(bridge,schema);if err!=nil{panic(err)};defer provider.Close()
  source:=NewProjectedSource();source.Current.Number=7;source.Current.Label="prepared";before,err:=source.Current.Encode(BINARY_NONE);if err!=nil{panic(err)}
  srlProjectionTripwire=true;defer func(){srlProjectionTripwire=false}()
  completed:=false;marker:=fmt.Errorf("Readonly cancellation after real libxml2 validation")
  selected:=projectionCompletingProvider{provider,&completed,nil};operation:=XmlOperationOptions{Options:XmlOptions{DefaultXmlLimits(),selected},Operation:func(string,int64,string)error{if completed{return marker};return nil}}
  invoked:=false;source.WithProject(func(view ProjectedSourceProjectTranslator){invoked=true;data,failure:=view.EncodeXMLWithOperation(operation);if !completed||data!=nil||failure!=marker{panic("Readonly XML escaped late prepared-provider cancellation")}});if !invoked{panic("Readonly provider control not invoked")}
  completed=false;original:=fmt.Errorf("Original readonly prepared-provider rejection");operation.Options.Validator=projectionCompletingProvider{provider,&completed,original}
  source.WithProject(func(view ProjectedSourceProjectTranslator){data,failure:=view.EncodeXMLWithOperation(operation);if !completed||data!=nil||failure!=original{panic("Readonly provider failure was replaced by cancellation")}})
  after,err:=source.Current.Encode(BINARY_NONE);if err!=nil{panic(err)};same(before,after);source.WithProject(func(view ProjectedSourceProjectTranslator){data,failure:=view.EncodeXML();if failure!=nil||len(data)==0{panic("Fresh readonly output did not recover")}})
  fmt.Println("Go readonly real libxml2 late-success cancellation preserves original errors and publishes no XML")
}
'''


def instrument(path,language):
    """Install test-only constructor traps in actual compiler output; generated source stays under ignored out."""
    source=path.read_text(encoding='utf-8')
    if language=='java':
        matched=re.search(r'public final class Schema\s*\{',source)
        if matched is None:raise AssertionError('Expected generated Java outer container')
        source=source[:matched.end()]+'\npublic static boolean srlProjectionTripwire;\n'+source[matched.end():]
        for name in ('Payload','Larger','Inherited','Envelope','WeakEnvelope','NestedEnvelope','OptionalEnvelope'):
            pattern=r'(public static final class '+name+r'\s*\{)'
            source,count=re.subn(pattern,lambda match:match[0]+'\npublic '+name+'(){if(Schema.srlProjectionTripwire)throw new AssertionError("Owning XML projection target constructed");}\n',source,count=1)
            if count!=1:raise AssertionError('Missing Java constructor tripwire: '+name)
    elif language=='csharp':
        matched=re.search(r'public static(?: partial)? class Schema\s*\{',source)
        if matched is None:raise AssertionError('Expected generated C# outer container')
        source=source[:matched.end()]+'\npublic static bool SrlProjectionTripwire;\n'+source[matched.end():]
        for name in ('ProjectedPayload','ProjectedLarger','ProjectedInherited','ProjectedEnvelope','ProjectedWeakEnvelope','ProjectedNestedEnvelope','ProjectedOptionalEnvelope'):
            source,count=re.subn(r'(public sealed(?: partial)? class '+name+r'\s*\{)',lambda match:match[0]+'\npublic '+name+'(){if(SerializerGenerated.Schema.SrlProjectionTripwire)throw new System.Exception("Owning XML projection target constructed");}\n',source,count=1)
            if count!=1:raise AssertionError('Missing C# constructor tripwire: '+name)
    else:
        source+='\nvar srlProjectionTripwire bool\n'
        for name in ('ProjectedPayload','ProjectedLarger','ProjectedInherited','ProjectedEnvelope','ProjectedWeakEnvelope','ProjectedNestedEnvelope','ProjectedOptionalEnvelope'):
            source,count=re.subn(r'(func New'+name+r'\(\) \*'+name+r'\s*\{)',lambda match:match[0]+'\nif srlProjectionTripwire{panic("Owning XML projection target constructed")}\n',source,count=1)
            if count!=1:raise AssertionError('Missing Go constructor tripwire: '+name)
    path.write_text(source,encoding='utf-8')


def main():
    """Compile strict actual SDKs and independently validate every retained published projection."""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path);parser.add_argument('--language',default='java,csharp,go');parser.add_argument('--wsl-languages',default='');parser.add_argument('--bridge',type=Path,help='Run additional Go late-provider controls with the actual Windows libxml2 bridge');parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args();args.compiler=args.compiler.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if not languages or len(languages)!=len(set(languages)) or any(language not in ('java','csharp','go') for language in languages):parser.error('Choose distinct names from java,csharp,go')
    if args.bridge and ('go' not in languages or os.name!='nt' or 'go' in args.wsl_languages.split(',')):parser.error('--bridge requires native Windows Go in --language')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:parser.error('In-repository output must be beneath ignored out')
    args.build.mkdir(parents=True,exist_ok=True);model=args.build/'model.serializer';model.write_text(MODEL,encoding='utf-8');command=[args.compiler,'--input',model,'--xml.enabled','true','--xml.runtime_xsd','true','--xml.validation_scope','both','--language',','.join(languages),'--go.package','main']
    for language in languages:
        directory=args.build/language;directory.mkdir(exist_ok=True);command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args);oracle=etree.XMLSchema(etree.fromstring(XSD.encode()));(args.build/'projection-provider.xsd').write_text(XSD,encoding='utf-8')
    for language,filename,source in [('java','Main.java',JAVA),('csharp','Program.cs',CSHARP),('go','main.go',GO)]:
        if language not in languages:continue
        instrument(args.build/language/sdk.OUTPUTS[language],language);directory=args.build/'source'/language;directory.mkdir(parents=True,exist_ok=True)
        if language=='go':
            source=source.replace('@prepared_provider_helpers@',GO_PREPARED if args.bridge else '').replace('@prepared_provider_call@','latePreparedProjection(directory)' if args.bridge else '')
            if args.bridge:
                args.bridge=args.bridge.resolve(strict=True);shutil.copyfile(ROOT/'test/resources/xml_provider/go/provider_windows.go',directory/'provider_windows.go')
        (directory/filename).write_text(source,encoding='utf-8')
        for name in ('reference','translator','inherited'):Path(args.build/f'{name}.{language}.xml').unlink(missing_ok=True)
        launch=runner.build(language,'xml_projection',args.build/language,directory);arguments=[*launch,runner.path(language,args.build)]
        if language=='go' and args.bridge:arguments.append(runner.path(language,args.bridge))
        sdk.run(arguments)
        for name in ('reference','translator','inherited'):
            document=etree.parse(str(args.build/f'{name}.{language}.xml'));oracle.assertValid(document);node=document.getroot()
            if name=='reference':assert node.get('label')=='A<&"' and [child.text for child in node[0]]==['7','A<&"']
            elif name=='translator':assert [child.text for child in node.findall('{'+NS+'}values')]==['1','2'] and node.find('{'+NS+'}fallback').text=='default'
            else:assert node.get('{urn:serializer:xml:model}revision')=='3' and node.find('{'+NS+'}code').text=='7' and node.find('{'+NS+'}fallback').text=='fresh'
        large=etree.parse(str(args.build/f'large.{language}.xml'));oracle.assertValid(large)
        assert [int(child.text) for child in large.getroot().findall('{'+NS+'}values')]==list(range(25000))
        assert large.getroot().find('{'+NS+'}nested')[0].text=='17'
        assert large.getroot().find('{'+NS+'}fallback').text=='default'
    report={'status':'pass','compiler_sha256':hashlib.sha256(args.compiler.read_bytes()).hexdigest(),'languages':languages,
            'readonly_late_prepared_provider_controls':{'java':'java' in languages,'go':bool(args.bridge)},
            'generated_sha256':{language:hashlib.sha256((args.build/language/sdk.OUTPUTS[language]).read_bytes()).hexdigest() for language in languages}}
    (args.build/'verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'PASS: live reference/scoped getter exports, fresh inherited defaults, four native protocols, recursive input/provider barriers and constructor tripwires across {len(languages)} SDKs',flush=True)


if __name__=='__main__':main()
