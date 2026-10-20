// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Text;
using S = SchemaGenerated.Schema;
using P = SchemaGenerated.Schema.SerializerPackage;
using A = SchemaGenerated.Schema.ProfileArchive;
using C = SchemaGenerated.Schema.ProfileCollectionArchive;
using I = IdentityGenerated.Identity;
using IP = IdentityGenerated.Identity.SerializerPackage;
using PA = ParameterizedGenerated.Parameterized.ProfileArchive;

// Qualify actual public generated owning APIs with independent fixture expectations.
internal static class Program {
  private static int passed,failed;
  private static string root="";

  // Reject a false independent acceptance decision.
  private static void Require(bool value,string message){if(!value)throw new Exception(message);}

  // Complete independent decisions while keeping every failure visible.
  private static void Test(string name,Action action){try{action();++passed;}catch(Exception error){++failed;Console.Error.WriteLine("FAIL "+name+": "+error);}}

  // Capture an expected failure without substituting an implementation codec.
  private static Exception Failure(Action action){try{action();}catch(Exception error){return error;}throw new Exception("Unexpected success");}

  // Map public diagnostic names to the maintained corpus spelling.
  private static string CodeName(Enum code)=>System.Text.RegularExpressions.Regex.Replace(code.ToString(),"([a-z])([A-Z])","$1_$2").ToUpperInvariant();

  // Require the independently expected structured failure category.
  private static void Code(Exception error,string expected){string? actual=error is P.PackageError package?CodeName(package.Code):error is ParameterizedGenerated.Parameterized.SerializerPackage.PackageError parameterized?CodeName(parameterized.Code):null;Require(actual!=null&&expected.Split(',').Contains(actual),"Expected "+expected+"; observed "+error);}

  // Relax only logical offset and count ceilings for boundary fixtures.
  private static P.PackageLimits Generous()=>new(){MaxArchiveBytes=1L<<34,MaxEntries=100000,MaxMetadataBytes=128L<<20,MaxBufferBytes=256L<<20,MaxDecoderAllocationBytes=512L<<20};

  // Select matching optional-part and relationship policies.
  private static P.ExtensionPolicy Policy(P.Unknown value)=>new(value,value);
  // Record legal short reads and enforce the parser's independent callback extents.
  private class Source:P.IRandomSource {
    internal readonly byte[] Bytes;internal int Calls,Largest;internal long Actual;internal Action? Callback;internal Exception? Error;internal bool Oversize,Empty;

  // Borrow independently authored bytes without assuming full reads.
  internal Source(byte[] bytes){Bytes=bytes;}
    public long Length=>Bytes.Length;

  // Return independently bounded short reads and expose callback boundaries.
  public virtual byte[] ReadAt(long offset,int maximum){Require(offset>=0&&offset<=Length&&maximum>0&&maximum<=Length-offset&&maximum<=32768,"Unchecked source span");++Calls;Largest=Math.Max(Largest,maximum);if(Error!=null)throw Error;Callback?.Invoke();int count=Empty?0:Oversize?maximum+1:Math.Min(17,maximum);Actual+=count;return Oversize?new byte[count]:Bytes.AsSpan((int)offset,count).ToArray();}
  }
  // Synthesize holes without allocating the independently authored logical ZIP64 extent.
  private sealed class Segments:P.IRandomSource {
    public long Length{get;}private readonly List<(long,byte[])> segments=new();internal long Actual;

  // Read only tiny authored segments, never the logical ZIP64 holes.
  internal Segments(string path){using var input=new BinaryReader(File.OpenRead(path));Require(input.ReadBytes(8).SequenceEqual(new byte[]{83,90,73,80,54,52,0,1}),"Segment signature");Length=input.ReadInt64();int count=input.ReadInt32();Require(count>=0&&count<16,"Segment count");for(int index=0;index<count;++index){long offset=input.ReadInt64(),size=input.ReadInt64();Require(size>=0&&size<1048576,"Segment extent");byte[] bytes=input.ReadBytes((int)size);Require(bytes.Length==size,"Truncated segments");segments.Add((offset,bytes));}Require(input.BaseStream.Position==input.BaseStream.Length,"Trailing descriptor");}

  // Return independently bounded short reads and expose callback boundaries.
  public byte[] ReadAt(long offset,int maximum){Require(offset>=0&&offset<=Length&&maximum>0&&maximum<=Length-offset&&maximum<=32768,"Sparse callback bounds");int count=Math.Min(17,maximum);Actual+=count;Require(Actual<1048576,"Materialized logical hole");byte[] result=new byte[count];foreach(var(start,bytes)in segments){long first=Math.Max(start,offset),last=Math.Min(start+bytes.Length,offset+count);if(first<last)Buffer.BlockCopy(bytes,(int)(first-start),result,(int)(first-offset),(int)(last-first));}return result;}
  }

  // Force complete exact payload and integrity checks through public ZIP APIs.
  private static void Zip(object input,string name="",string hex=""){using var archive=P.ZipReader.Open(input,new(Generous()));foreach(var entry in archive.Entries)Require(archive.Read(entry.Name).LongLength==entry.ExpandedBytes,"Expanded length");if(name.Length!=0)Require(archive.Read(name).SequenceEqual(Convert.FromHexString(hex)),"Owning payload");}

  // Evaluate one independent fixture decision through actual generated APIs.
  private static void Row(string[] row){string path=Path.Combine(root,"fixtures",row[2]);Action action=()=>{switch(row[1]){
    case "zip":Zip(File.ReadAllBytes(path),row[4],row[5]);break;
    case "segment":var segments=new Segments(path);Zip(segments,row[4],row[5]);Require(segments.Actual<1048576,"Sparse I/O");break;
    case "count":using(var archive=P.ZipReader.Open(path,new(Generous()))){int count=int.Parse(row[4]);Require(archive.Entries.Count==count,"Narrowed count");for(int index=0;index<count;++index)Require(archive.Entries[index].Name=="part"+index,"Entry order");Require(archive.Read("part"+(count-1)).Length==0,"Last entry");}break;
    case "collection":Require(C.Load(path).Value.Models.Count==3,"Transactional collection");break;
    case "parameterized":Require(PA.Load(path).Value.Main!.Name==row[5],"Parameterized role");break;
    default:var mode=row[1]=="preserve"?P.Unknown.Preserve:row[1]=="skip"?P.Unknown.Skip:P.Unknown.Reject;var value=A.Load(path,policy:Policy(mode)).Value;Require(value.Main!.Name==row[5],"Typed model");Require(value.Attachments.Count==1&&value.Attachments[0].Data.SequenceEqual(new byte[]{0,1,254,255}),"Binary payload");break;
  }};if(row[3]=="OK")action();else Code(Failure(action),row[3]);}

  // Construct public owning values for output, publication and exchange checks.
  private static A.Input Model()=>new(){Main=new S.ProfileModel{Name="generated & checked",Points=new(){new S.ProfilePoint{X=1,Y=2,Z=3}}},Attachments=new[]{new P.BinaryPart(new byte[]{0,1,254,255},"/Textures/raw.bin","application/octet-stream")}};
  // Borrowed streams expose lifecycle and final-callback controls without package implementation access.
  private sealed class Sink:MemoryStream {internal bool Closed,Flushed;internal Action? Callback;
  // Observe the last borrowed sink callback without transferring ownership.
  public override void Write(byte[] bytes,int offset,int count){base.Write(bytes,offset,count);Callback?.Invoke();}
  // Record any forbidden caller-sink flush.
  public override void Flush(){Flushed=true;base.Flush();}
  // Observe borrowed resource ownership and release only test-owned backing state.
  protected override void Dispose(bool disposing){Closed=true;base.Dispose(disposing);}}
  private sealed class Pipe:Stream {private readonly MemoryStream input;internal bool Closed;internal Action? Callback;
  // Expose an intentionally nonseekable borrowed stream.
  internal Pipe(byte[] bytes){input=new(bytes);}public override bool CanRead=>true;public override bool CanSeek=>false;public override bool CanWrite=>false;public override long Length=>throw new NotSupportedException();public override long Position{get=>throw new NotSupportedException();set=>throw new NotSupportedException();}
  // Return bounded sequential data while exposing cancellation boundaries.
  public override int Read(byte[] bytes,int offset,int count){int size=input.Read(bytes,offset,Math.Min(count,19));Callback?.Invoke();return size;}
  // Record any forbidden caller-sink flush.
  public override void Flush()=>throw new NotSupportedException();
  // Reject seek attempts on the independent nonseekable source.
  public override long Seek(long n,SeekOrigin origin)=>throw new NotSupportedException();
  // Reject mutation attempts on the independent borrowed source.
  public override void SetLength(long n)=>throw new NotSupportedException();
  // Observe the last borrowed sink callback without transferring ownership.
  public override void Write(byte[] bytes,int offset,int count)=>throw new NotSupportedException();
  // Observe borrowed resource ownership and release only test-owned backing state.
  protected override void Dispose(bool disposing){Closed=true;input.Dispose();base.Dispose(disposing);}}
  private sealed class Marker:P.PackageError {
  // Create a distinctive caller package-error subtype for identity controls.
  internal Marker():base(P.Code.Codec,"caller marker"){} }

  // Set exactly one cumulative logical resource ceiling.
  private static P.PackageLimits Ceiling(P.Resource resource,long maximum)=>resource switch{P.Resource.IoBytes=>new(){MaxIoBytes=maximum},P.Resource.MetadataBytes=>new(){MaxMetadataBytes=maximum},P.Resource.ExpandedBytes=>new(){MaxTotalExpandedBytes=maximum},P.Resource.Entries=>new(){MaxEntries=maximum},P.Resource.Relationships=>new(){MaxRelationships=maximum},P.Resource.DecoderAllocationBytes=>new(){MaxDecoderAllocationBytes=maximum},P.Resource.WorkUnits=>new(){MaxWorkUnits=maximum},P.Resource.References=>new(){MaxReferences=maximum},P.Resource.PreservedBytes=>new(){MaxPreservedBytes=maximum},_=>throw new Exception("resource")};
  // Retain independent pass/fail decisions even when another fixture exposes a defect.
  private static int Main(string[] arguments){if(arguments[0]=="--exchange"){var value=ExampleGenerated.Example.OpcExampleDocument.Load(arguments[1]).Value;Require(value.Main!.Title=="Parts & tools"&&value.Main.Items.Count==2&&value.Main.Items[0].Id==7&&value.Main.Items[0].Label=="Bolt <M6>"&&value.Main.Items[1].Id==8&&value.Main.Items[1].Label=="Nut"&&value.Attachments.Count==1,"Native exchange values");File.WriteAllBytes(arguments[2],ExampleGenerated.Example.OpcExampleDocument.Encode(value));return 0;}root=arguments[0];foreach(string line in File.ReadAllLines(Path.Combine(root,"fixtures/cases.tsv"))) {var row=line.Split('\t');Test(row[0],()=>Row(row));}
    byte[] valid=File.ReadAllBytes(Path.Combine(root,"fixtures/profile-descriptor-0-wide-0.zip"));
    foreach(bool wide in new[]{false,true})foreach(bool descriptor in new[]{false,true})Test("independent output "+wide+descriptor,()=>{var budget=new P.Budget(new(){ForceZip64=wide,Descriptor=descriptor});using(var writer=new P.ZipWriter(budget)){writer.Add("stored.bin",new byte[]{0,1,2});writer.Add("empty.bin",Array.Empty<byte>(),"deflate");writer.Add("deflated.bin",Encoding.UTF8.GetBytes(new string('z',90000)),"deflate");File.WriteAllBytes(Path.Combine(root,$"produced-{wide}-{descriptor}.zip"),writer.Finish());}File.WriteAllBytes(Path.Combine(root,$"produced-{wide}-{descriptor}.opc"),A.Encode(Model(),new(new(){ForceZip64=wide,Descriptor=descriptor})));});
    Test("helper collision XML-independent package",()=>{var input=new CollisionGenerated.Collision.Bundle.Input{attachments=new[]{new CollisionGenerated.Collision.SerializerPackage_3.BinaryPart(new byte[]{7},"/part.bin","application/octet-stream")}};byte[] bytes=CollisionGenerated.Collision.Bundle.Encode(input);Require(CollisionGenerated.Collision.Bundle.Load(bytes).Value.attachments[0].Data[0]==7,"Collision adapter failed");});
    Test("bounded borrowed short reads",()=>{var source=new Source(valid);var value=A.Load(source).Value;Require(value.Main!.Name=="independent"&&source.Calls>10&&source.Largest<=32768,"Source boundaries");});
    foreach(bool oversized in new[]{false,true})Test("invalid source count "+oversized,()=>{var source=new Source(valid){Oversize=oversized,Empty=!oversized};Code(Failure(()=>P.ZipReader.Open(source)),"IO");});
    Test("source callback identity",()=>{var marker=new Marker();Require(ReferenceEquals(marker,Failure(()=>P.ZipReader.Open(new Source(valid){Error=marker}))),"Source subtype replaced");});
    Test("borrowed seekable extent",()=>{using var stream=new MemoryStream();stream.Write(new byte[9]);stream.Write(valid);stream.Position=9;Require(A.Load(stream).Value.Main!.Name=="independent"&&stream.CanRead,"Seekable base/ownership");});
    Test("archive bound before callback",()=>{var source=new Source(valid);Code(Failure(()=>P.ZipReader.Open(source,new(new(){MaxArchiveBytes=valid.Length-1}))),"RESOURCE_LIMIT");Require(source.Calls==0,"Unadmitted callback");});
    Test("failed output capacity releases shared hold",()=>{bool fail=true;var marker=new Marker();var budget=new P.Budget(new(){MaxBufferBytes=512,IsCancelled=()=>{if(fail&&new System.Diagnostics.StackTrace().GetFrames().Any(frame=>frame.GetMethod()?.Name=="Consume"&&frame.GetMethod()?.DeclaringType?.Name=="State")&&new System.Diagnostics.StackTrace().GetFrames().Any(frame=>frame.GetMethod()?.Name=="Ensure"))throw marker;return false;}});using(var writer=new P.ZipWriter(budget)){Require(ReferenceEquals(marker,Failure(()=>writer.Finish())),"Capacity callback changed");}fail=false;using var retry=new P.ZipWriter(budget);Require(retry.Finish().Length==22,"Released candidate retained shared buffer capacity");});
    Test("closed empty reader rejects validation",()=>{using var writer=new P.ZipWriter();byte[] empty=writer.Finish();var archive=P.ZipReader.Open(empty);archive.Dispose();Code(Failure(()=>archive.ValidateAll()),"INVALID_STATE");});
    Test("monotonic deadline before source",()=>{var source=new Source(valid);Code(Failure(()=>A.Load(source,new(new(){DeadlineTimestamp=System.Diagnostics.Stopwatch.GetTimestamp()-1}))),"DEADLINE_EXCEEDED");Require(source.Calls==0,"Expired operation observed source");});
    Test("snapshot source",()=>{byte[] owned=(byte[])valid.Clone();using var archive=P.ZipReader.Open(owned);Array.Clear(owned);archive.ValidateAll();});
    Test("sink complete borrowed",()=>{var sink=new Sink();A.Write(sink,Model());Require(!sink.Closed&&!sink.Flushed&&sink.Length>0,"Sink ownership");});
    Test("sink last callback cancellation",()=>{bool cancel=false;var sink=new Sink{Callback=()=>cancel=true};Code(Failure(()=>A.Write(sink,Model(),new(new(){IsCancelled=()=>cancel}))),"CANCELLED");Require(!sink.Closed&&!sink.Flushed,"Cancelled borrowed sink");});
    Test("sink callback identity",()=>{var marker=new Marker();var sink=new Sink{Callback=()=>throw marker};Require(ReferenceEquals(marker,Failure(()=>A.Write(sink,Model()))),"Sink subtype replaced");});
    Test("writer reentry terminal",()=>{using var writer=new P.ZipWriter();writer.Add("x",new byte[]{1});var sink=new Sink{Callback=()=>{try{writer.Add("nested",Array.Empty<byte>());}catch(P.PackageError){}}};Code(Failure(()=>writer.WriteTo(sink)),"INVALID_STATE");Code(Failure(()=>writer.Finish()),"INVALID_STATE");});
    Test("second admission callback closure before sink",()=>{P.ZipWriter? writer=null;int checks=0;var budget=new P.Budget(new(){IsCancelled=()=>{var frames=new System.Diagnostics.StackTrace().GetFrames();if(frames.Any(frame=>frame.GetMethod()?.Name=="WriteCore")&&frames.Any(frame=>frame.GetMethod()?.Name=="Consume"&&frame.GetMethod()?.DeclaringType?.Name=="State")&&++checks==2)writer!.Dispose();return false;}});writer=new(budget);using(writer){writer.Add("part",new byte[]{1});var sink=new Sink();Code(Failure(()=>writer.WriteTo(sink)),"INVALID_STATE");Require(sink.Length==0,"Closed operation emitted bytes after callback");}});
    Test("source close terminal",()=>{var source=new Source(valid);using var reader=P.ZipReader.Open(source);source.Callback=()=>reader.Dispose();Code(Failure(()=>reader.Read("Alternate/root.xml")),"INVALID_STATE");});
    Test("spool explicit policy",()=>{var pipe=new Pipe(valid);Code(Failure(()=>P.ZipReader.Open(pipe)),"INVALID_STATE");Require(!pipe.Closed,"Borrowed pipe closed");});
    Test("spool bounded cleanup",()=>{var pipe=new Pipe(valid);using(var archive=P.ZipReader.Open(pipe,new(new(){AllowSpool=true,SpoolDirectory=root,MaxSpoolBytes=valid.Length}))){archive.ValidateAll();}Require(!pipe.Closed&&!Directory.EnumerateFiles(root,".serializer-zip-*").Any(),"Spool lifecycle");});
    Test("spool one tight",()=>{Code(Failure(()=>P.ZipReader.Open(new Pipe(valid),new(new(){AllowSpool=true,SpoolDirectory=root,MaxSpoolBytes=valid.Length-1}))),"RESOURCE_LIMIT");Require(!Directory.EnumerateFiles(root,".serializer-zip-*").Any(),"Failed spool leaked");});
    Test("spool cancellation cleanup",()=>{bool cancel=false;var pipe=new Pipe(valid){Callback=()=>cancel=true};Code(Failure(()=>P.ZipReader.Open(pipe,new(new(){AllowSpool=true,SpoolDirectory=root,IsCancelled=()=>cancel}))),"CANCELLED");Require(!pipe.Closed&&!Directory.EnumerateFiles(root,".serializer-zip-*").Any(),"Cancelled spool leaked");});
    foreach(string stage in new[]{"before_create","after_create","before_sync","before_publish","after_publish"})Test("atomic Save "+stage,()=>{string path=Path.Combine(root,"save-"+stage+".opc");byte[] old=Encoding.UTF8.GetBytes("old complete generation");File.WriteAllBytes(path,old);var marker=new Marker();var observed=Failure(()=>A.Save(path,Model(),fault:point=>{if(point==stage)throw marker;}));if(stage=="after_publish"){Code(observed,"PUBLICATION_UNCERTAIN");Require(ReferenceEquals(observed.InnerException,marker),"Published cause changed");Require(A.Load(path).Value.Main!.Name=="generated & checked","Incomplete publication");}else{Require(ReferenceEquals(observed,marker),"Prepublication exception changed");Require(File.ReadAllBytes(path).SequenceEqual(old),"Old generation changed");}Require(!Directory.EnumerateFiles(root,".serializer-opc-*").Any(),"Staging leaked");});
    Test("Save cleanup preserves caller exception",()=>{string path=Path.Combine(root,"save-cleanup.opc");File.WriteAllText(path,"old");var marker=new Marker();FileStream? held=null;string? retained=null;try{var observed=Failure(()=>A.Save(path,Model(),fault:point=>{if(point=="before_publish"){retained=Directory.EnumerateFiles(root,".serializer-opc-*").Single();held=new FileStream(retained,FileMode.Open,FileAccess.Read,FileShare.None);throw marker;}}));Require(ReferenceEquals(observed,marker),"Cleanup masked original callback exception");Require(File.ReadAllText(path)=="old","Cleanup failure published");if(OperatingSystem.IsWindows())Require(marker.Data["SerializerCleanupFailure"] is IOException,"Cleanup failure not exposed");}finally{held?.Dispose();if(retained!=null)File.Delete(retained);}});
    Test("Save validator before publication",()=>{string path=Path.Combine(root,"save-validator.opc");File.WriteAllText(path,"old");var marker=new Marker();Require(ReferenceEquals(marker,Failure(()=>A.Save(path,Model(),validate:(_,_)=>throw marker))),"Validator callback changed");Require(File.ReadAllText(path)=="old","Validator published");});
    foreach(P.Resource resource in Enum.GetValues<P.Resource>())Test("shared exact ceiling "+resource,()=>{string path=Path.Combine(root,"fixtures/extension.zip");var measured=new P.Budget();A.Load(path,measured,Policy(P.Unknown.Preserve));long used=measured.Used(resource);Require(used>0,"Uncharged resource "+resource);A.Load(path,new(Ceiling(resource,used)),Policy(P.Unknown.Preserve));Code(Failure(()=>A.Load(path,new(Ceiling(resource,used-1)),Policy(P.Unknown.Preserve))),"RESOURCE_LIMIT");});
    Test("transactional aggregate validator",()=>{int published=0;Code(Failure(()=>C.Load(Path.Combine(root,"fixtures/collection-middle-invalid.zip"),validate:(_,_)=>++published)),"CODEC");Require(published==0,"Partial native aggregate published");});
    Test("required opaque part",()=>Code(Failure(()=>A.Load(Path.Combine(root,"fixtures/extension.zip"),policy:new(P.Unknown.Preserve,P.Unknown.Preserve,IsRequiredPart:_=>true))),"UNSUPPORTED_FEATURE"));
    Test("required opaque relationship",()=>Code(Failure(()=>A.Load(Path.Combine(root,"fixtures/external-iris.zip"),policy:new(P.Unknown.Preserve,P.Unknown.Preserve,IsRequiredRelationship:_=>true))),"UNSUPPORTED_FEATURE"));
    Test("opaque roundtrip edited graph",()=>{var result=A.Load(Path.Combine(root,"fixtures/extension.zip"),policy:Policy(P.Unknown.Preserve));Require(result.Extensions.Parts.Count>0,"Opaque part missing");result.Value.Main!.Name="edited";byte[] bytes=A.Encode(result.Value);Require(A.Load(bytes,policy:Policy(P.Unknown.Preserve)).Value.Main!.Name=="edited","Native edit lost");var altered=new A.Input{Main=result.Value.Main,Attachments=result.Value.Attachments,Extensions=new P.Extensions(result.Extensions.Parts,result.Extensions.Relationships.Concat(new[]{new P.Relationship("/","bad","urn:bad","/missing.bin")}))};Code(Failure(()=>A.Encode(altered)),"MISSING_PART");});
    Test("fixed role relocation context",()=>{var value=A.Load(Path.Combine(root,"fixtures/relocation.opc"),policy:Policy(P.Unknown.Preserve)).Value;byte[] bytes=A.Encode(value);var edited=A.Load(bytes,policy:Policy(P.Unknown.Preserve));var link=edited.Extensions.Relationships.Single(item=>item.Id=="opaque-back");Require(link.Source=="/3D/model.xml"&&link.Target=="/3D/model.xml?mode=raw#kept","Preserved source/selector context lost");});
    Test("private spool exclusivity",()=>{bool observed=false;var pipe=new Pipe(valid){Callback=()=>{foreach(string path in Directory.EnumerateFiles(root,".serializer-zip-*")){observed=true;if(OperatingSystem.IsWindows())Require(Failure(()=>{using var file=File.OpenRead(path);}) is IOException,"Spool not private/exclusive");else Require((File.GetUnixFileMode(path)&(UnixFileMode.GroupRead|UnixFileMode.GroupWrite|UnixFileMode.OtherRead|UnixFileMode.OtherWrite))==0,"Spool permissions");}}};using var archive=P.ZipReader.Open(pipe,new(new(){AllowSpool=true,SpoolDirectory=root}));Require(observed,"No private spool observed");});
    Test("Unicode external identity roundtrip",()=>{var value=A.Load(Path.Combine(root,"fixtures/external-iris.zip"),policy:Policy(P.Unknown.Preserve)).Value;File.WriteAllBytes(Path.Combine(root,"external-roundtrip.opc"),A.Encode(value));});
    Test("canonical permutations",()=>{var first=Model();first.Extensions=new(new[]{new P.BinaryPart(new byte[]{7},"/Extras/z.bin","application/octet-stream"),new P.BinaryPart(new byte[]{8},"/Extras/a.bin","application/octet-stream")},new[]{new P.Relationship("/","z","urn:vendor","/Extras/z.bin"),new P.Relationship("/","a","urn:vendor","/Extras/a.bin")});var last=Model();last.Extensions=new(first.Extensions.Parts.Reverse(),first.Extensions.Relationships.Reverse());Require(A.Encode(first).SequenceEqual(A.Encode(last)),"Input order changed canonical bytes");});
    Test("XML package field attribution",()=>{bool found=false;for(int maximum=0;maximum<5000&&!found;++maximum){var error=Failure(()=>A.Encode(Model(),new(new(){MaxWorkUnits=maximum})));if(error is P.PackageError failure&&failure.Code==P.Code.ResourceLimit&&failure.FieldPath=="main.name"){Require(failure.Part=="/3D/model.xml"&&failure.RelationshipId=="srl_main_0","Lost native field/link context");found=true;}}Require(found,"Native member work not shared");});
    Test("physical typed role attribution",()=>{long envelope=0,main=0;using(var archive=P.ZipReader.Open(valid)){foreach(var entry in archive.Entries)if(entry.Name.EndsWith(".rels",StringComparison.Ordinal)||entry.Name=="[Content_Types].xml")envelope+=entry.ExpandedBytes;else if(entry.Name=="Alternate/root.xml")main=entry.ExpandedBytes;}var error=Failure(()=>A.Load(valid,new(new(){MaxTotalExpandedBytes=envelope+main-1})));Require(error is P.PackageError failure&&failure.Code==P.Code.ResourceLimit&&failure.Part=="/Alternate/root.xml"&&failure.FieldPath=="main"&&failure.RelationshipId=="main","Physical admission lost role/link: "+error);});
    Test("deferred input attribute attribution",()=>{P.Budget? measured=null;long before=-1;measured=new(new(){IsCancelled=()=>{if(before<0&&new System.Diagnostics.StackTrace().GetFrames().Any(frame=>frame.GetMethod()?.DeclaringType?.Name=="SrlXmlInputFields"&&frame.GetMethod()?.Name=="Attribute"))before=measured!.Used(P.Resource.WorkUnits);return false;}});A.Load(valid,measured);Require(before>=0,"Attribute presentation was not shared");var error=Failure(()=>A.Load(valid,new(new(){MaxWorkUnits=before})));Require(error is P.PackageError failure&&failure.Code==P.Code.ResourceLimit&&failure.Part=="/Alternate/root.xml"&&failure.FieldPath=="main.name"&&failure.RelationshipId=="main","Deferred attribute lost native field: "+error);});
    Test("provider callback XmlException identity",()=>{var marker=new System.Xml.XmlException("caller XML marker");var budget=new P.Budget(new(){IsCancelled=()=>{if(new System.Diagnostics.StackTrace().GetFrames().Any(frame=>frame.GetMethod()?.DeclaringType?.Name=="SrlXmlInput"&&frame.GetMethod()?.Name=="Advance"))throw marker;return false;}});Require(ReferenceEquals(marker,Failure(()=>A.Load(valid,budget))),"Provider catch translated caller XML exception");});
    Test("codec callback FormatException identity",()=>{var marker=new FormatException("caller format marker");var budget=new P.Budget(new(){IsCancelled=()=>{if(new System.Diagnostics.StackTrace().GetFrames().Any(frame=>frame.GetMethod()?.DeclaringType?.Name=="SrlXmlOutput"&&frame.GetMethod()?.Name=="Append"))throw marker;return false;}});Require(ReferenceEquals(marker,Failure(()=>A.Encode(Model(),budget))),"Codec catch translated caller format exception");});
    foreach(string method in new[]{"SelectField","NativeLimits"})Test("scope callback FormatException identity "+method,()=>{var marker=new FormatException("scope marker");var budget=new P.Budget(new(){IsCancelled=()=>{if(new System.Diagnostics.StackTrace().GetFrames().Any(frame=>frame.GetMethod()?.Name==method))throw marker;return false;}});Require(ReferenceEquals(marker,Failure(()=>A.Encode(Model(),budget))),"Scope catch translated original callback exception");});
    Test("metadata callback XmlException identity",()=>{var marker=new System.Xml.XmlException("metadata callback marker");int checks=0;var budget=new P.Budget(new(){IsCancelled=()=>{if(new System.Diagnostics.StackTrace().GetFrames().Any(frame=>frame.GetMethod()?.Name=="Metadata")&&++checks==2)throw marker;return false;}});Require(ReferenceEquals(marker,Failure(()=>A.Load(valid,budget))),"Metadata catch translated caller XML exception");});
    Test("QName package namespace admission",()=>{ScopedGenerated.Scoped.SerializerPackage.Budget? measured=null;long before=-1;measured=new(new(){IsCancelled=()=>{if(before<0&&new System.Diagnostics.StackTrace().GetFrames().Any(frame=>frame.GetMethod()?.Name=="NamespaceRetention"))before=measured!.Used(ScopedGenerated.Scoped.SerializerPackage.Resource.DecoderAllocationBytes);return false;}});var loaded=ScopedGenerated.Scoped.ScopedArchive.Load(Path.Combine(root,"fixtures/scoped.opc"),measured);Require(loaded.Value.Main!.First=="p:item"&&loaded.Value.Main.Second=="p:item"&&before>=0,"QName sidecar not shared");byte[] bytes=ScopedGenerated.Scoped.ScopedArchive.Encode(loaded.Value);File.WriteAllBytes(Path.Combine(root,"scoped-roundtrip.opc"),bytes);var error=Failure(()=>ScopedGenerated.Scoped.ScopedArchive.Load(Path.Combine(root,"fixtures/scoped.opc"),new(new(){MaxDecoderAllocationBytes=before})));Require(error is ScopedGenerated.Scoped.SerializerPackage.PackageError failure&&failure.Code==ScopedGenerated.Scoped.SerializerPackage.Code.ResourceLimit&&failure.Part=="/model.xml"&&failure.FieldPath=="main.first"&&failure.RelationshipId=="scope","Namespace retention lost field/link: "+error);});
    Test("callback package subtype unchanged",()=>{var marker=new Marker();bool armed=false;var limits=new P.PackageLimits{IsCancelled=()=>{if(armed)throw marker;return false;}};var budget=new P.Budget(limits);armed=true;Require(ReferenceEquals(marker,Failure(()=>A.Encode(Model(),budget))),"Cancellation subtype sliced");});
    Test("callback unrelated unchanged",()=>{var marker=new InvalidOperationException("marker");bool armed=false;var budget=new P.Budget(new(){IsCancelled=()=>{if(armed)throw marker;return false;}});armed=true;Require(ReferenceEquals(marker,Failure(()=>A.Load(valid,budget))),"Cancellation exception changed");});
    Test("identity cumulative tuple admission",()=>{var budget=new IP.Budget();var loaded=I.IdentityPackageArchive.Load(Path.Combine(root,"fixtures/identity.opc"),budget);Require(loaded.Value.Registries.Count==3,"Identity owner count");long references=budget.Used(IP.Resource.References);Require(references==15,"Missing complete tuple admission: "+references);var error=Failure(()=>I.IdentityPackageArchive.Load(Path.Combine(root,"fixtures/identity.opc"),new(new(){MaxReferences=14})));Require(error is IP.PackageError failure&&failure.Code==IP.Code.ResourceLimit&&failure.Part=="/Registries/registry2.xml"&&failure.RelationshipId=="record2"&&failure.FieldPath.StartsWith("registries.",StringComparison.Ordinal),"Missing constraint diagnostic: "+error);});
    Console.WriteLine(System.Text.Json.JsonSerializer.Serialize(new{passed,failed}));return failed==0?0:1;
  }
}
