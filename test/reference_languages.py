"""Compile and execute typed field references with every real Serializer output SDK."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)
FILES = {"cpp": "main.cpp", "java": "Main.java", "javascript": "main.mjs", "typescript": "main.mts",
         "go": "main.go", "csharp": "Program.cs", "rust": "main.rs", "python": "main.py",
         "swift": "main.swift", "kotlin": "Main.kt", "c": "main.c"}


def consumer(language):
    """Exercise live inherited fields, durable round trips, explicit resolution and expiry."""
    sources = {
        "python": '''from schema import *
import gc
import schema
def bad(action):
    try: action()
    except (ValueError, RuntimeError): return
    raise AssertionError('Invalid reference address accepted')
owner=ReferenceOwner();owner.base0.item.label='first'
shared=SharedLabel();shared.bind(owner,'doc',7,3)
borrowed=BorrowedLabel();borrowed.bind(owner,'doc',7,3)
weak=WeakLabel();weak.bind(owner,'doc',7,3)
assert shared.read_value()=='first' and borrowed.read_value()=='first'
owner.base0.item.label='second';assert shared.with_value(lambda value:value)=='second'
for protocol in Protocol:
    assert not SharedLabel.decode(SharedLabel().encode(protocol),protocol).is_bound()
    copy=SharedLabel.decode(shared.encode(protocol),protocol)
    assert not copy.is_bound() and not copy.resolve(owner,'other',7,3)
    assert copy.resolve(owner,'doc',7,3) and copy.read_value()=='second'
    SharedLabel._read(schema._Reader(shared.encode(protocol),protocol,Limits()),copy)
    assert not copy.is_bound() and copy.resolve(owner,'doc',7,3)
    copy.generation=4;assert not copy.is_bound()
    copy.unbind()
partial=SharedLabel();partial.owner_id=7;bad(lambda:partial.encode(Protocol.JSON))
wrong=SharedLabel();wrong.bind(owner,'doc',7,3);wrong.field_path=[999];bad(lambda:wrong.encode(Protocol.JSON));wrong.unbind()
bad(lambda:SharedLabel.decode(b'{"document_id":"doc","owner_id":7,"generation":3,"field_path":[999]}',Protocol.JSON))
bad(lambda:shared.bind(BorrowedLabel(),'doc',7,3))
saved=owner.base0.item;owner.base0.item=None;assert not shared.is_bound();owner.base0.item=saved;del saved
del copy
del owner;gc.collect();assert shared.read_value()=='second'
shared.unbind();gc.collect();assert not borrowed.is_bound() and not weak.is_bound()
''',
        "javascript": '''import {ReferenceOwner,SharedLabel,BorrowedLabel,Protocol} from './schema.mjs';
function bad(action){let failed=false;try{action()}catch(_){failed=true}if(!failed)throw Error('Invalid address accepted');}
const owner=new ReferenceOwner();owner.base0.item.label='first';
const shared=new SharedLabel();shared.bind(owner,'doc',7n,3n);
const borrowed=new BorrowedLabel();borrowed.bind(owner,'doc',7n,3n);
if(shared.readValue()!=='first'||borrowed.readValue()!=='first')throw Error('Initial binding');
owner.base0.item.label='second';if(shared.withValue(value=>value)!=='second')throw Error('Stale field');
for(const protocol of Object.values(Protocol)){
 if(SharedLabel.decode(new SharedLabel().encode(protocol),protocol).isBound())throw Error('Null binding');
 const copy=SharedLabel.decode(shared.encode(protocol),protocol);
 if(copy.isBound()||copy.resolve(owner,'other',7n,3n)||!copy.resolve(owner,'doc',7n,3n)||copy.readValue()!=='second')throw Error('Resolution');
 copy.generation=4n;if(copy.isBound())throw Error('Edited metadata');
}
const partial=new SharedLabel();partial.ownerId=7n;bad(()=>partial.encode(Protocol.JSON));
const wrong=new SharedLabel();wrong.bind(owner,'doc',7n,3n);wrong.fieldPath=[999];bad(()=>wrong.encode(Protocol.JSON));wrong.unbind();
bad(()=>SharedLabel.decode(new TextEncoder().encode('{"document_id":"doc","owner_id":7,"generation":3,"field_path":[999]}'),Protocol.JSON));
bad(()=>Reflect.apply(shared.bind,shared,[new BorrowedLabel(),'doc',7n,3n]));
const saved=owner.base0.item;Reflect.set(owner.base0,'item',null);if(shared.isBound())throw Error('Missing path accepted');owner.base0.item=saved;
borrowed.unbind();if(borrowed.isBound())throw Error('Unbind');
''',
        "java": '''public final class Main {
private static void bad(Runnable action){boolean failed=false;try{action.run();}catch(IllegalArgumentException error){failed=true;}if(!failed)throw new AssertionError("Invalid address accepted");}
public static void main(String[] args) {
var owner=new Schema.ReferenceOwner();owner.base0.item.label="first";
var shared=new Schema.SharedLabel();shared.bind(owner,"doc",7,3);
owner.enabled=true;owner.initial=65;
var enabled=new Schema.SharedEnabled();enabled.bind(owner,"doc",7,3);if(!enabled.withValue(value->value))throw new AssertionError("Boolean boxing");
var initial=new Schema.SharedInitial();initial.bind(owner,"doc",7,3);if(initial.withValue(value->value)!=65)throw new AssertionError("Character boxing");
var revision=new Schema.SharedRevision();revision.bind(owner,"doc",7,3);if(!revision.withValue(value->value).equals("1.2.3"))throw new AssertionError("Version boxing");
var status=new Schema.SharedStatus();status.bind(owner,"doc",7,3);if(status.withValue(value->value)!=owner.status)throw new AssertionError("Enum reference");
var borrowed=new Schema.BorrowedLabel();borrowed.bind(owner,"doc",7,3);
if(!shared.readValue().equals("first")||!borrowed.readValue().equals("first"))throw new AssertionError();
owner.base0.item.label="second";if(!shared.withValue(value->value).equals("second"))throw new AssertionError();
for(var protocol:Schema.Protocol.values()){
 if(Schema.SharedLabel.decode(new Schema.SharedLabel().encode(protocol),protocol).isBound())throw new AssertionError();
 var copy=Schema.SharedLabel.decode(shared.encode(protocol),protocol);
 if(copy.isBound()||copy.resolve(owner,"other",7,3)||!copy.resolve(owner,"doc",7,3)||!copy.readValue().equals("second"))throw new AssertionError();
 copy.generation=4;if(copy.isBound())throw new AssertionError();
}
var partial=new Schema.SharedLabel();partial.ownerId=7;bad(()->partial.encode(Schema.Protocol.JSON));
var wrong=new Schema.SharedLabel();wrong.bind(owner,"doc",7,3);wrong.fieldPath=new int[]{999};bad(()->wrong.encode(Schema.Protocol.JSON));wrong.unbind();
bad(()->Schema.SharedLabel.decode("{\\"document_id\\":\\"doc\\",\\"owner_id\\":7,\\"generation\\":3,\\"field_path\\":[999]}".getBytes(java.nio.charset.StandardCharsets.UTF_8),Schema.Protocol.JSON));
var saved=owner.base0.item;owner.base0.item=null;if(shared.isBound())throw new AssertionError("Missing path accepted");owner.base0.item=saved;
borrowed.unbind();if(borrowed.isBound())throw new AssertionError();
}}
''',
        "csharp": '''using System;
using static SerializerGenerated.Schema;
internal static class Program {
private static void Bad(Action action){bool failed=false;try{action();}catch(ArgumentException){failed=true;}if(!failed)throw new Exception("Invalid address accepted");}
private static void Main(){
var owner=new ReferenceOwner();owner.Base0.Item.Label="first";
var shared=new SharedLabel();shared.Bind(owner,"doc",7,3);
var borrowed=new BorrowedLabel();borrowed.Bind(owner,"doc",7,3);
if(shared.ReadValue()!="first"||borrowed.ReadValue()!="first")throw new Exception("Binding");
owner.Base0.Item.Label="second";if(shared.WithValue(value=>value)!="second")throw new Exception("Live slot");
foreach(Protocol protocol in Enum.GetValues(typeof(Protocol))){
 if(SharedLabel.Decode(new SharedLabel().Encode(protocol),protocol).IsBound())throw new Exception("Null binding");
 var copy=SharedLabel.Decode(shared.Encode(protocol),protocol);
 if(copy.IsBound()||copy.Resolve(owner,"other",7,3)||!copy.Resolve(owner,"doc",7,3)||copy.ReadValue()!="second")throw new Exception("Resolution");
 copy.Generation=4;if(copy.IsBound())throw new Exception("Edited metadata");
}
var partial=new SharedLabel{OwnerId=7};Bad(()=>partial.Encode(Protocol.JSON));
var wrong=new SharedLabel();wrong.Bind(owner,"doc",7,3);wrong.FieldPath.Clear();wrong.FieldPath.Add(999);Bad(()=>wrong.Encode(Protocol.JSON));wrong.Unbind();
Bad(()=>SharedLabel.Decode(System.Text.Encoding.UTF8.GetBytes("{\\"document_id\\":\\"doc\\",\\"owner_id\\":7,\\"generation\\":3,\\"field_path\\":[999]}"),Protocol.JSON));
var saved=owner.Base0.Item;owner.Base0.Item=null!;if(shared.IsBound())throw new Exception("Missing path accepted");owner.Base0.Item=saved;
borrowed.Unbind();if(borrowed.IsBound())throw new Exception("Unbind");}}
''',
        "go": '''package main
func main(){owner:=NewReferenceOwner();owner.Base0.Item.Label="first"
shared:=NewSharedLabel();if shared.Bind(owner,"doc",7,3)!=nil{panic("Binding")}
scope:=NewBorrowedLabelScope(owner);borrowed:=NewBorrowedLabel();if borrowed.Bind(scope,"doc",7,3)!=nil{panic("Borrow binding")}
if value,err:=shared.ReadValue();err!=nil||value!="first"{panic("Read")}
owner.Base0.Item.Label="second";if err:=shared.WithValue(func(value string){if value!="second"{panic("Live slot")}});err!=nil{panic(err)}
for _,protocol:=range []Protocol{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING}{
 nullBytes,err:=NewSharedLabel().Encode(protocol);if err!=nil{panic(err)};null,err:=DecodeSharedLabel(nullBytes,protocol);if err!=nil||null.IsBound(){panic("Null reference")}
 bytes,err:=shared.Encode(protocol);if err!=nil{panic(err)};copy,err:=DecodeSharedLabel(bytes,protocol);if err!=nil{panic(err)}
 if copy.IsBound()||copy.Resolve(owner,"other",7,3)||!copy.Resolve(owner,"doc",7,3){panic("Resolution")}
 input:=srlNewReader(bytes,protocol,DefaultLimits());readSharedLabel(input,copy);if input.err!=nil||copy.IsBound()||!copy.Resolve(owner,"doc",7,3){panic("Same-address decode binding")}
 copy.Generation=4;if copy.IsBound(){panic("Edited metadata")}
}
partial:=NewSharedLabel();partial.OwnerId=7;if _,err:=partial.Encode(JSON);err==nil{panic("Partial address")}
wrong:=NewSharedLabel();if wrong.Bind(owner,"doc",7,3)!=nil{panic("Binding")};wrong.FieldPath=[]uint32{999};if _,err:=wrong.Encode(JSON);err==nil{panic("Wrong path")}
if _,err:=DecodeSharedLabel([]byte(`{"document_id":"doc","owner_id":7,"generation":3,"field_path":[999]}`),JSON);err==nil{panic("Malformed address")}
saved:=owner.Base0.Item;owner.Base0.Item=nil;if shared.IsBound(){panic("Missing path accepted")};owner.Base0.Item=saved
scope.Close();if borrowed.IsBound(){panic("Scope expiry")};if !shared.IsBound(){panic("Shared expiry")}
}
''',
        "rust": '''mod schema;
use schema::*;
use std::rc::Rc;
use std::cell::RefCell;
fn main(){let owner=Rc::new(RefCell::new(ReferenceOwner::default()));owner.borrow_mut().base0.item.label="first".into();
let mut shared=SharedLabel::default();shared.bind(&owner,"doc",7,3).unwrap();
let mut borrowed=BorrowedLabel::default();borrowed.bind(&owner,"doc",7,3).unwrap();
let mut weak=WeakLabel::default();weak.bind(&owner,"doc",7,3).unwrap();
assert_eq!(shared.read_value().unwrap(),"first");owner.borrow_mut().base0.item.label="second".into();assert_eq!(shared.with_value(|value|value.len()).unwrap(),6);
for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{
 let null=SharedLabel::decode(&SharedLabel::default().encode(protocol).unwrap(),protocol).unwrap();assert!(!null.is_bound());
 let mut copy=SharedLabel::decode(&shared.encode(protocol).unwrap(),protocol).unwrap();
 assert!(!copy.is_bound());assert!(!copy.resolve(&owner,"other",7,3));assert!(copy.resolve(&owner,"doc",7,3));assert_eq!(copy.read_value().unwrap(),"second");copy.generation=4;assert!(!copy.is_bound());
}
let mut partial=SharedLabel::default();partial.owner_id=7;assert!(partial.encode(Protocol::Json).is_err());
let mut wrong=SharedLabel::default();wrong.bind(&owner,"doc",7,3).unwrap();wrong.field_path=vec![999];assert!(wrong.encode(Protocol::Json).is_err());wrong.unbind();
assert!(SharedLabel::decode(br#"{"document_id":"doc","owner_id":7,"generation":3,"field_path":[999]}"#,Protocol::Json).is_err());
drop(owner);assert_eq!(shared.read_value().unwrap(),"second");shared.unbind();assert!(!borrowed.is_bound());assert!(!weak.is_bound());
}
''',
        "swift": '''var owner: SrlReferenceOwner<ReferenceOwner>? = SrlReferenceOwner(ReferenceOwner())
owner!.value.base0.item.label="first"
var shared=SharedLabel();try shared.bind(owner!,"doc",7,3)
var borrowed=BorrowedLabel();try borrowed.bind(owner!,"doc",7,3)
guard try shared.readValue()=="first" else{fatalError("Binding")}
owner!.value.base0.item.label="second"
guard try shared.withValue({$0})=="second" else{fatalError("Live slot")}
for protocolValue in [Protocol.JSON,Protocol.BINARY_NONE,Protocol.BINARY_INTEGER,Protocol.BINARY_STRING]{
 let null=try SharedLabel.decode(SharedLabel().encode(protocolValue),protocolValue);if null.isBound(){fatalError("Null reference")}
 var copy=try SharedLabel.decode(shared.encode(protocolValue),protocolValue)
 if copy.isBound() || copy.resolve(owner!,"other",7,3) || !copy.resolve(owner!,"doc",7,3){fatalError("Resolution")}
 guard try copy.readValue()=="second" else{fatalError("Resolved read")};copy.generation=4;if copy.isBound(){fatalError("Edited metadata")}
}
var partial=SharedLabel();partial.ownerId=7;var failed=false;do{_ = try partial.encode(.JSON)}catch{failed=true};if !failed{fatalError("Partial address")}
var wrong=SharedLabel();try wrong.bind(owner!,"doc",7,3);wrong.fieldPath=[999];failed=false;do{_ = try wrong.encode(.JSON)}catch{failed=true};if !failed{fatalError("Wrong path")};wrong.unbind()
failed=false;do{_ = try SharedLabel.decode(Array(#"{"document_id":"doc","owner_id":7,"generation":3,"field_path":[999]}"#.utf8),.JSON)}catch{failed=true};if !failed{fatalError("Malformed address")}
owner=nil;guard try shared.readValue()=="second" else{fatalError("Shared owner")};shared.unbind();if borrowed.isBound(){fatalError("Expired owner")}
''',
        "kotlin": '''@file:OptIn(ExperimentalUnsignedTypes::class)
fun main(){val owner=ReferenceOwner();owner.base0.item.label="first"
val shared=SharedLabel();shared.bind(owner,"doc",7uL,3uL)
val borrowed=BorrowedLabel();borrowed.bind(owner,"doc",7uL,3uL)
check(shared.readValue()=="first"&&borrowed.readValue()=="first")
owner.base0.item.label="second";check(shared.withValue{it}=="second")
for(protocol in Protocol.entries){check(!SharedLabel.decode(SharedLabel().encode(protocol),protocol).isBound());val copy=SharedLabel.decode(shared.encode(protocol),protocol)
check(!copy.isBound()&&!copy.resolve(owner,"other",7uL,3uL)&&copy.resolve(owner,"doc",7uL,3uL)&&copy.readValue()=="second")
copy.srlRead(SrlReader(shared.encode(protocol),protocol,Limits()));check(!copy.isBound()&&copy.resolve(owner,"doc",7uL,3uL));copy.generation=4uL;check(!copy.isBound())}
val partial=SharedLabel();partial.ownerId=7uL;var failed=false;try{partial.encode(Protocol.JSON)}catch(error:IllegalArgumentException){failed=true};check(failed)
val wrong=SharedLabel();wrong.bind(owner,"doc",7uL,3uL);wrong.fieldPath=uintArrayOf(999u);failed=false;try{wrong.encode(Protocol.JSON)}catch(error:IllegalArgumentException){failed=true};check(failed);wrong.unbind()
failed=false;try{SharedLabel.decode("""{"document_id":"doc","owner_id":7,"generation":3,"field_path":[999]}""".toByteArray(),Protocol.JSON)}catch(error:IllegalArgumentException){failed=true};check(failed)
borrowed.unbind();check(!borrowed.isBound())}
''',
        "c": '''#include "schema.h"
static int released;
static shared_label* destroying_reference;
static srl_reference_scope* expected_reentrant_scope;
static int destroyed_references;
/* Dispose of adopted owner storage after its last strong scope lease. */
static void release_owner(void* pointer){reference_owner* owner=(reference_owner*)pointer;reference_owner_free(owner);free(owner);++released;}
/* A final owner release may destroy the wrapper that initiated the release. */
static void release_reentrant_owner(void* pointer){if(!destroying_reference||destroying_reference->srl_reference!=expected_reentrant_scope||(!expected_reentrant_scope&&destroying_reference->srl_reference_document.data)){abort();}shared_label* dying=destroying_reference;destroying_reference=NULL;shared_label_free(dying);free(dying);++destroyed_references;release_owner(pointer);}
/* Require a synchronous borrowed string without copying its storage. */
static void check_label(const srl_string* label,void* context){int* called=(int*)context;if(!srl_string_equal(label,"second")){abort();}++*called;}
int main(void){reference_owner* owner=(reference_owner*)calloc(1,sizeof(*owner));if(!owner||reference_owner_init(owner)!=srl_ok)return 1;
if(srl_string_set_cstr(&owner->base0.item.label,"first")!=srl_ok)return 2;
srl_reference_scope* scope=shared_label_create_scope(owner,release_owner);if(!scope)return 3;
shared_label shared={0};borrowed_label borrowed={0};if(shared_label_init(&shared)!=srl_ok||borrowed_label_init(&borrowed)!=srl_ok)return 4;
if(shared_label_bind(&shared,scope,"doc",7,3)!=srl_ok||borrowed_label_bind(&borrowed,scope,"doc",7,3)!=srl_ok)return 5;
const srl_string* selected=NULL;if(shared_label_read_value(&shared,&selected)!=srl_ok||!srl_string_equal(selected,"first"))return 6;
if(srl_string_set_cstr(&owner->base0.item.label,"second")!=srl_ok)return 7;int called=0;if(shared_label_with_value(&shared,check_label,&called)!=srl_ok||called!=1)return 8;
for(int index=0;index<4;++index){srl_protocol protocol=(srl_protocol)index;srl_buffer bytes={0},null_bytes={0};shared_label copy={0},null_value={0};if(shared_label_encode(&null_value,protocol,&null_bytes)!=srl_ok||shared_label_decode(&null_value,null_bytes.data,null_bytes.size,protocol,NULL)!=srl_ok||shared_label_is_bound(&null_value))return 14;shared_label_free(&null_value);srl_buffer_free(&null_bytes);if(shared_label_encode(&shared,protocol,&bytes)!=srl_ok||shared_label_decode(&copy,bytes.data,bytes.size,protocol,NULL)!=srl_ok)return 9;
if(shared_label_is_bound(&copy)||shared_label_resolve(&copy,scope,"other",7,3)||!shared_label_resolve(&copy,scope,"doc",7,3))return 10;
if(shared_label_decode(&copy,bytes.data,bytes.size,protocol,NULL)!=srl_ok||shared_label_is_bound(&copy)||!shared_label_resolve(&copy,scope,"doc",7,3))return 15;
copy.generation=4;if(shared_label_is_bound(&copy))return 11;shared_label_free(&copy);srl_buffer_free(&bytes);}
shared_label invalid={0};srl_buffer invalid_bytes={0};invalid.owner_id=7;if(shared_label_encode(&invalid,srl_json,&invalid_bytes)==srl_ok)return 16;
if(shared_label_bind(&invalid,scope,"doc",7,3)!=srl_ok)return 17;invalid.field_path.data[0]=999;if(shared_label_encode(&invalid,srl_json,&invalid_bytes)==srl_ok)return 18;shared_label_free(&invalid);
static const char malformed[]="{\\"document_id\\":\\"doc\\",\\"owner_id\\":7,\\"generation\\":3,\\"field_path\\":[999]}";
if(shared_label_decode(&invalid,(const uint8_t*)malformed,sizeof(malformed)-1,srl_json,NULL)==srl_ok)return 19;shared_label_free(&invalid);srl_buffer_free(&invalid_bytes);
if(shared_label_decode(&shared,(const uint8_t*)malformed,sizeof(malformed)-1,srl_json,NULL)==srl_ok||!shared_label_is_bound(&shared))return 31;
srl_reference_scope* untyped=srl_reference_scope_create(owner,NULL);if(!untyped||shared_label_bind(&invalid,untyped,"doc",7,3)==srl_ok)return 20;untyped->owner_type="different_owner";if(shared_label_bind(&invalid,untyped,"doc",7,3)==srl_ok)return 21;srl_reference_scope_release(untyped);shared_label_free(&invalid);
srl_reference_scope_release(scope);if(released||!shared_label_is_bound(&shared))return 12;shared_label_unbind(&shared);if(released!=1||borrowed_label_is_bound(&borrowed))return 13;shared_label_free(&shared);borrowed_label_free(&borrowed);
for(int operation=0;operation<5;++operation){reference_owner* reentrant_owner=(reference_owner*)calloc(1,sizeof(*reentrant_owner));destroying_reference=(shared_label*)calloc(1,sizeof(*destroying_reference));if(!reentrant_owner||!destroying_reference||reference_owner_init(reentrant_owner)!=srl_ok||shared_label_init(destroying_reference)!=srl_ok)return 22;
srl_reference_scope* reentrant_scope=shared_label_create_scope(reentrant_owner,release_reentrant_owner);if(!reentrant_scope||shared_label_bind(destroying_reference,reentrant_scope,"doc",7,3)!=srl_ok)return 23;
srl_buffer encoded={0};if(operation>=2&&shared_label_encode(destroying_reference,srl_json,&encoded)!=srl_ok)return 24;srl_reference_scope_release(reentrant_scope);expected_reentrant_scope=NULL;
if(operation==0){shared_label_unbind(destroying_reference);}else if(operation==1){shared_label_free(destroying_reference);}else if(operation==2){if(shared_label_decode(destroying_reference,encoded.data,encoded.size,srl_json,NULL)!=srl_ok)return 25;}else if(operation==3){srl_reader input={0};input.data=encoded.data;input.size=encoded.size;input.protocol=srl_json;input.limits=srl_default_limits();shared_label_srl_read(&input,destroying_reference);srl_finish(&input);if(input.status!=srl_ok)return 26;}else{reference_owner* replacement=(reference_owner*)calloc(1,sizeof(*replacement));if(!replacement||reference_owner_init(replacement)!=srl_ok)return 27;srl_reference_scope* replacement_scope=shared_label_create_scope(replacement,release_owner);if(!replacement_scope)return 28;expected_reentrant_scope=replacement_scope;if(shared_label_bind(destroying_reference,replacement_scope,"replacement",9,4)!=srl_ok)return 29;expected_reentrant_scope=NULL;srl_reference_scope_release(replacement_scope);}
srl_buffer_free(&encoded);if(destroying_reference||destroyed_references!=operation+1)return 30;}
return 0;}
''',
        "cpp": '''#include "message.hpp"
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>
#include <memory>
#include <stdexcept>
/* Encode one address through an independently selected native protocol. */
template<template<rohit::serializer::serialize_type>class Protocol>std::string encode(const shared_label& value){rohit::full_stream_auto_alloc stream;value.template serialize_out<Protocol>(stream);return {reinterpret_cast<const char*>(stream.begin()),stream.current_offset()};}
/* Require malformed address rejection in release builds as well as debug builds. */
template<typename Action>void bad(Action action){bool failed=false;try{action();}catch(const std::invalid_argument&){failed=true;}if(!failed)throw std::runtime_error("Invalid address accepted");}
/* Check null transport and same-address decoding without retaining a live owner binding. */
template<template<rohit::serializer::serialize_type>class Protocol>void verify(){shared_label value;const auto null_bytes=encode<Protocol>(value);auto null_input=rohit::make_constant_stream(null_bytes.data(),null_bytes.size());value.template serialize_in<Protocol>(null_input);if(value.is_bound())throw std::runtime_error("Null binding");auto owner=std::make_shared<reference_owner>();value.bind(owner,"doc",7,3);const auto bytes=encode<Protocol>(value);auto input=rohit::make_constant_stream(bytes.data(),bytes.size());value.template serialize_in<Protocol>(input);if(value.is_bound())throw std::runtime_error("Decoded binding retained");value.resolve(owner,"doc",7,3);}
int main(){verify<rohit::serializer::json>();verify<rohit::serializer::binary_none>();verify<rohit::serializer::binary_integer>();verify<rohit::serializer::binary_string>();shared_label invalid;invalid.owner_id=7;bad([&]{encode<rohit::serializer::json>(invalid);});invalid.document_id="doc";invalid.generation=3;invalid.field_path={999};bad([&]{encode<rohit::serializer::json>(invalid);});constexpr std::string_view malformed=R"({"document_id":"doc","owner_id":7,"generation":3,"field_path":[999]})";auto malformed_input=rohit::make_constant_stream(malformed.data(),malformed.size());bad([&]{invalid.serialize_in<rohit::serializer::json>(malformed_input);});auto owner=std::make_shared<reference_owner>();owner->item.label="first";shared_label shared;shared.bind(owner,"doc",7,3);borrowed_label borrowed;borrowed.bind(*owner,"doc",7,3);weak_label weak;weak.bind(owner,"doc",7,3);if(shared.read_value()!="first"||borrowed.read_value()!="first")return 1;owner->item.label="second";shared.with_value([](const std::string& value){if(value!="second")throw std::runtime_error("Live slot");});shared.generation=4;if(shared.is_bound())return 2;shared.generation=3;owner.reset();if(shared.read_value()!="second")return 3;shared.unbind();if(weak.is_bound())return 4;}
'''
    }
    if language == "typescript":
        return sources["javascript"].replace("function bad(action)", "function bad(action: () => unknown)")
    return sources[language]


def main():
    """Generate fresh references and require every selected SDK to compile and execute them."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/reference-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    check_name_collisions(args, languages)
    targets = {("js" if language == "javascript" else language): args.build / language / examples.OUTPUTS[language]
               for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/reference_fields.serializer", "--language", ",".join(targets),
               "--cpp.format=false", "--go.package=main", "--kotlin.package="]
    for language, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        command += [f"--{language}.output", destination]
    examples.run(command)
    if "typescript" in languages and "javascript" in languages:
        (args.build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    runner = examples.Runner(args)
    for language in languages:
        source = args.build / "consumers" / language
        source.mkdir(parents=True, exist_ok=True)
        (source / FILES[language]).write_text(consumer(language), encoding="utf-8")
        examples.run(runner.build(language, "references", args.build / language, source))
        print(f"PASS: {language}, typed references, live field slots, explicit resolution, expiry and metadata checks", flush=True)
    if "go" in languages:
        directory = args.build / "go-preserve"
        source = args.build / "consumers" / "go-preserve"
        directory.mkdir(parents=True, exist_ok=True)
        source.mkdir(parents=True, exist_ok=True)
        schema = directory / "model.serializer"
        schema.write_text("serializer version 1.18.0; class Owner stable_ids { public string Text (7); } "
                          "reference Ref = shared(Owner.Text);\n", encoding="utf-8")
        examples.run([args.compiler, "--input", schema, "--language", "go", "--go.output", directory / "schema.go",
                      "--go.package", "main", "--go.naming", "preserve"])
        (source / "main.go").write_text('package main\nfunc main(){owner:=NewOwner();owner.Text="live";ref:=NewRef();'
                                         'if ref.Bind(owner,"doc",1,1)!=nil{panic("Binding")};'
                                         'if value,err:=ref.ReadValue();err!=nil||value!="live"{panic("Read")}}\n', encoding="utf-8")
        examples.run(runner.build("go", "references-preserve", directory, source))
        print("PASS: go preserve naming, exported generated reference metadata", flush=True)


def check_name_collisions(args, languages):
    """Reject helper names only when new reference APIs would otherwise produce invalid code."""
    directory = args.build / "name-collisions"
    directory.mkdir(parents=True, exist_ok=True)

    def generate(language, label, source, diagnostic=None):
        """Check compiler success or an intentional backend diagnostic without stale artifacts."""
        schema = directory / (label + ".serializer")
        schema.write_text(source, encoding="utf-8")
        destination = directory / (label + (".cs" if language == "csharp" else ".mjs"))
        result = subprocess.run([str(args.compiler), "--input", str(schema), "--language", language,
                                 f"--{language}.output", str(destination)], capture_output=True, text=True,
                                encoding="utf-8", timeout=60)
        if diagnostic:
            if result.returncode == 0 or diagnostic not in result.stdout + result.stderr:
                raise AssertionError(f"Missing {label} diagnostic: {result.stdout}{result.stderr}")
        elif result.returncode:
            raise AssertionError(f"Unexpected {label} failure: {result.stdout}{result.stderr}")

    prefix = "serializer version 1.18.0; class owner stable_ids { public int32 value (7); } "
    if "csharp" in languages:
        for name in ("Bind", "Resolve", "ReadValue", "WithValue", "Unbind", "IsBound", "SrlValidateReferenceAddress",
                     "SrlReferencePathMatches", "Encode", "Decode", "Read", "Write"):
            generate("csharp", "csharp_" + name,
                     prefix + f"reference {name} = shared(owner.value);",
                     "reference helper/class name collision")
        print("PASS: C# reference helper/class collisions reject before publication", flush=True)
    if "javascript" in languages or "typescript" in languages:
        source = prefix + "class WeakRef {} "
        for mode in ("borrowed", "weak"):
            generate("js", "js_WeakRef_" + mode, source + f"reference ref = {mode}(owner.value);",
                     "weak reference runtime name collision")
        generate("js", "js_WeakRef_ordinary", source)
        generate("js", "js_WeakRef_shared", source + "reference ref = shared(owner.value);")
        print("PASS: JS WeakRef shadows reject only when weak runtime support is needed", flush=True)


if __name__ == "__main__":
    main()
