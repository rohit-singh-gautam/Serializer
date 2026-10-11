"""Compile portable construction, typed defaults and empty transient hints in all SDKs."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)
FILES = {"cpp": "main.cpp", "java": "Main.java", "javascript": "main.mjs", "typescript": "main.mts",
         "go": "main.go", "csharp": "Program.cs", "rust": "main.rs", "python": "main.py",
         "swift": "main.swift", "kotlin": "Main.kt", "c": "main.c"}


def consumer(language):
    """Use generated construction APIs and verify their fresh owned defaults."""
    sources = {
        "cpp": '''#include "message.hpp"
#include <map>
static_assert(construction_point::origin().x==2);
static_assert(construction_record::noop(1).position.y==3);
static_assert(construction_byte_identity::create(9).value==9);
static_assert(construction_identity<std::uint8_t>::from_value(11).value==11);
int main(){
  auto runtime=construction_identity<std::map<int,int>>::create(std::map<int,int>{{1,2}});
  if(runtime.value.at(1)!=2)return 6;
  construction_point direct(5,7);auto point=construction_point::create(5,7);
  construction_record defaults=construction_record::noop(1);auto record=construction_record::positioned(point);
  if(direct.x!=5 || point.y!=7 || defaults.position.x!=2 || defaults.position.y!=3 || record.position.x!=5 || record.tag!=0)return 1;
  auto identity=construction_byte_identity::create(9);auto other=construction_byte_identity::from_value(11);
  if(identity.value!=9 || other.value!=11)return 2;
  static_assert(sizeof(construction_record)==sizeof(construction_point)+sizeof(std::uint64_t));
}
''',
        "java": '''public final class Main {
  public static void main(String[] args){
    var point=Schema.ConstructionPoint.create(5,7);var defaults=Schema.ConstructionRecord.noop(1);var record=Schema.ConstructionRecord.positioned(point);
    if(point.y!=7 || defaults.position.x!=2 || defaults.position.y!=3 || record.position.x!=5 || record.tag!=0)throw new AssertionError();
    if(Schema.ConstructionByteIdentity.create((byte)9).value!=9 || Schema.ConstructionByteIdentity.fromValue((byte)11).value!=11)throw new AssertionError();
  }
}
''',
        "javascript": '''import {ConstructionPoint,ConstructionRecord,ConstructionByteIdentity} from './schema.mjs';
const point=ConstructionPoint.create(5,7),defaults=ConstructionRecord.noop(1),record=ConstructionRecord.positioned(point);
if(point.y!==7 || defaults.position.x!==2 || defaults.position.y!==3 || record.position.x!==5 || record.tag!==0n)throw Error('Construction/default mismatch');
if(ConstructionByteIdentity.create(9).value!==9 || ConstructionByteIdentity.fromValue(11).value!==11)throw Error('Concrete generic construction');
''',
        "go": '''package main
func main(){
  point:=ConstructionPoint_Create(5,7);defaults:=ConstructionRecord_Noop(1);record:=ConstructionRecord_Positioned(point)
  if point.Y!=7 || defaults.Position.X!=2 || defaults.Position.Y!=3 || record.Position.X!=5 || record.Tag!=0{panic("Construction/default mismatch")}
  if ConstructionByteIdentity_Create(9).Value!=9 || ConstructionByteIdentity_FromValue(11).Value!=11{panic("Concrete generic construction")}
}
''',
        "csharp": '''using System;
using static SerializerGenerated.Schema;
internal static class Program {
  private static void Main(){
    var point=ConstructionPoint.Create(5,7);var defaults=ConstructionRecord.Noop(1);var record=ConstructionRecord.Positioned(point);
    if(point.Y!=7 || defaults.Position.X!=2 || defaults.Position.Y!=3 || record.Position.X!=5 || record.Tag!=0)throw new Exception("Construction/default mismatch");
    if(ConstructionByteIdentity.Create(9).Value!=9 || ConstructionByteIdentity.FromValue(11).Value!=11)throw new Exception("Concrete generic construction");
  }
}
''',
        "rust": '''mod schema;
use schema::*;
fn main(){
  let point=ConstructionPoint::create(5.0,7.0);let defaults=ConstructionRecord::noop(1.0);let record=ConstructionRecord::positioned(point);
  assert_eq!(defaults.position.x,2.0);assert_eq!(defaults.position.y,3.0);assert_eq!(record.position.x,5.0);assert_eq!(record.position.y,7.0);assert_eq!(record.tag,0);
  assert_eq!(ConstructionByteIdentity::create(9).value,9);assert_eq!(ConstructionByteIdentity::from_value(11).value,11);
}
''',
        "python": '''from schema import ConstructionPoint,ConstructionRecord,ConstructionByteIdentity
point=ConstructionPoint.create(5,7);defaults=ConstructionRecord.noop(1);record=ConstructionRecord.positioned(point)
assert point.y==7 and defaults.position.x==2 and defaults.position.y==3 and record.position.x==5 and record.tag==0
assert ConstructionByteIdentity.create(9).value==9 and ConstructionByteIdentity.from_value(11).value==11
''',
        "swift": '''import Foundation
let point=ConstructionPoint.create(5,7);let defaults=ConstructionRecord.noop(1);let record=ConstructionRecord.positioned(point)
precondition(point.y==7 && defaults.position.x==2 && defaults.position.y==3 && record.position.x==5 && record.tag==0)
precondition(ConstructionByteIdentity.create(9).value==9 && ConstructionByteIdentity.fromValue(11).value==11)
''',
        "kotlin": '''fun main(){
  val point=ConstructionPoint.create(5.0,7.0);val defaults=ConstructionRecord.noop(1.0);val record=ConstructionRecord.positioned(point)
  check(point.y==7.0 && defaults.position.x==2.0 && defaults.position.y==3.0 && record.position.x==5.0 && record.tag==0uL)
  check(ConstructionByteIdentity.create(9u).value==9u.toUByte() && ConstructionByteIdentity.fromValue(11u).value==11u.toUByte())
}
''',
        "c": '''#include "schema.h"
int main(void){
  construction_point point={0};construction_record defaults={0},record={0};construction_byte_identity identity={0},other={0};
  if(construction_point_create(&point,5,7)!=srl_ok || construction_record_noop(&defaults,1)!=srl_ok || construction_record_positioned(&record,&point)!=srl_ok)return 1;
  if(defaults.position.x!=2 || defaults.position.y!=3 || record.position.x!=5 || record.position.y!=7 || record.tag!=0 || point.x!=0)return 2;
  if(construction_byte_identity_create(&identity,9)!=srl_ok || construction_byte_identity_from_value(&other,11)!=srl_ok || identity.value!=9 || other.value!=11)return 3;
  construction_point_free(&point);construction_record_free(&defaults);construction_record_free(&record);construction_byte_identity_free(&identity);construction_byte_identity_free(&other);
  return 0;
}
''',
    }
    anchors = {
        "cpp": ("  construction_point direct", "  construction_byte_derived derived{};derived.value=13;derived.count=9;if(derived.value!=13 || derived.count!=9)return 3;\n"),
        "java": ("    var point=", "    var derived=new Schema.ConstructionByteDerived();derived.base0.value=13;derived.count=9;if(derived.base0.value!=13 || derived.count!=9)throw new AssertionError();\n"),
        "javascript": ("const point=", "const derived=new ConstructionByteDerived();derived.base0.value=13;derived.count=9;if(derived.base0.value!==13 || derived.count!==9)throw Error('Generic inheritance');\n"),
        "go": ("  point:=", "  derived:=NewConstructionByteDerived();derived.Base0.Value=13;derived.Count=9;if derived.Base0.Value!=13 || derived.Count!=9{panic(\"Generic inheritance\")}\n"),
        "csharp": ("    var point=", "    var derived=new ConstructionByteDerived();derived.Base0.Value=13;derived.Count=9;if(derived.Base0.Value!=13 || derived.Count!=9)throw new Exception(\"Generic inheritance\");\n"),
        "rust": ("  let point=", "  let mut derived=ConstructionByteDerived::default();derived.base0.value=13;derived.count=9;assert_eq!(derived.base0.value,13);assert_eq!(derived.count,9);\n"),
        "python": ("point=", "derived=ConstructionByteDerived();derived.base0.value=13;derived.count=9;assert derived.base0.value==13 and derived.count==9\n"),
        "swift": ("let point=", "var derived=ConstructionByteDerived();derived.base0.value=13;derived.count=9;precondition(derived.base0.value==13 && derived.count==9)\n"),
        "kotlin": ("  val point=", "  val derived=ConstructionByteDerived();derived.base0.value=13u;derived.count=9u;check(derived.base0.value==13u.toUByte() && derived.count==9u)\n"),
        "c": ("  construction_point point=", "  construction_byte_derived derived={0};if(construction_byte_derived_init(&derived)!=srl_ok)return 4;derived.base0.value=13;derived.count=9;if(derived.base0.value!=13 || derived.count!=9)return 5;construction_byte_derived_free(&derived);\n"),
    }
    selected = "javascript" if language == "typescript" else language
    source = sources[selected]
    if selected in ("javascript", "python"):
        source = source.replace("ConstructionByteIdentity", "ConstructionByteIdentity,ConstructionByteDerived", 1)
    anchor, extra = anchors[selected]
    assert anchor in source
    return source.replace(anchor, extra + anchor, 1)


def main():
    """Regenerate each selected output, compile actual SDK consumers and execute them."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/construction-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language): args.build / language / examples.OUTPUTS[language] for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/construction.serializer", "--language", ",".join(targets),
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
        examples.run(runner.build(language, "construction", args.build / language, source))
        print(f"PASS: {language}, portable construction, structured defaults, empty transient hint and owning generic inheritance", flush=True)


if __name__ == "__main__":
    main()
