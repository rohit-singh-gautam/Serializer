// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
// SerializerPackageSource borrows stable random storage; Reader must not retain or later mutate p.
// SizeBytes is relative to this ReaderAt, so io.NewSectionReader supplies an explicit archive base.
type SerializerPackageSource struct { Reader srlPackageIO.ReaderAt; SizeBytes int64 }

// SerializerPackageEntry is an owning immutable-by-value physical ZIP catalog item.
// Entry names are logical metadata and are never used as extraction destinations.
type SerializerPackageEntry struct {
  Name string
  Method,Flags,CreatorSystem uint16
  CRC32 uint32
  CompressedBytes,ExpandedBytes int64
  ExternalAttributes uint32
  dataOffset,localOffset,recordEnd int64
}
// RegularFile excludes directory/link/device records from later OPC mapping without extracting them.
func (entry SerializerPackageEntry) RegularFile() bool {
  if srlPackageStrings.HasSuffix(entry.Name,"/") { return false }
  if (entry.CreatorSystem==0||entry.CreatorSystem==10||entry.CreatorSystem==14)&&entry.ExternalAttributes&24!=0 { return false }
  kind:=(entry.ExternalAttributes>>16)&0xf000
  return entry.CreatorSystem!=3&&entry.CreatorSystem!=19||kind==0||kind==0x8000
}
// SerializerPackageZipReader owns only byte snapshots and files opened from an explicit path/spool.
// Close never closes an application's ReaderAt/io.Reader. Calls require sequential caller access.
type SerializerPackageZipReader struct {
  state srlPackageState
  source srlPackageIO.ReaderAt
  extent int64
  owned *srlPackageOS.File
  temporary string
  snapshotBytes int64
  entries []SerializerPackageEntry
  names map[string]int
}
// SerializerPackageOpen indexes one complete ZIP and publishes no reader on any validation failure.
// Accepted sources are []byte, a filesystem path string, Source, or an explicitly spooled io.Reader.
func SerializerPackageOpen(source any,budgets ...*SerializerPackageBudget) (*SerializerPackageZipReader,error) {
  budget,err:=srlPackageBudget(budgets);if err!=nil { return nil,err }
  return srlPackageOpen(source,budget,nil)
}
// srlPackageOpen couples nested source/provider callbacks to an enclosing unpublished operation.
func srlPackageOpen(source any,budget *SerializerPackageBudget,parent *srlPackageState) (result *SerializerPackageZipReader,err error) {
  reader:=&SerializerPackageZipReader{state:srlPackageState{budget:budget,parent:parent},names:make(map[string]int)}
  if err=reader.state.begin();err!=nil { return nil,err };complete:=false
  defer func(){reader.state.end(&err,complete);if reader.state.closed { reader.cleanup() }}()
  if err=reader.attach(source);err!=nil { return nil,err }
  if err=reader.parse();err!=nil { return nil,err }
  if err=reader.state.poll();err!=nil { return nil,err };complete=true;return reader,nil
}
// Entries returns an independent catalog slice; editing it cannot change reader validation indexes.
func (reader *SerializerPackageZipReader) Entries() []SerializerPackageEntry {
  return append([]SerializerPackageEntry(nil),reader.entries...)
}
// Budget exposes the retained shared ledger without transferring transport ownership.
func (reader *SerializerPackageZipReader) Budget() *SerializerPackageBudget { return reader.state.budget }
// attach checks a complete source extent before snapshots or central-directory I/O.
func (reader *SerializerPackageZipReader) attach(input any) error {
  if err:=reader.state.poll();err!=nil { return err }
  switch source:=input.(type) {
  case []byte:
    size:=int64(len(source));if size>reader.state.budget.limits.MaxArchiveBytes { return srlPackageFailure(SerializerPackageResourceLimit,"Archive byte limit exceeded","") }
    if err:=reader.state.consume(SerializerPackageDecoderAllocationBytes,size,"");err!=nil { return err }
    if err:=reader.state.budget.hold(size,"");err!=nil { return err };reader.snapshotBytes=size
    snapshot:=append([]byte(nil),source...);reader.source=srlPackageBytes.NewReader(snapshot);reader.extent=size
  case string:
    file,err:=srlPackageOS.Open(source);if err!=nil { return err };reader.owned=file;reader.source=file
    info,err:=file.Stat();if err!=nil { return err };reader.extent=info.Size()
  case SerializerPackageSource:
    if source.Reader==nil { return srlPackageFailure(SerializerPackageInvalidOptions,"Missing package random source","") }
    reader.source=source.Reader;reader.extent=source.SizeBytes
  case srlPackageIO.Reader:
    if err:=reader.spool(source);err!=nil { return err }
  default:return srlPackageFailure(SerializerPackageInvalidOptions,"Unsupported package source","")
  }
  if reader.extent<0||reader.extent>reader.state.budget.limits.MaxArchiveBytes {
    return srlPackageFailure(SerializerPackageResourceLimit,"Archive byte limit exceeded","")
  }
  return reader.state.poll()
}
// spool creates private bounded storage only after capacity admission; it never closes the source.
func (reader *SerializerPackageZipReader) spool(source srlPackageIO.Reader) error {
  state:=&reader.state;limits:=state.budget.limits
  if !limits.AllowSpool||limits.SpoolDirectory=="" { return srlPackageFailure(SerializerPackageInvalidOptions,"Sequential input requires explicit private spooling","") }
  capacity:=limits.MaxBufferBytes;if capacity>srlPackageChunkBytes { capacity=srlPackageChunkBytes }
  if capacity==0||state.budget.Remaining(SerializerPackageIOBytes)==0 { return srlPackageFailure(SerializerPackageResourceLimit,"Private spool requires I/O and scratch capacity","") }
  if err:=state.budget.hold(capacity,"");err!=nil { return err };defer state.budget.release(capacity)
  if err:=state.consume(SerializerPackageDecoderAllocationBytes,capacity,"");err!=nil { return err }
  batch:=make([]byte,int(capacity))
  file,err:=srlPackageOS.CreateTemp(limits.SpoolDirectory,".serializer-zip-*");if err!=nil { return err }
  reader.owned=file;reader.temporary=file.Name();reader.source=file
  maximum:=limits.MaxArchiveBytes;if maximum>limits.MaxSpoolBytes { maximum=limits.MaxSpoolBytes }
  empty:=0
  for {
    if err=state.poll();err!=nil { return err }
    remaining:=maximum-reader.extent;requested:=capacity
    if remaining<requested { requested=remaining+1 }
    available:=state.budget.Remaining(SerializerPackageIOBytes);if requested>available { requested=available }
    if requested==0 { return srlPackageFailure(SerializerPackageResourceLimit,"Private spool I/O limit exceeded","") }
    count,failure:=source.Read(batch[:int(requested)])
    if failure!=nil&&failure!=srlPackageIO.EOF { state.fail(failure) }
    if count<0||int64(count)>requested { state.fail(srlPackageIO.ErrShortBuffer) }
    if err=state.poll();err!=nil { return err }
    if count<0||int64(count)>requested { return srlPackageIO.ErrShortBuffer }
    if count>0 {
      empty=0
      if int64(count)>remaining { return srlPackageFailure(SerializerPackageResourceLimit,"Private spool byte limit exceeded","") }
      if err=state.budget.charge(SerializerPackageIOBytes,int64(count),"");err!=nil { return err }
      for offset:=0;offset<count; {
        if err=state.poll();err!=nil { return err }
        written,writeErr:=file.Write(batch[offset:count]);if writeErr!=nil { return writeErr }
        if written<=0||written>count-offset { return srlPackageIO.ErrShortWrite };offset+=written
      }
      reader.extent+=int64(count)
    } else if failure==nil {
      empty++;if empty>=srlPackageMaxEmptyReads { return srlPackageIO.ErrNoProgress }
    }
    if failure==srlPackageIO.EOF { return state.poll() };if failure!=nil { return failure }
  }
}
// readAt admits full ranges and storage before bounded borrowed reads, including legal short results.
func (reader *SerializerPackageZipReader) readAt(offset,length int64) ([]byte,error) {
  if offset<0||length<0||offset>reader.extent||length>reader.extent-offset {
    return nil,srlPackageFailure(SerializerPackageInvalidArchive,"ZIP extent is outside archive","")
  }
  state:=&reader.state;count,err:=srlPackageArray(length,state.budget.limits.MaxBufferBytes,"");if err!=nil { return nil,err }
  if err=state.consume(SerializerPackageIOBytes,length,"");err!=nil { return nil,err }
  if err=state.consume(SerializerPackageDecoderAllocationBytes,length,"");err!=nil { return nil,err }
  if err=state.budget.hold(length,"");err!=nil { return nil,err };defer state.budget.release(length)
  output:=make([]byte,count)
  for position:=0;position<count; {
    if err=state.poll();err!=nil { return nil,err };requested:=count-position
    if requested>srlPackageChunkBytes { requested=srlPackageChunkBytes }
    n,failure:=reader.source.ReadAt(output[position:position+requested],offset+int64(position))
    // A completed invalid-progress result precedes any failure from the next policy poll.
    if failure!=nil&&failure!=srlPackageIO.EOF { state.fail(failure) }
    if n<0||n>requested { state.fail(srlPackageIO.ErrShortBuffer) }
    if n==0&&failure==srlPackageIO.EOF { state.fail(srlPackageIO.ErrUnexpectedEOF) }
    if n==0&&failure==nil { state.fail(srlPackageIO.ErrNoProgress) }
    if err=state.poll();err!=nil { return nil,err }
    if n<0||n>requested { return nil,srlPackageIO.ErrShortBuffer }
    position+=n
    if failure!=nil&&failure!=srlPackageIO.EOF { return nil,failure }
    if n==0 { if failure!=nil { return nil,srlPackageIO.ErrUnexpectedEOF };return nil,srlPackageIO.ErrNoProgress }
  }
  return output,nil
}
// srlPackagePhysicalName rejects traversal/absolute names while retaining exact ZIP identity.
func srlPackagePhysicalName(name string) error {
  if name==""||srlPackageStrings.HasPrefix(name,"/")||srlPackageStrings.ContainsAny(name,"\\\x00") {
    return srlPackageFailure(SerializerPackageInvalidName,"Invalid ZIP item name",name)
  }
  first:=0
  for index:=0;index<=len(name);index++ {
    if index<len(name)&&name[index]!='/' { continue };piece:=name[first:index]
    if piece=="."||piece==".."||piece==""&&index<len(name) {
      return srlPackageFailure(SerializerPackageInvalidName,"Invalid ZIP item segment",name)
    };first=index+1
  };return nil
}
// srlPackageCP437 is the legacy ZIP filename mapping, independent of locale or downloaded modules.
var srlPackageCP437=[]rune("ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ")
// srlPackageText preserves exact UTF-8 or CP437 scalar identities without Unicode normalization.
func srlPackageText(encoded []byte,unicode bool) (string,error) {
  if unicode {
    if !srlPackageUTF8.Valid(encoded) { return "",srlPackageFailure(SerializerPackageInvalidName,"Invalid UTF-8 ZIP item name","") }
    return string(encoded),nil
  }
  var result srlPackageStrings.Builder
  for _,value:=range encoded { if value<128 { result.WriteByte(value) } else { result.WriteRune(srlPackageCP437[int(value)-128]) } }
  return result.String(),nil
}
// srlPackageExtra reads only ordered ZIP64 fields whose ordinary values carry the wire sentinel.
func srlPackageExtra(data []byte,required ...bool) ([]int64,error) {
  var selected []byte;found:=false
  for position:=0;position<len(data); {
    kind,err:=srlPackageNumber(data,position,2);if err!=nil { return nil,err }
    size,err:=srlPackageNumber(data,position+2,2);if err!=nil { return nil,err };position+=4
    if size>int64(len(data)-position) { return nil,srlPackageFailure(SerializerPackageInvalidArchive,"Truncated ZIP extra field","") }
    if kind==1 {
      if found { return nil,srlPackageFailure(SerializerPackageInvalidArchive,"Duplicate ZIP64 extra field","") }
      selected=data[position:position+int(size)];found=true
    };position+=int(size)
  }
  values:=make([]int64,0,len(required));position:=0
  for _,needed:=range required { if needed {
    value,err:=srlPackageNumber(selected,position,8);if err!=nil { return nil,err };values=append(values,value);position+=8
  } };return values,nil
}
)packagego"
R"packagego(
// Read returns owning bytes only after exact size, raw-DEFLATE EOF and CRC validation.
// The first failure fences all subsequent I/O; caller-owned sources are never closed.
func (reader *SerializerPackageZipReader) Read(name string) (result []byte,err error) {
  if err=reader.state.begin();err!=nil { return nil,err };complete:=false
  defer func(){reader.state.end(&err,complete);if reader.state.closed { reader.cleanup() }}()
  index,exists:=reader.names[name];if !exists { return nil,srlPackageFailure(SerializerPackageMissingPart,"ZIP item is missing",name) }
  entry:=reader.entries[index];state:=&reader.state
  if err=state.consume(SerializerPackageExpandedBytes,entry.ExpandedBytes,name);err!=nil { return nil,err }
  if err=state.consume(SerializerPackageWorkUnits,entry.CompressedBytes,name);err!=nil { return nil,err }
  if err=state.consume(SerializerPackageWorkUnits,entry.ExpandedBytes,name);err!=nil { return nil,err }
  expected,err:=srlPackageArray(entry.ExpandedBytes,state.budget.limits.MaxPartBytes,name);if err!=nil { return nil,err }
  if entry.Method==0&&entry.CompressedBytes!=entry.ExpandedBytes { return nil,srlPackageFailure(SerializerPackageIntegrity,"Stored ZIP size mismatch",name) }
  if err=state.consume(SerializerPackageDecoderAllocationBytes,entry.ExpandedBytes,name);err!=nil { return nil,err }
  outputHeld:=int64(0);if entry.Method==8 { outputHeld=entry.ExpandedBytes }
  if err=state.budget.hold(outputHeld,name);err!=nil { return nil,err };defer state.budget.release(outputHeld)
  packed,err:=reader.readAt(entry.dataOffset,entry.CompressedBytes);if err!=nil { return nil,err }
  if err=state.budget.hold(int64(len(packed)),name);err!=nil { return nil,err };defer state.budget.release(int64(len(packed)))
  result=packed
  if entry.Method==8 {
    options:=SerializerCompressionDefaultDecodeOptions(SerializerCompressionDeflate)
    options.MaxInputBytes=int64(len(packed));options.MaxOutputBytes=int64(expected)
    options.MaxWindowBytes=32768;options.CheckCancelled=state.poll
    result,err=SerializerCompressionDecompress(packed,options)
    if err!=nil {
      if state.first!=nil { return nil,state.first }
      var compression *SerializerCompressionError
      if srlPackageErrors.As(err,&compression)&&compression.Code==SerializerCompressionResourceLimit {
        return nil,srlPackageFailure(SerializerPackageIntegrity,"ZIP expanded size mismatch",name)
      }
      return nil,srlPackageFailure(SerializerPackageInvalidArchive,"ZIP raw-DEFLATE payload failed",name)
    }
  }
  if len(result)!=expected { return nil,srlPackageFailure(SerializerPackageIntegrity,"ZIP expanded size mismatch",name) }
  checksum:=srlPackageCRC32.NewIEEE()
  for offset:=0;offset<len(result); {
    if err=state.poll();err!=nil { return nil,err };count:=len(result)-offset
    if count>srlPackageChunkBytes { count=srlPackageChunkBytes }
    checksum.Write(result[offset:offset+count]);offset+=count
  }
  if checksum.Sum32()!=entry.CRC32 { return nil,srlPackageFailure(SerializerPackageIntegrity,"ZIP CRC mismatch",name) }
  if err=state.poll();err!=nil { return nil,err };complete=true;return result,nil
}
// cleanup releases only private snapshots/opened files after the active borrowed callback has returned.
func (reader *SerializerPackageZipReader) cleanup() error {
  var first error
  if reader.owned!=nil { first=reader.owned.Close();reader.owned=nil }
  if reader.temporary!="" {
    err:=srlPackageOS.Remove(reader.temporary)
    if err==nil||srlPackageErrors.Is(err,srlPackageOS.ErrNotExist) { reader.temporary="" } else if first==nil { first=err }
  }
  reader.state.budget.release(reader.snapshotBytes);reader.snapshotBytes=0
  reader.source=nil;reader.entries=nil;reader.names=nil;return first
}
// Close abandons private state and never closes a borrowed source; callback cleanup is deferred safely.
func (reader *SerializerPackageZipReader) Close() error {
  reader.state.close();if reader.state.busy { return nil };return reader.cleanup()
}
)packagego"
R"packagego(
// srlPackageU16 reads a fixed-width field after its enclosing record span has been checked.
func srlPackageU16(data []byte,position int) uint16 { return srlPackageBinary.LittleEndian.Uint16(data[position:position+2]) }
// srlPackageU32 reads a fixed-width field after its enclosing record span has been checked.
func srlPackageU32(data []byte,position int) uint32 { return srlPackageBinary.LittleEndian.Uint32(data[position:position+4]) }
// parse validates complete directory/local/descriptor extents before any selected payload inflation.
func (reader *SerializerPackageZipReader) parse() error {
  state:=&reader.state;tailSize:=reader.extent
  if tailSize>srlPackageEndBytes+srlPackageMaxCommentBytes { tailSize=srlPackageEndBytes+srlPackageMaxCommentBytes }
  tailOffset:=reader.extent-tailSize;tail,err:=reader.readAt(tailOffset,tailSize);if err!=nil { return err }
  if err=state.budget.hold(tailSize,"");err!=nil { return err };defer state.budget.release(tailSize)
  selected:=-1
  for position:=len(tail)-srlPackageEndBytes;position>=0;position-- {
    if srlPackageU32(tail,position)==0x06054b50&&int(srlPackageU16(tail,position+20))==len(tail)-position-srlPackageEndBytes { selected=position;break }
  }
  if selected<0 { return srlPackageFailure(SerializerPackageInvalidArchive,"ZIP end directory is missing","") }
  disk,centralDisk:=srlPackageU16(tail,selected+4),srlPackageU16(tail,selected+6)
  diskCount,count:=int64(srlPackageU16(tail,selected+8)),int64(srlPackageU16(tail,selected+10))
  size,offset:=int64(srlPackageU32(tail,selected+12)),int64(srlPackageU32(tail,selected+16))
  end:=tailOffset+int64(selected);boundary:=end
  if disk!=0||centralDisk!=0||diskCount!=count { return srlPackageFailure(SerializerPackageUnsupportedFeature,"Multidisk ZIP is unsupported","") }
  // An actual locator selects ZIP64 independently of maximum-valued classic fields.
  // Without one, all classic count/size/offset values retain their full unsigned range.
  if end>=20 {
    locator,err:=reader.readAt(end-20,20);if err!=nil { return err }
    if srlPackageU32(locator,0)==0x07064b50 {
      location,err:=srlPackageNumber(locator,8,8);if err!=nil { return err }
      if srlPackageU32(locator,4)!=0||srlPackageU32(locator,16)!=1||location>end-20||end-20-location<56 {
        return srlPackageFailure(SerializerPackageInvalidArchive,"Invalid ZIP64 locator","")
      }
      record,err:=reader.readAt(location,56);if err!=nil { return err }
      recordSize,err:=srlPackageNumber(record,4,8);if err!=nil { return err }
      wideDiskCount,err:=srlPackageNumber(record,24,8);if err!=nil { return err }
      wideCount,err:=srlPackageNumber(record,32,8);if err!=nil { return err }
      wideSize,err:=srlPackageNumber(record,40,8);if err!=nil { return err }
      wideOffset,err:=srlPackageNumber(record,48,8);if err!=nil { return err }
      if srlPackageU32(record,0)!=0x06064b50||recordSize<44||recordSize!=end-20-location-12||
          srlPackageU32(record,16)!=0||srlPackageU32(record,20)!=0||wideDiskCount!=wideCount {
        return srlPackageFailure(SerializerPackageInvalidArchive,"Invalid ZIP64 directory","")
      }
      if srlPackageU16(record,14)>45 { return srlPackageFailure(SerializerPackageUnsupportedFeature,"Unsupported ZIP64 extraction version","") }
      if count!=65535&&count!=wideCount||size!=srlPackageUint32Max&&size!=wideSize||offset!=srlPackageUint32Max&&offset!=wideOffset {
        return srlPackageFailure(SerializerPackageInvalidArchive,"ZIP32 and ZIP64 directory records disagree","")
      }
      count,size,offset,boundary=wideCount,wideSize,wideOffset,location
    }
  }
  if offset>boundary||size!=boundary-offset { return srlPackageFailure(SerializerPackageInvalidArchive,"ZIP directory boundary mismatch","") }
  if err=state.consume(SerializerPackageEntries,count,"");err!=nil { return err }
  if err=state.consume(SerializerPackageMetadataBytes,size,"");err!=nil { return err }
  central,err:=reader.readAt(offset,size);if err!=nil { return err }
  if err=state.budget.hold(size,"");err!=nil { return err };defer state.budget.release(size)
  position:=0
  for index:=int64(0);index<count;index++ {
    if err=state.consume(SerializerPackageWorkUnits,1,"");err!=nil { return err }
    if position>len(central)||len(central)-position<46||srlPackageU32(central,position)!=0x02014b50 {
      return srlPackageFailure(SerializerPackageInvalidArchive,"Invalid ZIP central entry","")
    }
    flags,method:=srlPackageU16(central,position+8),srlPackageU16(central,position+10)
    crc:=srlPackageU32(central,position+16)
    compressed,expanded:=int64(srlPackageU32(central,position+20)),int64(srlPackageU32(central,position+24))
    nameSize,extraSize,commentSize:=int(srlPackageU16(central,position+28)),int(srlPackageU16(central,position+30)),int(srlPackageU16(central,position+32))
    local:=int64(srlPackageU32(central,position+42));length:=46+nameSize+extraSize+commentSize
    if length>len(central)-position { return srlPackageFailure(SerializerPackageInvalidArchive,"Truncated ZIP central fields","") }
    if srlPackageU16(central,position+34)!=0||flags & ^uint16(0x800|8|6)!=0||method!=0&&method!=8 {
      return srlPackageFailure(SerializerPackageUnsupportedFeature,"Unsupported ZIP entry","")
    }
    if srlPackageU16(central,position+6)>45 { return srlPackageFailure(SerializerPackageUnsupportedFeature,"Unsupported ZIP extraction version","") }
    if err=state.consume(SerializerPackageDecoderAllocationBytes,256+int64(nameSize)*4,"");err!=nil { return err }
    encoded:=central[position+46:position+46+nameSize]
    name,err:=srlPackageText(encoded,flags&0x800!=0);if err!=nil { return err }
    if err=srlPackagePhysicalName(name);err!=nil { return err }
    if _,exists:=reader.names[name];exists { return srlPackageFailure(SerializerPackageDuplicateName,"Duplicate physical ZIP item",name) }
    wide:=expanded==srlPackageUint32Max||compressed==srlPackageUint32Max
    values,err:=srlPackageExtra(central[position+46+nameSize:position+46+nameSize+extraSize],expanded==srlPackageUint32Max,compressed==srlPackageUint32Max,local==srlPackageUint32Max)
    if err!=nil { return err };next:=0
    if expanded==srlPackageUint32Max { expanded=values[next];next++ }
    if compressed==srlPackageUint32Max { compressed=values[next];next++ }
    if local==srlPackageUint32Max { local=values[next] }
    if expanded>state.budget.limits.MaxPartBytes { return srlPackageFailure(SerializerPackageResourceLimit,"Expanded part exceeds byte limit",name) }
    if local>offset||offset-local<30 { return srlPackageFailure(SerializerPackageInvalidArchive,"ZIP local header overlaps directory",name) }
    header,err:=reader.readAt(local,30);if err!=nil { return err }
    if srlPackageU32(header,0)!=0x04034b50||srlPackageU16(header,4)>45||srlPackageU16(header,6)!=flags||srlPackageU16(header,8)!=method {
      return srlPackageFailure(SerializerPackageInvalidArchive,"ZIP local and central headers disagree",name)
    }
    localName,localExtra:=int64(srlPackageU16(header,26)),int64(srlPackageU16(header,28))
    localCRC:=srlPackageU32(header,14)
    localExpanded32,localCompressed32:=int64(srlPackageU32(header,22)),int64(srlPackageU32(header,18))
    header=nil
    remaining:=offset-local-30
    if localName>remaining||localExtra>remaining-localName { return srlPackageFailure(SerializerPackageInvalidArchive,"ZIP local fields overlap directory",name) }
    data:=local+30+localName+localExtra
    if compressed>offset-data { return srlPackageFailure(SerializerPackageInvalidArchive,"ZIP payload overlaps directory",name) }
    actualName,err:=reader.readAt(local+30,localName);if err!=nil { return err }
    if !srlPackageBytes.Equal(actualName,encoded) { return srlPackageFailure(SerializerPackageInvalidArchive,"ZIP local filename mismatch",name) }
    actualName=nil
    localExpanded,localCompressed:=localExpanded32,localCompressed32
    extras,err:=reader.readAt(local+30+localName,localExtra);if err!=nil { return err }
    values,err=srlPackageExtra(extras,localExpanded==srlPackageUint32Max,localCompressed==srlPackageUint32Max);if err!=nil { return err };next=0
    extras=nil
    if localExpanded==srlPackageUint32Max { localExpanded=values[next];next++ }
    if localCompressed==srlPackageUint32Max { localCompressed=values[next] }
    finish:=data+compressed
    if flags&8!=0 {
      wide=wide||localCompressed32==srlPackageUint32Max||localExpanded32==srlPackageUint32Max
      descriptorSize:=int64(12);if wide { descriptorSize=20 }
      if descriptorSize>offset-finish { return srlPackageFailure(SerializerPackageInvalidArchive,"ZIP descriptor overlaps directory",name) }
      descriptor,err:=reader.readAt(finish,descriptorSize);if err!=nil { return err }
      matches,err:=srlPackageDescriptorMatches(descriptor,wide,crc,compressed,expanded);signed:=false
      if !matches&&srlPackageU32(descriptor,0)==0x08074b50 {
        if descriptorSize+4>offset-finish { return srlPackageFailure(SerializerPackageInvalidArchive,"Signed ZIP descriptor overlaps directory",name) }
        descriptor=nil
        descriptor,err=reader.readAt(finish+4,descriptorSize);if err!=nil { return err };signed=true
        matches,err=srlPackageDescriptorMatches(descriptor,wide,crc,compressed,expanded);if err!=nil { return err }
      }
      if err!=nil { return err }
      if !matches||localCRC!=0&&localCRC!=crc||
          localExpanded!=0&&localExpanded!=expanded||localCompressed!=0&&localCompressed!=compressed {
        return srlPackageFailure(SerializerPackageIntegrity,"ZIP descriptor mismatch",name)
      }
      finish+=descriptorSize;if signed { finish+=4 }
    } else if localCRC!=crc||localExpanded!=expanded||localCompressed!=compressed {
      return srlPackageFailure(SerializerPackageIntegrity,"ZIP local CRC/size mismatch",name)
    }
    entry:=SerializerPackageEntry{Name:name,Method:method,Flags:flags,CRC32:crc,
      CompressedBytes:compressed,ExpandedBytes:expanded,dataOffset:data,localOffset:local,recordEnd:finish,
      CreatorSystem:srlPackageU16(central,position+4)>>8,ExternalAttributes:srlPackageU32(central,position+38)}
    reader.names[name]=len(reader.entries);reader.entries=append(reader.entries,entry);position+=length
  }
  if position!=len(central) { return srlPackageFailure(SerializerPackageInvalidArchive,"Unexpected ZIP directory records","") }
  // Fixed-width offset indexes preserve physical entry order and have an explicit scratch reservation.
  const indexBytes=int64(8)
  if int64(len(reader.entries))>(1<<63-1)/indexBytes { return srlPackageFailure(SerializerPackageResourceLimit,"ZIP index scratch extent overflow","") }
  scratchBytes:=int64(len(reader.entries))*indexBytes
  if err=state.consume(SerializerPackageDecoderAllocationBytes,scratchBytes,"");err!=nil { return err }
  if err=state.budget.hold(scratchBytes,"");err!=nil { return err };defer state.budget.release(scratchBytes)
  indexes:=make([]uint64,len(reader.entries));for index:=range indexes { indexes[index]=uint64(index) }
  srlPackageSort.Slice(indexes,func(first,second int)bool { return reader.entries[indexes[first]].localOffset<reader.entries[indexes[second]].localOffset })
  for index:=1;index<len(indexes);index++ {
    if err=state.consume(SerializerPackageWorkUnits,1,"");err!=nil { return err }
    if reader.entries[indexes[index-1]].recordEnd>reader.entries[indexes[index]].localOffset {
      return srlPackageFailure(SerializerPackageInvalidArchive,"Overlapping ZIP local records","")
    }
  };return state.poll()
}
// srlPackageDescriptorMatches distinguishes an unsigned CRC equal to the optional descriptor signature.
func srlPackageDescriptorMatches(data []byte,wide bool,crc uint32,compressed,expanded int64) (bool,error) {
  if srlPackageU32(data,0)!=crc { return false,nil }
  width:=4;if wide { width=8 }
  packed,err:=srlPackageNumber(data,4,width);if err!=nil { return false,err }
  plain,err:=srlPackageNumber(data,4+width,width);if err!=nil { return false,err }
  return srlPackageU32(data,0)==crc&&packed==compressed&&plain==expanded,nil
}
)packagego"
