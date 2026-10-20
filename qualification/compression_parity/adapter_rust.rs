// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
use std::{error::Error,fs,path::Path};
mod schema;
use schema::{serializer_compression as c,AccountRecord,Protocol};
serializer_compression_providers::zstd_provider!(zstd_adapter,crate::schema::serializer_compression);
serializer_compression_providers::lz4_provider!(lz4_adapter,crate::schema::serializer_compression);
type Result<T> = std::result::Result<T,Box<dyn Error>>;
const FORMATS: [(&str,c::Format);6]=[("NONE",c::Format::None),("GZIP",c::Format::Gzip),("ZLIB",c::Format::Zlib),("DEFLATE",c::Format::Deflate),("ZSTD",c::Format::Zstd),("LZ4",c::Format::Lz4)];
const PROTOCOLS: [(&str,Protocol);4]=[("JSON",Protocol::Json),("BINARY_NONE",Protocol::BinaryNone),("BINARY_INTEGER",Protocol::BinaryInteger),("BINARY_STRING",Protocol::BinaryString)];
// Keep a provider borrowed only for its explicitly selected optional standard format.
fn provider(format:c::Format)->Option<&'static dyn c::Provider>{match format{c::Format::Zstd=>Some(&zstd_adapter::Zstd),c::Format::Lz4=>Some(&lz4_adapter::Lz4),_=>None}}
// Route selected optional contexts without changing the standalone core APIs.
fn decompress(input:&[u8],options:c::DecodeOptions<'_>)->c::Result<Vec<u8>>{if let Some(provider)=provider(options.format){c::decompress_with_provider(input,options,provider)}else{c::decompress(input,options)}}
// Exercise owning native encoding with the same explicit provider and independently bounded policy.
fn encode(value:&AccountRecord,protocol:Protocol,options:c::EncodeOptions<'_>)->c::Result<Vec<u8>>{if let Some(provider)=provider(options.format){c::encode_with_provider(||value.encode(protocol).map_err(c::into_error),options,provider)}else{c::encode(||value.encode(protocol).map_err(c::into_error),options)}}
// Replay every exact shared decision and decode actual owning native model exchanges.
fn main()->Result<()>{
 let args:Vec<_>=std::env::args().collect();let root=Path::new(&args[2]);
 if args[1]=="produce"{
  let value=AccountRecord{account_id:42,label:"shared \u{03bb} \u{1f642}".into()};
  for (name,protocol) in PROTOCOLS{for(kind,format)in FORMATS{
   let options=c::EncodeOptions{format,level:if format==c::Format::None||format==c::Format::Lz4{0}else{3},..Default::default()};
   let plain=value.encode(protocol)?;let frame=encode(&value,protocol,options)?;
   assert_eq!(encode(&value,protocol,c::EncodeOptions{max_input_bytes:plain.len(),max_output_bytes:frame.len(),..options})?,frame);
   for(input,output)in[(plain.len()-1,frame.len()),(plain.len(),frame.len()-1)]{
    let error=encode(&value,protocol,c::EncodeOptions{max_input_bytes:input,max_output_bytes:output,..options}).expect_err("One-tight encoding bound");
    assert_eq!(error.downcast_ref::<c::Failure>().ok_or_else(||error.to_string())?.code,c::Code::ResourceLimit);
   }
   let stem=format!("rust-{name}-{kind}");fs::write(root.join(stem.clone()+".frame"),frame)?;fs::write(root.join(stem+".plain"),plain)?;
  }}println!("{{\"produced\":24,\"encode_bounds\":72}}");return Ok(());
 }
 let mut decisions=0;let mut exchanges=0;
 for line in fs::read_to_string(root.join("cases.tsv"))?.lines(){
  let row:Vec<_>=line.split('\t').collect();let format=FORMATS.iter().find(|entry|entry.0==row[1]).ok_or("Unknown format")?.1;
  let options=c::DecodeOptions{format,max_input_bytes:row[5].parse()?,max_output_bytes:row[6].parse()?,max_window_bytes:row[7].parse()?,check_cancelled:None};
  let observed=match decompress(&fs::read(root.join(row[2]))?,options){Ok(plain)=>{if row[4]=="ok"{assert_eq!(plain,fs::read(root.join(row[3]))?,"{}",row[0]);}"ok"},Err(error)=>match error.downcast_ref::<c::Failure>().ok_or_else(||error.to_string())?.code{c::Code::InvalidData=>"invalid_data",c::Code::ResourceLimit=>"resource_limit",_=>return Err(error.to_string().into())}};
  assert_eq!(observed,row[4],"{}",row[0]);decisions+=1;
 }
 for line in fs::read_to_string(root.join("exchange.tsv"))?.lines(){
  let row:Vec<_>=line.split('\t').collect();let format=FORMATS.iter().find(|entry|entry.0==row[2]).ok_or("Unknown format")?.1;
  let protocol=PROTOCOLS.iter().find(|entry|entry.0==row[1]).ok_or("Unknown protocol")?.1;let frame=fs::read(root.join(row[3]))?;let options=c::DecodeOptions{format,..Default::default()};
  let value=if let Some(provider)=provider(format){c::decode_with_provider(&frame,|bytes|AccountRecord::decode(bytes,protocol).map_err(c::into_error),options,provider)?}else{c::decode(&frame,|bytes|AccountRecord::decode(bytes,protocol).map_err(c::into_error),options)?};
  assert_eq!(value.account_id,42,"{}",row[0]);assert_eq!(value.label,"shared \u{03bb} \u{1f642}","{}",row[0]);assert_eq!(decompress(&frame,options)?,fs::read(root.join(row[4]))?,"{}",row[0]);exchanges+=1;
 }println!("{{\"decisions\":{decisions},\"exchanges\":{exchanges}}}");Ok(())
}
