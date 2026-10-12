"""Regenerate, compile and run typed nested/fixed-array defaults in the selected SDKs."""
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
    """Verify fresh unit-Z defaults, constructor omissions, arrays, and customized coordinates."""
    sources = {
        "cpp": '''#include "message.hpp"
static_assert(default_frame{}.normal.z == 1);
static_assert(default_direction::create().coordinates[2] == 1);
int main() {
  auto first=default_frame::create_default_0(); auto second=default_frame{};
  auto direction=default_direction::create_default_0(); auto custom=default_vector3::create(2,3,4);
  first.normal.z=8; first.axes[0].x=9;
  return second.normal.z!=1 || second.axes[0].x!=1 || second.axes[1].y!=1 || direction.coordinates[2]!=1 || custom.x!=2 || custom.z!=4;
}
''',
        "java": '''public final class Main {
 public static void main(String[] args) throws Exception {
  var first=Schema.DefaultFrame.create_default_0(); var second=new Schema.DefaultFrame();
  var direction=Schema.DefaultDirection.create_default_0(); var custom=Schema.DefaultVector3.create(2,3,4);
  first.normal.z=8; first.axes[0].x=9;
  var decoded=Schema.DefaultFrame.decode(first.encode(Schema.Protocol.BINARY_INTEGER),Schema.Protocol.BINARY_INTEGER);
  var empty=Schema.DefaultFrame.decode("{}".getBytes(java.nio.charset.StandardCharsets.UTF_8),Schema.Protocol.JSON);
  if(decoded.normal.z!=8 || decoded.axes[0].x!=9 || empty.normal.z!=1 || empty.axes[1].y!=1)throw new AssertionError();
  if(second.normal.z!=1 || second.axes[0].x!=1 || second.axes[1].y!=1 || direction.coordinates[2]!=1 || custom.x!=2 || custom.z!=4)throw new AssertionError();
 }
}
''',
        "javascript": '''import {DefaultFrame,DefaultDirection,DefaultVector3,Protocol} from './schema.mjs';
const first=DefaultFrame.create_default_0(),second=new DefaultFrame(),direction=DefaultDirection.create_default_0(),custom=DefaultVector3.create(2,3,4);
first.normal.z=8;first.axes[0].x=9;
const decoded=DefaultFrame.decode(first.encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER),empty=DefaultFrame.decode(new TextEncoder().encode('{}'),Protocol.JSON);
if(decoded.normal.z!==8 || decoded.axes[0].x!==9 || empty.normal.z!==1 || empty.axes[1].y!==1)throw Error('decode default mismatch');
if(second.normal.z!==1 || second.axes[0].x!==1 || second.axes[1].y!==1 || direction.coordinates[2]!==1 || custom.x!==2 || custom.z!==4)throw Error('default mismatch');
''',
        "go": '''package main
func main(){
 first:=DefaultFrame_Create_default_0(); second:=NewDefaultFrame(); direction:=DefaultDirection_Create_default_0(); custom:=DefaultVector3_Create(2,3,4)
 first.Normal.Z=8;first.Axes[0].X=9
 encoded,err:=first.Encode(BINARY_INTEGER);if err!=nil{panic(err)};decoded,err:=DecodeDefaultFrame(encoded,BINARY_INTEGER);if err!=nil{panic(err)};empty,err:=DecodeDefaultFrame([]byte("{}"),JSON);if err!=nil{panic(err)}
 if decoded.Normal.Z!=8 || decoded.Axes[0].X!=9 || empty.Normal.Z!=1 || empty.Axes[1].Y!=1{panic("decode default mismatch")}
 if second.Normal.Z!=1 || second.Axes[0].X!=1 || second.Axes[1].Y!=1 || direction.Coordinates[2]!=1 || custom.X!=2 || custom.Z!=4{panic("default mismatch")}
}
''',
        "csharp": '''using System; using static SerializerGenerated.Schema;
internal static class Program { private static void Main(){
 var first=DefaultFrame.Create_default_0();var second=new DefaultFrame();var direction=DefaultDirection.Create_default_0();var custom=DefaultVector3.Create(2,3,4);
 first.Normal.Z=8;first.Axes[0].X=9;
 var decoded=DefaultFrame.Decode(first.Encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);var empty=DefaultFrame.Decode(System.Text.Encoding.UTF8.GetBytes("{}"),Protocol.JSON);
 if(decoded.Normal.Z!=8 || decoded.Axes[0].X!=9 || empty.Normal.Z!=1 || empty.Axes[1].Y!=1)throw new Exception("decode default mismatch");
 if(second.Normal.Z!=1 || second.Axes[0].X!=1 || second.Axes[1].Y!=1 || direction.Coordinates[2]!=1 || custom.X!=2 || custom.Z!=4)throw new Exception("default mismatch");
}}
''',
        "rust": '''mod schema;use schema::*;
fn main(){let mut first=DefaultFrame::create_default_0();let second=DefaultFrame::default();let direction=DefaultDirection::create_default_0();let custom=DefaultVector3::create(2.0,3.0,4.0);
first.normal.z=8.0;first.axes[0].x=9.0;
let decoded=DefaultFrame::decode(&first.encode(Protocol::BinaryInteger).unwrap(),Protocol::BinaryInteger).unwrap();let empty=DefaultFrame::decode(b"{}",Protocol::Json).unwrap();
assert_eq!(decoded.normal.z,8.0);assert_eq!(decoded.axes[0].x,9.0);assert_eq!(empty.normal.z,1.0);assert_eq!(empty.axes[1].y,1.0);
assert_eq!(second.normal.z,1.0);assert_eq!(second.axes[0].x,1.0);assert_eq!(second.axes[1].y,1.0);assert_eq!(direction.coordinates[2],1.0);assert_eq!(custom.x,2.0);assert_eq!(custom.z,4.0);}
''',
        "python": '''from schema import DefaultFrame,DefaultDirection,DefaultVector3,Protocol
first=DefaultFrame.create_default_0();second=DefaultFrame();direction=DefaultDirection.create_default_0();custom=DefaultVector3.create(2,3,4)
first.normal.z=8;first.axes[0].x=9
decoded=DefaultFrame.decode(first.encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);empty=DefaultFrame.decode(b'{}',Protocol.JSON)
assert decoded.normal.z==8 and decoded.axes[0].x==9 and empty.normal.z==1 and empty.axes[1].y==1
one=DefaultDirection.create();two=DefaultDirection.create();one.coordinates[2]=9;assert two.coordinates[2]==1
assert second.normal.z==1 and second.axes[0].x==1 and second.axes[1].y==1 and direction.coordinates[2]==1 and custom.x==2 and custom.z==4
''',
        "swift": '''import Foundation
var first=DefaultFrame.create_default_0();let second=DefaultFrame();let direction=DefaultDirection.create_default_0();let custom=DefaultVector3.create(2,3,4)
first.normal.z=8;first.axes[0].x=9
let decoded=try DefaultFrame.decode(first.encode(.BINARY_INTEGER),.BINARY_INTEGER);let empty=try DefaultFrame.decode(Array("{}".utf8),.JSON)
precondition(decoded.normal.z==8 && decoded.axes[0].x==9 && empty.normal.z==1 && empty.axes[1].y==1)
precondition(second.normal.z==1 && second.axes[0].x==1 && second.axes[1].y==1 && direction.coordinates[2]==1 && custom.x==2 && custom.z==4)
''',
        "kotlin": '''fun main(){
 val first=DefaultFrame.create_default_0();val second=DefaultFrame();val direction=DefaultDirection.create_default_0();val custom=DefaultVector3.create(2.0,3.0,4.0)
 first.normal.z=8.0;first.axes[0].x=9.0
 val decoded=DefaultFrame.decode(first.encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);val empty=DefaultFrame.decode("{}".toByteArray(),Protocol.JSON)
 check(decoded.normal.z==8.0 && decoded.axes[0].x==9.0 && empty.normal.z==1.0 && empty.axes[1].y==1.0)
 check(second.normal.z==1.0 && second.axes[0].x==1.0 && second.axes[1].y==1.0 && direction.coordinates[2]==1.0 && custom.x==2.0 && custom.z==4.0)
}
''',
        "c": '''#include "schema.h"
int main(void){default_frame first={0},second={0};default_direction direction={0};default_vector3 custom={0};
if(default_frame_create_default_0(&first)!=srl_ok || default_frame_init(&second)!=srl_ok || default_direction_create_default_0(&direction)!=srl_ok || default_vector3_create(&custom,2,3,4)!=srl_ok)return 1;
first.normal.z=8;first.axes.data[0].x=9;
srl_buffer encoded={0};default_frame decoded={0},empty={0};if(default_frame_encode(&first,srl_binary_integer,&encoded)!=srl_ok || default_frame_decode(&decoded,encoded.data,encoded.size,srl_binary_integer,NULL)!=srl_ok || default_frame_decode(&empty,(const uint8_t*)"{}",2,srl_json,NULL)!=srl_ok)return 2;
if(decoded.normal.z!=8 || decoded.axes.data[0].x!=9 || empty.normal.z!=1 || empty.axes.data[1].y!=1)return 3;
default_frame_free(&decoded);default_frame_free(&empty);srl_buffer_free(&encoded);
int failed=second.normal.z!=1 || second.axes.data[0].x!=1 || second.axes.data[1].y!=1 || direction.coordinates.data[2]!=1 || custom.x!=2 || custom.z!=4;
default_frame_free(&first);default_frame_free(&second);default_direction_free(&direction);default_vector3_free(&custom);return failed;}
'''
    }
    return sources["javascript" if language == "typescript" else language]


def main():
    """Use the maintained SDK runner with task-owned generated output under an ignored build path."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/constructor-defaults-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language): args.build / language / examples.OUTPUTS[language]
               for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/constructor_defaults_portable.serializer",
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
        examples.run(runner.build(language, "constructor_defaults", args.build / language, source))
        print(f"PASS: {language}, typed nested/fixed-array defaults and custom coordinates", flush=True)


if __name__ == "__main__":
    main()
