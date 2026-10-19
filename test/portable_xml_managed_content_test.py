"""Qualify Java/C#/Go singleton choices and authoritative XSD content using the shared corpus."""
import argparse
from pathlib import Path

from lxml import etree

from portable_xml_content_test import MODEL, OPTIONS, XSD, cases, semantic, sdk


ROOT = Path(__file__).resolve().parent.parent
JAVA = '''import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Optional;
/** Check native optional alternatives and authoritative complete typed content. */
public final class Main {
  /** Preserve native wire values and reject invalid edited choices before publication. */
  public static void main(String[] args)throws Exception{for(String path:args){boolean valid=path.endsWith(".valid.xml");String file=Path.of(path).getFileName().toString();byte[] data=Files.readAllBytes(Path.of(path)),output;
    try{
      if(file.startsWith("optional")){
        var value=Schema.Content.OptionalBatch.decodeXml(data);output=value.encodeXml();
        value.left=Optional.of(1);value.right=Optional.of("two");
        try{value.encodeXml();throw new AssertionError("Multiple optional alternatives published");}catch(IllegalArgumentException expected){}
      }else if(file.startsWith("price")){
        var value=Schema.Content.Price.decodeXml(data);output=value.encodeXml();
        var copy=Schema.Content.Price.decodeXml(output);
        if(value.amount!=copy.amount||!value.currency.equals(copy.currency))throw new AssertionError("Exact simpleContent value");
      }else if(file.startsWith("wrappers")){
        var value=Schema.Content.Wrappers.decodeXml(data);output=value.encodeXml();value.values=Arrays.copyOf(value.values,value.values.length+1);
        try{value.encodeXml();throw new AssertionError("Null wrapped record published");}catch(IllegalArgumentException expected){}
      }else{
        var value=Schema.Content.Sample.decodeXml(data);output=value.encodeXml();var copy=Schema.Content.Sample.decodeXml(output);
        if(!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE),copy.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Native choice storage changed");
        value.left=Optional.empty();value.right=Optional.empty();value.nested=Optional.empty();
        try{value.encodeXml();throw new AssertionError("Required choice absent");}catch(IllegalArgumentException expected){}
        value.left=Optional.of(1);value.right=Optional.of("two");
        try{value.encodeXml();throw new AssertionError("Multiple alternatives published");}catch(IllegalArgumentException expected){}
      }
    }catch(IllegalArgumentException failure){if(valid)throw new AssertionError(path,failure);continue;}
    if(!valid)throw new AssertionError("Invalid XML content published: "+path);Files.write(Path.of(path+".java.xml"),output);
  }System.out.println("Java singleton choices, native storage, authoritative simpleContent and record wrappers pass");}
}'''

CSHARP = '''using System;
using System.IO;
using static SerializerGenerated.Schema;
// Check native optional alternatives and authoritative complete typed content.
internal static class Program {
  // Preserve native values and reject invalid edited choices before publication.
  private static void Main(string[] args){foreach(string path in args){bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);string file=Path.GetFileName(path);byte[] data=File.ReadAllBytes(path),output;
    try{
      if(file.StartsWith("optional",StringComparison.Ordinal)){
        var value=ContentOptionalBatch.DecodeXml(data);output=value.EncodeXml();value.Left=new ValueTuple<int>(1);value.Right=new ValueTuple<string>("two");
        try{value.EncodeXml();throw new Exception("Multiple optional alternatives published");}catch(FormatException){}
      }else if(file.StartsWith("price",StringComparison.Ordinal)){
        var value=ContentPrice.DecodeXml(data);output=value.EncodeXml();var copy=ContentPrice.DecodeXml(output);
        if(value.Amount!=copy.Amount||value.Currency!=copy.Currency)throw new Exception("Exact simpleContent value");
      }else if(file.StartsWith("wrappers",StringComparison.Ordinal)){
        var value=ContentWrappers.DecodeXml(data);output=value.EncodeXml();value.Values.Add(null!);
        try{value.EncodeXml();throw new Exception("Null wrapped record published");}catch(FormatException){}
      }else{
        var value=ContentSample.DecodeXml(data);output=value.EncodeXml();var copy=ContentSample.DecodeXml(output);
        if(!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(copy.Encode(Protocol.BINARY_NONE)))throw new Exception("Native choice storage changed");
        value.Left=null;value.Right=null;value.Nested=null;
        try{value.EncodeXml();throw new Exception("Required choice absent");}catch(FormatException){}
        value.Left=new ValueTuple<int>(1);value.Right=new ValueTuple<string>("two");
        try{value.EncodeXml();throw new Exception("Multiple alternatives published");}catch(FormatException){}
        value.Left=null;value.Right=null;value.Nested=new ValueTuple<ContentChild>(null!);
        try{value.EncodeXml();throw new Exception("Null chosen record published");}catch(FormatException){}
      }
    }catch(FormatException failure){if(valid)throw new Exception(path,failure);continue;}
    if(!valid)throw new Exception("Invalid XML content published: "+path);File.WriteAllBytes(path+".csharp.xml",output);
  }Console.WriteLine("C# singleton choices, native storage, authoritative simpleContent and record wrappers pass");}
}'''

GO = r'''package main
import("bytes";"fmt";"os";"path/filepath";"strings")
// Require staged output errors to return no candidate bytes.
func rejected(data []byte,err error){if data!=nil||err==nil{panic("Invalid edited XML content published")}}
// Exercise generated native optional and text/wrapper storage for one complete document.
func probe(path string)([]byte,error){data,err:=os.ReadFile(path);if err!=nil{panic(err)};file:=filepath.Base(path)
  switch{
  case strings.HasPrefix(file,"optional"):
    value,err:=DecodeContentOptionalBatchXML(data);if err!=nil{return nil,err};output,err:=value.EncodeXML();if err!=nil{return nil,err};left,right:=int32(1),"two";value.Left=&left;value.Right=&right;rejected(value.EncodeXML());return output,nil
  case strings.HasPrefix(file,"price"):
    value,err:=DecodeContentPriceXML(data);if err!=nil{return nil,err};output,err:=value.EncodeXML();if err!=nil{return nil,err};copy,err:=DecodeContentPriceXML(output);if err!=nil{return nil,err};if value.Amount!=copy.Amount||value.Currency!=copy.Currency{panic("Exact simpleContent value")};return output,nil
  case strings.HasPrefix(file,"wrappers"):
    value,err:=DecodeContentWrappersXML(data);if err!=nil{return nil,err};output,err:=value.EncodeXML();if err!=nil{return nil,err};value.Values=append(value.Values,nil);rejected(value.EncodeXML());return output,nil
  }
  value,err:=DecodeContentSampleXML(data);if err!=nil{return nil,err};output,err:=value.EncodeXML();if err!=nil{return nil,err};copy,err:=DecodeContentSampleXML(output);if err!=nil{return nil,err};first,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};second,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(first,second){panic("Native choice storage changed")}
  value.Left=nil;value.Right=nil;value.Nested=nil;rejected(value.EncodeXML());left,right:=int32(1),"two";value.Left=&left;value.Right=&right;rejected(value.EncodeXML());value.Left=nil;value.Right=nil;var child *ContentChild;value.Nested=&child;rejected(value.EncodeXML());return output,nil
}
// Compare native outcomes with the independent fixtures before writing successful output.
func main(){for _,path:=range os.Args[1:]{valid:=strings.HasSuffix(path,".valid.xml");output,err:=probe(path);if valid!=(err==nil)||!valid&&output!=nil{panic(fmt.Sprintf("Content oracle differs: %s: %v",path,err))};if err==nil{if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}}};fmt.Println("Go singleton choices, native storage, authoritative simpleContent and record wrappers pass")}
'''


def main():
    """Share independent native corpus decisions and compare every emitted typed value."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp,go')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler = args.compiler.resolve(); args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(language not in ('java', 'csharp', 'go') for language in languages):
        parser.error('Select distinct language names from java,csharp,go')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    schema = args.build / 'content.xsd'; schema.write_text(XSD, encoding='utf-8')
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode())); paths = []
    for index, (kind, document, valid) in enumerate(cases()):
        if oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent oracle differs: {index}: {oracle.error_log}')
        path = args.build / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'; path.write_text(document, encoding='utf-8'); paths.append(path)
    model = args.build / 'model.serializer'; model.write_text(MODEL, encoding='utf-8')
    config = args.build / 'content.ini'; config.write_text(OPTIONS, encoding='utf-8')
    command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages), '--protocols', 'binary_none', '--go.package', 'main']
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language, filename, source in [('java', 'Main.java', JAVA), ('csharp', 'Program.cs', CSHARP), ('go', 'main.go', GO)]:
        if language not in languages:
            continue
        directory = args.build / 'source' / language; directory.mkdir(parents=True, exist_ok=True)
        (directory / filename).write_text(source, encoding='utf-8')
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        launch = runner.build(language, 'xml_content', args.build / language, directory)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            emitted = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(emitted)))
                if semantic(etree.parse(str(path)).getroot()) != semantic(etree.parse(str(emitted)).getroot()):
                    raise AssertionError(f'Typed content changed: {emitted}')
            elif emitted.exists():
                raise AssertionError(f'Invalid content output published: {emitted}')
    print(f'PASS: {len(paths)} independent choice/simpleContent/wrapper probes across {len(languages)} managed/Go SDKs', flush=True)


if __name__ == '__main__':
    main()
