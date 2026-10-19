# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify portable direct identity observers with independent XSD, native edits and exact limits."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

from lxml import etree

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_identity_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec); spec.loader.exec_module(sdk)
NS = 'urn:serializer:identity-test'
MODEL = '''serializer version 1.20.0; namespace identity_xml {
class entry {public optional<uint32> id(1);public optional<string> label(2);public optional<string> code(3);public optional<bool> flag(4);}
class request {public optional<uint32> id(1);public optional<string> code(2);public optional<bool> flag(3);}
class registry {public array request requests(1);public array entry entries(2);}
class registries {public array registry items(1);}
class keyed {public string value(1);}
class nil_keyed {public optional<string> value(1);}
class unique_value {public optional<string> value(1);}
class nullable_keys {public optional<optional<string>> items(1);}
}'''
OPTIONS = '''[xml]
enabled=true
xsd=identity.xsd
[xml.class.identity_xml::registry]
name.requests=ref
name.entries=entry
[xml.class.identity_xml::registries]
name.items=registry
[xml.class.identity_xml::nil_keyed]
presence.value=nil
[xml.class.identity_xml::nullable_keys]
name.items=value
presence.items=absent_or_nil
[xml.class.identity_xml::unique_value]
presence.value=nil
'''
XSD = (ROOT / 'test/resources/xml_identity_generated.xsd').read_text(encoding='utf-8')
XSD = XSD.replace('<xs:complexType name="entry">', '<xs:complexType name="entry"><xs:sequence><xs:element name="code" type="xs:decimal" minOccurs="0"/></xs:sequence><xs:attribute name="flag" type="xs:boolean"/>')
XSD = XSD.replace('<xs:complexType name="request">', '<xs:complexType name="request"><xs:sequence><xs:element name="code" type="xs:decimal" minOccurs="0"/></xs:sequence><xs:attribute name="flag" type="xs:boolean"/>')
XSD = XSD.replace('    <xs:keyref name="references"', '''    <xs:unique name="tuples"><xs:selector xpath=".//t:entry"/><xs:field xpath="t:code"/><xs:field xpath="@flag"/></xs:unique>
    <xs:keyref name="tuple_refs" refer="t:tuples"><xs:selector xpath="t:ref"/><xs:field xpath="t:code"/><xs:field xpath="@flag"/></xs:keyref>
    <xs:keyref name="references"''')
XSD = XSD.replace('</xs:schema>', '''<xs:element name="registries"><xs:complexType><xs:sequence><xs:element ref="t:registry" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element>
<xs:element name="keyed"><xs:complexType><xs:sequence><xs:element name="value" type="xs:string"/></xs:sequence></xs:complexType><xs:key name="scalar_key"><xs:selector xpath="t:value"/><xs:field xpath="."/></xs:key></xs:element>
<xs:element name="nil_keyed"><xs:complexType><xs:sequence><xs:element name="value" type="xs:string" nillable="true"/></xs:sequence></xs:complexType><xs:key name="nil_scalar_key"><xs:selector xpath="t:value"/><xs:field xpath="."/></xs:key></xs:element>
<xs:element name="unique_value"><xs:complexType><xs:sequence><xs:element name="value" type="xs:string" nillable="true"/></xs:sequence></xs:complexType><xs:unique name="scalar_unique"><xs:selector xpath="t:value"/><xs:field xpath="."/></xs:unique></xs:element>
<xs:element name="nullable_keys"><xs:complexType><xs:sequence><xs:element name="value" type="xs:string" minOccurs="0" nillable="true"/></xs:sequence></xs:complexType><xs:key name="nullable_key"><xs:selector xpath="t:value"/><xs:field xpath="."/></xs:key></xs:element></xs:schema>''')
DOCUMENT = '<registry xmlns="'+NS+'"><ref id="0007"/><ref id="7"/><entry id="7" label="ab"/></registry>'
DEFAULTS = (128, 32, 128, 100000, 100000, 8388608, 67108864)


def cases():
    """Exercise exact tuples, descendant selectors, forward references and independent nested scopes."""
    def document(body):
        return '<registry xmlns="'+NS+'">'+body+'</registry>'
    probes = [('forward', DOCUMENT, True), ('aliases', DOCUMENT.replace('0007', '+0007'), True),
              ('duplicate', document('<entry id="7"/><entry id="0007"/>'), False),
              ('missing', document('<entry/>'), False),
              ('unresolved', document('<ref id="8"/><entry id="7"/>'), False),
              ('unique', document('<entry id="7" label="a b"/><entry id="8" label=" a&#9;b "/>'), False),
              ('empty-unique', document('<entry id="7" label=""/><entry id="8" label=""/>'), False),
              ('incomplete', document('<ref/><entry id="7"/><entry id="8"/>'), True),
              ('empty', document(''), True),
              ('tuple', document('<ref flag="1"><code>+001.000</code></ref><entry id="7" flag="true"><code>1</code></entry>'), True),
              ('tuple-duplicate', document('<entry id="7" flag="true"><code>1.00</code></entry><entry id="8" flag="1"><code>+001</code></entry>'), False),
              ('tuple-incomplete', document('<ref><code>2</code></ref><entry id="7" flag="true"><code>1</code></entry>'), True),
              ('tuple-unresolved', document('<ref flag="false"><code>1</code></ref><entry id="7" flag="true"><code>1</code></entry>'), False),
              ('tuple-exact-decimal', document('<ref flag="false"><code>9007199254740993.00</code></ref><entry id="7" flag="0"><code>+09007199254740993</code></entry>'), True),
              ('tuple-distinct-decimal', document('<ref flag="false"><code>9007199254740992</code></ref><entry id="7" flag="0"><code>9007199254740993</code></entry>'), False),
              ('split-text', document('<ref flag="1"><code>1<![CDATA[.0]]><!--ignored-->0</code></ref><entry id="7" flag="true"><code>+001</code></entry>'), True),
              ('unicode', document('<entry id="7" label="é😀"/><entry id="8" label="é😀"/>'), False),
              ('unicode-distinct', document('<entry id="7" label="é😀"/><entry id="8" label="É😀"/>'), True),
              ('order', document('<entry id="7"/><ref id="7"/>'), False),
              ('duplicate-field', document('<entry id="7" flag="true"><code>1</code><code>2</code></entry>'), False),
              ('complex-field', document('<entry id="7" flag="true"><code><other/></code></entry>'), False),
              ('unknown', document('<entry id="7" other="1"/>'), False),
              ('trailing', DOCUMENT+'<extra/>', False)]
    opening='<registries xmlns="'+NS+'">'
    probes += [('owners', opening+DOCUMENT+DOCUMENT+'</registries>', True),
               ('owners-isolated', opening+document('<ref id="8"/><entry id="7"/>')+document('<entry id="8"/>')+'</registries>', False),
               ('owners-duplicate', opening+document('<entry id="7"/><entry id="7"/>')+'</registries>', False)]
    nil='<value xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:nil="true"/>'
    probes += [('scalar-key-nil','<nil_keyed xmlns="'+NS+'">'+nil+'</nil_keyed>',False),
               ('scalar-key-empty','<keyed xmlns="'+NS+'"><value/></keyed>',True),
               ('scalar-key-space','<keyed xmlns="'+NS+'"><value> </value></keyed>',True),
               ('scalar-unique-nil','<unique_value xmlns="'+NS+'">'+nil+'</unique_value>',True),
               ('scalar-unique-empty','<unique_value xmlns="'+NS+'"><value/></unique_value>',True),
               ('scalar-unique-nil-text','<unique_value xmlns="'+NS+'">'+nil.replace('/>','>x</value>')+'</unique_value>',False),
               ('scalar-key-nil-present','<nil_keyed xmlns="'+NS+'"><value/></nil_keyed>',False),
               ('scalar-key-nil-false','<nil_keyed xmlns="'+NS+'">'+nil.replace('true','false').replace('/>','>present</value>')+'</nil_keyed>',False),
               ('scalar-nullable-keys-empty','<nullable_keys xmlns="'+NS+'"/>',True),
               ('scalar-nullable-keys-present','<nullable_keys xmlns="'+NS+'"><value>present</value></nullable_keys>',False)]
    result = [(label, value, DEFAULTS, valid, valid) for label, value, valid in probes]
    # The base document selects seven constraint nodes and retains four tuples/five canonical bytes.
    exact = (5, 2, 2, 7, 4, 5, DEFAULTS[-1])
    result.append(('exact', DOCUMENT, exact, True, True))
    for index, name in enumerate(('constraints', 'fields', 'depth', 'selected', 'tuples', 'tuple-bytes', 'work')):
        limits=list(exact);limits[index]=0 if name=='work' else limits[index]-1
        result.append(('limit-'+name, DOCUMENT, tuple(limits), False, True))
    result.append(('empty-zero', document(''), (5,2,1,0,0,0,DEFAULTS[-1]), True, True))
    return result


JAVA = '''import java.nio.file.*;import java.util.*;
/** Qualify native aggregate edits and identity admission independently of optional runtime XSD. */
public final class Main {
  /** Require complete aggregate rejection before XML publication. */
  private static void rejected(Runnable work){try{work.run();throw new AssertionError("Invalid identity aggregate published");}catch(IllegalArgumentException expected){}}
  /** Match each independent validity/admission decision through the public owning codec. */
  public static void main(String[] args)throws Exception{
    rejected(()->new Schema.XmlIdentityLimits(-1,0,0,0,0,0,0));
    for(int argument=0;argument<args.length;argument+=8){String path=args[argument];boolean valid=path.endsWith(".valid.xml");byte[] data=Files.readAllBytes(Path.of(path)),output;var limits=new Schema.XmlIdentityLimits(Integer.parseInt(args[argument+1]),Integer.parseInt(args[argument+2]),Integer.parseInt(args[argument+3]),Integer.parseInt(args[argument+4]),Integer.parseInt(args[argument+5]),Integer.parseInt(args[argument+6]),Integer.parseInt(args[argument+7]));
      try{if(Path.of(path).getFileName().toString().startsWith("scalar-nullable-keys")){var owner=Schema.IdentityXml.NullableKeys.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);output=owner.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);owner.items=Optional.of(Optional.of(""));rejected(()->owner.encodeXml());}
      else if(Path.of(path).getFileName().toString().startsWith("scalar-key-nil")){var owner=Schema.IdentityXml.NilKeyed.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);output=owner.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);}
      else if(Path.of(path).getFileName().toString().startsWith("scalar-key")){var owner=Schema.IdentityXml.Keyed.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);output=owner.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);}
      else if(Path.of(path).getFileName().toString().startsWith("scalar-unique")){var owner=Schema.IdentityXml.UniqueValue.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);output=owner.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);}
      else if(Path.of(path).getFileName().toString().startsWith("owners")){var owner=Schema.IdentityXml.Registries.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);output=owner.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);}
      else{var value=Schema.IdentityXml.Registry.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);output=value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),limits);var copy=Schema.IdentityXml.Registry.decodeXml(output);if(!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE),copy.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Identity native wire changed");
        if(Path.of(path).getFileName().toString().startsWith("forward")){value.requests[0].id=Optional.of(8);rejected(()->value.encodeXml());value.requests[0].id=Optional.of(7);var saved=value.entries;value.entries=Arrays.copyOf(saved,2);value.entries[1]=saved[0];rejected(()->value.encodeXml());value.entries=saved;value.entries[0].id=Optional.empty();rejected(()->value.encodeXml());}}
      }catch(IllegalArgumentException failure){if(valid)throw new AssertionError(path,failure);continue;}if(!valid)throw new AssertionError("Invalid identity document published: "+path);Files.write(Path.of(path+".java.xml"),output);
    }System.out.println("Java direct scoped identities, exact tuples/aliases, native edits and admission pass");
  }
}'''

CSHARP = '''using System;using System.IO;using static SerializerGenerated.Schema;
// Match independent XSD/admission decisions without optional runtime validation.
internal static class Program {
  // Require invalid native aggregates to fail before returning XML bytes.
  private static void Rejected(Action work){try{work();throw new Exception("Invalid identity aggregate published");}catch(FormatException){}}
  // Exercise nested scopes, native wire identity and all explicit identity limits.
  private static void Main(string[] args){try{new XmlIdentityLimits(-1,0,0,0,0,0,0);throw new Exception("Negative identity policy");}catch(ArgumentOutOfRangeException){}
    for(int argument=0;argument<args.Length;argument+=8){string path=args[argument];bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);byte[] data=File.ReadAllBytes(path),output;var limits=new XmlIdentityLimits(int.Parse(args[argument+1],System.Globalization.CultureInfo.InvariantCulture),int.Parse(args[argument+2],System.Globalization.CultureInfo.InvariantCulture),int.Parse(args[argument+3],System.Globalization.CultureInfo.InvariantCulture),int.Parse(args[argument+4],System.Globalization.CultureInfo.InvariantCulture),int.Parse(args[argument+5],System.Globalization.CultureInfo.InvariantCulture),int.Parse(args[argument+6],System.Globalization.CultureInfo.InvariantCulture),int.Parse(args[argument+7],System.Globalization.CultureInfo.InvariantCulture));
      try{if(Path.GetFileName(path).StartsWith("scalar-nullable-keys",StringComparison.Ordinal)){var owner=IdentityXmlNullableKeys.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);output=owner.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);owner.Items=new ValueTuple<ValueTuple<string>?>(new ValueTuple<string>(""));Rejected(()=>owner.EncodeXml());}
      else if(Path.GetFileName(path).StartsWith("scalar-key-nil",StringComparison.Ordinal)){var owner=IdentityXmlNilKeyed.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);output=owner.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);}
      else if(Path.GetFileName(path).StartsWith("scalar-key",StringComparison.Ordinal)){var owner=IdentityXmlKeyed.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);output=owner.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);}
      else if(Path.GetFileName(path).StartsWith("scalar-unique",StringComparison.Ordinal)){var owner=IdentityXmlUniqueValue.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);output=owner.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);}
      else if(Path.GetFileName(path).StartsWith("owners",StringComparison.Ordinal)){var owner=IdentityXmlRegistries.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);output=owner.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);}
      else{var value=IdentityXmlRegistry.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);output=value.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),limits);var copy=IdentityXmlRegistry.DecodeXml(output);if(!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(copy.Encode(Protocol.BINARY_NONE)))throw new Exception("Identity native wire changed");
        if(Path.GetFileName(path).StartsWith("forward",StringComparison.Ordinal)){value.Requests[0].Id=new ValueTuple<uint>(8U);Rejected(()=>value.EncodeXml());value.Requests[0].Id=new ValueTuple<uint>(7U);value.Entries.Add(value.Entries[0]);Rejected(()=>value.EncodeXml());value.Entries.RemoveAt(1);value.Entries[0].Id=null;Rejected(()=>value.EncodeXml());}}
      }catch(FormatException failure){if(valid)throw new Exception(path,failure);continue;}if(!valid)throw new Exception("Invalid identity document published: "+path);File.WriteAllBytes(path+".csharp.xml",output);
    }Console.WriteLine("C# direct scoped identities, exact tuples/aliases, native edits and admission pass");
  }
}'''

GO = r'''package main
import("bytes";"fmt";"os";"path/filepath";"strconv";"strings")
// Require failure to discard the entire unpublished XML result.
func rejected(data []byte,err error){if data!=nil||err==nil{panic("Invalid identity aggregate published")}}
// Inspect independently scoped native aggregates under explicit identity admission.
func probe(path string,limits XmlIdentityLimits)([]byte,error){data,err:=os.ReadFile(path);if err!=nil{panic(err)}
  if strings.HasPrefix(filepath.Base(path),"scalar-nullable-keys"){value,err:=DecodeIdentityXmlNullableKeysXMLWithIdentityLimits(data,limits);if err!=nil{return nil,err};output,err:=value.EncodeXMLWithIdentityLimits(limits);if err!=nil{return nil,err};text:="";pointer:=&text;value.Items=&pointer;rejected(value.EncodeXML());return output,nil}
  if strings.HasPrefix(filepath.Base(path),"scalar-key-nil"){value,err:=DecodeIdentityXmlNilKeyedXMLWithIdentityLimits(data,limits);if err!=nil{return nil,err};return value.EncodeXMLWithIdentityLimits(limits)}
  if strings.HasPrefix(filepath.Base(path),"scalar-key"){value,err:=DecodeIdentityXmlKeyedXMLWithIdentityLimits(data,limits);if err!=nil{return nil,err};return value.EncodeXMLWithIdentityLimits(limits)}
  if strings.HasPrefix(filepath.Base(path),"scalar-unique"){value,err:=DecodeIdentityXmlUniqueValueXMLWithIdentityLimits(data,limits);if err!=nil{return nil,err};return value.EncodeXMLWithIdentityLimits(limits)}
  if strings.HasPrefix(filepath.Base(path),"owners"){value,err:=DecodeIdentityXmlRegistriesXMLWithIdentityLimits(data,limits);if err!=nil{if value!=nil{panic("Failed native identity candidate published")};return nil,err};return value.EncodeXMLWithIdentityLimits(limits)}
  value,err:=DecodeIdentityXmlRegistryXMLWithIdentityLimits(data,limits);if err!=nil{if value!=nil{panic("Failed native identity candidate published")};return nil,err};output,err:=value.EncodeXMLWithIdentityLimits(limits);if err!=nil{return nil,err};copy,err:=DecodeIdentityXmlRegistryXML(output);if err!=nil{return nil,err};first,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};second,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(first,second){panic("Identity native wire changed")}
  if strings.HasPrefix(filepath.Base(path),"forward"){next:=uint32(8);value.Requests[0].Id=&next;rejected(value.EncodeXML());next=7;value.Entries=append(value.Entries,value.Entries[0]);rejected(value.EncodeXML());value.Entries=value.Entries[:1];value.Entries[0].Id=nil;rejected(value.EncodeXML())};return output,nil
}
// Match each independent XML validity and exact admission policy, including negative limits.
func main(){rejected(NewIdentityXmlRegistry().EncodeXMLWithIdentityLimits(XmlIdentityLimits{-1,0,0,0,0,0,0}));for argument:=1;argument<len(os.Args);argument+=8{path:=os.Args[argument];var values [7]int;for index:=range values{value,err:=strconv.Atoi(os.Args[argument+index+1]);if err!=nil{panic(err)};values[index]=value};limits:=XmlIdentityLimits{values[0],values[1],values[2],values[3],values[4],values[5],values[6]};valid:=strings.HasSuffix(path,".valid.xml");output,err:=probe(path,limits);if valid!=(err==nil)||!valid&&output!=nil{panic(fmt.Sprintf("Identity oracle differs: %s: %v",path,err))};if err==nil{if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}}};fmt.Println("Go direct scoped identities, exact tuples/aliases, native edits and admission pass")}
'''

ORACLE = '''import javax.xml.validation.SchemaFactory;import javax.xml.transform.stream.StreamSource;import javax.xml.XMLConstants;import java.io.File;
/** Independent JDK XSD implementation verifies raw input, including libxml2 unsigned-plus aliases. */
public final class Oracle {
  /** Validate each document independently without loading generated model code. */
  public static void main(String[] args)throws Exception{var factory=SchemaFactory.newInstance(XMLConstants.W3C_XML_SCHEMA_NS_URI);factory.setProperty(XMLConstants.ACCESS_EXTERNAL_DTD,"");factory.setProperty(XMLConstants.ACCESS_EXTERNAL_SCHEMA,"");var schema=factory.newSchema(new File(args[0]));for(int index=1;index<args.length;index+=2){boolean valid=true;String failure="";try{schema.newValidator().validate(new StreamSource(new File(args[index])));}catch(org.xml.sax.SAXException expected){valid=false;failure=expected.getMessage();}if(valid!=Boolean.parseBoolean(args[index+1]))throw new AssertionError("Independent JDK identity oracle differs: "+args[index]+": "+failure);}System.out.println("Independent JDK XSD identity oracle pass");}
}'''


def rejected_bindings(args, languages):
    """Keep unsupported identity augmentation, XPath, scope and value spaces atomic and precise."""
    directory=args.build/'rejections';directory.mkdir(exist_ok=True)
    (directory/'model.serializer').write_text(MODEL,encoding='utf-8');(directory/'identity.ini').write_text(OPTIONS,encoding='utf-8')
    probes=[('default',XSD.replace('name="id" type="xs:unsignedInt"','name="id" type="xs:unsignedInt" default="7"'),'default/fixed'),
            ('fixed',XSD.replace('name="id" type="xs:unsignedInt"','name="id" type="xs:unsignedInt" fixed="7"'),'default/fixed'),
            ('union',XSD.replace('xpath="t:entry"','xpath="t:entry | t:ref"'),'identity'),
            ('scope',XSD.replace('refer="t:ids"','refer="t:missing"'),'XSD'),
            ('list',XSD.replace('type="xs:decimal"','type="xs:IDREFS"'),'identity field value space')]
    for label,schema,diagnostic in probes:
        (directory/'identity.xsd').write_text(schema,encoding='utf-8');command=[args.compiler,'--input',directory/'model.serializer','--config',directory/'identity.ini','--language',','.join(languages)]
        outputs=[]
        for language in languages:
            output=directory/sdk.OUTPUTS[language];output.write_text('preserve existing output',encoding='utf-8');outputs.append(output);command += [f'--{language}.output',output]
        result=subprocess.run([str(item) for item in command],capture_output=True,encoding='utf-8',timeout=60)
        if result.returncode==0 or diagnostic not in result.stdout+result.stderr or any(output.read_text(encoding='utf-8')!='preserve existing output' for output in outputs):raise AssertionError(f'Identity binding gate {label}: {result.stdout} {result.stderr}')


def main():
    """Compile actual SDKs and independently validate every emitted complete owner scope."""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler',type=Path,required=True);parser.add_argument('--build',type=Path,required=True);parser.add_argument('--language',default='java,csharp,go');parser.add_argument('--wsl-languages',default='');parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args();args.compiler=args.compiler.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if not languages or len(languages)!=len(set(languages)) or any(language not in ('java','csharp','go') for language in languages):parser.error('Select distinct names from java,csharp,go')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:parser.error('In-repository output must be beneath ignored out')
    args.build.mkdir(parents=True,exist_ok=True);xsd=args.build/'identity.xsd';xsd.write_text(XSD,encoding='utf-8');oracle=etree.XMLSchema(etree.parse(str(xsd)));paths=[]
    for label,document,limits,valid,xml_valid in cases():
        try:actual=oracle.validate(etree.fromstring(document.encode()))
        except etree.XMLSyntaxError:actual=False
        # libxml2 2.11 rejects valid unsigned-plus aliases and nil unique fields,
        # and misses the XSD key prohibition on selected nillable declarations.
        # The independent JDK provider checks every unmodified input and emitted output.
        known_provider_case=(label=='aliases' and xml_valid and 'unsignedInt' in str(oracle.error_log)) or (label=='scalar-unique-nil' and xml_valid and 'No precomputed value' in str(oracle.error_log)) or (label in {'scalar-key-nil-present','scalar-key-nil-false','scalar-nullable-keys-present'} and actual and not xml_valid)
        if actual!=xml_valid and not known_provider_case:raise AssertionError(f'Independent identity oracle differs: {label}: {oracle.error_log}')
        path=args.build/f'{label}.{"valid" if valid else "invalid"}.xml';path.write_text(document,encoding='utf-8');paths.append((path,limits))
    provider=args.build/'oracle';provider.mkdir(exist_ok=True);(provider/'Oracle.java').write_text(ORACLE,encoding='utf-8')
    sdk.run(['javac','--release','17','-Xlint:all','-Werror',provider/'Oracle.java'])
    arguments=[]
    for (path,_),(_,_,_,_,xml_valid) in zip(paths,cases()):arguments += [path,str(xml_valid).lower()]
    sdk.run(['java','-cp',provider,'Oracle',xsd,*arguments])
    model=args.build/'model.serializer';model.write_text(MODEL,encoding='utf-8');config=args.build/'identity.ini';config.write_text(OPTIONS,encoding='utf-8')
    command=[args.compiler,'--input',model,'--config',config,'--language',','.join(languages),'--protocols','binary_none','--go.package','main']
    for language in languages:
        directory=args.build/language;directory.mkdir(exist_ok=True);command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args)
    for language,filename,source in [('java','Main.java',JAVA),('csharp','Program.cs',CSHARP),('go','main.go',GO)]:
        if language not in languages:continue
        directory=args.build/'source'/language;directory.mkdir(parents=True,exist_ok=True);(directory/filename).write_text(source,encoding='utf-8')
        for path,_ in paths:Path(str(path)+f'.{language}.xml').unlink(missing_ok=True)
        launch=runner.build(language,'xml_identity',args.build/language,directory);arguments=[]
        for path,limits in paths:arguments += [runner.path(language,path),*limits]
        sdk.run([*launch,*arguments])
        emitted_arguments=[]
        for path,_ in paths:
            emitted=Path(str(path)+f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                actual=oracle.validate(etree.parse(str(emitted)))
                if not actual and not(path.name.startswith('scalar-unique-nil') and 'No precomputed value' in str(oracle.error_log)):raise AssertionError(str(oracle.error_log))
                emitted_arguments += [emitted,'true']
            elif emitted.exists():raise AssertionError(f'Invalid identity output published: {emitted}')
        sdk.run(['java','-cp',provider,'Oracle',xsd,*emitted_arguments])
    rejected_bindings(args,languages)
    print(f'PASS: {len(paths)} independent XSD/admission identity probes +5 atomic binding gates across {len(languages)} actual SDKs',flush=True)


if __name__=='__main__':main()
