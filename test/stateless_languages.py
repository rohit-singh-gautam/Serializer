# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the public stateless-layout example with every owning language backend."""
import argparse
import subprocess
from pathlib import Path

from record_comparison_languages import examples, FILES, ROOT


def consumer(language):
    """Keep passive APIs, active state and durable wire values independent of layout optimization."""
    sources = {
        "cpp": r'''#include "message.hpp"
#include <type_traits>
#include <string_view>
namespace schema=rohit::serializer;
template<template<schema::serialize_type> class Protocol> void verify(){
  passive_layout::counter<> value{};value.count=73;value.reference={};
  passive_layout::active_counter active{};active.count=73;active.reference.handle=91;
  rohit::full_stream_auto_alloc first{},second{};value.serialize_out<Protocol>(first);active.serialize_out<Protocol>(second);
  if(std::string_view(reinterpret_cast<const char*>(first.begin()),first.current_offset())!=std::string_view(reinterpret_cast<const char*>(second.begin()),second.current_offset()))throw 1;
  auto input=rohit::make_constant_stream(first.begin(),first.current_offset());auto restored=schema::deserialize_exact<passive_layout::compact_counter,Protocol>(input);if(restored.count!=73)throw 2;
  auto active_input=rohit::make_constant_stream(second.begin(),second.current_offset());if(schema::deserialize_exact<passive_layout::active_counter,Protocol>(active_input).reference.handle!=0)throw 3;
}
int main(){static_assert(std::is_empty_v<passive_layout::empty_token>);static_assert(sizeof(passive_layout::counter<>)==sizeof(std::uint32_t));static_assert(sizeof(passive_layout::active_counter)>sizeof(passive_layout::counter<>));verify<schema::json>();verify<schema::binary_none>();verify<schema::binary_integer>();verify<schema::binary_string>();passive_layout::routed_counter routed{};routed.value.count=29;return routed.value.count==29?0:4;}
''',
        "java": r'''public final class Main {public static void main(String[] args){
  for(var protocol:Schema.Protocol.values()){var value=new Schema.PassiveLayout.CompactCounter();value.count=73;value.reference=new Schema.PassiveLayout.EmptyToken();var active=new Schema.PassiveLayout.ActiveCounter();active.count=73;active.reference.handle=91;
    byte[] bytes=value.encode(protocol);if(!java.util.Arrays.equals(bytes,active.encode(protocol)) || Schema.PassiveLayout.CompactCounter.decode(bytes,protocol).count!=73 || Schema.PassiveLayout.ActiveCounter.decode(bytes,protocol).reference.handle!=0)throw new AssertionError();}
  var routed=new Schema.PassiveLayout.RoutedCounter();routed.value.count=29;if(routed.value.count!=29)throw new AssertionError();}}
''',
        "javascript": r'''import {PassiveLayoutCompactCounter,PassiveLayoutEmptyToken,PassiveLayoutActiveCounter,PassiveLayoutRoutedCounter,Protocol} from './schema.mjs';
for(const protocol of Object.values(Protocol)){const value=new PassiveLayoutCompactCounter();value.count=73;value.reference=new PassiveLayoutEmptyToken();const active=new PassiveLayoutActiveCounter();active.count=73;active.reference.handle=91n;const bytes=value.encode(protocol);if(String(bytes)!==String(active.encode(protocol)) || PassiveLayoutCompactCounter.decode(bytes,protocol).count!==73 || PassiveLayoutActiveCounter.decode(bytes,protocol).reference.handle!==0n)throw Error('stateless');}
const routed=new PassiveLayoutRoutedCounter();routed.value.count=29;if(routed.value.count!==29)throw Error('forwarding');
''',
        "go": r'''package main
import "bytes"
func main(){for _,protocol:=range []Protocol{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING}{value:=NewPassiveLayoutCompactCounter();value.Count=73;value.Reference=NewPassiveLayoutEmptyToken();active:=NewPassiveLayoutActiveCounter();active.Count=73;active.Reference.Handle=91;first,err:=value.Encode(protocol);if err!=nil{panic(err)};second,err:=active.Encode(protocol);if err!=nil{panic(err)};restored,err:=DecodePassiveLayoutCompactCounter(first,protocol);if err!=nil{panic(err)};restoredActive,err:=DecodePassiveLayoutActiveCounter(first,protocol);if err!=nil || !bytes.Equal(first,second) || restored.Count!=73 || restoredActive.Reference.Handle!=0{panic("stateless")}};routed:=NewPassiveLayoutRoutedCounter();routed.Value.Count=29;if routed.Value.Count!=29{panic("forwarding")}}
''',
        "csharp": r'''using System;using static SerializerGenerated.Schema;
internal static class Program{private static void Main(){foreach(var protocol in Enum.GetValues<Protocol>()){var value=new PassiveLayoutCompactCounter();value.Count=73;value.Reference=new PassiveLayoutEmptyToken();var active=new PassiveLayoutActiveCounter();active.Count=73;active.Reference.Handle=91;var bytes=value.Encode(protocol);if(!bytes.AsSpan().SequenceEqual(active.Encode(protocol)) || PassiveLayoutCompactCounter.Decode(bytes,protocol).Count!=73 || PassiveLayoutActiveCounter.Decode(bytes,protocol).Reference.Handle!=0)throw new Exception("stateless");}var routed=new PassiveLayoutRoutedCounter();routed.Value.Count=29;if(routed.Value.Count!=29)throw new Exception("forwarding");}}
''',
        "rust": r'''mod schema;use schema::*;
fn main(){for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{let mut value=PassiveLayoutCompactCounter::default();value.count=73;value.reference=PassiveLayoutEmptyToken::default();let mut active=PassiveLayoutActiveCounter::default();active.count=73;active.reference.handle=91;let bytes=value.encode(protocol).unwrap();assert_eq!(bytes,active.encode(protocol).unwrap());assert_eq!(PassiveLayoutCompactCounter::decode(&bytes,protocol).unwrap().count,73);assert_eq!(PassiveLayoutActiveCounter::decode(&bytes,protocol).unwrap().reference.handle,0);}let mut routed=PassiveLayoutRoutedCounter::default();routed.value.count=29;assert_eq!(routed.value.count,29);}
''',
        "python": r'''from schema import *
for protocol in Protocol:
    value=PassiveLayoutCompactCounter();value.count=73;value.reference=PassiveLayoutEmptyToken()
    active=PassiveLayoutActiveCounter();active.count=73;active.reference.handle=91
    encoded=value.encode(protocol);assert encoded==active.encode(protocol)
    assert PassiveLayoutCompactCounter.decode(encoded,protocol).count==73
    assert PassiveLayoutActiveCounter.decode(encoded,protocol).reference.handle==0
routed=PassiveLayoutRoutedCounter();routed.value.count=29;assert routed.value.count==29
''',
        "swift": r'''for protocolValue in [Protocol.JSON,.BINARY_NONE,.BINARY_INTEGER,.BINARY_STRING]{var value=PassiveLayoutCompactCounter();value.count=73;value.reference=PassiveLayoutEmptyToken();var active=PassiveLayoutActiveCounter();active.count=73;active.reference.handle=91;let bytes=try value.encode(protocolValue);let activeBytes=try active.encode(protocolValue);precondition(bytes==activeBytes);let restored=try PassiveLayoutCompactCounter.decode(bytes,protocolValue);let restoredActive=try PassiveLayoutActiveCounter.decode(bytes,protocolValue);precondition(restored.count==73 && restoredActive.reference.handle==0)}
var routed=PassiveLayoutRoutedCounter();routed.value.count=29;precondition(routed.value.count==29)
''',
        "kotlin": r'''fun main(){for(protocol in Protocol.entries){val value=PassiveLayoutCompactCounter();value.count=73u;value.reference=PassiveLayoutEmptyToken();val active=PassiveLayoutActiveCounter();active.count=73u;active.reference.handle=91uL;val bytes=value.encode(protocol);check(bytes.contentEquals(active.encode(protocol)));check(PassiveLayoutCompactCounter.decode(bytes,protocol).count==73u);check(PassiveLayoutActiveCounter.decode(bytes,protocol).reference.handle==0uL)};val routed=PassiveLayoutRoutedCounter();routed.value.count=29u;check(routed.value.count==29u)}
''',
        "c": r'''#include "schema.h"
int main(void){passive_layout_compact_counter value={0},restored={0};passive_layout_active_counter active={0},restored_active={0};if(passive_layout_compact_counter_init(&value)!=srl_ok || passive_layout_active_counter_init(&active)!=srl_ok)return 1;value.count=73;active.count=73;active.reference.handle=91;
for(int protocol=srl_json;protocol<=srl_binary_string;++protocol){srl_buffer first={0},second={0};if(passive_layout_compact_counter_encode(&value,(srl_protocol)protocol,&first)!=srl_ok || passive_layout_active_counter_encode(&active,(srl_protocol)protocol,&second)!=srl_ok)return 2;if(first.size!=second.size || memcmp(first.data,second.data,first.size)!=0)return 3;if(passive_layout_compact_counter_decode(&restored,first.data,first.size,(srl_protocol)protocol,NULL)!=srl_ok || passive_layout_active_counter_decode(&restored_active,first.data,first.size,(srl_protocol)protocol,NULL)!=srl_ok || restored.count!=73 || restored_active.reference.handle!=0)return 4;srl_buffer_free(&first);srl_buffer_free(&second);}passive_layout_routed_counter routed={0};if(passive_layout_routed_counter_init(&routed)!=srl_ok)return 5;routed.value.count=29;return routed.value.count==29?0:6;}
''',
    }
    return sources["javascript" if language == "typescript" else language]


def main():
    """Generate the public schema, execute all protocols and require direct native template rejection."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/stateless-layout-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language):
               args.build / language / examples.OUTPUTS[language] for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "example/schemas/stateless_layout/model.serializer",
               "--language", ",".join(targets), "--cpp.format=false", "--go.package=main", "--kotlin.package="]
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
        examples.run(runner.build(language, "stateless_layout", args.build / language, source))
        print(f"PASS: {language}, stateless/default/forwarded owning APIs, active reference preservation, four protocols", flush=True)
    if "cpp" in languages:
        source = args.build / "consumers/cpp-invalid"
        source.mkdir(parents=True, exist_ok=True)
        (source / "main.cpp").write_text('#include "message.hpp"\npassive_layout::counter<passive_layout::active_token> invalid;\nint main(){}\n', encoding="utf-8")
        try:
            runner.build("cpp", "stateless_layout", args.build / "cpp", source)
        except subprocess.CalledProcessError:
            print("PASS: cpp, direct native stateful template argument rejects", flush=True)
        else:
            raise AssertionError("Native C++ stateless constraint accepted a stateful argument")


if __name__ == "__main__":
    main()
