// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.IO;
using System.Linq;
using System.Reflection;
using Schema = CompressionParity.Schema;
using C = CompressionParity.Schema.SerializerCompression;

// Execute the mandatory inventory with explicitly selected caller-owned managed providers.
static class Adapter {
  static C.Provider Zstd = null!, Lz4 = null!;
  // Preserve default core options and select optional capabilities only for their declared format.
  static C.EncodeOptions Encoding(C.Format format,int input=8388608,int output=8388608) => format == C.Format.Zstd || format == C.Format.Lz4
    ? C.EncodeOptions.WithProvider(format, format == C.Format.Zstd ? Zstd : Lz4, input, output, level:format == C.Format.Lz4 ? 0 : 3)
    : new C.EncodeOptions(format,input,output);
  // Apply the same exact input, expansion and history caps in every runtime.
  static C.DecodeOptions Decoding(C.Format format, int input=8388608, int output=8388608, int window=8388608) =>
    format == C.Format.Zstd || format == C.Format.Lz4
    ? C.DecodeOptions.WithProvider(format, format == C.Format.Zstd ? Zstd : Lz4, input, output, window)
    : new C.DecodeOptions(format,input,output,window);
  // Fail independently of SDK release assertion settings.
  static void Require(bool value, string message) { if (!value) throw new Exception(message); }
  // Replay every selected fixture and producer frame through actual generated model decoding.
  static void Main(string[] args) {
    Zstd = C.Provider.ZstdSharp(Assembly.LoadFrom(args[2])); Lz4 = C.Provider.Lz4(Assembly.LoadFrom(args[3]));
    string root=args[1];
    if(args[0]=="produce") {
      var value=new Schema.AccountRecord{AccountId=42,Label="shared \u03bb \U0001f642"};
      foreach(var protocol in Enum.GetValues<Schema.Protocol>())foreach(var format in Enum.GetValues<C.Format>()) {
        string stem="csharp-"+protocol+"-"+format.ToString().ToUpperInvariant();
        byte[] plain=value.Encode(protocol),frame=C.Encode(()=>value.Encode(protocol),Encoding(format));
        Require(frame.SequenceEqual(C.Encode(()=>value.Encode(protocol),Encoding(format,plain.Length,frame.Length))),"Exact encode admission");
        foreach(var bounds in new[]{new[]{plain.Length-1,frame.Length},new[]{plain.Length,frame.Length-1}}){
          try{C.Encode(()=>value.Encode(protocol),Encoding(format,bounds[0],bounds[1]));throw new Exception("One-tight encode admission");}
          catch(C.Error failure){Require(failure.Code==C.Code.ResourceLimit,"Wrong encode admission category");}
        }
        File.WriteAllBytes(Path.Combine(root,stem+".frame"),frame);
        File.WriteAllBytes(Path.Combine(root,stem+".plain"),plain);
      }
      Console.WriteLine("{\"produced\":24,\"encode_bounds\":72}");return;
    }
    int decisions=0,exchanges=0;
    foreach(string line in File.ReadAllLines(Path.Combine(root,"cases.tsv"))) {
      string[] row=line.Split('\t');string observed="ok";
      try { byte[] plain=C.Decompress(File.ReadAllBytes(Path.Combine(root,row[2])),Decoding(Enum.Parse<C.Format>(row[1],true),int.Parse(row[5]),int.Parse(row[6]),int.Parse(row[7])));
        if(row[4]=="ok")Require(plain.SequenceEqual(File.ReadAllBytes(Path.Combine(root,row[3]))),row[0]); }
      catch(C.Error failure){observed=failure.Code switch{C.Code.InvalidData=>"invalid_data",C.Code.ResourceLimit=>"resource_limit",_=>failure.Code.ToString()};}
      Require(observed==row[4],row[0]+": "+observed+" != "+row[4]);++decisions;
    }
    foreach(string line in File.ReadAllLines(Path.Combine(root,"exchange.tsv"))) {
      string[] row=line.Split('\t');var protocol=Enum.Parse<Schema.Protocol>(row[1]);var policy=Decoding(Enum.Parse<C.Format>(row[2],true));
      byte[] frame=File.ReadAllBytes(Path.Combine(root,row[3]));var value=C.Decode(frame,bytes=>Schema.AccountRecord.Decode(bytes,protocol),policy);
      Require(value.AccountId==42&&value.Label=="shared \u03bb \U0001f642",row[0]);
      Require(C.Decompress(frame,policy).SequenceEqual(File.ReadAllBytes(Path.Combine(root,row[4]))),row[0]);++exchanges;
    }
    Console.WriteLine("{\"decisions\":"+decisions+",\"exchanges\":"+exchanges+"}");
  }
}
