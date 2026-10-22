// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejava(
/** Checked ZIP/OPC operations; generated helper name: @package_name@. */
public static final class @package_name@ {
  private static final int CHUNK_BYTES = 32768;
  private static final long UINT32_MAX = 0xffffffffL;
  /** Static operations never install a global codec or filesystem provider. */
  private @package_name@() {}
  public enum Code { INVALID_OPTIONS, INVALID_STATE, INVALID_ARCHIVE, UNSUPPORTED_FEATURE,
    INVALID_NAME, DUPLICATE_NAME, INTEGRITY, RESOURCE_LIMIT, INVALID_METADATA,
    INVALID_RELATIONSHIP, MISSING_PART, SEMANTIC, CODEC, PUBLICATION_UNCERTAIN, DEADLINE_EXCEEDED }
  /** Structured diagnostics contain locations, never model/attachment payload excerpts. */
  public static final class PackageError extends java.io.IOException {
    private static final long serialVersionUID = 1L;
    public final Code code;
    public final String part, fieldPath, reference, relationshipId;
    public final long offsetBytes;
    /** Retain bounded logical context independently of the diagnostic message. */
    public PackageError(Code code, String message, String part, long offsetBytes,
        String fieldPath, String reference, String relationshipId) {
      super(message); this.code = code; this.part = part; this.offsetBytes = offsetBytes;
      this.fieldPath = fieldPath; this.reference = reference; this.relationshipId = relationshipId;
    }
    /** Construct a location-free package failure without exposing input bytes. */
    public PackageError(Code code, String message) { this(code,message,"",0,"","",""); }
  }
  /** Mutable configuration is snapshotted by Budget before an operation starts. */
  public static final class Limits {
    public long maxArchiveBytes = 256L*1024*1024, maxIoBytes = 1024L*1024*1024;
    public long maxMetadataBytes = 16L*1024*1024, maxPartBytes = 64L*1024*1024;
    public long maxTotalExpandedBytes = 256L*1024*1024, maxBufferBytes = 128L*1024*1024;
    public long maxEntries = 4096, maxRelationships = 16384;
    public long maxDecoderAllocationBytes = 128L*1024*1024, maxWorkUnits = 512L*1024*1024;
    public long maxReferences = 65536, maxPreservedBytes = 64L*1024*1024;
    public java.lang.Runnable checkCancelled;
    public java.lang.Long deadlineNanos;
    public java.nio.file.Path spoolDirectory;
    public long maxSpoolBytes = 256L*1024*1024;
    public boolean allowSpool, forceZip64, descriptor = true;
    /** Construct the bounded default profile without touching providers or files. */
    public Limits() {}
    /** Copy policy fields so later caller mutations cannot relax active admission. */
    private Limits(Limits value) {
      maxArchiveBytes=value.maxArchiveBytes; maxIoBytes=value.maxIoBytes;
      maxMetadataBytes=value.maxMetadataBytes; maxPartBytes=value.maxPartBytes;
      maxTotalExpandedBytes=value.maxTotalExpandedBytes; maxBufferBytes=value.maxBufferBytes;
      maxEntries=value.maxEntries; maxRelationships=value.maxRelationships;
      maxDecoderAllocationBytes=value.maxDecoderAllocationBytes; maxWorkUnits=value.maxWorkUnits;
      maxReferences=value.maxReferences; maxPreservedBytes=value.maxPreservedBytes;
      checkCancelled=value.checkCancelled; deadlineNanos=value.deadlineNanos;
      spoolDirectory=value.spoolDirectory; maxSpoolBytes=value.maxSpoolBytes;
      allowSpool=value.allowSpool; forceZip64=value.forceZip64; descriptor=value.descriptor;
    }
  }
  public enum Resource { IO_BYTES, METADATA_BYTES, EXPANDED_BYTES, ENTRIES, RELATIONSHIPS,
    DECODER_ALLOCATION_BYTES, WORK_UNITS, REFERENCES, PRESERVED_BYTES }
  /** One cumulative sequential ledger spans ZIP, metadata, typed callbacks and Save validation. */
  public static final class Budget {
    private final Limits limits;
    private final long[] used = new long[Resource.values().length];
    private long heldBytes;
    private boolean checking;private Throwable policyFailure;
    /** Validate a policy snapshot before callbacks or source access. */
    public Budget(Limits value) throws java.io.IOException {
      if (value == null) throw new PackageError(Code.INVALID_OPTIONS,"Missing package limits");
      limits = new Limits(value);
      for (Resource resource : Resource.values()) if (maximum(resource) < 0) throw new PackageError(Code.INVALID_OPTIONS,"Negative package limit");
      if (limits.maxArchiveBytes < 0 || limits.maxPartBytes < 0 || limits.maxBufferBytes < 0 || limits.maxSpoolBytes < 0)
        throw new PackageError(Code.INVALID_OPTIONS,"Negative package byte limit");
      check();
    }
    /** Construct an independent default ledger. */
    public Budget() throws java.io.IOException { this(new Limits()); }
    /** Return a detached configuration snapshot; it cannot mutate the active policy. */
    public Limits configured() { return new Limits(limits); }
    /** Derive a resource ceiling directly from the retained policy. */
    private long maximum(Resource resource) {
      return switch (resource) {
        case IO_BYTES -> limits.maxIoBytes; case METADATA_BYTES -> limits.maxMetadataBytes;
        case EXPANDED_BYTES -> limits.maxTotalExpandedBytes; case ENTRIES -> limits.maxEntries;
        case RELATIONSHIPS -> limits.maxRelationships; case DECODER_ALLOCATION_BYTES -> limits.maxDecoderAllocationBytes;
        case WORK_UNITS -> limits.maxWorkUnits; case REFERENCES -> limits.maxReferences;
        case PRESERVED_BYTES -> limits.maxPreservedBytes;
      };
    }
    /** Inspect unspent capacity without invoking application code. */
    public long remaining(Resource resource) { return maximum(resource)-used[resource.ordinal()]; }
    /** Check cooperative cancellation/deadline; recursive policy evaluation is rejected. */
    public void check() throws java.io.IOException {
      if(policyFailure!=null)throw policyException();
      if (checking) {policyFailure=new PackageError(Code.INVALID_STATE,"Reentrant package policy");throw policyException();}
      checking=true;
      try {
        if (limits.checkCancelled != null) limits.checkCancelled.run();
        if(policyFailure!=null)throw policyException();
        if (limits.deadlineNanos != null && java.lang.System.nanoTime()-limits.deadlineNanos >= 0)
          throw new PackageError(Code.DEADLINE_EXCEEDED,"Package deadline exceeded");
      } catch(java.io.IOException|RuntimeException|Error failure) {
        if(policyFailure==null)policyFailure=failure;throw policyException();
      } finally { checking=false; }
    }
    /** A swallowed recursive policy failure still fences the active outer operation. */
    private java.io.IOException policyException(){if(policyFailure instanceof java.io.IOException value)return value;if(policyFailure instanceof RuntimeException value)throw value;if(policyFailure instanceof Error value)throw value;throw new AssertionError(policyFailure);}
    /** Charge before allocation or publication; failed work is never reset. */
    public void consume(Resource resource, long amount, String part) throws java.io.IOException {
      check(); charge(resource,amount,part);
    }
    /** Update checked counters without a second application callback inside an activity guard. */
    private void charge(Resource resource, long amount, String part) throws java.io.IOException {
      if (amount < 0 || amount > remaining(resource)) throw error(Code.RESOURCE_LIMIT,"Package resource limit exceeded",part);
      used[resource.ordinal()]+=amount;
    }
    /** Reserve simultaneous internal byte storage without promising a process-wide heap cap. */
    private void hold(long amount, String part) throws java.io.IOException {
      if (amount < 0 || amount > limits.maxBufferBytes-heldBytes) throw error(Code.RESOURCE_LIMIT,"Package buffer limit exceeded",part);
      heldBytes+=amount;
    }
    /** Release only a previously admitted private byte reservation. */
    private void release(long amount) { heldBytes-=amount; }
  }
  /** Keep activity checks on both sides of every application policy/transport callback. */
  private static final class State {
    final Budget budget;
    final State parent;
    boolean busy, closed;
    Throwable first;
    /** Borrow one operation's retained ledger. */
    State(Budget budget) { this(budget,null); }
    /** Nested archive/provider loops observe the enclosing wrapper's first failure before further I/O. */
    State(Budget budget,State parent) { this.budget=java.util.Objects.requireNonNull(budget);this.parent=parent; }
    /** Admit one sequential operation and latch swallowed same-wrapper reentry. */
    void begin() throws java.io.IOException {
      if (busy) throw fail(new java.io.IOException("Reentrant package operation"));
      if (closed || first != null) throw new java.io.IOException("Inactive package operation",first);
      busy=true;
    }
    /** Reject callback close/failure before another borrowed call or successful publication. */
    void active() throws java.io.IOException {
      if (first != null) throw fail(first);
      if(parent!=null)parent.active();
      if (closed) throw fail(new java.io.IOException("Inactive package operation"));
    }
    /** Preserve the exact first checked, runtime or fatal application failure. */
    java.io.IOException fail(Throwable observed) {
      if (first == null) first=observed;
      if (first instanceof java.io.IOException value) return value;
      if (first instanceof RuntimeException value) throw value;
      if (first instanceof Error value) throw value;
      throw new AssertionError(first);
    }
    /** Poll around hooks so closing or swallowed reentry cannot resume work. */
    void poll() throws java.io.IOException {
      active();try{budget.check();}catch(java.io.IOException|RuntimeException|Error failure){throw fail(failure);}active();
    }
    /** Bridge checked lifecycle failures through the existing Runnable codec policy. */
    void codecPoll() {
      try { poll(); } catch (java.io.IOException failure) { first=first == null ? failure : first; throw new java.io.UncheckedIOException(failure); }
      catch(RuntimeException|Error failure){if(first==null)first=failure;throw failure;}
    }
    /** Charge only after the callback returns with the wrapper still active. */
    void consume(Resource resource,long amount,String part) throws java.io.IOException { poll(); budget.charge(resource,amount,part); }
    /** Mark closure immediately while resource owners defer cleanup until active calls return. */
    void close() { closed=true; if (busy && first == null) first=new java.io.IOException("Closed active package operation"); }
  }
  /** Attach only caller-visible logical context to a package failure. */
  private static PackageError error(Code code,String message,String part) { return new PackageError(code,message,part,0,"","",""); }
  /** Convert admitted Java array extents without narrowing a ZIP64 value silently. */
  private static int arraySize(long value,long maximum,String part) throws java.io.IOException {
    if (value < 0 || value > maximum || value > Integer.MAX_VALUE-8) throw error(Code.RESOURCE_LIMIT,"Package byte extent exceeds array policy",part);
    return (int)value;
  }
  /** Decode unsigned little-endian wire fields after checking their complete byte span. */
  private static long number(byte[] bytes,int position,int count) throws java.io.IOException {
    if (position < 0 || count < 0 || position > bytes.length || count > bytes.length-position)
      throw new PackageError(Code.INVALID_ARCHIVE,"Truncated ZIP field");
    long value=0;
    for (int index=0;index<count;++index) value|=(bytes[position+index]&255L)<<(index*8);
    if (count == 8 && value < 0) throw new PackageError(Code.RESOURCE_LIMIT,"Unsigned ZIP64 extent exceeds native range");
    return value;
  }
  /** Strict UTF-8 preserves exact scalar identities instead of normalizing Unicode text. */
  private static String text(byte[] data,boolean unicode) throws java.io.IOException {
    try {
      var charset=unicode ? java.nio.charset.StandardCharsets.UTF_8 : java.nio.charset.Charset.forName("IBM437");
      return charset.newDecoder().onMalformedInput(java.nio.charset.CodingErrorAction.REPORT)
          .onUnmappableCharacter(java.nio.charset.CodingErrorAction.REPORT).decode(java.nio.ByteBuffer.wrap(data)).toString();
    } catch (java.nio.charset.CharacterCodingException failure) { throw new PackageError(Code.INVALID_NAME,"Invalid ZIP filename encoding"); }
  }
  /** Treat ZIP names solely as logical metadata; no entry name ever becomes a filesystem path. */
  private static String physicalName(String name) throws java.io.IOException {
    if (name == null || name.isEmpty() || name.startsWith("/") || name.indexOf('\\') >= 0 || name.indexOf(0) >= 0)
      throw new PackageError(Code.INVALID_NAME,"Invalid ZIP item name");
    String[] pieces=name.split("/",-1);
    for (int index=0;index<pieces.length;++index)
      if (pieces[index].equals(".") || pieces[index].equals("..") || pieces[index].isEmpty() && index+1<pieces.length)
        throw new PackageError(Code.INVALID_NAME,"Invalid ZIP item segment");
    return name;
  }
  /** Expose checked random access without taking ownership of the application source. */
  public interface RandomSource {
    long size() throws java.io.IOException;
    byte[] readAt(long offset,int maximum) throws java.io.IOException;
  }
  /** Immutable physical metadata retains original names and creator attributes. */
  public static final class Entry {
    public final String name;
    public final int method,flags,creatorSystem;
    public final long crc32,compressedBytes,expandedBytes,externalAttributes;
    private final long dataOffset,localOffset,recordEnd;
    /** Bind only previously checked directory and local-record extents. */
    private Entry(String name,int method,int flags,long crc,long compressed,long expanded,
        long dataOffset,long localOffset,long recordEnd,int creator,long attributes) {
      this.name=name;this.method=method;this.flags=flags;crc32=crc;compressedBytes=compressed;
      expandedBytes=expanded;this.dataOffset=dataOffset;this.localOffset=localOffset;
      this.recordEnd=recordEnd;creatorSystem=creator;externalAttributes=attributes;
    }
    /** Exclude directories, links and device entries from OPC payload mapping without extracting them. */
    public boolean regularFile() {
      if (name.endsWith("/")) return false;
      if ((creatorSystem==0 || creatorSystem==10 || creatorSystem==14) && (externalAttributes&24)!=0) return false;
      int kind=(int)(externalAttributes>>>16)&0xf000;
      return creatorSystem!=3 && creatorSystem!=19 || kind==0 || kind==0x8000;
    }
  }
)packagejava"
R"packagejava(
  /** A checked archive owns only private staging or a Path-opened channel. */
  public static final class ZipReader implements AutoCloseable {
    private final State state;
    private RandomSource source;
    private AutoCloseable owned;
    private long extent;
    private final java.util.List<Entry> entries=new java.util.ArrayList<>();
    private final java.util.Map<String,Entry> names=new java.util.LinkedHashMap<>();
    /** Construct privately so malformed metadata never publishes a partially initialized reader. */
    private ZipReader(Budget budget,State parent) { state=new State(budget,parent); }
    /** Open a value-semantic array snapshot with an independent default ledger. */
    public static ZipReader open(byte[] input) throws java.io.IOException { return open(input,new Budget()); }
    /** Accept bytes, Path, RandomSource or borrowed InputStream; sequential input requires explicit spooling. */
    public static ZipReader open(Object input,Budget budget) throws java.io.IOException {
      return open(input,budget,null);
    }
    /** Keep the enclosing package wrapper in every inner bounded transport/provider poll. */
    private static ZipReader open(Object input,Budget budget,State parent) throws java.io.IOException {
      var reader=new ZipReader(budget,parent); reader.state.begin();
      try {
        reader.attach(input); reader.parse();reader.state.poll();return reader;
      } catch (java.io.IOException|RuntimeException|Error failure) {
        reader.state.closed=true;throw reader.state.fail(failure);
      } finally { reader.state.busy=false;if(reader.state.closed)reader.cleanup(); }
    }
    /** Return immutable metadata without granting access to the mutable validation indexes. */
    public java.util.List<Entry> entries() { return java.util.List.copyOf(entries); }
    /** Borrow the retained shared ledger for explicit domain validation. */
    public Budget budget() { return state.budget; }
    /** Bound complete input before retaining memory or reading the central directory. */
    private void attach(Object input) throws java.io.IOException {
      state.poll();
      if (input instanceof byte[] bytes) {
        arraySize(bytes.length,state.budget.limits.maxArchiveBytes,"");
        state.budget.hold(bytes.length,"");owned=()->state.budget.release(bytes.length);
        state.consume(Resource.DECODER_ALLOCATION_BYTES,bytes.length,"");
        byte[] snapshot=bytes.clone();extent=snapshot.length;
        source=new RandomSource() {
          /** Report the admitted stable byte extent. */
          public long size(){return snapshot.length;}
          /** Return an independent bounded slice from stable private bytes. */
          public byte[] readAt(long offset,int maximum){return java.util.Arrays.copyOfRange(snapshot,(int)offset,(int)offset+maximum);}
        };
      } else if (input instanceof java.nio.file.Path path) {
        var file=new FileSource(path,null);owned=file;source=file;extent=file.size();
      } else if(input instanceof RandomSource selected) {
        source=selected;extent=selected.size();state.poll();
      } else if(input instanceof java.io.InputStream stream) {
        if (!state.budget.limits.allowSpool || state.budget.limits.spoolDirectory==null)
          throw new PackageError(Code.INVALID_OPTIONS,"Sequential input requires explicit private spooling");
        int batchSize=arraySize(Math.min(CHUNK_BYTES,state.budget.limits.maxBufferBytes-state.budget.heldBytes),CHUNK_BYTES,"");
        if(batchSize==0 || state.budget.remaining(Resource.IO_BYTES)==0)throw new PackageError(Code.RESOURCE_LIMIT,"Private spool requires admitted I/O and scratch capacity");
        java.nio.file.Path temporary=privateFile(state.budget.limits.spoolDirectory,".serializer-zip-");
        try {
          try(var output=java.nio.channels.FileChannel.open(temporary,java.nio.file.StandardOpenOption.WRITE)) {
            state.budget.hold(batchSize,"");
            try {
            byte[] batch=new byte[batchSize];long size=0;
            while(true) {
              state.poll();long remaining=Math.min(state.budget.limits.maxArchiveBytes,state.budget.limits.maxSpoolBytes)-size;
              long io=state.budget.remaining(Resource.IO_BYTES);if(io==0)throw new PackageError(Code.RESOURCE_LIMIT,"Private spool has no admitted EOF/read capacity");
              int requested=(int)Math.min(io,remaining>=batchSize?batchSize:remaining+1);
              int count=stream.read(batch,0,requested);state.poll();if(count==-1)break;
              if(count<=0 || count>requested)throw new java.io.IOException("Invalid or zero-progress package source");
              if(count>remaining)throw new PackageError(Code.RESOURCE_LIMIT,"Package spool exceeds limit");
              state.consume(Resource.IO_BYTES,count,"");var buffer=java.nio.ByteBuffer.wrap(batch,0,count);
              while(buffer.hasRemaining()){state.poll();if(output.write(buffer)<=0)throw new java.io.IOException("Private spool made no progress");}
              size+=count;
            }
            }finally{state.budget.release(batchSize);}
          }
          var file=new FileSource(temporary,temporary);owned=file;source=file;extent=file.size();
        } catch(java.io.IOException|RuntimeException|Error failure) {
          try{java.nio.file.Files.deleteIfExists(temporary);}catch(java.io.IOException cleanup){failure.addSuppressed(cleanup);}throw failure;
        }
      } else throw new PackageError(Code.INVALID_OPTIONS,"Unsupported package source");
      if(extent<0 || extent>state.budget.limits.maxArchiveBytes)throw new PackageError(Code.RESOURCE_LIMIT,"Archive byte limit exceeded");
      state.poll();
    }
    /** Admit full extents before allocation/I/O, accumulating legal bounded short reads. */
    private byte[] readAt(long offset,long length) throws java.io.IOException {
      if(offset<0 || length<0 || offset>extent || length>extent-offset)throw new PackageError(Code.INVALID_ARCHIVE,"ZIP extent is outside archive");
      int count=arraySize(length,state.budget.limits.maxBufferBytes,"");
      state.consume(Resource.IO_BYTES,count,"");state.consume(Resource.DECODER_ALLOCATION_BYTES,count,"");
      state.budget.hold(count,"");
      try {
        byte[] result=new byte[count];int position=0;
        while(position<count) {
          state.poll();int requested=Math.min(CHUNK_BYTES,count-position);
          byte[] block=source.readAt(offset+position,requested);state.poll();
          if(block==null || block.length==0 || block.length>requested)throw new java.io.IOException("Invalid or truncated package random read");
          java.lang.System.arraycopy(block,0,result,position,block.length);position+=block.length;
        }
        return result;
      } finally {state.budget.release(count);}
    }
    /** Resolve ordered ZIP64 extra values only for fields carrying the standard sentinel. */
    private static long[] extra(byte[] bytes,boolean... required) throws java.io.IOException {
      int position=0;byte[] selected=null;
      while(position<bytes.length) {
        int kind=(int)number(bytes,position,2),size=(int)number(bytes,position+2,2);position+=4;
        if(size>bytes.length-position)throw new PackageError(Code.INVALID_ARCHIVE,"Truncated ZIP extra field");
        if(kind==1){if(selected!=null)throw new PackageError(Code.INVALID_ARCHIVE,"Duplicate ZIP64 extra");selected=java.util.Arrays.copyOfRange(bytes,position,position+size);}
        position+=size;
      }
      int count=0;for(boolean value:required)if(value)++count;
      if(count==0)return new long[0];
      if(selected==null || selected.length<count*8)throw new PackageError(Code.INVALID_ARCHIVE,"Missing ZIP64 extra values");
      long[] values=new long[count];for(int index=0;index<count;++index)values[index]=number(selected,index*8,8);return values;
    }
)packagejava"
R"packagejava(
    /** Validate exact directory/local boundaries and descriptors before any entry inflation. */
    private void parse() throws java.io.IOException {
      long tailOffset=Math.max(0,extent-65557);byte[] tail=readAt(tailOffset,extent-tailOffset);int selected=-1;
      state.budget.hold(tail.length,"");long centralHeld=0;
      try {
      for(int position=tail.length-22;position>=0;--position)
        if(number(tail,position,4)==0x06054b50L && number(tail,position+20,2)==tail.length-position-22){selected=position;break;}
      if(selected<0)throw new PackageError(Code.INVALID_ARCHIVE,"ZIP end directory is missing");
      long disk=number(tail,selected+4,2),centralDisk=number(tail,selected+6,2);
      long diskCount=number(tail,selected+8,2),count=number(tail,selected+10,2);
      long size=number(tail,selected+12,4),offset=number(tail,selected+16,4),end=tailOffset+selected,boundary=end;
      if(disk!=0 || centralDisk!=0 || diskCount!=count)throw new PackageError(Code.UNSUPPORTED_FEATURE,"Multidisk ZIP is unsupported");
      if(end>=20) {
        byte[] locator=readAt(end-20,20);
        if(number(locator,0,4)==0x07064b50L) {
          long location=number(locator,8,8);
          if(number(locator,4,4)!=0 || number(locator,16,4)!=1 || location>end-20 || end-20-location<56)
            throw new PackageError(Code.INVALID_ARCHIVE,"Invalid ZIP64 locator");
          byte[] record=readAt(location,56);long wideCount=number(record,32,8),wideSize=number(record,40,8),wideOffset=number(record,48,8);
          if(number(record,0,4)!=0x06064b50L || number(record,4,8)<44 || number(record,4,8)!=end-20-location-12 ||
              number(record,16,4)!=0 || number(record,20,4)!=0 || number(record,24,8)!=wideCount)
            throw new PackageError(Code.INVALID_ARCHIVE,"Invalid ZIP64 directory");
          if(number(record,14,2)>45)throw new PackageError(Code.UNSUPPORTED_FEATURE,"Unsupported ZIP64 extraction version");
          if(count!=65535 && count!=wideCount || size!=UINT32_MAX && size!=wideSize || offset!=UINT32_MAX && offset!=wideOffset)
            throw new PackageError(Code.INVALID_ARCHIVE,"ZIP32 and ZIP64 records disagree");
          count=wideCount;size=wideSize;offset=wideOffset;boundary=location;
        }
      }
      if(offset>boundary || size!=boundary-offset)throw new PackageError(Code.INVALID_ARCHIVE,"ZIP directory boundary mismatch");
      state.consume(Resource.ENTRIES,count,"");state.consume(Resource.METADATA_BYTES,size,"");
      byte[] central=readAt(offset,size);state.budget.hold(central.length,"");centralHeld=central.length;int position=0;
      for(long index=0;index<count;++index) {
        state.consume(Resource.WORK_UNITS,1,"");
        if(number(central,position,4)!=0x02014b50L)throw new PackageError(Code.INVALID_ARCHIVE,"Invalid ZIP central entry");
        int flags=(int)number(central,position+8,2),method=(int)number(central,position+10,2);
        long crc=number(central,position+16,4),compressed=number(central,position+20,4),expanded=number(central,position+24,4);
        int nameSize=(int)number(central,position+28,2),extraSize=(int)number(central,position+30,2),commentSize=(int)number(central,position+32,2);
        long local=number(central,position+42,4);int length=46+nameSize+extraSize+commentSize;
        if(length>65535||length>central.length-position)throw new PackageError(Code.INVALID_ARCHIVE,"Truncated ZIP central fields");
        if(number(central,position+34,2)!=0 || (flags&~(0x800|8|6))!=0 || method!=0 && method!=8)
          throw new PackageError(Code.UNSUPPORTED_FEATURE,"Unsupported ZIP entry");
        byte[] encoded=java.util.Arrays.copyOfRange(central,position+46,position+46+nameSize);
        String name=physicalName(text(encoded,(flags&0x800)!=0));
        if(names.containsKey(name))throw error(Code.DUPLICATE_NAME,"Duplicate physical ZIP item",name);
        boolean wide=expanded==UINT32_MAX || compressed==UINT32_MAX;
        long[] values=extra(java.util.Arrays.copyOfRange(central,position+46+nameSize,position+46+nameSize+extraSize),expanded==UINT32_MAX,compressed==UINT32_MAX,local==UINT32_MAX);
        int next=0;if(expanded==UINT32_MAX)expanded=values[next++];if(compressed==UINT32_MAX)compressed=values[next++];if(local==UINT32_MAX)local=values[next];
        if(expanded>state.budget.limits.maxPartBytes)throw error(Code.RESOURCE_LIMIT,"Expanded part exceeds limit",name);
        if(local>offset || offset-local<30)throw error(Code.INVALID_ARCHIVE,"ZIP local header overlaps directory",name);
        byte[] header=readAt(local,30);
        if(number(header,0,4)!=0x04034b50L || number(header,4,2)>45 || number(central,position+6,2)>45 || number(header,10,2)!=number(central,position+12,2) || number(header,12,2)!=number(central,position+14,2) || number(header,6,2)!=flags || number(header,8,2)!=method)
          throw error(Code.INVALID_ARCHIVE,"ZIP local and central headers disagree",name);
        int localName=(int)number(header,26,2),localExtra=(int)number(header,28,2);
        long data=local+30+localName+localExtra;
        if(data>offset || compressed>offset-data)throw error(Code.INVALID_ARCHIVE,"ZIP payload overlaps directory",name);
        if(!java.util.Arrays.equals(readAt(local+30,localName),encoded))throw error(Code.INVALID_ARCHIVE,"ZIP local filename mismatch",name);
        long localExpanded=number(header,22,4),localCompressed=number(header,18,4);
        values=extra(readAt(local+30+localName,localExtra),localExpanded==UINT32_MAX,localCompressed==UINT32_MAX);
        next=0;if(localExpanded==UINT32_MAX)localExpanded=values[next++];if(localCompressed==UINT32_MAX)localCompressed=values[next];
        long finish=data+compressed;
        if((flags&8)!=0) {
          wide=wide || number(header,18,4)==UINT32_MAX || number(header,22,4)==UINT32_MAX;
          int descriptorSize=wide?20:12;byte[] descriptor=readAt(finish,descriptorSize);boolean signed=false;
          if(!descriptor(descriptor,wide,crc,compressed,expanded) && number(descriptor,0,4)==0x08074b50L) {
            descriptor=readAt(finish+4,descriptorSize);signed=true;
          }
          if(!descriptor(descriptor,wide,crc,compressed,expanded) || number(header,14,4)!=0 && number(header,14,4)!=crc ||
              localExpanded!=0 && localExpanded!=expanded || localCompressed!=0 && localCompressed!=compressed)
            throw error(Code.INTEGRITY,"ZIP descriptor mismatch",name);
          finish+=descriptorSize+(signed?4:0);
        } else if(number(header,14,4)!=crc || localExpanded!=expanded || localCompressed!=compressed)
          throw error(Code.INTEGRITY,"ZIP local CRC/size mismatch",name);
        if(finish>offset)throw error(Code.INVALID_ARCHIVE,"ZIP record overlaps directory",name);
        var entry=new Entry(name,method,flags,crc,compressed,expanded,data,local,finish,(int)(number(central,position+4,2)>>>8),number(central,position+38,4));
        state.consume(Resource.DECODER_ALLOCATION_BYTES,256L+encoded.length,name);entries.add(entry);names.put(name,entry);position+=length;
      }
      if(position!=central.length && (central.length-position<6 || number(central,position,4)!=0x05054b50L || number(central,position+4,2)!=central.length-position-6))throw new PackageError(Code.INVALID_ARCHIVE,"Unexpected ZIP directory records");
      var intervals=new java.util.ArrayList<>(entries);intervals.sort(java.util.Comparator.comparingLong(entry->entry.localOffset));
      for(int index=1;index<intervals.size();++index)if(intervals.get(index-1).recordEnd>intervals.get(index).localOffset)
        throw new PackageError(Code.INVALID_ARCHIVE,"Overlapping ZIP local records");
      }finally{state.budget.release(centralHeld+tail.length);}
    }
    /** Distinguish unsigned descriptors whose CRC happens to equal the optional signature. */
    private static boolean descriptor(byte[] bytes,boolean wide,long crc,long compressed,long expanded) throws java.io.IOException {
      return number(bytes,0,4)==crc && number(bytes,4,wide?8:4)==compressed && number(bytes,wide?12:8,wide?8:4)==expanded;
    }
    /** Decode one directory-selected payload and verify exact standard extent, size and CRC. */
    public byte[] read(String name) throws java.io.IOException {
      state.begin();
      try {
        Entry entry=names.get(name);if(entry==null)throw error(Code.MISSING_PART,"ZIP item is missing",name);
        state.poll();state.consume(Resource.EXPANDED_BYTES,entry.expandedBytes,name);
        state.consume(Resource.WORK_UNITS,entry.compressedBytes,name);
        int expected=arraySize(entry.expandedBytes,state.budget.limits.maxPartBytes,name);
        byte[] packed=readAt(entry.dataOffset,entry.compressedBytes);state.budget.hold((long)packed.length+expected,name);
        try {
          state.consume(Resource.DECODER_ALLOCATION_BYTES,expected,name);
          byte[] result;
          if(entry.method==0)result=packed;
          else {
            try { result=@compression_name@.decompress(packed,new @compression_name@.DecodeOptions(@compression_name@.Format.DEFLATE,packed.length,expected,32768,state::codecPoll)); }
            catch(@compression_name@.Failure failure) {
              if(state.first!=null)throw state.fail(state.first);
              throw error(failure.code==@compression_name@.Code.RESOURCE_LIMIT?Code.INTEGRITY:Code.INVALID_ARCHIVE,"ZIP DEFLATE payload failed",name);
            }
          }
          if(result.length!=expected)throw error(Code.INTEGRITY,"ZIP expanded size mismatch",name);
          var crc=new java.util.zip.CRC32();
          for(int position=0;position<result.length;position+=Math.min(CHUNK_BYTES,result.length-position)) {
            state.poll();crc.update(result,position,Math.min(CHUNK_BYTES,result.length-position));
          }
          if(crc.getValue()!=entry.crc32)throw error(Code.INTEGRITY,"ZIP CRC mismatch",name);
          state.poll();return result;
        } finally {state.budget.release((long)packed.length+expected);}
      } catch(java.io.IOException|RuntimeException|Error failure) {state.closed=true;throw state.fail(failure);}
      finally {state.busy=false;if(state.closed)cleanup();}
    }
    /** Close only private staging or a channel opened from Path; borrowed sources remain open. */
    @Override public void close() throws java.io.IOException {state.close();if(!state.busy)cleanup();}
    /** Release owned resources after callbacks/providers have stopped borrowing them. */
    private void cleanup() throws java.io.IOException {
      if(owned!=null){var current=owned;owned=null;try{current.close();}catch(java.io.IOException|RuntimeException|Error failure){if(state.first!=null)state.first.addSuppressed(failure);else if(failure instanceof java.io.IOException value)throw value;else if(failure instanceof RuntimeException value)throw value;else throw (Error)failure;}catch(Exception failure){if(state.first!=null)state.first.addSuppressed(failure);else throw new java.io.IOException("Private package cleanup failed",failure);}}
      source=null;
    }
  }
  /** Open only caller-selected paths; entry names never reach this adapter. */
  private static final class FileSource implements RandomSource,AutoCloseable {
    final java.nio.channels.FileChannel channel;
    final java.nio.file.Path temporary;
    /** Own a readonly channel and optionally its private spool pathname. */
    FileSource(java.nio.file.Path path,java.nio.file.Path temporary) throws java.io.IOException {
      channel=java.nio.channels.FileChannel.open(path,java.nio.file.StandardOpenOption.READ);this.temporary=temporary;
    }
    /** Report the native file extent without allocating archive storage. */
    @Override public long size() throws java.io.IOException {return channel.size();}
    /** Perform bounded positional I/O without changing another owner's stream cursor. */
    @Override public byte[] readAt(long offset,int maximum) throws java.io.IOException {
      byte[] bytes=new byte[maximum];int count=channel.read(java.nio.ByteBuffer.wrap(bytes),offset);
      return count<0?new byte[0]:java.util.Arrays.copyOf(bytes,count);
    }
    /** Close the owned channel before deleting its private spool. */
    @Override public void close() throws java.io.IOException {
      java.io.IOException first=null;try{channel.close();}catch(java.io.IOException failure){first=failure;}
      try{if(temporary!=null)java.nio.file.Files.deleteIfExists(temporary);}catch(java.io.IOException failure){if(first==null)first=failure;else first.addSuppressed(failure);}if(first!=null)throw first;
    }
  }
  /** Create a private unpredictable file, explicitly requesting owner-only POSIX permissions where supported. */
  private static java.nio.file.Path privateFile(java.nio.file.Path directory,String prefix) throws java.io.IOException {
    if(java.nio.file.Files.getFileStore(directory).supportsFileAttributeView("posix"))
      return java.nio.file.Files.createTempFile(directory,prefix,".tmp",java.nio.file.attribute.PosixFilePermissions.asFileAttribute(java.util.EnumSet.of(java.nio.file.attribute.PosixFilePermission.OWNER_READ,java.nio.file.attribute.PosixFilePermission.OWNER_WRITE)));
    return java.nio.file.Files.createTempFile(directory,prefix,".tmp");
  }
)packagejava"
