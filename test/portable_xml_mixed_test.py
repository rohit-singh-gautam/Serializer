# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify managed/Go ordered native mixed fragments, escaping and exact resource admission."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

from lxml import etree

from javascript_xml_mixed_test import DOCUMENT, SCHEMA, CONFIG


ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_mixed_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec); spec.loader.exec_module(sdk)
MODEL = SCHEMA.replace('class fragment', 'class link { public string href xml(attribute); public string text xml(text); } class fragment')
MODEL = MODEL.replace('variant(string = text, emphasis = emphasis)', 'variant(string = text, emphasis = emphasis, link = link)')
MODEL = MODEL.rsplit('}', 1)[0] + 'class holder { public paragraph value; }}'
OPTIONS = CONFIG + 'mixed.child.link={urn:format}a\n[xml.class.js_mixed::holder]\nnamespace=urn:paragraph\n'
XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:p="urn:paragraph" xmlns:f="urn:format" targetNamespace="urn:paragraph" elementFormDefault="qualified">
<xs:import namespace="urn:format" schemaLocation="format.xsd"/>
<xs:complexType name="paragraphType" mixed="true"><xs:choice minOccurs="0" maxOccurs="unbounded"><xs:element ref="f:em"/><xs:element ref="f:a"/></xs:choice><xs:attribute name="id" type="xs:string" use="required"/></xs:complexType>
<xs:element name="paragraph" type="p:paragraphType"/>
<xs:element name="holder"><xs:complexType><xs:sequence><xs:element name="value" type="p:paragraphType"/></xs:sequence></xs:complexType></xs:element>
</xs:schema>'''
FORMAT_XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:format"><xs:element name="em" type="xs:string"/><xs:element name="a"><xs:complexType><xs:simpleContent><xs:extension base="xs:string"><xs:attribute name="href" type="xs:string" use="required"/></xs:extension></xs:simpleContent></xs:complexType></xs:element></xs:schema>'''


def cases():
    """Separate logical XML validity from explicit per-sequence admission policies."""
    start = '<paragraph xmlns="urn:paragraph" id="p">'
    ending = '</paragraph>'
    probes = [
        ('document', DOCUMENT, 5, 16, True),
        ('fragments', DOCUMENT, 4, 16, False), ('bytes', DOCUMENT, 5, 15, False),
        ('empty', start + ending, 0, 0, True),
        ('joined', start + 'a<![CDATA[b]]><!--ignored-->c<?ignored x?>d' + ending, 1, 4, True),
        ('whitespace', start + ' \t&#13;\n' + ending, 1, 4, True),
        ('entities', start + '&lt;&amp;&gt;é😀' + ending, 1, 9, True),
        ('children', start + '<em xmlns="urn:format">one</em><a xmlns="urn:format" href="a&amp;b">two</a><em xmlns="urn:format"/>tail' + ending, 4, 4, True),
        ('child-limit', start + '<em xmlns="urn:format"/>' + ending, 0, 0, False),
        ('text-limit', start + 'a' + ending, 1, 0, False),
        ('unknown', DOCUMENT.replace('f:em', 'f:unknown'), 20, 100, False),
        ('namespace', DOCUMENT.replace('urn:format', 'urn:unknown'), 20, 100, False),
        ('attribute', DOCUMENT.replace('id="p"', 'id="p" extra="1"'), 20, 100, False),
        ('missing-attribute', DOCUMENT.replace(' id="p"', ''), 20, 100, False),
        ('child-attribute', DOCUMENT.replace('f:em xmlns:f=', 'f:em extra="1" xmlns:f='), 20, 100, False),
        ('child-content', DOCUMENT.replace('middle', '<other/>'), 20, 100, False),
        ('link-attribute', start + '<a xmlns="urn:format">missing</a>' + ending, 20, 100, False),
    ]
    large = 'é😀' * 4096
    probes += [('large', start + large + ending, 1, len(large.encode()), True),
               ('large-limit', start + large + ending, 1, len(large.encode()) - 1, False)]
    nested = DOCUMENT.replace('<paragraph ', '<value ', 1).replace('</paragraph>', '</value>')
    probes += [('holder', '<holder xmlns="urn:paragraph">' + nested + '</holder>', 5, 16, True),
               ('holder-limit', '<holder xmlns="urn:paragraph">' + nested + '</holder>', 4, 16, False)]
    return probes


def semantic(node):
    """Observe independent expanded names and every text/tail position, including significant whitespace."""
    return (node.tag, sorted(node.attrib.items()), node.text or '', [(semantic(child), child.tail or '') for child in node])


JAVA = '''import java.nio.file.Files;
import java.nio.file.Path;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.Arrays;
/** Inspect original private native storage without depending on generated XML bridge names. */
public final class Main {
  /** Borrow one explicit native field for independent typed payload checks. */
  private static Field field(String name)throws Exception{Field value=Schema.JsMixed.Fragment.class.getDeclaredField(name);if(!Modifier.isPrivate(value.getModifiers()))throw new AssertionError("Native visibility changed");value.setAccessible(true);return value;}
  /** Compare native binary identity, logical text order and exact mixed limits. */
  public static void main(String[] args)throws Exception{
    Field index=field("valueIndex"),text=field("valueText");
    try{new Schema.XmlMixedLimits(-1,0);throw new AssertionError("Negative mixed policy");}catch(IllegalArgumentException expected){}
    for(int argument=0;argument<args.length;argument+=3){String path=args[argument];boolean valid=path.endsWith(".valid.xml");byte[] data=Files.readAllBytes(Path.of(path)),output;var limits=new Schema.XmlMixedLimits(Integer.parseInt(args[argument+1]),Integer.parseInt(args[argument+2]));
      try{
        if(Path.of(path).getFileName().toString().startsWith("holder")){
          var holder=Schema.JsMixed.Holder.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),limits);output=holder.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),limits);
        }else{
          var value=Schema.JsMixed.Paragraph.decodeXml(data,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),limits);output=value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),limits);var copy=Schema.JsMixed.Paragraph.decodeXml(output);
          if(!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE),copy.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Mixed native wire changed");
          if(Path.of(path).getFileName().toString().startsWith("document")){
            if(value.content.size()!=5||!text.get(value.content.get(0)).equals("head&😀"))throw new AssertionError("Ordered text payload");
            int[] tags={0,1,0,1,0};for(int item=0;item<tags.length;++item)if(index.getInt(value.content.get(item))!=tags[item])throw new AssertionError("Ordered child payload");
            var blank=new Schema.JsMixed.Fragment();text.set(blank,"");var adjacent=new Schema.JsMixed.Fragment();text.set(adjacent,"adjacent");value.content.add(1,blank);value.content.add(2,adjacent);
            try{value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(6,100));throw new AssertionError("Empty authored fragments uncharged");}catch(IllegalArgumentException expected){}
            if(!new String(value.encodeXml(),java.nio.charset.StandardCharsets.UTF_8).contains("head&amp;😀adjacent<"))throw new AssertionError("Authored adjacent text order");
          }
          var invalid=new Schema.JsMixed.Fragment();index.setInt(invalid,999);value.content.add(invalid);
          try{value.encodeXml();throw new AssertionError("Unknown native mixed index published");}catch(IllegalArgumentException expected){}
        }
      }catch(IllegalArgumentException failure){if(valid)throw new AssertionError(path,failure);continue;}
      if(!valid)throw new AssertionError("Invalid mixed document published: "+path);Files.write(Path.of(path+".java.xml"),output);
    }System.out.println("Java ordered private native mixed variants, logical runs, nested limits and escaping pass");
  }
}'''

CSHARP = '''using System;
using System.IO;
using System.Reflection;
using static SerializerGenerated.Schema;
// Inspect original private native storage independently of XML bridge property names.
internal static class Program {
  // Borrow native storage only after preserving its declared visibility.
  private static FieldInfo Field(string name){var result=typeof(JsMixedFragment).GetField(name,BindingFlags.Instance|BindingFlags.NonPublic)??throw new Exception("Private native field missing");if(!result.IsPrivate)throw new Exception("Native visibility changed");return result;}
  // Compare native wire values, order and exact limits before publishing successful XML.
  private static void Main(string[] args){var index=Field("ValueIndex");var text=Field("ValueText");
    try{new XmlMixedLimits(-1,0);throw new Exception("Negative mixed policy");}catch(ArgumentOutOfRangeException){}
    for(int argument=0;argument<args.Length;argument+=3){string path=args[argument];bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);byte[] data=File.ReadAllBytes(path),output;var limits=new XmlMixedLimits(int.Parse(args[argument+1],System.Globalization.CultureInfo.InvariantCulture),int.Parse(args[argument+2],System.Globalization.CultureInfo.InvariantCulture));
      try{
        if(Path.GetFileName(path).StartsWith("holder",StringComparison.Ordinal)){
          var holder=JsMixedHolder.DecodeXml(data,new Limits(),null,new XmlMapLimits(),limits);output=holder.EncodeXml(new Limits(),null,new XmlMapLimits(),limits);
        }else{
          var value=JsMixedParagraph.DecodeXml(data,new Limits(),null,new XmlMapLimits(),limits);output=value.EncodeXml(new Limits(),null,new XmlMapLimits(),limits);var copy=JsMixedParagraph.DecodeXml(output);
          if(!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(copy.Encode(Protocol.BINARY_NONE)))throw new Exception("Mixed native wire changed");
          if(Path.GetFileName(path).StartsWith("document",StringComparison.Ordinal)){
            if(value.Content.Count!=5||!Equals(text.GetValue(value.Content[0]),"head&😀"))throw new Exception("Ordered text payload");
            int[] tags={0,1,0,1,0};for(int item=0;item<tags.Length;++item)if(!Equals(index.GetValue(value.Content[item]),tags[item]))throw new Exception("Ordered child payload");
            var blank=new JsMixedFragment();text.SetValue(blank,"");var adjacent=new JsMixedFragment();text.SetValue(adjacent,"adjacent");value.Content.Insert(1,blank);value.Content.Insert(2,adjacent);
            try{value.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(6,100));throw new Exception("Empty authored fragments uncharged");}catch(FormatException){}
            if(!System.Text.Encoding.UTF8.GetString(value.EncodeXml()).Contains("head&amp;😀adjacent<",StringComparison.Ordinal))throw new Exception("Authored adjacent text order");
          }
          var invalid=new JsMixedFragment();index.SetValue(invalid,999);value.Content.Add(invalid);
          try{value.EncodeXml();throw new Exception("Unknown native mixed index published");}catch(FormatException){}
        }
      }catch(FormatException failure){if(valid)throw new Exception(path,failure);continue;}
      if(!valid)throw new Exception("Invalid mixed document published: "+path);File.WriteAllBytes(path+".csharp.xml",output);
    }Console.WriteLine("C# ordered private native mixed variants, logical runs, nested limits and escaping pass");
  }
}'''

GO = r'''package main
import("bytes";"fmt";"os";"path/filepath";"strconv";"strings")
// Require operation failure without publishing candidate XML bytes.
func rejected(data []byte,err error){if data!=nil||err==nil{panic("Invalid mixed output published")}}
// Exercise private native variants and complete mixed limit policies.
func probe(path string,limits XmlMixedLimits)([]byte,error){data,err:=os.ReadFile(path);if err!=nil{panic(err)}
  if strings.HasPrefix(filepath.Base(path),"holder"){value,err:=DecodeJsMixedHolderXMLWithMixedLimits(data,limits);if err!=nil{return nil,err};return value.EncodeXMLWithMixedLimits(limits)}
  value,err:=DecodeJsMixedParagraphXMLWithMixedLimits(data,limits);if err!=nil{return nil,err};output,err:=value.EncodeXMLWithMixedLimits(limits);if err!=nil{return nil,err};copy,err:=DecodeJsMixedParagraphXML(output);if err!=nil{return nil,err};first,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};second,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(first,second){panic("Mixed native wire changed")}
  if strings.HasPrefix(filepath.Base(path),"document"){
    if len(value.Content)!=5||value.Content[0].valueText!="head&😀"{panic("Ordered text payload")};for index,tag:=range []int{0,1,0,1,0}{if value.Content[index].valueIndex!=tag{panic("Ordered child payload")}}
    blank,adjacent:=NewJsMixedFragment(),NewJsMixedFragment();adjacent.valueText="adjacent";value.Content=append(value.Content[:1],append([]*JsMixedFragment{blank,adjacent},value.Content[1:]...)...)
    rejected(value.EncodeXMLWithMixedLimits(XmlMixedLimits{6,100}));authored,err:=value.EncodeXML();if err!=nil{panic(err)};if !bytes.Contains(authored,[]byte("head&amp;😀adjacent<")){panic("Authored adjacent text order")}
  }
  invalid:=NewJsMixedFragment();invalid.valueIndex=999;value.Content=append(value.Content,invalid);rejected(value.EncodeXML());return output,nil
}
// Match independent validity/admission fixtures and preserve staged failure behavior.
func main(){rejected(NewJsMixedParagraph().EncodeXMLWithMixedLimits(XmlMixedLimits{-1,0}));for argument:=1;argument<len(os.Args);argument+=3{path:=os.Args[argument];fragments,err:=strconv.Atoi(os.Args[argument+1]);if err!=nil{panic(err)};text,err:=strconv.Atoi(os.Args[argument+2]);if err!=nil{panic(err)};valid:=strings.HasSuffix(path,".valid.xml");output,err:=probe(path,XmlMixedLimits{fragments,text});if valid!=(err==nil)||!valid&&output!=nil{panic(fmt.Sprintf("Mixed oracle differs: %s: %v",path,err))};if err==nil{if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}}};fmt.Println("Go ordered private native mixed variants, logical runs, nested limits and escaping pass")}
'''


def rejected_bindings(args, languages):
    """Reject implicit helper use outside its owning mixed sequence before replacing any output."""
    directory = args.build / 'rejections'; directory.mkdir(parents=True, exist_ok=True)
    for label, field, mapping in [('ordinary', 'fragment', ''), ('map', 'map<string,fragment>', 'map.payload=entry\nwrapper.payload=payload\n'),
                                  ('variant', 'variant(string=text,fragment=helper)', 'variant.payload=element\nwrapper.payload=payload\n')]:
        model = directory / (label + '.serializer'); config = directory / (label + '.ini')
        model.write_text(MODEL.rsplit('}', 1)[0] + 'class misuse { public ' + field + ' payload; }}', encoding='utf-8')
        config.write_text(OPTIONS + '[xml.class.js_mixed::misuse]\n' + mapping, encoding='utf-8')
        command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages)]
        outputs = []
        for language in languages:
            output = directory / sdk.OUTPUTS[language]; output.write_text('preserve existing output', encoding='utf-8'); outputs.append(output)
            command += [f'--{language}.output', output]
        result = subprocess.run([str(value) for value in command], capture_output=True, encoding='utf-8', timeout=60)
        if result.returncode == 0 or 'mixed fragment storage' not in result.stdout + result.stderr or any(path.read_text(encoding='utf-8') != 'preserve existing output' for path in outputs):
            raise AssertionError(f'Mixed helper misuse {label}: {result.stdout} {result.stderr}')


def main():
    """Compile actual SDKs and validate emitted mixed text/tails independently of generated readers."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True); parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp,go'); parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler = args.compiler.resolve(); args.build = args.build.resolve(); languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(language not in ('java', 'csharp', 'go') for language in languages):
        parser.error('Select distinct language names from java,csharp,go')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    (args.build / 'format.xsd').write_text(FORMAT_XSD, encoding='utf-8'); schema = args.build / 'mixed.xsd'; schema.write_text(XSD, encoding='utf-8')
    oracle = etree.XMLSchema(etree.parse(str(schema))); paths = []; controls = {'fragments', 'bytes', 'child-limit', 'text-limit', 'large-limit', 'holder-limit'}
    for label, document, fragments, text_bytes, valid in cases():
        if oracle.validate(etree.fromstring(document.encode())) != (valid or label in controls):
            raise AssertionError(f'Independent mixed oracle differs: {label}: {oracle.error_log}')
        path = args.build / f'{label}.{"valid" if valid else "invalid"}.xml'; path.write_text(document, encoding='utf-8'); paths.append((path, fragments, text_bytes))
    model = args.build / 'model.serializer'; model.write_text(MODEL, encoding='utf-8'); config = args.build / 'mixed.ini'; config.write_text(OPTIONS, encoding='utf-8')
    command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages), '--protocols', 'binary_none', '--go.package', 'main']
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True); command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language, filename, source in [('java', 'Main.java', JAVA), ('csharp', 'Program.cs', CSHARP), ('go', 'main.go', GO)]:
        if language not in languages:
            continue
        directory = args.build / 'source' / language; directory.mkdir(parents=True, exist_ok=True); (directory / filename).write_text(source, encoding='utf-8')
        for path, _, _ in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        launch = runner.build(language, 'xml_mixed', args.build / language, directory); arguments = []
        for path, fragments, text_bytes in paths:
            arguments += [runner.path(language, path), fragments, text_bytes]
        sdk.run([*launch, *arguments])
        for path, _, _ in paths:
            emitted = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(emitted)))
                original = etree.parse(str(path), etree.XMLParser(remove_comments=True, remove_pis=True)).getroot()
                if semantic(original) != semantic(etree.parse(str(emitted)).getroot()):
                    raise AssertionError(f'Ordered mixed value changed: {emitted}')
            elif emitted.exists():
                raise AssertionError(f'Invalid mixed output published: {emitted}')
    rejected_bindings(args, languages)
    print(f'PASS: {len(paths)} independent mixed probes, logical runs, resource bounds and 3 atomic helper gates across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
