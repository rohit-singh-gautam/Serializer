// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagecs(
  /// <summary>Bounded synchronous ZIP/OPC operations; entry names never select filesystem paths.</summary>
  public static class @package_name@ {
    public enum Code { Unavailable, UnsupportedFeature, InvalidArchive, InvalidName, DuplicateName, Integrity, ResourceLimit, Cancelled, Io, InvalidState, InvalidMetadata, MissingPart, InvalidRelationship, Publication, PublicationUncertain, Codec, InvalidReference, Semantic, DeadlineExceeded }
    public enum Resource { IoBytes, MetadataBytes, ExpandedBytes, Entries, Relationships, DecoderAllocationBytes, WorkUnits, References, PreservedBytes }
    /// <summary>Structured logical locations contain no model payload excerpts.</summary>
    public class PackageError : global::System.IO.IOException {
      public Code Code { get; }
      public string Part { get; }
      public string FieldPath { get; }
      public string Reference { get; }
      public string RelationshipId { get; }
      public long OffsetBytes { get; }
      // Preserve application exception identity; only runtime-created failures gain locations.
      public PackageError(Code code,string message,string part="",string fieldPath="",string reference="",string relationshipId="",long offsetBytes=0,global::System.Exception? inner=null):base(message,inner){Code=code;Part=part;FieldPath=fieldPath;Reference=reference;RelationshipId=relationshipId;OffsetBytes=offsetBytes;}
    }
    /// <summary>Logical admission limits are independent of host allocator overhead.</summary>
    public sealed class PackageLimits {
      public long MaxArchiveBytes { get; init; }=256L*1024*1024;
      public long MaxIoBytes { get; init; }=1024L*1024*1024;
      public long MaxMetadataBytes { get; init; }=16L*1024*1024;
      public long MaxPartBytes { get; init; }=64L*1024*1024;
      public long MaxTotalExpandedBytes { get; init; }=256L*1024*1024;
      public long MaxBufferBytes { get; init; }=128L*1024*1024;
      public long MaxEntries { get; init; }=4096;
      public long MaxRelationships { get; init; }=16384;
      public long MaxDecoderAllocationBytes { get; init; }=128L*1024*1024;
      public long MaxWorkUnits { get; init; }=512L*1024*1024;
      public long MaxReferences { get; init; }=65536;
      public long MaxPreservedBytes { get; init; }=64L*1024*1024;
      public long MaxSpoolBytes { get; init; }=256L*1024*1024;
      public bool AllowSpool { get; init; }
      public string? SpoolDirectory { get; init; }
      public bool ForceZip64 { get; init; }
      public bool Descriptor { get; init; }
      public global::System.Func<bool>? IsCancelled { get; init; }
      public long? DeadlineTimestamp { get; init; }
    }
    /// <summary>One non-thread-safe cumulative ledger follows metadata, codecs and validation.</summary>
    public sealed class Budget {
      public PackageLimits Limits { get; }
      private readonly long[] used=new long[9];
      internal long HeldBytes;
      private Location? location;
      // Synchronous role attribution follows byte reads and DEFLATE before native traversal starts.
      private sealed class Location:global::System.IDisposable {internal readonly Budget Budget;internal readonly string Part,Member,Link;internal readonly global::System.Func<string> Field;private readonly Location? previous;
    // Borrow one role location until this synchronous scope is disposed.
    internal Location(Budget budget,string part,string member,string link){Budget=budget;Part=part;Member=member;Link=link;Field=()=>Member;previous=budget.location;budget.location=this;}
    // Release runtime-owned state and restore borrowed scope without transferring ownership.
    public void Dispose(){Budget.location=previous;}}

    // Admit and attach one role location before physical payload work.
    internal global::System.IDisposable Locate(string part,string member,string link){ConsumeAt(Resource.DecoderAllocationBytes,64,part,()=>member,link);return new Location(this,part,member,link);}

    // Fill omitted locations from the active synchronous role scope.
    private void Resolve(ref string part,ref global::System.Func<string>? field,ref string link){if(location!=null){part=location.Part;field??=location.Field;if(link.Length==0)link=location.Link;}}

    // Create only runtime-owned resource failures with the active role location.
    private PackageError Bound(string message,string part){global::System.Func<string>? field=null;string link="";Resolve(ref part,ref field,ref link);return new PackageError(Code.ResourceLimit,message,part,field?.Invoke()??"",relationshipId:link);}
      // Validate all bounds before observing a source or creating staging storage.
      public Budget(PackageLimits? limits=null){Limits=limits??new();foreach(long value in new[]{Limits.MaxArchiveBytes,Limits.MaxIoBytes,Limits.MaxMetadataBytes,Limits.MaxPartBytes,Limits.MaxTotalExpandedBytes,Limits.MaxBufferBytes,Limits.MaxEntries,Limits.MaxRelationships,Limits.MaxDecoderAllocationBytes,Limits.MaxWorkUnits,Limits.MaxReferences,Limits.MaxPreservedBytes,Limits.MaxSpoolBytes})if(value<0)throw new PackageError(Code.ResourceLimit,"Negative package bound");Check();}
      // Invoke original callbacks outside runtime error translation.
      public void Check(){CheckAt("",null,"");}

    // Poll original callbacks and attach locations only to runtime-created failures.
    internal void CheckAt(string part,global::System.Func<string>? field,string link){Resolve(ref part,ref field,ref link);if(Limits.IsCancelled?.Invoke()==true)throw new PackageError(Code.Cancelled,"Package operation cancelled",part,field?.Invoke()??"",relationshipId:link);if(Limits.DeadlineTimestamp is long deadline&&global::System.Diagnostics.Stopwatch.GetTimestamp()>=deadline)throw new PackageError(Code.DeadlineExceeded,"Package deadline exceeded",part,field?.Invoke()??"",relationshipId:link);}

    // Select the immutable ceiling for one cumulative resource.
    private long Maximum(Resource resource)=>resource switch{Resource.IoBytes=>Limits.MaxIoBytes,Resource.MetadataBytes=>Limits.MaxMetadataBytes,Resource.ExpandedBytes=>Limits.MaxTotalExpandedBytes,Resource.Entries=>Limits.MaxEntries,Resource.Relationships=>Limits.MaxRelationships,Resource.DecoderAllocationBytes=>Limits.MaxDecoderAllocationBytes,Resource.WorkUnits=>Limits.MaxWorkUnits,Resource.References=>Limits.MaxReferences,Resource.PreservedBytes=>Limits.MaxPreservedBytes,_=>throw new global::System.ArgumentOutOfRangeException(nameof(resource))};
      // Failed and repeated work is never refunded.
      public long Used(Resource resource)=>used[(int)resource];

    // Return the exact unconsumed logical capacity.
    public long Remaining(Resource resource)=>Maximum(resource)-Used(resource);

    // Admit cumulative work before mutation while preserving callback exception identity.
    public void Consume(Resource resource,long amount,string part=""){ConsumeAt(resource,amount,part,null,"");}

    // Admit one cumulative resource with explicit part, member and relationship identity.
    internal void ConsumeAt(Resource resource,long amount,string part,global::System.Func<string>? field,string link){Resolve(ref part,ref field,ref link);CheckAt(part,field,link);if(amount<0||amount>Remaining(resource))throw new PackageError(Code.ResourceLimit,"Shared package budget exceeded",part,field?.Invoke()??"",relationshipId:link);used[(int)resource]+=amount;}
      // Reserve simultaneous private capacities before BCL buffers can grow.
      internal void Hold(long count,string part=""){Check();if(count<0||count>Limits.MaxBufferBytes-HeldBytes)throw Bound("Package buffer limit exceeded",part);HeldBytes+=count;}

    // Release only simultaneous capacity; cumulative resource counters remain charged.
    internal void Release(long count){HeldBytes-=count;}

    // Validate signed policy and native array limits before narrowing an extent.
    internal int ArraySize(long count,long maximum,string part=""){if(count<0||count>maximum||count>global::System.Array.MaxLength)throw Bound("Package array extent exceeds policy",part);return (int)count;}
    }
    private const int ChunkBytes=32768;
    private const long U32=0xffffffffL;
    private static readonly global::System.Text.UTF8Encoding Utf8=new(false,true);
    internal sealed class State {
      internal readonly Budget Budget;internal readonly State? Parent;internal bool Busy,Closed;internal global::System.Runtime.ExceptionServices.ExceptionDispatchInfo? First;

    // Bind one synchronous wrapper to a shared ledger and optional parent.
    internal State(Budget budget,State? parent=null){Budget=budget??throw new global::System.ArgumentNullException(nameof(budget));Parent=parent;}

    // Latch and rethrow the first original exception for terminal wrapper failure.
    internal void Fail(global::System.Exception failure){First??=global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure);First.Throw();}

    // Reject terminal or closed wrappers and propagate parent failure.
    internal void Active(){First?.Throw();Parent?.Active();if(Closed)Fail(new PackageError(Code.InvalidState,"Package wrapper is closed"));}

    // Guard one operation against reentry before external callbacks.
    internal void Begin(){if(Busy)Fail(new PackageError(Code.InvalidState,"Reentrant package operation"));Active();Busy=true;}

    // Check liveness on both sides of original cancellation callbacks.
    internal void Poll(){Active();try{Budget.Check();}catch(global::System.Exception failure){Fail(failure);}Active();}

    // Admit cumulative work before mutation while preserving callback exception identity.
    internal void Consume(Resource resource,long count,string part=""){Poll();try{Budget.Consume(resource,count,part);}catch(global::System.Exception failure){Fail(failure);}Active();}

    // Invalidate the wrapper without closing borrowed caller resources.
    internal void Close(){Closed=true;if(Busy&&First==null)First=global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(new PackageError(Code.InvalidState,"Closed active package operation"));}
    }

    // Read or emit a checked little-endian wire integer.
    private static long Number(byte[] bytes,int start,int count){if(start<0||count<0||start>bytes.Length||count>bytes.Length-start)throw new PackageError(Code.InvalidArchive,"Truncated ZIP field");ulong value=0;for(int index=0;index<count;++index)value|=(ulong)bytes[start+index]<<(index*8);if(value>long.MaxValue)throw new PackageError(Code.ResourceLimit,"ZIP64 extent exceeds signed native range");return (long)value;}

    // Copy a validated byte extent into owning storage.
    private static byte[] Slice(byte[] bytes,int start,int count){if(start<0||count<0||start>bytes.Length||count>bytes.Length-start)throw new PackageError(Code.InvalidArchive,"Truncated ZIP bytes");return global::System.MemoryExtensions.AsSpan(bytes,start,count).ToArray();}

    // Validate ZIP spelling independently of filesystem paths.
    private static string PhysicalName(string name){if(string.IsNullOrEmpty(name)||name[0]=='/'||name.Contains('\\')||name.Contains('\0'))throw new PackageError(Code.InvalidName,"Invalid physical ZIP name");var pieces=name.Split('/');for(int index=0;index<pieces.Length;++index)if(pieces[index]=="."||pieces[index]==".."||pieces[index].Length==0&&index+1<pieces.Length)throw new PackageError(Code.InvalidName,"Invalid ZIP name segment");return name;}

    // Decode strict UTF8 or the APPNOTE CP437 fallback without replacement characters.
    private static string DecodeName(byte[] bytes,bool unicode){try{return (unicode?Utf8:global::System.Text.CodePagesEncodingProvider.Instance.GetEncoding(437,global::System.Text.EncoderFallback.ExceptionFallback,global::System.Text.DecoderFallback.ExceptionFallback)!).GetString(bytes);}catch(global::System.Text.DecoderFallbackException failure){throw new PackageError(Code.InvalidName,"Invalid ZIP name encoding",inner:failure);}}
    /// <summary>Random access is borrowed; bounded short reads are legal and no source is closed.</summary>
    public interface IRandomSource { long Length { get; } byte[] ReadAt(long offset,int maximum); }
    private sealed class BytesSource:IRandomSource {private readonly byte[] bytes;
    // Retain the runtime-owned immutable byte snapshot.
    internal BytesSource(byte[] bytes){this.bytes=bytes;}public long Length=>bytes.Length;
    // Read a checked bounded extent; legal short source callbacks are coalesced.
    public byte[] ReadAt(long offset,int maximum)=>Slice(bytes,(int)offset,maximum);}
    private sealed class StreamSource:IRandomSource {private readonly global::System.IO.Stream stream;private readonly long start;public long Length{get;}
    // Borrow only the seekable stream extent beginning at its current position.
)packagecs"
R"packagecs(    internal StreamSource(global::System.IO.Stream stream){this.stream=stream;start=stream.Position;long end=stream.Length;if(start<0||end<start)throw new PackageError(Code.Io,"Invalid package stream extent");Length=end-start;}
    // Read a checked bounded extent; legal short source callbacks are coalesced.
    public byte[] ReadAt(long offset,int maximum){stream.Position=checked(start+offset);var bytes=new byte[maximum];int count=stream.Read(bytes,0,maximum);if(count<0||count>maximum)throw new PackageError(Code.Io,"Invalid source read count");if(count!=maximum)global::System.Array.Resize(ref bytes,count);return bytes;}}
    // Create one unpredictable private file without ever consulting an entry name.
    private static global::System.IO.FileStream PrivateFile(string directory,string prefix,out string path){path=global::System.IO.Path.Combine(directory,prefix+global::System.Guid.NewGuid().ToString("N")+".tmp");var options=new global::System.IO.FileStreamOptions{Mode=global::System.IO.FileMode.CreateNew,Access=global::System.IO.FileAccess.ReadWrite,Share=global::System.IO.FileShare.None,Options=global::System.IO.FileOptions.WriteThrough};if(!global::System.OperatingSystem.IsWindows())options.UnixCreateMode=global::System.IO.UnixFileMode.UserRead|global::System.IO.UnixFileMode.UserWrite;return new global::System.IO.FileStream(path,options);}
    /// <summary>Immutable physical metadata distinguishes payload files from directories and links.</summary>
    public sealed class Entry {
      public string Name{get;}public int Method{get;}public int Flags{get;}public long Crc32{get;}public long CompressedBytes{get;}public long ExpandedBytes{get;}public int CreatorSystem{get;}public long ExternalAttributes{get;}internal readonly long DataOffset,LocalOffset,RecordEnd;

    // Retain immutable checked physical metadata for one directory entry.
    internal Entry(string name,int method,int flags,long crc,long compressed,long expanded,long data,long local,long end,int creator,long attributes){Name=name;Method=method;Flags=flags;Crc32=crc;CompressedBytes=compressed;ExpandedBytes=expanded;DataOffset=data;LocalOffset=local;RecordEnd=end;CreatorSystem=creator;ExternalAttributes=attributes;}

    // Distinguish payload files from directories, links and device records.
    public bool RegularFile(){if(Name.EndsWith("/",global::System.StringComparison.Ordinal))return false;if((CreatorSystem==0||CreatorSystem==10||CreatorSystem==14)&&(ExternalAttributes&24)!=0)return false;long kind=(ExternalAttributes>>16)&0xf000;return CreatorSystem!=3&&CreatorSystem!=19||kind==0||kind==0x8000;}
    }
    /// <summary>Checked ZIP32/ZIP64 directory access; no payload is published before exact CRC/size validation.</summary>
    public sealed class ZipReader:global::System.IDisposable {
      private readonly State state;private IRandomSource source=null!;private global::System.IO.Stream? owned;private string? temporary;private long extent,snapshotHeld;
      private readonly global::System.Collections.Generic.List<Entry> entries=new();private readonly global::System.Collections.Generic.Dictionary<string,Entry> names=new(global::System.StringComparer.Ordinal);

    // Bind a private parser state without taking ownership of caller sources.
    private ZipReader(Budget budget,State? parent){state=new(budget,parent);}
      public global::System.Collections.Generic.IReadOnlyList<Entry> Entries=>entries.AsReadOnly();public Budget SharedBudget=>state.Budget;

    // Validate and index a complete bounded archive before returning its reader.
    public static ZipReader Open(object input,Budget? budget=null)=>OpenCore(input,budget??new(),null);
)packagecs"
R"packagecs(
    // Share parent liveness and budget admission through complete index preparation.
    internal static ZipReader OpenCore(object input,Budget budget,State? parent){var value=new ZipReader(budget,parent);value.state.Begin();try{value.Attach(input);value.Parse();value.state.Poll();return value;}catch(global::System.Exception failure){value.state.Closed=true;value.state.Fail(failure);throw;}finally{value.state.Busy=false;if(value.state.Closed)value.Cleanup();}}

    // Select a borrowed source or explicitly bounded private spool.
    private void Attach(object input){state.Poll();if(input is byte[] bytes){state.Budget.ArraySize(bytes.Length,state.Budget.Limits.MaxArchiveBytes);state.Budget.Hold(bytes.Length);snapshotHeld=bytes.Length;state.Consume(Resource.DecoderAllocationBytes,bytes.Length);source=new BytesSource((byte[])bytes.Clone());}
        else if(input is IRandomSource random)source=random;
        else if(input is string path){owned=new global::System.IO.FileStream(path,global::System.IO.FileMode.Open,global::System.IO.FileAccess.Read,global::System.IO.FileShare.Read);source=new StreamSource(owned);}
        else if(input is global::System.IO.Stream stream){if(stream.CanSeek)source=new StreamSource(stream);else{if(!state.Budget.Limits.AllowSpool||state.Budget.Limits.SpoolDirectory==null)throw new PackageError(Code.InvalidState,"Nonseekable source requires explicit private spooling");owned=PrivateFile(state.Budget.Limits.SpoolDirectory,".serializer-zip-",out string temp);temporary=temp;state.Budget.Hold(ChunkBytes);try{state.Consume(Resource.DecoderAllocationBytes,ChunkBytes);var batch=new byte[ChunkBytes];long size=0;while(true){state.Poll();long remaining=global::System.Math.Min(state.Budget.Limits.MaxArchiveBytes,state.Budget.Limits.MaxSpoolBytes)-size;long io=state.Budget.Remaining(Resource.IoBytes);if(io==0)throw new PackageError(Code.ResourceLimit,"No admitted spool read capacity");int requested=(int)global::System.Math.Min(io,global::System.Math.Min(ChunkBytes,remaining<ChunkBytes?remaining+1:ChunkBytes));int count=stream.Read(batch,0,requested);state.Poll();if(count==0)break;if(count<0||count>requested)throw new PackageError(Code.Io,"Invalid nonseekable source read");if(count>remaining)throw new PackageError(Code.ResourceLimit,"Private spool bound exceeded");state.Consume(Resource.IoBytes,count);owned.Write(batch,0,count);size+=count;}owned.Position=0;source=new StreamSource(owned);}finally{state.Budget.Release(ChunkBytes);}}}
        else throw new PackageError(Code.InvalidState,"Unsupported package source");extent=source.Length;if(extent<0||extent>state.Budget.Limits.MaxArchiveBytes)throw new PackageError(Code.ResourceLimit,"Archive extent exceeds limit");state.Poll();}

    // Read a checked bounded extent; legal short source callbacks are coalesced.
    private byte[] ReadAt(long offset,long length){if(offset<0||length<0||offset>extent||length>extent-offset)throw new PackageError(Code.InvalidArchive,"ZIP extent outside archive");int size=state.Budget.ArraySize(length,state.Budget.Limits.MaxBufferBytes);state.Consume(Resource.IoBytes,size);state.Consume(Resource.DecoderAllocationBytes,size);state.Budget.Hold(size);try{var result=new byte[size];int position=0;while(position<size){state.Poll();int requested=global::System.Math.Min(ChunkBytes,size-position);byte[] block=source.ReadAt(offset+position,requested);state.Poll();if(block==null||block.Length==0||block.Length>requested)throw new PackageError(Code.Io,"Invalid or truncated random source read");global::System.Buffer.BlockCopy(block,0,result,position,block.Length);position+=block.Length;}return result;}finally{state.Budget.Release(size);}}

    // Read ordered ZIP64 sentinel values and reject malformed or duplicate extras.
    private static long[] Extra(byte[] bytes,params bool[] required){int position=0;byte[]? selected=null;while(position<bytes.Length){int kind=(int)Number(bytes,position,2),size=(int)Number(bytes,position+2,2);position+=4;if(size>bytes.Length-position)throw new PackageError(Code.InvalidArchive,"Truncated ZIP extra");if(kind==1){if(selected!=null)throw new PackageError(Code.InvalidArchive,"Duplicate ZIP64 extra");selected=Slice(bytes,position,size);}position+=size;}int count=0;foreach(bool value in required)if(value)++count;if(count==0)return global::System.Array.Empty<long>();if(selected==null||selected.Length<count*8)throw new PackageError(Code.InvalidArchive,"Missing ZIP64 extra fields");var result=new long[count];for(int index=0;index<count;++index)result[index]=Number(selected,index*8,8);return result;}
)packagecs"
R"packagecs(

    // Validate directory boundaries and every local record before publishing entry metadata.
    private void Parse(){long tailOffset=global::System.Math.Max(0,extent-65557);byte[] tail=ReadAt(tailOffset,extent-tailOffset);int selected=-1;state.Budget.Hold(tail.Length);long centralHeld=0;try{for(int position=tail.Length-22;position>=0;--position)if(Number(tail,position,4)==0x06054b50&&Number(tail,position+20,2)==tail.Length-position-22){selected=position;break;}if(selected<0)throw new PackageError(Code.InvalidArchive,"ZIP end directory missing");long disk=Number(tail,selected+4,2),centralDisk=Number(tail,selected+6,2),diskCount=Number(tail,selected+8,2),count=Number(tail,selected+10,2),size=Number(tail,selected+12,4),offset=Number(tail,selected+16,4),end=tailOffset+selected,boundary=end;if(disk!=0||centralDisk!=0||diskCount!=count)throw new PackageError(Code.UnsupportedFeature,"Multidisk ZIP unsupported");if(end>=20){byte[] locator=ReadAt(end-20,20);if(Number(locator,0,4)==0x07064b50){long location=Number(locator,8,8);if(Number(locator,4,4)!=0||Number(locator,16,4)!=1||location>end-20||end-20-location<56)throw new PackageError(Code.InvalidArchive,"Invalid ZIP64 locator");byte[] record=ReadAt(location,56);long wideCount=Number(record,32,8),wideSize=Number(record,40,8),wideOffset=Number(record,48,8);if(Number(record,0,4)!=0x06064b50||Number(record,4,8)<44||Number(record,4,8)!=end-20-location-12||Number(record,16,4)!=0||Number(record,20,4)!=0||Number(record,24,8)!=wideCount)throw new PackageError(Code.InvalidArchive,"Invalid ZIP64 directory");if(Number(record,14,2)>45)throw new PackageError(Code.UnsupportedFeature,"Unsupported extraction version");if(count!=65535&&count!=wideCount||size!=U32&&size!=wideSize||offset!=U32&&offset!=wideOffset)throw new PackageError(Code.InvalidArchive,"ZIP32/64 directory disagreement");count=wideCount;size=wideSize;offset=wideOffset;boundary=location;}}
        if(offset>boundary||size!=boundary-offset)throw new PackageError(Code.InvalidArchive,"ZIP directory boundary mismatch");state.Consume(Resource.Entries,count);state.Consume(Resource.MetadataBytes,size);byte[] central=ReadAt(offset,size);state.Budget.Hold(central.Length);centralHeld=central.Length;int cursor=0;for(long index=0;index<count;++index){state.Consume(Resource.WorkUnits,1);if(Number(central,cursor,4)!=0x02014b50)throw new PackageError(Code.InvalidArchive,"Invalid central entry");int flags=(int)Number(central,cursor+8,2),method=(int)Number(central,cursor+10,2);long crc=Number(central,cursor+16,4),compressed=Number(central,cursor+20,4),expanded=Number(central,cursor+24,4);int nameSize=(int)Number(central,cursor+28,2),extraSize=(int)Number(central,cursor+30,2),commentSize=(int)Number(central,cursor+32,2),length=46+nameSize+extraSize+commentSize;long local=Number(central,cursor+42,4);if(length>65535||length>central.Length-cursor)throw new PackageError(Code.InvalidArchive,"Truncated central entry");if(Number(central,cursor+34,2)!=0||(flags&~(0x800|8|6))!=0||method!=0&&method!=8)throw new PackageError(Code.UnsupportedFeature,"Unsupported ZIP item");if(Number(central,cursor+6,2)>45)throw new PackageError(Code.UnsupportedFeature,"Unsupported ZIP extraction version");byte[] encoded=Slice(central,cursor+46,nameSize);string name=PhysicalName(DecodeName(encoded,(flags&0x800)!=0));if(names.ContainsKey(name))throw new PackageError(Code.DuplicateName,"Duplicate physical ZIP item",name);bool wide=expanded==U32||compressed==U32;long[] values=Extra(Slice(central,cursor+46+nameSize,extraSize),expanded==U32,compressed==U32,local==U32);int next=0;if(expanded==U32)expanded=values[next++];if(compressed==U32)compressed=values[next++];if(local==U32)local=values[next];if(expanded>state.Budget.Limits.MaxPartBytes)throw new PackageError(Code.ResourceLimit,"Expanded part exceeds policy",name);if(local>offset||offset-local<30)throw new PackageError(Code.InvalidArchive,"Local header overlaps directory",name);byte[] header=ReadAt(local,30);if(Number(header,0,4)!=0x04034b50||Number(header,4,2)>45||Number(central,cursor+6,2)>45||Number(header,10,2)!=Number(central,cursor+12,2)||Number(header,12,2)!=Number(central,cursor+14,2)||Number(header,6,2)!=flags||Number(header,8,2)!=method)throw new PackageError(Code.InvalidArchive,"Local/central header disagreement",name);int localName=(int)Number(header,26,2),localExtra=(int)Number(header,28,2);long data=local+30+localName+localExtra;if(data>offset||compressed>offset-data)throw new PackageError(Code.InvalidArchive,"Payload overlaps directory",name);if(!global::System.MemoryExtensions.SequenceEqual<byte>(ReadAt(local+30,localName),encoded))throw new PackageError(Code.InvalidArchive,"Local filename mismatch",name);long localExpanded=Number(header,22,4),localCompressed=Number(header,18,4);values=Extra(ReadAt(local+30+localName,localExtra),localExpanded==U32,localCompressed==U32);next=0;if(localExpanded==U32)localExpanded=values[next++];if(localCompressed==U32)localCompressed=values[next];long finish=data+compressed;if((flags&8)!=0){wide=wide||Number(header,18,4)==U32||Number(header,22,4)==U32;int descriptorSize=wide?20:12;byte[] descriptor=ReadAt(finish,descriptorSize);bool signed=false;if(!DescriptorMatches(descriptor,wide,crc,compressed,expanded)&&Number(descriptor,0,4)==0x08074b50){descriptor=ReadAt(finish+4,descriptorSize);signed=true;}if(!DescriptorMatches(descriptor,wide,crc,compressed,expanded)||Number(header,14,4)!=0&&Number(header,14,4)!=crc||localExpanded!=0&&localExpanded!=expanded||localCompressed!=0&&localCompressed!=compressed)throw new PackageError(Code.Integrity,"ZIP descriptor mismatch",name);finish+=descriptorSize+(signed?4:0);}else if(Number(header,14,4)!=crc||localExpanded!=expanded||localCompressed!=compressed)throw new PackageError(Code.Integrity,"Local size/CRC mismatch",name);if(finish>offset)throw new PackageError(Code.InvalidArchive,"Local record overlaps directory",name);var entry=new Entry(name,method,flags,crc,compressed,expanded,data,local,finish,(int)(Number(central,cursor+4,2)>>8),Number(central,cursor+38,4));state.Consume(Resource.DecoderAllocationBytes,256L+encoded.Length,name);entries.Add(entry);names.Add(name,entry);cursor+=length;}if(cursor!=central.Length&&(central.Length-cursor<6||Number(central,cursor,4)!=0x05054b50||Number(central,cursor+4,2)!=central.Length-cursor-6))throw new PackageError(Code.InvalidArchive,"Unexpected central directory records");var intervals=new global::System.Collections.Generic.List<Entry>(entries);intervals.Sort((first,last)=>first.LocalOffset.CompareTo(last.LocalOffset));for(int index=1;index<intervals.Count;++index)if(intervals[index-1].RecordEnd>intervals[index].LocalOffset)throw new PackageError(Code.InvalidArchive,"Overlapping local ZIP records");}finally{state.Budget.Release(centralHeld+tail.Length);}}

    // Compare all descriptor values, including the CRC/signature ambiguity.
    private static bool DescriptorMatches(byte[] bytes,bool wide,long crc,long compressed,long expanded)=>Number(bytes,0,4)==crc&&Number(bytes,4,wide?8:4)==compressed&&Number(bytes,wide?12:8,wide?8:4)==expanded;

    // Validate exact expansion and CRC before returning owning payload bytes.
    public byte[] Read(string name){state.Begin();try{if(!names.TryGetValue(name,out Entry? entry))throw new PackageError(Code.MissingPart,"ZIP item missing",name);state.Consume(Resource.ExpandedBytes,entry.ExpandedBytes,name);state.Consume(Resource.WorkUnits,entry.CompressedBytes,name);int expected=state.Budget.ArraySize(entry.ExpandedBytes,state.Budget.Limits.MaxPartBytes,name);byte[] packed=ReadAt(entry.DataOffset,entry.CompressedBytes);state.Budget.Hold((long)packed.Length+expected,name);try{state.Consume(Resource.DecoderAllocationBytes,expected,name);byte[] result=packed;if(entry.Method==8){try{result=@compression_name@.Decompress(packed,new @compression_name@.DecodeOptions(@compression_name@.Format.Deflate,packed.Length,expected,32768,state.Poll));}catch(@compression_name@.Error failure){state.First?.Throw();throw new PackageError(failure.Code==@compression_name@.Code.ResourceLimit?Code.Integrity:Code.InvalidArchive,"ZIP DEFLATE rejected",name,inner:failure);}}if(result.Length!=expected||Crc(result,state)!=entry.Crc32)throw new PackageError(Code.Integrity,"ZIP CRC or expanded size mismatch",name);state.Poll();return result;}finally{state.Budget.Release((long)packed.Length+expected);}}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);throw;}finally{state.Busy=false;if(state.Closed)Cleanup();}}

    // Force integrity validation of every physical item.
    public void ValidateAll(){state.Active();foreach(Entry entry in entries)Read(entry.Name);}

    // Release runtime-owned state and restore borrowed scope without transferring ownership.
    public void Dispose(){state.Close();if(!state.Busy)Cleanup();}

    // Release private snapshots and staging files on all terminal paths.
    private void Cleanup(){global::System.Exception? cleanup=null;try{owned?.Dispose();}catch(global::System.Exception failure){cleanup=failure;}finally{owned=null;try{if(temporary!=null){string path=temporary;temporary=null;global::System.IO.File.Delete(path);}}catch(global::System.Exception failure){cleanup??=failure;}finally{if(snapshotHeld!=0){state.Budget.Release(snapshotHeld);snapshotHeld=0;}}}if(cleanup!=null){if(state.First!=null){state.First.SourceException.Data["SerializerCleanupFailure"]=cleanup;state.First.Throw();}throw cleanup;}}
    }

    // Calculate APPNOTE CRC32 with bounded cancellation intervals.
    private static uint Crc(byte[] bytes,State state){uint value=0xffffffff;for(int index=0;index<bytes.Length;++index){if(index%ChunkBytes==0)state.Poll();value^=bytes[index];for(int bit=0;bit<8;++bit)value=(value>>1)^((value&1)==0?0:0xedb88320);}return value^0xffffffff;}
)packagecs"
