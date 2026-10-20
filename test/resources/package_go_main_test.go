// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
  "bytes"
  "encoding/binary"
  "encoding/hex"
  "errors"
  "io"
  "os"
  "path/filepath"
  "sort"
  "strconv"
  "strings"
  "testing"
  "time"
)

// fixturePath restricts independent fixture lookup to the runner's private directory.
func fixturePath(name string) string { return filepath.Join(os.Getenv("SRL_PACKAGE_FIXTURES"),name) }
// fixtureBytes requires the actual wire fixture; absence can never masquerade as an expected rejection.
func fixtureBytes(t *testing.T,name string) []byte {
  t.Helper();data,err:=os.ReadFile(fixturePath(name));if err!=nil { t.Fatal(err) };return data
}
// requireCode checks the isolated public failure category and forbids a published owning result.
func requireCode(t *testing.T,codes string,data []byte,err error) {
  t.Helper();var failure *SerializerPackageError
  if data!=nil||!errors.As(err,&failure) { t.Fatalf("Expected %s and no output, got %d bytes/%v",codes,len(data),err) }
  for _,code:=range strings.Split(codes,",") { if string(failure.Code)==strings.ToLower(code) { return } }
  t.Fatalf("Expected %s, got %s",codes,failure.Code)
}
// generous admits real 65K-entry metadata and virtual offsets while keeping actual storage bounded.
func generous() SerializerPackageLimits {
  limits:=SerializerPackageDefaultLimits();limits.MaxArchiveBytes=1<<34;limits.MaxEntries=100000
  limits.MaxMetadataBytes=128<<20;limits.MaxBufferBytes=256<<20;return limits
}
// budgetFor validates each complete fixture's fresh independent policy snapshot.
func budgetFor(t *testing.T,limits SerializerPackageLimits) *SerializerPackageBudget {
  t.Helper();budget,err:=SerializerPackageNewBudget(limits);if err!=nil { t.Fatal(err) };return budget
}

// segmentedSource supplies only tiny independently authored wire records in a large logical extent.
type segmentedSource struct {
  extent int64
  offsets []int64
  parts [][]byte
  bytes,calls int64
}
// newSegments parses the independent descriptor rather than allocating its sparse logical archive.
func newSegments(t *testing.T,name string) *segmentedSource {
  t.Helper();data:=fixtureBytes(t,name)
  if len(data)<20||!bytes.Equal(data[:8],[]byte{'S','Z','I','P','6','4',0,1}) { t.Fatal("Invalid segmented fixture header") }
  value:=&segmentedSource{extent:int64(binary.LittleEndian.Uint64(data[8:16]))}
  count:=int(binary.LittleEndian.Uint32(data[16:20]));position:=20
  if count<0||count>16||value.extent<0 { t.Fatal("Invalid segment count/extent") }
  for index:=0;index<count;index++ {
    if len(data)-position<16 { t.Fatal("Truncated segment descriptor") }
    offset,size:=int64(binary.LittleEndian.Uint64(data[position:])),int64(binary.LittleEndian.Uint64(data[position+8:]));position+=16
    if offset<0||size<0||size>1<<20||size>int64(len(data)-position)||offset>value.extent||size>value.extent-offset { t.Fatal("Invalid segment extent") }
    value.offsets=append(value.offsets,offset);value.parts=append(value.parts,data[position:position+int(size)]);position+=int(size)
  }
  if position!=len(data) { t.Fatal("Unexpected segment descriptor suffix") };return value
}
// ReadAt verifies every requested extent and synthesizes legal bounded short results only.
func (source *segmentedSource) ReadAt(output []byte,offset int64) (int,error) {
  if offset<0||offset>source.extent||int64(len(output))>source.extent-offset||len(output)>32768 { panic("Unchecked segmented source extent") }
  count:=len(output);if count>17 { count=17 };clear(output[:count]);source.calls++;source.bytes+=int64(count)
  if source.bytes>1<<20 { panic("Excessive physical segmented-source I/O") }
  for index,start:=range source.offsets {
    first,last:=offset,offset+int64(count);if first<start { first=start };finish:=start+int64(len(source.parts[index]));if last>finish { last=finish }
    if first<last { copy(output[first-offset:last-offset],source.parts[index][first-start:last-start]) }
  }
  if count<len(output) { return count,io.EOF };return count,nil
}

// TestIndependentInputs checks shared wire decisions through the actual generated public API.
func TestIndependentInputs(t *testing.T) {
  for _,line:=range strings.Split(strings.TrimSuffix(string(fixtureBytes(t,"cases.tsv")),"\n"),"\n") {
    values:=strings.Split(strings.TrimSuffix(line,"\r"),"\t");if len(values)!=6 { t.Fatal("Malformed independent decision manifest") }
    t.Run(values[0],func(t *testing.T){
      var input any=fixturePath(values[2]);var segmented *segmentedSource
      if values[1]=="segment" { segmented=newSegments(t,values[2]);input=SerializerPackageSource{Reader:segmented,SizeBytes:segmented.extent} }
      reader,err:=SerializerPackageOpen(input,budgetFor(t,generous()))
      if err!=nil&&reader!=nil { t.Fatal("Failed indexing published a partial reader") }
      if err==nil {
        defer reader.Close();entries:=reader.Entries()
        if strings.HasPrefix(values[1],"count") {
          count,parseErr:=strconv.Atoi(values[4]);if parseErr!=nil||len(entries)!=count { t.Fatal("Narrowed entry count",parseErr,len(entries),count) }
          if values[1]=="count" { for index,entry:=range entries { if entry.Name!="part"+strconv.Itoa(index) { t.Fatal("Input entry order changed") } } }
          _,err=reader.Read("part"+strconv.Itoa(count-1))
        } else {
          for _,entry:=range entries {
            var actual []byte;actual,err=reader.Read(entry.Name)
            if err!=nil { if actual!=nil { t.Fatal("Failed entry returned partial owning bytes") };break }
          }
          if err==nil&&values[4]!="" {
            var actual []byte;actual,err=reader.Read(values[4]);expected,parseErr:=hex.DecodeString(values[5])
            if parseErr!=nil||err==nil&&!bytes.Equal(actual,expected) { t.Fatal("Independent payload mismatch",parseErr) }
          }
        }
      }
      if values[3]=="OK" { if err!=nil { t.Fatal(err) } } else { requireCode(t,values[3],nil,err) }
      if segmented!=nil&&segmented.bytes>1<<20 { t.Fatal("Virtual archive was materialized") }
    })
  }
}

// trackedSource checks borrowed ownership, exact callback ranges, short I/O and late application effects.
type trackedSource struct {
  data []byte
  calls,bytes int
  closed bool
  callback func()
  failure error
  invalid int
}
// ReadAt returns bounded short results and can inject a first transport error or callback reentry.
func (source *trackedSource) ReadAt(output []byte,offset int64) (int,error) {
  if source.closed||len(output)>32768||offset<0||offset>int64(len(source.data))||int64(len(output))>int64(len(source.data))-offset { panic("Borrowed-source contract violated") }
  source.calls++;if source.callback!=nil { source.callback() }
  if source.failure!=nil { return 0,source.failure }
  if source.invalid==1 { return 0,nil };if source.invalid==2 { return len(output)+1,nil };if source.invalid==3 { return -1,nil }
  count:=len(output);if count>17 { count=17 };copy(output[:count],source.data[int(offset):int(offset)+count]);source.bytes+=count
  if count<len(output) { return count,io.EOF };return count,nil
}
// Close records forbidden ownership transfer by generated readers.
func (source *trackedSource) Close() error { source.closed=true;return nil }
// sourceValue creates the explicit stable extent wrapper used by the public Go API.
func sourceValue(source *trackedSource) SerializerPackageSource {
  return SerializerPackageSource{Reader:source,SizeBytes:int64(len(source.data))}
}

// TestSourceContracts validates snapshots, base-relative ReaderAt, limits and sticky first failures.
func TestSourceContracts(t *testing.T) {
  data:=fixtureBytes(t,"independent-0-0.zip")
  t.Run("snapshot",func(t *testing.T){
    mutable:=append([]byte(nil),data...);reader,err:=SerializerPackageOpen(mutable);if err!=nil { t.Fatal(err) };defer reader.Close()
    clear(mutable);actual,err:=reader.Read("data.bin");if err!=nil||string(actual)!="independent data" { t.Fatal("Source snapshot changed",err) }
    entries:=reader.Entries();entries[0].Name="mutated";if reader.Entries()[0].Name=="mutated" { t.Fatal("Mutable entry catalog") }
  })
  t.Run("borrowed-section-base",func(t *testing.T){
    prefixed:=append([]byte("ignored-prefix"),data...);underlying:=bytes.NewReader(prefixed)
    section:=io.NewSectionReader(underlying,int64(len("ignored-prefix")),int64(len(data)))
    reader,err:=SerializerPackageOpen(SerializerPackageSource{Reader:section,SizeBytes:int64(len(data))});if err!=nil { t.Fatal(err) };defer reader.Close()
    actual,err:=reader.Read("data.bin");position,seekErr:=underlying.Seek(0,io.SeekCurrent)
    if err!=nil||seekErr!=nil||position!=0||string(actual)!="independent data" { t.Fatal("Borrowed base/position changed",err,seekErr) }
  })
  for _,mode:=range []int{1,2,3} { t.Run("invalid-progress-"+strconv.Itoa(mode),func(t *testing.T){
    source:=&trackedSource{data:data,invalid:mode};reader,err:=SerializerPackageOpen(sourceValue(source));if reader!=nil||err==nil||source.closed { t.Fatal("Invalid source result accepted/closed") }
  }) }
  t.Run("pre-I/O-admission",func(t *testing.T){
    source:=&trackedSource{data:data};limits:=SerializerPackageDefaultLimits();limits.MaxArchiveBytes=int64(len(data))-1
    reader,err:=SerializerPackageOpen(sourceValue(source),budgetFor(t,limits));if reader!=nil||source.calls!=0 { t.Fatal("Archive bound read source") };requireCode(t,"RESOURCE_LIMIT",nil,err)
  })
  t.Run("transport-before-late-policy",func(t *testing.T){
    source:=&trackedSource{data:data};armed:=false;transport,policy:=errors.New("source marker"),errors.New("policy marker")
    limits:=SerializerPackageDefaultLimits();limits.CheckCancelled=func()error{if armed{return policy};return nil}
    reader,err:=SerializerPackageOpen(sourceValue(source),budgetFor(t,limits));if err!=nil { t.Fatal(err) };defer reader.Close()
    source.callback=func(){armed=true};source.failure=transport;actual,err:=reader.Read("data.bin")
    if actual!=nil||err!=transport { t.Fatal("First transport error replaced",err) };calls:=source.calls
    _,later:=reader.Read("data.bin");if !errors.Is(later,transport)||source.calls!=calls||source.closed { t.Fatal("Failed reader resumed/lost cause",later) }
  })
  t.Run("no-progress-before-late-policy",func(t *testing.T){
    source:=&trackedSource{data:data};armed:=false;marker:=errors.New("later cancellation")
    limits:=SerializerPackageDefaultLimits();limits.CheckCancelled=func()error{if armed{return marker};return nil}
    reader,err:=SerializerPackageOpen(sourceValue(source),budgetFor(t,limits));if err!=nil { t.Fatal(err) };defer reader.Close()
    source.invalid=1;source.callback=func(){armed=true};before:=source.calls
    actual,err:=reader.Read("data.bin")
    if actual!=nil||!errors.Is(err,io.ErrNoProgress)||source.calls!=before+1 { t.Fatal("Source progress failure was masked",err) }
  })
  for _,action:=range []string{"close","reentry","panic"} { t.Run("callback-"+action,func(t *testing.T){
    source:=&trackedSource{data:data};reader,err:=SerializerPackageOpen(sourceValue(source));if err!=nil { t.Fatal(err) };defer reader.Close()
    marker:=&struct{value int}{17};var nested error
    source.callback=func(){switch action {case "close":reader.Close();case "reentry":_,nested=reader.Read("data.bin");case "panic":panic(marker)}}
    before:=source.calls;var actual []byte
    if action=="panic" { func(){defer func(){if recover()!=marker{t.Fatal("Panic identity changed")}}();reader.Read("data.bin");t.Fatal("Panic swallowed")}() } else { actual,err=reader.Read("data.bin");if actual!=nil||err==nil { t.Fatal("Callback failure published output") };if action=="reentry"&&err!=nested { t.Fatal("Nested first error replaced") } }
    if source.calls!=before+1 { t.Fatal("Callback failure allowed later source calls") };before=source.calls
    if _,err=reader.Read("data.bin");err==nil||source.calls!=before||source.closed { t.Fatal("Inactive source resumed/closed") }
  }) }
}

// sequentialSource records a borrowed nonseekable stream and its exact source-consumption boundary.
type sequentialSource struct {
  data []byte
  position,calls int
  closed,empty bool
  failure error
  eof func()
}
// Read supplies legal short reads and an optional true-EOF callback without implicit ownership changes.
func (source *sequentialSource) Read(output []byte) (int,error) {
  if source.closed||len(output)==0||len(output)>32768 { panic("Invalid borrowed sequential read") };source.calls++
  if source.failure!=nil { return 0,source.failure };if source.empty { return 0,nil }
  if source.position==len(source.data) { if source.eof!=nil { source.eof() };return 0,io.EOF }
  count:=len(output);if count>17 { count=17 };if count>len(source.data)-source.position { count=len(source.data)-source.position }
  copy(output[:count],source.data[source.position:source.position+count]);source.position+=count;return count,nil
}
// Close records forbidden borrowed ownership transfer.
func (source *sequentialSource) Close() error { source.closed=true;return nil }

// TestSpooling checks explicit admission, private cleanup, true EOF, zero progress and pre-I/O quota bounds.
func TestSpooling(t *testing.T) {
  data:=fixtureBytes(t,"independent-0-0.zip")
  t.Run("explicit-private-borrow",func(t *testing.T){
    source:=&sequentialSource{data:data};reader,err:=SerializerPackageOpen(source)
    if reader!=nil||source.calls!=0 { t.Fatal("Implicit spool touched source") };requireCode(t,"INVALID_OPTIONS",nil,err)
    directory:=t.TempDir();limits:=SerializerPackageDefaultLimits();limits.AllowSpool=true;limits.SpoolDirectory=directory;limits.MaxSpoolBytes=int64(len(data))
    reader,err=SerializerPackageOpen(source,budgetFor(t,limits));if err!=nil { t.Fatal(err) }
    actual,err:=reader.Read("data.bin");if err!=nil||string(actual)!="independent data" { t.Fatal(err) }
    files,err:=os.ReadDir(directory);if err!=nil||len(files)!=1||!strings.HasPrefix(files[0].Name(),".serializer-zip-") { t.Fatal("Missing private spool",err) }
    if err=reader.Close();err!=nil { t.Fatal(err) };files,err=os.ReadDir(directory)
    if err!=nil||len(files)!=0||source.closed { t.Fatal("Private spool ownership/cleanup",err) }
  })
  for _,capacity:=range []int64{0,int64(len(data)-1)} { t.Run("I/O-"+strconv.FormatInt(capacity,10),func(t *testing.T){
    source:=&sequentialSource{data:data};directory:=t.TempDir();limits:=SerializerPackageDefaultLimits()
    limits.AllowSpool=true;limits.SpoolDirectory=directory;limits.MaxIOBytes=capacity;limits.MaxSpoolBytes=int64(len(data))
    reader,err:=SerializerPackageOpen(source,budgetFor(t,limits));if reader!=nil||int64(source.position)>capacity||source.closed { t.Fatal("Spool exceeded I/O admission or published reader") }
    requireCode(t,"RESOURCE_LIMIT",nil,err);files,err:=os.ReadDir(directory);if err!=nil||len(files)!=0 { t.Fatal("Failed spool leaked",err) }
  }) }
  for _,capacity:=range []string{"I/O","buffer"} { t.Run("pre-file-"+capacity,func(t *testing.T){
    source:=&sequentialSource{data:data};limits:=SerializerPackageDefaultLimits();limits.AllowSpool=true
    limits.SpoolDirectory=filepath.Join(t.TempDir(),"absent","private")
    if capacity=="I/O" { limits.MaxIOBytes=0 } else { limits.MaxBufferBytes=0 }
    reader,err:=SerializerPackageOpen(source,budgetFor(t,limits));if reader!=nil||source.calls!=0 { t.Fatal("Spool preflight read input") };requireCode(t,"RESOURCE_LIMIT",nil,err)
  }) }
  for _,scenario:=range []string{"byte-limit","transport","cancel-at-EOF","no-progress"} { t.Run(scenario,func(t *testing.T){
    source:=&sequentialSource{data:data};directory:=t.TempDir();limits:=SerializerPackageDefaultLimits();limits.AllowSpool=true;limits.SpoolDirectory=directory
    marker:=errors.New("borrowed input marker");cancelled:=false
    switch scenario {case "byte-limit":limits.MaxSpoolBytes=int64(len(data))-1;case "transport":source.failure=marker
    case "cancel-at-EOF":source.eof=func(){cancelled=true};limits.CheckCancelled=func()error{if cancelled{return marker};return nil}
    case "no-progress":source.empty=true}
    reader,err:=SerializerPackageOpen(source,budgetFor(t,limits));if reader!=nil||err==nil||source.closed { t.Fatal("Failed spool accepted/closed source") }
    if (scenario=="transport"||scenario=="cancel-at-EOF")&&err!=marker { t.Fatal("Source/cancellation identity replaced",err) }
    if scenario=="no-progress"&&(!errors.Is(err,io.ErrNoProgress)||source.calls>100) { t.Fatal("Unbounded zero-progress source",err) }
    files,readErr:=os.ReadDir(directory);if readErr!=nil||len(files)!=0 { t.Fatal("Abandoned private spool",readErr) }
  }) }
}

// readAll performs the same complete public operation for independent exact/one-tight ledger controls.
func readAll(data []byte,budget *SerializerPackageBudget) error {
  reader,err:=SerializerPackageOpen(data,budget);if err!=nil { return err };defer reader.Close()
  for _,entry:=range reader.Entries() { if _,err=reader.Read(entry.Name);err!=nil { return err } };return nil
}
// resourceLimit maps a public resource to its independent default/modified policy field.
func resourceLimit(limits *SerializerPackageLimits,resource SerializerPackageResource) *int64 {
  switch resource {
  case SerializerPackageIOBytes:return &limits.MaxIOBytes
  case SerializerPackageMetadataBytes:return &limits.MaxMetadataBytes
  case SerializerPackageExpandedBytes:return &limits.MaxTotalExpandedBytes
  case SerializerPackageEntries:return &limits.MaxEntries
  case SerializerPackageRelationships:return &limits.MaxRelationships
  case SerializerPackageDecoderAllocationBytes:return &limits.MaxDecoderAllocationBytes
  case SerializerPackageWorkUnits:return &limits.MaxWorkUnits
  case SerializerPackageReferences:return &limits.MaxReferences
  case SerializerPackagePreservedBytes:return &limits.MaxPreservedBytes
  };panic("Unknown test resource")
}
// TestBudgets checks independent policy snapshots, exact cumulative charges and simultaneous storage.
func TestBudgets(t *testing.T) {
  data:=fixtureBytes(t,"independent-0-0.zip")
  t.Run("snapshots",func(t *testing.T){
    limits:=SerializerPackageDefaultLimits();limits.MaxArchiveBytes=int64(len(data))-1;budget:=budgetFor(t,limits)
    limits.MaxArchiveBytes=1<<40;configured:=budget.Configured();configured.MaxArchiveBytes=1<<40
    reader,err:=SerializerPackageOpen(data,budget);if reader!=nil { t.Fatal("Mutable policy relaxed active bound") };requireCode(t,"RESOURCE_LIMIT",nil,err)
  })
  t.Run("invalid-before-callback",func(t *testing.T){
    calls:=0;limits:=SerializerPackageDefaultLimits();limits.MaxEntries=-1;limits.CheckCancelled=func()error{calls++;return nil}
    budget,err:=SerializerPackageNewBudget(limits);if budget!=nil||calls!=0 { t.Fatal("Invalid policy invoked callback") };requireCode(t,"INVALID_OPTIONS",nil,err)
  })
  t.Run("expired-deadline",func(t *testing.T){
    limits:=SerializerPackageDefaultLimits();limits.Deadline=time.Now().Add(-time.Hour)
    budget,err:=SerializerPackageNewBudget(limits);if budget!=nil { t.Fatal("Expired policy published ledger") };requireCode(t,"DEADLINE_EXCEEDED",nil,err)
  })
  t.Run("policy-reentry-first-error",func(t *testing.T){
    var budget *SerializerPackageBudget;var nested error;active:=false
    returned:=errors.New("later callback return")
    limits:=SerializerPackageDefaultLimits();limits.CheckCancelled=func()error {
      if !active { return nil };nested=budget.Check();return returned
    }
    budget=budgetFor(t,limits);active=true;err:=budget.Check()
    requireCode(t,"INVALID_STATE",nil,nested)
    if err!=nested||budget.Check()!=nested { t.Fatal("Later callback return replaced first policy failure") }
  })
  t.Run("exact-cumulative",func(t *testing.T){
    measured:=budgetFor(t,SerializerPackageDefaultLimits());if err:=readAll(data,measured);err!=nil { t.Fatal(err) }
    for _,resource:=range []SerializerPackageResource{SerializerPackageIOBytes,SerializerPackageMetadataBytes,SerializerPackageExpandedBytes,SerializerPackageEntries,SerializerPackageDecoderAllocationBytes,SerializerPackageWorkUnits} {
      limits:=SerializerPackageDefaultLimits();used:=*resourceLimit(&limits,resource)-measured.Remaining(resource)
      if used==0 { t.Fatal("Unexercised resource") };*resourceLimit(&limits,resource)=used
      if err:=readAll(data,budgetFor(t,limits));err!=nil { t.Fatal("Exact resource rejected",resource,err) }
      *resourceLimit(&limits,resource)=used-1;requireCode(t,"RESOURCE_LIMIT",nil,readAll(data,budgetFor(t,limits)))
    }
  })
  t.Run("exact-private-buffer",func(t *testing.T){
    lower,upper:=int64(0),int64(16384)
    for lower<upper { middle:=lower+(upper-lower)/2;limits:=SerializerPackageDefaultLimits();limits.MaxBufferBytes=middle
      err:=readAll(data,budgetFor(t,limits));if err==nil { upper=middle } else { requireCode(t,"RESOURCE_LIMIT",nil,err);lower=middle+1 } }
    if lower==0||lower==16384 { t.Fatal("Invalid buffer boundary") };limits:=SerializerPackageDefaultLimits();limits.MaxBufferBytes=lower
    if err:=readAll(data,budgetFor(t,limits));err!=nil { t.Fatal("Exact buffer rejected",err) };limits.MaxBufferBytes--
    requireCode(t,"RESOURCE_LIMIT",nil,readAll(data,budgetFor(t,limits)))
  })
}

// trackedSink records accepted bytes and rejects accidental close/flush ownership transfers.
type trackedSink struct {
  bytes.Buffer
  calls int
  closed,flushed,short bool
  failure error
  callback func()
}
// Write accepts one bounded request and can inject partial I/O or synchronous application reentry.
func (sink *trackedSink) Write(data []byte) (int,error) {
  if len(data)>32768||sink.closed { panic("Invalid borrowed sink request") };sink.calls++
  count:=len(data);if sink.short&&count>0 { count-- };if sink.failure!=nil&&count>1 { count=1 }
  sink.Buffer.Write(data[:count]);if sink.callback!=nil { sink.callback() };return count,sink.failure
}
// Close records forbidden borrowed sink closure.
func (sink *trackedSink) Close() error { sink.closed=true;return nil }
// Flush records forbidden implicit transport flushing.
func (sink *trackedSink) Flush() error { sink.flushed=true;return nil }
// basicWriter snapshots a fixed mixed-compression entry set through actual public Add calls.
func basicWriter(t *testing.T,budgets ...*SerializerPackageBudget) *SerializerPackageZipWriter {
  t.Helper();writer,err:=SerializerPackageNewWriter(budgets...);if err!=nil { t.Fatal(err) }
  if err=writer.Add("stored.bin",[]byte{0,1,254,255});err!=nil { t.Fatal(err) }
  if err=writer.Add("data.bin",[]byte("independent deflate payload"),"deflate");err!=nil { t.Fatal(err) }
  if err=writer.Add("empty.bin",nil,"deflate");err!=nil { t.Fatal(err) };return writer
}
// TestWriterOutputs produces independently consumed ZIP32/ZIP64 and every descriptor form.
func TestWriterOutputs(t *testing.T) {
  for _,wide:=range []bool{false,true} { for _,descriptor:=range []SerializerPackageDescriptor{
      SerializerPackageDescriptorNone,SerializerPackageDescriptorSigned,SerializerPackageDescriptorUnsigned} {
    name:="output-";if wide { name+="wide-" };name+=strconv.Itoa(int(descriptor))+".zip"
    t.Run(name,func(t *testing.T){
      limits:=SerializerPackageDefaultLimits();limits.ForceZIP64=wide;limits.Descriptor=descriptor
      writer:=basicWriter(t,budgetFor(t,limits));defer writer.Close();data,err:=writer.Finish();if err!=nil { t.Fatal(err) }
      if err=os.WriteFile(name,data,0600);err!=nil { t.Fatal(err) }
      reader,err:=SerializerPackageOpen(data);if err!=nil { t.Fatal(err) };defer reader.Close()
      actual,err:=reader.Read("data.bin");if err!=nil||string(actual)!="independent deflate payload" { t.Fatal("Generated owning payload changed",err) }
      if result,err:=writer.Finish();result!=nil||err==nil { t.Fatal("Finished writer resumed") }
      if err=writer.Add("late.bin",nil);err==nil { t.Fatal("Finished writer accepted entries") }
    })
  } }
  t.Run("count-65536",func(t *testing.T){
    writer,err:=SerializerPackageNewWriter(budgetFor(t,generous()));if err!=nil { t.Fatal(err) };defer writer.Close()
    for index:=0;index<65536;index++ { if err=writer.Add("part"+strconv.Itoa(index),nil);err!=nil { t.Fatal(err) } }
    data,err:=writer.Finish();if err!=nil { t.Fatal(err) };if err=os.WriteFile("writer-65536.zip",data,0600);err!=nil { t.Fatal(err) }
    reader,err:=SerializerPackageOpen(data,budgetFor(t,generous()));if err!=nil { t.Fatal(err) };defer reader.Close()
    entries:=reader.Entries();names:=make([]string,len(entries));for index,entry:=range entries { names[index]=entry.Name }
    if len(entries)!=65536||!sort.StringsAreSorted(names) { t.Fatal("Invalid ZIP64 writer count/order") }
    if data,err=reader.Read("part65535");err!=nil||len(data)!=0 { t.Fatal("Missing final ZIP64 entry",err) }
  })
}

// TestWriterLifecycle checks immutable snapshots, canonical bytes, sticky errors and callback fencing.
func TestWriterLifecycle(t *testing.T) {
  t.Run("snapshot-canonical",func(t *testing.T){
    mutable:=[]byte("original");first,err:=SerializerPackageNewWriter();if err!=nil { t.Fatal(err) };defer first.Close()
    if err=first.Add("z.bin",mutable,"deflate");err!=nil { t.Fatal(err) };if err=first.Add("a.bin",[]byte{1});err!=nil { t.Fatal(err) };clear(mutable)
    one,err:=first.Finish();if err!=nil { t.Fatal(err) };second,err:=SerializerPackageNewWriter();if err!=nil { t.Fatal(err) };defer second.Close()
    if err=second.Add("a.bin",[]byte{1});err!=nil { t.Fatal(err) };if err=second.Add("z.bin",[]byte("original"),"deflate");err!=nil { t.Fatal(err) }
    two,err:=second.Finish();if err!=nil||!bytes.Equal(one,two) { t.Fatal("Canonical permutation or snapshot changed",err) }
  })
  for _,scenario:=range []string{"short","short-before-late-policy","transport","late-cancel","close","reentry","panic"} { t.Run(scenario,func(t *testing.T){
    sink:=&trackedSink{};marker:=errors.New("sink/cancellation marker");armed:=false
    limits:=SerializerPackageDefaultLimits();limits.CheckCancelled=func()error{if armed{return marker};return nil}
    writer:=basicWriter(t,budgetFor(t,limits));defer writer.Close();var nested error;panicMarker:=&struct{value int}{17}
    switch scenario {case "short":sink.short=true;case "transport":sink.failure=marker
    case "short-before-late-policy":sink.short=true;sink.callback=func(){armed=true}
    case "late-cancel":sink.callback=func(){armed=true};case "close":sink.callback=func(){writer.Close()}
    case "reentry":sink.callback=func(){_,nested=writer.Finish()};case "panic":sink.callback=func(){panic(panicMarker)}}
    var err error
    if scenario=="panic" { func(){defer func(){if recover()!=panicMarker{t.Fatal("Sink panic identity changed")}}();writer.WriteTo(sink);t.Fatal("Sink panic swallowed")}() } else {
      _,err=writer.WriteTo(sink);if err==nil { t.Fatal("Failed borrowed output succeeded") }
      if (scenario=="transport"||scenario=="late-cancel")&&err!=marker { t.Fatal("First application/transport error replaced",err) }
      if strings.HasPrefix(scenario,"short")&&!errors.Is(err,io.ErrShortWrite) { t.Fatal("Short sink error changed",err) }
      if scenario=="reentry"&&err!=nested { t.Fatal("Swallowed nested error replaced") }
    }
    if sink.calls!=1||sink.closed||sink.flushed { t.Fatal("Borrowed sink was resumed/closed/flushed") };calls:=sink.calls
    if _,later:=writer.WriteTo(sink);later==nil||sink.calls!=calls { t.Fatal("Failed writer resumed") }
  }) }
  t.Run("borrowed-success",func(t *testing.T){
    sink:=&trackedSink{};writer:=basicWriter(t);defer writer.Close();written,err:=writer.WriteTo(sink)
    if err!=nil||written!=int64(sink.Len())||sink.closed||sink.flushed { t.Fatal("Successful sink ownership/count",err) }
  })
  t.Run("admission-before-output",func(t *testing.T){
    limits:=SerializerPackageDefaultLimits();limits.MaxIOBytes=0;writer:=basicWriter(t,budgetFor(t,limits));defer writer.Close();sink:=&trackedSink{}
    _,err:=writer.WriteTo(sink);if sink.calls!=0 { t.Fatal("Output budget emitted a prefix") };requireCode(t,"RESOURCE_LIMIT",nil,err)
  })
}

// TestSavePublication exercises real synchronized sibling replacement and every named fault boundary.
func TestSavePublication(t *testing.T) {
  writer:=basicWriter(t);data,err:=writer.Finish();if err!=nil { t.Fatal(err) };writer.Close()
  for _,point:=range []string{"before_temporary","after_temporary","before_file_sync","after_file_sync","before_replace","after_replace","after_directory_sync"} {
    t.Run(point,func(t *testing.T){
      directory:=t.TempDir();destination:=filepath.Join(directory,"complete.zip");old:=[]byte("original complete generation")
      if err=os.WriteFile(destination,old,0600);err!=nil { t.Fatal(err) };marker:=errors.New("Save failure marker")
      err=SerializerPackageSaveBytes(destination,data,nil,func(actual string)error{if actual==point{return marker};return nil})
      actual,readErr:=os.ReadFile(destination);if readErr!=nil { t.Fatal(readErr) }
      if point=="after_replace"||point=="after_directory_sync" {
        requireCode(t,"PUBLICATION_UNCERTAIN",nil,err);if !errors.Is(err,marker)||!bytes.Equal(actual,data) { t.Fatal("Post-replacement cause/content changed",err) }
      } else if err!=marker||!bytes.Equal(actual,old) { t.Fatal("Pre-replacement cause/content changed",err) }
      files,readErr:=os.ReadDir(directory);if readErr!=nil||len(files)!=1 { t.Fatal("Save staging leaked",readErr) }
    })
  }
  t.Run("invalid-archive-before-files",func(t *testing.T){
    directory:=t.TempDir();destination:=filepath.Join(directory,"complete.zip");old:=[]byte("old complete")
    if err=os.WriteFile(destination,old,0600);err!=nil { t.Fatal(err) };calls:=0
    err=SerializerPackageSaveBytes(destination,fixtureBytes(t,"shared-crc_mismatch.zip"),nil,func(point string)error{calls++;return nil})
    if err==nil||calls!=0 { t.Fatal("Invalid archive entered publication") };actual,readErr:=os.ReadFile(destination)
    files,listErr:=os.ReadDir(directory);if readErr!=nil||listErr!=nil||!bytes.Equal(actual,old)||len(files)!=1 { t.Fatal("Rejected archive touched final/staging") }
  })
  t.Run("snapshot-before-fault-callback",func(t *testing.T){
    mutable:=append([]byte(nil),data...);destination:=filepath.Join(t.TempDir(),"complete.zip")
    err=SerializerPackageSaveBytes(destination,mutable,nil,func(point string)error{if point=="before_temporary"{clear(mutable)};return nil})
    actual,readErr:=os.ReadFile(destination);if err!=nil||readErr!=nil||!bytes.Equal(actual,data) { t.Fatal("Save snapshot changed after validation",err,readErr) }
  })
  for _,point:=range []string{"after_temporary","after_replace"} { t.Run("panic-"+point,func(t *testing.T){
    directory:=t.TempDir();destination:=filepath.Join(directory,"complete.zip");old:=[]byte("old complete")
    if err=os.WriteFile(destination,old,0600);err!=nil { t.Fatal(err) };marker:=&struct{value int}{17}
    func(){defer func(){if recover()!=marker{t.Fatal("Save panic identity changed")}}();SerializerPackageSaveBytes(destination,data,nil,func(actual string)error{if actual==point{panic(marker)};return nil});t.Fatal("Save panic swallowed")}()
    actual,readErr:=os.ReadFile(destination);expected:=old;if point=="after_replace" { expected=data }
    files,listErr:=os.ReadDir(directory);if readErr!=nil||listErr!=nil||!bytes.Equal(actual,expected)||len(files)!=1 { t.Fatal("Panic left partial file/staging") }
  }) }
}
