# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify managed/Go approved HTML fragments against independent ordinary HTML token/tree providers."""
import argparse
import importlib.util
from pathlib import Path
import sys

from html_interop_test import trace, tree_trace
from javascript_html_test import FIXTURES
from mobile_html_test import INVALID

ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('serializer_portable_html_sdk',ROOT/'example/run.py')
sdk=importlib.util.module_from_spec(spec);spec.loader.exec_module(sdk)

JAVA=r'''import java.nio.file.*;import java.nio.charset.StandardCharsets;import java.util.*;
/** Qualify ordinary HTML without relying on any XML parser or XSD provider. */
public final class Main {
  /** Require invalid subset/resources to reject before returning a completed candidate. */
  private static void rejected(Runnable action){try{action.run();throw new AssertionError("Invalid HTML operation accepted");}catch(IllegalArgumentException expected){}}
  /** Exercise independent fixtures, authored traces, Unicode and exact admission. */
  public static void main(String[] args)throws Exception{
    for(String path:args){byte[] input=Files.readAllBytes(Path.of(path));boolean valid=path.endsWith(".valid.html");List<Schema.HtmlEvent> events;try{events=Schema.Html.parseFragment(input);}catch(IllegalArgumentException failure){if(valid)throw failure;continue;}if(!valid)throw new AssertionError("Invalid HTML candidate published: "+path);byte[] output=Schema.Html.writeFragment(events);if(!Arrays.equals(output,Schema.Html.writeFragment(Schema.Html.parseFragment(output))))throw new AssertionError("HTML canonical replay");Files.write(Path.of(path+".java.html"),output);}
    if(!Schema.Html.supportedSubset().contains("no doctype"))throw new AssertionError("HTML subset description");
    byte[] input="<DIV hidden title=\"x\">é&amp;😀<br></DIV>".getBytes(StandardCharsets.UTF_8);var events=Schema.Html.parseFragment(input);byte[] output=Schema.Html.writeFragment(events);int count=events.size();var exact=new Schema.HtmlLimits(input.length,output.length,count,2,2,7);
    if(!Arrays.equals(output,Schema.Html.writeFragment(Schema.Html.parseFragment(input,exact),exact)))throw new AssertionError("Exact HTML bounds");
    for(var shortPolicy:List.of(new Schema.HtmlLimits(input.length,output.length,count-1,2,2,7),new Schema.HtmlLimits(input.length,output.length,count,1,2,7),new Schema.HtmlLimits(input.length,output.length,count,2,1,7),new Schema.HtmlLimits(input.length,output.length,count,2,2,6))){rejected(()->Schema.Html.parseFragment(input,shortPolicy));rejected(()->Schema.Html.writeFragment(events,shortPolicy));}
    rejected(()->Schema.Html.parseFragment(input,new Schema.HtmlLimits(input.length-1,output.length,count,2,2,7)));rejected(()->Schema.Html.writeFragment(events,new Schema.HtmlLimits(input.length,output.length-1,count,2,2,7)));rejected(()->new Schema.HtmlLimits(0,0,0,-1,0,0));rejected(()->Schema.Html.parseFragment(new byte[]{(byte)0xc3,0x28}));
    var authored=List.of(new Schema.HtmlEvent("start_fragment"),new Schema.HtmlEvent("start_element","span",List.of(new Schema.HtmlAttribute("hidden","",true)),""),new Schema.HtmlEvent("text","",List.of(),"<&\r\""),new Schema.HtmlEvent("end_element","span"),new Schema.HtmlEvent("end_fragment"));if(!new String(Schema.Html.writeFragment(authored),StandardCharsets.UTF_8).equals("<span hidden>&lt;&amp;&#13;\"</span>"))throw new AssertionError("Authored HTML escaping");
    var malformed=new ArrayList<>(events);for(int index=0;index<malformed.size();++index)if(malformed.get(index).kind.equals("text"))malformed.set(index,new Schema.HtmlEvent("text","",List.of(),"\uD800"));rejected(()->Schema.Html.writeFragment(malformed));rejected(()->Schema.Html.writeFragment(List.of(new Schema.HtmlEvent("start_fragment"),new Schema.HtmlEvent("end_fragment")),new Schema.HtmlLimits(0,0,1,0,0,0)));
    var zero=new Schema.HtmlLimits(0,0,2,0,0,0);if(Schema.Html.writeFragment(Schema.Html.parseFragment(new byte[0],zero),zero).length!=0)throw new AssertionError("Empty zero-limit HTML");
    System.out.println("Java HTML traces, approved syntax, fatal Unicode, authored events and exact policies pass");
  }
}'''

CSHARP=r'''using System;using System.IO;using System.Text;using System.Linq;using static SerializerGenerated.Schema;
// Keep ordinary HTML behavior independent of XML parsers and XSD providers.
internal static class Program {
  // Require invalid subset/resources to reject before returning a complete candidate.
  private static void Rejected(Action action){try{action();throw new Exception("Invalid HTML operation accepted");}catch(FormatException){}}
  // Exercise actual fixtures, authored event storage, Unicode and exact resource admission.
  private static void Main(string[] args){foreach(string path in args){byte[] input=File.ReadAllBytes(path);bool valid=path.EndsWith(".valid.html",StringComparison.Ordinal);System.Collections.Generic.IReadOnlyList<HtmlEvent> events;try{events=Html.ParseFragment(input);}catch(FormatException){if(valid)throw;continue;}if(!valid)throw new Exception("Invalid HTML candidate published: "+path);byte[] output=Html.WriteFragment(events);if(!output.AsSpan().SequenceEqual(Html.WriteFragment(Html.ParseFragment(output))))throw new Exception("HTML canonical replay");File.WriteAllBytes(path+".csharp.html",output);}
    if(!Html.SupportedSubset().Contains("no doctype",StringComparison.Ordinal))throw new Exception("HTML subset description");
    byte[] source=Encoding.UTF8.GetBytes("<DIV hidden title=\"x\">é&amp;😀<br></DIV>");var trace=Html.ParseFragment(source);byte[] result=Html.WriteFragment(trace);int count=trace.Count;var exact=new HtmlLimits(source.Length,result.Length,count,2,2,7);if(!result.AsSpan().SequenceEqual(Html.WriteFragment(Html.ParseFragment(source,exact),exact)))throw new Exception("Exact HTML bounds");
    foreach(var policy in new[]{new HtmlLimits(source.Length,result.Length,count-1,2,2,7),new HtmlLimits(source.Length,result.Length,count,1,2,7),new HtmlLimits(source.Length,result.Length,count,2,1,7),new HtmlLimits(source.Length,result.Length,count,2,2,6)}){Rejected(()=>Html.ParseFragment(source,policy));Rejected(()=>Html.WriteFragment(trace,policy));}
    Rejected(()=>Html.ParseFragment(source,new HtmlLimits(source.Length-1,result.Length,count,2,2,7)));Rejected(()=>Html.WriteFragment(trace,new HtmlLimits(source.Length,result.Length-1,count,2,2,7)));try{new HtmlLimits(0,0,0,-1,0,0);throw new Exception("Negative HTML policy");}catch(ArgumentOutOfRangeException){}Rejected(()=>Html.ParseFragment(new byte[]{0xc3,0x28}));
    var authored=new[]{new HtmlEvent("start_fragment"),new HtmlEvent("start_element","span",new[]{new HtmlAttribute("hidden",boolean:true)}),new HtmlEvent("text",text:"<&\r\""),new HtmlEvent("end_element","span"),new HtmlEvent("end_fragment")};if(Encoding.UTF8.GetString(Html.WriteFragment(authored))!="<span hidden>&lt;&amp;&#13;\"</span>")throw new Exception("Authored HTML escaping");Rejected(()=>Html.WriteFragment(trace.Select(entry=>entry.Kind=="text"?new HtmlEvent("text",text:"\uD800"):entry).ToArray()));Rejected(()=>Html.WriteFragment(new[]{new HtmlEvent("start_fragment"),new HtmlEvent("end_fragment")},new HtmlLimits(0,0,1,0,0,0)));
    var zero=new HtmlLimits(0,0,2,0,0,0);if(Html.WriteFragment(Html.ParseFragment(Array.Empty<byte>(),zero),zero).Length!=0)throw new Exception("Empty zero-limit HTML");Console.WriteLine("C# HTML traces, approved syntax, fatal Unicode, authored events and exact policies pass");
  }
}'''

GO=r'''package main
import("bytes";"fmt";"os";"strings")
// Require operation failure without a partially published result.
func rejectedBytes(data []byte,err error){if data!=nil||err==nil{panic("Invalid HTML bytes published")}}
// Require complete trace rejection before native event publication.
func rejectedEvents(events []HtmlEvent,err error){if events!=nil||err==nil{panic("Invalid HTML events published")}}
// Exercise ordinary HTML independently of XML provider entry points and metadata.
func main(){for _,path:=range os.Args[1:]{input,err:=os.ReadFile(path);if err!=nil{panic(err)};valid:=strings.HasSuffix(path,".valid.html");events,err:=HtmlParseFragment(input);if err!=nil{if valid{panic(err)};if events!=nil{panic("Invalid native HTML trace published")};continue};if !valid{panic("Invalid HTML candidate published: "+path)};output,err:=HtmlWriteFragment(events);if err!=nil{panic(err)};copy,err:=HtmlParseFragment(output);if err!=nil{panic(err)};second,err:=HtmlWriteFragment(copy);if err!=nil||!bytes.Equal(output,second){panic("HTML canonical replay")};if err=os.WriteFile(path+".go.html",output,0600);err!=nil{panic(err)}}
  if !strings.Contains(HtmlSupportedSubset(),"no doctype"){panic("HTML subset description")};input:=[]byte("<DIV hidden title=\"x\">é&amp;😀<br></DIV>");events,err:=HtmlParseFragment(input);if err!=nil{panic(err)};output,err:=HtmlWriteFragment(events);if err!=nil{panic(err)};exact:=HtmlLimits{len(input),len(output),len(events),2,2,7};copy,err:=HtmlParseFragment(input,exact);if err!=nil{panic(err)};repeated,err:=HtmlWriteFragment(copy,exact);if err!=nil||!bytes.Equal(output,repeated){panic("Exact HTML bounds")};for index:=2;index<6;index++{policy:=exact;switch index{case 2:policy.MaxEvents--;case 3:policy.MaxDepth--;case 4:policy.MaxAttributesPerElement--;case 5:policy.MaxTextBytes--};rejectedEvents(HtmlParseFragment(input,policy));rejectedBytes(HtmlWriteFragment(events,policy))};short:=exact;short.MaxInputBytes--;rejectedEvents(HtmlParseFragment(input,short));short=exact;short.MaxOutputBytes--;rejectedBytes(HtmlWriteFragment(events,short));negative:=exact;negative.MaxDepth=-1;rejectedEvents(HtmlParseFragment(input,negative));rejectedBytes(HtmlWriteFragment(events,negative));rejectedEvents(HtmlParseFragment([]byte{0xc3,0x28}));
  authored:=[]HtmlEvent{{Kind:"start_fragment"},{Kind:"start_element",Name:"span",Attributes:[]HtmlAttribute{{Name:"hidden",Boolean:true}}},{Kind:"text",Text:"<&\r\""},{Kind:"end_element",Name:"span"},{Kind:"end_fragment"}};actual,err:=HtmlWriteFragment(authored);if err!=nil||string(actual)!="<span hidden>&lt;&amp;&#13;\"</span>"{panic("Authored HTML escaping")};malformed:=append([]HtmlEvent(nil),events...);for index:=range malformed{if malformed[index].Kind=="text"{malformed[index].Text="\xed\xa0\x80"}};rejectedBytes(HtmlWriteFragment(malformed));rejectedBytes(HtmlWriteFragment([]HtmlEvent{{Kind:"start_fragment"},{Kind:"end_fragment"}},HtmlLimits{0,0,1,0,0,0}));zero:=HtmlLimits{0,0,2,0,0,0};empty,err:=HtmlParseFragment([]byte{},zero);if err!=nil{panic(err)};result,err:=HtmlWriteFragment(empty,zero);if err!=nil||len(result)!=0{panic("Empty zero-limit HTML")};fmt.Println("Go HTML traces, approved syntax, fatal Unicode, authored events and exact policies pass")
}'''


def main():
    """Generate actual XML-opted-in SDKs and validate HTML with independent token and tree implementations."""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path);parser.add_argument('--language',default='java,csharp,go');parser.add_argument('--wsl-languages',default='');parser.add_argument('--python-dependencies',type=Path);parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args()
    if args.python_dependencies:sys.path.insert(0,str(args.python_dependencies.resolve()))
    from lxml import html as provider
    provider.fragment_fromstring('<p>required independent tree oracle</p>')
    args.compiler=args.compiler.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if not languages or len(languages)!=len(set(languages)) or any(language not in ('java','csharp','go') for language in languages):parser.error('Choose distinct languages from java,csharp,go')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:parser.error('In-repository output must remain beneath ignored out')
    args.build.mkdir(parents=True,exist_ok=True);paths=[]
    fixtures=FIXTURES+['<DIV\fhidden>text</DIV>','<p>line\fspace</p>','']
    for index,document in enumerate(fixtures+INVALID):
        valid=index<len(fixtures);path=args.build/(str(index)+('.valid.html' if valid else '.invalid.html'));path.write_bytes(document.encode());paths.append(path)
    model=args.build/'html.serializer';model.write_text('serializer version 1.20.0; namespace portable_html {class sample {public uint32 value(1);}}',encoding='utf-8');command=[args.compiler,'--input',model,'--language',','.join(languages),'--xml.enabled','true','--go.package','main']
    for language in languages:
        directory=args.build/language;directory.mkdir(exist_ok=True);command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args)
    for language,filename,source in [('java','Main.java',JAVA),('csharp','Program.cs',CSHARP),('go','main.go',GO)]:
        if language not in languages:continue
        directory=args.build/'source'/language;directory.mkdir(parents=True,exist_ok=True);(directory/filename).write_text(source,encoding='utf-8')
        for path in paths:Path(str(path)+f'.{language}.html').unlink(missing_ok=True)
        launch=runner.build(language,'html_fragments',args.build/language,directory);sdk.run([*launch,*(runner.path(language,path) for path in paths)])
        for index,path in enumerate(paths):
            output=Path(str(path)+f'.{language}.html')
            if index<len(fixtures):
                actual=output.read_bytes().decode()
                # libxml2 drops form feed in text and misses its attribute-separator role.
                # HTMLParser checks the exact original trace; only the secondary tree
                # comparison normalizes that shared HTML whitespace on both inputs.
                if trace(fixtures[index])!=trace(actual) or tree_trace(fixtures[index].replace('\f',' '))!=tree_trace(actual.replace('\f',' ')):raise AssertionError(f'Independent ordinary HTML oracle differs: {language}:{index}')
            elif output.exists():raise AssertionError('Invalid HTML subset candidate published')
    print(f'PASS: {len(paths)} HTML subset decisions, two independent semantic oracles and exact bounds across {len(languages)} actual SDKs',flush=True)


if __name__=='__main__':main()
