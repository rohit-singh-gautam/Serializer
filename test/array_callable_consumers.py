"""Native SDK consumers for checked array exposure, packs and ownership boundaries."""

SOURCES = {
    "cpp": r'''#include "message.hpp"
#include <cassert>
#include <stdexcept>
// Require checked access to reject an invalid coordinate.
template<class Action> void rejects(Action action) {bool failed=false;try{action();}catch(const std::exception&){failed=true;}assert(failed);}
// Observe mutable facade availability independently of public backing storage.
template<class T> concept has_set = requires(T& value) {value.set(0,1u);};
static_assert(!has_set<array_exposure::readonly_triangle>);
int main(){
 auto t=array_exposure::triangle::create(7,8,9);assert(t.size()==3 && t.get(1)==8);
 t.set(1,11);const auto& borrowed=t.values();t.set(0,10);assert(borrowed[0]==10);
 rejects([&]{(void)t.get(3);});rejects([&]{t.set(3,1);});assert(t.get(1)==11);
 array_exposure::readonly_triangle r;assert(r.get(0)==4 && r.values()[2]==6);
 array_exposure::grid g;assert(g.get(1,2)==6);g.set(1,1,42);assert(g.get(1,1)==42);
 rejects([&]{(void)g.get(0,3);});rejects([&]{g.set(2,0,1);});assert(g.get(1,1)==42);
 array_exposure::dynamic_values d;assert(d.size()==0 && d.values().empty());rejects([&]{(void)d.get(0);});d.elements.push_back(41);assert(d.get(0)==41);
 array_exposure::owned_values strings("first","second");strings.set(0,"replacement");assert(strings.get(0)=="replacement");
 rohit::full_stream_auto_alloc output;t.serialize_out<rohit::serializer::binary_integer>(output);
 const auto restored=rohit::serializer::deserialize_exact<array_exposure::triangle,rohit::serializer::binary_integer>(rohit::make_constant_stream(output.begin(),output.current_offset()));
 assert(restored.get(0)==10 && restored.get(1)==11);
}
''',
    "java": r'''public final class Main {
 // Require malformed storage or invalid access to fail before replacing a value.
 static void rejects(Runnable body){boolean failed=false;try{body.run();}catch(RuntimeException error){failed=true;}if(!failed)throw new AssertionError();}
 public static void main(String[] args){
  var t=Schema.ArrayExposureTriangle.create(7,8,9);if(t.size()!=3 || t.get(1)!=8)throw new AssertionError();t.set(1,11);var copy=t.values();copy.set(1,99);if(t.get(1)!=11)throw new AssertionError();
  rejects(()->t.get(3));rejects(()->t.set(-1,1));
  var r=new Schema.ArrayExposureReadonlyTriangle();if(r.get(0)!=4 || r.values().get(2)!=6)throw new AssertionError();rejects(()->r.values().set(0,1));for(var method:r.getClass().getMethods())if(method.getName().equals("set"))throw new AssertionError();
  var g=new Schema.ArrayExposureGrid();if(g.get(1,2)!=6)throw new AssertionError();g.set(1,1,42);rejects(()->g.get(0,3));rejects(()->g.set(2,0,1));if(g.get(1,1)!=42)throw new AssertionError();
  var d=new Schema.ArrayExposureDynamicValues();if(d.size()!=0 || !d.values().isEmpty())throw new AssertionError();rejects(()->d.get(0));d.elements=new int[]{41};if(d.get(0)!=41)throw new AssertionError();
  var restored=Schema.ArrayExposureTriangle.decode(t.encode(Schema.Protocol.BINARY_INTEGER),Schema.Protocol.BINARY_INTEGER);if(restored.get(1)!=11)throw new AssertionError();
  t.indices=new int[]{1,2};rejects(()->t.size());rejects(()->t.get(0));
 }
}
''',
    "javascript": r'''import {ArrayExposureTriangle,ArrayExposureReadonlyTriangle,ArrayExposureGrid,ArrayExposureDynamicValues,Protocol} from './schema.mjs';
// Require invalid access to throw before a replacement is published.
function rejects(callback){let failed=false;try{callback();}catch(error){failed=true;}if(!failed)throw Error('invalid access accepted');}
const t=ArrayExposureTriangle.create(7,8,9);if(t.size()!==3 || t.get(1)!==8)throw Error('pack');
t.set(1,11);const copy=t.values();copy[1]=99;if(t.get(1)!==11)throw Error('snapshot');
rejects(()=>t.get(3));rejects(()=>t.set(-1,1));rejects(()=>t.get(0.5));
const r=new ArrayExposureReadonlyTriangle();if(r.get(0)!==4 || r.values()[2]!==6 || 'set' in r)throw Error('readonly');rejects(()=>{r.values()[0]=1;});
const g=new ArrayExposureGrid();if(g.get(1,2)!==6)throw Error('grid');g.set(1,1,42);rejects(()=>g.get(0,3));rejects(()=>g.set(2,0,1));if(g.get(1,1)!==42)throw Error('mutation');
const d=new ArrayExposureDynamicValues();if(d.size()!==0 || d.values().length!==0)throw Error('empty');rejects(()=>d.get(0));d.elements.push(41);if(d.get(0)!==41)throw Error('dynamic');
const restored=ArrayExposureTriangle.decode(t.encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);if(restored.get(1)!==11)throw Error('codec');
t.indices=[1,2];rejects(()=>t.size());rejects(()=>t.get(0));
''',
    "go": r'''package main
// Require invalid access to panic before changing its owner.
func rejects(body func()){defer func(){if recover()==nil{panic("invalid access accepted")}}();body()}
func main(){
 t:=ArrayExposureTriangle_Create(7,8,9);if t.Size()!=3 || t.Get(1)!=8{panic("pack")};t.Set(1,11);copy:=t.Values();copy[1]=99;if t.Get(1)!=11{panic("snapshot")}
 rejects(func(){t.Get(3)});rejects(func(){t.Set(-1,1)})
 r:=NewArrayExposureReadonlyTriangle();if r.Get(0)!=4 || r.Values()[2]!=6{panic("readonly")};if _,ok:=interface{}(r).(interface{Set(int,uint32)});ok{panic("readonly setter")}
 g:=NewArrayExposureGrid();if g.Get(1,2)!=6{panic("grid")};g.Set(1,1,42);rejects(func(){g.Get(0,3)});rejects(func(){g.Set(2,0,1)});if g.Get(1,1)!=42{panic("mutation")}
 d:=NewArrayExposureDynamicValues();if d.Size()!=0 || len(d.Values())!=0{panic("empty")};rejects(func(){d.Get(0)});d.Elements=append(d.Elements,41);if d.Get(0)!=41{panic("dynamic")}
 encoded,err:=t.Encode(BINARY_INTEGER);if err!=nil{panic(err)};restored,err:=DecodeArrayExposureTriangle(encoded,BINARY_INTEGER);if err!=nil || restored.Get(1)!=11{panic("codec")}
 t.Indices=[]uint32{1,2};rejects(func(){t.Size()});rejects(func(){t.Get(0)})
}
''',
    "csharp": r'''using System;using static SerializerGenerated.Schema;
internal static class Program {
 // Require malformed storage or invalid coordinates to fail transactionally.
 private static void Rejects(Action action){bool failed=false;try{action();}catch(Exception){failed=true;}if(!failed)throw new Exception("invalid access accepted");}
 private static void Main(){
  var t=ArrayExposureTriangle.Create(7,8,9);if(t.Size()!=3 || t.Get(1)!=8)throw new Exception("pack");t.Set(1,11);var copy=t.Values();copy[1]=99;if(t.Get(1)!=11)throw new Exception("snapshot");Rejects(()=>t.Get(3));Rejects(()=>t.Set(-1,1));
  var r=new ArrayExposureReadonlyTriangle();if(r.Get(0)!=4 || r.Values()[2]!=6 || r.GetType().GetMethod("Set")!=null)throw new Exception("readonly");Rejects(()=>((System.Collections.Generic.IList<uint>)r.Values())[0]=1);
  var g=new ArrayExposureGrid();if(g.Get(1,2)!=6)throw new Exception("grid");g.Set(1,1,42);Rejects(()=>g.Get(0,3));Rejects(()=>g.Set(2,0,1));if(g.Get(1,1)!=42)throw new Exception("mutation");
  var d=new ArrayExposureDynamicValues();if(d.Size()!=0 || d.Values().Count!=0)throw new Exception("empty");Rejects(()=>d.Get(0));d.Elements.Add(41);if(d.Get(0)!=41)throw new Exception("dynamic");
  var restored=ArrayExposureTriangle.Decode(t.Encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);if(restored.Get(1)!=11)throw new Exception("codec");
  t.Indices.RemoveAt(2);Rejects(()=>t.Size());Rejects(()=>t.Get(0));
 }
}
''',
    "python": r'''from schema import *
# Require malformed storage or invalid coordinates to raise before owner mutation.
def rejects(body):
    try: body()
    except (IndexError,ValueError): return
    raise AssertionError('invalid access accepted')
t=ArrayExposureTriangle.create(7,8,9);assert t.size()==3 and t.get(1)==8
t.set(1,11);copy=t.values();copy[1]=99;assert t.get(1)==11
rejects(lambda:t.get(3));rejects(lambda:t.set(-1,1));rejects(lambda:t.get(0.5))
r=ArrayExposureReadonlyTriangle();assert r.get(0)==4 and r.values()[2]==6 and not hasattr(r,'set') and isinstance(r.values(),tuple)
g=ArrayExposureGrid();assert g.get(1,2)==6;g.set(1,1,42);rejects(lambda:g.get(0,3));rejects(lambda:g.set(2,0,1));assert g.get(1,1)==42
d=ArrayExposureDynamicValues();assert d.size()==0 and d.values()==[];rejects(lambda:d.get(0));d.elements.append(41);assert d.get(0)==41
restored=ArrayExposureTriangle.decode(t.encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);assert restored.get(1)==11
t.indices=[1,2];rejects(t.size);rejects(lambda:t.get(0))
''',
    "rust": r'''mod schema;use schema::*;
fn main(){
 let mut t=ArrayExposureTriangle::create(7,8,9);assert_eq!(t.size().unwrap(),3);assert_eq!(*t.get(1).unwrap(),8);t.set(1,11).unwrap();
 let borrowed=t.values().unwrap();assert!(std::ptr::eq(borrowed.as_ptr(),t.indices.as_ptr()));assert_eq!(borrowed.iter().sum::<u32>(),27);
 assert!(t.get(3).is_err());assert!(t.set(3,1).is_err());assert_eq!(*t.get(1).unwrap(),11);
 let r=ArrayExposureReadonlyTriangle::default();let readonly:&[u32]=r.values().unwrap();assert_eq!(readonly,&[4,5,6]);
 let mut g=ArrayExposureGrid::default();assert_eq!(*g.get(1,2).unwrap(),6);g.set(1,1,42).unwrap();assert!(g.get(0,3).is_err());assert!(g.set(2,0,1).is_err());assert_eq!(*g.get(1,1).unwrap(),42);
 let mut d=ArrayExposureDynamicValues::default();assert_eq!(d.size().unwrap(),0);assert!(d.values().unwrap().is_empty());assert!(d.get(0).is_err());d.elements.push(41);assert_eq!(*d.get(0).unwrap(),41);
 let restored=ArrayExposureTriangle::decode(&t.encode(Protocol::BinaryInteger).unwrap(),Protocol::BinaryInteger).unwrap();assert_eq!(*restored.get(1).unwrap(),11);
 t.indices.pop();assert!(t.size().is_err());assert!(t.get(0).is_err());
}
''',
    "swift": r'''import Foundation
// Evaluate throwing expressions before checking their Boolean result.
func check(_ value:Bool){precondition(value)}
// Require invalid access to throw instead of indexing invalid storage.
func rejects(_ body:() throws -> Void){var failed=false;do{try body()}catch{failed=true};precondition(failed)}
var t=ArrayExposureTriangle.create(7,8,9);check(try t.size()==3);check(try t.get(1)==8);try t.set(1,11)
var copy=try t.values();copy[1]=99;check(try t.get(1)==11);rejects{_ = try t.get(3)};rejects{try t.set(-1,1)}
let r=ArrayExposureReadonlyTriangle();var readonlyCopy=try r.values();readonlyCopy[0]=99;check(try r.get(0)==4)
var g=ArrayExposureGrid();check(try g.get(1,2)==6);try g.set(1,1,42);rejects{_ = try g.get(0,3)};rejects{try g.set(2,0,1)};check(try g.get(1,1)==42)
var d=ArrayExposureDynamicValues();check(try d.size()==0);rejects{_ = try d.get(0)};d.elements.append(41);check(try d.get(0)==41)
let restored=try ArrayExposureTriangle.decode(t.encode(.BINARY_INTEGER),.BINARY_INTEGER);check(try restored.get(1)==11)
t.indices.removeLast();rejects{_ = try t.size()};rejects{_ = try t.get(0)}
''',
    "kotlin": r'''// Require malformed storage or invalid coordinates to fail before owner mutation.
fun rejects(body:()->Unit){var failed=false;try{body()}catch(error:RuntimeException){failed=true};check(failed)}
fun main(){
 val t=ArrayExposureTriangle.create(7u,8u,9u);check(t.size()==3 && t.get(1)==8u);t.set(1,11u);val copy=t.values();copy[1]=99u;check(t.get(1)==11u);rejects{t.get(3)};rejects{t.set(-1,1u)}
 val r=ArrayExposureReadonlyTriangle();check(r.get(0)==4u && r.values()[2]==6u);rejects{(r.values() as MutableList<UInt>)[0]=1u};check(r.javaClass.methods.none{it.name=="set"})
 val g=ArrayExposureGrid();check(g.get(1,2)==6);g.set(1,1,42);rejects{g.get(0,3)};rejects{g.set(2,0,1)};check(g.get(1,1)==42)
 val d=ArrayExposureDynamicValues();check(d.size()==0 && d.values().isEmpty());rejects{d.get(0)};d.elements=uintArrayOf(41u);check(d.get(0)==41u)
 val restored=ArrayExposureTriangle.decode(t.encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);check(restored.get(1)==11u)
 t.indices=uintArrayOf(1u,2u);rejects{t.size()};rejects{t.get(0)}
}
''',
    "c": r'''#include <stdbool.h>
#include <stdlib.h>
static bool reject_allocation;
// Inject candidate allocation failure after the supplied owning inputs are ready.
static void* fixture_calloc(size_t count,size_t bytes){if(reject_allocation)return NULL;return calloc(count,bytes);}
#define calloc fixture_calloc
#include "schema.h"
#undef calloc
int main(void){
 array_exposure_triangle t={0},restored={0};if(array_exposure_triangle_create(&t,7,8,9)!=srl_ok)return 1;
 size_t count=0;const uint32_t* item=NULL;if(array_exposure_triangle_size(&t,&count)!=srl_ok || count!=3 || array_exposure_triangle_get(&t,1,&item)!=srl_ok || *item!=8)return 2;
 if(array_exposure_triangle_set(&t,1,11)!=srl_ok || array_exposure_triangle_get(&t,1,&item)!=srl_ok || *item!=11)return 3;
 const uint32_t sentinel=99;item=&sentinel;if(array_exposure_triangle_get(&t,3,&item)!=srl_invalid || item!=&sentinel || array_exposure_triangle_set(&t,3,1)!=srl_invalid)return 4;
 array_exposure_readonly_triangle r={0};if(array_exposure_readonly_triangle_init(&r)!=srl_ok || array_exposure_readonly_triangle_values(&r,&item,&count)!=srl_ok || count!=3 || item[0]!=4 || item[2]!=6)return 5;
 array_exposure_grid g={0};const int32_t* cell=NULL;if(array_exposure_grid_init(&g)!=srl_ok || array_exposure_grid_get(&g,1,2,&cell)!=srl_ok || *cell!=6 || array_exposure_grid_set(&g,1,1,42)!=srl_ok)return 6;
 if(array_exposure_grid_get(&g,0,3,&cell)!=srl_invalid || array_exposure_grid_set(&g,2,0,1)!=srl_invalid || array_exposure_grid_get(&g,1,1,&cell)!=srl_ok || *cell!=42)return 7;
 array_exposure_dynamic_values d={0};if(array_exposure_dynamic_values_init(&d)!=srl_ok || array_exposure_dynamic_values_values(&d,&item,&count)!=srl_ok || count!=0 || array_exposure_dynamic_values_get(&d,0,&item)!=srl_invalid)return 8;
 d.elements.data=calloc(1,sizeof(*d.elements.data));if(!d.elements.data)return 9;d.elements.capacity=d.elements.size=1;d.elements.data[0]=41;if(array_exposure_dynamic_values_get(&d,0,&item)!=srl_ok || *item!=41)return 10;
 srl_buffer encoded={0};if(array_exposure_triangle_encode(&t,srl_binary_integer,&encoded)!=srl_ok || array_exposure_triangle_decode(&restored,encoded.data,encoded.size,srl_binary_integer,NULL)!=srl_ok || restored.indices.data[1]!=11)return 11;
 t.indices.size=2;if(array_exposure_triangle_size(&t,&count)!=srl_invalid || array_exposure_triangle_get(&t,0,&item)!=srl_invalid)return 12;t.indices.size=3;
 array_exposure_owned_values owned={0};srl_string first={0},second={0},replacement={0};if(array_exposure_owned_values_init(&owned)!=srl_ok || srl_string_set(&first,"first",5)!=srl_ok || srl_string_set(&second,"second",6)!=srl_ok)return 13;
 const void* first_data=first.data;const void* second_data=second.data;const void* previous=owned.entries.data;
 reject_allocation=true;srl_status status=array_exposure_owned_values_create(&owned,&first,&second);reject_allocation=false;
 if(status!=srl_allocation || first.data!=first_data || second.data!=second_data || owned.entries.data!=previous)return 14;
 if(array_exposure_owned_values_create(&owned,&first,&second)!=srl_ok || first.data || second.data || owned.entries.data[0].data!=first_data || owned.entries.data[1].data!=second_data)return 15;
 if(srl_string_set(&replacement,"replacement",11)!=srl_ok)return 16;
 const void* replacement_data=replacement.data;
 if(array_exposure_owned_values_set(&owned,2,&replacement)!=srl_invalid || replacement.data!=replacement_data || owned.entries.data[0].data!=first_data)return 17;
 if(array_exposure_owned_values_set(&owned,0,&replacement)!=srl_ok || replacement.data || owned.entries.data[0].data!=replacement_data)return 18;
 array_exposure_triangle_free(&t);array_exposure_triangle_free(&restored);array_exposure_readonly_triangle_free(&r);array_exposure_grid_free(&g);array_exposure_dynamic_values_free(&d);array_exposure_owned_values_free(&owned);srl_buffer_free(&encoded);srl_string_free(&first);srl_string_free(&second);srl_string_free(&replacement);
 return 0;
}
''',
}


def consumer(language):
    """Return an SDK consumer with additional TypeScript readonly declaration checks."""
    source = SOURCES["javascript" if language == "typescript" else language]
    if language == "java":
        source = source.replace("Schema.ArrayExposure", "Schema.ArrayExposure.")
    if language == "kotlin":
        source = "@file:OptIn(kotlin.ExperimentalUnsignedTypes::class)\n" + source
    if language == "typescript":
        source = source.replace("function rejects(callback)", "function rejects(callback: () => unknown)")
        source = source.replace("r.values()[0]=1;", "(r.values() as number[])[0]=1;")
        source += "\nif(false){\n// @ts-expect-error Readonly exposure deliberately has no setter.\nr.set(0,1);\n}\n"
    return source
