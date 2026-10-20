// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejava(
  /** Check CRC through bounded batches without taking transport ownership. */
  private static long crc(byte[] bytes,State state) throws java.io.IOException {
    var crc=new java.util.zip.CRC32();
    for(int offset=0;offset<bytes.length;){state.poll();int count=Math.min(CHUNK_BYTES,bytes.length-offset);crc.update(bytes,offset,count);offset+=count;}
    state.poll();return crc.getValue();
  }
  /** Own an admitted in-memory archive; spare capacity is outside logical byte accounting. */
  private static final class Bytes {
    final State state;
    final boolean archive;
    final java.io.ByteArrayOutputStream stream=new java.io.ByteArrayOutputStream();
    long reserved;
    /** Borrow the active operation's exact cumulative ledger. */
    Bytes(State state){this(state,false);}
    /** Count archive bytes as I/O while metadata scratch contributes bounded work only. */
    Bytes(State state,boolean archive){this.state=state;this.archive=archive;}
    /** Admit every complete append before retaining its bytes. */
    void append(byte[] value) throws java.io.IOException {
      if(value.length>state.budget.limits.maxArchiveBytes-stream.size() || value.length>Integer.MAX_VALUE-8-stream.size())
        throw new PackageError(Code.RESOURCE_LIMIT,"ZIP output extent exceeds limit");
      state.consume(archive?Resource.IO_BYTES:Resource.WORK_UNITS,value.length,"");state.budget.hold(value.length,"");reserved+=value.length;
      stream.write(value,0,value.length);
    }
    /** Append a bounded explicit little-endian integer independent of object layout. */
    void number(long value,int width) throws java.io.IOException {
      byte[] bytes=new byte[width];for(int index=0;index<width;++index)bytes[index]=(byte)(value>>>(index*8));append(bytes);
    }
    /** Expose only the logical output extent. */
    long size(){return stream.size();}
    /** Return an independent owning result after admitting the simultaneous copy. */
    byte[] copy() throws java.io.IOException {
      state.poll();state.budget.hold(stream.size(),"");
      try{state.consume(Resource.DECODER_ALLOCATION_BYTES,stream.size(),"");return stream.toByteArray();}
      finally{state.budget.release(stream.size());}
    }
    /** Release ledger reservations after all provider/transport borrows have ended. */
    void close(){state.budget.release(reserved);reserved=0;stream.reset();}
  }
  /** One seek-free complete archive writer; failed/finished/closed operations cannot resume. */
  public static final class ZipWriter implements AutoCloseable {
    private static final class Part {
      final String name;final byte[] bytes;final int method;
      /** Retain an already admitted private input snapshot. */
      Part(String name,byte[] bytes,int method){this.name=name;this.bytes=bytes;this.method=method;}
    }
    private final State state;
    private final java.util.List<Part> parts=new java.util.ArrayList<>();
    private final java.util.Set<String> names=new java.util.HashSet<>();
    private long held;
    private boolean finished;
    /** Create a writer without creating files or publishing a ZIP header. */
    public ZipWriter() throws java.io.IOException {this(new Budget());}
    /** Borrow the exact caller-selected ledger through all staged work. */
    public ZipWriter(Budget budget){state=new State(budget);}
    /** Couple internal writer polls to the active profile composer without changing public construction. */
    private ZipWriter(Budget budget,State parent){state=new State(budget,parent);}
    /** Snapshot a stored entry; names remain logical metadata, never filesystem destinations. */
    public void add(String name,byte[] bytes) throws java.io.IOException {add(name,bytes,"stored");}
    /** Admit stored/raw-DEFLATE payloads before retaining immutable private bytes. */
    public void add(String name,byte[] bytes,String compression) throws java.io.IOException {
      begin();
      try {
        physicalName(name);
        if(bytes==null || !("stored".equals(compression)||"deflate".equals(compression)))
          throw new PackageError(Code.INVALID_OPTIONS,"Invalid ZIP entry input");
        int size=arraySize(bytes.length,state.budget.limits.maxPartBytes,name);
        if(names.contains(name))throw error(Code.DUPLICATE_NAME,"Duplicate ZIP output item",name);
        state.consume(Resource.ENTRIES,1,name);state.consume(Resource.EXPANDED_BYTES,size,name);
        state.consume(Resource.METADATA_BYTES,name.length()*2L+128,name);
        state.consume(Resource.DECODER_ALLOCATION_BYTES,size,name);state.budget.hold(size,name);held+=size;
        byte[] snapshot=bytes.clone();state.poll();parts.add(new Part(name,snapshot,"deflate".equals(compression)?8:0));names.add(name);
      }catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}
      finally{leave();}
    }
    /** Reject finished state before the shared activity/reentry guard. */
    private void begin() throws java.io.IOException {
      if(finished)throw new java.io.IOException("Finished ZIP writer",state.first);state.begin();
    }
    /** Complete canonical metadata and bounded raw providers without seeking a caller sink. */
    private Bytes prepare() throws java.io.IOException {
      var output=new Bytes(state,true);var central=new java.util.ArrayList<byte[]>();long centralHeld=0;
      try {
        var ordered=new java.util.ArrayList<>(parts);ordered.sort(java.util.Comparator.comparing(part->part.name));
        for(Part part:ordered) {
          state.poll();byte[] encoded=part.name.getBytes(java.nio.charset.StandardCharsets.UTF_8);
          if(encoded.length>65535)throw error(Code.INVALID_NAME,"ZIP filename exceeds wire limit",part.name);
          long checksum=crc(part.bytes,state);byte[] compressed=part.bytes;
          if(part.method==8) {
            long capacity=Math.min(state.budget.limits.maxArchiveBytes-output.size(),state.budget.limits.maxBufferBytes-state.budget.heldBytes);
            try{compressed=@compression_name@.compress(part.bytes,new @compression_name@.EncodeOptions(@compression_name@.Format.DEFLATE,6,
                part.bytes.length,arraySize(capacity,Integer.MAX_VALUE-8,part.name),state::codecPoll));}
            catch(@compression_name@.Failure failure){if(state.first!=null)throw state.fail(state.first);throw error(failure.code==@compression_name@.Code.RESOURCE_LIMIT?Code.RESOURCE_LIMIT:Code.CODEC,"ZIP compression failed",part.name);}
          }
          state.poll();
          long extraHeld=part.method==8?compressed.length:0;state.budget.hold(extraHeld,part.name);
          try {
            boolean wide=state.budget.limits.forceZip64 || part.bytes.length>=UINT32_MAX || compressed.length>=UINT32_MAX;
            boolean descriptor=state.budget.limits.descriptor;long local=output.size();int flags=0x800|(descriptor?8:0);
            var header=new Bytes(state);
            try {
              header.number(0x04034b50L,4);header.number(wide?45:20,2);header.number(flags,2);header.number(part.method,2);
              header.number(0,2);header.number(33,2);header.number(descriptor?0:checksum,4);
              header.number(wide?UINT32_MAX:descriptor?0:compressed.length,4);header.number(wide?UINT32_MAX:descriptor?0:part.bytes.length,4);
              header.number(encoded.length,2);header.number(wide?20:0,2);header.append(encoded);
              if(wide){header.number(1,2);header.number(16,2);header.number(descriptor?0:part.bytes.length,8);header.number(descriptor?0:compressed.length,8);}
              output.append(header.copy());
            }finally{header.close();}
            for(int offset=0;offset<compressed.length;){state.poll();int count=Math.min(CHUNK_BYTES,compressed.length-offset);output.append(java.util.Arrays.copyOfRange(compressed,offset,offset+count));offset+=count;}
            if(descriptor){output.number(0x08074b50L,4);output.number(checksum,4);output.number(compressed.length,wide?8:4);output.number(part.bytes.length,wide?8:4);}
            boolean wideOffset=wide || local>=UINT32_MAX;var record=new Bytes(state);
            try {
              record.number(0x02014b50L,4);record.number(3*256+(wideOffset?45:20),2);record.number(wideOffset?45:20,2);record.number(flags,2);record.number(part.method,2);
              record.number(0,2);record.number(33,2);record.number(checksum,4);
              record.number(wide?UINT32_MAX:compressed.length,4);record.number(wide?UINT32_MAX:part.bytes.length,4);
              record.number(encoded.length,2);record.number(wideOffset?(wide?28:12):0,2);record.number(0,2);record.number(0,2);record.number(0,2);
              record.number(0x81800000L,4);record.number(wideOffset?UINT32_MAX:local,4);record.append(encoded);
              if(wideOffset){record.number(1,2);record.number(wide?24:8,2);if(wide){record.number(part.bytes.length,8);record.number(compressed.length,8);}record.number(local,8);}
              byte[] value=record.copy();state.consume(Resource.METADATA_BYTES,value.length,part.name);state.budget.hold(value.length,part.name);centralHeld+=value.length;central.add(value);
            }finally{record.close();}
          }finally{state.budget.release(extraHeld);}
        }
        long start=output.size();for(byte[] record:central)output.append(record);long size=output.size()-start;
        boolean wide=state.budget.limits.forceZip64 || parts.size()>=65535 || start>=UINT32_MAX || size>=UINT32_MAX;
        if(wide){long location=output.size();output.number(0x06064b50L,4);output.number(44,8);output.number(45,2);output.number(45,2);output.number(0,4);output.number(0,4);output.number(parts.size(),8);output.number(parts.size(),8);output.number(size,8);output.number(start,8);output.number(0x07064b50L,4);output.number(0,4);output.number(location,8);output.number(1,4);}
        output.number(0x06054b50L,4);output.number(0,2);output.number(0,2);output.number(wide?65535:parts.size(),2);output.number(wide?65535:parts.size(),2);
        output.number(wide?UINT32_MAX:size,4);output.number(wide?UINT32_MAX:start,4);output.number(0,2);state.poll();return output;
      }catch(java.io.IOException|RuntimeException|Error failure){output.close();throw failure;}
      finally{state.budget.release(centralHeld);}
    }
    /** Return complete owned ZIP bytes only after metadata/provider validation and final policy checks. */
    public byte[] finish() throws java.io.IOException {
      begin();Bytes output=null;
      try{output=prepare();byte[] result=output.copy();state.poll();finished=true;return result;}
      catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}
      finally{if(output!=null)output.close();if(finished)cleanup();leave();}
    }
    /** Borrow a sink only after the complete archive is prepared; failure may retain a prefix. */
    public void writeTo(java.io.OutputStream destination) throws java.io.IOException {
      if(destination==null)throw new PackageError(Code.INVALID_OPTIONS,"Missing ZIP sink");begin();Bytes output=null;
      try {
        output=prepare();byte[] bytes=output.copy();
        for(int offset=0;offset<bytes.length;){state.poll();int count=Math.min(CHUNK_BYTES,bytes.length-offset);destination.write(bytes,offset,count);state.poll();offset+=count;}
        state.poll();finished=true;
      }catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}
      finally{if(output!=null)output.close();if(finished)cleanup();leave();}
    }
    /** Leave the active borrow before releasing staging abandoned by a callback. */
    private void leave(){state.busy=false;if(state.closed)cleanup();}
    /** Drop only owned staging and its ledger reservations. */
    private void cleanup(){parts.clear();names.clear();state.budget.release(held);held=0;}
    /** Abandon staging without a trailer or a borrowed close/flush. */
    @Override public void close(){state.close();if(!state.busy)cleanup();}
  }
)packagejava"
