# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify Go/Java/C# QName contracts with independent JAXP/lxml and native-wire controls."""
import argparse
import importlib.util
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location('qname_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdk)
JAVA_ORACLE = '''import java.nio.file.Path;
import javax.xml.XMLConstants;
import javax.xml.transform.stream.StreamSource;
import javax.xml.validation.SchemaFactory;
import org.xml.sax.SAXException;
/** Validate original documents with the JDK's independent XSD implementation. */
public final class Main {
  /** Keep external resources disabled and classify each complete input without generated code. */
  public static void main(String[] paths) throws Exception {
    var factory=SchemaFactory.newInstance(XMLConstants.W3C_XML_SCHEMA_NS_URI);
    factory.setFeature(XMLConstants.FEATURE_SECURE_PROCESSING,true);
    factory.setProperty(XMLConstants.ACCESS_EXTERNAL_DTD,"");
    factory.setProperty(XMLConstants.ACCESS_EXTERNAL_SCHEMA,"");
    var schema=factory.newSchema(Path.of(paths[0]).toFile());
    for(int index=1;index<paths.length;++index){
      boolean expected=paths[index].contains(".valid.xml");
      try{schema.newValidator().validate(new StreamSource(Path.of(paths[index]).toFile()));
        if(!expected)throw new AssertionError("Invalid document admitted: "+paths[index]);
      }catch(SAXException failure){if(expected)throw new AssertionError(paths[index],failure);}
    }
    System.out.println("Independent JAXP XSD decisions pass for "+(paths.length-1)+" documents");
  }
}'''
XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:names" xmlns:a="urn:allowed"
 targetNamespace="urn:names" elementFormDefault="qualified">
 <xs:simpleType name="Kind"><xs:restriction base="xs:QName"><xs:enumeration value="a:item"/><xs:enumeration value="local"/><xs:enumeration value="t:item"/></xs:restriction></xs:simpleType>
 <xs:simpleType name="Names"><xs:list itemType="xs:QName"/></xs:simpleType>
 <xs:simpleType name="AllowedNames"><xs:restriction base="t:Names"><xs:enumeration value="a:one a:two"/></xs:restriction></xs:simpleType>
 <xs:simpleType name="Union"><xs:union memberTypes="t:Kind t:AllowedNames xs:unsignedShort"/></xs:simpleType>
 <xs:simpleType name="StringFirst"><xs:union memberTypes="xs:string xs:QName"/></xs:simpleType>
 <xs:simpleType name="StringLast"><xs:union memberTypes="t:Kind xs:string"/></xs:simpleType>
 <xs:simpleType name="PatternName"><xs:restriction base="xs:QName"><xs:pattern value="(p|srl):item|local"/></xs:restriction></xs:simpleType>
 <xs:simpleType name="PatternNames"><xs:restriction base="t:Names"><xs:pattern value="(p|srl):one (p|srl):two"/></xs:restriction></xs:simpleType>
 <xs:simpleType name="Prefixes"><xs:restriction base="xs:string"><xs:enumeration value="p srl"/></xs:restriction></xs:simpleType>
 <xs:element name="patterned"><xs:complexType><xs:sequence><xs:element name="names" type="t:PatternNames"/></xs:sequence>
 <xs:attribute name="value" type="t:PatternName" use="required"/><xs:attribute name="prefixes" type="t:Prefixes" use="required"/>
 </xs:complexType></xs:element>
 <xs:element name="unions"><xs:complexType><xs:sequence>
 <xs:element name="value" type="t:Union" minOccurs="0" maxOccurs="unbounded"/>
 <xs:element name="raw" type="t:StringFirst"/><xs:element name="fallback" type="t:StringLast"/>
 </xs:sequence><xs:attribute name="kind" type="t:Kind" use="required"/></xs:complexType></xs:element>
 <xs:complexType name="OwnerAttribute">
 <xs:attribute name="no_namespace" type="xs:QName" default="local"/>
 <xs:attribute name="value" type="t:StringLast" use="required"/>
 <xs:attribute name="names" type="t:Names" default="local a:item"/>
 <xs:attribute name="fixed_names" type="t:Names" fixed="local a:item"/>
 </xs:complexType>
 <xs:element name="owner_attribute" type="t:OwnerAttribute"/>
 <xs:complexType name="OwnerText"><xs:simpleContent><xs:extension base="t:StringLast">
 <xs:attribute name="no_namespace" type="xs:QName" default="local"/>
 <xs:attribute name="names" type="t:Names" default="local a:item"/>
 <xs:attribute name="fixed_names" type="t:Names" fixed="local a:item"/>
 </xs:extension></xs:simpleContent></xs:complexType>
 <xs:element name="owner_text" type="t:OwnerText"/>
 <xs:element name="envelope"><xs:complexType><xs:sequence>
 <xs:element name="child" type="t:OwnerAttribute"/>
 </xs:sequence></xs:complexType></xs:element>
 <xs:element name="record"><xs:complexType><xs:sequence>
 <xs:element name="value" type="t:Kind" minOccurs="0" maxOccurs="unbounded"/>
 <xs:element name="child_names" type="t:AllowedNames"/>
 <xs:element name="lexical" type="t:AllowedNames"/>
 </xs:sequence><xs:attribute name="kind" type="t:Kind" use="required"/>
 <xs:attribute name="names" type="t:AllowedNames" use="required"/></xs:complexType></xs:element>
 <xs:element name="defaults_record"><xs:complexType><xs:sequence>
 <xs:element name="value" type="xs:QName" default="a:item"/>
 <xs:element name="fixed_value" type="xs:QName" fixed="a:item"/>
 <xs:element name="names" type="t:AllowedNames" default="a:one a:two"/>
 <xs:element name="lexical" type="t:AllowedNames" fixed="a:one a:two"/>
 </xs:sequence><xs:attribute name="category" type="t:Kind" default="a:item"/>
 <xs:attribute name="fixed" type="t:Kind" fixed="a:item"/>
 <xs:attribute name="required_fixed" type="t:Kind" use="required" fixed="a:item"/>
 <xs:attribute name="no_namespace" type="xs:QName" default="local"/>
 <xs:attribute name="attribute_names" type="t:AllowedNames" default="a:one a:two"/>
 <xs:attribute name="fixed_names" type="t:AllowedNames" fixed="a:one a:two"/>
 </xs:complexType></xs:element>
 </xs:schema>'''
MODEL = '''serializer version 1.20.0;
namespace names {
  using schema "names.xsd";
  class record {
    public string kind (1) xml(attribute, qname);
    public array[2] string attribute_names (2) xml(attribute, name("names"), list);
    public array<string> values (3) xml(name("value"));
    public array[2] string names (4) xml(name("child_names"), list);
    public string lexical (5);
  }
  class defaults_record {
    public string category (1) xml(attribute);
    public string fixed (2) xml(attribute);
    public string required_fixed (3) xml(attribute);
    public string no_namespace (4) xml(attribute);
    public array[2] string attribute_names (5) xml(attribute, list);
    public array[2] string fixed_names (6) xml(attribute, list);
    public string value (7);
    public string fixed_value (8);
    public array[2] string names (9) xml(list);
    public string lexical (10);
  }
  class unions {
    public array<string> values (1) xml(name("value"));
    public string raw (2);
    public string fallback (3);
    public string kind (4) xml(attribute);
  }
  class owner_attribute {
    public string no_namespace (1) xml(attribute);
    public string value (2) xml(attribute);
    public array[2] string names (3) xml(attribute, list);
    public array[2] string fixed_names (4) xml(attribute, list);
  }
  class owner_text {
    public string no_namespace (1) xml(attribute);
    public string value (2) xml(text);
    public array[2] string names (3) xml(attribute, list);
    public array[2] string fixed_names (4) xml(attribute, list);
  }
  class envelope { public owner_attribute child (1); }
  class patterned {
    public string value (1) xml(attribute);
    public string prefixes (2) xml(attribute, prefix_list);
    public array[2] string names (3) xml(list);
  }
}
namespace collision {
  xml(namespace("urn:collision"));
  class root { public uint32 srl_xml_reserve_namespaces (1); }
}
'''
GO = r'''package main
import("bytes";"fmt";"os";"path/filepath";"strings")
// Require nil output on every rejected enumeration mutation.
func rejected(value *NamesRecord){out,err:=value.EncodeXML();if err==nil||out!=nil{panic("Invalid QName enumeration published")}}
// Preserve ordinary strings/wire bytes while checking scope-dependent scalar and ordered-list identities.
func main(){
for _,path:=range os.Args[1:]{input,err:=os.ReadFile(path);if err!=nil{panic(err)}
if strings.HasPrefix(filepath.Base(path),"defaults-"){defaults(path,input);continue}
if strings.HasPrefix(filepath.Base(path),"unions-"){unions(path,input);continue}
if strings.HasPrefix(filepath.Base(path),"owner-"){owner(path,input);continue}
if strings.HasPrefix(filepath.Base(path),"pattern-"){pattern(path,input);continue}
value,err:=DecodeNamesRecordXML(input);valid:=strings.HasSuffix(path,".valid.xml")
if !valid{if err==nil||value!=nil{panic("Invalid QName enumeration admitted: "+path)};continue};if err!=nil{panic(err)}
protocols:=[]Protocol{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING};before:=make([][]byte,len(protocols))
for index,protocol:=range protocols{before[index],err=value.Encode(protocol);if err!=nil{panic(err)}}
output,err:=value.EncodeXML();if err!=nil{panic(err)}
if _,err=DecodeNamesRecordXML(output);err!=nil{panic(err)}
for index,protocol:=range protocols{after,failure:=value.Encode(protocol);if failure!=nil||!bytes.Equal(before[index],after){panic("XML modified native fields")}}
saved:=value.Kind;value.Kind="missing:item";rejected(value);value.Kind=saved
savedNames:=append([]string(nil),value.Names...);value.Names[0],value.Names[1]=value.Names[1],value.Names[0];rejected(value);value.Names=savedNames
savedLexical:=value.Lexical;value.Lexical="p:two p:one";rejected(value);value.Lexical=savedLexical
if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}
}
fmt.Println("Go QName scalar/list enumeration, scoped aliases and transactional output pass")
}
// Materialize schema defaults in private scopes and compare fixed values through expanded identity.
func defaults(path string,input []byte){
value,err:=DecodeNamesDefaultsRecordXML(input);valid:=strings.HasSuffix(path,".valid.xml")
if !valid{if err==nil||value!=nil{panic("Invalid QName default/fixed candidate: "+path)};return};if err!=nil{panic(err)}
before,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};output,err:=value.EncodeXML();if err!=nil{panic(err)}
after,err:=value.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(before,after){panic("QName defaults changed native fields during XML encode")}
if _,err=DecodeNamesDefaultsRecordXML(output);err!=nil{panic(err)}
saved:=value.Fixed;value.Fixed="p:wrong";data,failure:=value.EncodeXML();if failure==nil||data!=nil{panic("Changed scalar fixed QName published")};value.Fixed=saved
savedNames:=append([]string(nil),value.FixedNames...);value.FixedNames[0],value.FixedNames[1]=value.FixedNames[1],value.FixedNames[0];data,failure=value.EncodeXML();if failure==nil||data!=nil{panic("Changed fixed QName list published")};value.FixedNames=savedNames
if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}
}
// Preserve QName/list/numeric union selection and leave selected string members uninterpreted.
func unions(path string,input []byte){
value,err:=DecodeNamesUnionsXML(input);valid:=strings.HasSuffix(path,".valid.xml")
if !valid{if err==nil||value!=nil{panic("Invalid QName union candidate: "+path)};return};if err!=nil{panic(err)}
before,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};output,err:=value.EncodeXML();if err!=nil{panic(err)}
after,err:=value.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(before,after){panic("XML union output changed native fields")}
if _,err=DecodeNamesUnionsXML(output);err!=nil{panic(err)}
value.Values[0]="missing:item";data,failure:=value.EncodeXML();if failure==nil||data!=nil{panic("Invalid QName union output published")}
if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}
}
// Keep schema-owned no-namespace defaults private while a union string retains its owner default.
func owner(path string,input []byte){
var value *NamesOwnerAttribute;var text *NamesOwnerText;var envelope *NamesEnvelope;var err error
var encode func()([]byte,error);var native func()([]byte,error);var mutate func(string)
if strings.Contains(filepath.Base(path),"text"){text,err=DecodeNamesOwnerTextXML(input);if err==nil{if text.NoNamespace!="local"||text.Value!="local"||len(text.Names)!=2||len(text.FixedNames)!=2{panic("Owner defaults missing")};encode=func()([]byte,error){return text.EncodeXML()};native=func()([]byte,error){return text.Encode(BINARY_NONE)};mutate=func(value string){text.NoNamespace=value}}
}else{if strings.Contains(filepath.Base(path),"envelope"){envelope,err=DecodeNamesEnvelopeXML(input);if err==nil{value=envelope.Child;encode=func()([]byte,error){return envelope.EncodeXML()};native=func()([]byte,error){return envelope.Encode(BINARY_NONE)}}}else{value,err=DecodeNamesOwnerAttributeXML(input);if err==nil{encode=func()([]byte,error){return value.EncodeXML()};native=func()([]byte,error){return value.Encode(BINARY_NONE)}}};if err==nil{if value.NoNamespace!="local"||value.Value!="local"||len(value.Names)!=2||len(value.FixedNames)!=2{panic("Owner defaults missing")};mutate=func(text string){value.NoNamespace=text}}}
if err!=nil{panic(err)};before,err:=native();if err!=nil{panic(err)};output,err:=encode();if err!=nil{panic(err)};after,err:=native();if err!=nil||!bytes.Equal(before,after){panic("Owner planning changed native fields")}
mutate("different");data,failure:=encode();if failure==nil||data!=nil{panic("Unrepresentable owner QName state published")};mutate("local")
if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}
}
// Preserve pattern-sensitive aliases and prefix-list lexical enumeration without native edits.
func pattern(path string,input []byte){
value,err:=DecodeNamesPatternedXML(input);valid:=strings.HasSuffix(path,".valid.xml")
if !valid{if err==nil||value!=nil{panic("Invalid namespace pattern admitted")};return};if err!=nil{panic(err)}
before,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};output,err:=value.EncodeXML();if err!=nil{panic(err)}
after,err:=value.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(before,after){panic("Namespace pattern changed native fields")};if _,err=DecodeNamesPatternedXML(output);err!=nil{panic(err)}
value.Value="q:item";data,failure:=value.EncodeXML();if failure==nil||data!=nil{panic("Invalid namespace pattern published")}
if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}
}'''
JAVA = '''import java.nio.file.*;import java.util.*;
/** Exercise actual generated QName contracts without enabling optional runtime schema validation. */
public final class Main {
  /** Require complete output rejection before returning an owning byte array. */
  private static void rejected(Runnable work){try{work.run();throw new AssertionError("Invalid QName output published");}catch(IllegalArgumentException expected){}}
  /** Check schema augmentation and native-byte stability through a common owning callback. */
  private static byte[] ownerBytes(java.util.function.Supplier<byte[]> encode,java.util.function.Supplier<byte[]> nativeBytes,java.util.function.Consumer<String> mutate){byte[] before=nativeBytes.get(),output=encode.get();if(!Arrays.equals(before,nativeBytes.get()))throw new AssertionError("Owner planning changed native fields");mutate.accept("different");rejected(()->encode.get());mutate.accept("local");return output;}
  /** Preserve native fields while regenerating scalar/list declarations and selected union members. */
  public static void main(String[] paths)throws Exception{for(String path:paths){
    boolean valid=path.endsWith(".valid.xml");byte[] input=Files.readAllBytes(Path.of(path)),output;
    String file=Path.of(path).getFileName().toString();
    try{if(file.startsWith("pattern-")){var value=Schema.Names.Patterned.decodeXml(input);byte[] before=value.encode(Schema.Protocol.BINARY_NONE);output=value.encodeXml();if(!Arrays.equals(before,value.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Namespace pattern changed native fields");Schema.Names.Patterned.decodeXml(output);value.value="q:item";rejected(()->value.encodeXml());
    }else if(file.startsWith("owner-")){if(file.contains("text")){var value=Schema.Names.OwnerText.decodeXml(input);if(!value.noNamespace.equals("local")||!value.value.equals("local")||value.names.length!=2||value.fixedNames.length!=2)throw new AssertionError("Owner defaults missing");output=ownerBytes(()->value.encodeXml(),()->value.encode(Schema.Protocol.BINARY_NONE),text->value.noNamespace=text);
      }else{var envelope=file.contains("envelope")?Schema.Names.Envelope.decodeXml(input):null;var value=envelope==null?Schema.Names.OwnerAttribute.decodeXml(input):envelope.child;if(!value.noNamespace.equals("local")||!value.value.equals("local")||value.names.length!=2||value.fixedNames.length!=2)throw new AssertionError("Owner defaults missing");output=envelope==null?ownerBytes(()->value.encodeXml(),()->value.encode(Schema.Protocol.BINARY_NONE),text->value.noNamespace=text):ownerBytes(()->envelope.encodeXml(),()->envelope.encode(Schema.Protocol.BINARY_NONE),text->value.noNamespace=text);}
    }else if(file.startsWith("defaults-")){var value=Schema.Names.DefaultsRecord.decodeXml(input);byte[] before=value.encode(Schema.Protocol.BINARY_NONE);output=value.encodeXml();if(!Arrays.equals(before,value.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Native default fields changed");Schema.Names.DefaultsRecord.decodeXml(output);
      String saved=value.fixed;value.fixed="p:wrong";rejected(()->value.encodeXml());value.fixed=saved;
      String[] names=value.fixedNames.clone();value.fixedNames[0]=names[1];value.fixedNames[1]=names[0];rejected(()->value.encodeXml());value.fixedNames=names;
    }else if(file.startsWith("unions-")){var value=Schema.Names.Unions.decodeXml(input);byte[] before=value.encode(Schema.Protocol.BINARY_NONE);output=value.encodeXml();if(!Arrays.equals(before,value.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Native union fields changed");Schema.Names.Unions.decodeXml(output);value.values.set(0,"missing:item");rejected(()->value.encodeXml());
    }else{var value=Schema.Names.Record.decodeXml(input);byte[] before=value.encode(Schema.Protocol.BINARY_NONE);output=value.encodeXml();if(!Arrays.equals(before,value.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Native enumeration fields changed");Schema.Names.Record.decodeXml(output);
      String saved=value.kind;value.kind="missing:item";rejected(()->value.encodeXml());value.kind=saved;
      String[] names=value.names.clone();value.names[0]=names[1];value.names[1]=names[0];rejected(()->value.encodeXml());value.names=names;
      value.lexical="p:two p:one";rejected(()->value.encodeXml());
    }}catch(IllegalArgumentException failure){if(valid)throw new AssertionError(path,failure);continue;}
    if(!valid)throw new AssertionError("Invalid QName input published: "+path);Files.write(Path.of(path+".java.xml"),output);
  }System.out.println("Java QName enumeration/default/fixed/union identities pass");}
}'''
CSHARP = '''using System;using System.IO;using static SerializerGenerated.Schema;
// Exercise complete generated QName candidates with strict native SDK compilation.
internal static class Program {
  // Require complete output rejection rather than a partially encoded result.
  private static void Rejected(Action work){try{work();throw new Exception("Invalid QName output published");}catch(FormatException){}}
  // Verify private schema defaults and transactional rejection of an unrepresentable edited state.
  private static byte[] OwnerBytes(Func<byte[]> encode,Func<byte[]> nativeBytes,Action<string> mutate){byte[] before=nativeBytes(),output=encode();if(!before.AsSpan().SequenceEqual(nativeBytes()))throw new Exception("Owner planning changed native fields");mutate("different");Rejected(()=>encode());mutate("local");return output;}
  // Preserve native fields while checking schema identities, ordered lists and union selection.
  private static void Main(string[] paths){foreach(string path in paths){bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);byte[] input=File.ReadAllBytes(path),output;string file=Path.GetFileName(path);
    try{if(file.StartsWith("pattern-",StringComparison.Ordinal)){var value=NamesPatterned.DecodeXml(input);byte[] before=value.Encode(Protocol.BINARY_NONE);output=value.EncodeXml();if(!before.AsSpan().SequenceEqual(value.Encode(Protocol.BINARY_NONE)))throw new Exception("Namespace pattern changed native fields");NamesPatterned.DecodeXml(output);value.Value="q:item";Rejected(()=>value.EncodeXml());
    }else if(file.StartsWith("owner-",StringComparison.Ordinal)){if(file.Contains("text",StringComparison.Ordinal)){var value=NamesOwnerText.DecodeXml(input);if(value.NoNamespace!="local"||value.Value!="local"||value.Names.Count!=2||value.FixedNames.Count!=2)throw new Exception("Owner defaults missing");output=OwnerBytes(()=>value.EncodeXml(),()=>value.Encode(Protocol.BINARY_NONE),text=>value.NoNamespace=text);
      }else{var envelope=file.Contains("envelope",StringComparison.Ordinal)?NamesEnvelope.DecodeXml(input):null;var value=envelope==null?NamesOwnerAttribute.DecodeXml(input):envelope.Child;if(value.NoNamespace!="local"||value.Value!="local"||value.Names.Count!=2||value.FixedNames.Count!=2)throw new Exception("Owner defaults missing");output=envelope==null?OwnerBytes(()=>value.EncodeXml(),()=>value.Encode(Protocol.BINARY_NONE),text=>value.NoNamespace=text):OwnerBytes(()=>envelope.EncodeXml(),()=>envelope.Encode(Protocol.BINARY_NONE),text=>value.NoNamespace=text);}
    }else if(file.StartsWith("defaults-",StringComparison.Ordinal)){var value=NamesDefaultsRecord.DecodeXml(input);byte[] before=value.Encode(Protocol.BINARY_NONE);output=value.EncodeXml();if(!before.AsSpan().SequenceEqual(value.Encode(Protocol.BINARY_NONE)))throw new Exception("Native default fields changed");NamesDefaultsRecord.DecodeXml(output);
      string saved=value.Fixed;value.Fixed="p:wrong";Rejected(()=>value.EncodeXml());value.Fixed=saved;
      var names=new System.Collections.Generic.List<string>(value.FixedNames);value.FixedNames[0]=names[1];value.FixedNames[1]=names[0];Rejected(()=>value.EncodeXml());value.FixedNames=names;
    }else if(file.StartsWith("unions-",StringComparison.Ordinal)){var value=NamesUnions.DecodeXml(input);byte[] before=value.Encode(Protocol.BINARY_NONE);output=value.EncodeXml();if(!before.AsSpan().SequenceEqual(value.Encode(Protocol.BINARY_NONE)))throw new Exception("Native union fields changed");NamesUnions.DecodeXml(output);value.Values[0]="missing:item";Rejected(()=>value.EncodeXml());
    }else{var value=NamesRecord.DecodeXml(input);byte[] before=value.Encode(Protocol.BINARY_NONE);output=value.EncodeXml();if(!before.AsSpan().SequenceEqual(value.Encode(Protocol.BINARY_NONE)))throw new Exception("Native enumeration fields changed");NamesRecord.DecodeXml(output);
      string saved=value.Kind;value.Kind="missing:item";Rejected(()=>value.EncodeXml());value.Kind=saved;
      var names=new System.Collections.Generic.List<string>(value.Names);value.Names[0]=names[1];value.Names[1]=names[0];Rejected(()=>value.EncodeXml());value.Names=names;
      value.Lexical="p:two p:one";Rejected(()=>value.EncodeXml());
    }}catch(FormatException failure){if(valid)throw new Exception(path,failure);continue;}
    if(!valid)throw new Exception("Invalid QName input published: "+path);File.WriteAllBytes(path+".csharp.xml",output);
  }Console.WriteLine("C# QName enumeration/default/fixed/union identities pass");}
}'''


def expanded(node, text):
    """Resolve each lexical name independently in the scope active on its own element."""
    prefix, colon, local = text.partition(':')
    return (node.nsmap[prefix], local) if colon else (node.nsmap.get(None, ''), text)


def identities(node):
    """Keep list order and child occurrences while ignoring prefix presentation choices."""
    if etree_name(node) == 'defaults_record':
        result = []
        for attribute in ('category', 'fixed', 'required_fixed', 'no_namespace', 'attribute_names', 'fixed_names'):
            result.extend(expanded(node, item) for item in node.get(attribute).split())
        for child in node:
            result.extend(expanded(child, item) for item in (child.text or '').split())
        return result
    result = [expanded(node, node.get('kind'))]
    result.extend(expanded(node, item) for item in node.get('names').split())
    for child in node:
        result.extend(expanded(child, item) for item in (child.text or '').split())
    return result


def etree_name(node):
    """Read the expanded local name without importing generated XML code."""
    return node.tag.rsplit('}', 1)[-1]


def fallback_member(node):
    """Select the fixture's first QName-enumeration member with an independent namespace lookup."""
    try:
        identity = expanded(node, node.text or '')
    except KeyError:
        return 'string'
    return 'qname' if identity in {('urn:allowed', 'item'), ('', 'local'), ('urn:names', 'item')} else 'string'


def oracle_control(node, etree):
    """Avoid libxml2's documented schema-prefix/default bug using equivalent expanded values."""
    if etree_name(node) != 'defaults_record':
        return node
    # Every field in this fixture is QName-valued. Rebuild presentation only, retaining
    # expanded element names, attribute presence, child order and every explicit value.
    aliases = {'urn:names': 'd', 'urn:allowed': 'a', 'urn:foreign': 'f'}
    control = etree.Element(node.tag, nsmap={alias: uri for uri, alias in aliases.items()})

    def lexical(owner, text):
        """Resolve through the original node before selecting an independent presentation alias."""
        values = []
        for token in text.split():
            uri, local = expanded(owner, token)
            values.append((aliases[uri] + ':' if uri else '') + local)
        return ' '.join(values)

    for name, text in node.attrib.items():
        control.set(name, lexical(node, text))
    for child in node:
        target = etree.SubElement(control, child.tag)
        target.text = lexical(child, child.text or '') or None
    return control


def documents():
    """Share independently authored namespace decisions across the actual SDK consumers."""
    base = ('<d:record xmlns:d="urn:names" xmlns:p="urn:allowed" xmlns:alias="urn:allowed" '
            'kind="alias:item" names="p:one alias:two"><d:value>p:item</d:value>'
            '<d:value xmlns:p="urn:allowed">p:item</d:value><d:value>local</d:value>'
            '<d:child_names xmlns:p="urn:allowed">p:one p:two</d:child_names>'
            '<d:lexical xmlns:p="urn:allowed">p:one p:two</d:lexical></d:record>')
    probes = [('aliases', base, True),
              ('no-namespace', base.replace('kind="alias:item"', 'kind="local"'), True),
              ('equivalent-prefix', base.replace('alias:item', 'p:item'), True),
              ('wrong-attribute', base.replace('kind="alias:item"', 'kind="p:wrong"'), False),
              ('wrong-child', base.replace('<d:value>p:item', '<d:value>p:wrong'), False),
              ('wrong-scope', base.replace('<d:value xmlns:p="urn:allowed">', '<d:value xmlns:p="urn:foreign">'), False),
              ('unbound', base.replace('kind="alias:item"', 'kind="missing:item"'), False),
              ('wrong-order', base.replace('names="p:one alias:two"', 'names="p:two alias:one"'), False),
              ('wrong-child-list', base.replace('>p:one p:two</d:child_names>', '>p:two p:one</d:child_names>'), False),
              ('wrong-lexical-list', base.replace('>p:one p:two</d:lexical>', '>p:two p:one</d:lexical>'), False),
              ('list-scope', base.replace('<d:child_names xmlns:p="urn:allowed">', '<d:child_names xmlns:p="urn:foreign">'), False)]
    defaults = ('<defaults_record xmlns="urn:names" xmlns:a="urn:foreign" xmlns:p="urn:allowed" '
                'required_fixed="p:item"><value/><fixed_value>p:item</fixed_value>'
                '<names/><lexical/></defaults_record>')
    probes.extend([
        ('defaults-shadowed', defaults, True),
        ('defaults-explicit-aliases', defaults.replace('required_fixed="p:item"',
         'required_fixed="p:item" category="p:item" fixed="p:item" '
         'attribute_names="p:one p:two" fixed_names="p:one p:two"'), True),
        ('defaults-required-missing', defaults.replace('required_fixed="p:item"', ''), False),
        ('defaults-wrong-fixed', defaults.replace('required_fixed="p:item"', 'required_fixed="p:wrong"'), False),
        ('defaults-wrong-child', defaults.replace('>p:item</fixed_value>', '>p:wrong</fixed_value>'), False),
        ('defaults-wrong-list', defaults.replace('required_fixed="p:item"',
         'required_fixed="p:item" fixed_names="p:two p:one"'), False),
        ('defaults-empty-attribute', defaults.replace('required_fixed="p:item"',
         'required_fixed="p:item" fixed=""'), False),
    ])
    union = ('<d:unions xmlns:d="urn:names" xmlns:p="urn:allowed" kind="p:item"><d:value>p:item</d:value>'
             '<d:value>p:one p:two</d:value><d:value>17</d:value>'
             '<d:raw>p:item</d:raw><d:fallback>missing:item</d:fallback></d:unions>')
    probes.extend([
        ('unions-mixed', union, True),
        ('unions-equivalent-scope', union.replace('<d:value>p:one', '<d:value xmlns:p="urn:allowed">p:one'), True),
        ('unions-string-fallback', union.replace('missing:item', 'some literal text'), True),
        ('unions-wrong-enum', union.replace('>p:item</d:value>', '>p:wrong</d:value>'), False),
        ('unions-wrong-order', union.replace('>p:one p:two</d:value>', '>p:two p:one</d:value>'), False),
        ('unions-shadowed', union.replace('<d:value>p:item', '<d:value xmlns:p="urn:foreign">p:item'), False),
        ('unions-unbound', union.replace('>p:item</d:value>', '>missing:item</d:value>'), False),
        ('unions-reserved-prefix', union.replace('missing:item', 'q:item'), True),
        ('unions-reserved-element-prefix', union.replace('missing:item', 'srl:item'), True),
        ('unions-retained-default', union.replace('<d:fallback>missing:item',
         '<d:fallback xmlns="urn:foreign">local'), True),
    ])
    probes.extend([
        ('owner-attribute', '<d:owner_attribute xmlns:d="urn:names" xmlns="urn:foreign" xmlns:a="urn:allowed" value="local"/>', True),
        ('owner-text', '<d:owner_text xmlns:d="urn:names" xmlns="urn:foreign" xmlns:a="urn:allowed">local</d:owner_text>', True),
        ('owner-envelope', '<d:envelope xmlns:d="urn:names" xmlns="urn:names"><d:child xmlns="urn:foreign" xmlns:a="urn:allowed" value="local"/></d:envelope>', True),
    ])
    patterned = ('<d:patterned xmlns:d="urn:names" xmlns:p="urn:allowed" xmlns:srl="urn:foreign" '
                 'value="p:item" prefixes="p srl"><d:names>p:one srl:two</d:names></d:patterned>')
    probes.extend([
        ('pattern-prefixed', patterned, True),
        ('pattern-element-alias', patterned.replace('value="p:item"', 'value="srl:item"'), True),
        ('pattern-default', patterned.replace('value="p:item"', 'xmlns="urn:foreign" value="local"'), True),
        ('pattern-wrong-alias', patterned.replace('value="p:item"', 'xmlns:q="urn:allowed" value="q:item"'), False),
        ('pattern-wrong-prefix-list', patterned.replace('prefixes="p srl"', 'prefixes="p p"'), False),
        ('pattern-wrong-list-alias', patterned.replace('>p:one srl:two<', '>d:one srl:two<'), False),
    ])
    return probes


def execute(args, etree):
    """Generate actual Go output and compare its decisions with an independent XSD provider."""
    (args.build / 'names.xsd').write_text(XSD, encoding='utf-8')
    (args.build / 'model.serializer').write_text(MODEL, encoding='utf-8')
    languages = args.language.split(',')
    generated = {language: args.build / language for language in languages}
    command = [args.compiler, '--input', args.build / 'model.serializer', '--language', args.language,
               '--go.package', 'main', '--csharp.namespace', 'SerializerGenerated']
    for language, directory in generated.items():
        directory.mkdir()
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command)
    probes = documents()
    schema = etree.XMLSchema(etree.fromstring(XSD.encode()))
    paths, oracle_paths = [], []
    for name, text, valid in probes:
        node = etree.fromstring(text.encode())
        if not name.startswith(('unions-', 'owner-')) and schema.validate(oracle_control(node, etree)) != valid:
            raise AssertionError(f'Independent oracle disagrees: {name}: {schema.error_log}')
        path = args.build / f'{name}.{"valid" if valid else "invalid"}.xml'
        path.write_text(text, encoding='utf-8')
        paths.append(path)
        if name.startswith('defaults-'):
            control_path = args.build / f'{name}.oracle.{"valid" if valid else "invalid"}.xml'
            control_path.write_bytes(etree.tostring(oracle_control(node, etree)))
            oracle_paths.append(control_path)
        else:
            oracle_paths.append(path)
    runner = sdk.Runner(args)
    oracle = args.build / 'oracle'
    oracle.mkdir()
    (oracle / 'Main.java').write_text(JAVA_ORACLE, encoding='utf-8')
    sdk.run(runner.command('java', 'javac', '--release', '17', '-encoding', 'UTF-8', '-Xlint:all',
                           '-Werror', '-d', runner.path('java', oracle), runner.path('java', oracle / 'Main.java')))
    java = runner.command('java', 'java', '-cp', runner.path('java', oracle), 'Main',
                          runner.path('java', args.build / 'names.xsd'))
    sdk.run([*java, *(runner.path('java', path) for path in oracle_paths)])
    outputs = []
    for language in languages:
        source = args.build / f'source-{language}'
        source.mkdir()
        filename, body = {'go': ('main.go', GO), 'java': ('Main.java', JAVA), 'csharp': ('Program.cs', CSHARP)}[language]
        (source / filename).write_text(body, encoding='utf-8')
        launch = runner.build(language, 'qname_constraints', generated[language], source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for (name, text, valid), path in zip(probes, paths):
            emitted = Path(str(path) + f'.{language}.xml')
            if valid:
                outputs.append(emitted)
                output = etree.parse(str(emitted))
                if not name.startswith(('unions-', 'owner-')):
                    schema.assertValid(oracle_control(output.getroot(), etree))
                if name.startswith('pattern-'):
                    root = output.getroot()
                    original = etree.fromstring(text.encode())
                    assert root.get('value') == original.get('value')
                    assert expanded(root, root.get('value')) == expanded(original, original.get('value'))
                    assert root.get('prefixes') == 'p srl'
                    assert [root.nsmap[prefix] for prefix in root.get('prefixes').split()] == [
                        'urn:allowed', 'urn:foreign']
                    child = root.find('{urn:names}names')
                    assert child.text == 'p:one srl:two'
                    assert [expanded(child, token) for token in child.text.split()] == [
                        ('urn:allowed', 'one'), ('urn:foreign', 'two')]
                elif name.startswith('owner-'):
                    root = output.getroot()
                    owner_node = root[0] if name == 'owner-envelope' else root
                    assert owner_node.nsmap.get(None, '') == 'urn:foreign'
                    assert (owner_node.text if name == 'owner-text' else owner_node.get('value')) == 'local'
                    for attribute in ('no_namespace', 'names', 'fixed_names'):
                        assert attribute not in owner_node.attrib
                elif name.startswith('unions-'):
                    root = output.getroot()
                    assert expanded(root, root.get('kind')) == ('urn:allowed', 'item')
                    values = root.findall('{urn:names}value')
                    assert expanded(values[0], values[0].text) == ('urn:allowed', 'item')
                    assert [expanded(values[1], item) for item in values[1].text.split()] == [
                        ('urn:allowed', 'one'), ('urn:allowed', 'two')]
                    assert values[2].text == '17'
                    original = etree.fromstring(text.encode())
                    for local in ('raw', 'fallback'):
                        assert root.find('{urn:names}' + local).text == original.find('{urn:names}' + local).text
                    assert fallback_member(root.find('{urn:names}fallback')) == fallback_member(
                        original.find('{urn:names}fallback'))
                elif name.startswith('defaults-'):
                    expected = [('urn:allowed', 'item')] * 3 + [('', 'local')]
                    expected += [('urn:allowed', part) for part in ('one', 'two')] * 2
                    expected += [('urn:allowed', 'item')] * 2
                    expected += [('urn:allowed', part) for part in ('one', 'two')] * 2
                    assert identities(output.getroot()) == expected
                else:
                    assert identities(output.getroot()) == identities(etree.fromstring(text.encode()))
            else:
                assert not emitted.exists()
    sdk.run([*java, *(runner.path('java', path) for path in outputs)])
    print(f'PASS: {len(probes)} independent QName probes across {len(languages)} actual SDKs')


def main():
    """Keep SDK failures visible and remove this run's private generated scratch on completion."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--python-dependencies', type=Path)
    parser.add_argument('--language', default='go')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    languages = args.language.split(',')
    if len(languages) != len(set(languages)) or any(language not in ('go', 'java', 'csharp') for language in languages):
        parser.error('Select distinct names from go,java,csharp')
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath ignored out/')
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    from lxml import etree
    args.build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='go-qname-constraints-', dir=args.build) as temporary:
        args.build = Path(temporary)
        execute(args, etree)


if __name__ == '__main__':
    main()
