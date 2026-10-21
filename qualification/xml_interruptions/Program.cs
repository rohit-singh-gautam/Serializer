// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.Diagnostics;
using System.IO;
using static SerializerGenerated.Schema;

// Qualify the public policy-copy wrapper with exact original exceptions and fresh recovery.
internal static class Program {
  // Execute six controls per maintained large input without retaining a failed candidate.
  private static void Main(string[] args) {
    for (int index=1;index<args.Length;++index) {
      byte[] input=File.ReadAllBytes(args[index]);
      var value=XmlExampleOrder.DecodeXml(input);
      byte[] canonical=value.Encode(Protocol.BINARY_NONE);
      int verified=0;
      foreach(bool writing in new[]{false,true}) foreach(int threshold in new[]{1,1000,0}) {
        int polls=0;long deadline=Stopwatch.GetTimestamp()-1;
        var marker=new InvalidOperationException("Caller XML interruption");
        var limits=new Limits().WithCancellation(()=>{
          if (++polls>=threshold&&threshold!=0||threshold==0&&Stopwatch.GetTimestamp()>=deadline) throw marker;
        });
        bool failed=false;
        try { if(writing)value.EncodeXml(limits);else XmlExampleOrder.DecodeXml(input,limits); }
        catch(InvalidOperationException failure) { if(!ReferenceEquals(failure,marker))throw;failed=true; }
        if(!failed||threshold!=0&&polls!=threshold)throw new Exception("Interruption returned a candidate");
        if(!canonical.AsSpan().SequenceEqual(value.Encode(Protocol.BINARY_NONE))||
           !canonical.AsSpan().SequenceEqual(XmlExampleOrder.DecodeXml(value.EncodeXml()).Encode(Protocol.BINARY_NONE)))throw new Exception("Recovery changed values");
        ++verified;
      }
      File.WriteAllBytes(Path.Combine(args[0],index+".wire"),canonical);
      Console.WriteLine($"{{\"fixture\":{index},\"verified_interruptions\":{verified},\"recovered\":true}}");
    }
  }
}
