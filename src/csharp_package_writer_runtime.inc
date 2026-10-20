// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagecs(
    private sealed class Bytes:global::System.IDisposable {
      private readonly State state;private readonly global::System.IO.MemoryStream stream=new();private long held;

    // Bind private output storage to admitted shared capacities.
    internal Bytes(State state){this.state=state;}
      internal long Length=>stream.Length;

    // Admit replacement capacity while the old buffer is still live.
    private void Ensure(int count){state.Poll();if(count<0||count>state.Budget.Limits.MaxArchiveBytes-stream.Length)throw new PackageError(Code.ResourceLimit,"ZIP output exceeds archive policy");int required=state.Budget.ArraySize(stream.Length+count,state.Budget.Limits.MaxArchiveBytes);if(required<=stream.Capacity)return;long doubled=global::System.Math.Max(256L,(long)stream.Capacity*2);int capacity=state.Budget.ArraySize(global::System.Math.Max(required,global::System.Math.Min(doubled,global::System.Math.Min(state.Budget.Limits.MaxArchiveBytes,global::System.Array.MaxLength))),global::System.Math.Min(state.Budget.Limits.MaxArchiveBytes,global::System.Array.MaxLength));state.Budget.Hold(capacity);try{state.Consume(Resource.DecoderAllocationBytes,capacity);stream.Capacity=capacity;}catch{state.Budget.Release(capacity);throw;}state.Budget.Release(held);held=capacity;}

    // Append an admitted owning byte span to private staging.
    internal void Append(byte[] bytes){Append(bytes,0,bytes.Length);}

    // Append an admitted owning byte span to private staging.
    internal void Append(byte[] bytes,int offset,int count){Ensure(count);stream.Write(bytes,offset,count);}

    // Read or emit a checked little-endian wire integer.
    internal void Number(long value,int width){Ensure(width);global::System.Span<byte> bytes=stackalloc byte[8];for(int index=0;index<width;++index)bytes[index]=(byte)((ulong)value>>(index*8));stream.Write(bytes[..width]);}

    // Admit the final owning copy before exposing complete bytes.
    internal byte[] Copy(){state.Budget.Hold(stream.Length);try{state.Consume(Resource.DecoderAllocationBytes,stream.Length);return stream.ToArray();}finally{state.Budget.Release(stream.Length);}}

    // Release runtime-owned state and restore borrowed scope without transferring ownership.
    public void Dispose(){stream.Dispose();state.Budget.Release(held);held=0;}
    }
    /// <summary>Canonical ZIP output snapshots each input before any caller sink is touched.</summary>
    public sealed class ZipWriter:global::System.IDisposable {
      private readonly State state;private bool finished;private long held;
      private readonly global::System.Collections.Generic.List<ZipPart> parts=new();private readonly global::System.Collections.Generic.HashSet<string> names=new(global::System.StringComparer.Ordinal);

    // Keep this prepared operation bounded by the shared package contract.
    private sealed record ZipPart(string Name,byte[] Data,int Method);

    // Create independent canonical ZIP staging over one shared ledger.
    public ZipWriter(Budget? budget=null){state=new(budget??new());}

    // Create independent canonical ZIP staging over one shared ledger.
    internal ZipWriter(Budget budget,State parent){state=new(budget,parent);}

    // Guard one operation against reentry before external callbacks.
    private void Begin(){state.Begin();if(finished){state.Busy=false;throw new PackageError(Code.InvalidState,"ZIP writer finished");}}

    // Validate and snapshot an input before any external sink callback.
    public void Add(string name,byte[] bytes,string compression="stored"){Begin();try{PhysicalName(name);if(bytes==null)throw new global::System.ArgumentNullException(nameof(bytes));int method=compression=="stored"?0:compression=="deflate"?8:throw new PackageError(Code.UnsupportedFeature,"Unsupported ZIP compression",name);if(!names.Add(name))throw new PackageError(Code.DuplicateName,"Duplicate ZIP name",name);state.Budget.ArraySize(bytes.Length,state.Budget.Limits.MaxPartBytes,name);state.Consume(Resource.Entries,1,name);state.Consume(Resource.ExpandedBytes,bytes.Length,name);state.Consume(Resource.MetadataBytes,128L+Utf8.GetByteCount(name),name);state.Consume(Resource.DecoderAllocationBytes,bytes.Length,name);state.Budget.Hold(bytes.Length,name);held+=bytes.Length;parts.Add(new(name,(byte[])bytes.Clone(),method));state.Poll();}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);}finally{state.Busy=false;if(state.Closed)Cleanup();}}

    // Produce deterministic local and central records entirely in private staging.
    private Bytes Prepare(){var output=new Bytes(state);var central=new global::System.Collections.Generic.List<byte[]>();long centralHeld=0;try{var ordered=new global::System.Collections.Generic.List<ZipPart>(parts);ordered.Sort((first,last)=>global::System.StringComparer.Ordinal.Compare(first.Name,last.Name));foreach(ZipPart part in ordered){state.Poll();byte[] encoded=Utf8.GetBytes(part.Name);if(encoded.Length>65535)throw new PackageError(Code.InvalidName,"ZIP filename exceeds wire limit",part.Name);uint checksum=Crc(part.Data,state);byte[] compressed=part.Data;long packedHeld=0;if(part.Method==8){long bound=(long)part.Data.Length+((long)part.Data.Length+16382)/16383*5+6;int capacity=state.Budget.ArraySize(bound,global::System.Math.Min(state.Budget.Limits.MaxArchiveBytes,state.Budget.Limits.MaxBufferBytes-state.Budget.HeldBytes),part.Name);state.Consume(Resource.WorkUnits,part.Data.Length,part.Name);state.Consume(Resource.DecoderAllocationBytes,capacity,part.Name);try{compressed=@compression_name@.Compress(part.Data,new @compression_name@.EncodeOptions(@compression_name@.Format.Deflate,part.Data.Length,capacity,global::System.IO.Compression.CompressionLevel.Optimal,state.Poll));}catch(@compression_name@.Error failure){state.First?.Throw();throw new PackageError(failure.Code==@compression_name@.Code.ResourceLimit?Code.ResourceLimit:Code.Codec,"ZIP compression failed",part.Name,inner:failure);}state.Budget.Hold(compressed.Length,part.Name);packedHeld=compressed.Length;}try{bool wide=state.Budget.Limits.ForceZip64||(long)part.Data.Length>=U32||(long)compressed.Length>=U32,descriptor=state.Budget.Limits.Descriptor;long local=output.Length;int flags=0x800|(descriptor?8:0);output.Number(0x04034b50,4);output.Number(wide?45:20,2);output.Number(flags,2);output.Number(part.Method,2);output.Number(0,2);output.Number(33,2);output.Number(descriptor?0:checksum,4);output.Number(wide?U32:descriptor?0:compressed.Length,4);output.Number(wide?U32:descriptor?0:part.Data.Length,4);output.Number(encoded.Length,2);output.Number(wide?20:0,2);output.Append(encoded);if(wide){output.Number(1,2);output.Number(16,2);output.Number(descriptor?0:part.Data.Length,8);output.Number(descriptor?0:compressed.Length,8);}for(int offset=0;offset<compressed.Length;){int count=global::System.Math.Min(ChunkBytes,compressed.Length-offset);output.Append(compressed,offset,count);offset+=count;}if(descriptor){output.Number(0x08074b50,4);output.Number(checksum,4);output.Number(compressed.Length,wide?8:4);output.Number(part.Data.Length,wide?8:4);}bool wideOffset=wide||local>=U32;using var record=new Bytes(state);record.Number(3*256+(wideOffset?45:20),2);using var entry=new Bytes(state);entry.Number(0x02014b50,4);entry.Append(record.Copy());entry.Number(wideOffset?45:20,2);entry.Number(flags,2);entry.Number(part.Method,2);entry.Number(0,2);entry.Number(33,2);entry.Number(checksum,4);entry.Number(wide?U32:compressed.Length,4);entry.Number(wide?U32:part.Data.Length,4);entry.Number(encoded.Length,2);entry.Number(wideOffset?(wide?28:12):0,2);entry.Number(0,2);entry.Number(0,2);entry.Number(0,2);entry.Number(0x81800000,4);entry.Number(wideOffset?U32:local,4);entry.Append(encoded);if(wideOffset){entry.Number(1,2);entry.Number(wide?24:8,2);if(wide){entry.Number(part.Data.Length,8);entry.Number(compressed.Length,8);}entry.Number(local,8);}byte[] value=entry.Copy();state.Consume(Resource.MetadataBytes,value.Length,part.Name);state.Budget.Hold(value.Length,part.Name);centralHeld+=value.Length;central.Add(value);}finally{state.Budget.Release(packedHeld);}}long start=output.Length;foreach(byte[] record in central)output.Append(record);long size=output.Length-start;bool wideDirectory=state.Budget.Limits.ForceZip64||parts.Count>=65535||start>=U32||size>=U32;if(wideDirectory){long location=output.Length;output.Number(0x06064b50,4);output.Number(44,8);output.Number(45,2);output.Number(45,2);output.Number(0,4);output.Number(0,4);output.Number(parts.Count,8);output.Number(parts.Count,8);output.Number(size,8);output.Number(start,8);output.Number(0x07064b50,4);output.Number(0,4);output.Number(location,8);output.Number(1,4);}output.Number(0x06054b50,4);output.Number(0,2);output.Number(0,2);output.Number(wideDirectory?65535:parts.Count,2);output.Number(wideDirectory?65535:parts.Count,2);output.Number(wideDirectory?U32:size,4);output.Number(wideDirectory?U32:start,4);output.Number(0,2);state.Poll();return output;}catch{output.Dispose();throw;}finally{state.Budget.Release(centralHeld);}}

    // Publish a complete private result only after final liveness checks.
    public byte[] Finish(){Begin();try{using var output=Prepare();byte[] result=output.Copy();state.Poll();finished=true;Cleanup();return result;}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);throw;}finally{state.Busy=false;if(state.Closed)Cleanup();}}

    // Borrow a sink only after complete canonical ZIP preparation.
    public void WriteTo(global::System.IO.Stream destination){if(destination==null)throw new global::System.ArgumentNullException(nameof(destination));Begin();try{using var output=Prepare();byte[] bytes=output.Copy();WriteCore(destination,bytes,state);finished=true;Cleanup();}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);}finally{state.Busy=false;if(state.Closed)Cleanup();}}

    // Release private snapshots and staging files on all terminal paths.
    private void Cleanup(){parts.Clear();names.Clear();state.Budget.Release(held);held=0;}

    // Release runtime-owned state and restore borrowed scope without transferring ownership.
    public void Dispose(){state.Close();if(!state.Busy)Cleanup();}
    }
    // Borrow the sink and poll after the last callback; never flush or close it.
    private static void WriteCore(global::System.IO.Stream destination,byte[] bytes,State state){for(int offset=0;offset<bytes.Length;){state.Poll();int count=global::System.Math.Min(ChunkBytes,bytes.Length-offset);state.Consume(Resource.IoBytes,count);destination.Write(bytes,offset,count);state.Poll();offset+=count;}state.Poll();}

    // Borrow the caller sink without flushing or closing it.
    public static void Write(global::System.IO.Stream destination,byte[] bytes,Budget? budget=null){var state=new State(budget??new());state.Begin();try{WriteCore(destination,bytes,state);}catch(global::System.Exception failure){state.Fail(failure);}finally{state.Busy=false;}}
    /// <summary>Publish a previously validated complete generation through a private sibling file.</summary>
    public static void Save(string path,byte[] bytes,Budget? budget=null,global::System.Action<string>? fault=null){var state=new State(budget??new());state.Begin();string? temporary=null;bool published=false;global::System.Exception? original=null;try{state.Poll();string target=global::System.IO.Path.GetFullPath(path),directory=global::System.IO.Path.GetDirectoryName(target)!;fault?.Invoke("before_create");state.Poll();using(var output=PrivateFile(directory,".serializer-opc-",out string created)){temporary=created;fault?.Invoke("after_create");state.Poll();WriteCore(output,bytes,state);fault?.Invoke("before_sync");state.Poll();output.Flush(true);state.Poll();}fault?.Invoke("before_publish");state.Poll();global::System.IO.File.Move(temporary!,target,true);published=true;temporary=null;fault?.Invoke("after_publish");state.Poll();}catch(global::System.Exception failure){original=failure;if(published)throw new PackageError(Code.PublicationUncertain,"New complete generation was published",inner:failure);throw;}finally{state.Busy=false;if(temporary!=null){try{global::System.IO.File.Delete(temporary);}catch(global::System.Exception cleanup){if(original==null)throw;original.Data["SerializerCleanupFailure"]=cleanup;}}}}
)packagecs"
