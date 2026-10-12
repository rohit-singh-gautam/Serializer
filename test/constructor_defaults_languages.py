"""Regenerate, compile and run typed, record and generic owner defaults in selected SDKs."""
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


def consumer(language, generic_owner_defaults=False):
    """Verify fresh typed/empty-record defaults, shortened calls, and supplied overrides."""
    sources = {
        "cpp": '''#include "message.hpp"
static_assert(default_frame{}.normal.z == 1);
static_assert(default_direction::create().coordinates[2] == 1);
int main() {
  auto box=default_box::create(1,2);auto sized=default_box::sized_default_2(1,2);
  auto supplied=default_box::create(1,2,default_area::create(11,12));
  auto owner=default_frame_owner::create();auto untouched=default_frame_owner::create();
  owner.value.normal.z=8;owner.value.axes[0].x=9;
  auto nested=default_frame_owner::from_nested_default_0();
  if(box.area.width!=3 || box.area.height!=4 || sized.area.width!=10 || sized.area.height!=20 || supplied.area.width!=11 || supplied.area.height!=12 || untouched.value.normal.z!=1 || untouched.value.axes[0].x!=1 || nested.value.axes[0].z!=1 || nested.value.axes[1].z!=1)return 2;
  if(default_frame_owner::from_frame().value.axes[1].y!=1)return 3;
  auto first=default_frame::create_default_0(); auto second=default_frame{};
  auto direction=default_direction::create_default_0(); auto custom=default_vector3::create(2,3,4);
  first.normal.z=8; first.axes[0].x=9;
  return second.normal.z!=1 || second.axes[0].x!=1 || second.axes[1].y!=1 || direction.coordinates[2]!=1 || custom.x!=2 || custom.z!=4;
}
''',
        "java": '''public final class Main {
 public static void main(String[] args) throws Exception {
  var box=Schema.DefaultBox.create_default_2(1,2);var sized=Schema.DefaultBox.sized_default_2(1,2);
  var supplied=Schema.DefaultBox.create(1,2,Schema.DefaultArea.create(11,12));
  var owner=Schema.DefaultFrameOwner.create_default_0();var untouched=Schema.DefaultFrameOwner.create_default_0();
  owner.value.normal.z=8;owner.value.axes[0].x=9;
  var nested=Schema.DefaultFrameOwner.fromNested_default_0();
  if(box.area.width!=3 || box.area.height!=4 || sized.area.width!=10 || sized.area.height!=20 || supplied.area.width!=11 || supplied.area.height!=12 || untouched.value.normal.z!=1 || untouched.value.axes[0].x!=1 || nested.value.axes[0].z!=1 || nested.value.axes[1].z!=1)throw new AssertionError();
  if(Schema.DefaultFrameOwner.fromFrame_default_0().value.axes[1].y!=1)throw new AssertionError();
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
        "javascript": '''import {DefaultFrame,DefaultDirection,DefaultVector3,DefaultArea,DefaultBox,DefaultFrameOwner,Protocol} from './schema.mjs';
const box=DefaultBox.create(1,2),sized=DefaultBox.sized_default_2(1,2),supplied=DefaultBox.create(1,2,DefaultArea.create(11,12));
const owner=DefaultFrameOwner.create(),untouched=DefaultFrameOwner.create();owner.value.normal.z=8;owner.value.axes[0].x=9;
const nested=DefaultFrameOwner.fromNested_default_0();
if(box.area.width!==3 || box.area.height!==4 || sized.area.width!==10 || sized.area.height!==20 || supplied.area.width!==11 || supplied.area.height!==12 || untouched.value.normal.z!==1 || untouched.value.axes[0].x!==1 || nested.value.axes[0].z!==1 || nested.value.axes[1].z!==1)throw Error('empty record default mismatch');
if(DefaultFrameOwner.fromFrame().value.axes[1].y!==1)throw Error('factory empty record default mismatch');
const first=DefaultFrame.create_default_0(),second=new DefaultFrame(),direction=DefaultDirection.create_default_0(),custom=DefaultVector3.create(2,3,4);
first.normal.z=8;first.axes[0].x=9;
const decoded=DefaultFrame.decode(first.encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER),empty=DefaultFrame.decode(new TextEncoder().encode('{}'),Protocol.JSON);
if(decoded.normal.z!==8 || decoded.axes[0].x!==9 || empty.normal.z!==1 || empty.axes[1].y!==1)throw Error('decode default mismatch');
if(second.normal.z!==1 || second.axes[0].x!==1 || second.axes[1].y!==1 || direction.coordinates[2]!==1 || custom.x!==2 || custom.z!==4)throw Error('default mismatch');
''',
        "go": '''package main
func main(){
 box:=DefaultBox_Create_default_2(1,2);sized:=DefaultBox_Sized_default_2(1,2);supplied:=DefaultBox_Create(1,2,DefaultArea_Create(11,12))
 owner:=DefaultFrameOwner_Create_default_0();untouched:=DefaultFrameOwner_Create_default_0();owner.Value.Normal.Z=8;owner.Value.Axes[0].X=9;nested:=DefaultFrameOwner_FromNested_default_0()
 if box.Area.Width!=3 || box.Area.Height!=4 || sized.Area.Width!=10 || sized.Area.Height!=20 || supplied.Area.Width!=11 || supplied.Area.Height!=12 || untouched.Value.Normal.Z!=1 || untouched.Value.Axes[0].X!=1 || nested.Value.Axes[0].Z!=1 || nested.Value.Axes[1].Z!=1{panic("empty record default mismatch")}
 if DefaultFrameOwner_FromFrame_default_0().Value.Axes[1].Y!=1{panic("factory empty record default mismatch")}
 first:=DefaultFrame_Create_default_0(); second:=NewDefaultFrame(); direction:=DefaultDirection_Create_default_0(); custom:=DefaultVector3_Create(2,3,4)
 first.Normal.Z=8;first.Axes[0].X=9
 encoded,err:=first.Encode(BINARY_INTEGER);if err!=nil{panic(err)};decoded,err:=DecodeDefaultFrame(encoded,BINARY_INTEGER);if err!=nil{panic(err)};empty,err:=DecodeDefaultFrame([]byte("{}"),JSON);if err!=nil{panic(err)}
 if decoded.Normal.Z!=8 || decoded.Axes[0].X!=9 || empty.Normal.Z!=1 || empty.Axes[1].Y!=1{panic("decode default mismatch")}
 if second.Normal.Z!=1 || second.Axes[0].X!=1 || second.Axes[1].Y!=1 || direction.Coordinates[2]!=1 || custom.X!=2 || custom.Z!=4{panic("default mismatch")}
}
''',
        "csharp": '''using System; using static SerializerGenerated.Schema;
internal static class Program { private static void Main(){
 var box=DefaultBox.Create_default_2(1,2);var sized=DefaultBox.Sized_default_2(1,2);var supplied=DefaultBox.Create(1,2,DefaultArea.Create(11,12));
 var owner=DefaultFrameOwner.Create_default_0();var untouched=DefaultFrameOwner.Create_default_0();owner.Value.Normal.Z=8;owner.Value.Axes[0].X=9;var nested=DefaultFrameOwner.FromNested_default_0();
 if(box.Area.Width!=3 || box.Area.Height!=4 || sized.Area.Width!=10 || sized.Area.Height!=20 || supplied.Area.Width!=11 || supplied.Area.Height!=12 || untouched.Value.Normal.Z!=1 || untouched.Value.Axes[0].X!=1 || nested.Value.Axes[0].Z!=1 || nested.Value.Axes[1].Z!=1)throw new Exception("empty record default mismatch");
 if(DefaultFrameOwner.FromFrame_default_0().Value.Axes[1].Y!=1)throw new Exception("factory empty record default mismatch");
 var first=DefaultFrame.Create_default_0();var second=new DefaultFrame();var direction=DefaultDirection.Create_default_0();var custom=DefaultVector3.Create(2,3,4);
 first.Normal.Z=8;first.Axes[0].X=9;
 var decoded=DefaultFrame.Decode(first.Encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);var empty=DefaultFrame.Decode(System.Text.Encoding.UTF8.GetBytes("{}"),Protocol.JSON);
 if(decoded.Normal.Z!=8 || decoded.Axes[0].X!=9 || empty.Normal.Z!=1 || empty.Axes[1].Y!=1)throw new Exception("decode default mismatch");
 if(second.Normal.Z!=1 || second.Axes[0].X!=1 || second.Axes[1].Y!=1 || direction.Coordinates[2]!=1 || custom.X!=2 || custom.Z!=4)throw new Exception("default mismatch");
}}
''',
        "rust": '''mod schema;use schema::*;
fn main(){let box_value=DefaultBox::create_default_2(1,2);let sized=DefaultBox::sized_default_2(1,2);let supplied=DefaultBox::create(1,2,DefaultArea::create(11,12));
let mut owner=DefaultFrameOwner::create_default_0();let untouched=DefaultFrameOwner::create_default_0();owner.value.normal.z=8.0;owner.value.axes[0].x=9.0;let nested=DefaultFrameOwner::from_nested_default_0();
assert_eq!((box_value.area.width,box_value.area.height),(3,4));assert_eq!((sized.area.width,sized.area.height),(10,20));assert_eq!((supplied.area.width,supplied.area.height),(11,12));assert_eq!(untouched.value.normal.z,1.0);assert_eq!(untouched.value.axes[0].x,1.0);assert_eq!(nested.value.axes[0].z,1.0);assert_eq!(nested.value.axes[1].z,1.0);assert_eq!(DefaultFrameOwner::from_frame_default_0().value.axes[1].y,1.0);
let mut first=DefaultFrame::create_default_0();let second=DefaultFrame::default();let direction=DefaultDirection::create_default_0();let custom=DefaultVector3::create(2.0,3.0,4.0);
first.normal.z=8.0;first.axes[0].x=9.0;
let decoded=DefaultFrame::decode(&first.encode(Protocol::BinaryInteger).unwrap(),Protocol::BinaryInteger).unwrap();let empty=DefaultFrame::decode(b"{}",Protocol::Json).unwrap();
assert_eq!(decoded.normal.z,8.0);assert_eq!(decoded.axes[0].x,9.0);assert_eq!(empty.normal.z,1.0);assert_eq!(empty.axes[1].y,1.0);
assert_eq!(second.normal.z,1.0);assert_eq!(second.axes[0].x,1.0);assert_eq!(second.axes[1].y,1.0);assert_eq!(direction.coordinates[2],1.0);assert_eq!(custom.x,2.0);assert_eq!(custom.z,4.0);}
''',
        "python": '''from schema import DefaultFrame,DefaultDirection,DefaultVector3,DefaultArea,DefaultBox,DefaultFrameOwner,Protocol
box=DefaultBox.create(1,2);sized=DefaultBox.sized_default_2(1,2);supplied=DefaultBox.create(1,2,DefaultArea.create(11,12))
owner=DefaultFrameOwner.create();untouched=DefaultFrameOwner.create();owner.value.normal.z=8;owner.value.axes[0].x=9;nested=DefaultFrameOwner.from_nested_default_0()
assert (box.area.width,box.area.height)==(3,4) and (sized.area.width,sized.area.height)==(10,20) and (supplied.area.width,supplied.area.height)==(11,12)
assert untouched.value.normal.z==1 and untouched.value.axes[0].x==1 and nested.value.axes[0].z==1 and nested.value.axes[1].z==1
assert DefaultFrameOwner.from_frame().value.axes[1].y==1
first=DefaultFrame.create_default_0();second=DefaultFrame();direction=DefaultDirection.create_default_0();custom=DefaultVector3.create(2,3,4)
first.normal.z=8;first.axes[0].x=9
decoded=DefaultFrame.decode(first.encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);empty=DefaultFrame.decode(b'{}',Protocol.JSON)
assert decoded.normal.z==8 and decoded.axes[0].x==9 and empty.normal.z==1 and empty.axes[1].y==1
one=DefaultDirection.create();two=DefaultDirection.create();one.coordinates[2]=9;assert two.coordinates[2]==1
assert second.normal.z==1 and second.axes[0].x==1 and second.axes[1].y==1 and direction.coordinates[2]==1 and custom.x==2 and custom.z==4
''',
        "swift": '''import Foundation
let box=DefaultBox.create(1,2);let sized=DefaultBox.sized_default_2(1,2);let supplied=DefaultBox.create(1,2,DefaultArea.create(11,12))
var owner=DefaultFrameOwner.create();let untouched=DefaultFrameOwner.create();owner.value.normal.z=8;owner.value.axes[0].x=9;let nested=DefaultFrameOwner.fromNested_default_0()
precondition(box.area.width==3 && box.area.height==4 && sized.area.width==10 && sized.area.height==20 && supplied.area.width==11 && supplied.area.height==12 && untouched.value.normal.z==1 && untouched.value.axes[0].x==1 && nested.value.axes[0].z==1 && nested.value.axes[1].z==1)
precondition(DefaultFrameOwner.fromFrame().value.axes[1].y==1)
var first=DefaultFrame.create_default_0();let second=DefaultFrame();let direction=DefaultDirection.create_default_0();let custom=DefaultVector3.create(2,3,4)
first.normal.z=8;first.axes[0].x=9
let decoded=try DefaultFrame.decode(first.encode(.BINARY_INTEGER),.BINARY_INTEGER);let empty=try DefaultFrame.decode(Array("{}".utf8),.JSON)
precondition(decoded.normal.z==8 && decoded.axes[0].x==9 && empty.normal.z==1 && empty.axes[1].y==1)
precondition(second.normal.z==1 && second.axes[0].x==1 && second.axes[1].y==1 && direction.coordinates[2]==1 && custom.x==2 && custom.z==4)
''',
        "kotlin": '''fun main(){
 val box=DefaultBox.create(1u,2u);val sized=DefaultBox.sized_default_2(1u,2u);val supplied=DefaultBox.create(1u,2u,DefaultArea.create(11u,12u))
 val owner=DefaultFrameOwner.create();val untouched=DefaultFrameOwner.create();owner.value.normal.z=8.0;owner.value.axes[0].x=9.0;val nested=DefaultFrameOwner.fromNested_default_0()
 check(box.area.width==3u && box.area.height==4u && sized.area.width==10u && sized.area.height==20u && supplied.area.width==11u && supplied.area.height==12u && untouched.value.normal.z==1.0 && untouched.value.axes[0].x==1.0 && nested.value.axes[0].z==1.0 && nested.value.axes[1].z==1.0)
 check(DefaultFrameOwner.fromFrame().value.axes[1].y==1.0)
 val first=DefaultFrame.create_default_0();val second=DefaultFrame();val direction=DefaultDirection.create_default_0();val custom=DefaultVector3.create(2.0,3.0,4.0)
 first.normal.z=8.0;first.axes[0].x=9.0
 val decoded=DefaultFrame.decode(first.encode(Protocol.BINARY_INTEGER),Protocol.BINARY_INTEGER);val empty=DefaultFrame.decode("{}".toByteArray(),Protocol.JSON)
 check(decoded.normal.z==8.0 && decoded.axes[0].x==9.0 && empty.normal.z==1.0 && empty.axes[1].y==1.0)
 check(second.normal.z==1.0 && second.axes[0].x==1.0 && second.axes[1].y==1.0 && direction.coordinates[2]==1.0 && custom.x==2.0 && custom.z==4.0)
}
''',
        "c": '''#include "schema.h"
int main(void){default_frame first={0},second={0};default_direction direction={0};default_vector3 custom={0};
default_box box={0},sized={0},supplied={0};default_area area={0};default_frame_owner owner={0},untouched={0},nested={0},factory={0};
if(default_box_create_default_2(&box,1,2)!=srl_ok || default_box_sized_default_2(&sized,1,2)!=srl_ok || default_area_create(&area,11,12)!=srl_ok || default_box_create(&supplied,1,2,&area)!=srl_ok || default_frame_owner_create_default_0(&owner)!=srl_ok || default_frame_owner_create_default_0(&untouched)!=srl_ok || default_frame_owner_from_nested_default_0(&nested)!=srl_ok || default_frame_owner_from_frame_default_0(&factory)!=srl_ok)return 4;
owner.value.normal.z=8;owner.value.axes.data[0].x=9;
int record_failed=box.area.width!=3 || box.area.height!=4 || sized.area.width!=10 || sized.area.height!=20 || supplied.area.width!=11 || supplied.area.height!=12 || untouched.value.normal.z!=1 || untouched.value.axes.data[0].x!=1 || nested.value.axes.data[0].z!=1 || nested.value.axes.data[1].z!=1 || factory.value.axes.data[1].y!=1;
default_box_free(&box);default_box_free(&sized);default_box_free(&supplied);default_area_free(&area);default_frame_owner_free(&owner);default_frame_owner_free(&untouched);default_frame_owner_free(&nested);default_frame_owner_free(&factory);
if(record_failed)return 5;
if(default_frame_create_default_0(&first)!=srl_ok || default_frame_init(&second)!=srl_ok || default_direction_create_default_0(&direction)!=srl_ok || default_vector3_create(&custom,2,3,4)!=srl_ok)return 1;
first.normal.z=8;first.axes.data[0].x=9;
srl_buffer encoded={0};default_frame decoded={0},empty={0};if(default_frame_encode(&first,srl_binary_integer,&encoded)!=srl_ok || default_frame_decode(&decoded,encoded.data,encoded.size,srl_binary_integer,NULL)!=srl_ok || default_frame_decode(&empty,(const uint8_t*)"{}",2,srl_json,NULL)!=srl_ok)return 2;
if(decoded.normal.z!=8 || decoded.axes.data[0].x!=9 || empty.normal.z!=1 || empty.axes.data[1].y!=1)return 3;
default_frame_free(&decoded);default_frame_free(&empty);srl_buffer_free(&encoded);
int failed=second.normal.z!=1 || second.axes.data[0].x!=1 || second.axes.data[1].y!=1 || direction.coordinates.data[2]!=1 || custom.x!=2 || custom.z!=4;
default_frame_free(&first);default_frame_free(&second);default_direction_free(&direction);default_vector3_free(&custom);return failed;}
'''
    }
    source = sources["javascript" if language == "typescript" else language]
    if generic_owner_defaults:
        source = generic_owner_consumer(source, language)
    return source


def generic_owner_consumer(source, language):
    """Check supplied constructor/factory overrides on concretized owner-type defaults."""
    checks = {
        "cpp": ("  auto owner=", '''  auto supplied_frame=default_frame::create_default_0();supplied_frame.normal.z=4;
  auto explicit_owner=default_frame_owner::create(supplied_frame);
  auto explicit_factory=default_frame_owner::from_frame(supplied_frame);
  if(explicit_owner.value.normal.z!=4 || explicit_factory.value.normal.z!=4)return 4;
'''),
        "java": ("  var owner=", '''  var suppliedFrame=Schema.DefaultFrame.create_default_0();suppliedFrame.normal.z=4;
  var explicitOwner=Schema.DefaultFrameOwner.create(suppliedFrame);
  var explicitFactory=Schema.DefaultFrameOwner.fromFrame(suppliedFrame);
  if(explicitOwner.value.normal.z!=4 || explicitFactory.value.normal.z!=4)throw new AssertionError("supplied owner default mismatch");
'''),
        "javascript": ("const owner=", '''const suppliedFrame=DefaultFrame.create_default_0();suppliedFrame.normal.z=4;
const explicitOwner=DefaultFrameOwner.create(suppliedFrame),explicitFactory=DefaultFrameOwner.fromFrame(suppliedFrame);
if(explicitOwner.value.normal.z!==4 || explicitFactory.value.normal.z!==4)throw Error('supplied owner default mismatch');
'''),
        "go": (" owner:=", ''' suppliedFrame:=DefaultFrame_Create_default_0();suppliedFrame.Normal.Z=4
 explicitOwner:=DefaultFrameOwner_Create(suppliedFrame);explicitFactory:=DefaultFrameOwner_FromFrame(suppliedFrame)
 if explicitOwner.Value.Normal.Z!=4 || explicitFactory.Value.Normal.Z!=4{panic("supplied owner default mismatch")}
'''),
        "csharp": (" var owner=", ''' var suppliedFrame=DefaultFrame.Create_default_0();suppliedFrame.Normal.Z=4;
 var explicitOwner=DefaultFrameOwner.Create(suppliedFrame);var explicitFactory=DefaultFrameOwner.FromFrame(suppliedFrame);
 if(explicitOwner.Value.Normal.Z!=4 || explicitFactory.Value.Normal.Z!=4)throw new Exception("supplied owner default mismatch");
'''),
        "rust": ("let mut owner=", '''let mut supplied_frame=DefaultFrame::create_default_0();supplied_frame.normal.z=4.0;
let explicit_owner=DefaultFrameOwner::create(supplied_frame.clone());let explicit_factory=DefaultFrameOwner::from_frame(supplied_frame);
assert_eq!(explicit_owner.value.normal.z,4.0);assert_eq!(explicit_factory.value.normal.z,4.0);
'''),
        "python": ("owner=", '''supplied_frame=DefaultFrame.create_default_0();supplied_frame.normal.z=4
explicit_owner=DefaultFrameOwner.create(supplied_frame);explicit_factory=DefaultFrameOwner.from_frame(supplied_frame)
assert explicit_owner.value.normal.z==4 and explicit_factory.value.normal.z==4
'''),
        "swift": ("var owner=", '''var suppliedFrame=DefaultFrame.create_default_0();suppliedFrame.normal.z=4
let explicitOwner=DefaultFrameOwner.create(suppliedFrame);let explicitFactory=DefaultFrameOwner.fromFrame(suppliedFrame)
precondition(explicitOwner.value.normal.z==4 && explicitFactory.value.normal.z==4)
'''),
        "kotlin": (" val owner=", ''' val suppliedFrame=DefaultFrame.create_default_0();suppliedFrame.normal.z=4.0
 val explicitOwner=DefaultFrameOwner.create(suppliedFrame);val explicitFactory=DefaultFrameOwner.fromFrame(suppliedFrame)
 check(explicitOwner.value.normal.z==4.0 && explicitFactory.value.normal.z==4.0)
'''),
        "c": ("default_box box=", '''default_frame supplied_frame={0},factory_frame={0};default_frame_owner explicit_owner={0},explicit_factory={0};
if(default_frame_create_default_0(&supplied_frame)!=srl_ok || default_frame_create_default_0(&factory_frame)!=srl_ok)return 6;
supplied_frame.normal.z=4;factory_frame.normal.z=4;
/* Owned C bindings move each supplied frame and leave that input zeroed. */
if(default_frame_owner_create(&explicit_owner,&supplied_frame)!=srl_ok || default_frame_owner_from_frame(&explicit_factory,&factory_frame)!=srl_ok)return 6;
int supplied_failed=explicit_owner.value.normal.z!=4 || explicit_factory.value.normal.z!=4 || supplied_frame.normal.z!=0 || factory_frame.normal.z!=0 || supplied_frame.axes.data!=NULL || factory_frame.axes.data!=NULL;
default_frame_free(&supplied_frame);default_frame_free(&factory_frame);default_frame_owner_free(&explicit_owner);default_frame_owner_free(&explicit_factory);
if(supplied_failed)return 7;
''')
    }
    anchor, check = checks["javascript" if language == "typescript" else language]
    if source.count(anchor) != 1:
        raise AssertionError(f"Expected one {language} owner-default insertion point")
    source = source.replace(anchor, check + anchor, 1)
    source = source.replace("default_frame_owner", "generic_default_frame_owner").replace(
        "DefaultFrameOwner", "GenericDefaultFrameOwner")
    # Nested positional literals remain on the included 1.15 concrete owner: an
    # unconstrained Frame cannot accept a default_frame binding before specialization.
    for suffix in ("::from_nested", "_from_nested"):
        source = source.replace("generic_default_frame_owner" + suffix, "default_frame_owner" + suffix)
    for suffix in (".fromNested", "_FromNested", ".FromNested", "::from_nested", ".from_nested"):
        source = source.replace("GenericDefaultFrameOwner" + suffix, "DefaultFrameOwner" + suffix)
    if language in ("javascript", "typescript", "python"):
        source = source.replace("DefaultBox,GenericDefaultFrameOwner,Protocol",
                                "DefaultBox,DefaultFrameOwner,GenericDefaultFrameOwner,Protocol", 1)
    if language == "c":
        source = source.replace("generic_default_frame_owner owner={0},untouched={0},nested={0},factory={0};",
                                "generic_default_frame_owner owner={0},untouched={0},factory={0};default_frame_owner nested={0};")
        source = source.replace("generic_default_frame_owner_free(&nested)", "default_frame_owner_free(&nested)")
    return source


def main():
    """Use the maintained SDK runner with task-owned generated output under an ignored build path."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/constructor-defaults-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    parser.add_argument("--generic-owner-defaults", action="store_true",
                        help="Qualify language 1.16 concretized owner-type defaults; retain the 1.15 fixture otherwise")
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
    schema_name = ("constructor_defaults_owner_portable.serializer" if args.generic_owner_defaults else
                   "constructor_defaults_portable.serializer")
    command = [args.compiler, "--input", ROOT / "test/resources" / schema_name,
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
        (source / FILES[language]).write_text(consumer(language, args.generic_owner_defaults), encoding="utf-8")
        examples.run(runner.build(language, "constructor_defaults", args.build / language, source))
        scope = "concretized generic owner defaults" if args.generic_owner_defaults else "fresh empty-record/typed defaults"
        print(f"PASS: {language}, {scope} and custom arguments", flush=True)


if __name__ == "__main__":
    main()
