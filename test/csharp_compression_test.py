# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated .NET 8 compression, exact framing, native bytes and borrowed lifecycle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import re
import shutil
import struct
import subprocess
import tempfile
import zlib

from java_compression_test import fixtures as common_fixtures


SCHEMA = 'serializer version 1; class account_record stable_ids { public uint32 account_id (1); public string label (2); }'

CSHARP = r'''using System;
using System.IO;
using System.IO.Compression;
using System.Diagnostics;
using System.Linq;
using Schema = SerializerGenerated.Schema;
using C = SerializerGenerated.Schema.SerializerCompression;

static class Program {
  // Keep every check active in Release builds.
  static void Require(bool value, string message) { if (!value) throw new Exception(message); }
  // Distinguish compression categories from callback, allocation and transport exceptions.
  static void Reject(C.Code code, Action operation) {
    try { operation(); } catch (C.Error failure) { Require(failure.Code == code, "Wrong category: " + failure.Code); return; }
    throw new Exception("Accepted " + code);
  }
  // Application exception objects must survive provider and cleanup boundaries unchanged.
  static void Same(Exception expected, Action operation) {
    try { operation(); } catch (Exception actual) { Require(ReferenceEquals(expected, actual), "Original exception replaced: " + actual); return; }
    throw new Exception("Expected original exception");
  }
  // Fenced streams cannot retry a provider, callback or borrowed transport.
  static void Fenced(Action operation) {
    try { operation(); } catch (InvalidOperationException) { return; }
    throw new Exception("Reused a failed/finished stream");
  }
  // Observe bounded borrowed writes and reject ownership or flush propagation.
  sealed class Sink : MemoryStream {
    internal int Calls, Maximum, Flushes, Closes;
    internal Exception? Failure;
    internal Action? AfterWrite;
    public override void Write(byte[] data, int offset, int count) {
      ++Calls; Maximum = Math.Max(Maximum, count);
      if (Failure != null) throw Failure;
      base.Write(data, offset, count); AfterWrite?.Invoke();
    }
    public override void Flush() { ++Flushes; }
    protected override void Dispose(bool disposing) { ++Closes; }
  }
  // Exercise short reads, EOF probes, failed transports and retained source lifetime.
  sealed class Source : MemoryStream {
    internal int Calls, Maximum, Closes;
    internal int Batch = int.MaxValue;
    internal Exception? Failure;
    internal Action? AtEof;
    internal Action? AfterRead;
    internal Source(byte[] value) : base(value) { }
    public override int Read(byte[] data, int offset, int count) {
      ++Calls; Maximum = Math.Max(Maximum, count);
      if (Failure != null) throw Failure;
      int result = base.Read(data, offset, Math.Min(count, Batch));
      AfterRead?.Invoke(); if (result == 0) AtEof?.Invoke(); return result;
    }
    protected override void Dispose(bool disposing) { ++Closes; }
  }
  static readonly C.Format[] Formats = {C.Format.None,C.Format.Gzip,C.Format.Zlib,C.Format.Deflate};
  // Decode every independently authored valid/invalid frame at exact and one-tight bounds.
  static int Fixtures(string directory) {
    int count = 0;
    foreach (string row in File.ReadAllLines(Path.Combine(directory, "fixtures.tsv"))) {
      string[] fields = row.Split('\t');
      C.Format format = fields[0] switch {"GZIP"=>C.Format.Gzip,"ZLIB"=>C.Format.Zlib,"DEFLATE"=>C.Format.Deflate,_=>throw new Exception("Unknown fixture format")};
      byte[] packed = File.ReadAllBytes(Path.Combine(directory, fields[2]));
      if (fields[1] == "invalid") Reject(C.Code.InvalidData, () => C.Decompress(packed, new C.DecodeOptions(format)));
      else {
        byte[] plain = File.ReadAllBytes(Path.Combine(directory, fields[3]));
        Require(C.Decompress(packed,new C.DecodeOptions(format,packed.Length,plain.Length)).SequenceEqual(plain),fields[2]);
        if (plain.Length != 0) Reject(C.Code.ResourceLimit,()=>C.Decompress(packed,new C.DecodeOptions(format,packed.Length,plain.Length-1)));
        if (packed.Length != 0) Reject(C.Code.ResourceLimit,()=>C.Decompress(packed,new C.DecodeOptions(format,packed.Length-1,plain.Length)));
      }
      ++count;
    }
    return count;
  }
  // Wrap the original model methods without changing their wire paths or protocol admission.
  static void Models(string directory) {
    var value = new Schema.AccountRecord { AccountId=42, Label="message \U0001f600" };
    foreach (Schema.Protocol protocol in Enum.GetValues<Schema.Protocol>()) {
      byte[] ordinary = value.Encode(protocol);
      File.WriteAllBytes(Path.Combine(directory,"native-"+protocol+".bin"),ordinary);
      foreach (C.Format format in Formats) {
        byte[] packed = C.Encode(()=>value.Encode(protocol),new C.EncodeOptions(format));
        var candidate = C.Decode(packed,bytes=>Schema.AccountRecord.Decode(bytes,protocol),new C.DecodeOptions(format));
        Require(candidate.AccountId == value.AccountId && candidate.Label == value.Label,"Whole-model candidate changed");
        Require(C.Decompress(packed,new C.DecodeOptions(format)).SequenceEqual(ordinary),"Ordinary wire bytes changed");
        File.WriteAllBytes(Path.Combine(directory,"output-"+format+"-"+protocol+".bin"),packed);
        byte[] extra = ordinary.Concat(new byte[]{0}).ToArray();
        try { C.Decode(C.Compress(extra,new C.EncodeOptions(format)),bytes=>Schema.AccountRecord.Decode(bytes,protocol),new C.DecodeOptions(format)); throw new Exception("Accepted inner suffix"); }
        catch (Exception failure) when (failure is FormatException || failure is ArgumentException) { }
      }
    }
    bool cancel = false; var original = new OperationCanceledException("late candidate cancellation");
    var policy = new C.DecodeOptions(C.Format.Gzip,checkCancelled:()=>{if(cancel)throw original;});
    byte[] wire = C.Compress(value.Encode(Schema.Protocol.BINARY_NONE),new C.EncodeOptions(C.Format.Gzip));
    Same(original,()=>C.Decode(wire,bytes=>{var candidate=Schema.AccountRecord.Decode(bytes,Schema.Protocol.BINARY_NONE);cancel=true;return candidate;},policy));
  }
  // Validate options and provider absence before callbacks or borrowed I/O.
  static void Admission() {
    foreach (C.Format format in new[]{C.Format.Zstd,C.Format.Lz4}) {
      Require(!C.Available(format),"Unavailable provider reported available"); int callbacks=0;
      Reject(C.Code.Unavailable,()=>C.Encode(()=>{++callbacks;return Array.Empty<byte>();},new C.EncodeOptions(format)));
      var source=new Source(Array.Empty<byte>());var sink=new Sink();
      Reject(C.Code.Unavailable,()=>C.Decompress(source,new C.DecodeOptions(format)));
      Reject(C.Code.Unavailable,()=>new C.OutputStream(sink,new C.EncodeOptions(format)));
      Require(source.Calls==0&&sink.Calls==0&&callbacks==0,"Unavailable provider touched application state");
    }
    Reject(C.Code.InvalidOptions,()=>C.Available((C.Format)100));
    foreach (C.Format format in Formats) {
      Reject(C.Code.InvalidOptions,()=>C.Compress(Array.Empty<byte>(),new C.EncodeOptions(format,-1)));
      Reject(C.Code.InvalidOptions,()=>C.Compress(Array.Empty<byte>(),new C.EncodeOptions(format,level:(CompressionLevel)100)));
      Reject(C.Code.InvalidOptions,()=>C.Decompress(Array.Empty<byte>(),new C.DecodeOptions(format,maxWindowBytes:-1)));
    }
    int polls=0;var unopened=new Source(Array.Empty<byte>());
    Reject(C.Code.ResourceLimit,()=>C.Decompress(unopened,new C.DecodeOptions(C.Format.Gzip,maxWindowBytes:32767,checkCancelled:()=>++polls)));
    Require(unopened.Calls==0&&polls==0,"Inadequate history touched source/callback");
  }
  // Check all provider levels and exact compressed/plain bounds, including zero-length messages.
  static void Bounds(byte[] payload) {
    foreach (C.Format format in Formats) foreach (CompressionLevel level in Enum.GetValues<CompressionLevel>()) {
      byte[] wire=C.Compress(payload,new C.EncodeOptions(format,level:level));
      Require(C.Compress(payload,new C.EncodeOptions(format,payload.Length,wire.Length,level)).SequenceEqual(wire),"Exact encoding limit changed output");
      Require(C.Decompress(wire,new C.DecodeOptions(format,wire.Length,payload.Length)).SequenceEqual(payload),"Level mismatch");
      if(payload.Length!=0)Reject(C.Code.ResourceLimit,()=>C.Compress(payload,new C.EncodeOptions(format,payload.Length-1,wire.Length,level)));
      if(wire.Length!=0)Reject(C.Code.ResourceLimit,()=>C.Compress(payload,new C.EncodeOptions(format,payload.Length,wire.Length-1,level)));
      byte[] empty=C.Compress(Array.Empty<byte>(),new C.EncodeOptions(format,0,128,level));
      Require(C.Decompress(empty,new C.DecodeOptions(format,empty.Length,0)).Length==0,"Empty frame failed");
    }
    var sentinel=new InvalidDataException("application callback sentinel");
    byte[] packed=C.Compress(payload,new C.EncodeOptions(C.Format.Deflate));int checks=0;
    C.Decompress(packed,new C.DecodeOptions(C.Format.Deflate,checkCancelled:()=>++checks));
    for(int stop=1;stop<=checks;++stop){int current=0;int selected=stop;
      Same(sentinel,()=>C.Decompress(packed,new C.DecodeOptions(C.Format.Deflate,checkCancelled:()=>{if(++current==selected)throw sentinel;})));
    }
  }
  // Finish/dispose/failure paths never retry external effects or acquire borrowed ownership.
  static void Lifecycle(byte[] payload) {
    foreach(C.Format format in Formats){
      var sink=new Sink();var writer=new C.OutputStream(sink,new C.EncodeOptions(format));
      writer.Write(payload,0,payload.Length);writer.Flush();Require(sink.Length==0&&sink.Flushes==0,"Staging published early");writer.Finish();
      byte[] wire=sink.ToArray();Require(C.Decompress(wire,new C.DecodeOptions(format)).SequenceEqual(payload),"Wrapper output mismatch");
      Fenced(()=>writer.Finish());Fenced(()=>writer.WriteByte(0));writer.Dispose();Require(sink.Closes==0&&sink.Flushes==0&&sink.Maximum<=32768,"Borrowed sink contract");
      var abandoned=new Sink();using(var unused=new C.OutputStream(abandoned,new C.EncodeOptions(format))){unused.Write(payload);}
      Require(abandoned.Length==0&&abandoned.Closes==0&&abandoned.Flushes==0,"Dispose finished output");
      var source=new Source(wire){Batch=7};var reader=new C.InputStream(source,new C.DecodeOptions(format));reader.Flush();Require(source.Calls==0,"Input flush read source");
      using var decoded=new MemoryStream();byte[] buffer=new byte[199];int count;while((count=reader.Read(buffer,0,buffer.Length))!=0)decoded.Write(buffer,0,count);
      reader.Finish();Fenced(()=>reader.ReadByte());reader.Dispose();Require(decoded.ToArray().SequenceEqual(payload)&&source.Closes==0&&source.Maximum<=32768,"Borrowed short source contract");
      var unread=new C.InputStream(new Source(wire),new C.DecodeOptions(format));Reject(C.Code.InvalidData,()=>unread.Finish());Fenced(()=>unread.ReadByte());
      var broken=new Sink{Failure=new IOException("original sink")};var failed=new C.OutputStream(broken,new C.EncodeOptions(format));failed.Write(payload);
      Same(broken.Failure!,()=>failed.Finish());int calls=broken.Calls;Fenced(()=>failed.Finish());failed.Dispose();Require(broken.Calls==calls&&broken.Closes==0,"Failed output retried");
      var brokenSource=new Source(wire){Failure=new IOException("original source")};var failedInput=new C.InputStream(brokenSource,new C.DecodeOptions(format));
      Same(brokenSource.Failure!,()=>failedInput.ReadByte());calls=brokenSource.Calls;Fenced(()=>failedInput.ReadByte());failedInput.Dispose();Require(brokenSource.Calls==calls&&brokenSource.Closes==0,"Failed input retried");
      bool cancel=false;var sentinel=new OperationCanceledException("staged cancellation");
      var cancellable=new C.InputStream(new Source(wire),new C.DecodeOptions(format,checkCancelled:()=>{if(cancel)throw sentinel;}));cancellable.ReadByte();cancel=true;Same(sentinel,()=>cancellable.ReadByte());Fenced(()=>cancellable.Finish());
      cancel=false;var lateSink=new Sink{AfterWrite=()=>cancel=true};var lateWriter=new C.OutputStream(lateSink,new C.EncodeOptions(format,checkCancelled:()=>{if(cancel)throw sentinel;}));lateWriter.Write(payload);Same(sentinel,()=>lateWriter.Finish());Fenced(()=>lateWriter.Finish());
      cancel=false;var lateSource=new Source(wire){AtEof=()=>cancel=true};var lateInput=new C.InputStream(lateSource,new C.DecodeOptions(format,checkCancelled:()=>{if(cancel)throw sentinel;}));Same(sentinel,()=>lateInput.ReadByte());Fenced(()=>lateInput.Finish());
    }
    var overflowSource=new Source(new byte[100]);Reject(C.Code.ResourceLimit,()=>C.Decompress(overflowSource,new C.DecodeOptions(C.Format.None,10,100)));
    Require(overflowSource.Maximum==11&&overflowSource.Position==11,"Overflow source was read unboundedly");
  }
  // Synchronous policy and transport callbacks cannot close/reenter an active wrapper and publish.
  static void Reentrancy() {
    foreach(C.Format format in Formats) {
      var sink=new Sink(); C.OutputStream? writer=null; bool armed=false;
      var encodePolicy=new C.EncodeOptions(format,checkCancelled:()=>{if(armed)writer!.Dispose();});
      writer=new C.OutputStream(sink,encodePolicy);writer.WriteByte(1);armed=true;
      Fenced(()=>writer.Finish());Require(sink.Calls==0,"Disposed policy published borrowed output");
      Fenced(()=>writer.WriteByte(2));writer.Dispose();Require(sink.Closes==0,"Disposed borrowed sink");

      sink=new Sink();armed=false;writer=new C.OutputStream(sink,encodePolicy);armed=true;
      Fenced(()=>writer.WriteByte(1));Require(sink.Calls==0,"Disposed write published output");
      sink=new Sink();armed=false;writer=new C.OutputStream(sink,encodePolicy);armed=true;
      Fenced(()=>writer.Flush());Require(sink.Flushes==0,"Disposed flush touched sink");

      byte[] packed=C.Compress(new byte[]{1},new C.EncodeOptions(format));
      var source=new Source(packed);C.InputStream? reader=null;armed=false;
      var decodePolicy=new C.DecodeOptions(format,checkCancelled:()=>{if(armed)reader!.Dispose();});
      reader=new C.InputStream(source,decodePolicy);armed=true;
      Fenced(()=>reader.ReadByte());Require(source.Calls==0,"Disposed policy read borrowed input");
      Fenced(()=>reader.Finish());reader.Dispose();Require(source.Closes==0,"Disposed borrowed source");

      source=new Source(packed);source.AfterRead=()=>reader!.Dispose();
      reader=new C.InputStream(source,new C.DecodeOptions(format));byte[] destination={99};
      Fenced(()=>reader.Read(destination,0,destination.Length));
      Require(source.Calls==1&&destination[0]==99,"Closed source callback continued or published bytes");
      Fenced(()=>reader.Finish());reader.Dispose();Require(source.Closes==0,"Closed source ownership changed");

      armed=false;int polls=0;
      var emptySource=new Source(C.Compress(Array.Empty<byte>(),new C.EncodeOptions(format)));
      reader=new C.InputStream(emptySource,new C.DecodeOptions(format,checkCancelled:()=>{
        if(armed&&++polls==3)reader!.Dispose();
      }));
      Require(reader.ReadByte()==-1,"Empty source did not reach EOF");armed=true;
      Fenced(()=>reader.Finish());Fenced(()=>reader.ReadByte());

      // Swallowing the nested rejection must still fence the outer operation with that first error.
      sink=new Sink();armed=false;Exception? first=null;
      writer=new C.OutputStream(sink,new C.EncodeOptions(format,checkCancelled:()=>{
        if(armed){armed=false;try{writer!.Flush();}catch(Exception failure){first=failure;}}
      }));
      armed=true;Exception? outer=null;
      try{writer.WriteByte(1);}catch(Exception failure){outer=failure;}
      Require(first is InvalidOperationException&&ReferenceEquals(first,outer),"Output reentry lost first failure");
      Require(sink.Calls==0,"Reentrant policy published output");Fenced(()=>writer.Finish());writer.Dispose();

      source=new Source(packed);armed=false;first=null;
      reader=new C.InputStream(source,new C.DecodeOptions(format,checkCancelled:()=>{
        if(armed){armed=false;try{reader!.Flush();}catch(Exception failure){first=failure;}}
      }));
      armed=true;outer=null;try{reader.ReadByte();}catch(Exception failure){outer=failure;}
      Require(first is InvalidOperationException&&ReferenceEquals(first,outer),"Input reentry lost first failure");
      Require(source.Calls==0,"Reentrant policy read input");Fenced(()=>reader.Finish());reader.Dispose();
    }
    // Closing in a borrowed write callback must stop the next independently bounded write.
    var target=new Sink();C.OutputStream? output=null;
    target.AfterWrite=()=>output!.Dispose();output=new C.OutputStream(target);
    output.Write(new byte[32769]);Fenced(()=>output.Finish());
    Require(target.Calls==1&&target.Length==32768&&target.Closes==0,"Closed transport continued writing");
    Fenced(()=>output.Finish());
    // Preserve the first nested-operation error even if that callback later throws another error.
    target=new Sink();Exception? retained=null;var later=new IOException("later transport failure");
    target.AfterWrite=()=>{try{output!.Flush();}catch(Exception failure){retained=failure;}throw later;};
    output=new C.OutputStream(target);output.WriteByte(1);Exception? actual=null;
    try{output.Finish();}catch(Exception failure){actual=failure;}
    Require(retained is InvalidOperationException&&ReferenceEquals(retained,actual),"Transport reentry replaced first error");
    Require(target.Calls==1,"Reentrant transport retried");Fenced(()=>output.Finish());output.Dispose();
  }
  // Compare buffered whole-message validation with the BCL baseline; no per-byte transport is used.
  static void Performance(string directory) {
    foreach(string name in new[]{"random","repeated"}) {
      byte[] plain=File.ReadAllBytes(Path.Combine(directory,"perf-"+name+".bin"));byte[] packed=C.Compress(plain,new C.EncodeOptions(C.Format.Deflate));
      byte[] Baseline(){using var source=new MemoryStream(packed);using var provider=new DeflateStream(source,CompressionMode.Decompress);byte[] result=new byte[plain.Length];provider.ReadExactly(result);return result;}
      for(int warm=0;warm<2;++warm){Baseline();C.Decompress(packed,new C.DecodeOptions(C.Format.Deflate));}
      var clock=Stopwatch.StartNew();for(int trial=0;trial<5;++trial)Require(Baseline().SequenceEqual(plain),"Baseline mismatch");long baseline=clock.ElapsedTicks;
      clock.Restart();for(int trial=0;trial<5;++trial)Require(C.Decompress(packed,new C.DecodeOptions(C.Format.Deflate)).SequenceEqual(plain),"Measured output mismatch");long exact=clock.ElapsedTicks;
      var borrowed=new Source(packed);Require(C.Decompress(borrowed,new C.DecodeOptions(C.Format.Deflate)).SequenceEqual(plain),"Buffered source mismatch");
      Require(borrowed.Calls<100&&borrowed.Maximum<=32768,"Per-byte borrowed I/O bottleneck");
      Console.WriteLine($"PERF {name} plain={plain.Length} compressed={packed.Length} baseline_ticks={baseline} validated_ticks={exact} frequency={Stopwatch.Frequency} source_reads={borrowed.Calls}");
    }
  }
  // Run independent framing and actual generated native-codec checks on .NET 8.
  static void Main(string[] args){try{Console.WriteLine("Runtime "+Environment.Version);Reentrancy();int count=Fixtures(args[0]);byte[] payload=File.ReadAllBytes(Path.Combine(args[0],"plain.bin"));Models(args[0]);Admission();Bounds(payload);Lifecycle(payload);Performance(args[0]);Console.WriteLine("PASS "+count+" independent framing decisions, native bytes, bounds, cancellation and lifecycle");}catch(Exception failure){Console.Error.WriteLine(failure);Environment.Exit(1);}}
}
'''


def bits_frame(values):
    """Pack explicitly authored DEFLATE wire bits, least-significant bit first within each byte."""
    return bytes(sum(bit << offset for offset, bit in enumerate(values[index:index + 8]))
                 for index in range(0, len(values), 8))


def wire_bits(value, count):
    """Represent fixed-width RFC fields in their little-bit-endian transmission order."""
    return [(value >> bit) & 1 for bit in range(count)]


def fixed_symbol(symbol):
    """Emit the canonical fixed literal alphabet in its most-significant-code-bit order."""
    code, count = ((symbol + 48, 8) if symbol < 144 else (symbol - 144 + 400, 9)
                   if symbol < 256 else (symbol - 256, 7) if symbol < 280 else (symbol - 280 + 192, 8))
    return [(code >> bit) & 1 for bit in reversed(range(count))]


def extra_fixtures(directory):
    """Add independent grammar/history/window cases that ordinary provider output does not exercise."""
    rows = []

    def add(format_name, packed, plain=None):
        """Retain an explicit expected decision alongside the shared gzip/zlib/raw corpus."""
        stem = 'extent-' + str(len(rows))
        (directory / (stem + '.bin')).write_bytes(packed)
        if plain is not None:
            (directory / (stem + '.plain')).write_bytes(plain)
        rows.append('\t'.join((format_name, 'valid' if plain is not None else 'invalid', stem + '.bin',
                               stem + '.plain' if plain is not None else '-')))

    for packed in (b'\x07', b'\x01\x01\0\0\0x', bits_frame([1, 1, 0] + fixed_symbol(257) + [0] * 5 + fixed_symbol(256)),
                   bits_frame([1, 1, 0] + fixed_symbol(286) + fixed_symbol(256)),
                   bits_frame([1, 1, 0] + fixed_symbol(65) + fixed_symbol(257) + [1,1,1,1,0] + fixed_symbol(256))):
        add('DEFLATE', packed)
    dynamic = [1, 0, 1]
    add('DEFLATE', bits_frame(dynamic + wire_bits(30, 5) + [0] * 9))
    for lengths in ((1,1,1,1), (0,0,0,2)):
        add('DEFLATE', bits_frame(dynamic + [0] * 14 + sum((wire_bits(x, 3) for x in lengths), [])))
    add('DEFLATE', bits_frame(dynamic + [0] * 14 + sum((wire_bits(x, 3) for x in (1,0,0,1)), []) + [1] + [0] * 2))
    add('DEFLATE', bits_frame(dynamic + [0] * 14 + sum((wire_bits(x, 3) for x in (0,0,1,1)), []) + [1] + [1] * 7 + [1] + [1] * 7))
    # A complete code-length tree can describe an empty distance alphabet when no match occurs.
    order = [16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1]
    header = dynamic + [0] * 10 + wire_bits(14, 4) + sum((wire_bits(1 if x in (0,1) else 0, 3) for x in order), [])
    for literal, plain in ((False, b''), (True, b'A')):
        lengths = [int(i == 256 or literal and i == 65) for i in range(258)]
        body = header + lengths + ([0,1] if literal else [0])
        packed = bits_frame(body)
        if zlib.decompress(packed, -15) != plain:
            raise AssertionError('Independent empty-distance control is invalid')
        add('DEFLATE', packed, plain)
    missing_end = header + [int(i == 65) for i in range(258)] + [0]
    add('DEFLATE', bits_frame(missing_end))
    payload = bytes(range(256)) + b'\0' + b'\0\1\2'
    raw = bits_frame([1,1,0] + sum((fixed_symbol(value) for value in payload[:257]), []) +
                     fixed_symbol(257) + [1,0,0,0,0] + [0] * 7 + fixed_symbol(256))
    if zlib.decompress(raw, -15) != payload:
        raise AssertionError('Independent distance-257 control is invalid')
    for cmf, valid in ((0x78, True), (0x08, False), (0x88, False)):
        flags = (-cmf * 256) % 31
        packed = bytes((cmf, flags)) + raw + struct.pack('>I', zlib.adler32(payload))
        add('ZLIB', packed, payload if valid else None)
    encoder = zlib.compressobj(wbits=-15, strategy=zlib.Z_FIXED)
    plain = b'fixed alphabet' * 100
    add('DEFLATE', encoder.compress(plain) + encoder.flush(), plain)
    with (directory / 'fixtures.tsv').open('a', encoding='utf-8') as output:
        output.write('\n'.join(rows) + '\n')
    return len(rows)


def run(command, directory, environment):
    """Execute actual SDK/compiler tools without shell evaluation or suppressed diagnostics."""
    print('RUN', subprocess.list2cmdline([str(value) for value in command]), flush=True)
    subprocess.run([str(value) for value in command], cwd=directory, env=environment, check=True, timeout=300)


def generate(compiler, directory, schema=SCHEMA, output='Schema.cs', extra=()):
    """Generate the real C# model/helper and discover the selected collision-free container."""
    source = directory / 'model.serializer'
    source.write_text(schema, encoding='utf-8')
    destination = directory / output
    subprocess.run([str(compiler), '--input', str(source), '--language', 'csharp', '--output', str(destination),
                    '--csharp.namespace', 'SerializerGenerated', *extra], check=True)
    match = re.search(r'Generated C# compression helper name: (\w+)', destination.read_text(encoding='utf-8'))
    if not match:
        raise AssertionError('Actual generated C# compression API is absent')
    return match[1], destination.stat().st_size


def project(directory, sdk):
    """Pin the actual .NET 8 SDK and prevent restore from consulting remote package sources."""
    (directory / 'global.json').write_text(json.dumps({'sdk': {'version': sdk, 'rollForward': 'disable'}}))
    (directory / 'NuGet.Config').write_text('<configuration><packageSources><clear /></packageSources></configuration>')
    (directory / 'Compression.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup>'
        '<TargetFramework>net8.0</TargetFramework><LangVersion>12.0</LangVersion><OutputType>Exe</OutputType>'
        '<Nullable>enable</Nullable><TreatWarningsAsErrors>true</TreatWarningsAsErrors>'
        '<UseSharedCompilation>false</UseSharedCompilation><NuGetAudit>false</NuGetAudit>'
        '<EnableDefaultCompileItems>false</EnableDefaultCompileItems></PropertyGroup>'
        '<ItemGroup><Compile Include="*.cs" /></ItemGroup></Project>')


def main():
    """Compile actual emitted C# under net8.0, exercise framing and independently decode every output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--dotnet', default=shutil.which('dotnet'))
    args = parser.parse_args()
    if not args.dotnet:
        parser.error('The actual .NET 8 SDK/runtime is required')
    root, build = Path(__file__).resolve().parent.parent, args.build.resolve()
    if build == root or root in build.parents and 'out' not in build.relative_to(root).parts:
        parser.error('In-repository scratch must remain below ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    sdks = subprocess.check_output([args.dotnet, '--list-sdks'], text=True)
    versions = re.findall(r'^(8\.0\.\d+) ', sdks, re.M)
    if not versions:
        parser.error('Install a .NET 8 SDK; newer SDK compilation alone does not qualify this baseline')
    sdk = max(versions, key=lambda value: tuple(map(int, value.split('.'))))
    environment = os.environ.copy()
    environment.update(DOTNET_CLI_HOME=str(build / 'dotnet-home'), DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1',
                       DOTNET_CLI_TELEMETRY_OPTOUT='1', DOTNET_GENERATE_ASPNET_CERTIFICATE='false',
                       DOTNET_ADD_GLOBAL_TOOLS_TO_PATH='false', NUGET_PACKAGES=str(build / 'nuget'),
                       MSBUILDDISABLENODEREUSE='1')
    sizes = {}
    with tempfile.TemporaryDirectory(prefix='csharp-compression-', dir=build) as temporary:
        directory = Path(temporary)
        count = common_fixtures(directory) + extra_fixtures(directory)
        rng = random.Random(28291)
        (directory / 'perf-random.bin').write_bytes(rng.randbytes(1024 * 1024))
        (directory / 'perf-repeated.bin').write_bytes(b'A' * (1024 * 1024))
        prefix, sizes['normal'] = generate(args.compiler.resolve(), directory)
        (directory / 'Program.cs').write_text(CSHARP.replace('Schema.SerializerCompression', 'Schema.' + prefix), encoding='utf-8')
        project(directory, sdk)
        run([args.dotnet, 'build', '--configuration', 'Release', '--nologo'], directory, environment)
        run([args.dotnet, 'bin/Release/net8.0/Compression.dll', directory], directory, environment)
        for protocol in ('JSON', 'BINARY_NONE', 'BINARY_INTEGER', 'BINARY_STRING'):
            native = (directory / ('native-' + protocol + '.bin')).read_bytes()
            for name, bits in (('None', None), ('Gzip',31), ('Zlib',15), ('Deflate',-15)):
                packed = (directory / ('output-' + name + '-' + protocol + '.bin')).read_bytes()
                if bits is None:
                    actual = packed
                else:
                    decoder = zlib.decompressobj(bits)
                    actual = decoder.decompress(packed) + decoder.flush()
                    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                        raise AssertionError('C# output did not contain exactly one independently accepted frame')
                if actual != native:
                    raise AssertionError('Compressed output changed native model bytes')
        if json.loads((directory / 'native-JSON.bin').read_bytes())['account_id'] != 42:
            raise AssertionError('Native JSON field identity changed')
        for name, schema, output, extra in (
                ('collision','serializer version 1; class SerializerCompression {} class SerializerCompression_2 {} class AccountRecord {public uint32 AccountId (1);}', 'Schema.cs', ('--csharp.naming','preserve')),
                ('binary',SCHEMA,'Schema.cs',('--protocols','binary_none')),
                ('outer',SCHEMA,'SerializerCompression.cs',()),
                ('outer-format',SCHEMA,'Format.cs',())):
            case = directory / name
            case.mkdir()
            family, sizes[name] = generate(args.compiler.resolve(), case, schema, output, extra)
            if name == 'collision' and family != 'SerializerCompression_3':
                raise AssertionError('Existing schema helper names were not preserved')
            unit = Path(output).stem
            smoke = ('using System; using S=SerializerGenerated.' + unit + '; using C=SerializerGenerated.' + unit + '.' + family + ';'
                     'static class Program {static void Main(){try{var value=new S.AccountRecord{AccountId=7};'
                     'byte[] packed=C.Encode(()=>value.Encode(S.Protocol.BINARY_NONE),new C.EncodeOptions(C.Format.Gzip));'
                     'var result=C.Decode(packed,bytes=>S.AccountRecord.Decode(bytes,S.Protocol.BINARY_NONE),new C.DecodeOptions(C.Format.Gzip));'
                     'if(result.AccountId!=7)throw new Exception("Candidate changed");')
            if name == 'binary':
                smoke += 'try{C.Encode(()=>value.Encode(S.Protocol.JSON));throw new Exception("Disabled protocol accepted");}catch(ArgumentException){}'
            (case / 'Program.cs').write_text(smoke + '}catch(Exception failure){Console.Error.WriteLine(failure);Environment.Exit(1);}}}', encoding='utf-8')
            project(case, sdk)
            run([args.dotnet,'build','--configuration','Release','--nologo'],case,environment)
            run([args.dotnet,'bin/Release/net8.0/Compression.dll'],case,environment)
    report = {'sdk': sdk, 'target': 'net8.0', 'independent_input_decisions': count,
              'independent_output_decisions': 16, 'generated_bytes': sizes,
              'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest()}
    (build / 'verification.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))
    print('PASS: actual generated C# exact compression, native bytes, .NET 8 policies and lifecycle', flush=True)


if __name__ == '__main__':
    main()
