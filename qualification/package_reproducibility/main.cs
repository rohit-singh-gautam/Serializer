// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using P = SerializerGenerated.Schema.SerializerPackage;

// Qualify existing public physical writers without sorting inventories in the adapter.
static class Producer {
  sealed record Part(string Name,string Media,byte[] Bytes);

  // Identify the actual managed compression implementation separately from generated metadata policy.
  static object Provider() => new {
    implementation="System.IO.Compression.DeflateStream",
    version=typeof(DeflateStream).Assembly.GetName().Version!.ToString(),
    framework=RuntimeInformation.FrameworkDescription,
    runtime_identifier=RuntimeInformation.RuntimeIdentifier,
    runtime_directory=RuntimeEnvironment.GetRuntimeDirectory(),
    assembly_path=typeof(DeflateStream).Assembly.Location,
    options=new {level="Optimal",window="provider default",strategy="provider default",wrapper="raw-deflate"},
    metadata_method=8
  };

  // Read exact UTF8 fields, retaining an empty final payload cell and literal URI suffixes.
  static List<string[]> ReadRows(string path,int fields) {
    var result=new List<string[]>();
    foreach(string line in File.ReadLines(path,new UTF8Encoding(false,true))) {
      string[] row=line.Split('\t');
      if(row.Length!=fields)throw new FormatException("Invalid TSV field count");
      result.Add(row);
    }
    return result;
  }

  // Reorder each caller inventory according to the frozen six-case contract.
  static List<T> Permute<T>(IReadOnlyList<T> input,int permutation) {
    var indexes=Enumerable.Range(0,input.Count).ToList();
    if(permutation==1)indexes.Reverse();
    else if(permutation==2||permutation==3) {
      int offset=permutation==2?1:input.Count/2;
      indexes=indexes.Skip(offset).Concat(indexes.Take(offset)).ToList();
    } else if(permutation==4)indexes=indexes.Where(index=>index%2==0).Concat(indexes.Where(index=>index%2!=0)).ToList();
    else if(permutation==5)indexes=indexes.Where(index=>index%2!=0).Reverse().Concat(indexes.Where(index=>index%2==0).Reverse()).ToList();
    return indexes.Select(index=>input[index]).ToList();
  }

  // Produce one complete archive through public ZIP or opaque OPC composition and atomic file Save.
  static void Produce(string[] args) {
    if(args.Length<4||args.Length>5)throw new ArgumentException("Expected fixture directory, profile, permutation, output and optional force64");
    string profile=args[1];
    if(!new[]{"zip-stored","zip-deflate","opc-stored","opc-deflate"}.Contains(profile))throw new ArgumentException("Unknown profile");
    int permutation=int.Parse(args[2],System.Globalization.CultureInfo.InvariantCulture);
    if(permutation<0||permutation>=6)throw new ArgumentException("Unknown permutation");
    if(args.Length==5&&args[4]!="0"&&args[4]!="1")throw new ArgumentException("Force64 must be literal 0 or 1");
    bool force=args.Length==5&&args[4]=="1",opc=profile.StartsWith("opc-",StringComparison.Ordinal);
    string storage=profile.EndsWith("-stored",StringComparison.Ordinal)?"stored":"deflate";
    string directory=Path.Combine(args[0],opc?"opc":"zip");
    var parts=Permute(ReadRows(Path.Combine(directory,"parts.tsv"),4)
      .Select(row=>new Part(row[0],row[1],Convert.FromHexString(row[3]))).ToList(),permutation);
    var links=Permute(ReadRows(Path.Combine(directory,"links.tsv"),5),permutation);
    var budget=new P.Budget(new P.PackageLimits{ForceZip64=force});
    byte[] bytes;
    if(opc) {
      var payloads=parts.Select(part=>new P.BinaryPart(part.Bytes,part.Name,part.Media,0,storage)).ToList();
      var relationships=links.Select(row=>new P.Relationship(row[0],row[1],row[2],row[3],row[4])).ToList();
      bytes=new P.Composer(budget).Finish(new P.Extensions(payloads,relationships));
    } else {
      using var writer=new P.ZipWriter(budget);
      foreach(Part part in parts)writer.Add(part.Name,part.Bytes,storage);
      bytes=writer.Finish();
    }
    P.Save(args[3],bytes,budget);
    Console.WriteLine(JsonSerializer.Serialize(new {profile,permutation,force_zip64=force,parts=parts.Count,relationships=links.Count,provider=Provider()}));
  }

  // Catch SDK failures explicitly so Windows error-reporting UI cannot mask a failed qualification.
  static int Main(string[] args) {
    try {
      if(args.Length==1&&args[0]=="--provider")Console.WriteLine(JsonSerializer.Serialize(Provider()));
      else Produce(args);
      return 0;
    } catch(Exception failure) {
      Console.Error.WriteLine(failure.GetType().FullName+": "+failure.Message);
      return 1;
    }
  }
}
