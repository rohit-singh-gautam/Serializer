# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify Java/C#/Go exact model metadata, historical layouts and bounded owning digest bytes."""
import argparse
from pathlib import Path
import native_xml_lifecycle_test as contract

ROOT, sdk = contract.ROOT, contract.sdk
MODEL=contract.MODEL.rsplit('}',1)[0]+'''class wide {
  public magic uint64(99) { 18446744073709551615 };
  public version uint64 ver(1) { 18446744073709551615 } compatibility { 18446744073709551614 };
  obsolete(18446744073709551615) public uint16 old(2);
  created(18446744073709551615) public uint16 current(3);
} }'''


def cases():
    """Extend the shared corpus through the complete unsigned native revision domain."""
    result=contract.cases()
    for revision,field,valid in [('18446744073709551614','old',True),('18446744073709551615','current',True),('18446744073709551613','old',False),('18446744073709551616','current',False)]:
        result.append(('wide',contract.document('wide',revision,'<'+field+'>7</'+field+'>','18446744073709551615'),valid))
    return result


def check_output(kind,source):
    """Check exact wide unsigned metadata using arbitrary-precision independent integers."""
    contract.check_output(kind,source)
    if kind=='wide':
        node=contract.etree.fromstring(source);revision=int(node.get('{'+contract.META+'}revision'))
        assert 18446744073709551614<=revision<=18446744073709551615
        assert node.get('{'+contract.META+'}magic')=='18446744073709551615'
        assert [contract.etree.QName(child).localname for child in node]==(['old'] if revision<18446744073709551615 else ['current'])

JAVA = r'''import java.nio.file.*;import java.lang.reflect.*;import java.util.*;
/** Match independent layout decisions through strict generated whole-object APIs. */
public final class Main {
  /** Unwrap only the generated exception; reflection must not hide a failed validity decision. */
  private static Object call(Method method,Object owner,Object...arguments)throws Exception{try{return method.invoke(owner,arguments);}catch(InvocationTargetException failure){if(failure.getCause() instanceof RuntimeException cause)throw cause;throw failure;}}
  /** Require invalid authored state to discard complete XML publication. */
  private static void rejected(Runnable operation){try{operation.run();throw new AssertionError("Invalid XML model published");}catch(IllegalArgumentException expected){}}
  /** Exercise exact layouts, unchanged native protocols and independent nested digest limits. */
  public static void main(String[] arguments)throws Exception{
    rejected(()->new Schema.XmlDigestLimits(-1));
    for(String path:arguments){boolean valid=path.endsWith(".valid.xml");String kind=Path.of(path).getFileName().toString().split("-")[0];Class<?> type=Class.forName("Schema$Lifecycle$"+Character.toUpperCase(kind.charAt(0))+kind.substring(1));byte[] output;
      try{var decode=type.getMethod("decodeXml",byte[].class);Object value=call(decode,null,(Object)Files.readAllBytes(Path.of(path)));output=(byte[])call(type.getMethod("encodeXml"),value);Object copy=call(decode,null,(Object)output);
        if(!Arrays.equals(output,(byte[])call(type.getMethod("encodeXml"),copy)))throw new AssertionError("Unstable model XML");
        for(var protocol:Schema.Protocol.values())if(!Arrays.equals((byte[])call(type.getMethod("encode",Schema.Protocol.class),value,protocol),(byte[])call(type.getMethod("encode",Schema.Protocol.class),copy,protocol)))throw new AssertionError("Native revision protocol changed");
      }catch(IllegalArgumentException failure){if(valid)throw new AssertionError(path,failure);continue;}
      if(!valid)throw new AssertionError("Invalid XML accepted: "+path);Files.write(Path.of(path+".java.xml"),output);
    }
    String source="<digests xmlns='urn:serializer:js-lifecycle' raw='' exact='00ff'><algorithm>00000000000000000000000000000000</algorithm><items/></digests>";
    var value=Schema.Lifecycle.Digests.decodeXml(source.getBytes(java.nio.charset.StandardCharsets.UTF_8));value.raw=new byte[1024*1024];Arrays.fill(value.raw,(byte)255);byte[] output=value.encodeXml();if(!Arrays.equals(value.raw,Schema.Lifecycle.Digests.decodeXml(output).raw))throw new AssertionError("Exact default digest bytes");
    value.raw=Arrays.copyOf(value.raw,value.raw.length+1);rejected(()->value.encodeXml());var policy=new Schema.XmlDigestLimits(1024*1024+1);output=value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),policy);byte[] large=output;rejected(()->Schema.Lifecycle.Digests.decodeXml(large));if(Schema.Lifecycle.Digests.decodeXml(output,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),policy).raw.length!=value.raw.length)throw new AssertionError("Custom digest bytes");
    value.exact=new byte[3];rejected(()->value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),policy));
    var holder=new Schema.Lifecycle.DigestHolder();holder.value.raw=new byte[17];byte[] nested=holder.encodeXml();var small=new Schema.XmlDigestLimits(16);rejected(()->holder.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),small));rejected(()->Schema.Lifecycle.DigestHolder.decodeXml(nested,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),small));holder.value.raw=new byte[16];holder.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),small);
    var numeric=new Schema.Lifecycle.Numeric();numeric.ver=9007199254740992L;rejected(()->numeric.encodeXml());numeric.ver=9007199254740996L;rejected(()->numeric.encodeXml());var floating=new Schema.Lifecycle.Floating();floating.ver=Float.NaN;rejected(()->floating.encodeXml());floating.ver=-0.0F;rejected(()->floating.encodeXml());var dotted=new Schema.Lifecycle.Dotted();dotted.ver="1.02.0";rejected(()->dotted.encodeXml());
    System.out.println("Java exact model revisions, magic, historical defaults, native protocols and owning digest bounds pass");
  }
}'''

CSHARP = r'''using System;using System.IO;using System.Reflection;using System.Runtime.ExceptionServices;using static SerializerGenerated.Schema;
// Inspect actual public codecs while retaining strict nullable and warning checks.
internal static class Program {
  // Preserve generated failure identity across reflection without swallowing fixture failures.
  private static object Call(MethodInfo method,object? owner,params object[] arguments){try{return method.Invoke(owner,arguments)??throw new Exception("Missing codec result");}catch(TargetInvocationException failure){if(failure.InnerException is FormatException cause){ExceptionDispatchInfo.Capture(cause).Throw();}throw;}}
  // Require failure before complete XML output publication.
  private static void Rejected(Action operation){try{operation();throw new Exception("Invalid XML model published");}catch(FormatException){}}
  // Exercise exact metadata, unchanged native protocols and per-digest graph admission.
  private static void Main(string[] arguments){try{new XmlDigestLimits(-1);throw new Exception("Negative digest policy");}catch(ArgumentOutOfRangeException){}
    foreach(string path in arguments){bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);string kind=Path.GetFileName(path).Split('-')[0];Type type=typeof(LifecycleNumeric).DeclaringType!.GetNestedType("Lifecycle"+char.ToUpperInvariant(kind[0])+kind.Substring(1))??throw new Exception("Missing model");byte[] output;
      try{var decode=type.GetMethod("DecodeXml",new[]{typeof(byte[])})!;object value=Call(decode,null,File.ReadAllBytes(path));output=(byte[])Call(type.GetMethod("EncodeXml",Type.EmptyTypes)!,value);object copy=Call(decode,null,output);if(!output.AsSpan().SequenceEqual((byte[])Call(type.GetMethod("EncodeXml",Type.EmptyTypes)!,copy)))throw new Exception("Unstable XML");
        foreach(Protocol protocol in Enum.GetValues<Protocol>())if(!((byte[])Call(type.GetMethod("Encode",new[]{typeof(Protocol)})!,value,protocol)).AsSpan().SequenceEqual((byte[])Call(type.GetMethod("Encode",new[]{typeof(Protocol)})!,copy,protocol)))throw new Exception("Native protocol changed");
      }catch(FormatException failure){if(valid)throw new Exception(path,failure);continue;}if(!valid)throw new Exception("Invalid XML accepted: "+path);File.WriteAllBytes(path+".csharp.xml",output);
    }
    string source="<digests xmlns='urn:serializer:js-lifecycle' raw='' exact='00ff'><algorithm>00000000000000000000000000000000</algorithm><items/></digests>";
    var valueDigest=LifecycleDigests.DecodeXml(System.Text.Encoding.UTF8.GetBytes(source));valueDigest.Raw=new byte[1024*1024];Array.Fill(valueDigest.Raw,(byte)255);byte[] encoded=valueDigest.EncodeXml();if(!valueDigest.Raw.AsSpan().SequenceEqual(LifecycleDigests.DecodeXml(encoded).Raw))throw new Exception("Default digest bytes");valueDigest.Raw=new byte[1024*1024+1];Rejected(()=>valueDigest.EncodeXml());var policy=new XmlDigestLimits(1024*1024+1);encoded=valueDigest.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),policy);Rejected(()=>LifecycleDigests.DecodeXml(encoded));if(LifecycleDigests.DecodeXml(encoded,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),policy).Raw.Length!=valueDigest.Raw.Length)throw new Exception("Custom digest bytes");valueDigest.Exact=new byte[3];Rejected(()=>valueDigest.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),policy));
    var holder=new LifecycleDigestHolder();holder.Value.Raw=new byte[17];byte[] nested=holder.EncodeXml();var small=new XmlDigestLimits(16);Rejected(()=>holder.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),small));Rejected(()=>LifecycleDigestHolder.DecodeXml(nested,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),small));holder.Value.Raw=new byte[16];holder.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),small);
    var numeric=new LifecycleNumeric();numeric.Ver=9007199254740992UL;Rejected(()=>numeric.EncodeXml());numeric.Ver=9007199254740996UL;Rejected(()=>numeric.EncodeXml());var floating=new LifecycleFloating();floating.Ver=float.NaN;Rejected(()=>floating.EncodeXml());floating.Ver=-0.0F;Rejected(()=>floating.EncodeXml());var dotted=new LifecycleDotted();dotted.Ver="1.02.0";Rejected(()=>dotted.EncodeXml());
    Console.WriteLine("C# exact revisions, magic, historical defaults, native protocols and owning digest bounds pass");
  }
}'''

GO = r'''package main
import("bytes";"fmt";"os";"path/filepath";"strings";"math")
// Require complete failure without publishing owning XML output bytes.
func rejected(data []byte,err error){if data!=nil||err==nil{panic("Invalid XML model published")}}
type lifecycleCodec interface{EncodeXML(...XmlOptions)([]byte,error);Encode(Protocol)([]byte,error)}
// Select the actual generated owning decoder without replacing its typed implementation.
func decode(kind string,data []byte)(lifecycleCodec,error){switch kind{case "numeric":return DecodeLifecycleNumericXML(data);case "wide":return DecodeLifecycleWideXML(data);case "dotted":return DecodeLifecycleDottedXML(data);case "floating":return DecodeLifecycleFloatingXML(data);case "derived":return DecodeLifecycleDerivedXML(data);case "typed":return DecodeLifecycleTypedXML(data);case "plain":return DecodeLifecyclePlainXML(data);case "envelope":return DecodeLifecycleEnvelopeXML(data);case "digests":return DecodeLifecycleDigestsXML(data)};return nil,fmt.Errorf("Unknown model")}
// Verify all native protocols, exact independent layout decisions and owning digest boundaries.
func main(){rejected(NewLifecycleDigests().EncodeXMLWithDigestLimits(XmlDigestLimits{-1}));for _,path:=range os.Args[1:]{data,err:=os.ReadFile(path);if err!=nil{panic(err)};valid:=strings.HasSuffix(path,".valid.xml");kind:=strings.Split(filepath.Base(path),"-")[0];value,err:=decode(kind,data);if err!=nil{if valid{panic(fmt.Sprintf("%s: %v",path,err))};continue};if !valid{panic("Invalid XML accepted: "+path)};output,err:=value.EncodeXML();if err!=nil{panic(err)};copy,err:=decode(kind,output);if err!=nil{panic(err)};again,err:=copy.EncodeXML();if err!=nil||!bytes.Equal(output,again){panic("Unstable model XML")};for _,protocol:=range []Protocol{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING}{first,err:=value.Encode(protocol);if err!=nil{panic(err)};second,err:=copy.Encode(protocol);if err!=nil||!bytes.Equal(first,second){panic("Native protocol changed")}};if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}}
  source:=[]byte("<digests xmlns='urn:serializer:js-lifecycle' raw='' exact='00ff'><algorithm>00000000000000000000000000000000</algorithm><items/></digests>");value,err:=DecodeLifecycleDigestsXML(source);if err!=nil{panic(err)};value.Raw=bytes.Repeat([]byte{255},1024*1024);output,err:=value.EncodeXML();if err!=nil{panic(err)};copy,err:=DecodeLifecycleDigestsXML(output);if err!=nil||!bytes.Equal(value.Raw,copy.Raw){panic("Default digest bytes")};value.Raw=append(value.Raw,0);rejected(value.EncodeXML());policy:=XmlDigestLimits{1024*1024+1};output,err=value.EncodeXMLWithDigestLimits(policy);if err!=nil{panic(err)};invalid,err:=DecodeLifecycleDigestsXML(output);if err==nil||invalid!=nil{panic("Oversized digest candidate published")};copy,err=DecodeLifecycleDigestsXMLWithDigestLimits(output,policy);if err!=nil||!bytes.Equal(value.Raw,copy.Raw){panic("Custom digest bytes")};value.Exact=append(value.Exact,0);rejected(value.EncodeXMLWithDigestLimits(policy));holder:=NewLifecycleDigestHolder();holder.Value.Raw=make([]byte,17);nested,err:=holder.EncodeXML();if err!=nil{panic(err)};small:=XmlDigestLimits{16};rejected(holder.EncodeXMLWithDigestLimits(small));held,err:=DecodeLifecycleDigestHolderXMLWithDigestLimits(nested,small);if err==nil||held!=nil{panic("Nested digest candidate published")};holder.Value.Raw=holder.Value.Raw[:16];if _,err=holder.EncodeXMLWithDigestLimits(small);err!=nil{panic(err)}
  numeric:=NewLifecycleNumeric();numeric.Ver=9007199254740992;rejected(numeric.EncodeXML());numeric.Ver=9007199254740996;rejected(numeric.EncodeXML());floating:=NewLifecycleFloating();floating.Ver=float32(math.NaN());rejected(floating.EncodeXML());floating.Ver=float32(math.Copysign(0,-1));rejected(floating.EncodeXML());dotted:=NewLifecycleDotted();dotted.Ver="1.02.0";rejected(dotted.EncodeXML());fmt.Println("Go exact revisions, magic, native protocols and bounded owning digest bytes pass")
}'''


def main():
    """Generate native models, compile strict actual SDKs and inspect published values independently."""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path);parser.add_argument('--language',default='java,csharp,go');parser.add_argument('--wsl-languages',default='');parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args();args.compiler=args.compiler.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if not languages or len(languages)!=len(set(languages)) or any(language not in ('java','csharp','go') for language in languages):parser.error('Choose distinct names from java,csharp,go')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:parser.error('In-repository output must stay beneath ignored out')
    args.build.mkdir(parents=True,exist_ok=True);(args.build/'model.serializer').write_text(MODEL,encoding='utf-8');(args.build/'options.ini').write_text('[xml]\nenabled=true\n[xml.class.lifecycle::digests]\nwrapper.items=items\nname.items=item\n',encoding='utf-8');paths=[]
    for index,(kind,source,valid) in enumerate(cases()):
        path=args.build/f'{kind}-{index}.{"valid" if valid else "invalid"}.xml';path.write_text(source,encoding='utf-8');paths.append((kind,path,valid))
    command=[args.compiler,'--input',args.build/'model.serializer','--config',args.build/'options.ini','--language',','.join(languages),'--go.package','main']
    for language in languages:
        directory=args.build/language;directory.mkdir(exist_ok=True);command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args)
    for language,filename,source in [('java','Main.java',JAVA),('csharp','Program.cs',CSHARP),('go','main.go',GO)]:
        if language not in languages:continue
        directory=args.build/'source'/language;directory.mkdir(parents=True,exist_ok=True);(directory/filename).write_text(source,encoding='utf-8')
        for _,path,_ in paths:Path(str(path)+f'.{language}.xml').unlink(missing_ok=True)
        launch=runner.build(language,'xml_lifecycle',args.build/language,directory);sdk.run([*launch,*(runner.path(language,path) for _,path,_ in paths)])
        for kind,path,valid in paths:
            output=Path(str(path)+f'.{language}.xml')
            if valid:check_output(kind,output.read_bytes())
            elif output.exists():raise AssertionError('Invalid model XML published')
    print(f'PASS: {len(paths)} exact model/digest probes, four native protocols, authored revision edits and nested digest boundaries across {len(languages)} SDKs',flush=True)


if __name__=='__main__':main()
