"""Compile and execute class-configured record comparisons in every supported SDK."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)
FILES = {"cpp": "main.cpp", "java": "Main.java", "javascript": "main.mjs",
         "typescript": "main.mts", "go": "main.go", "csharp": "Program.cs", "rust": "main.rs",
         "python": "main.py", "swift": "main.swift", "kotlin": "Main.kt", "c": "main.c"}


def consumer(language):
    """Check value semantics and ordering without exercising object identity or platform hashing."""
    sources = {
        "cpp": r'''#include "message.hpp"
#include <limits>
int main(){
  comparison_key a{},b{};a.cache=9;if(!(a==b))return 1;
  a.rank=std::numeric_limits<std::uint64_t>::max();b.rank=1;if(!(a>b))return 2;
  a.rank=b.rank=0;b.sequence=1;if(!(a<b))return 3;b.sequence=0;b.active=true;if(!(a<b))return 4;
  b.active=false;a.label="\xF0\x90\x80\x80";b.label="\xEE\x80\x80";if(!(a>b))return 5;
  a.label=b.label="";b.stage=comparison_stage::queued;if(!(a<b))return 6;b.stage=comparison_stage::idle;
  b.atom.ordinal=1;if(!(a<b))return 7;
  comparison_measurement x{},y{};x.value=-0.0;if(!(x==y) || (x<=>y)!=0)return 8;
  x.value=std::numeric_limits<double>::quiet_NaN();if(x==x || (x<=>y)!=std::partial_ordering::unordered)return 9;
  comparison_derived d{},e{};d.category=1;e.sequence=9;if(!(d>e))return 10;
  comparison_selected s{},t{};s.category=99;s.unused=99;if(!(s==t))return 11;s.cache=1;if(!(s>t))return 12;
  comparison_byte g{},h{};h.value=1;if(!(g<h))return 13;
  if(!(comparison_empty{}==comparison_empty{}))return 14;
}
''',
        "java": r'''public final class Main {
  private static void check(boolean condition){if(!condition)throw new AssertionError();}
  public static void main(String[] args){
    var a=new Schema.ComparisonKey();var b=new Schema.ComparisonKey();a.cache=9;check(a.equals_value(b));
    a.rank=-1L;b.rank=1;check(a.compare_value(b)==1);a.rank=b.rank=0;b.sequence=1;check(a.compare_value(b)==-1);
    b.sequence=0;b.active=true;check(a.compare_value(b)==-1);b.active=false;
    a.label="\uD800\uDC00";b.label="\uE000";check(a.compare_value(b)==1);a.label=b.label="";
    b.stage=Schema.ComparisonStage.QUEUED;check(a.compare_value(b)==-1);b.stage=Schema.ComparisonStage.IDLE;
    b.atom.ordinal=1;check(a.compare_value(b)==-1);
    var x=new Schema.ComparisonMeasurement();var y=new Schema.ComparisonMeasurement();x.value=-0.0;check(x.equals_value(y) && x.compare_value(y)==0);
    x.value=Double.NaN;check(!x.equals_value(x) && x.compare_value(y)==null);
    var d=new Schema.ComparisonDerived();var e=new Schema.ComparisonDerived();d.base0.category=1;e.sequence=9;check(d.compare_value(e)==1);
    var s=new Schema.ComparisonSelected();var t=new Schema.ComparisonSelected();s.base0.category=99;s.unused=99;check(s.equals_value(t));s.cache=1;check(s.compare_value(t)==1);
    var g=new Schema.ComparisonByte();var h=new Schema.ComparisonByte();h.value=1;check(g.compare_value(h)==-1);
    check(new Schema.ComparisonEmpty().equals_value(new Schema.ComparisonEmpty()));
  }
}
''',
        "javascript": r'''import {ComparisonKey,ComparisonMeasurement,ComparisonDerived,ComparisonSelected,ComparisonByte,ComparisonEmpty,ComparisonStage} from './schema.mjs';
function check(condition){if(!condition)throw Error('Record comparison mismatch');}
const a=new ComparisonKey(),b=new ComparisonKey();a.cache=9;check(a.equals_value(b));
a.rank=0xffffffffffffffffn;b.rank=1n;check(a.compare_value(b)===1);a.rank=b.rank=0n;b.sequence=1;check(a.compare_value(b)===-1);
b.sequence=0;b.active=true;check(a.compare_value(b)===-1);b.active=false;
a.label='\u{10000}';b.label='\uE000';check(a.compare_value(b)===1);a.label=b.label='';
b.stage=ComparisonStage.Queued;check(a.compare_value(b)===-1);b.stage=ComparisonStage.Idle;
b.atom.ordinal=1n;check(a.compare_value(b)===-1);
const x=new ComparisonMeasurement(),y=new ComparisonMeasurement();x.value=-0;check(x.equals_value(y) && x.compare_value(y)===0);
x.value=NaN;check(!x.equals_value(x) && x.compare_value(y)===null);
const d=new ComparisonDerived(),e=new ComparisonDerived();d.base0.category=1;e.sequence=9;check(d.compare_value(e)===1);
const s=new ComparisonSelected(),t=new ComparisonSelected();s.base0.category=99;s.unused=99;check(s.equals_value(t));s.cache=1;check(s.compare_value(t)===1);
const g=new ComparisonByte(),h=new ComparisonByte();h.value=1;check(g.compare_value(h)===-1);check(new ComparisonEmpty().equals_value(new ComparisonEmpty()));
''',
        "go": r'''package main
import "math"
func check(value bool){if !value{panic("Record comparison mismatch")}}
func order(a,b *ComparisonKey,want int){result,ordered:=a.CompareValue(b);check(ordered && result==want)}
func main(){
  a,b:=NewComparisonKey(),NewComparisonKey();a.Cache=9;check(a.EqualsValue(b));a.Rank=^uint64(0);b.Rank=1;order(a,b,1)
  a.Rank=0;b.Rank=0;b.Sequence=1;order(a,b,-1);b.Sequence=0;b.Active=true;order(a,b,-1);b.Active=false
  a.Label="\U00010000";b.Label="\uE000";order(a,b,1);a.Label="";b.Label=""
  b.Stage=ComparisonStageQueued;order(a,b,-1);b.Stage=ComparisonStageIdle;b.Atom.Ordinal=1;order(a,b,-1)
  x,y:=NewComparisonMeasurement(),NewComparisonMeasurement();x.Value=math.Copysign(0,-1);check(x.EqualsValue(y));result,ordered:=x.CompareValue(y);check(ordered && result==0)
  x.Value=math.NaN();check(!x.EqualsValue(x));_,ordered=x.CompareValue(y);check(!ordered)
  d,e:=NewComparisonDerived(),NewComparisonDerived();d.Base0.Category=1;e.Sequence=9;result,ordered=d.CompareValue(e);check(ordered && result==1)
  s,t:=NewComparisonSelected(),NewComparisonSelected();s.Base0.Category=99;s.Unused=99;check(s.EqualsValue(t));s.Cache=1;result,ordered=s.CompareValue(t);check(ordered && result==1)
  g,h:=NewComparisonByte(),NewComparisonByte();h.Value=1;result,ordered=g.CompareValue(h);check(ordered && result==-1);check(NewComparisonEmpty().EqualsValue(NewComparisonEmpty()))
}
''',
        "csharp": r'''using System;
using static SerializerGenerated.Schema;
internal static class Program {
  private static void Check(bool value){if(!value)throw new Exception("Record comparison mismatch");}
  private static void Main(){
    var a=new ComparisonKey();var b=new ComparisonKey();a.Cache=9;Check(a.equals_value(b));a.Rank=ulong.MaxValue;b.Rank=1;Check(a.compare_value(b)==1);
    a.Rank=b.Rank=0;b.Sequence=1;Check(a.compare_value(b)==-1);b.Sequence=0;b.Active=true;Check(a.compare_value(b)==-1);b.Active=false;
    a.Label="\U00010000";b.Label="\uE000";Check(a.compare_value(b)==1);a.Label=b.Label="";
    b.Stage=ComparisonStage.Queued;Check(a.compare_value(b)==-1);b.Stage=ComparisonStage.Idle;b.Atom.Ordinal=1;Check(a.compare_value(b)==-1);
    var x=new ComparisonMeasurement();var y=new ComparisonMeasurement();x.Value=-0.0;Check(x.equals_value(y) && x.compare_value(y)==0);x.Value=double.NaN;Check(!x.equals_value(x) && x.compare_value(y)==null);
    var d=new ComparisonDerived();var e=new ComparisonDerived();d.Base0.Category=1;e.Sequence=9;Check(d.compare_value(e)==1);
    var s=new ComparisonSelected();var t=new ComparisonSelected();s.Base0.Category=99;s.Unused=99;Check(s.equals_value(t));s.Cache=1;Check(s.compare_value(t)==1);
    var g=new ComparisonByte();var h=new ComparisonByte();h.Value=1;Check(g.compare_value(h)==-1);Check(new ComparisonEmpty().equals_value(new ComparisonEmpty()));
  }
}
''',
        "rust": r'''mod schema;
use schema::*;
use std::cmp::Ordering;
fn main(){
  let mut a=ComparisonKey::default();let mut b=ComparisonKey::default();a.cache=9;assert!(a.equals_value(&b));a.rank=u64::MAX;b.rank=1;assert_eq!(a.compare_value(&b),Some(Ordering::Greater));
  a.rank=0;b.rank=0;b.sequence=1;assert_eq!(a.compare_value(&b),Some(Ordering::Less));b.sequence=0;b.active=true;assert_eq!(a.compare_value(&b),Some(Ordering::Less));b.active=false;
  a.label="\u{10000}".into();b.label="\u{E000}".into();assert_eq!(a.compare_value(&b),Some(Ordering::Greater));a.label.clear();b.label.clear();
  b.stage=ComparisonStage::Queued;assert_eq!(a.compare_value(&b),Some(Ordering::Less));b.stage=ComparisonStage::Idle;b.atom.ordinal=1;assert_eq!(a.compare_value(&b),Some(Ordering::Less));
  let mut x=ComparisonMeasurement::default();let y=ComparisonMeasurement::default();x.value=-0.0;assert!(x.equals_value(&y));assert_eq!(x.compare_value(&y),Some(Ordering::Equal));x.value=f64::NAN;assert!(!x.equals_value(&x));assert_eq!(x.compare_value(&y),None);
  let mut d=ComparisonDerived::default();let mut e=ComparisonDerived::default();d.base0.category=1;e.sequence=9;assert_eq!(d.compare_value(&e),Some(Ordering::Greater));
  let mut s=ComparisonSelected::default();let t=ComparisonSelected::default();s.base0.category=99;s.unused=99;assert!(s.equals_value(&t));s.cache=1;assert_eq!(s.compare_value(&t),Some(Ordering::Greater));
  let g=ComparisonByte::default();let mut h=ComparisonByte::default();h.value=1;assert_eq!(g.compare_value(&h),Some(Ordering::Less));assert!(ComparisonEmpty::default().equals_value(&ComparisonEmpty::default()));
}
''',
        "python": r'''from schema import *
a,b=ComparisonKey(),ComparisonKey();a.cache=9;assert a.equals_value(b)
a.rank=(1<<64)-1;b.rank=1;assert a.compare_value(b)==1
a.rank=b.rank=0;b.sequence=1;assert a.compare_value(b)==-1
b.sequence=0;b.active=True;assert a.compare_value(b)==-1
b.active=False;a.label='\U00010000';b.label='\uE000';assert a.compare_value(b)==1
a.label=b.label='';b.stage=ComparisonStage.Queued;assert a.compare_value(b)==-1
b.stage=ComparisonStage.Idle;b.atom.ordinal=1;assert a.compare_value(b)==-1
x,y=ComparisonMeasurement(),ComparisonMeasurement();x.value=-0.0;assert x.equals_value(y) and x.compare_value(y)==0
x.value=float('nan');assert not x.equals_value(x) and x.compare_value(y) is None
d,e=ComparisonDerived(),ComparisonDerived();d.base0.category=1;e.sequence=9;assert d.compare_value(e)==1
s,t=ComparisonSelected(),ComparisonSelected();s.base0.category=99;s.unused=99;assert s.equals_value(t)
s.cache=1;assert s.compare_value(t)==1
g,h=ComparisonByte(),ComparisonByte();h.value=1;assert g.compare_value(h)==-1
assert ComparisonEmpty().equals_value(ComparisonEmpty())
''',
        "swift": r'''import Foundation
var a=ComparisonKey(),b=ComparisonKey();a.cache=9;precondition(a.equals_value(b));a.rank=UInt64.max;b.rank=1;precondition(a.compare_value(b)==1)
a.rank=0;b.rank=0;b.sequence=1;precondition(a.compare_value(b)==(-1));b.sequence=0;b.active=true;precondition(a.compare_value(b)==(-1));b.active=false
a.label="\u{10000}";b.label="\u{E000}";precondition(a.compare_value(b)==1);a.label="";b.label=""
b.stage = .Queued;precondition(a.compare_value(b)==(-1));b.stage = .Idle;b.atom.ordinal=1;precondition(a.compare_value(b)==(-1))
var x=ComparisonMeasurement();let y=ComparisonMeasurement();x.value = -0.0;precondition(x.equals_value(y) && x.compare_value(y)==0);x.value = .nan;precondition(!x.equals_value(x) && x.compare_value(y)==nil)
var d=ComparisonDerived(),e=ComparisonDerived();d.base0.category=1;e.sequence=9;precondition(d.compare_value(e)==1)
var s=ComparisonSelected();let t=ComparisonSelected();s.base0.category=99;s.unused=99;precondition(s.equals_value(t));s.cache=1;precondition(s.compare_value(t)==1)
let g=ComparisonByte();var h=ComparisonByte();h.value=1;precondition(g.compare_value(h)==(-1));precondition(ComparisonEmpty().equals_value(ComparisonEmpty()))
''',
        "kotlin": r'''fun main(){
  val a=ComparisonKey();val b=ComparisonKey();a.cache=9u;check(a.equals_value(b));a.rank=ULong.MAX_VALUE;b.rank=1uL;check(a.compare_value(b)==1)
  a.rank=0uL;b.rank=0uL;b.sequence=1u;check(a.compare_value(b)==-1);b.sequence=0u;b.active=true;check(a.compare_value(b)==-1);b.active=false
  a.label="\uD800\uDC00";b.label="\uE000";check(a.compare_value(b)==1);a.label="";b.label=""
  b.stage=ComparisonStage.Queued;check(a.compare_value(b)==-1);b.stage=ComparisonStage.Idle;b.atom.ordinal=1uL;check(a.compare_value(b)==-1)
  val x=ComparisonMeasurement();val y=ComparisonMeasurement();x.value=-0.0;check(x.equals_value(y) && x.compare_value(y)==0);x.value=Double.NaN;check(!x.equals_value(x) && x.compare_value(y)==null)
  val d=ComparisonDerived();val e=ComparisonDerived();d.base0.category=1u;e.sequence=9u;check(d.compare_value(e)==1)
  val s=ComparisonSelected();val t=ComparisonSelected();s.base0.category=99u;s.unused=99u;check(s.equals_value(t));s.cache=1u;check(s.compare_value(t)==1)
  val g=ComparisonByte();val h=ComparisonByte();h.value=1u;check(g.compare_value(h)==-1);check(ComparisonEmpty().equals_value(ComparisonEmpty()))
}
''',
        "c": r'''#include "schema.h"
int main(void){
  comparison_key a={0},b={0};comparison_measurement x={0},y={0};comparison_derived d={0},e={0};comparison_selected s={0},t={0};comparison_byte g={0},h={0};comparison_empty z={0};
  if(comparison_key_init(&a)!=srl_ok || comparison_key_init(&b)!=srl_ok || comparison_derived_init(&d)!=srl_ok || comparison_derived_init(&e)!=srl_ok || comparison_selected_init(&s)!=srl_ok || comparison_selected_init(&t)!=srl_ok)return 20;
  a.cache=9;if(!comparison_key_equals_value(&a,&b))return 1;a.rank=UINT64_MAX;b.rank=1;if(comparison_key_compare_value(&a,&b)!=srl_greater)return 2;
  a.rank=b.rank=0;b.sequence=1;if(comparison_key_compare_value(&a,&b)!=srl_less)return 3;b.sequence=0;b.active=true;if(comparison_key_compare_value(&a,&b)!=srl_less)return 4;b.active=false;
  if(srl_string_set_cstr(&a.label,"\xF0\x90\x80\x80")!=srl_ok || srl_string_set_cstr(&b.label,"\xEE\x80\x80")!=srl_ok)return 21;
  if(comparison_key_compare_value(&a,&b)!=srl_greater){return 5;}
  srl_string_free(&a.label);srl_string_free(&b.label);
  b.stage=1;if(comparison_key_compare_value(&a,&b)!=srl_less)return 6;b.stage=0;b.atom.ordinal=1;if(comparison_key_compare_value(&a,&b)!=srl_less)return 7;
  x.value=-0.0;if(!comparison_measurement_equals_value(&x,&y) || comparison_measurement_compare_value(&x,&y)!=srl_equal)return 8;x.value=NAN;if(comparison_measurement_equals_value(&x,&x) || comparison_measurement_compare_value(&x,&y)!=srl_unordered)return 9;
  d.base0.category=1;e.sequence=9;if(comparison_derived_compare_value(&d,&e)!=srl_greater)return 10;s.base0.category=99;s.unused=99;if(!comparison_selected_equals_value(&s,&t))return 11;s.cache=1;if(comparison_selected_compare_value(&s,&t)!=srl_greater)return 12;
  h.value=1;if(comparison_byte_compare_value(&g,&h)!=srl_less)return 13;if(!comparison_empty_equals_value(&z,&z))return 14;
  comparison_key_free(&a);comparison_key_free(&b);comparison_derived_free(&d);comparison_derived_free(&e);comparison_selected_free(&s);comparison_selected_free(&t);return 0;
}
''',
    }
    source = sources["javascript" if language == "typescript" else language]
    if language == "typescript":
        source = source.replace("function check(condition)", "function check(condition: boolean)")
    return source


def main():
    """Generate each backend with independent class settings, then run strict SDK consumers."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/record-comparison-languages")
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
    owners = ("comparison_atom", "comparison_key", "comparison_measurement", "comparison_base",
              "comparison_derived", "comparison_selected", "comparison_generic", "comparison_empty")
    configuration = args.build / "comparison.ini"
    configuration.parent.mkdir(parents=True, exist_ok=True)
    text = "[cpp]\nformat = false\n"
    for language in targets:
        for owner in owners:
            text += f"\n[{language}.class.{owner}]\nequality = true\nordering = lexicographic\n"
            if owner == "comparison_selected":
                text += "fields = cache, sequence\n"
    configuration.write_text(text, encoding="utf-8")
    command = [args.compiler, "--input", ROOT / "test/resources/record_comparison_portable.serializer",
               "--config", configuration, "--language", ",".join(targets),
               "--go.package=main", "--kotlin.package="]
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
        examples.run(runner.build(language, "record_comparison", args.build / language, source))
        print(f"PASS: {language}, configured equality, lexicographic partial ordering, exact uint64, "
              "UTF-8, nested/base/generic values and explicit transient selection", flush=True)


if __name__ == "__main__":
    main()
