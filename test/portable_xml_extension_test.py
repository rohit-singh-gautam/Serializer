# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify Java/C#/Go positioned opaque XML, exact admission and namespace meaning."""
import argparse
import re
import subprocess
from pathlib import Path
import sys
from lxml import etree
from python_xml_extension_test import DOCUMENT, NS
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'example'))
import run as sdk
ROOT=Path(__file__).resolve().parent.parent
# This shared three-SDK fixture owns its model and slots independently from other backend tests.
MODEL='''serializer version 1.20.0; namespace extension { xml(namespace("urn:serializer:extension-tokens"));
  class root { public uint32 id(1) xml(attribute); public array<uint32> items(2); public string tail(3); }
  class only_skip { public uint32 value(1); }class text { public uint32 id xml(attribute); public string content xml(text); }
class holder { public array<root> values; }
class source { public root current; public translator root as project { id=current.id; items=current.items; tail=current.tail; }; }
class envelope { public shared(source.current) value; }
}'''
OPTIONS='''[xml]
enabled=true
[xml.class.extension::root]
namespace=urn:serializer:extension-tokens
name.items=item
extension.attributes.kind=attribute
extension.attributes.namespaces=urn:metadata urn:serializer:xml:model
extension.attributes.policy=preserve
extension.attributes.maximum=1
extension.before.kind=element
extension.before.before=items
extension.before.namespaces=urn:vendor
extension.before.policy=preserve
extension.before.minimum=1
extension.before.maximum=2
extension.end.kind=element
extension.end.namespaces=urn:end urn:serializer:xml:model
extension.end.policy=preserve
extension.end.maximum=1
extension.skip.kind=element
extension.skip.before=tail
extension.skip.namespaces=urn:skip
extension.skip.policy=skip
extension.reject.kind=element
extension.reject.namespaces=urn:reject
extension.reject.policy=reject
extensions.required=urn:vendor
[xml.class.extension::only_skip]
namespace=urn:serializer:extension-tokens
extension.skip.kind=element
extension.skip.namespaces=urn:skip
extension.skip.policy=skip
[xml.class.extension::holder]
name.values=root
[xml.class.extension::text]
extension.attributes.kind=attribute
extension.attributes.namespaces=urn:metadata urn:serializer:xml:model
extension.attributes.policy=preserve
extension.attributes.maximum=1
'''
XSD='''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="'''+NS+'''" elementFormDefault="qualified">
<xs:element name="root"><xs:complexType><xs:sequence><xs:choice minOccurs="0" maxOccurs="unbounded"><xs:any namespace="urn:vendor" processContents="skip"/><xs:element name="item" type="xs:unsignedInt"/></xs:choice><xs:any namespace="urn:skip" processContents="skip" minOccurs="0" maxOccurs="unbounded"/><xs:element name="tail" type="xs:string"/><xs:any namespace="urn:end" processContents="skip" minOccurs="0" maxOccurs="1"/></xs:sequence><xs:attribute name="id" type="xs:unsignedInt" use="required"/><xs:anyAttribute namespace="urn:metadata" processContents="skip"/></xs:complexType></xs:element>
<xs:element name="only_skip"><xs:complexType><xs:sequence><xs:element name="value" type="xs:unsignedInt"/><xs:any namespace="urn:skip" processContents="skip" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element></xs:schema>'''


XSD=XSD.replace('</xs:schema>','<xs:element name="text"><xs:complexType><xs:simpleContent><xs:extension base="xs:string"><xs:attribute name="id" type="xs:unsignedInt" use="required"/><xs:anyAttribute namespace="urn:metadata" processContents="skip"/></xs:extension></xs:simpleContent></xs:complexType></xs:element></xs:schema>')
XSD=XSD.replace('<xs:schema xmlns:xs=', '<xs:schema xmlns:t="'+NS+'" xmlns:xs=').replace('</xs:schema>','<xs:element name="holder"><xs:complexType><xs:sequence><xs:element ref="t:root" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element></xs:schema>')


def cases():
    """Separate XSD vocabulary decisions from stricter positioned permissions and independent bounds."""
    document=DOCUMENT.decode();default=(5,100,65536,'','urn:vendor,urn:parent')
    result=[('root-basic',document,default,True,True),('root-unicode',document.replace('head','headé😀'),default,True,True),('root-comments',document.replace('head','h<![CDATA[ea]]><!-- ignored -->d'),default,True,True)]
    for label,old,new,xml_valid in [('unknown','<v:between/>','<intruder/>',False),('unlisted','<v:between/>','<q:extra xmlns:q="urn:unlisted"/>',False),('overflow','<v:between/>','<v:one/><v:two/>',True),('late','<tail>Z</tail>','<tail>Z</tail><v:late/>',False),('attribute','id="7"','id="7" unknown="x"',False),('core','<v:between/>','<item>3</item><item>4</item><item>5</item>',True),('duplicate','id="7"','id="7" id="8"',False),('reject','</root>','<r:denied xmlns:r="urn:reject"/></root>',False),('model','id="7"','id="7" xmlns:x="urn:serializer:xml:model" x:unknown="1"',False)]:
        # Extra ordinary repeated items remain valid; native storage has no artificial extent.
        result.append(('root-'+label,document.replace(old,new),default,label=='core',xml_valid))
    result.append(('root-missing-minimum',document.replace('<v:between/>','').replace(document[document.index('<v:before '):document.index('</v:before>')+len('</v:before>')],''),default,False,True))
    for label,policy in [('slots',(4,100,65536,'','urn:vendor,urn:parent')),('items',(5,4,65536,'','urn:vendor,urn:parent')),('bytes',(5,100,0,'','urn:vendor,urn:parent')),('understanding',(5,100,65536,'','')),('prefix',(5,100,65536,'missing','urn:vendor,urn:parent')),('prefix-understanding',(5,100,65536,'p','urn:vendor'))]:result.append(('root-'+label,document,policy,False,True))
    result.append(('root-prefix-understood',document,(5,100,65536,'p','urn:vendor,urn:parent'),True,True))
    result.append(('root-model-child',document.replace('<e:after>kept</e:after>','<z:unknown xmlns:z="urn:serializer:xml:model"/>'),default,False,False))
    result.append(('root-exact-items',document,(5,5,65536,'','urn:vendor,urn:parent'),True,True))
    result.append(('root-pinned-collisions',document.replace('xmlns:p="urn:parent"','xmlns:p="urn:parent" xmlns:srl="urn:pinned-srl" xmlns:a="urn:pinned-a" xmlns:t="urn:pinned-t"'),default,True,True))
    result.append(('root-foreign-xml-attribute',document.replace('m:again="n1:Thing"','m:again="n1:Thing" xml:lang="en"'),default,True,True))
    result.append(('root-dtd', '<!DOCTYPE root>'+document,default,False,True))
    result.append(('root-prefix-items',document,(5,5,65536,'p p p p p p','urn:vendor,urn:parent'),False,True))
    skip='<only_skip xmlns="'+NS+'" xmlns:s="urn:skip"><value>1</value><s:discard>'+('discarded'*4096)+'</s:discard></only_skip>'
    result.append(('skip-zero',skip,(1,1,0,'',''),True,True))
    text='<text xmlns="'+NS+'" xmlns:m="urn:metadata" xmlns:p="urn:parent" id="7" m:label="p:Thing">head&lt;&amp;<![CDATA[tail]]><!-- ignored --></text>'
    result += [('text-attributes',text,(1,2,65536,'',''),True,True),('text-unknown',text.replace('id="7"','id="7" unknown="x"'),(1,2,65536,'',''),False,False),('text-child',text.replace('head&lt;&amp;','<m:child/>'),(1,2,65536,'',''),False,False)]
    return result


def check_output(source):
    """Inspect QName namespace context and mixed opaque text independently of every generated reader."""
    root=etree.fromstring(source)
    if etree.QName(root).localname=='only_skip':assert len(root)==1;return
    if etree.QName(root).localname=='text':assert root.text=='head<&tail' and root.nsmap['p']=='urn:parent' and root.get('{urn:metadata}label')=='p:Thing';return
    assert root.attrib['{urn:metadata}label']=='p:Thing' and root.nsmap['p']=='urn:parent'
    before=next(child for child in root if child.tag=='{urn:vendor}before')
    assert before.nsmap['n1']=='urn:another' and before.attrib['{urn:metadata}again']=='n1:Thing'
    assert (before.text,before[0].tail,before[1].tail) in [('head','tail','last'),('headé😀','tail','last')]
    assert before[0].tag=='{urn:inner}nest' and before[0].nsmap['p']=='urn:inner' and before[0].get('name')=='p:Thing'
    assert before[1].tag=='plain'
    assert not any(child.tag=='{urn:skip}discard' for child in root)

JAVA=r'''import java.nio.file.*;import java.util.*;
/** Exercise opaque XML through actual owning models and established wire protocols. */
public final class Main {
  /** Carry the complete explicit extension policy into the actual owning decoder. */
  private static Schema.Extension.Root read(byte[] bytes,Schema.XmlExtensionOptions options){return Schema.Extension.Root.decodeXml(bytes,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options);}
  /** Replay private preserved state through the complete explicit policy overload. */
  private static byte[] write(Schema.Extension.Root value,Schema.XmlExtensionOptions options){return value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options);}
  /** Reject partial publication outside the catch. */
  private static void rejected(Runnable operation){try{operation.run();}catch(IllegalArgumentException expected){return;}throw new AssertionError("Invalid extension accepted");}
  /** Keep independent admission controls separate from ordinary document limits. */
  private static Schema.XmlExtensionOptions policy(int slots,int items,long bytes,String prefixes,String understood){return new Schema.XmlExtensionOptions(understood.isEmpty()?List.of():List.of(understood.split(",")),prefixes,new Schema.XmlExtensionLimits(slots,items,bytes));}
  /** Roundtrip known native values without retaining or interpreting unknown vocabulary payloads. */
  public static void main(String[] arguments)throws Exception{
    for(int offset=0;offset<arguments.length;offset+=6){String path=arguments[offset];boolean valid=path.endsWith(".valid.xml");var options=policy(Integer.parseInt(arguments[offset+1]),Integer.parseInt(arguments[offset+2]),Long.parseLong(arguments[offset+3]),arguments[offset+4],arguments[offset+5]);byte[] output;
      try{byte[] data=Files.readAllBytes(Path.of(path));if(Path.of(path).getFileName().toString().startsWith("skip")){var value=Schema.Extension.OnlySkip.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options);output=value.encodeXml();}
        else if(Path.of(path).getFileName().toString().startsWith("text")){var value=Schema.Extension.Text.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options);output=value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options);}
        else{var value=Schema.Extension.Root.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options);output=value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options);var copy=Schema.Extension.Root.decodeXml(output,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options);for(var protocol:Schema.Protocol.values())if(!Arrays.equals(value.encode(protocol),copy.encode(protocol)))throw new AssertionError("Native wire changed");if(!Arrays.equals(output,copy.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options)))throw new AssertionError("Unstable opaque replay");}
      }catch(IllegalArgumentException failure){if(valid)throw new AssertionError(path,failure);continue;}if(!valid)throw new AssertionError("Invalid extension accepted: "+path);Files.write(Path.of(path+".java.xml"),output);
    }
    {byte[] data=Files.readAllBytes(Path.of(arguments[0]));int low=0,high=65536;while(low<high){int middle=(low+high)/2;try{read(data,policy(5,5,middle,"","urn:vendor,urn:parent"));high=middle;}catch(IllegalArgumentException expected){low=middle+1;}}
    final int exact=low;var precise=policy(5,5,exact,"","urn:vendor,urn:parent");var value=read(data,precise);byte[] previous=write(value,precise);for(int iteration=0;iteration<4;++iteration){value=read(previous,precise);byte[] next=write(value,precise);if(!Arrays.equals(previous,next))throw new AssertionError("Unstable exact preserved budget");previous=next;}final var retained=value;rejected(()->read(data,policy(5,5,exact-1,"","urn:vendor,urn:parent")));rejected(()->write(retained,policy(5,5,exact-1,"","urn:vendor,urn:parent")));
    value.items.add(3);if(read(write(value,precise),precise).items.size()!=3)throw new AssertionError("Known edit lost");value.items.clear();rejected(()->write(retained,precise));
    var source=new Schema.Extension.Source();source.current=read(data,policy(5,5,65536,"","urn:vendor,urn:parent"));var envelope=new Schema.Extension.Envelope();envelope.value.bind(source);if(!Arrays.equals(write(source.current,precise),envelope.value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),precise)))throw new AssertionError("Readonly reference lost opaque state");rejected(()->source.withProject(view->view.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),precise)));
    String document=new String(data,java.nio.charset.StandardCharsets.UTF_8);byte[] nested=("<holder xmlns=\"'+NS+'\">"+document+document+"</holder>").getBytes(java.nio.charset.StandardCharsets.UTF_8);var options=policy(5,5,65536,"","urn:vendor,urn:parent");var holder=Schema.Extension.Holder.decodeXml(nested,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options);if(holder.values.size()!=2)throw new AssertionError("Owner-local extension counters conflated");Files.write(Path.of(arguments[0]+".nested.java.xml"),holder.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),options));
    System.out.println("Java exact preserved-byte boundaries, edits, nested owner-local counters and readonly extension forwarding pass");}
  }
}'''

CSHARP=r'''using System;using System.IO;using static SerializerGenerated.Schema;
// Exercise the secure direct reader and writer using explicit independent extension policies.
internal static class Program {
  // Select the complete explicit policy without changing earlier overloads.
  private static ExtensionRoot Read(byte[] bytes,XmlExtensionOptions options)=>ExtensionRoot.DecodeXml(bytes,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),options);
  // Replay the actual opaque sidecar with the chosen independent bounds.
  private static byte[] Write(ExtensionRoot value,XmlExtensionOptions options)=>value.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),options);
  // Detect an accidental successful publication outside the expected-failure catch.
  private static void Rejected(Action action){try{action();}catch(FormatException){return;}throw new Exception("Invalid extension published");}
  // Own one bounded vocabulary inventory independently from complete-document limits.
  private static XmlExtensionOptions Policy(long bytes)=>new(new[]{"urn:vendor","urn:parent"},"",new XmlExtensionLimits(5,5,bytes));
  // Keep an accidental successful invalid publication outside the generated failure catch.
  private static void Main(string[] arguments){for(int offset=0;offset<arguments.Length;offset+=6){string path=arguments[offset];bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);var options=new XmlExtensionOptions(arguments[offset+5].Length==0?Array.Empty<string>():arguments[offset+5].Split(','),arguments[offset+4],new XmlExtensionLimits(int.Parse(arguments[offset+1]),int.Parse(arguments[offset+2]),long.Parse(arguments[offset+3])));byte[] output;
    try{byte[] data=File.ReadAllBytes(path);if(Path.GetFileName(path).StartsWith("skip",StringComparison.Ordinal)){var value=ExtensionOnlySkip.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),options);output=value.EncodeXml();}
      else if(Path.GetFileName(path).StartsWith("text",StringComparison.Ordinal)){var value=ExtensionText.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),options);output=value.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),options);}
      else{var value=ExtensionRoot.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),options);output=value.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),options);var copy=ExtensionRoot.DecodeXml(output,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),options);foreach(var protocol in Enum.GetValues<Protocol>())if(!value.Encode(protocol).AsSpan().SequenceEqual(copy.Encode(protocol)))throw new Exception("Native wire changed");if(!output.AsSpan().SequenceEqual(copy.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),options)))throw new Exception("Unstable opaque replay");}
    }catch(FormatException failure){if(valid)throw new Exception(path,failure);continue;}if(!valid)throw new Exception("Invalid extension accepted: "+path);File.WriteAllBytes(path+".csharp.xml",output);
  }
  {byte[] data=File.ReadAllBytes(arguments[0]);int low=0,high=65536;while(low<high){int middle=(low+high)/2;try{Read(data,Policy(middle));high=middle;}catch(FormatException){low=middle+1;}}int exact=low;var precise=Policy(exact);var value=Read(data,precise);byte[] previous=Write(value,precise);for(int iteration=0;iteration<4;++iteration){value=Read(previous,precise);byte[] next=Write(value,precise);if(!previous.AsSpan().SequenceEqual(next))throw new Exception("Unstable exact preserved budget");previous=next;}Rejected(()=>Read(data,Policy(exact-1)));Rejected(()=>Write(value,Policy(exact-1)));value.Items.Add(3);if(Read(Write(value,precise),precise).Items.Count!=3)throw new Exception("Known edit lost");value.Items.Clear();Rejected(()=>Write(value,precise));
  var source=new ExtensionSource();source.Current=Read(data,Policy(65536));var envelope=new ExtensionEnvelope();envelope.Value.Bind(source);if(!Write(source.Current,precise).AsSpan().SequenceEqual(envelope.Value.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),precise)))throw new Exception("Readonly reference lost opaque state");Rejected(()=>source.WithProject(view=>view.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),precise)));
  string document=System.Text.Encoding.UTF8.GetString(data);byte[] nested=System.Text.Encoding.UTF8.GetBytes("<holder xmlns=\"'+NS+'\">"+document+document+"</holder>");var holder=ExtensionHolder.DecodeXml(nested,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),Policy(65536));if(holder.Values.Count!=2)throw new Exception("Owner-local extension counters conflated");File.WriteAllBytes(arguments[0]+".nested.csharp.xml",holder.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),Policy(65536)));
  Console.WriteLine("C# exact preserved-byte boundaries, edits, nested owner-local counters and readonly extension forwarding pass");}}
}'''

GO=r'''package main
import("bytes";"fmt";"os";"path/filepath";"strconv";"strings")
// Exercise typed direct event traversal while retaining opaque vocabulary only in explicit slots.
func main(){arguments:=os.Args[1:];for offset:=0;offset<len(arguments);offset+=6{path:=arguments[offset];valid:=strings.HasSuffix(path,".valid.xml");slots,_:=strconv.Atoi(arguments[offset+1]);items,_:=strconv.Atoi(arguments[offset+2]);bound,_:=strconv.Atoi(arguments[offset+3]);var understood []string;if arguments[offset+5]!=""{understood=strings.Split(arguments[offset+5],",")};options:=XmlExtensionOptions{understood,arguments[offset+4],XmlExtensionLimits{slots,items,bound}};data,err:=os.ReadFile(path);if err!=nil{panic(err)};var output []byte
  if strings.HasPrefix(filepath.Base(path),"skip"){value,failure:=DecodeExtensionOnlySkipXMLWithAllBounds(data,DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits(),options);err=failure;if err==nil{output,err=value.EncodeXML()}}else if strings.HasPrefix(filepath.Base(path),"text"){value,failure:=DecodeExtensionTextXMLWithExtensionOptions(data,options);err=failure;if err==nil{output,err=value.EncodeXMLWithExtensionOptions(options)}}else{value,failure:=DecodeExtensionRootXMLWithAllBounds(data,DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits(),options);err=failure;if err==nil{output,err=value.EncodeXMLWithAllBounds(DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits(),options)};if err==nil{copy,failure:=DecodeExtensionRootXMLWithAllBounds(output,DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits(),options);if failure!=nil{panic(failure)};for _,protocol:=range []Protocol{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING}{first,failure:=value.Encode(protocol);if failure!=nil{panic(failure)};second,failure:=copy.Encode(protocol);if failure!=nil||!bytes.Equal(first,second){panic("Native wire changed")}};again,failure:=copy.EncodeXMLWithAllBounds(DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits(),options);if failure!=nil||!bytes.Equal(again,output){panic("Unstable opaque replay")}}}
  if err!=nil{if valid{panic(fmt.Errorf("%s: %w",path,err))};continue};if !valid{panic("Invalid extension accepted: "+path)};if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}
};extensionChecks(arguments[0]);fmt.Println("Go opaque scope, native wire and exact positioned admission pass")}
// Carry a complete explicit policy through owning and live borrowed exports.
func extensionRead(data []byte,options XmlExtensionOptions)(*ExtensionRoot,error){return DecodeExtensionRootXMLWithExtensionOptions(data,options)}
// Replay only the original private sidecar and selected native fields.
func extensionWrite(value *ExtensionRoot,options XmlExtensionOptions)([]byte,error){return value.EncodeXMLWithExtensionOptions(options)}
// Supply exact owner-local preserved storage and item policies.
func extensionPolicy(bound int)XmlExtensionOptions{return XmlExtensionOptions{[]string{"urn:vendor","urn:parent"},"",XmlExtensionLimits{5,5,bound}}}
// Probe exact boundaries, edited collection positions, live references and nested owner admission.
func extensionChecks(path string){data,err:=os.ReadFile(path);if err!=nil{panic(err)};low,high:=0,65536;for low<high{middle:=(low+high)/2;if _,err:=extensionRead(data,extensionPolicy(middle));err==nil{high=middle}else{low=middle+1}};precise:=extensionPolicy(low);value,err:=extensionRead(data,precise);if err!=nil{panic(err)};previous,err:=extensionWrite(value,precise);if err!=nil{panic(err)};for iteration:=0;iteration<4;iteration++{value,err=extensionRead(previous,precise);if err!=nil{panic(err)};next,failure:=extensionWrite(value,precise);if failure!=nil||!bytes.Equal(previous,next){panic("Unstable exact preserved budget")};previous=next};if candidate,failure:=extensionRead(data,extensionPolicy(low-1));candidate!=nil||failure==nil{panic("Below-floor input published")};if output,failure:=extensionWrite(value,extensionPolicy(low-1));output!=nil||failure==nil{panic("Below-floor output published")};value.Items=append(value.Items,3);output,err:=extensionWrite(value,precise);if err!=nil{panic(err)};copy,err:=extensionRead(output,precise);if err!=nil||len(copy.Items)!=3{panic("Known edit lost")};value.Items=nil;if output,failure:=extensionWrite(value,precise);output!=nil||failure==nil{panic("Lost extension position accepted")}
  source:=NewExtensionSource();source.Current,err=extensionRead(data,extensionPolicy(65536));if err!=nil{panic(err)};envelope:=NewExtensionEnvelope();if err=envelope.Value.Bind(source);err!=nil{panic(err)};direct,err:=extensionWrite(source.Current,precise);if err!=nil{panic(err)};borrowed,err:=envelope.Value.EncodeXMLWithExtensionOptions(precise);if err!=nil||!bytes.Equal(direct,borrowed){panic("Readonly reference lost opaque state")};source.WithProject(func(view ExtensionSourceProjectTranslator){if output,failure:=view.EncodeXMLWithExtensionOptions(precise);output!=nil||failure==nil{panic("Translator invented required opaque state")}})
  nested:=[]byte("<holder xmlns=\"urn:serializer:extension-tokens\">"+string(data)+string(data)+"</holder>");holder,err:=DecodeExtensionHolderXMLWithExtensionOptions(nested,extensionPolicy(65536));if err!=nil||len(holder.Values)!=2{panic(fmt.Errorf("Owner-local counters conflated: %w",err))};output,err=holder.EncodeXMLWithExtensionOptions(extensionPolicy(65536));if err!=nil{panic(err)};if err=os.WriteFile(path+".nested.go.xml",output,0600);err!=nil{panic(err)};fmt.Println("Go exact preserved-byte boundaries, edits, nested owner-local counters and readonly extension forwarding pass")
}
'''


def forbid_retained_document(path,language):
    """Make any fallback to the retained input document fail under the actual target SDK."""
    source=path.read_text(encoding='utf-8')
    if language=='java':
        source=source.replace('private static SrlXmlNode srlXmlParse(byte[] bytes, Limits limits) {','private static boolean srlXmlTreeForbidden=true;\nprivate static SrlXmlNode srlXmlParse(byte[] bytes, Limits limits) { if(srlXmlTreeForbidden)throw new AssertionError("Retained XML document forbidden");',1)
    elif language=='csharp':
        source=source.replace('private static SrlXmlNode SrlXmlParse(byte[] bytes, Limits limits) {','private static bool SrlXmlTreeForbidden=true;\nprivate static SrlXmlNode SrlXmlParse(byte[] bytes, Limits limits) { if(SrlXmlTreeForbidden)throw new System.InvalidOperationException("Retained XML document forbidden");',1)
    else:
        source=source.replace('func srlXmlParse(data []byte, limits XmlLimits) (*srlXmlNode,error) {','var srlXmlTreeForbidden=true\nfunc srlXmlParse(data []byte, limits XmlLimits) (*srlXmlNode,error) { if srlXmlTreeForbidden{return nil,fmt.Errorf("Retained XML document forbidden")}',1)
        source=source.replace('func srlXmlWrite(node *srlXmlNode, limits XmlLimits)([]byte,error) {','func srlXmlWrite(node *srlXmlNode, limits XmlLimits)([]byte,error) { if srlXmlTreeForbidden{return nil,fmt.Errorf("Retained XML output forbidden")}',1)
    if 'Retained XML document forbidden' not in source:raise AssertionError('Missing retained document tripwire: '+language)
    path.write_text(source,encoding='utf-8')


def rejected_bindings(args,languages):
    """Reject unrepresentable native counters atomically while preserving all prior backend outputs."""
    directory=args.build/'rejections';directory.mkdir(exist_ok=True);model=directory/'model.serializer';model.write_text(MODEL,encoding='utf-8')
    for label,key in [('maximum','extension.end.maximum'),('minimum','extension.before.minimum')]:
        config=directory/'extensions.ini';value=re.sub(r'(?m)^'+re.escape(key)+r'=.*$',key+'=2147483648',OPTIONS)
        if key.endswith('minimum'):value=value.replace('extension.before.maximum=2','extension.before.maximum=2147483648')
        config.write_text(value,encoding='utf-8');command=[args.compiler,'--input',model,'--config',config,'--language',','.join(languages)];outputs=[]
        for language in languages:
            output=directory/sdk.OUTPUTS[language];output.write_text('preserve prior output',encoding='utf-8');outputs.append(output);command += [f'--{language}.output',output]
        result=subprocess.run([str(value) for value in command],capture_output=True,text=True,encoding='utf-8',timeout=60)
        if result.returncode==0 or 'native counter range' not in result.stdout+result.stderr or any(output.read_text(encoding='utf-8')!='preserve prior output' for output in outputs):raise AssertionError(f'Invalid extension {label}: {result.stdout} {result.stderr}')


def main():
    """Generate all selected backends and check native SDK results with an independent XML provider."""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler',type=Path,required=True);parser.add_argument('--build',type=Path,required=True);parser.add_argument('--language',default='java,csharp,go');parser.add_argument('--wsl-languages',default='');parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args();args.compiler=args.compiler.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if not languages or len(languages)!=len(set(languages)) or any(value not in ('java','csharp','go') for value in languages):parser.error('Select distinct language names from java,csharp,go')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:parser.error('Repository output must remain beneath ignored out')
    args.build.mkdir(parents=True,exist_ok=True);oracle=etree.XMLSchema(etree.fromstring(XSD.encode()));paths=[]
    for label,document,policy,valid,xml_valid in cases():
        try:actual=oracle.validate(etree.fromstring(document.encode()))
        except etree.XMLSyntaxError:actual=False
        if actual!=xml_valid:raise AssertionError(f'Independent vocabulary oracle differs: {label}: {oracle.error_log}')
        path=args.build/f'{label}.{"valid" if valid else "invalid"}.xml';path.write_text(document,encoding='utf-8');paths.append((path,policy))
    model=args.build/'model.serializer';model.write_text(MODEL,encoding='utf-8');config=args.build/'extensions.ini';config.write_text(OPTIONS,encoding='utf-8');command=[args.compiler,'--input',model,'--config',config,'--language',','.join(languages),'--go.package','main']
    for language in languages:
        directory=args.build/language;directory.mkdir(exist_ok=True);command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args)
    for language in languages:forbid_retained_document(args.build/language/sdk.OUTPUTS[language],language)
    for language,filename,source in [('java','Main.java',JAVA),('csharp','Program.cs',CSHARP),('go','main.go',GO)]:
        if language not in languages:continue
        directory=args.build/'source'/language;directory.mkdir(parents=True,exist_ok=True);(directory/filename).write_text(source.replace("'+NS+'",NS),encoding='utf-8');launch=runner.build(language,'xml_extensions',args.build/language,directory);arguments=[]
        for path,policy in paths:Path(str(path)+f'.{language}.xml').unlink(missing_ok=True);arguments += [runner.path(language,path),*policy]
        sdk.run([*launch,*arguments])
        for path,_ in paths:
            output=Path(str(path)+f'.{language}.xml')
            if path.name.endswith('.valid.xml'):oracle.assertValid(etree.parse(str(output)));check_output(output.read_bytes())
            elif output.exists():raise AssertionError('Invalid partial publication: '+str(output))
        nested=Path(str(paths[0][0])+f'.nested.{language}.xml');oracle.assertValid(etree.parse(str(nested)));children=etree.parse(str(nested)).getroot();assert len(children)==2
        for child in children:check_output(etree.tostring(child))
    rejected_bindings(args,languages)
    print(f'PASS: {len(paths)} independent vocabulary/position/admission probes, exact storage/edit/readonly checks, no-tree tripwires and 2 atomic metadata gates across {len(languages)} actual SDKs',flush=True)


if __name__=='__main__':main()
