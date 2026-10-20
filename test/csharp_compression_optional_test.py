# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual emitted .NET 8 optional managed providers, exact frames and borrowed lifecycle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import csharp_compression_test as core
from java_compression_optional_test import fixtures as optional_fixtures
from go_compression_optional_test import native_fixtures


EXTRA = r'''
  static C.Provider? Zstd, Lz4;
  // Keep selection explicit and operation-local; no process-global provider registration is used.
  static C.Provider? Provider(C.Format format) => format == C.Format.Zstd ? Zstd : Lz4;
  // Adapt the core lifecycle controls' BCL level selectors to defined optional provider levels.
  static C.EncodeOptions E(C.Format format, int maxInputBytes=C.DefaultMaxBytes, int maxOutputBytes=C.DefaultMaxBytes,
      CompressionLevel level=CompressionLevel.Optimal, Action? checkCancelled=null) {
    if(format!=C.Format.Zstd&&format!=C.Format.Lz4)return new C.EncodeOptions(format,maxInputBytes,maxOutputBytes,level,checkCancelled);
    int selected=format==C.Format.Zstd?level switch{CompressionLevel.NoCompression=>0,CompressionLevel.Fastest=>1,CompressionLevel.SmallestSize=>9,_=>3}:
      level switch{CompressionLevel.NoCompression=>0,CompressionLevel.Fastest=>0,CompressionLevel.SmallestSize=>12,_=>3};
    return C.EncodeOptions.WithProvider(format,Provider(format)!,maxInputBytes,maxOutputBytes,selected,checkCancelled);
  }
  // Preserve every compression/native bound while selecting the independently admitted provider.
  static C.DecodeOptions D(C.Format format, int maxInputBytes=C.DefaultMaxBytes, int maxOutputBytes=C.DefaultMaxBytes,
      int maxWindowBytes=C.DefaultMaxWindowBytes, Action? checkCancelled=null) {
    return format!=C.Format.Zstd&&format!=C.Format.Lz4?new C.DecodeOptions(format,maxInputBytes,maxOutputBytes,maxWindowBytes,checkCancelled):
      C.DecodeOptions.WithProvider(format,Provider(format)!,maxInputBytes,maxOutputBytes,maxWindowBytes,checkCancelled);
  }
  // Replay independent frame decisions with explicit expected categories and byte/history policies.
  static int Fixtures(string directory) {
    int count=0;
    foreach(C.Format format in Formats)foreach(string line in File.ReadAllLines(Path.Combine(directory,format.ToString().ToUpperInvariant()+".tsv"))){
      string[] row=line.Split('\t');byte[] frame=File.ReadAllBytes(Path.Combine(directory,row[0]));
      var options=D(format,int.Parse(row[3]),int.Parse(row[4]),int.Parse(row[5]));
      if(row[2]=="ok")Require(C.Decompress(frame,options).SequenceEqual(File.ReadAllBytes(Path.Combine(directory,row[1]))),row[0]);
      else Reject(row[2] switch{"INVALID_DATA"=>C.Code.InvalidData,"RESOURCE_LIMIT"=>C.Code.ResourceLimit,_=>throw new Exception("Unknown category")},()=>C.Decompress(frame,options));
      ++count;
    }
    return count;
  }
  // Missing or mismatched deployment and level/window admission must precede all application effects.
  static void Admission() {
    Reject(C.Code.Unavailable,()=>C.Provider.ZstdSharp(typeof(Program).Assembly));
    Reject(C.Code.Unavailable,()=>C.Provider.Lz4(typeof(Program).Assembly));
    Reject(C.Code.Unavailable,()=>C.Provider.ZstdSharp(null!));
    Reject(C.Code.Unavailable,()=>C.Provider.Lz4(null!));
    Reject(C.Code.InvalidOptions,()=>C.Available((C.Format)100,Zstd));
    foreach(C.Format format in Formats){
      var selected=Provider(format);bool present=selected!=null;int effects=0;var source=new Source(Array.Empty<byte>());var sink=new Sink();
      Require(!C.Available(format),"Default optional provider became implicit");
      Require(C.Available(format,selected)==present,"Explicit availability differs");
      Reject(C.Code.Unavailable,()=>C.Encode(()=>{++effects;return Array.Empty<byte>();},new C.EncodeOptions(format,checkCancelled:()=>++effects)));
      Reject(C.Code.Unavailable,()=>C.Decompress(source,new C.DecodeOptions(format,checkCancelled:()=>++effects)));
      if(!present){
        Reject(C.Code.Unavailable,()=>C.Encode(()=>{++effects;return Array.Empty<byte>();},C.EncodeOptions.WithProvider(format,selected!,checkCancelled:()=>++effects)));
        Reject(C.Code.Unavailable,()=>new C.InputStream(source,C.DecodeOptions.WithProvider(format,selected!,checkCancelled:()=>++effects)));
        Reject(C.Code.Unavailable,()=>new C.OutputStream(sink,C.EncodeOptions.WithProvider(format,selected!,checkCancelled:()=>++effects)));
      }else{
        Reject(C.Code.InvalidOptions,()=>C.Compress(Array.Empty<byte>(),E(format,maxInputBytes:-1,checkCancelled:()=>++effects)));
        Reject(C.Code.InvalidOptions,()=>C.Compress(Array.Empty<byte>(),E(format,maxOutputBytes:-1,checkCancelled:()=>++effects)));
        Reject(C.Code.InvalidOptions,()=>C.Decompress(source,D(format,maxInputBytes:-1,checkCancelled:()=>++effects)));
        Reject(C.Code.InvalidOptions,()=>C.Decompress(source,D(format,maxOutputBytes:-1,checkCancelled:()=>++effects)));
        Reject(C.Code.InvalidOptions,()=>C.Decompress(source,D(format,maxWindowBytes:-1,checkCancelled:()=>++effects)));
        Reject(C.Code.ResourceLimit,()=>C.Decompress(source,D(format,maxWindowBytes:format==C.Format.Zstd?1023:65535,checkCancelled:()=>++effects)));
        foreach(int level in new[]{selected!.MinimumLevel-1,selected.MaximumLevel+1})
          Reject(C.Code.InvalidOptions,()=>C.Encode(()=>{++effects;return Array.Empty<byte>();},C.EncodeOptions.WithProvider(format,selected,level:level,checkCancelled:()=>++effects)));
        if(format==C.Format.Lz4)foreach(int level in new[]{1,2})
          Reject(C.Code.InvalidOptions,()=>C.Encode(()=>{++effects;return Array.Empty<byte>();},C.EncodeOptions.WithProvider(format,selected,level:level,checkCancelled:()=>++effects)));
        foreach(int level in new[]{selected.MinimumLevel,0,selected.MaximumLevel}){
          byte[] frame=C.Compress(new byte[]{1},C.EncodeOptions.WithProvider(format,selected,level:level));
          Require(C.Decompress(frame,D(format)).SequenceEqual(new byte[]{1}),"Provider edge level differs");
        }
      }
      Require(effects==0&&source.Calls==0&&sink.Calls==0,"Provider admission touched application");
    }
    if(Zstd!=null&&Lz4!=null){
      Reject(C.Code.InvalidOptions,()=>C.Compress(Array.Empty<byte>(),C.EncodeOptions.WithProvider(C.Format.Zstd,Lz4)));
      Reject(C.Code.InvalidOptions,()=>C.Decompress(Array.Empty<byte>(),C.DecodeOptions.WithProvider(C.Format.Lz4,Zstd)));
    }
  }
  // Every observed optional-provider policy boundary must preserve a checked application exception exactly.
  static void Cancellation(byte[] payload) {
    foreach(C.Format format in Formats){
      byte[] frame=C.Compress(payload,E(format));
      foreach(bool encoding in new[]{false,true}){
        int observed=0;if(encoding)C.Compress(payload,E(format,checkCancelled:()=>++observed));else C.Decompress(frame,D(format,checkCancelled:()=>++observed));
        var original=new InvalidDataException("original optional cancellation");
        for(int boundary=1;boundary<=observed;++boundary){
          int polls=0;Action check=()=>{if(++polls==boundary)throw original;};
          Same(original,()=>{if(encoding)C.Compress(payload,E(format,checkCancelled:check));else C.Decompress(frame,D(format,checkCancelled:check));});
        }
      }
      bool cancel=false;var late=new IOException("late optional model cancellation");
      Same(late,()=>C.Decode(frame,bytes=>{cancel=true;return new object();},D(format,checkCancelled:()=>{if(cancel)throw late;})));
    }
  }
  // Retain independently consumable empty and multichunk outputs for every selected native level.
  static void Outputs(string directory) {
    foreach(C.Format format in Formats)foreach(int level in format==C.Format.Zstd?new[]{0,1,6,9}:new[]{0,3,6,12})foreach(int sample in new[]{0,1}){
      byte[] plain=sample==0?Array.Empty<byte>():Enumerable.Repeat((byte)65,65537).ToArray();
      File.WriteAllBytes(Path.Combine(directory,$"level-{format}-{level}-{sample}.bin"),C.Compress(plain,C.EncodeOptions.WithProvider(format,Provider(format)!,level:level)));
    }
  }
  // Exercise the real owning model, first-error guards and exact frames on actual .NET 8.
  static void Main(string[] args){try{
    Console.WriteLine("Runtime "+Environment.Version);
    Zstd=args[1]=="-"?null:C.Provider.ZstdSharp(System.Reflection.Assembly.LoadFrom(args[1]));
    Lz4=args[2]=="-"?null:C.Provider.Lz4(System.Reflection.Assembly.LoadFrom(args[2]));
    Admission();if(args[0]=="availability"){Console.WriteLine("{\"admission\":true}");return;}
    int count=Fixtures(args[0]);byte[] payload=File.ReadAllBytes(Path.Combine(args[0],"plain.bin"));
    Models(args[0]);Bounds(payload);Lifecycle(payload);Reentrancy();Cancellation(payload);Outputs(args[0]);
    Console.WriteLine("{\"input_decisions\":"+count+",\"independent_outputs\":24}");
  }catch(Exception failure){Console.Error.WriteLine(failure);Environment.Exit(1);}}
'''


def consumer(helper):
    """Reuse maintained owning-model and first-failure controls while changing only provider admission/corpus."""
    source = core.CSHARP[:core.CSHARP.index('  // Compare buffered whole-message')]
    begin, end = source.index('  // Decode every independently'), source.index('  // Wrap the original model')
    source = source[:begin] + source[end:]
    begin, end = source.index('  // Validate options and provider absence'), source.index('  // Check all provider levels')
    source = source[:begin] + source[end:]
    source = source.replace('{C.Format.None,C.Format.Gzip,C.Format.Zlib,C.Format.Deflate}', '{C.Format.Zstd,C.Format.Lz4}')
    source = source.replace('new C.EncodeOptions(', 'E(').replace('new C.DecodeOptions(', 'D(')
    source = source.replace('C.Format.Deflate', 'C.Format.Zstd')
    return (source + EXTRA + '}').replace('Schema.SerializerCompression', 'Schema.'+helper)


def run(command, directory, environment):
    """Require every explicit SDK/native invocation and retain diagnostics in the enclosing run log."""
    command = [str(value) for value in command]
    print('RUN', subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=directory, env=environment, capture_output=True, text=True, timeout=300)
    print(result.stdout+result.stderr, end='', flush=True)
    result.check_returncode()
    return result.stdout


def verify_outputs(directory, cpp_helper, environment):
    """Require all 24 standard frames to preserve exact bytes in independent Python and native C++ readers."""
    from compression import zstd
    import lz4.frame
    count = 0
    for name, levels in (('Zstd', (0, 1, 6, 9)), ('Lz4', (0, 3, 6, 12))):
        cases = [(directory/f'output-{name}-{protocol}.bin', (directory/f'native-{protocol}.bin').read_bytes())
                 for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING')]
        cases += [(directory/f'level-{name}-{level}-{sample}.bin', b'A'*65537 if sample else b'')
                  for level in levels for sample in (0, 1)]
        for path, expected in cases:
            frame = path.read_bytes()
            decoder = zstd.ZstdDecompressor() if name == 'Zstd' else lz4.frame.LZ4FrameDecompressor()
            if decoder.decompress(frame) != expected or not decoder.eof or decoder.unused_data:
                raise AssertionError('Independent C# output differs: '+path.name)
            if not frame[4] & 4:
                raise AssertionError('Optional content checksum absent')
            for mode in ('decode', 'stream_decode'):
                destination = directory/(path.stem+'-'+mode+'.plain')
                run([cpp_helper, mode, name.lower(), path, destination], directory, environment)
                if destination.read_bytes() != expected:
                    raise AssertionError('Native C++ output differs: '+path.name)
            count += 1
    return count


def smoke_source(helper, unit, profile):
    """Exercise provider overloads alongside preserved model names and native protocol selectors."""
    source = ('using System;using System.Reflection;using S=SerializerGenerated.'+unit+';'
              'using C=SerializerGenerated.'+unit+'.'+helper+';static class Program{'
              '// Preserve optional helpers and ordinary model construction in this generated profile.\n'
              'static void Main(string[] args){try{'
              '_=new C.EncodeOptions(C.Format.None,default);_=new C.DecodeOptions(C.Format.None,default);'
              'var value=new S.AccountRecord{AccountId=7};'
              'foreach(var format in new[]{C.Format.Zstd,C.Format.Lz4}){'
              'var assembly=Assembly.LoadFrom(args[format==C.Format.Zstd?0:1]);'
              'var provider=format==C.Format.Zstd?C.Provider.ZstdSharp(assembly):C.Provider.Lz4(assembly);'
              'var encode=C.EncodeOptions.WithProvider(format,provider);var decode=C.DecodeOptions.WithProvider(format,provider);'
              'byte[] packed=C.Encode(()=>value.Encode(S.Protocol.BINARY_NONE),encode);'
              'var result=C.Decode(packed,bytes=>S.AccountRecord.Decode(bytes,S.Protocol.BINARY_NONE),decode);'
              'if(result.AccountId!=7)throw new Exception("Candidate changed");')
    if profile == 'binary':
        source += ('try{C.Encode(()=>value.Encode(S.Protocol.JSON),encode);throw new Exception("Disabled protocol accepted");}'
                   'catch(ArgumentException){}')
    source += '}'
    if profile == 'collision':
        for name in ('SerializerCompression', 'SerializerCompression_2'):
            source += ('S.'+name+'.Decode(new S.'+name+'().Encode(S.Protocol.BINARY_NONE),S.Protocol.BINARY_NONE);')
    if profile == 'models':
        for name in ('Provider', 'OptionalCheck', 'ZstdApi', 'Lz4Api'):
            source += ('S.'+name+'.Decode(new S.'+name+'().Encode(S.Protocol.BINARY_NONE),S.Protocol.BINARY_NONE);')
    return source+'Console.WriteLine("PASS optional provider profile");}catch(Exception failure){Console.Error.WriteLine(failure);Environment.Exit(1);}}}'


def main():
    """Build actual generated source without provider references, then qualify explicit per-operation adapters."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--dotnet', default=shutil.which('dotnet'))
    parser.add_argument('--zstd-dll', required=True, type=Path)
    parser.add_argument('--lz4-dll', required=True, type=Path)
    parser.add_argument('--provider-path', type=Path)
    parser.add_argument('--cpp-helper', required=True, type=Path)
    parser.add_argument('--cpp-linked-helper', required=True, type=Path)
    args = parser.parse_args()
    root, build = Path(__file__).resolve().parents[1], args.build.resolve()
    if not args.dotnet:
        parser.error('The actual .NET 8 SDK/runtime is required')
    if build == root or root in build.parents and build.relative_to(root).parts[0] != 'out':
        parser.error('In-repository output must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    if args.provider_path:
        sys.path.insert(0, str(args.provider_path.resolve()))
    sdks = subprocess.check_output([args.dotnet, '--list-sdks'], text=True)
    versions = re.findall(r'^(8\.0\.\d+) ', sdks, re.M)
    if not versions:
        parser.error('An actual .NET 8 SDK is required')
    sdk = max(versions, key=lambda value: tuple(map(int, value.split('.'))))
    environment = os.environ.copy()
    environment.update(DOTNET_CLI_HOME=str(build/'dotnet-home'), DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1',
                       DOTNET_CLI_TELEMETRY_OPTOUT='1', DOTNET_GENERATE_ASPNET_CERTIFICATE='false',
                       DOTNET_ADD_GLOBAL_TOOLS_TO_PATH='false', NUGET_PACKAGES=str(build/'nuget'),
                       MSBUILDDISABLENODEREUSE='1')
    args.cpp_helper, args.cpp_linked_helper = args.cpp_helper.resolve(), args.cpp_linked_helper.resolve()
    providers = {'Zstd': args.zstd_dll.resolve(), 'Lz4': args.lz4_dll.resolve()}
    for path in providers.values():
        if not path.is_file():
            parser.error('Explicit provider DLL is absent: '+str(path))
    counts = optional_fixtures(build, ('ZSTD', 'LZ4'))
    native = native_fixtures(args, build, environment, counts)
    helper, size = core.generate(args.compiler.resolve(), build)
    (build/'Program.cs').write_text(consumer(helper), encoding='utf-8')
    core.project(build, sdk)
    run([args.dotnet, 'build', '--configuration', 'Release', '--nologo'], build, environment)
    executable = build/'bin/Release/net8.0/Compression.dll'
    deployments = {}
    for names in ((), ('Zstd',), ('Lz4',), ('Zstd', 'Lz4')):
        paths = [providers[name] if name in names else '-' for name in providers]
        run([args.dotnet, executable, 'availability', *paths], build, environment)
        deployments['+'.join(names) or 'absent'] = True
    output = run([args.dotnet, executable, build, *providers.values()], build, environment)
    result = json.loads(output.strip().splitlines()[-1])
    if result['input_decisions'] != sum(counts.values()) or verify_outputs(build, args.cpp_helper, environment) != 24:
        raise AssertionError('Incomplete C# optional corpus')
    profiles = {'normal': {'generated_bytes': size, 'generated_sha256': hashlib.sha256((build/'Schema.cs').read_bytes()).hexdigest()}}
    collision_schema = ('serializer version 1; class SerializerCompression {} class SerializerCompression_2 {} '
                        'class AccountRecord { public uint32 AccountId (1); }')
    for name, schema, unit, flags in (
            ('collision', collision_schema, 'Schema', ('--csharp.naming', 'preserve')),
            ('models', core.SCHEMA+' class Provider {} class OptionalCheck {} class ZstdApi {} class Lz4Api {}', 'Schema', ()),
            ('binary', core.SCHEMA, 'Schema', ('--protocols', 'binary_none')),
            ('checked', core.SCHEMA, 'Schema', ()),
            *((name.lower(), core.SCHEMA, name, ()) for name in ('Provider', 'OptionalCheck', 'ZstdApi', 'Lz4Api'))):
        case = build/name
        case.mkdir(exist_ok=True)
        family, size = core.generate(args.compiler.resolve(), case, schema, unit+'.cs', flags)
        if name == 'collision' and family != 'SerializerCompression_3':
            raise AssertionError('Existing helper-name models were not preserved')
        (case/'Program.cs').write_text(smoke_source(family, unit, name), encoding='utf-8')
        core.project(case, sdk)
        if name == 'checked':
            project = case/'Compression.csproj'
            project.write_text(project.read_text().replace('<Nullable>enable</Nullable>',
                '<Nullable>enable</Nullable><CheckForOverflowUnderflow>true</CheckForOverflowUnderflow>'), encoding='utf-8')
        run([args.dotnet, 'build', '--configuration', 'Release', '--nologo'], case, environment)
        run([args.dotnet, case/'bin/Release/net8.0/Compression.dll', *providers.values()], case, environment)
        profiles[name] = {'generated_bytes': size, 'generated_sha256': hashlib.sha256((case/(unit+'.cs')).read_bytes()).hexdigest()}
    report = {'sdk': sdk, 'runtime': output.splitlines()[0], 'input_decisions': counts,
              'independent_outputs': 24, 'native_cpp_consumers': 48, 'native_cpp_producers': native,
              'deployments': deployments, 'profiles': profiles,
              'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'test_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'providers': {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in providers.items()}}
    (build/'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
