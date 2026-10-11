"""Qualify native generated owning codecs and transport-only derived metadata."""


def consumer(language, retain=False):
    """Return strict SDK consumers for both declared-size validation policies."""
    sources = {
        "python": r'''from schema import *
def bad(action):
    failed=False
    try: action()
    except (ValueError, IndexError, OverflowError): failed=True
    assert failed
for protocol in Protocol:
    value=SizedEnvelope();value.encoded_size=444;value.payload.encoded_size=333;value.payload.text='crate';value.payload.samples=[1,2,3]
    item=SizedPayload();item.text='\u03c0';value.items=[item]
    data=value.encode(protocol);copy=SizedEnvelope.decode(data,protocol)
    assert value.encoded_size==444 and value.payload.encoded_size==333 and item.encoded_size==0
    assert copy.tag==7 and copy.payload.text=='crate' and copy.items[0].text=='\u03c0'
    if protocol==Protocol.JSON:
        assert copy.encoded_size==0 and b'encoded_size' not in data
    else:
        assert copy.encoded_size==len(data) and copy.payload.encoded_size==len(value.payload.encode(protocol)) and copy.items[0].encoded_size==len(item.encode(protocol))
    bad(lambda:SizedEnvelope.decode(data[:-1],protocol));bad(lambda:SizedEnvelope.decode(data+b'\0',protocol))
    bad(lambda:SizedEnvelope.decode(data,protocol,Limits(max_bytes=len(data)-1)))
    split=SplitSized();split.positional_size=11;split.integer_size=22;split.string_size=33;split.text='bin'
    split_data=split.encode(protocol);sc=SplitSized.decode(split_data,protocol)
    assert (split.positional_size,split.integer_size,split.string_size)==(11,22,33)
    assert sc.positional_size==(len(split_data) if protocol==Protocol.BINARY_NONE else 0)
    assert sc.integer_size==(len(split_data) if protocol==Protocol.BINARY_INTEGER else 0)
    assert sc.string_size==(len(split_data) if protocol==Protocol.BINARY_STRING else 0)
    wire=WireSized();wire.text='wire';wire_data=wire.encode(protocol)
    assert WireSized.decode(wire_data,protocol).text=='wire' and not hasattr(wire,'encoded_size')
    scoped=PayloadSized();scoped.encoded_size=777;scoped.text='body';sd=scoped.encode(protocol);restored=PayloadSized.decode(sd,protocol)
    assert scoped.encoded_size==777
    magic=SizedMagic();magic.encoded_size=99;magic.text='parcel';md=magic.encode(protocol)
    assert magic.encoded_size==99 and SizedMagic.decode(md,protocol).encoded_size==(0 if protocol==Protocol.JSON else len(md))
    if protocol!=Protocol.JSON:
        position=0 if protocol==Protocol.BINARY_NONE else 1 if protocol==Protocol.BINARY_INTEGER else 13
        assert restored.encoded_size==len(sd)-position-4
        assert int.from_bytes(wire_data[position:position+4],'little')==len(wire_data)
        changed=bytearray(md);changed[0]^=1;bad(lambda:SizedMagic.decode(changed,protocol))
        payload=value.payload.encode(protocol);mismatch=bytearray(payload);mismatch[position:position+4]=b'\xff'*4
        MISMATCH
        if protocol!=Protocol.BINARY_NONE:
            length=position+4;bad(lambda:SizedPayload.decode(payload[length:],protocol))
            bad(lambda:SizedPayload.decode(payload[:length]+payload,protocol))
            noncanonical=bytearray(sd);noncanonical.insert(0,0x40)
            assert PayloadSized.decode(noncanonical,protocol).encoded_size==len(noncanonical)-position-5
''',
        "rust": r'''mod schema;
use schema::*;
fn main(){
  for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{
    let mut value=SizedEnvelope::default();value.encoded_size=444;value.payload.encoded_size=333;value.payload.text="crate".into();value.payload.samples=vec![1,2,3];
    let mut item=SizedPayload::default();item.text="\u{03c0}".into();value.items=vec![item];let bytes=value.encode(protocol).unwrap();let copy=SizedEnvelope::decode(&bytes,protocol).unwrap();
    assert_eq!(value.encoded_size,444);assert_eq!(value.payload.encoded_size,333);assert_eq!(value.items[0].encoded_size,0);assert_eq!(copy.payload.text,"crate");assert_eq!(copy.items[0].text,"\u{03c0}");assert_eq!(copy.tag,7);
    if protocol==Protocol::Json{assert_eq!(copy.encoded_size,0);assert!(!String::from_utf8(bytes.clone()).unwrap().contains("encoded_size"));}
    else{assert_eq!(copy.encoded_size,bytes.len() as u64);assert_eq!(copy.payload.encoded_size as usize,value.payload.encode(protocol).unwrap().len());assert_eq!(copy.items[0].encoded_size as usize,value.items[0].encode(protocol).unwrap().len());}
    assert!(SizedEnvelope::decode(&bytes[..bytes.len()-1],protocol).is_err());let mut trailing=bytes.clone();trailing.push(0);assert!(SizedEnvelope::decode(&trailing,protocol).is_err());
    let limits=Limits{max_bytes:bytes.len()-1,..Limits::default()};assert!(SizedEnvelope::decode_with_limits(&bytes,protocol,limits).is_err());
    let mut split=SplitSized::default();split.positional_size=11;split.integer_size=22;split.string_size=33;split.text="bin".into();let sb=split.encode(protocol).unwrap();let sc=SplitSized::decode(&sb,protocol).unwrap();
    assert_eq!((split.positional_size,split.integer_size,split.string_size),(11,22,33));assert_eq!(sc.positional_size as usize,if protocol==Protocol::BinaryNone{sb.len()}else{0});assert_eq!(sc.integer_size as usize,if protocol==Protocol::BinaryInteger{sb.len()}else{0});assert_eq!(sc.string_size as usize,if protocol==Protocol::BinaryString{sb.len()}else{0});
    let mut wire=WireSized::default();wire.text="wire".into();let wb=wire.encode(protocol).unwrap();assert_eq!(WireSized::decode(&wb,protocol).unwrap().text,"wire");
    let mut scoped=PayloadSized::default();scoped.encoded_size=777;scoped.text="body".into();let sd=scoped.encode(protocol).unwrap();let restored=PayloadSized::decode(&sd,protocol).unwrap();assert_eq!(scoped.encoded_size,777);
    let mut magic=SizedMagic::default();magic.encoded_size=99;magic.text="parcel".into();let mut md=magic.encode(protocol).unwrap();assert_eq!(magic.encoded_size,99);assert_eq!(SizedMagic::decode(&md,protocol).unwrap().encoded_size as usize,if protocol==Protocol::Json{0}else{md.len()});
    if protocol!=Protocol::Json{let position=if protocol==Protocol::BinaryNone{0}else if protocol==Protocol::BinaryInteger{1}else{13};assert_eq!(restored.encoded_size as usize,sd.len()-position-4);assert_eq!(u32::from_le_bytes(wb[position..position+4].try_into().unwrap())as usize,wb.len());md[0]^=1;assert!(SizedMagic::decode(&md,protocol).is_err());
      let payload=value.payload.encode(protocol).unwrap();let mut mismatch=payload.clone();mismatch[position..position+4].fill(255);MISMATCH
      if protocol!=Protocol::BinaryNone{let length=position+4;assert!(SizedPayload::decode(&payload[length..],protocol).is_err());let mut duplicate=payload[..length].to_vec();duplicate.extend_from_slice(&payload);assert!(SizedPayload::decode(&duplicate,protocol).is_err());let mut noncanonical=sd.clone();noncanonical.insert(0,0x40);assert_eq!(PayloadSized::decode(&noncanonical,protocol).unwrap().encoded_size as usize,noncanonical.len()-position-5);}
    }
  }
}
''',
        "cpp": r'''#include "message.hpp"
#include <rohit/serializer.hpp>
#include <cassert>
#include <cstdint>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
namespace codec=rohit::serializer;
#undef assert
#define assert(value) do { if (!(value)) { ::std::fprintf(stderr, "auto_size assertion at line %d\n", __LINE__); ::std::exit(1); } } while(false)
// Serialize using the generated native/JSON protocol without mutating source fields.
template<template<codec::serialize_type>typename Protocol,typename Value>std::string encode(const Value&value){rohit::full_stream_auto_alloc output;value.template serialize_out<Protocol>(output);return{reinterpret_cast<const char*>(output.begin()),output.current_offset()};}
// Require complete independently bounded decoding.
template<template<codec::serialize_type>typename Protocol,typename Value>Value decode(const std::string&bytes){auto input=rohit::make_constant_full_stream(bytes.data(),bytes.size());return codec::deserialize_exact<Value,Protocol>(input);}
// Keep error assertions effective in every SDK build configuration.
template<typename Action>void bad(Action action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}assert(failed);}
// Qualify the same record graph across each native wire selection and excluded JSON headers.
template<template<codec::serialize_type>typename Protocol,int Mode>void verify(){
  sized_envelope value{};value.encoded_size=444;value.payload.encoded_size=333;value.payload.text="crate";value.payload.samples={1,2,3};sized_payload item{};item.text="\xCF\x80";value.items={item};
  const auto bytes=encode<Protocol>(value);const auto copy=decode<Protocol,sized_envelope>(bytes);assert(value.encoded_size==444&&value.payload.encoded_size==333&&value.items[0].encoded_size==0);assert(copy.tag==7&&copy.payload.text=="crate"&&copy.items[0].text==item.text);
  if constexpr(Mode==0){assert(copy.encoded_size==0&&bytes.find("encoded_size")==std::string::npos);}else{assert(copy.encoded_size==bytes.size()&&copy.payload.encoded_size==encode<Protocol>(value.payload).size()&&copy.items[0].encoded_size==encode<Protocol>(item).size());}
  bad([&]{decode<Protocol,sized_envelope>(bytes.substr(0,bytes.size()-1));});bad([&]{decode<Protocol,sized_envelope>(bytes+std::string(1,'\0'));});
  split_sized split{};split.positional_size=11;split.integer_size=22;split.string_size=33;split.text="bin";const auto sb=encode<Protocol>(split);const auto sc=decode<Protocol,split_sized>(sb);assert(split.positional_size==11&&split.integer_size==22&&split.string_size==33);assert(sc.positional_size==(Mode==1?sb.size():0)&&sc.integer_size==(Mode==2?sb.size():0)&&sc.string_size==(Mode==3?sb.size():0));
  wire_sized wire{};wire.text="wire";const auto wb=encode<Protocol>(wire);assert((decode<Protocol,wire_sized>(wb)).text=="wire");static_assert(sizeof(wire_sized)==sizeof(std::string));
  payload_sized scoped{};scoped.encoded_size=777;scoped.text="body";const auto sd=encode<Protocol>(scoped);const auto restored=decode<Protocol,payload_sized>(sd);assert(scoped.encoded_size==777);
  sized_magic magic{};magic.encoded_size=99;magic.text="parcel";auto md=encode<Protocol>(magic);assert(magic.encoded_size==99);assert((decode<Protocol,sized_magic>(md)).encoded_size==(Mode==0?0:md.size()));
  if constexpr(Mode!=0){constexpr std::size_t position=Mode==1?0:Mode==2?1:13;assert(restored.encoded_size==sd.size()-position-4);std::uint32_t declared{};for(std::size_t i=0;i<4;++i){declared|=static_cast<std::uint32_t>(static_cast<unsigned char>(wb[position+i]))<<(i*8);}assert(declared==wb.size());md[0]^=1;bad([&]{decode<Protocol,sized_magic>(md);});
    const auto payload=encode<Protocol>(value.payload);auto mismatch=payload;mismatch.replace(position,4,4,static_cast<char>(255));MISMATCH
    if constexpr(Mode!=1){constexpr auto length=position+4;bad([&]{decode<Protocol,sized_payload>(payload.substr(length));});bad([&]{decode<Protocol,sized_payload>(payload.substr(0,length)+payload);});auto noncanonical=sd;noncanonical.insert(0,1,static_cast<char>(0x40));assert((decode<Protocol,payload_sized>(noncanonical)).encoded_size==noncanonical.size()-position-5);}
  }
}
// Execute every supported protocol against ordinary and wire-only declarations.
int main(){verify<codec::json,0>();verify<codec::binary_none,1>();verify<codec::binary_integer,2>();verify<codec::binary_string,3>();}
''',
    }
    if language == "c":
        return c_consumer(retain)
    source = sources[language]
    replacements = {
        "python": "assert SizedPayload.decode(mismatch,protocol).encoded_size==0xffffffff" if retain else "bad(lambda:SizedPayload.decode(mismatch,protocol))",
        "rust": "assert_eq!(SizedPayload::decode(&mismatch,protocol).unwrap().encoded_size,u32::MAX);" if retain else "assert!(SizedPayload::decode(&mismatch,protocol).is_err());",
        "cpp": "assert((decode<Protocol,sized_payload>(mismatch)).encoded_size==std::numeric_limits<std::uint32_t>::max());" if retain else "bad([&]{decode<Protocol,sized_payload>(mismatch);});",
    }
    return source.replace("MISMATCH", replacements[language])


def c_consumer(retain):
    """Preserve C candidate ownership and error statuses when the declared size is untrusted."""
    source = r'''#include "schema.h"
/* Keep ownership explicit while constructing representative nested payloads. */
static void check(bool value){if(!value){fputs("auto_size mismatch\n",stderr);exit(1);}}
/* Allocate a string with the same ordinary ownership contract as decoded values. */
static srl_string text(const char* value){srl_string result={0};result.size=strlen(value);result.data=malloc(result.size+1);check(result.data!=NULL);memcpy(result.data,value,result.size+1);return result;}
/* Read fixed little-endian metadata independently of the generated decoder. */
static uint32_t size_value(const srl_buffer* value,size_t position){uint32_t result=0;for(size_t i=0;i<4;++i){result|=(uint32_t)value->data[position+i]<<(i*8);}return result;}
/* Search only initialized output bytes, because the output is not a C string. */
static bool contains_size_name(const srl_buffer* value){const char name[]="encoded_size";for(size_t i=0;i+sizeof(name)-1<=value->size;++i){if(!memcmp(value->data+i,name,sizeof(name)-1)){return true;}}return false;}
/* Qualify every native codec while releasing every candidate and output allocation. */
int main(void){
 for(int mode=srl_json;mode<=srl_binary_string;++mode){const srl_protocol protocol=(srl_protocol)mode;
  sized_envelope value={0},copy={0};check(sized_envelope_init(&value)==srl_ok);value.encoded_size=444;value.payload.encoded_size=333;srl_string_free(&value.payload.text);value.payload.text=text("crate");
  value.payload.samples.size=3;value.payload.samples.data=malloc(3*sizeof(uint32_t));check(value.payload.samples.data!=NULL);value.payload.samples.data[0]=1;value.payload.samples.data[1]=2;value.payload.samples.data[2]=3;
  value.items.size=1;value.items.data=calloc(1,sizeof(sized_payload));check(value.items.data!=NULL);check(sized_payload_init(&value.items.data[0])==srl_ok);srl_string_free(&value.items.data[0].text);value.items.data[0].text=text("item");
  srl_buffer bytes={0},nested={0},item={0};check(sized_envelope_encode(&value,protocol,&bytes)==srl_ok);check(sized_envelope_decode(&copy,bytes.data,bytes.size,protocol,NULL)==srl_ok);check(value.encoded_size==444&&value.payload.encoded_size==333&&value.items.data[0].encoded_size==0);check(copy.tag==7&&copy.payload.text.size==5&&copy.items.size==1);
  check(sized_payload_encode(&value.payload,protocol,&nested)==srl_ok);check(sized_payload_encode(&value.items.data[0],protocol,&item)==srl_ok);if(protocol==srl_json){check(copy.encoded_size==0&&!contains_size_name(&bytes));}else{check(copy.encoded_size==bytes.size&&copy.payload.encoded_size==nested.size&&copy.items.data[0].encoded_size==item.size);}
  check(sized_envelope_decode(&copy,bytes.data,bytes.size-1,protocol,NULL)!=srl_ok);srl_limits limits=srl_default_limits();limits.max_bytes=bytes.size-1;check(sized_envelope_decode(&copy,bytes.data,bytes.size,protocol,&limits)==srl_limit);check(copy.tag==7);
  split_sized split={0},sc={0};check(split_sized_init(&split)==srl_ok);split.positional_size=11;split.integer_size=22;split.string_size=33;srl_buffer sb={0};check(split_sized_encode(&split,protocol,&sb)==srl_ok);check(split_sized_decode(&sc,sb.data,sb.size,protocol,NULL)==srl_ok);check(split.positional_size==11&&split.integer_size==22&&split.string_size==33);check(sc.positional_size==(protocol==srl_binary_none?sb.size:0)&&sc.integer_size==(protocol==srl_binary_integer?sb.size:0)&&sc.string_size==(protocol==srl_binary_string?sb.size:0));
  wire_sized wire={0},wc={0};check(wire_sized_init(&wire)==srl_ok);srl_string_free(&wire.text);wire.text=text("wire");srl_buffer wb={0};check(wire_sized_encode(&wire,protocol,&wb)==srl_ok);check(wire_sized_decode(&wc,wb.data,wb.size,protocol,NULL)==srl_ok);check(wc.text.size==4);_Static_assert(sizeof(wire_sized)==sizeof(srl_string),"wire_only occupies no storage");
  payload_sized scoped={0},pc={0};check(payload_sized_init(&scoped)==srl_ok);scoped.encoded_size=777;srl_string_free(&scoped.text);scoped.text=text("body");srl_buffer sd={0};check(payload_sized_encode(&scoped,protocol,&sd)==srl_ok);check(payload_sized_decode(&pc,sd.data,sd.size,protocol,NULL)==srl_ok);check(scoped.encoded_size==777);
  sized_magic magic={0},mc={0};check(sized_magic_init(&magic)==srl_ok);magic.encoded_size=99;srl_buffer md={0};check(sized_magic_encode(&magic,protocol,&md)==srl_ok);check(sized_magic_decode(&mc,md.data,md.size,protocol,NULL)==srl_ok);check(magic.encoded_size==99&&mc.encoded_size==(protocol==srl_json?0:md.size));
  if(protocol!=srl_json){const size_t position=protocol==srl_binary_none?0:protocol==srl_binary_integer?1:13;check(pc.encoded_size==sd.size-position-4&&size_value(&wb,position)==wb.size);md.data[0]^=1;check(sized_magic_decode(&mc,md.data,md.size,protocol,NULL)!=srl_ok);
   memset(nested.data+position,255,4);MISMATCH
   if(protocol!=srl_binary_none){const size_t length=position+4;check(sized_payload_decode(&copy.payload,nested.data+length,nested.size-length,protocol,NULL)!=srl_ok);srl_buffer duplicate={0};check(srl_reserve(&duplicate,nested.size+length));duplicate.size=nested.size+length;memcpy(duplicate.data,nested.data,length);memcpy(duplicate.data+length,nested.data,nested.size);check(sized_payload_decode(&copy.payload,duplicate.data,duplicate.size,protocol,NULL)!=srl_ok);srl_buffer_free(&duplicate);
    srl_buffer noncanonical={0};check(srl_reserve(&noncanonical,sd.size+1));noncanonical.size=sd.size+1;noncanonical.data[0]=0x40;memcpy(noncanonical.data+1,sd.data,sd.size);check(payload_sized_decode(&pc,noncanonical.data,noncanonical.size,protocol,NULL)==srl_ok);check(pc.encoded_size==noncanonical.size-position-5);srl_buffer_free(&noncanonical);}
  }
  sized_envelope_free(&value);sized_envelope_free(&copy);split_sized_free(&split);split_sized_free(&sc);wire_sized_free(&wire);wire_sized_free(&wc);payload_sized_free(&scoped);payload_sized_free(&pc);sized_magic_free(&magic);sized_magic_free(&mc);
  srl_buffer_free(&bytes);srl_buffer_free(&nested);srl_buffer_free(&item);srl_buffer_free(&sb);srl_buffer_free(&wb);srl_buffer_free(&sd);srl_buffer_free(&md);
 }
 return 0;
}
'''
    mismatch = ("check(sized_payload_decode(&copy.payload,nested.data,nested.size,protocol,NULL)==srl_ok);check(copy.payload.encoded_size==UINT32_MAX);" if retain else
                "check(sized_payload_decode(&copy.payload,nested.data,nested.size,protocol,NULL)!=srl_ok);check(copy.payload.encoded_size!=UINT32_MAX);")
    return source.replace("MISMATCH", mismatch)
