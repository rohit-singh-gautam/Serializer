// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
// srlPackageBuffer owns admitted logical bytes; spare Go slice capacity is outside this byte ledger.
type srlPackageBuffer struct { state *srlPackageState; data []byte; held int64; archive bool }
// reserve admits one complete append before allocation and counts archive output once before publication.
func (buffer *srlPackageBuffer) reserve(amount int64) error {
  maximum:=buffer.state.budget.limits.MaxArchiveBytes
  if amount>maximum-int64(len(buffer.data))||uint64(amount)>uint64(^uint(0)>>1)-uint64(len(buffer.data)) {
    return srlPackageFailure(SerializerPackageResourceLimit,"ZIP output extent exceeds limit","")
  }
  resource:=SerializerPackageWorkUnits;if buffer.archive { resource=SerializerPackageIOBytes }
  if err:=buffer.state.consume(resource,amount,"");err!=nil { return err }
  if err:=buffer.state.consume(SerializerPackageDecoderAllocationBytes,amount,"");err!=nil { return err }
  if err:=buffer.state.budget.hold(amount,"");err!=nil { return err }
  buffer.held+=amount;return nil
}
// append copies admitted caller/private bytes into owning internal storage.
func (buffer *srlPackageBuffer) append(data []byte) error {
  if err:=buffer.reserve(int64(len(data)));err!=nil { return err };buffer.data=append(buffer.data,data...);return nil
}
// appendText copies UTF-8 names without an unadmitted intermediate byte-array allocation.
func (buffer *srlPackageBuffer) appendText(data string) error {
  if err:=buffer.reserve(int64(len(data)));err!=nil { return err };buffer.data=append(buffer.data,data...);return nil
}
// number appends one explicit little-endian wire value without depending on Go object layout.
func (buffer *srlPackageBuffer) number(value uint64,width int) error {
  var bytes [8]byte
  for index:=0;index<width;index++ { bytes[index]=byte(value>>uint(index*8)) }
  return buffer.append(bytes[:width])
}
// close releases internal reservations without changing independently transferred output bytes.
func (buffer *srlPackageBuffer) close() { buffer.state.budget.release(buffer.held);buffer.held=0;buffer.data=nil }
// srlPackageCRC checks integrity in bounded batches with enclosing callback/lifetime fences.
func srlPackageCRC(data []byte,state *srlPackageState) (uint32,error) {
  checksum:=srlPackageCRC32.NewIEEE()
  for offset:=0;offset<len(data); {
    if err:=state.poll();err!=nil { return 0,err };count:=len(data)-offset
    if count>srlPackageChunkBytes { count=srlPackageChunkBytes };checksum.Write(data[offset:offset+count]);offset+=count
  }
  if err:=state.poll();err!=nil { return 0,err };return checksum.Sum32(),nil
}
// srlPackageWrite borrows a sink only after complete staging; ordinary io.Writer short writes fail.
func srlPackageWrite(destination srlPackageIO.Writer,data []byte,state *srlPackageState,charge bool) (int64,error) {
  var total int64
  for len(data)>0 {
    if err:=state.poll();err!=nil { return total,err };count:=len(data)
    if count>srlPackageChunkBytes { count=srlPackageChunkBytes }
    if charge { if err:=state.consume(SerializerPackageIOBytes,int64(count),"");err!=nil { return total,err } }
    n,err:=destination.Write(data[:count])
    // Preserve a short-write contract failure before invoking a later cancellation callback.
    if err!=nil { state.fail(err) }
    if n<count&&err==nil { state.fail(srlPackageIO.ErrShortWrite) }
    if n<0||n>count { state.fail(srlPackageIO.ErrShortWrite) }
    if n<0||n>count { return total,srlPackageIO.ErrShortWrite };total+=int64(n)
    if active:=state.poll();active!=nil { return total,active }
    if err!=nil { return total,err };if n!=count { return total,srlPackageIO.ErrShortWrite };data=data[count:]
  }
  return total,state.poll()
}
// SerializerPackageZipWriter snapshots entries and stages one canonical ZIP before any borrowed sink call.
// Finish/WriteTo are explicit; Close abandons staging without writing a trailer or closing a sink.
type SerializerPackageZipWriter struct {
  state srlPackageState
  parts []srlPackageZipPart
  names map[string]bool
  held int64
  finished bool
}
// srlPackageZipPart retains one admitted immutable payload snapshot and exact physical ZIP name.
type srlPackageZipPart struct { name string; data []byte; method uint16 }
// SerializerPackageNewWriter constructs a private writer without files, native callbacks or output.
func SerializerPackageNewWriter(budgets ...*SerializerPackageBudget) (*SerializerPackageZipWriter,error) {
  budget,err:=srlPackageBudget(budgets);if err!=nil { return nil,err }
  return &SerializerPackageZipWriter{state:srlPackageState{budget:budget},names:make(map[string]bool)},nil
}
// begin rejects finished state before entering the shared reentry/activity guard.
func (writer *SerializerPackageZipWriter) begin() error {
  if writer.finished { return &SerializerPackageError{Code:SerializerPackageInvalidState,Message:"Finished ZIP writer",Cause:writer.state.first} }
  return writer.state.begin()
}
// cleanup drops only private snapshots after all provider and borrowed transport callbacks finish.
func (writer *SerializerPackageZipWriter) cleanup() {
  writer.parts=nil;writer.names=nil;writer.state.budget.release(writer.held);writer.held=0
}
// Add snapshots one stored or raw-DEFLATE entry; omitted compression means stored.
func (writer *SerializerPackageZipWriter) Add(name string,data []byte,compression ...string) (err error) {
  if err=writer.begin();err!=nil { return err };complete:=false
  defer func(){writer.state.end(&err,complete);if writer.state.closed { writer.cleanup() }}()
  if len(name)>65535 { return srlPackageFailure(SerializerPackageInvalidName,"ZIP output filename exceeds wire limit","") }
  if !srlPackageUTF8.ValidString(name) { return srlPackageFailure(SerializerPackageInvalidName,"Invalid UTF-8 ZIP output filename",name) }
  if err=srlPackagePhysicalName(name);err!=nil { return err }
  kind:="stored";if len(compression)==1 { kind=compression[0] }
  if len(compression)>1||kind!="stored"&&kind!="deflate" { return srlPackageFailure(SerializerPackageInvalidOptions,"Invalid ZIP compression selection",name) }
  if writer.names[name] { return srlPackageFailure(SerializerPackageDuplicateName,"Duplicate ZIP output item",name) }
  state:=&writer.state;size:=int64(len(data))
  if _,err=srlPackageArray(size,state.budget.limits.MaxPartBytes,name);err!=nil { return err }
  if err=state.consume(SerializerPackageEntries,1,name);err!=nil { return err }
  if err=state.consume(SerializerPackageExpandedBytes,size,name);err!=nil { return err }
  if err=state.consume(SerializerPackageMetadataBytes,128+int64(len(name)),name);err!=nil { return err }
  if err=state.consume(SerializerPackageDecoderAllocationBytes,256+int64(len(name))*4,name);err!=nil { return err }
  if err=state.consume(SerializerPackageDecoderAllocationBytes,size,name);err!=nil { return err }
  if err=state.budget.hold(size,name);err!=nil { return err };writer.held+=size
  snapshot:=append([]byte(nil),data...);if err=state.poll();err!=nil { return err }
  method:=uint16(0);if kind=="deflate" { method=8 }
  writer.parts=append(writer.parts,srlPackageZipPart{name:name,data:snapshot,method:method});writer.names[name]=true
  complete=true;return nil
}
// prepare owns local/central metadata through exact archive completion, with no borrowed output effects.
func (writer *SerializerPackageZipWriter) prepare() (output *srlPackageBuffer,err error) {
  state:=&writer.state;output=&srlPackageBuffer{state:state,archive:true};prepared:=output;complete:=false
  var records []*srlPackageBuffer
  defer func(){for _,record:=range records { record.close() };if !complete { prepared.close() }}()
  if err=state.consume(SerializerPackageWorkUnits,int64(len(writer.parts)),"");err!=nil { return nil,err }
  // The writer has no published entry catalog, so sort its existing private descriptors in place.
  // This avoids an additional count-sized scratch allocation before canonical output construction.
  srlPackageSort.Slice(writer.parts,func(first,second int)bool{return writer.parts[first].name<writer.parts[second].name})
  for _,part:=range writer.parts {
    if err=state.poll();err!=nil { return nil,err }
    checksum,crcErr:=srlPackageCRC(part.data,state);if crcErr!=nil { return nil,crcErr }
    compressed:=part.data
    if part.method==8 {
      capacity:=state.budget.limits.MaxArchiveBytes-int64(len(output.data))
      remaining:=state.budget.limits.MaxBufferBytes-state.budget.held;if capacity>remaining { capacity=remaining }
      allocation:=state.budget.Remaining(SerializerPackageDecoderAllocationBytes);if capacity>allocation { capacity=allocation }
      options:=SerializerCompressionDefaultEncodeOptions(SerializerCompressionDeflate)
      options.MaxInputBytes=int64(len(part.data));options.MaxOutputBytes=capacity;options.CheckCancelled=state.poll
      compressed,err=SerializerCompressionCompress(part.data,options)
      if err!=nil {
        if state.first!=nil { return nil,state.first };var failure *SerializerCompressionError
        if srlPackageErrors.As(err,&failure)&&failure.Code==SerializerCompressionResourceLimit { return nil,srlPackageFailure(SerializerPackageResourceLimit,"ZIP compressed output exceeds limits",part.name) }
        return nil,srlPackageFailure(SerializerPackageCodec,"ZIP compression failed",part.name)
      }
      // The provider's bounded returned array is owned runtime storage; its internal workspace is not.
      // Admit its maximum before the provider call, then cumulatively charge only actual returned bytes.
      if err=state.consume(SerializerPackageDecoderAllocationBytes,int64(len(compressed)),part.name);err!=nil { return nil,err }
    }
    if err=state.poll();err!=nil { return nil,err }
    if err=writer.appendPart(output,&records,part,compressed,checksum);err!=nil { return nil,err }
  }
  start:=int64(len(output.data));for _,record:=range records { if err=output.append(record.data);err!=nil { return nil,err } }
  size:=int64(len(output.data))-start
  wide:=state.budget.limits.ForceZIP64||len(writer.parts)>=65535||start>=srlPackageUint32Max||size>=srlPackageUint32Max
  // Wire-field sequences remain explicit and independent of native struct packing.
  if wide {
    location:=len(output.data)
    fields:=[][2]uint64{{0x06064b50,4},{44,8},{45,2},{45,2},{0,4},{0,4},{uint64(len(writer.parts)),8},{uint64(len(writer.parts)),8},{uint64(size),8},{uint64(start),8},{0x07064b50,4},{0,4},{uint64(location),8},{1,4}}
    for _,field:=range fields { if err=output.number(field[0],int(field[1]));err!=nil { return nil,err } }
  }
  count,ordinarySize,ordinaryStart:=uint64(len(writer.parts)),uint64(size),uint64(start)
  if wide { count=65535;ordinarySize=uint64(srlPackageUint32Max);ordinaryStart=uint64(srlPackageUint32Max) }
  fields:=[][2]uint64{{0x06054b50,4},{0,2},{0,2},{count,2},{count,2},{ordinarySize,4},{ordinaryStart,4},{0,2}}
  for _,field:=range fields { if err=output.number(field[0],int(field[1]));err!=nil { return nil,err } }
  if err=state.poll();err!=nil { return nil,err };complete=true;return output,nil
}
)packagego"
R"packagego(
// srlPackageFields emits explicit ordered wire fields through the same pre-allocation ledger.
func srlPackageFields(buffer *srlPackageBuffer,fields [][2]uint64) error {
  for _,field:=range fields { if err:=buffer.number(field[0],int(field[1]));err!=nil { return err } };return nil
}
// appendPart appends one complete local record and retains its canonical central-directory record.
func (writer *SerializerPackageZipWriter) appendPart(output *srlPackageBuffer,records *[]*srlPackageBuffer,
    part srlPackageZipPart,packed []byte,checksum uint32) (err error) {
  state:=&writer.state;wide:=state.budget.limits.ForceZIP64||int64(len(part.data))>=srlPackageUint32Max||int64(len(packed))>=srlPackageUint32Max
  extraHeld:=int64(0);if part.method==8 { extraHeld=int64(len(packed)) }
  if err=state.budget.hold(extraHeld,part.name);err!=nil { return err };defer state.budget.release(extraHeld)
  descriptor:=state.budget.limits.Descriptor;local:=int64(len(output.data))
  flags:=uint64(0x800);if descriptor!=SerializerPackageDescriptorNone { flags|=8 }
  version:=uint64(20);extra:=uint64(0);if wide { version=45;extra=20 }
  localCRC,localPacked,localPlain:=uint64(checksum),uint64(len(packed)),uint64(len(part.data))
  if descriptor!=SerializerPackageDescriptorNone { localCRC=0;localPacked=0;localPlain=0 }
  if wide { localPacked=uint64(srlPackageUint32Max);localPlain=uint64(srlPackageUint32Max) }
  header:=&srlPackageBuffer{state:state};defer header.close()
  if err=srlPackageFields(header,[][2]uint64{{0x04034b50,4},{version,2},{flags,2},{uint64(part.method),2},{0,2},{33,2},
      {localCRC,4},{localPacked,4},{localPlain,4},{uint64(len(part.name)),2},{extra,2}});err!=nil { return err }
  if err=header.appendText(part.name);err!=nil { return err }
  if wide {
    plain,compressed:=uint64(len(part.data)),uint64(len(packed));if descriptor!=SerializerPackageDescriptorNone { plain=0;compressed=0 }
    if err=srlPackageFields(header,[][2]uint64{{1,2},{16,2},{plain,8},{compressed,8}});err!=nil { return err }
  }
  if err=output.append(header.data);err!=nil { return err };header.close()
  for position:=0;position<len(packed); {
    count:=len(packed)-position;if count>srlPackageChunkBytes { count=srlPackageChunkBytes }
    if err=output.append(packed[position:position+count]);err!=nil { return err };position+=count
  }
  if descriptor!=SerializerPackageDescriptorNone {
    if descriptor==SerializerPackageDescriptorSigned { if err=output.number(0x08074b50,4);err!=nil { return err } }
    width:=4;if wide { width=8 }
    if err=srlPackageFields(output,[][2]uint64{{uint64(checksum),4},{uint64(len(packed)),uint64(width)},{uint64(len(part.data)),uint64(width)}});err!=nil { return err }
  }
  wideOffset:=wide||local>=srlPackageUint32Max
  version=20;extra=0;if wideOffset { version=45;extra=12;if wide { extra=28 } }
  packedSize,plainSize,relative:=uint64(len(packed)),uint64(len(part.data)),uint64(local)
  if wide { packedSize=uint64(srlPackageUint32Max);plainSize=uint64(srlPackageUint32Max) }
  if wideOffset { relative=uint64(srlPackageUint32Max) }
  record:=&srlPackageBuffer{state:state};retained:=false
  defer func(){if !retained { record.close() }}()
  if err=srlPackageFields(record,[][2]uint64{{0x02014b50,4},{3*256+version,2},{version,2},{flags,2},{uint64(part.method),2},
      {0,2},{33,2},{uint64(checksum),4},{packedSize,4},{plainSize,4},{uint64(len(part.name)),2},{extra,2},
      {0,2},{0,2},{0,2},{0x81800000,4},{relative,4}});err!=nil { return err }
  if err=record.appendText(part.name);err!=nil { return err }
  if wideOffset {
    length:=uint64(8);if wide { length=24 }
    if err=srlPackageFields(record,[][2]uint64{{1,2},{length,2}});err!=nil { return err }
    if wide { if err=srlPackageFields(record,[][2]uint64{{uint64(len(part.data)),8},{uint64(len(packed)),8}});err!=nil { return err } }
    if err=record.number(uint64(local),8);err!=nil { return err }
  }
  if err=state.consume(SerializerPackageMetadataBytes,int64(len(record.data)),part.name);err!=nil { return err }
  *records=append(*records,record);retained=true;return state.poll()
}
// Finish returns one owned canonical ZIP after all providers and final cancellation checks succeed.
func (writer *SerializerPackageZipWriter) Finish() (result []byte,err error) {
  if err=writer.begin();err!=nil { return nil,err };complete:=false
  defer func(){writer.state.end(&err,complete);if writer.state.closed||writer.finished { writer.cleanup() }}()
  output,err:=writer.prepare();if err!=nil { return nil,err };defer output.close()
  if err=writer.state.poll();err!=nil { return nil,err }
  result=output.data;writer.finished=true;complete=true;return result,nil
}
// WriteTo implements io.WriterTo with bounded borrowed calls after complete archive staging.
// A transport failure can leave an acknowledged output prefix; no close/flush/finish is borrowed.
func (writer *SerializerPackageZipWriter) WriteTo(destination srlPackageIO.Writer) (written int64,err error) {
  if err=writer.begin();err!=nil { return 0,err };complete:=false
  defer func(){writer.state.end(&err,complete);if writer.state.closed||writer.finished { writer.cleanup() }}()
  if destination==nil { return 0,srlPackageFailure(SerializerPackageInvalidOptions,"Missing ZIP sink","") }
  output,err:=writer.prepare();if err!=nil { return 0,err };defer output.close()
  written,err=srlPackageWrite(destination,output.data,&writer.state,false);if err!=nil { return written,err }
  writer.finished=true;complete=true;return written,nil
}
// Close abandons staged bytes; closing inside a callback fences the outer operation before more I/O.
func (writer *SerializerPackageZipWriter) Close() error {
  writer.state.close();if !writer.state.busy { writer.cleanup() };return nil
}
)packagego"
