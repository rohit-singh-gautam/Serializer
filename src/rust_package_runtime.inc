// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustpackage(
/// Bounded ZIP/OPC operations share one operation ledger and never open external targets.
pub mod @package_name@ {
    use std::collections::{BTreeMap,BTreeSet};
    use std::sync::Arc;
    use super::@compression_name@ as compression;
    use super::@xml_operation@ as native_operation;
    /// Original caller and native errors retain their concrete error types.
    pub type Error=Arc<dyn std::error::Error+Send+Sync>;
    /// No failed operation returns a partially constructed candidate.
    pub type Result<T>=std::result::Result<T,Error>;
    /// Stable package failure categories, distinct from native codec and transport errors.
    #[derive(Clone,Copy,Debug,PartialEq,Eq)]
    pub enum Code { InvalidZip,Integrity,InvalidName,InvalidMetadata,InvalidRelationship,MissingPart,Semantic,ResourceLimit,Codec,InvalidOptions }
    /// Bounded diagnostics identify a package part, declared field and relationship.
    #[derive(Debug)]
    pub struct Failure {pub code:Code,pub message:&'static str,pub part:String,pub field:String,pub link:String,pub cause:Option<Error>}
    impl std::fmt::Display for Failure {
        /// Describe a stable failure without copying untrusted payload excerpts.
        fn fmt(&self,f:&mut std::fmt::Formatter<'_>)->std::fmt::Result {write!(f,"{} (part={}, field={}, link={})",self.message,self.part,self.field,self.link)}
    }
    impl std::error::Error for Failure {
        /// Retain native/provider failure objects when adding bounded package resource locations.
        fn source(&self)->Option<&(dyn std::error::Error+'static)>{self.cause.as_ref().map(|cause|cause.as_ref() as &(dyn std::error::Error+'static))}
    }
    /// Retain the original concrete caller/native error in a shared owning handle.
    pub fn native_error<E:std::error::Error+Send+Sync+'static>(error:E)->Error {Arc::new(error)}
    /// Create location-aware infrastructure failures without payload snippets.
    pub(super) fn fail(code:Code,message:&'static str,part:&str,field:&str,link:&str)->Error {Arc::new(Failure{code,message,part:part.into(),field:field.into(),link:link.into(),cause:None})}
    /// Count cumulative resource admissions across ZIP, XML, references and preservation.
    #[derive(Clone,Copy,Debug)]
    #[repr(usize)]
    pub enum Resource {PhysicalBytes,ExpandedBytes,MetadataBytes,Entries,Relationships,References,WorkUnits,XmlNodes,XmlAttributes,ScalarBytes,DecoderAllocationBytes,PreservedBytes}
    const RESOURCE_COUNT:usize=Resource::PreservedBytes as usize+1;
    const CHUNK_BYTES:usize=32768;
    const MAX_NAME_BYTES:usize=4096;
    const CT:&str="http://schemas.openxmlformats.org/package/2006/content-types";
    const RELS:&str="http://schemas.openxmlformats.org/package/2006/relationships";
    const RELS_MIME:&str="application/vnd.openxmlformats-package.relationships+xml";
    /// Logical operation caps; native allocator overhead is not a hard process-memory quota.
    #[derive(Clone,Copy)]
    pub struct Limits {pub maximum:[u64;RESOURCE_COUNT],pub max_part_bytes:usize,pub max_archive_bytes:usize,pub max_depth:usize}
    impl Default for Limits {
        /// Select finite owning-document limits independently of platform word size.
        fn default()->Self {Self{maximum:[256*1024*1024,256*1024*1024,16*1024*1024,100000,100000,100000,10000000,1000000,1000000,64*1024*1024,512*1024*1024,64*1024*1024],max_part_bytes:64*1024*1024,max_archive_bytes:256*1024*1024,max_depth:128}}
    }
    /// Borrowed cancellation and role overrides apply throughout one synchronous operation.
    pub struct Budget<'a> {pub limits:Limits,pub used:[u64;RESOURCE_COUNT],pub check_cancelled:Option<&'a dyn Fn()->Result<()>>,pub force_zip64:bool,pub overrides:BTreeMap<String,u16>}
    impl Default for Budget<'_> {
        /// Begin a fresh operation without hidden global counters.
        fn default()->Self {Self{limits:Limits::default(),used:[0;RESOURCE_COUNT],check_cancelled:None,force_zip64:false,overrides:BTreeMap::new()}}
    }
    impl Budget<'_> {
        /// Observe the caller's original cancellation failure at every bounded stage.
        pub fn poll(&self)->Result<()> {if let Some(check)=self.check_cancelled {check()?;} Ok(())}
        /// Admit arithmetic before allocation or traversal, retaining precise role context.
        pub fn consume(&mut self,resource:Resource,count:usize,part:&str,field:&str,link:&str)->Result<()> {self.poll()?;let slot=resource as usize;let total=self.used[slot].checked_add(count as u64).filter(|value|*value<=self.limits.maximum[slot]).ok_or_else(||fail(Code::ResourceLimit,"Shared package resource exhausted",part,field,link))?;self.used[slot]=total;Ok(())}
        /// Derive per-codec caps from the same cumulative operation ledger.
        pub fn xml_limits(&self)->super::XmlLimits {super::XmlLimits{max_bytes:self.limits.max_part_bytes,max_text_bytes:self.limits.max_part_bytes,max_nodes:(self.limits.maximum[Resource::XmlNodes as usize].saturating_sub(self.used[Resource::XmlNodes as usize])).min(usize::MAX as u64) as usize,max_attributes:(self.limits.maximum[Resource::XmlAttributes as usize].saturating_sub(self.used[Resource::XmlAttributes as usize])).min(usize::MAX as u64) as usize,max_depth:self.limits.max_depth}}
    }
    /// Borrow one native codec location and the existing cumulative package ledger synchronously.
    pub struct XmlOperation<'a,'b> {pub budget:&'a mut Budget<'b>,pub role:&'a Role,pub part:&'a str,pub link:&'a str,first:Option<Error>,profile:Option<&'a RoleProfile>}
    impl<'a,'b> XmlOperation<'a,'b> {
        /// Keep separate codec contexts independent while borrowing the caller's operation ledger.
        pub fn new(budget:&'a mut Budget<'b>,role:&'a Role,part:&'a str,link:&'a str)->Self {Self{budget,role,part,link,first:None,profile:None}}
        /// Attach immutable generated type/vocabulary metadata only for this package invocation.
        pub fn with_profile(budget:&'a mut Budget<'b>,profile:&'a RoleProfile,part:&'a str,link:&'a str)->Self{let mut value=Self::new(budget,&profile.role,part,link);value.profile=Some(profile);value}
        /// Convert native wire admissions through the same first-failure latch as XML operations.
        fn native_consume(&mut self,kind:&str,count:usize)->super::SrlResult<()>{let selected=if kind=="allocation"{native_operation::Resource::DecoderAllocationBytes}else{native_operation::Resource::WorkUnits};native_operation::Operation::consume(self,selected,count,"").map_err(|_|super::Error("Package native operation failed"))}
        /// Classify only known native quota origins while preserving original caller error handles.
        fn wire_result<T>(&mut self,result:super::SrlResult<T>)->Result<T>{match result{Ok(value)=>self.finish(value),Err(error)=>{if let Some(first)=&self.first{return Err(first.clone());}let resource=matches!(error.0,"Message byte limit"|"String limit"|"Element limit"|"Depth limit"|"Encode depth limit"|"Array allocation overflow"|"Output allocation");Err(Arc::new(Failure{code:if resource{Code::ResourceLimit}else{Code::Codec},message:"Native package codec rejected payload",part:self.part.into(),field:self.role.field.into(),link:self.link.into(),cause:Some(native_error(error))}))}}}
        /// Encode the existing native model directly into bounded, privately staged bytes.
        pub(super) fn encode_native(&mut self,codec:PayloadCodec,encode:impl FnOnce(&mut super::SrlWriter<'_>)->super::SrlResult<()>)->Result<Vec<u8>>{let maximum=self.budget.limits.max_part_bytes;let result={let mut operation=|kind:&str,count|self.native_consume(kind,count);let mut writer=super::SrlWriter::new_with_operation(if codec==PayloadCodec::Json{super::Protocol::Json}else{super::Protocol::BinaryNone},maximum,&mut operation);encode(&mut writer).map(|()|writer.bytes)};self.wire_result(result)}
        /// Decode one exact native payload into an unpublished owning model under the original ledger.
        pub(super) fn decode_native<T>(&mut self,codec:PayloadCodec,bytes:&[u8],decode:impl FnOnce(&mut super::SrlReader<'_>)->super::SrlResult<T>)->Result<T>{let limits=super::Limits{max_bytes:self.budget.limits.max_part_bytes,max_string_bytes:self.budget.limits.max_part_bytes,max_elements:self.budget.limits.maximum[Resource::WorkUnits as usize].saturating_sub(self.budget.used[Resource::WorkUnits as usize]).min(1000000) as usize,max_depth:self.budget.limits.max_depth,..Default::default()};self.budget.consume(Resource::WorkUnits,bytes.len(),self.part,self.role.field,self.link)?;let result={let mut operation=|kind:&str,count|self.native_consume(kind,count);(move||{let mut reader=super::SrlReader::new_with_operation(bytes,if codec==PayloadCodec::Json{super::Protocol::Json}else{super::Protocol::BinaryNone},limits,&mut operation)?;let value=decode(&mut reader)?;reader.finish()?;Ok(value)})()};self.wire_result(result)}
        /// Preserve the exact first caller error before attaching context to native codec failures.
        fn native_result<T>(&mut self,result:native_operation::Result<T>)->Result<T>{result.map_err(|error|{if let Some(first)=&self.first{return first.clone();}if error.downcast_ref::<super::Error>().is_some(){Arc::new(Failure{code:Code::Codec,message:"Native XML codec rejected payload",part:self.part.into(),field:self.role.field.into(),link:self.link.into(),cause:Some(error)}) as Error}else{error}})}
        /// Encode the selected native model with the same package operation and existing local policies.
        pub fn encode<T:native_operation::Codec>(&mut self,value:&T)->Result<String>{let limits=self.budget.xml_limits();let result=<T as native_operation::Codec>::encode_xml_with_operation(value,native_operation::Options{limits,..Default::default()},self);self.native_result(result)}
        /// Decode only the package role's prevalidated matching root type into a private native value.
        pub fn decode<T:native_operation::Codec>(&mut self,text:&str)->Result<T>{let limits=self.budget.xml_limits();let expected=self.role.expected_type;let result=<T as native_operation::Codec>::decode_package_with_operation(text,native_operation::Options{limits,..Default::default()},expected,self);self.native_result(result)}
        /// Return a successful callback only while the shared context remains unfenced.
        pub fn finish<T>(&mut self,value:T)->Result<T>{if let Some(first)=&self.first{return Err(first.clone());}self.budget.poll()?;Ok(value)}
    }
    impl native_operation::Operation for XmlOperation<'_,'_> {
        /// Admit opaque XML-only bytes before any companion retention under the same first-failure latch.
        fn preserve_bytes(&mut self,count:usize,field:&str)->Result<()>{if let Some(first)=&self.first{return Err(first.clone());}let result=self.budget.consume(Resource::PreservedBytes,count,self.part,if field.is_empty(){self.role.field}else{field},self.link);if let Err(error)=&result{self.first=Some(error.clone());}result}
        /// Root capabilities are scoped to the selected package model and never grant opaque understanding.
        fn package_contract(&self)->Option<native_operation::PackageContract>{self.profile.map(|profile|native_operation::PackageContract{expected_type:profile.role.expected_type,requires_type:profile.requires_type,vocabularies:profile.vocabularies})}
        /// Map explicit native counters to package counters without relying on enum ordinals.
        fn consume(&mut self,resource:native_operation::Resource,count:usize,field:&str)->Result<()>{if let Some(first)=&self.first{return Err(first.clone());}let resource=match resource{native_operation::Resource::WorkUnits=>Resource::WorkUnits,native_operation::Resource::DecoderAllocationBytes=>Resource::DecoderAllocationBytes,native_operation::Resource::XmlNodes=>Resource::XmlNodes,native_operation::Resource::XmlAttributes=>Resource::XmlAttributes,native_operation::Resource::ScalarBytes=>Resource::ScalarBytes,native_operation::Resource::References=>Resource::References};let result=self.budget.consume(resource,count,self.part,if field.is_empty(){self.role.field}else{field},self.link);if let Err(error)=&result{self.first=Some(error.clone());}result}
        /// Preserve the original cancellation Arc and prevent any later callback from replacing it.
        fn poll(&mut self,_field:&str)->Result<()>{if let Some(first)=&self.first{return Err(first.clone());}let result=self.budget.poll();if let Err(error)=&result{self.first=Some(error.clone());}result}
        /// Classify native local quotas explicitly while retaining their native owner/member origin.
        fn limit(&mut self,message:&'static str,field:&str)->Error{if let Some(first)=&self.first{return first.clone();}let error=fail(Code::ResourceLimit,message,self.part,if field.is_empty(){self.role.field}else{field},self.link);self.first=Some(error.clone());error}
        /// Locate native semantic failures without translating arbitrary callback error handles.
        fn codec(&mut self,error:super::Error,field:&str)->Error{if let Some(first)=&self.first{return first.clone();}let error=Arc::new(Failure{code:Code::Codec,message:"Native XML codec rejected payload",part:self.part.into(),field:if field.is_empty(){self.role.field}else{field}.into(),link:self.link.into(),cause:Some(native_error(error))}) as Error;self.first=Some(error.clone());error}
    }
)rustpackage"
#include "rust_package_companion_runtime.inc"
R"rustpackage(
    /// Decode explicit wire integers after checking the complete source span.
    fn number(data:&[u8],offset:usize,count:usize)->Result<u64> {if count>8||offset>data.len()||count>data.len()-offset {return Err(fail(Code::InvalidZip,"Truncated ZIP integer","","",""));}Ok(data[offset..offset+count].iter().enumerate().fold(0,|value,(index,byte)|value|(u64::from(*byte)<<(index*8))))}
    /// Convert untrusted offsets only when the whole span is addressable.
    fn extent(data:&[u8],offset:u64,length:u64)->Result<(usize,usize)> {let end=offset.checked_add(length).filter(|value|*value<=data.len() as u64).ok_or_else(||fail(Code::InvalidZip,"ZIP span outside source","","",""))?;Ok((offset as usize,end as usize))}
    /// Append fixed-width wire integers independently of native object layout.
    fn word(bytes:&mut Vec<u8>,value:u64,count:usize) {bytes.extend_from_slice(&value.to_le_bytes()[..count]);}
    /// Compute standard CRC32 using bounded fixed workspace and cooperative intervals.
    fn crc(bytes:&[u8],budget:&Budget<'_>)->Result<u32> {let mut value=u32::MAX;for (index,byte) in bytes.iter().enumerate(){if index%4096==0 {budget.poll()?;}value^=u32::from(*byte);for _ in 0..8 {value=(value>>1)^if value&1!=0 {0xedb88320}else{0};}}Ok(!value)}
    /// Reject ambiguous or unsafe ZIP names without interpreting filesystem paths.
    fn zip_name(value:&str)->Result<()> {if value.is_empty()||value.len()>MAX_NAME_BYTES||value.starts_with('/')||value.contains(['\\','\0',':'])||value.split('/').any(|part|part.is_empty()||part=="."||part=="..") {return Err(fail(Code::InvalidName,"Invalid ZIP entry name",value,"",""));}Ok(())}
    /// One admitted directory entry borrows physical archive storage by offset.
    #[derive(Clone,Debug)]
    pub struct Entry {pub name:String,pub method:u16,pub crc:u32,pub expanded:u64,pub compressed:u64,pub offset:u64,pub flags:u16,pub data_offset:usize,pub end_offset:usize}
    /// A complete validated central/local inventory cannot expose unindexed physical bytes.
    pub struct ZipReader<'a> {bytes:&'a [u8],pub entries:Vec<Entry>,index:BTreeMap<&'a str,usize>,non_files:BTreeSet<&'a str>}
    impl<'a> ZipReader<'a> {
        /// Validate ZIP32/ZIP64, descriptor forms, duplicate names and all source extents before reads.
        pub fn new(bytes:&'a [u8],budget:&mut Budget<'_>)->Result<Self> {Self::new_policy(bytes,budget,false)}
        /// Select the inert physical-name policy only for the OPC loader.
        fn new_policy(bytes:&'a [u8],budget:&mut Budget<'_>,opc:bool)->Result<Self> {
            budget.consume(Resource::PhysicalBytes,bytes.len(),"","","")?;if bytes.len()>budget.limits.max_archive_bytes||bytes.len()<22 {return Err(fail(Code::InvalidZip,"Invalid ZIP source size","","",""));}
            let earliest=bytes.len().saturating_sub(22+u16::MAX as usize);let end=(earliest..=bytes.len()-22).rev().find(|offset|bytes[*offset..].starts_with(&[0x50,0x4b,5,6])&&number(bytes,*offset+20,2).ok()==Some((bytes.len()-*offset-22) as u64)).ok_or_else(||fail(Code::InvalidZip,"ZIP end record missing","","",""))?;
            if number(bytes,end+4,2)?!=0||number(bytes,end+6,2)?!=0||number(bytes,end+8,2)?!=number(bytes,end+10,2)? {return Err(fail(Code::InvalidZip,"Split ZIP archives are unavailable","","",""));}
            let(mut count,mut size,mut offset)=(number(bytes,end+10,2)?,number(bytes,end+12,4)?,number(bytes,end+16,4)?);let mut directory_end=end;
            if size==u32::MAX as u64||offset==u32::MAX as u64||end>=20&&number(bytes,end-20,4)?==0x07064b50 {
                if end<20||number(bytes,end-20,4)?!=0x07064b50||number(bytes,end-16,4)?!=0||number(bytes,end-4,4)?!=1 {return Err(fail(Code::InvalidZip,"ZIP64 locator invalid","","",""));}
                let(z,_)=extent(bytes,number(bytes,end-12,8)?,56)?;if number(bytes,z,4)?!=0x06064b50||number(bytes,z+4,8)?<44||number(bytes,z+16,4)?!=0||number(bytes,z+20,4)?!=0||number(bytes,z+24,8)?!=number(bytes,z+32,8)? {return Err(fail(Code::InvalidZip,"ZIP64 end record invalid","","",""));}
                if z.checked_add(12).and_then(|value|value.checked_add(number(bytes,z+4,8).ok()? as usize))!=Some(end-20) {return Err(fail(Code::InvalidZip,"ZIP64 end record extent invalid","","",""));}
                count=number(bytes,z+32,8)?;size=number(bytes,z+40,8)?;offset=number(bytes,z+48,8)?;directory_end=z;
            }
            let(start,finish)=extent(bytes,offset,size)?;if finish!=directory_end||count>size/46 {return Err(fail(Code::InvalidZip,"ZIP directory extent/count mismatch","","",""));}
            let mut entries=Vec::new();let mut index=BTreeMap::new();let mut names=BTreeSet::new();let mut spans=Vec::new();let mut cursor=start;let mut non_files=BTreeSet::new();
            for _ in 0..count {
                budget.consume(Resource::Entries,1,"","","")?;budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<Entry>(),"","","")?;
                if cursor.checked_add(46).is_none_or(|end|end>finish)||number(bytes,cursor,4)?!=0x02014b50 {return Err(fail(Code::InvalidZip,"ZIP directory record invalid","","",""));}
                let(flags,method)=(number(bytes,cursor+8,2)? as u16,number(bytes,cursor+10,2)? as u16);if flags&!0x080e!=0||!matches!(method,0|8)||number(bytes,cursor+34,2)?!=0 {return Err(fail(Code::InvalidZip,"ZIP flags, method or disk unsupported","","",""));}
                let(n,x,c)=(number(bytes,cursor+28,2)? as usize,number(bytes,cursor+30,2)? as usize,number(bytes,cursor+32,2)? as usize);if n+x+c>usize::from(u16::MAX)-46 {return Err(fail(Code::InvalidZip,"OPC ZIP central header exceeds 65535 bytes","","",""));}let next=cursor.checked_add(46+n+x+c).filter(|value|*value<=finish).ok_or_else(||fail(Code::InvalidZip,"ZIP directory fields overflow","","",""))?;
                let wire_name=std::str::from_utf8(&bytes[cursor+46..cursor+46+n]).map_err(native_error)?;let name=wire_name.to_owned();zip_item_name(&name,opc)?;if !names.insert(if opc{name.clone()}else{name.to_ascii_lowercase()}) {return Err(fail(Code::InvalidName,"Equivalent ZIP entry names",&name,"",""));}
                if opc{let creator=number(bytes,cursor+4,2)?>>8;let attributes=number(bytes,cursor+38,4)?;let regular=!name.ends_with('/')&&(!matches!(creator,0|10|14)||attributes&0x18==0)&&(!matches!(creator,3|19)||matches!((attributes>>16)&0o170000,0|0o100000));if !regular{budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<&str>(),&name,"physical","")?;non_files.insert(wire_name);}}
                let mut expanded=number(bytes,cursor+24,4)?;let mut compressed=number(bytes,cursor+20,4)?;let mut local=number(bytes,cursor+42,4)?;let zip64=expanded==u32::MAX as u64||compressed==u32::MAX as u64||local==u32::MAX as u64;
                let mut extra=cursor+46+n;let extra_end=extra+x;let mut found=false;
                while extra<extra_end {if extra+4>extra_end {return Err(fail(Code::InvalidZip,"Truncated ZIP extra",&name,"",""));}let tag=number(bytes,extra,2)?;let length=number(bytes,extra+2,2)? as usize;extra+=4;if length>extra_end-extra {return Err(fail(Code::InvalidZip,"ZIP extra overflow",&name,"",""));}if tag==1 {if found {return Err(fail(Code::InvalidZip,"Duplicate ZIP64 extra",&name,"",""));}found=true;let mut at=extra;for value in [&mut expanded,&mut compressed,&mut local] {if *value==u32::MAX as u64 {if at+8>extra+length {return Err(fail(Code::InvalidZip,"Truncated ZIP64 extra",&name,"",""));}*value=number(bytes,at,8)?;at+=8;}}}extra+=length;}
                if zip64&&!found {return Err(fail(Code::InvalidZip,"ZIP64 extra missing",&name,"",""));}if expanded>budget.limits.max_part_bytes as u64 {return Err(fail(Code::ResourceLimit,"ZIP part exceeds limit",&name,"",""));}
                let(l,_)=extent(bytes,local,30)?;if l>=start||number(bytes,l,4)?!=0x04034b50||number(bytes,l+4,2)?>45||number(bytes,cursor+6,2)?>45||number(bytes,l+10,2)?!=number(bytes,cursor+12,2)?||number(bytes,l+12,2)?!=number(bytes,cursor+14,2)?||number(bytes,l+6,2)?!=u64::from(flags)||number(bytes,l+8,2)?!=u64::from(method) {return Err(fail(Code::InvalidZip,"ZIP local header mismatch",&name,"",""));}
                let(ln,lx)=(number(bytes,l+26,2)? as usize,number(bytes,l+28,2)? as usize);let(data,_)=extent(bytes,local,30+ln as u64+lx as u64)?;let data=data+30+ln+lx;if bytes.get(l+30..l+30+ln)!=Some(name.as_bytes()) {return Err(fail(Code::InvalidZip,"ZIP local name mismatch",&name,"",""));}
                let(_,mut payload_end)=extent(bytes,data as u64,compressed)?;let checksum=number(bytes,cursor+16,4)? as u32;
                if flags&8!=0 {
                    let widths=if zip64||number(bytes,l+18,4)?==u32::MAX as u64||number(bytes,l+22,4)?==u32::MAX as u64 {[8usize,4]}else{[4usize,8]};let mut descriptor=None;
                    for width in widths {for signed in [false,true] {let at=payload_end+if signed{4}else{0};let length=4+width*2+if signed{4}else{0};if payload_end.checked_add(length).is_some_and(|end|end<=start)&&(!signed||number(bytes,payload_end,4)?==0x08074b50)&&number(bytes,at,4)?==u64::from(checksum)&&number(bytes,at+4,width)?==compressed&&number(bytes,at+4+width,width)?==expanded {descriptor=Some(payload_end+length);break;}}if descriptor.is_some(){break;}}
                    payload_end=descriptor.ok_or_else(||fail(Code::Integrity,"ZIP descriptor mismatch",&name,"",""))?;
                } else {
                    if number(bytes,l+14,4)?!=u64::from(checksum) {return Err(fail(Code::Integrity,"ZIP local CRC mismatch",&name,"",""));}
                    let mut local_expanded=number(bytes,l+22,4)?;let mut local_compressed=number(bytes,l+18,4)?;let mut at=l+30+ln;let end=at+lx;
                    while at<end {if at+4>end {return Err(fail(Code::InvalidZip,"Local ZIP extra truncated",&name,"",""));}let tag=number(bytes,at,2)?;let length=number(bytes,at+2,2)? as usize;at+=4;if length>end-at {return Err(fail(Code::InvalidZip,"Local ZIP extra overflow",&name,"",""));}if tag==1 {let mut position=at;for value in [&mut local_expanded,&mut local_compressed] {if *value==u32::MAX as u64 {if position+8>at+length {return Err(fail(Code::InvalidZip,"Local ZIP64 extra truncated",&name,"",""));}*value=number(bytes,position,8)?;position+=8;}}}at+=length;}
                    if local_expanded!=expanded||local_compressed!=compressed {return Err(fail(Code::Integrity,"ZIP local sizes mismatch",&name,"",""));}
                }
                if payload_end>start {return Err(fail(Code::InvalidZip,"ZIP payload intersects directory",&name,"",""));}spans.push((l,payload_end));budget.consume(Resource::DecoderAllocationBytes,name.len(),&name,"","")?;budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<(&str,usize)>(),&name,"","")?;index.insert(wire_name,entries.len());entries.push(Entry{name,method,crc:checksum,expanded,compressed,offset:local,flags,data_offset:data,end_offset:payload_end});cursor=next;
            }
)rustpackage"
R"rustpackage(            if cursor!=finish&&(finish-cursor<6||number(bytes,cursor,4)?!=0x05054b50||number(bytes,cursor+4,2)?!=(finish-cursor-6) as u64) {return Err(fail(Code::InvalidZip,"ZIP directory has trailing data","","",""));}spans.sort();if spans.windows(2).any(|pair|pair[0].1>pair[1].0) {return Err(fail(Code::InvalidZip,"Overlapping ZIP entries","","",""));}budget.poll()?;Ok(Self{bytes,entries,index,non_files})
        }
        /// Read one exact entry, rejecting CRC, size, truncation and trailing compressed input.
        pub fn read(&self,name:&str,budget:&mut Budget<'_>)->Result<Vec<u8>> {let entry=self.index.get(name).map(|index|&self.entries[*index]).ok_or_else(||fail(Code::MissingPart,"ZIP part missing",name,"",""))?;budget.consume(Resource::ExpandedBytes,entry.expanded as usize,name,"","")?;budget.consume(Resource::DecoderAllocationBytes,entry.expanded as usize,name,"","")?;let packed=&self.bytes[entry.data_offset..entry.data_offset+entry.compressed as usize];let bytes=if entry.method==0 {if entry.compressed!=entry.expanded {return Err(fail(Code::Integrity,"Stored size mismatch",name,"",""));}packed.to_vec()}else{compression::decompress(packed,compression::DecodeOptions{format:compression::Format::Deflate,max_input_bytes:packed.len(),max_output_bytes:entry.expanded as usize,check_cancelled:budget.check_cancelled,..Default::default()})?};if bytes.len() as u64!=entry.expanded||crc(&bytes,budget)?!=entry.crc {return Err(fail(Code::Integrity,"ZIP payload integrity failed",name,"",""));}budget.poll()?;Ok(bytes)}
    }
    /// Caller-authored binary entries own their bytes; source_index routes repeated parent roles.
    #[derive(Clone,Debug,Default)]
    pub struct BinaryPart {pub name:String,pub content_type:String,pub data:Vec<u8>,pub source_index:usize,pub method:u16}
    /// A typed occurrence retains its discovered package path and parent occurrence.
    #[derive(Clone,Debug)]
    pub struct ModelPart<T> {pub name:String,pub content_type:String,pub value:T,pub source_index:usize}
    /// Compose deterministic ZIP bytes, optionally forcing real ZIP64 structures on small fixtures.
    pub fn zip_write(parts:&[BinaryPart],budget:&mut Budget<'_>)->Result<Vec<u8>> {zip_write_policy(parts,budget,false)}
    /// Preserve generic ZIP naming while allowing inert names in OPC-only assembly.
    fn zip_write_policy(parts:&[BinaryPart],budget:&mut Budget<'_>,opc:bool)->Result<Vec<u8>> {
        let mut ordered:Vec<_>=parts.iter().collect();ordered.sort_by(|left,right|left.name.cmp(&right.name));let mut seen=BTreeSet::new();let mut bytes=Vec::new();let mut directory=Vec::new();let force=budget.force_zip64;
        for part in ordered {budget.poll()?;zip_item_name(&part.name,opc)?;if !seen.insert(part.name.to_ascii_lowercase()) {return Err(fail(Code::InvalidName,"Duplicate ZIP name",&part.name,"",""));}if !matches!(part.method,0|8)||part.data.len()>budget.limits.max_part_bytes {return Err(fail(Code::InvalidOptions,"Invalid ZIP entry policy",&part.name,"",""));}budget.consume(Resource::Entries,1,&part.name,"","")?;budget.consume(Resource::ExpandedBytes,part.data.len(),&part.name,"","")?;let packed=if part.method==8 {compression::compress(&part.data,compression::EncodeOptions{format:compression::Format::Deflate,max_input_bytes:part.data.len(),max_output_bytes:budget.limits.max_archive_bytes,check_cancelled:budget.check_cancelled,..Default::default()})?}else{part.data.clone()};let checksum=crc(&part.data,budget)?;let offset=bytes.len() as u64;let large=force||offset>=u32::MAX as u64||packed.len()>=u32::MAX as usize||part.data.len()>=u32::MAX as usize;let mut extra=Vec::new();if large {word(&mut extra,1,2);word(&mut extra,16,2);word(&mut extra,part.data.len() as u64,8);word(&mut extra,packed.len() as u64,8);}
            let additional=30+part.name.len()+extra.len()+packed.len();if bytes.len().checked_add(additional).is_none_or(|value|value>budget.limits.max_archive_bytes){return Err(fail(Code::ResourceLimit,"ZIP output exceeds limit",&part.name,"",""));}budget.consume(Resource::DecoderAllocationBytes,additional,&part.name,"","")?;
            for(value,size) in [(0x04034b50,4),(if large{45}else{20},2),(0x800,2),(part.method as u64,2),(0,2),(33,2),(checksum as u64,4),(if large{u32::MAX as u64}else{packed.len() as u64},4),(if large{u32::MAX as u64}else{part.data.len() as u64},4),(part.name.len() as u64,2),(extra.len() as u64,2)] {word(&mut bytes,value,size);}bytes.extend_from_slice(part.name.as_bytes());bytes.extend_from_slice(&extra);bytes.extend_from_slice(&packed);
            extra.clear();if large{word(&mut extra,1,2);word(&mut extra,24,2);word(&mut extra,part.data.len() as u64,8);word(&mut extra,packed.len() as u64,8);word(&mut extra,offset,8);}if part.name.len()>MAX_NAME_BYTES-46-extra.len(){return Err(fail(Code::InvalidName,"ZIP central header exceeds OPC bound",&part.name,"",""));}
            for(value,size) in [(0x02014b50,4),(0x033f,2),(if large{45}else{20},2),(0x800,2),(part.method as u64,2),(0,2),(33,2),(checksum as u64,4),(if large{u32::MAX as u64}else{packed.len() as u64},4),(if large{u32::MAX as u64}else{part.data.len() as u64},4),(part.name.len() as u64,2),(extra.len() as u64,2),(0,2),(0,2),(0,2),(0o100644u64<<16,4),(if large{u32::MAX as u64}else{offset},4)]{word(&mut directory,value,size);}directory.extend_from_slice(part.name.as_bytes());directory.extend_from_slice(&extra);
        }
        let offset=bytes.len() as u64;let size=directory.len() as u64;bytes.extend_from_slice(&directory);let large=force||parts.len()>=u16::MAX as usize||offset>=u32::MAX as u64||size>=u32::MAX as u64;
        if large {let zip64_offset=bytes.len() as u64;for(value,size) in [(0x06064b50,4),(44,8),(45,2),(45,2),(0,4),(0,4),(parts.len() as u64,8),(parts.len() as u64,8),(directory.len() as u64,8),(offset,8),(0x07064b50,4),(0,4),(zip64_offset,8),(1,4)]{word(&mut bytes,value,size);}}
        for(value,size) in [(0x06054b50,4),(0,2),(0,2),(if large{u16::MAX as u64}else{parts.len() as u64},2),(if large{u16::MAX as u64}else{parts.len() as u64},2),(if large{u32::MAX as u64}else{size},4),(if large{u32::MAX as u64}else{offset},4),(0,2)]{word(&mut bytes,value,size);}if bytes.len()>budget.limits.max_archive_bytes {return Err(fail(Code::ResourceLimit,"ZIP output exceeds limit","","",""));}budget.consume(Resource::PhysicalBytes,bytes.len(),"","","")?;Ok(bytes)
    }
)rustpackage"
#include "rust_package_uri_runtime.inc"
R"rustpackage(
    /// Exact MIME identity folds media/parameter names and retains decoded parameter values.
    #[derive(PartialEq,Eq)]
    struct Mime {media:String,parameters:BTreeMap<String,String>}
    /// Recognize the fixed MIME token alphabet independently of Unicode case conversion.
    fn mime_token(value:u8)->bool {value.is_ascii_alphanumeric()||b"!#$%&'*+-.^_`|~".contains(&value)}
    /// Parse complete type/subtype and quoted parameters, rejecting duplicate or reserved parameters.
    fn mime(value:&str)->Result<Mime> {
        let invalid=||fail(Code::InvalidMetadata,"Invalid content type","","","");
        if value.len()>MAX_NAME_BYTES {return Err(invalid());}
        let data=value.as_bytes();let mut position=0;
        while position<data.len()&&mime_token(data[position]) {position+=1;}
        if position==0||data.get(position)!=Some(&b'/') {return Err(invalid());}
        position+=1;let subtype=position;
        while position<data.len()&&mime_token(data[position]) {position+=1;}
        if position==subtype {return Err(invalid());}
        let media=value[..position].to_ascii_lowercase();let mut parameters=BTreeMap::new();
        while position<data.len() {
            while data.get(position).is_some_and(|byte|matches!(*byte,b' '|b'\t')) {position+=1;}
            if data.get(position)!=Some(&b';') {return Err(invalid());}position+=1;
            while data.get(position).is_some_and(|byte|matches!(*byte,b' '|b'\t')) {position+=1;}
            let start=position;while position<data.len()&&mime_token(data[position]) {position+=1;}
            if start==position||data.get(position)!=Some(&b'=') {return Err(invalid());}
            let name=value[start..position].to_ascii_lowercase();position+=1;let mut bytes=Vec::new();
            if data.get(position)==Some(&b'"') {
                position+=1;let mut ended=false;
                while position<data.len() {let mut byte=data[position];position+=1;if byte==b'"' {ended=true;break;}
                    if byte==b'\\' {byte=*data.get(position).ok_or_else(invalid)?;position+=1;}
                    if byte<32&&byte!=b'\t'||byte==127 {return Err(invalid());}bytes.push(byte);
                }if !ended {return Err(invalid());}
            } else {let start=position;while position<data.len()&&mime_token(data[position]) {position+=1;}if start==position {return Err(invalid());}bytes.extend_from_slice(&data[start..position]);}
            let text=String::from_utf8(bytes).map_err(|_|invalid())?;
            if parameters.insert(name,text).is_some() {return Err(invalid());}
        }
        if !parameters.is_empty()&&matches!(media.as_str(),RELS_MIME|"application/vnd.openxmlformats-package.core-properties+xml") {return Err(invalid());}
        Ok(Mime{media,parameters})
    }
    /// One source-scoped link retains its original target string, mode and selector.
    #[derive(Clone,Debug,PartialEq,Eq)]
    pub struct Relationship {pub source:String,pub id:String,pub kind:String,pub target:String,pub external:bool}
    /// Preserve whole unknown payloads and source-local relationships without XML rewriting.
    #[derive(Clone,Debug,Default)]
    pub struct Extensions {pub parts:Vec<BinaryPart>,pub relationships:Vec<Relationship>}
    /// Unknown optional and required capabilities are selected independently.
    #[derive(Clone,Copy,Debug,PartialEq,Eq)]
    pub enum Unknown {Reject,Skip,Preserve}
    /// Required relationship types are explicit caller capabilities; unknown core names still reject.
    pub struct ExtensionPolicy {pub optional:Unknown,pub required:Unknown,pub required_relationships:BTreeSet<String>}
    impl Default for ExtensionPolicy {
        /// Default to rejecting unknown package capabilities.
        fn default()->Self {Self{optional:Unknown::Reject,required:Unknown::Reject,required_relationships:BTreeSet::new()}}
    }
    impl ExtensionPolicy {
        /// Retain optional and explicitly required extensions for a checked edit/save cycle.
        pub fn preserve()->Self {Self{optional:Unknown::Preserve,required:Unknown::Preserve,..Default::default()}}
    }
    /// Declarative role metadata is generated alongside the statically typed model adapter.
    #[derive(Clone,Copy)]
    pub struct Role {pub field:&'static str,pub name:Option<&'static str>,pub content_type:Option<&'static str>,pub source:&'static str,pub relationship:&'static str,pub id:Option<&'static str>,pub method:u16,pub minimum:usize,pub maximum:Option<usize>,pub root:Option<(&'static str,&'static str)>,pub expected_type:Option<(&'static str,&'static str)>,pub fixed:bool}
    /// Additive payload selection metadata preserves existing Role, Budget and aggregate struct literals.
    #[derive(Clone,Copy,Debug,PartialEq,Eq)] pub enum PayloadCodec{Xml,Json,BinaryNone,Bytes}
    /// External byte roles retain exact inert URI text without creating a ZIP payload.
    #[derive(Clone,Copy,Debug,PartialEq,Eq)] pub enum TargetMode{Internal,External,Both}
    /// Generated complete role metadata remains independent of owning model values.
    #[derive(Clone,Copy)] pub struct RoleProfile{pub role:Role,pub codec:PayloadCodec,pub target_mode:TargetMode,pub requires_type:bool,pub vocabularies:&'static [&'static str]}
    /// Runtime pipeline choices cannot replace the schema's native model or relationship role.
    #[derive(Clone,Default)] pub struct PipelineOverride{pub role:String,pub codec:Option<PayloadCodec>,pub method:Option<u16>}
    /// Resolve both selection tables before native or provider work; conflicting methods reject explicitly.
    pub fn resolve_profiles(profiles:&[RoleProfile],overrides:&[PipelineOverride],budget:&Budget<'_>)->Result<Vec<RoleProfile>>{let roles=profiles.iter().map(|profile|profile.role).collect::<Vec<_>>();validate_overrides(&roles,budget)?;let mut seen=BTreeSet::new();for selected in overrides{if !seen.insert(selected.role.as_str())||!profiles.iter().any(|profile|profile.role.field==selected.role){return Err(fail(Code::InvalidOptions,"Unknown or duplicate package pipeline role","",&selected.role,""));}}profiles.iter().map(|profile|{let mut result=*profile;if let Some(method)=budget.overrides.get(profile.role.field){result.role.method=*method;}if let Some(selected)=overrides.iter().find(|selected|selected.role==profile.role.field){if let Some(codec)=selected.codec{result.codec=codec;}if let Some(method)=selected.method{if budget.overrides.get(profile.role.field).is_some_and(|legacy|*legacy!=method){return Err(fail(Code::InvalidOptions,"Conflicting package method selections","",profile.role.field,""));}result.role.method=method;}}if !matches!(result.role.method,0|8)||profile.role.fixed&&(result.codec!=profile.codec||result.role.method!=profile.role.method)||(profile.role.root.is_none())!=(result.codec==PayloadCodec::Bytes){return Err(fail(Code::InvalidOptions,"Fixed or unsupported package pipeline selection","",profile.role.field,""));}Ok(result)}).collect()}
    /// Reject unknown overrides and fixed-policy changes before native or source callbacks.
    pub fn validate_overrides(roles:&[Role],budget:&Budget<'_>)->Result<()> {for(field,method)in &budget.overrides{let role=roles.iter().find(|role|role.field==field).ok_or_else(||fail(Code::InvalidOptions,"Unknown profile override role","",field,""))?;if !matches!(*method,0|8)||role.fixed&&*method!=role.method{return Err(fail(Code::InvalidOptions,"Fixed or unsupported profile override","",field,""));}}budget.poll()}
    /// A complete-candidate identity inventory checks references after every role is collected.
    #[derive(Default)]
    pub struct ReferenceIndex {keys:BTreeMap<(String,String),String>,references:Vec<(String,String,String,String,String,String)>}
    impl ReferenceIndex {
        /// Admit one schema-selected key, rejecting duplicate scoped values across target types.
        pub fn key(&mut self,scope:&str,value:&str,role:&str,part:&str,field:&str,link:&str,budget:&mut Budget<'_>)->Result<()> {budget.consume(Resource::References,1,part,field,link)?;budget.consume(Resource::DecoderAllocationBytes,scope.len()+value.len()+role.len(),part,field,link)?;if self.keys.insert((scope.into(),value.into()),role.into()).is_some(){return Err(fail(Code::Semantic,"Duplicate scoped application key",part,field,link));}Ok(())}
        /// Retain a forward reference with exact schema-selected target-role identity.
)rustpackage"
R"rustpackage(        pub fn reference(&mut self,scope:&str,value:&str,role:&str,part:&str,field:&str,link:&str,budget:&mut Budget<'_>)->Result<()> {budget.consume(Resource::References,1,part,field,link)?;budget.consume(Resource::DecoderAllocationBytes,scope.len()+value.len()+role.len()+part.len()+field.len()+link.len(),part,field,link)?;self.references.push((scope.into(),value.into(),role.into(),part.into(),field.into(),link.into()));Ok(())}
        /// Reject missing and wrong-model keys before aggregate or file publication.
        pub fn finish(&mut self,budget:&mut Budget<'_>)->Result<()> {for(scope,value,role,part,field,link)in &self.references{budget.consume(Resource::WorkUnits,1,part,field,link)?;if self.keys.get(&(scope.clone(),value.clone()))!=Some(role){return Err(fail(Code::Semantic,"Missing or wrong-type application key",part,field,link));}}budget.poll()}
    }
    /// Check source-local ID uniqueness, inert URI types and internal closure before publication.
    fn relationships(parts:&BTreeMap<String,BinaryPart>,links:&[Relationship],budget:&mut Budget<'_>)->Result<()> {let mut ids=BTreeSet::new();for link in links {budget.consume(Resource::Relationships,1,&link.source,"relationships",&link.id)?;budget.consume(Resource::References,1,&link.source,"relationships",&link.id)?;if !compatibility_name(&link.id)||!ids.insert((link.source.to_ascii_lowercase(),link.id.clone())) {return Err(fail(Code::InvalidRelationship,"Invalid or duplicate scoped relationship ID",&link.source,"relationships",&link.id));}if link.source!="/"&&!parts.contains_key(&part_name(&link.source)?) {return Err(fail(Code::MissingPart,"Relationship source missing",&link.source,"relationships",&link.id));}uri(&link.kind)?;if link.external {iri_reference(&link.target)?;}else if !parts.contains_key(&part_name(&opc_target(&link.source,&link.target)?)?) {return Err(fail(Code::MissingPart,"Relationship target missing",&link.source,"relationships",&link.id));}}Ok(())}
)rustpackage"
#include "rust_package_mce_runtime.inc"
R"rustpackage(
    /// Normalize only BOM-marked infrastructure UTF-16; ordinary generated model XML remains UTF-8.
    fn infrastructure_text<'a>(bytes:&'a [u8],part:&str,budget:&mut Budget<'_>)->Result<std::borrow::Cow<'a,str>> {
        budget.consume(Resource::MetadataBytes,bytes.len(),part,"","")?;
        if bytes.len()>budget.limits.max_part_bytes{return Err(fail(Code::ResourceLimit,"Infrastructure XML part limit exceeded",part,"",""));}
        let little=bytes.starts_with(&[0xff,0xfe]);let big=bytes.starts_with(&[0xfe,0xff]);
        if !little&&!big{return Ok(std::borrow::Cow::Borrowed(std::str::from_utf8(bytes).map_err(native_error)?));}
        if bytes.len()<4||bytes.len()%2!=0||bytes[2..4]==[0,0]{return Err(fail(Code::InvalidMetadata,"Invalid UTF-16 infrastructure signature",part,"",""));}
        budget.consume(Resource::WorkUnits,bytes.len(),part,"","")?;
        let units=||bytes[2..].chunks_exact(2).map(|unit|if little{u16::from_le_bytes([unit[0],unit[1]])}else{u16::from_be_bytes([unit[0],unit[1]])});
        let mut size=0usize;
        for(index,value)in std::char::decode_utf16(units()).enumerate(){
            if index%CHUNK_BYTES==0{budget.poll()?;}
            let value=value.map_err(|_|fail(Code::InvalidMetadata,"Invalid UTF-16 infrastructure scalar",part,"",""))?;
            size=size.checked_add(value.len_utf8()).ok_or_else(||fail(Code::ResourceLimit,"Infrastructure XML size overflow",part,"",""))?;
            if size>budget.limits.max_part_bytes{return Err(fail(Code::ResourceLimit,"Normalized infrastructure XML limit exceeded",part,"",""));}
        }
        budget.consume(Resource::DecoderAllocationBytes,size,part,"","")?;
        let mut text=String::new();text.try_reserve_exact(size).map_err(native_error)?;
        for(index,value)in std::char::decode_utf16(units()).enumerate(){if index%CHUNK_BYTES==0{budget.poll()?;}text.push(value.map_err(native_error)?);}
        infrastructure_declaration(&mut text,part)?;
        budget.poll()?;Ok(std::borrow::Cow::Owned(text))
    }
    /// Rewrite only a matching encoding pseudo-attribute in the initial declaration after transcoding.
    fn infrastructure_declaration(text:&mut String,part:&str)->Result<()> {
        let invalid=||fail(Code::InvalidMetadata,"UTF-16 infrastructure declaration mismatch",part,"","");
        let bytes=text.as_bytes();let space=|value:u8|matches!(value,b' '|b'\t'|b'\r'|b'\n');
        if !bytes.starts_with(b"<?xml")||bytes.get(5).is_none_or(|value|!space(*value)){return Ok(());}
        let mut offset=5;let mut encoding=None;
        loop {
            while bytes.get(offset).is_some_and(|value|space(*value)){offset+=1;}
            if bytes.get(offset..offset.saturating_add(2))==Some(b"?>"){break;}
            let start=offset;while bytes.get(offset).is_some_and(u8::is_ascii_alphabetic){offset+=1;}
            if offset==start{return Err(invalid());}let selected=&bytes[start..offset]==b"encoding";
            while bytes.get(offset).is_some_and(|value|space(*value)){offset+=1;}
            if bytes.get(offset)!=Some(&b'='){return Err(invalid());}offset+=1;
            while bytes.get(offset).is_some_and(|value|space(*value)){offset+=1;}
            let quote=*bytes.get(offset).ok_or_else(invalid)?;if !matches!(quote,b'\''|b'"'){return Err(invalid());}offset+=1;
            let start=offset;while bytes.get(offset).is_some_and(|value|*value!=quote){offset+=1;}
            if offset==bytes.len(){return Err(invalid());}
            if selected{if encoding.is_some()||!bytes[start..offset].eq_ignore_ascii_case(b"utf-16"){return Err(invalid());}encoding=Some(start..offset);}
            offset+=1;
            if bytes.get(offset..offset.saturating_add(2))!=Some(b"?>")&&bytes.get(offset).is_none_or(|value|!space(*value)){return Err(invalid());}
        }
        if let Some(range)=encoding{text.replace_range(range,"UTF-8");}Ok(())
    }
    /// Parse normalized metadata with the existing no-DTD/no-fetch XML policy and cumulative ledger.
    fn metadata<'a>(text:&'a str,part:&str,budget:&mut Budget<'_>)->Result<roxmltree::Document<'a>> {xml_document(text,part,"","",budget)}
    /// Account every XML node, scalar and namespace before invoking a generated native decoder.
    fn xml_document<'a>(text:&'a str,part:&str,field:&str,link:&str,budget:&mut Budget<'_>)->Result<roxmltree::Document<'a>> {let limits=budget.xml_limits();let document=super::srl_xml_parse(text,limits).map_err(|error|if error.0.contains("limit"){Arc::new(Failure{code:Code::ResourceLimit,message:"Shared XML resource exhausted",part:part.into(),field:field.into(),link:link.into(),cause:Some(native_error(error))}) as Error}else{native_error(error)})?;for node in document.descendants() {budget.consume(Resource::WorkUnits,1,part,field,link)?;if node.is_element(){budget.consume(Resource::XmlNodes,1,part,field,link)?;budget.consume(Resource::XmlAttributes,node.attributes().len(),part,field,link)?;for attribute in node.attributes(){budget.consume(Resource::ScalarBytes,attribute.value().len(),part,attribute.name(),link)?;}for namespace in node.namespaces(){budget.consume(Resource::ScalarBytes,namespace.uri().len(),part,field,link)?;}}else if let Some(value)=node.text().filter(|_|node.is_text()){budget.consume(Resource::ScalarBytes,value.len(),part,field,link)?;}}Ok(document)}
    /// Reject attributes outside the exact standard infrastructure vocabulary.
    fn attributes(node:roxmltree::Node<'_,'_>,allowed:&[&str],part:&str)->Result<()> {if node.attributes().any(|attribute|attribute.namespace().is_some()||!allowed.contains(&attribute.name()))||node.children().any(|child|child.is_element()||child.is_text()&&child.text().is_some_and(|text|!text.trim().is_empty())) {return Err(fail(Code::InvalidMetadata,"Unknown OPC infrastructure markup",part,"",""));}Ok(())}
    /// Read one mandatory unqualified metadata attribute.
    fn attribute<'a>(node:roxmltree::Node<'a,'a>,name:&str,part:&str)->Result<&'a str> {node.attribute(name).ok_or_else(||fail(Code::InvalidMetadata,"Missing OPC metadata attribute",part,name,""))}
    /// Convert standard physical relationship-part names to their owning source identity.
    fn relationship_source(name:&str)->Option<String> {if name.eq_ignore_ascii_case("/_rels/.rels"){return Some("/".into());}let(parent,file)=name.rsplit_once('/')?;let(grandparent,directory)=parent.rsplit_once('/')?;if !directory.eq_ignore_ascii_case("_rels")||file.len()<=5||!file[file.len()-5..].eq_ignore_ascii_case(".rels"){return None;}Some(format!("{grandparent}/{}",&file[..file.len()-5]))}
    /// Read all standard infrastructure before any generated model decoder is selected.
    fn opc_read(archive:&ZipReader<'_>,budget:&mut Budget<'_>)->Result<(BTreeMap<String,BinaryPart>,Vec<Relationship>)> {let manifest=archive.entries.iter().find(|entry|entry.name=="[Content_Types].xml"&&!archive.non_files.contains(entry.name.as_str())).ok_or_else(||fail(Code::InvalidMetadata,"Content type manifest missing","","",""))?;let bytes=archive.read(&manifest.name,budget)?;let text=infrastructure_text(&bytes,"/[Content_Types].xml",budget)?;let document=metadata(&text,"/[Content_Types].xml",budget)?;let root=document.root_element();if root.tag_name().namespace()!=Some(CT)||root.tag_name().name()!="Types"||root.attributes().len()!=0||root.children().any(|node|node.is_text()&&node.text().is_some_and(|text|!text.trim().is_empty())) {return Err(fail(Code::InvalidMetadata,"Invalid content type root","","",""));}let mut defaults=BTreeMap::new();let mut overrides=BTreeMap::new();for node in root.children().filter(|node|node.is_element()){if node.tag_name().namespace()!=Some(CT){return Err(fail(Code::InvalidMetadata,"Unknown content type namespace","","",""));}let media=attribute(node,"ContentType","")?.to_owned();mime(&media)?;match node.tag_name().name(){"Override"=>{attributes(node,&["PartName","ContentType"],"")?;let key=part_name(&uri_attribute(node,"PartName","/[Content_Types].xml",budget)?)?;if overrides.insert(key,media).is_some(){return Err(fail(Code::InvalidMetadata,"Duplicate content type override","","",""));}},"Default"=>{attributes(node,&["Extension","ContentType"],"")?;let key=attribute(node,"Extension","")?.to_ascii_lowercase();if !valid_extension(&key)||defaults.insert(key,media).is_some(){return Err(fail(Code::InvalidMetadata,"Duplicate or invalid default extension","","",""));}},_=>return Err(fail(Code::InvalidMetadata,"Unknown content type declaration","","",""))}}
        let mut parts=BTreeMap::new();let mut relation_parts=Vec::new();let mut mapped:BTreeSet<String>=BTreeSet::new();for entry in &archive.entries{if entry.name=="[Content_Types].xml"{continue;}if archive.non_files.contains(entry.name.as_str()){archive.read(&entry.name,budget)?;continue;}let name="/".to_owned()+&entry.name;let Some(key)=mapped_part(&name)?else{archive.read(&entry.name,budget)?;continue;};for old in &mapped {budget.consume(Resource::WorkUnits,1,&name,"","")?;if key.starts_with(&format!("{old}/"))||old.starts_with(&format!("{key}/")){return Err(fail(Code::InvalidName,"Conflicting OPC part ancestry",&name,"",""));}}if !mapped.insert(key.clone()){return Err(fail(Code::InvalidName,"Equivalent OPC part names",&name,"",""));}let media=overrides.get(&key).or_else(||name.rsplit_once('.').and_then(|(_,extension)|defaults.get(&extension.to_ascii_lowercase()))).ok_or_else(||fail(Code::InvalidMetadata,"Part lacks content type",&name,"",""))?.clone();if let Some(source)=relationship_source(&name){if mime(&media)?.media!=RELS_MIME{return Err(fail(Code::InvalidMetadata,"Relationship part content type mismatch",&name,"",""));}relation_parts.push((name,source));}else{if mime(&media)?.media==RELS_MIME{return Err(fail(Code::InvalidName,"Reserved OPC infrastructure path",&name,"",""));}parts.insert(key,BinaryPart{name,content_type:media,data:Vec::new(),source_index:0,method:entry.method});}}
        if overrides.keys().any(|name|!mapped.contains(name)){return Err(fail(Code::InvalidMetadata,"Content type override names missing part","","",""));}let mut links=Vec::new();for(name,source)in relation_parts{let bytes=archive.read(&name[1..],budget)?;let text=infrastructure_text(&bytes,&name,budget)?;let document=metadata(&text,&name,budget)?;links.extend(compatibility_relationships(&document,&name,&source,budget)?);}relationships(&parts,&links,budget)?;Ok((parts,links))}
    /// Escape generated XML metadata without retaining unbounded temporary fragments.
    fn escape(value:&str)->Result<String>{let mut text=String::new();for character in value.chars(){match character{'&'=>text.push_str("&amp;"),'<'=>text.push_str("&lt;"),'"'=>text.push_str("&quot;"),'\r'=>text.push_str("&#13;"),'\n'=>text.push_str("&#10;"),'\t'=>text.push_str("&#9;"),value if value.is_control()=>return Err(fail(Code::InvalidMetadata,"Invalid metadata character","","","")),value=>text.push(value)}}Ok(text)}
    /// Construct canonical content types and source-local relationship XML before ZIP assembly.
    fn opc_write(parts:Vec<BinaryPart>,mut links:Vec<Relationship>,budget:&mut Budget<'_>)->Result<Vec<u8>>{let mut lookup:BTreeMap<String,BinaryPart>=BTreeMap::new();for part in parts {let key=part_name(&part.name)?;for old in lookup.keys(){budget.consume(Resource::WorkUnits,1,&part.name,"","")?;if key.starts_with(&format!("{old}/"))||old.starts_with(&format!("{key}/")){return Err(fail(Code::InvalidName,"Conflicting OPC part ancestry",&part.name,"",""));}}mime(&part.content_type)?;if relationship_source(&part.name).is_some()||mime(&part.content_type)?.media==RELS_MIME||lookup.insert(key,part).is_some(){return Err(fail(Code::InvalidName,"Duplicate or reserved package part","","",""));}}relationships(&lookup,&links,budget)?;links.sort_by(|left,right|(&left.source,&left.id).cmp(&(&right.source,&right.id)));let mut groups:BTreeMap<String,Vec<Relationship>>=BTreeMap::new();for link in links{groups.entry(link.source.clone()).or_default().push(link);}let mut files:Vec<BinaryPart>=lookup.into_values().collect();for(source,links)in groups{let name=if source=="/"{"/_rels/.rels".into()}else{let(parent,file)=source.rsplit_once('/').unwrap();format!("{parent}/_rels/{file}.rels")};let mut text=format!("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"{RELS}\">");for link in links{text.push_str(&format!("<Relationship Id=\"{}\" Target=\"{}\"{} Type=\"{}\"/>",escape(&link.id)?,escape(&link.target)?,if link.external{" TargetMode=\"External\""}else{""},escape(&link.kind)?));}text.push_str("</Relationships>");budget.consume(Resource::MetadataBytes,text.len(),&name,"","")?;files.push(BinaryPart{name,content_type:RELS_MIME.into(),data:text.into_bytes(),method:8,..Default::default()});}files.sort_by(|left,right|left.name.cmp(&right.name));let mut manifest=format!("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"{CT}\">");for part in &files{manifest.push_str(&format!("<Override ContentType=\"{}\" PartName=\"{}\"/>",escape(&part.content_type)?,escape(&part.name)?));}manifest.push_str("</Types>");budget.consume(Resource::MetadataBytes,manifest.len(),"/[Content_Types].xml","","")?;for part in &mut files{part.name.remove(0);}files.push(BinaryPart{name:"[Content_Types].xml".into(),data:manifest.into_bytes(),method:8,..Default::default()});zip_write_policy(&files,budget,true)}
    /// Bound role cardinality before any value leaves its private aggregate.
    fn occurs(role:&Role,count:usize)->Result<()> {if count<role.minimum||role.maximum.is_some_and(|maximum|count>maximum){return Err(fail(Code::Semantic,"Package role cardinality failed","",role.field,""));}Ok(())}
    /// Account one payload and verify its immutable expanded root before native model decode.
    fn typed<'a>(bytes:&'a [u8],role:&Role,name:&str,link:&str,budget:&mut Budget<'_>)->Result<&'a str>{let text=std::str::from_utf8(bytes).map_err(native_error)?;let document=xml_document(text,name,role.field,link,budget)?;let root=document.root_element();if let Some(kind)=root.attribute(("http://www.w3.org/2001/XMLSchema-instance","type")){let kind=kind.trim_matches([' ','\t','\r','\n']);let(prefix,local)=kind.split_once(':').unwrap_or(("",kind));let uri=root.lookup_namespace_uri(if prefix.is_empty(){None}else{Some(prefix)}).unwrap_or("");if role.expected_type!=Some((uri,local)){return Err(fail(Code::Codec,"Payload has unexpected expanded xsi:type",name,role.field,link));}}if role.root.is_none_or(|(uri,local)|root.tag_name().namespace().unwrap_or("")!=uri||root.tag_name().name()!=local){return Err(fail(Code::Codec,"Payload has unexpected expanded root",name,role.field,link));}Ok(text)}
)rustpackage"
R"rustpackage(
    /// Inspect only root/type dispatch; the selected native codec owns logical XML event accounting.
    fn typed_profile_header<'a>(bytes:&'a [u8],profile:&RoleProfile,name:&str,link:&str,budget:&mut Budget<'_>)->Result<&'a str>{
        let text=typed_header(bytes,&profile.role,name,link,budget)?;
        // Secondary header parsing spends work/storage; the primary codec owns logical event quotas and native field diagnostics.
        let limits=super::XmlLimits{max_nodes:usize::MAX,max_attributes:usize::MAX,..budget.xml_limits()};let mut context=XmlOperation::with_profile(budget,profile,name,link);
        {let state=native_operation::State::new(&mut context,&[],&[])?;state.field(profile.role.field);let result=(||{let mut input=super::SrlXmlInput::new_with_operation(text,limits,Some(state.secondary()))?;input.package_contract=Some(native_operation::PackageContract{expected_type:profile.role.expected_type,requires_type:profile.requires_type,vocabularies:profile.vocabularies});input.permitted_root_type=profile.role.expected_type;if let Some((uri,local))=profile.role.root{let _=input.open(uri,local)?;}Ok(())})();state.finish(result)?;}context.finish(text)
    }
    /// Check only immutable expanded identities before any selected XML decoder is invoked.
    fn typed_header<'a>(bytes:&'a [u8],role:&Role,name:&str,link:&str,budget:&mut Budget<'_>)->Result<&'a str>{
        budget.poll()?;
        if bytes.len()>budget.limits.max_part_bytes||budget.limits.max_depth==0{return Err(fail(Code::ResourceLimit,"Payload header exceeds local policy",name,role.field,link));}
        budget.consume(Resource::WorkUnits,bytes.len(),name,role.field,link)?;
        let text=std::str::from_utf8(bytes).map_err(native_error)?;
        let limits=super::XmlLimits{max_bytes:budget.limits.max_part_bytes,max_text_bytes:budget.limits.max_part_bytes,max_nodes:usize::MAX,max_attributes:usize::MAX,max_depth:budget.limits.max_depth};
        let mut context=XmlOperation::new(budget,role,name,link);
        {
            // Header dispatch spends real work/storage; the selected codec alone admits logical XML events.
            let state=native_operation::State::new(&mut context,&[],&[])?;state.field(role.field);
            let result=(||{
                let mut input=super::SrlXmlInput::new_with_operation(text,limits,Some(state.secondary()))?;
                if !matches!(&input.current,super::SrlXmlToken::Start(actual,_) if role.root==Some((actual.uri.as_str(),actual.local))){return Err(super::Error("Payload has unexpected expanded root"));}
                let selected=input.take_type()?;
                if selected.as_ref().is_some_and(|(uri,local)|role.expected_type!=Some((uri.as_str(),local.as_str()))){return Err(super::Error("Payload has unexpected expanded xsi:type"));}
                Ok(())
            })();
            state.finish(result)?;
        }
        context.finish(text)
    }
    /// Derive the exact generated relationship ID before a selected encoder receives its location.
    fn role_link(role:&Role,name:&str)->String{role.id.map(str::to_owned).unwrap_or_else(||format!("srl_{}_{}",role.field,name.as_bytes().iter().map(|byte|format!("{byte:02x}")).collect::<String>()))}
    /// Composer retains private encoded role candidates until every package invariant passes.
    pub struct Composer<'a,'b>{budget:&'a mut Budget<'b>,parts:Vec<BinaryPart>,links:Vec<Relationship>,sources:BTreeMap<String,Vec<String>>,paths:BTreeMap<String,String>}
    impl<'a,'b> Composer<'a,'b>{
        /// Stage a typed singleton with the selected existing XML/native envelope.
        pub fn single_pipeline<T>(&mut self,profile:&RoleProfile,value:&Option<T>,mut encoder:impl FnMut(&T,&RoleProfile,&mut XmlOperation<'_,'_>)->Result<Vec<u8>>)->Result<()>{let role=&profile.role;occurs(role,usize::from(value.is_some()))?;let mut discovered=Vec::new();if let Some(value)=value{let path=self.paths.get(role.field).map(String::as_str).unwrap_or(role.name.unwrap_or("")).to_owned();let link=role_link(role,&path);let mut context=XmlOperation::with_profile(self.budget,profile,&path,&link);let data=encoder(value,profile,&mut context)?;let data=context.finish(data)?;if profile.codec==PayloadCodec::Xml{typed_profile_header(&data,profile,&path,&link,self.budget)?;}self.add(role,BinaryPart{data,..Default::default()},0,&mut discovered)?;}self.sources.insert(role.field.into(),discovered);Ok(())}
        /// Stage repeated native models without changing their original owning representations.
        pub fn models_pipeline<T>(&mut self,profile:&RoleProfile,values:&[ModelPart<T>],mut encoder:impl FnMut(&T,&RoleProfile,&mut XmlOperation<'_,'_>)->Result<Vec<u8>>)->Result<()>{let role=&profile.role;occurs(role,values.len())?;let mut discovered=Vec::new();for(index,item)in values.iter().enumerate(){let link=role_link(role,&item.name);let mut context=XmlOperation::with_profile(self.budget,profile,&item.name,&link);let data=encoder(&item.value,profile,&mut context)?;let data=context.finish(data)?;if profile.codec==PayloadCodec::Xml{typed_profile_header(&data,profile,&item.name,&link,self.budget)?;}self.add(role,BinaryPart{name:item.name.clone(),content_type:item.content_type.clone(),data,source_index:item.source_index,method:role.method},index,&mut discovered)?;}self.sources.insert(role.field.into(),discovered);Ok(())}
        /// Compose internal bytes or exact inert external URIs using one declared occurrence count.
        pub fn binaries_pipeline(&mut self,profile:&RoleProfile,values:&[BinaryPart])->Result<()>{let role=&profile.role;occurs(role,values.len())?;let mut discovered=Vec::new();for(index,item)in values.iter().enumerate(){if profile.target_mode==TargetMode::External||profile.target_mode==TargetMode::Both&&item.content_type.is_empty(){if !item.content_type.is_empty()||!item.data.is_empty(){return Err(fail(Code::InvalidRelationship,"External role requires empty media and payload",&item.name,role.field,""));}iri_reference(&item.name)?;let source=if role.source=="/"{"/".into()}else{self.sources.get(role.source).and_then(|values|values.get(item.source_index)).cloned().ok_or_else(||fail(Code::MissingPart,"External role source is missing",&item.name,role.field,""))?};self.budget.consume(Resource::DecoderAllocationBytes,item.name.len()+std::mem::size_of::<Relationship>(),&item.name,role.field,"")?;self.links.push(Relationship{source,id:role_link(role,&item.name),kind:role.relationship.into(),target:item.name.clone(),external:true});continue;}self.budget.consume(Resource::DecoderAllocationBytes,item.data.len(),&item.name,role.field,"")?;self.add(role,item.clone(),index,&mut discovered)?;}self.sources.insert(role.field.into(),discovered);Ok(())}
        /// Borrow one ledger for the full synchronous generated encode.
        pub fn new(budget:&'a mut Budget<'b>,paths:&BTreeMap<String,String>)->Self{Self{budget,parts:Vec::new(),links:Vec::new(),sources:BTreeMap::new(),paths:paths.clone()}}
        /// Add a checked occurrence using its schema-selected source role.
        fn add(&mut self,role:&Role,mut part:BinaryPart,_index:usize,discovered:&mut Vec<String>)->Result<()>{if let Some(name)=role.name{part.name=self.paths.get(role.field).cloned().unwrap_or_else(||name.into());}if let Some(media)=role.content_type{part.content_type=media.into();}part_name(&part.name)?;mime(&part.content_type)?;let source=if role.source=="/"{"/".into()}else{self.sources.get(role.source).and_then(|values|values.get(part.source_index)).cloned().ok_or_else(||fail(Code::MissingPart,"Role source occurrence missing",&part.name,role.field,""))?};part.method=*self.budget.overrides.get(role.field).unwrap_or(&role.method);if !matches!(part.method,0|8){return Err(fail(Code::InvalidOptions,"Unsupported role compression override",&part.name,role.field,""));}self.links.push(Relationship{source,id:role.id.map(str::to_owned).unwrap_or_else(||format!("srl_{}_{}",role.field,part.name.as_bytes().iter().map(|byte|format!("{byte:02x}")).collect::<String>())),kind:role.relationship.into(),target:part.name.clone(),external:false});discovered.push(part.name.clone());self.parts.push(part);Ok(())}
        /// Encode one fixed singleton while preserving required/optional presence semantics.
)rustpackage"
R"rustpackage(        pub fn single<T>(&mut self,role:&Role,value:&Option<T>,encoder:impl Fn(&T,super::XmlLimits)->Result<String>)->Result<()>{occurs(role,usize::from(value.is_some()))?;let mut discovered=Vec::new();if let Some(value)=value{let text=encoder(value,self.budget.xml_limits())?;typed(text.as_bytes(),role,role.name.unwrap_or(""),"",self.budget)?;self.add(role,BinaryPart{data:text.into_bytes(),..Default::default()},0,&mut discovered)?;}self.sources.insert(role.field.into(),discovered);Ok(())}
        /// Encode one singleton using a caller-selected codec and the same native operation context.
        pub fn single_with_operation<T>(&mut self,role:&Role,value:&Option<T>,mut encoder:impl FnMut(&T,&mut XmlOperation<'_,'_>)->Result<String>)->Result<()>{occurs(role,usize::from(value.is_some()))?;let mut discovered=Vec::new();if let Some(value)=value{let path=self.paths.get(role.field).map(String::as_str).unwrap_or(role.name.unwrap_or("")).to_owned();let link=role_link(role,&path);let mut context=XmlOperation::new(self.budget,role,&path,&link);let text=encoder(value,&mut context)?;let text=context.finish(text)?;typed_header(text.as_bytes(),role,&path,&link,self.budget)?;self.add(role,BinaryPart{data:text.into_bytes(),..Default::default()},0,&mut discovered)?;}self.sources.insert(role.field.into(),discovered);Ok(())}
        /// Encode dynamic or repeated typed models in declared occurrence order.
        pub fn models<T>(&mut self,role:&Role,values:&[ModelPart<T>],encoder:impl Fn(&T,super::XmlLimits)->Result<String>)->Result<()>{occurs(role,values.len())?;let mut discovered=Vec::new();for(index,item)in values.iter().enumerate(){let limits=self.budget.xml_limits();let text=encoder(&item.value,limits)?;typed(text.as_bytes(),role,&item.name,"",self.budget)?;self.add(role,BinaryPart{name:item.name.clone(),content_type:item.content_type.clone(),data:text.into_bytes(),source_index:item.source_index,method:role.method},index,&mut discovered)?;}self.sources.insert(role.field.into(),discovered);Ok(())}
        /// Compose repeated native models without charging their primary XML events a second time.
        pub fn models_with_operation<T>(&mut self,role:&Role,values:&[ModelPart<T>],mut encoder:impl FnMut(&T,&mut XmlOperation<'_,'_>)->Result<String>)->Result<()>{occurs(role,values.len())?;let mut discovered=Vec::new();for(index,item)in values.iter().enumerate(){let link=role_link(role,&item.name);let mut context=XmlOperation::new(self.budget,role,&item.name,&link);let text=encoder(&item.value,&mut context)?;let text=context.finish(text)?;typed_header(text.as_bytes(),role,&item.name,&link,self.budget)?;self.add(role,BinaryPart{name:item.name.clone(),content_type:item.content_type.clone(),data:text.into_bytes(),source_index:item.source_index,method:role.method},index,&mut discovered)?;}self.sources.insert(role.field.into(),discovered);Ok(())}
        /// Add opaque bytes without interpreting unknown model content.
        pub fn binaries(&mut self,role:&Role,values:&[BinaryPart])->Result<()>{occurs(role,values.len())?;let mut discovered=Vec::new();for(index,item)in values.iter().enumerate(){self.budget.consume(Resource::DecoderAllocationBytes,item.data.len(),&item.name,role.field,"")?;self.add(role,item.clone(),index,&mut discovered)?;}self.sources.insert(role.field.into(),discovered);Ok(())}
        /// Validate preserved internal closure and canonical ordering before archive publication.
        pub fn finish(mut self,extensions:&Extensions)->Result<Vec<u8>>{self.parts.extend(extensions.parts.iter().cloned());self.links.extend(extensions.relationships.iter().cloned());opc_write(self.parts,self.links,self.budget)}
    }
    /// Loader decodes role-local candidates without publishing any aggregate until finish succeeds.
    pub struct Loader<'a,'b,'c>{archive:ZipReader<'a>,budget:&'b mut Budget<'c>,parts:BTreeMap<String,BinaryPart>,links:Vec<Relationship>,used_parts:BTreeSet<String>,used_links:BTreeSet<usize>,sources:BTreeMap<String,Vec<String>>,reference_links:BTreeMap<String,Vec<String>>}
    impl<'a,'b,'c> Loader<'a,'b,'c>{
        /// Decode one selected singleton only after its complete source-scoped metadata is checked.
        pub fn single_pipeline<T>(&mut self,profile:&RoleProfile,decoder:impl FnMut(&[u8],&RoleProfile,&mut XmlOperation<'_,'_>)->Result<T>)->Result<Option<T>>{let mut values=self.models_pipeline(profile,decoder)?;Ok(values.pop().map(|part|part.value))}
        /// Decode original native wire bytes without a UTF8 conversion for positional binary payloads.
        pub fn models_pipeline<T>(&mut self,profile:&RoleProfile,mut decoder:impl FnMut(&[u8],&RoleProfile,&mut XmlOperation<'_,'_>)->Result<T>)->Result<Vec<ModelPart<T>>>{let role=&profile.role;let mut values=Vec::new();for(part,link)in self.select_modes(role,TargetMode::Internal)?{if profile.codec==PayloadCodec::Xml{typed_profile_header(&part.data,profile,&part.name,&link,self.budget)?;}let mut context=XmlOperation::with_profile(self.budget,profile,&part.name,&link);let value=decoder(&part.data,profile,&mut context)?;let value=context.finish(value)?;self.budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<ModelPart<T>>(),&part.name,role.field,&link)?;values.try_reserve_exact(1).map_err(native_error)?;values.push(ModelPart{name:part.name,content_type:part.content_type,source_index:part.source_index,value});}Ok(values)}
        /// Return empty bytes/media for external URI occurrences without opening or fetching a target.
        pub fn binaries_pipeline(&mut self,profile:&RoleProfile)->Result<Vec<BinaryPart>>{Ok(self.select_modes(&profile.role,profile.target_mode)?.into_iter().map(|(part,_)|part).collect())}
        /// Parse all infrastructure and link closure before any model decoder callback.
        pub fn new(bytes:&'a [u8],budget:&'b mut Budget<'c>)->Result<Self>{let archive=ZipReader::new_policy(bytes,budget,true)?;let(parts,links)=opc_read(&archive,budget)?;Ok(Self{archive,budget,parts,links,used_parts:BTreeSet::new(),used_links:BTreeSet::new(),sources:BTreeMap::new(),reference_links:BTreeMap::new()})}
        /// Select a complete source-scoped relationship inventory with immutable expected media.
        fn select(&mut self,role:&Role)->Result<Vec<(BinaryPart,String)>>{self.select_modes(role,TargetMode::Internal)}
        /// Discover all declared target modes while retaining physical provenance only for internal parts.
        fn select_modes(&mut self,role:&Role,mode:TargetMode)->Result<Vec<(BinaryPart,String)>>{let parents=if role.source=="/"{vec!["/".into()]}else{self.sources.get(role.source).cloned().ok_or_else(||fail(Code::MissingPart,"Role source missing","",role.field,""))?};let mut values=Vec::new();let mut discovered=Vec::new();for(index,link)in self.links.iter().enumerate(){if link.kind!=role.relationship||!parents.contains(&link.source){continue;}if role.id.is_some_and(|id|id!=link.id)||link.external&&mode==TargetMode::Internal||!link.external&&mode==TargetMode::External{return Err(fail(Code::InvalidRelationship,"Declared role relationship mismatch",&link.source,role.field,&link.id));}if link.external{self.budget.consume(Resource::DecoderAllocationBytes,link.target.len()+std::mem::size_of::<BinaryPart>(),&link.source,role.field,&link.id)?;self.used_links.insert(index);values.push((BinaryPart{name:link.target.clone(),content_type:String::new(),data:Vec::new(),source_index:parents.iter().position(|source|source==&link.source).unwrap(),method:role.method},link.id.clone()));continue;}let name=opc_target(&link.source,&link.target)?;let key=part_name(&name)?;let mut part=self.parts.get(&key).cloned().ok_or_else(||fail(Code::MissingPart,"Declared role part missing",&name,role.field,&link.id))?;if role.fixed&&part.method!=role.method||role.content_type.is_some_and(|expected|mime(expected).ok()!=mime(&part.content_type).ok()){return Err(fail(Code::Codec,"Declared role metadata mismatch",&name,role.field,&link.id));}if !self.used_parts.insert(key){return Err(fail(Code::Semantic,"Part selected by multiple roles",&name,role.field,&link.id));}self.used_links.insert(index);part.source_index=parents.iter().position(|source|source==&link.source).unwrap();part.data=self.archive.read(&part.name[1..],self.budget)?;discovered.push(part.name.clone());values.push((part,link.id.clone()));}occurs(role,values.len())?;for(part,link)in &values{self.budget.consume(Resource::DecoderAllocationBytes,link.len(),&part.name,role.field,link)?;self.budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<String>(),&part.name,role.field,link)?;}self.reference_links.insert(role.field.into(),values.iter().map(|(_,link)|link.clone()).collect());self.sources.insert(role.field.into(),discovered);Ok(values)}
        /// Decode one required or optional singleton from a fresh owning candidate.
        pub fn single<T>(&mut self,role:&Role,decoder:impl Fn(&str,super::XmlLimits)->Result<T>)->Result<Option<T>>{let mut values=self.models(role,decoder)?;Ok(values.pop().map(|part|part.value))}
        /// Decode a singleton through the selected codec before returning any owning aggregate.
        pub fn single_with_operation<T>(&mut self,role:&Role,decoder:impl FnMut(&str,&mut XmlOperation<'_,'_>)->Result<T>)->Result<Option<T>>{let mut values=self.models_with_operation(role,decoder)?;Ok(values.pop().map(|part|part.value))}
        /// Decode all typed occurrences using root checks before callbacks and one shared ledger.
        pub fn models<T>(&mut self,role:&Role,decoder:impl Fn(&str,super::XmlLimits)->Result<T>)->Result<Vec<ModelPart<T>>>{let mut values=Vec::new();for(part,link)in self.select(role)?{let limits=self.budget.xml_limits();let text=typed(&part.data,role,&part.name,&link,self.budget)?;let value=decoder(text,limits)?;self.budget.poll()?;values.push(ModelPart{name:part.name,content_type:part.content_type,source_index:part.source_index,value});}Ok(values)}
        /// Share native event/identity counters across every occurrence and publish only complete values.
        pub fn models_with_operation<T>(&mut self,role:&Role,mut decoder:impl FnMut(&str,&mut XmlOperation<'_,'_>)->Result<T>)->Result<Vec<ModelPart<T>>>{let mut values=Vec::new();for(part,link)in self.select(role)?{let text=typed_header(&part.data,role,&part.name,&link,self.budget)?;let mut context=XmlOperation::new(self.budget,role,&part.name,&link);let value=decoder(text,&mut context)?;let value=context.finish(value)?;self.budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<ModelPart<T>>(),&part.name,role.field,&link)?;values.try_reserve_exact(1).map_err(native_error)?;values.push(ModelPart{name:part.name,content_type:part.content_type,source_index:part.source_index,value});}Ok(values)}
        /// Retain declared opaque resources with checked discovered metadata.
        pub fn binaries(&mut self,role:&Role)->Result<Vec<BinaryPart>>{Ok(self.select(role)?.into_iter().map(|(part,_)|part).collect())}
        /// Retain discovered singleton names for lossless loaded-package editing.
        pub fn paths(&self)->BTreeMap<String,String>{self.sources.iter().filter_map(|(field,values)|if values.len()==1{Some((field.clone(),values[0].clone()))}else{None}).collect()}
        /// Retain source IDs alongside each selected occurrence for later automatic checks.
        pub fn reference_links(&mut self)->BTreeMap<String,Vec<String>>{std::mem::take(&mut self.reference_links)}
        /// Apply independent optional/required unknown policies and revalidate preserved sources.
        pub fn finish(self,policy:ExtensionPolicy)->Result<Extensions>{let mut result=Extensions::default();let mut skipped=BTreeSet::new();for(index,link)in self.links.iter().enumerate(){if self.used_links.contains(&index){continue;}let action=if policy.required_relationships.contains(&link.kind){policy.required}else{policy.optional};match action{Unknown::Reject=>return Err(fail(Code::Semantic,"Unknown package relationship capability",&link.source,"",&link.id)),Unknown::Preserve=>result.relationships.push(link.clone()),Unknown::Skip=>{if !link.external{skipped.insert(part_name(&opc_target(&link.source,&link.target)?)?);}}}}for(key,part)in self.parts{if self.used_parts.contains(&key){continue;}let required=result.relationships.iter().any(|link|!link.external&&opc_target(&link.source,&link.target).is_ok_and(|name|name.eq_ignore_ascii_case(&part.name)));let action=if required{Unknown::Preserve}else if skipped.contains(&key){Unknown::Skip}else{policy.optional};match action{Unknown::Reject=>return Err(fail(Code::Semantic,"Unknown package part",&part.name,"","")),Unknown::Skip=>(),Unknown::Preserve=>{let mut part=part;part.data=self.archive.read(&part.name[1..],self.budget)?;self.budget.consume(Resource::PreservedBytes,part.data.len(),&part.name,"","")?;result.parts.push(part);}}}self.budget.poll()?;Ok(result)}
    }
    /// An owning bounded spool supports nonseekable sources without temporary filesystem state.
    pub fn read_source(source:&mut impl std::io::Read,budget:&mut Budget<'_>)->Result<Vec<u8>>{let mut bytes=Vec::new();let mut chunk=[0u8;CHUNK_BYTES];loop{budget.poll()?;let remaining=budget.limits.max_archive_bytes-bytes.len();let size=CHUNK_BYTES.min(remaining.saturating_add(1));let count=source.read(&mut chunk[..size]).map_err(native_error)?;budget.poll()?;if count==0{break;}if count>remaining{return Err(fail(Code::ResourceLimit,"Spool exceeds archive limit","","",""));}budget.consume(Resource::DecoderAllocationBytes,count,"","","")?;bytes.extend_from_slice(&chunk[..count]);}Ok(bytes)}
    /// Borrow a sink until complete publication; short writes and caller errors remain observable.
    pub fn write_sink(destination:&mut impl std::io::Write,bytes:&[u8],budget:&Budget<'_>)->Result<()>{for chunk in bytes.chunks(CHUNK_BYTES){budget.poll()?;destination.write_all(chunk).map_err(native_error)?;}budget.poll()}
    /// Remove only this operation's private staging file on every pre-replacement failure.
    struct Stage(std::path::PathBuf);
    impl Drop for Stage{
        /// Best-effort cleanup does not replace the original operation failure.
        fn drop(&mut self){let _=std::fs::remove_file(&self.0);}
    }
    /// Stage with create-new ownership, sync, then atomically replace a same-directory destination.
    pub fn save(path:&std::path::Path,bytes:&[u8],budget:&Budget<'_>,fault:Option<&dyn Fn(&str)->Result<()>>)->Result<()>{use std::io::Write;static NEXT:std::sync::atomic::AtomicU64=std::sync::atomic::AtomicU64::new(0);budget.poll()?;if let Some(fault)=fault{fault("before_create")?;}let parent=path.parent().filter(|value|!value.as_os_str().is_empty()).unwrap_or_else(||std::path::Path::new("."));let stamp=std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_err(native_error)?.as_nanos();let stage=Stage(parent.join(format!(".serializer-package-{}-{}-{}.tmp",std::process::id(),stamp,NEXT.fetch_add(1,std::sync::atomic::Ordering::Relaxed))));let mut options=std::fs::OpenOptions::new();options.write(true).create_new(true);#[cfg(unix)]{use std::os::unix::fs::OpenOptionsExt;options.mode(0o600);}let mut file=options.open(&stage.0).map_err(native_error)?;write_sink(&mut file,bytes,budget)?;file.flush().map_err(native_error)?;file.sync_all().map_err(native_error)?;drop(file);if let Some(fault)=fault{fault("after_write")?;}budget.poll()?;if let Some(fault)=fault{fault("before_replace")?;}budget.poll()?;std::fs::rename(&stage.0,path).map_err(native_error)?;if let Some(fault)=fault{fault("after_replace")?;}Ok(())}
)rustpackage"
R"rustpackage(
    /// Explicit stable physical identity for one dynamic role occurrence; keep unchanged records as tombstones.
    #[derive(Clone,Debug)]
    pub struct PartRename {pub role:String,pub original:String,pub destination:String}
    /// Classify unsupported caller routing choices through the package diagnostic contract.
    pub fn invalid_options(message:&'static str)->Error{fail(Code::InvalidOptions,message,"","relocations","")}
    /// Additive canonical output keeps every existing package value and extension struct literal intact.
    pub trait CanonicalPackage {
        type Input;
        /// Borrow models while rebasing fixed singleton paths and cloned preserved links privately.
        fn encode_canonical(value:&Self::Input,budget:&mut Budget<'_>)->Result<Vec<u8>>;
        /// Rebind explicitly retained dynamic occurrence identities after native list edits.
        fn encode_renamed(value:&Self::Input,budget:&mut Budget<'_>,renames:&[PartRename])->Result<Vec<u8>>{if renames.is_empty(){Self::encode_canonical(value,budget)}else{Err(invalid_options("Dynamic renames are unavailable"))}}
        /// Snapshot every dynamic occurrence identity before editing or reordering its native collection.
        fn dynamic_paths(_value:&Self::Input)->Vec<PartRename>{Vec::new()}
        /// Validate explicit dynamic moves before replacing a final package file.
        fn save_renamed(_path:&std::path::Path,_value:&Self::Input,_budget:&mut Budget<'_>,_renames:&[PartRename],_fault:Option<&dyn Fn(&str)->Result<()>>)->Result<()>{Err(invalid_options("Dynamic rename Save is unavailable"))}
        /// Finish canonical validation before borrowing a caller-owned sink.
        fn write_canonical(destination:&mut impl std::io::Write,value:&Self::Input,budget:&mut Budget<'_>)->Result<()>;
        /// Revalidate the staged archive before same-directory atomic file replacement.
        fn save_canonical(path:&std::path::Path,value:&Self::Input,budget:&mut Budget<'_>,fault:Option<&dyn Fn(&str)->Result<()>>)->Result<()>;
    }
    /// Own only routing metadata; authored/native model payloads are never copied by canonical selection.
    pub struct CanonicalPaths {pub paths:BTreeMap<String,String>,relocations:Vec<(String,String,bool)>}
    impl CanonicalPaths {
        /// Retain an explicit dynamic identity and role liveness without copying native payloads.
        pub fn add_dynamic(&mut self,rename:&PartRename,present:bool,budget:&mut Budget<'_>)->Result<()>{
            part_name(&rename.original)?;part_name(&rename.destination)?;
            for(before,after,_)in &self.relocations{budget.consume(Resource::WorkUnits,before.len()+rename.original.len(),&rename.original,"relocations","")?;if before.eq_ignore_ascii_case(&rename.original)&&after!=&rename.destination{return Err(fail(Code::InvalidRelationship,"Conflicting dynamic role origins",&rename.original,"relocations",""));}}
            let bytes=rename.original.len().checked_add(rename.destination.len()).and_then(|size|size.checked_add(std::mem::size_of::<(String,String,bool)>())).ok_or_else(||fail(Code::ResourceLimit,"Dynamic rename inventory overflow",&rename.original,"relocations",""))?;
            budget.consume(Resource::MetadataBytes,bytes,&rename.original,"relocations","")?;budget.consume(Resource::DecoderAllocationBytes,bytes,&rename.original,"relocations","")?;
            self.relocations.try_reserve_exact(1).map_err(native_error)?;self.relocations.push((rename.original.clone(),rename.destination.clone(),present));Ok(())
        }
        /// Preserve moved-origin tombstones so a removed swap partner cannot be captured by another role.
        pub fn new(roles:&[Role],loaded:&BTreeMap<String,String>,present:&[bool],budget:&mut Budget<'_>)->Result<Self>{
            if roles.len()!=present.len(){return Err(fail(Code::InvalidOptions,"Canonical role inventory mismatch","","relocations",""));}
            let mut result=Self{paths:BTreeMap::new(),relocations:Vec::new()};
            for (role,exists) in roles.iter().zip(present){let Some(canonical)=role.name else{continue;};let original=loaded.get(role.field).map(String::as_str).unwrap_or(canonical);
                if *exists{budget.consume(Resource::DecoderAllocationBytes,role.field.len()+canonical.len()+std::mem::size_of::<(String,String)>(),original,"relocations","")?;result.paths.insert(role.field.into(),canonical.into());}
                if original==canonical{continue;}
                budget.consume(Resource::WorkUnits,original.len()+canonical.len(),original,"relocations","")?;let key=part_name(original)?;part_name(canonical)?;
                for (before,after,_) in &result.relocations{budget.consume(Resource::WorkUnits,before.len()+original.len(),original,"relocations","")?;if before.eq_ignore_ascii_case(&key)&&after!=canonical{return Err(fail(Code::InvalidRelationship,"Conflicting canonical role origins",original,"relocations",""));}}
                budget.consume(Resource::MetadataBytes,original.len()+canonical.len()+std::mem::size_of::<(String,String,bool)>(),original,"relocations","")?;
                budget.consume(Resource::DecoderAllocationBytes,original.len()+canonical.len()+std::mem::size_of::<(String,String,bool)>(),original,"relocations","")?;
                result.relocations.try_reserve_exact(1).map_err(native_error)?;result.relocations.push((original.into(),canonical.into(),*exists));
            }budget.poll()?;Ok(result)
        }
        /// Map once from original OPC identity, rejecting references to a moved role that was removed.
        fn destination<'a>(&'a self,original:&'a str,budget:&mut Budget<'_>)->Result<&'a str>{for(before,after,present)in &self.relocations{
            budget.consume(Resource::WorkUnits,before.len()+original.len(),original,"relocations","")?;
            if before.eq_ignore_ascii_case(original){if !present{return Err(fail(Code::MissingPart,"Preserved link refers to removed moved role",original,"relocations",""));}return Ok(after);}}
            Ok(original)
        }
        /// Resolve original target scope before moving either endpoint and retain exact raw selectors/external text.
        fn relationship(&self,link:&Relationship,budget:&mut Budget<'_>)->Result<Relationship>{
            budget.poll()?;let source=self.destination(&link.source,budget)?;let mut destination=None;
            if !link.external{budget.consume(Resource::DecoderAllocationBytes,link.source.len()+link.target.len(),&link.source,"relocations",&link.id)?;let resolved=opc_target(&link.source,&link.target)?;let moved=self.destination(&resolved,budget)?;
                if source!=link.source||moved!=resolved{let offset=link.target.find(['?','#']).unwrap_or(link.target.len());let suffix=&link.target[offset..];let count=moved.len().checked_add(suffix.len()).ok_or_else(||fail(Code::ResourceLimit,"Retargeted URI overflow",&link.source,"relocations",&link.id))?;
                    budget.consume(Resource::DecoderAllocationBytes,count,&link.source,"relocations",&link.id)?;let mut text=String::new();text.try_reserve_exact(count).map_err(native_error)?;text.push_str(moved);text.push_str(suffix);destination=Some(text);}}
            budget.consume(Resource::DecoderAllocationBytes,source.len()+link.id.len()+link.kind.len()+if destination.is_some(){0}else{link.target.len()},&link.source,"relocations",&link.id)?;
            Ok(Relationship{source:source.into(),id:link.id.clone(),kind:link.kind.clone(),target:destination.unwrap_or_else(||link.target.clone()),external:link.external})
        }
    }
    impl Composer<'_,'_>{
        /// Transfer admitted canonical routing metadata without cloning any map or model a second time.
        pub fn new_canonical<'a,'b>(budget:&'a mut Budget<'b>,paths:BTreeMap<String,String>)->Composer<'a,'b>{Composer{budget,parts:Vec::new(),links:Vec::new(),sources:BTreeMap::new(),paths}}
        /// Keep the old no-relocation path and privately clone only final preserved output ownership.
        pub fn finish_canonical(mut self,extensions:&Extensions,paths:&CanonicalPaths)->Result<Vec<u8>>{
            if paths.relocations.is_empty(){return self.finish(extensions);}
            for part in &extensions.parts{self.budget.consume(Resource::DecoderAllocationBytes,part.data.len()+part.name.len()+part.content_type.len()+std::mem::size_of::<BinaryPart>(),&part.name,"relocations","")?;self.parts.try_reserve_exact(1).map_err(native_error)?;self.parts.push(part.clone());}
            for link in &extensions.relationships{self.budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<Relationship>(),&link.source,"relocations",&link.id)?;let rebound=paths.relationship(link,self.budget)?;self.links.try_reserve_exact(1).map_err(native_error)?;self.links.push(rebound);}
            opc_write(self.parts,self.links,self.budget)
        }
    }
}
)rustpackage"
