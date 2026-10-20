# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify direct Go QName scopes and regenerated declarations with an independent XML oracle."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile

NS = 'urn:serializer:names'
XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:names" xmlns:a="urn:attribute"
 targetNamespace="urn:serializer:names" elementFormDefault="qualified">
<xs:import namespace="urn:attribute" schemaLocation="attributes.xsd"/>
<xs:simpleType name="Names"><xs:list itemType="xs:QName"/></xs:simpleType>
<xs:complexType name="entry"><xs:simpleContent><xs:extension base="xs:QName">
<xs:attribute name="name" type="xs:QName" use="required"/>
</xs:extension></xs:simpleContent></xs:complexType>
<xs:element name="record"><xs:complexType><xs:sequence>
<xs:element name="value" type="xs:QName" minOccurs="0" maxOccurs="unbounded"/>
<xs:element name="lexical" type="t:Names"/>
<xs:element name="child_names" type="t:Names"/>
<xs:element name="optional_names" type="t:Names" minOccurs="0"/>
<xs:element name="child" type="t:entry"/>
</xs:sequence><xs:attribute ref="a:名称" use="required"/>
<xs:attribute name="category" type="xs:QName"/>
<xs:attribute name="required" type="xs:string"/>
<xs:attribute name="names" type="t:Names" use="required"/>
</xs:complexType></xs:element>
<xs:element name="all_record"><xs:complexType><xs:all>
<xs:element name="first" type="xs:QName"/><xs:element name="second" type="t:Names"/>
</xs:all></xs:complexType></xs:element>
<xs:element name="scope_record"><xs:complexType><xs:sequence>
<xs:element name="value" type="xs:QName" minOccurs="0" maxOccurs="unbounded"/>
</xs:sequence></xs:complexType></xs:element>
</xs:schema>'''
MODEL = '''serializer version 1.20.0; namespace names {
xml(namespace("urn:serializer:names")); using schema "names.xsd";
class entry { public string value xml(text,qname); public string name xml(attribute,qname); }
class record {
public string name xml(attribute,qname,namespace("urn:attribute"),name("名称"));
public optional<string> category xml(attribute,qname);
public optional<string> required xml(attribute,prefix_list);
public array[2] string names xml(attribute,list);
public array string values xml(name("value"));
public string lexical;
public array[2] string child_names xml(list);
public optional<array<string>> optional_names xml(list);
public entry child;
}
class all_record { public string first; public array[2] string second xml(list); }
class scope_record { public array string values xml(name("value")); }
}'''
GO = r'''package main
import("bytes";"fmt";"os";"path/filepath";"strings")
// Require transactional XML failure with no published candidate bytes.
func rejected(data []byte,err error){if data!=nil||err==nil{panic("Invalid QName output published")}}
// Exercise public owning values, copied native-authoring bindings and precise namespace lifetime.
func main(){
for _,path:=range os.Args[1:]{data,err:=os.ReadFile(path);if err!=nil{panic(err)};valid:=strings.HasSuffix(path,".valid.xml");var output []byte
if strings.HasPrefix(filepath.Base(path),"scope-"){policy:=DefaultXmlOptions();policy.Limits.MaxTextBytes=5000;value,failure:=DecodeNamesScopeRecordXML(data,policy);if failure==nil||value!=nil||!strings.Contains(failure.Error(),"scope byte"){panic("QName scope amplification admission")};continue}
if strings.HasPrefix(filepath.Base(path),"all-"){value,failure:=DecodeNamesAllRecordXML(data);if !valid{if failure==nil||value!=nil{panic("Invalid all QName candidate")};continue};if failure!=nil{panic(failure)};output,err=value.EncodeXML()
}else{value,failure:=DecodeNamesRecordXML(data);if !valid{if failure==nil||value!=nil{panic("Invalid QName candidate: "+path)};continue};if failure!=nil{panic(failure)}
before,failure:=value.Encode(BINARY_NONE);if failure!=nil{panic(failure)};output,err=value.EncodeXML();after,failure:=value.Encode(BINARY_NONE);if failure!=nil||!bytes.Equal(before,after){panic("QName XML output changed native wire values")}
if err==nil{copy,failure:=DecodeNamesRecordXML(output);if failure!=nil{panic(failure)};if len(copy.Values)!=len(value.Values)||len(copy.Names)!=2{panic("QName collection shape")}}
saved:=value.Name;value.Name="missing:item";rejected(value.EncodeXML());value.Name=saved
value.Name=string([]byte{0xff});rejected(value.EncodeXML());value.Name=saved
original:=value.srlXmlNamespaces;if failure=value.SetXMLNamespaces(map[string]string{"xml":"urn:wrong"});failure==nil{panic("Reserved binding admitted")};if len(original)!=len(value.srlXmlNamespaces){panic("Failed binding setter changed metadata")}
if strings.HasPrefix(filepath.Base(path),"base."){bindings:=map[string]string{"p":"urn:native","":""};if failure=value.SetXMLNamespaces(bindings);failure!=nil{panic(failure)};bindings["p"]="urn:mutated";value.Name="p:native";value.Category=nil;value.Required=nil;value.Names=[]string{"p:one","p:two"};value.Values=[]string{"p:three"};value.Lexical="p:one p:two";value.ChildNames=[]string{"p:one","p:two"};value.OptionalNames=nil;value.Child.Value="xml:lang";value.Child.Name="local";if failure=value.Child.SetXMLNamespaces(map[string]string{"":""});failure!=nil{panic(failure)};native,failure:=value.EncodeXML();if failure!=nil{panic(failure)};if failure=os.WriteFile(path+".native.xml",native,0600);failure!=nil{panic(failure)}}
}
if err!=nil{panic(fmt.Sprintf("%s: %v",path,err))};if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}
}
fmt.Println("Go QName attributes, child-local/shadowed scopes, lists, prefix lists and native binding copies pass")
}'''

JAVA = '''import java.nio.file.*;import java.util.*;
/** Exercise generated owning QName values without optional runtime schema validation. */
public final class Main {
  /** Require complete output rejection before returning bytes. */
  private static void rejected(Runnable work){try{work.run();throw new AssertionError("Invalid QName output published");}catch(IllegalArgumentException expected){}}
  /** Keep native fields unchanged while namespaces are regenerated per scalar occurrence. */
  public static void main(String[] args)throws Exception{for(String path:args){boolean valid=path.endsWith(".valid.xml");byte[] data=Files.readAllBytes(Path.of(path)),output;
    if(Path.of(path).getFileName().toString().startsWith("scope-")){try{Schema.Names.ScopeRecord.decodeXml(data,new Schema.Limits(100000,5000,1000,64),null);throw new AssertionError("QName scope amplification admitted");}catch(IllegalArgumentException failure){if(!failure.getMessage().contains("scope byte"))throw failure;}continue;}
    try{if(Path.of(path).getFileName().toString().startsWith("all-")){var value=Schema.Names.AllRecord.decodeXml(data);output=value.encodeXml();}
    else{var value=Schema.Names.Record.decodeXml(data);byte[] before=value.encode(Schema.Protocol.BINARY_NONE);output=value.encodeXml();if(!Arrays.equals(before,value.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Native wire values changed");Schema.Names.Record.decodeXml(output);
      String saved=value.name;value.name="missing:item";rejected(()->value.encodeXml());value.name=saved;rejected(()->value.setXmlNamespaces(Map.of("xml","urn:wrong")));
      if(Path.of(path).getFileName().toString().startsWith("base.")){var bindings=new HashMap<String,String>();bindings.put("p","urn:native");bindings.put("","");value.setXmlNamespaces(bindings);bindings.put("p","urn:mutated");value.name="p:native";value.category=Optional.empty();value.required=Optional.empty();value.names=new String[]{"p:one","p:two"};value.values=new String[]{"p:three"};value.lexical="p:one p:two";value.childNames=new String[]{"p:one","p:two"};value.optionalNames=Optional.empty();value.child.value="xml:lang";value.child.name="local";value.child.setXmlNamespaces(Map.of("",""));Files.write(Path.of(path+".native.java.xml"),value.encodeXml());}}
    }catch(IllegalArgumentException failure){if(valid)throw new AssertionError(path,failure);continue;}if(!valid)throw new AssertionError("Invalid QName input published: "+path);Files.write(Path.of(path+".java.xml"),output);
  }System.out.println("Java QName scopes, namespace aliases, native binding copies and atomic output pass");}
}'''

CSHARP = '''using System;using System.IO;using System.Collections.Generic;using static SerializerGenerated.Schema;
// Exercise complete generated QName candidates with strict native SDK compilation.
internal static class Program {
  // Invalid values must fail before returning a complete XML candidate.
  private static void Rejected(Action work){try{work();throw new Exception("Invalid QName output published");}catch(FormatException){}}
  // Keep native fields unchanged while namespace declarations are regenerated per occurrence.
  private static void Main(string[] args){foreach(string path in args){bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);byte[] data=File.ReadAllBytes(path),output;
    if(Path.GetFileName(path).StartsWith("scope-",StringComparison.Ordinal)){try{NamesScopeRecord.DecodeXml(data,new Limits(maxStringBytes:5000),null);throw new Exception("QName scope amplification admitted");}catch(FormatException failure){if(!failure.Message.Contains("scope byte",StringComparison.Ordinal))throw;}continue;}
    try{if(Path.GetFileName(path).StartsWith("all-",StringComparison.Ordinal)){var value=NamesAllRecord.DecodeXml(data);output=value.EncodeXml();}
    else{var value=NamesRecord.DecodeXml(data);byte[] before=value.Encode(Protocol.BINARY_NONE);output=value.EncodeXml();if(!before.AsSpan().SequenceEqual(value.Encode(Protocol.BINARY_NONE)))throw new Exception("Native wire values changed");NamesRecord.DecodeXml(output);
      string saved=value.Name;value.Name="missing:item";Rejected(()=>value.EncodeXml());value.Name=saved;Rejected(()=>value.SetXmlNamespaces(new Dictionary<string,string>{{"xml","urn:wrong"}}));
      if(Path.GetFileName(path).StartsWith("base.",StringComparison.Ordinal)){var bindings=new Dictionary<string,string>{{"p","urn:native"},{"",""}};value.SetXmlNamespaces(bindings);bindings["p"]="urn:mutated";value.Name="p:native";value.Category=null;value.Required=null;value.Names=new(){"p:one","p:two"};value.Values=new(){"p:three"};value.Lexical="p:one p:two";value.ChildNames=new(){"p:one","p:two"};value.OptionalNames=null;value.Child.Value="xml:lang";value.Child.Name="local";value.Child.SetXmlNamespaces(new Dictionary<string,string>{{"",""}});File.WriteAllBytes(path+".native.csharp.xml",value.EncodeXml());}}
    }catch(FormatException){if(valid)throw;continue;}if(!valid)throw new Exception("Invalid QName input published: "+path);File.WriteAllBytes(path+".csharp.xml",output);
  }Console.WriteLine("C# QName scopes, namespace aliases, native binding copies and atomic output pass");}
}'''


def expanded(node, text, prefix_only=False):
    """Resolve exact lexical values with lxml's independently retained per-element namespace map."""
    result = []
    for token in text.split():
        prefix, local = (token, '') if prefix_only else token.split(':', 1) if ':' in token else ('', token)
        uri = 'http://www.w3.org/XML/1998/namespace' if prefix == 'xml' else node.nsmap.get(prefix or None, '')
        result.append((uri, local))
    return result


def identities(root):
    """Compare expanded QName and prefix-list identities independently of output alias allocation."""
    result = []
    for node in root.iter():
        for key, value in node.attrib.items():
            if key in ('name', 'category', 'names', 'required', '{urn:attribute}名称'):
                result.append((node.tag, key, expanded(node, value, key == 'required')))
        if node.tag.rsplit('}', 1)[-1] in ('value', 'lexical', 'child_names', 'optional_names', 'child', 'first', 'second'):
            result.append((node.tag, '#text', expanded(node, node.text or '')))
    return result


def qualify(args, etree):
    """Compile the real generated model, execute actual Go SDK checks and compare independent identities."""
    args.build.mkdir(parents=True, exist_ok=True)
    (args.build / 'names.xsd').write_text(XSD, encoding='utf-8')
    (args.build / 'attributes.xsd').write_text(
        '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:attribute">'
        '<xs:attribute name="名称" type="xs:QName"/></xs:schema>', encoding='utf-8')
    model = args.build / 'names.serializer'; model.write_text(MODEL, encoding='utf-8')
    root = Path(__file__).resolve().parent.parent
    spec = importlib.util.spec_from_file_location('serializer_namespace_sdk', root / 'example/run.py')
    sdk = importlib.util.module_from_spec(spec); spec.loader.exec_module(sdk)
    command = [str(args.compiler), '--input', str(model), '--language', ','.join(args.languages),
               '--go.package', 'main', '--protocols', 'binary_none', '--cpp.format', 'false']
    for language in args.languages:
        directory = args.build / language; directory.mkdir()
        command += [f'--{language}.output', str(directory / sdk.OUTPUTS[language])]
    subprocess.run(command, check=True)
    document = ('<n:record xmlns:n="'+NS+'" xmlns:a="urn:attribute" xmlns:p="urn:outer" xmlns:别名="urn:outer" '
                'a:名称="p:名称" category="别名:名称" required="p 别名 xml" names="p:one 别名:two">'
                '<n:value xmlns:p="urn:first">p:名称</n:value>'
                '<n:value xmlns:p="urn:second">p:名称</n:value>'
                '<n:value xmlns="urn:default-value">名称</n:value>'
                '<n:value xmlns="">local</n:value>'
                '<n:lexical xmlns:p="urn:list">p:one xml:lang</n:lexical>'
                '<n:child_names xmlns:p="urn:child-list">p:one p:two</n:child_names>'
                '<n:optional_names xmlns:p="urn:optional-list">p:one p:two</n:optional_names>'
                '<n:child xmlns:p="urn:nested" name="p:attribute">p:text</n:child></n:record>')
    probes = [('base', document, True), ('empty-list', document.replace('p:one xml:lang', ''), True),
              ('empty-optional', document.replace('p:one p:two</n:optional_names>', '</n:optional_names>'), True),
              ('unbound', document.replace('a:名称="p:名称"', 'a:名称="missing:名称"'), False),
              ('bad-local', document.replace('p:名称</n:value>', 'p:1name</n:value>', 1), False),
              ('extra-colon', document.replace('p:名称</n:value>', 'p:other:name</n:value>', 1), False),
              ('empty-qname', document.replace('p:名称</n:value>', '</n:value>', 1), False),
              ('bad-prefix-list', document.replace('required="p 别名 xml"', 'required="p missing"'), False),
              ('prefix-list-empty', document.replace('required="p 别名 xml"', 'required=""'), True),
              ('list-cardinality', document.replace('names="p:one 别名:two"', 'names="p:one"'), False),
              ('markup', document.replace('p:名称</n:value>', '<other/></n:value>', 1), False),
              ('all-reversed', '<n:all_record xmlns:n="'+NS+'"><n:second xmlns:p="urn:two">p:one p:two</n:second><n:first xmlns:p="urn:one">p:first</n:first></n:all_record>', True),
              ('scope-limit', '<n:scope_record xmlns:n="'+NS+'" xmlns:p="urn:'+('x'*256)+'">'+('<n:value>p:item</n:value>'*100)+'</n:scope_record>', False)]
    oracle = etree.XMLSchema(etree.parse(str(args.build / 'names.xsd')))
    paths = []
    for label, text, valid in probes:
        node = etree.fromstring(text.encode())
        # Prefix-list and fixed native cardinality are additional Serializer contracts.
        if label not in ('bad-prefix-list', 'list-cardinality', 'scope-limit') and oracle.validate(node) != valid:
            raise AssertionError(f'Independent XSD differs for {label}: {oracle.error_log}')
        path = args.build / f'{label}.{"valid" if valid else "invalid"}.xml'
        path.write_text(text, encoding='utf-8'); paths.append(path)
    runner = sdk.Runner(args)
    for language, filename, source in [('go', 'main.go', GO), ('java', 'Main.java', JAVA), ('csharp', 'Program.cs', CSHARP)]:
        if language not in args.languages:
            continue
        directory = args.build / 'source' / language; directory.mkdir(parents=True)
        if language == 'go':
            source = source.replace('.native.xml', '.native.go.xml')
        (directory / filename).write_text(source, encoding='utf-8')
        launch = runner.build(language, 'xml_namespace', args.build / language, directory)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for (_, text, valid), path in zip(probes, paths):
            emitted = Path(str(path)+f'.{language}.xml')
            if not valid:
                if emitted.exists():
                    raise AssertionError(f'Invalid candidate published: {path}')
                continue
            output = etree.parse(str(emitted)).getroot()
            if not oracle.validate(output):
                raise AssertionError(str(oracle.error_log))
            expected, actual = identities(etree.fromstring(text.encode())), identities(output)
            if path.name.startswith('all-'):
                expected.sort(); actual.sort()
            if expected != actual:
                raise AssertionError(f'Expanded QName identity changed: {path}')
        native = etree.parse(str(paths[0])+f'.native.{language}.xml').getroot()
        if expanded(native, native.get('{urn:attribute}名称')) != [('urn:native', 'native')]:
            raise AssertionError('Native binding copy or QName declaration regeneration changed identity')
    print(f'PASS: {len(probes)} namespace probes across {len(args.languages)} SDKs with independent XSD/expanded identities')


def main():
    """Resolve optional provider dependencies before running in an automatically cleaned scratch directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--python-dependencies', type=Path)
    parser.add_argument('--language', default='go')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    args.languages = args.language.split(',')
    if len(set(args.languages)) != len(args.languages) or any(value not in ('go', 'java', 'csharp') for value in args.languages):
        parser.error('Choose distinct languages from go,java,csharp')
    if args.build == root or root in args.build.parents and 'out' not in args.build.relative_to(root).parts:
        parser.error('In-repository output must be beneath ignored out/')
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    from lxml import etree
    args.build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='go-xml-namespace-', dir=args.build) as temporary:
        args.build = Path(temporary)
        qualify(args, etree)


if __name__ == '__main__':
    main()
