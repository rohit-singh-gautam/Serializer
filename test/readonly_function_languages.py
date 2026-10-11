# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile declared readonly namespace and member calls with every selected real SDK."""
import argparse
import importlib.util
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)
FILES = {"cpp": "main.cpp", "java": "Main.java", "javascript": "main.mjs", "typescript": "main.mts",
         "go": "main.go", "csharp": "Program.cs", "rust": "main.rs", "python": "main.py",
         "swift": "main.swift", "kotlin": "Main.kt", "c": "main.c"}
OVERLOAD_LANGUAGES = {"cpp", "java", "csharp", "swift", "kotlin"}


def consumer(language):
    """Exercise forward binding, exact scalar returns, checked failures and lazy caller branches."""
    sources = {
        "cpp": r'''#include "message.hpp"
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <utility>
// Evaluate declared free and member calls through their ordinary generated C++ implementations.
constexpr bool qualify() {
  call_probe value{};
  return readonly_helpers::add(2.0,3.0)==5.0 && readonly_helpers::forwarded(41.0)==42.0 &&
      value.sum(2.0)==14.0 && value.composed(2.0)==15.0 && value.forwarded()==13.0 &&
      value.echoed(UINT64_MAX)==UINT64_MAX && value.matches(UINT64_MAX) && !value.matches(UINT64_MAX-1) &&
      value.divided(2.0)==6.0 && value.positive(2.0) && value.guarded(false,0.0);
}
static_assert(qualify());
static_assert(!noexcept(std::declval<const call_probe&>().positive(0.0)));
// Ordinary checked callees keep their runtime exception behavior through a predicate caller.
int main() {
  if(!qualify())return 1;
  call_probe value{};
  for(double invalid : {0.0,-0.0,std::numeric_limits<double>::infinity()}) {
    bool rejected=false;
    try { static_cast<void>(value.divided(invalid)); } catch(const std::domain_error&) { rejected=true; }
    if(!rejected)return 2;
  }
  bool rejected=false;
  try { static_cast<void>(value.positive(0.0)); } catch(const std::domain_error&) { rejected=true; }
  return rejected && value.guarded(false,std::numeric_limits<double>::quiet_NaN()) ? 0 : 3;
}
''',
        "java": r'''public final class Main {
  private static void check(boolean value) { if(!value)throw new AssertionError("Readonly call mismatch"); }
  public static void main(String[] args) {
    check(Schema.ReadonlyHelpers.add(2.0,3.0)==5.0 && Schema.ReadonlyHelpers.forwarded(41.0)==42.0);
    check(Schema.ReadonlyHelpers.identity(-1L)==-1L && Schema.ReadonlyHelpers.equal(-1L,-1L));
    var value=new Schema.CallProbe();
    check(value.sum(2.0)==14.0 && value.composed(2.0)==15.0 && value.forwarded()==13.0);
    check(value.echoed(-1L)==-1L && value.matches(-1L) && !value.matches(-2L));
    check(value.divided(2.0)==6.0 && value.positive(2.0) && value.guarded(false,0.0));
    for(double invalid:new double[]{0.0,-0.0,Double.POSITIVE_INFINITY}) {
      boolean rejected=false;
      try { value.divided(invalid); } catch(ArithmeticException exception) { rejected=true; }
      check(rejected);
    }
    check(!value.positive(0.0) && value.guarded(false,Double.NaN));
  }
}
''',
        "javascript": r'''import {CallProbe} from './schema.mjs';
const check=value=>{if(!value)throw Error('Readonly call mismatch')};
const value=new CallProbe();
check(value.sum(2)==14 && value.composed(2)==15 && value.forwarded()==13);
check(value.echoed(18446744073709551615n)==18446744073709551615n && value.matches(18446744073709551615n) && !value.matches(18446744073709551614n));
check(value.divided(2)==6 && value.positive(2) && value.guarded(false,0));
for(const invalid of [0,-0,Infinity]){let rejected=false;try{value.divided(invalid)}catch(error){rejected=error instanceof Error}check(rejected)}
check(!value.positive(0) && value.guarded(false,NaN));
''',
        "go": r'''package main
import "math"
func check(value bool){if !value{panic("Readonly call mismatch")}}
func rejected(value *CallProbe,divisor float64)(result bool){defer func(){result=recover()!=nil}();value.Divided(divisor);return false}
func main(){
  value:=NewCallProbe();check(value.Sum(2)==14 && value.Composed(2)==15 && value.Forwarded()==13)
  check(value.Echoed(math.MaxUint64)==math.MaxUint64 && value.Matches(math.MaxUint64) && !value.Matches(math.MaxUint64-1))
  check(value.Divided(2)==6 && value.Positive(2) && value.Guarded(false,0))
  check(rejected(value,0) && rejected(value,math.Copysign(0,-1)) && rejected(value,math.Inf(1)))
  check(!value.Positive(0) && value.Guarded(false,math.NaN()))
}
''',
        "csharp": r'''using System;
using static SerializerGenerated.Schema;
internal static class Program {
  private static void Check(bool value){if(!value)throw new Exception("Readonly call mismatch");}
  private static void Main(){
    var value=new CallProbe();Check(value.Sum(2)==14 && value.Composed(2)==15 && value.Forwarded()==13);
    Check(value.Echoed(ulong.MaxValue)==ulong.MaxValue && value.Matches(ulong.MaxValue) && !value.Matches(ulong.MaxValue-1));
    Check(value.Divided(2)==6 && value.Positive(2) && value.Guarded(false,0));
    foreach(var invalid in new[]{0.0,-0.0,double.PositiveInfinity}){
      var rejected=false;try{value.Divided(invalid);}catch(ArithmeticException){rejected=true;}Check(rejected);
    }
    Check(!value.Positive(0) && value.Guarded(false,double.NaN));
  }
}
''',
        "rust": r'''mod schema;
use schema::*;
fn main(){
  let value=CallProbe::default();assert!(value.sum(2.0)==14.0 && value.composed(2.0)==15.0 && value.forwarded()==13.0);
  assert!(value.echoed(u64::MAX)==u64::MAX && value.matches(u64::MAX) && !value.matches(u64::MAX-1));
  assert!(value.divided(2.0)==6.0 && value.positive(2.0) && value.guarded(false,0.0));
  for invalid in [0.0,-0.0,f64::INFINITY]{assert!(std::panic::catch_unwind(||value.divided(invalid)).is_err());}
  assert!(std::panic::catch_unwind(||value.positive(0.0)).is_err());
  assert!(value.guarded(false,f64::NAN));
}
''',
        "python": r'''from schema import CallProbe
value=CallProbe()
assert value.sum(2)==14 and value.composed(2)==15 and value.forwarded()==13
assert value.echoed(18446744073709551615)==18446744073709551615 and value.matches(18446744073709551615) and not value.matches(18446744073709551614)
assert value.divided(2)==6 and value.positive(2) and value.guarded(False,0)
for invalid in (0,-0.0,float('inf')):
    rejected=False
    try:
        value.divided(invalid)
    except ValueError:
        rejected=True
    assert rejected
assert not value.positive(0) and value.guarded(False,float('nan'))
''',
        "swift": r'''import Foundation
let value=CallProbe()
let sum=try value.sum(2);let composed=try value.composed(2);let forwarded=try value.forwarded()
precondition(sum==14 && composed==15 && forwarded==13)
let echoed=try value.echoed(UInt64.max);let matches=try value.matches(UInt64.max);let different=try value.matches(UInt64.max-1)
precondition(echoed==UInt64.max && matches && !different)
let divided=try value.divided(2);precondition(divided==6 && value.positive(2) && value.guarded(false,0))
for invalid in [0.0,-0.0,Double.infinity]{var rejected=false;do{_=try value.divided(invalid)}catch{rejected=true};precondition(rejected)}
precondition(!value.positive(0) && value.guarded(false,Double.nan))
''',
        "kotlin": r'''fun main(){
  val value=CallProbe();check(value.sum(2.0)==14.0 && value.composed(2.0)==15.0 && value.forwarded()==13.0)
  check(value.echoed(ULong.MAX_VALUE)==ULong.MAX_VALUE && value.matches(ULong.MAX_VALUE) && !value.matches(ULong.MAX_VALUE-1uL))
  check(value.divided(2.0)==6.0 && value.positive(2.0) && value.guarded(false,0.0))
  for(invalid in listOf(0.0,-0.0,Double.POSITIVE_INFINITY)){check(runCatching{value.divided(invalid)}.isFailure)}
  check(!value.positive(0.0) && value.guarded(false,Double.NaN))
}
''',
        "c": r'''#include "schema.h"
#include <stdbool.h>
#include <stdint.h>
int main(void){
  call_probe value={0};call_probe_init(&value);
  if(call_probe_sum(&value,2)!=14 || call_probe_composed(&value,2)!=15 || call_probe_forwarded(&value)!=13)return 1;
  if(call_probe_echoed(&value,UINT64_MAX)!=UINT64_MAX || !call_probe_matches(&value,UINT64_MAX) || call_probe_matches(&value,UINT64_MAX-1))return 2;
  if(call_probe_divided(&value,2)!=6 || !call_probe_positive(&value,2) || !call_probe_guarded(&value,false,0))return 3;
  double unchanged=17;
  if(call_probe_divided_checked(&value,0,&unchanged)!=srl_range || unchanged!=17 || call_probe_divided_checked(&value,-0.0,&unchanged)!=srl_range || unchanged!=17)return 4;
  if(call_probe_positive(&value,0))return 5;
  call_probe_free(&value);return 0;
}
''',
    }
    if language == "typescript":
        return sources["javascript"].replace("const check=value=>", "const check=(value: boolean)=>")
    return sources[language]


def overload_consumer(language):
    """Require the selected SDK to distinguish numeric overload signatures and exact scalar returns."""
    sources = {
        "cpp": r'''#include "message.hpp"
#include <cstdint>
// Exercise native overload resolution and the schema-bound calls during constant evaluation.
constexpr bool qualify() {
  overload_probe value{};
  return value.scalar()==13.0 && value.exact()==UINT64_MAX &&
      overloaded_helpers::identity(41.0)==42.0 && overloaded_helpers::identity(UINT64_MAX)==UINT64_MAX;
}
static_assert(qualify());
// Repeat the same actual generated methods at runtime.
int main(){return qualify()?0:1;}
''',
        "java": r'''public final class Main {
  public static void main(String[] args) {
    var value=new Schema.OverloadProbe();
    if(value.scalar()!=13.0 || value.exact()!=-1L ||
       Schema.OverloadedHelpers.identity(41.0)!=42.0 || Schema.OverloadedHelpers.identity(-1L)!=-1L)
      throw new AssertionError("Readonly overload mismatch");
  }
}
''',
        "csharp": r'''using System;
using static SerializerGenerated.Schema;
internal static class Program {
  private static void Main(){
    var value=new OverloadProbe();
    if(value.Scalar()!=13.0 || value.Exact()!=ulong.MaxValue ||
       OverloadedHelpersIdentity(41.0)!=42.0 || OverloadedHelpersIdentity(ulong.MaxValue)!=ulong.MaxValue)
      throw new Exception("Readonly overload mismatch");
  }
}
''',
        "swift": r'''let value=OverloadProbe()
let scalar=try value.scalar()
let exact=try value.exact()
precondition(scalar==13.0 && exact==UInt64.max)
''',
        "kotlin": r'''fun main(){
  val value=OverloadProbe()
  check(value.scalar()==13.0 && value.exact()==ULong.MAX_VALUE)
}
''',
    }
    return sources[language]


def check_overload_rejection(args, language, fixture, directory):
    """Reject unsupported or erased overloads with the actual backend diagnostic before SDK compilation."""
    target = "js" if language == "javascript" else language
    directory.mkdir(parents=True, exist_ok=True)
    command = [args.compiler, "--input", ROOT / "test/resources" / fixture, "--language", target,
               "--output", directory / examples.OUTPUTS[language], "--cpp.format=false", "--go.package=main",
               "--kotlin.package="]
    result = subprocess.run([str(value) for value in command], capture_output=True, text=True,
                            encoding="utf-8", timeout=60, creationflags=examples.PROCESS_CREATION_FLAGS)
    diagnostics = ("standalone overload/name erasure collision", "Generated type name collision",
                   "C symbol collision")
    if result.returncode == 0 or not any(message in result.stderr for message in diagnostics):
        raise AssertionError(f"{language}: expected an explicit overload diagnostic, got {result.stdout}{result.stderr}")
    print(f"PASS: {language}, unsupported or erased standalone overload rejected", flush=True)


def run_overloads(args, languages, runner):
    """Compile distinguishable overloads on capable SDKs and check explicit rejection elsewhere."""
    for language in languages:
        directory = args.build / "overloads" / language
        if language not in OVERLOAD_LANGUAGES:
            check_overload_rejection(args, language, "readonly_function_overloads.serializer", directory)
            continue
        directory.mkdir(parents=True, exist_ok=True)
        examples.run([args.compiler, "--input", ROOT / "test/resources/readonly_function_overloads.serializer",
                      "--language", language, "--output", directory / examples.OUTPUTS[language],
                      "--cpp.format=false", "--kotlin.package="])
        source = args.build / "consumers/overloads" / language
        source.mkdir(parents=True, exist_ok=True)
        (source / FILES[language]).write_text(overload_consumer(language), encoding="utf-8")
        examples.run(runner.build(language, "readonly_function_overloads", directory, source))
        print(f"PASS: {language}, readonly namespace overload dispatch and exact uint64 return", flush=True)
    if "java" in languages:
        check_overload_rejection(args, "java", "readonly_function_erasure.serializer",
                                 args.build / "erased-overloads/java")


def run_evaluation_once(args, languages, runner):
    """Use native counters to verify reached calls run once and finite inspection retains lazy branches."""
    sources = {
        "python": '''import schema
probe = schema.ObservationProbe()
assert probe.valid() and schema.observation_count == 1
schema.observation_value = float('nan')
assert not probe.valid() and schema.observation_count == 2
assert probe.guarded(True) and schema.observation_count == 2
assert not probe.guarded(False) and schema.observation_count == 3
''',
        "go": '''package main
import "math"
var observationCount int
var observationValue float64 = 1
func main() {
  probe := NewObservationProbe()
  if !probe.Valid() || observationCount != 1 { panic("Call was not observed once") }
  observationValue = math.NaN()
  if probe.Valid() || observationCount != 2 { panic("Nonfinite call was not observed once") }
  if !probe.Guarded(true) || observationCount != 2 { panic("Skipped call was reached") }
  if probe.Guarded(false) || observationCount != 3 { panic("Reached call was repeated") }
}
''',
    }
    for language in languages:
        if language not in sources:
            continue
        directory = args.build / "evaluation-once" / language
        directory.mkdir(parents=True, exist_ok=True)
        examples.run([args.compiler, "--input", ROOT / "test/resources/readonly_function_observation.serializer",
                      "--language", language, "--output", directory / examples.OUTPUTS[language],
                      "--go.package=main", "--go.naming=preserve"])
        source = args.build / "consumers/evaluation-once" / language
        source.mkdir(parents=True, exist_ok=True)
        (source / FILES[language]).write_text(sources[language], encoding="utf-8")
        examples.run(runner.build(language, "readonly_function_observation", directory, source))
        print(f"PASS: {language}, reached readonly call evaluated once, NaN observed and skipped branch remains lazy", flush=True)


def run_native_qualifiers(args, languages, runner):
    """Compile application types whose copies or equality can throw against generated C++ contracts."""
    if "cpp" not in languages:
        return
    directory = args.build / "native-types" / "cpp"
    directory.mkdir(parents=True, exist_ok=True)
    examples.run([args.compiler, "--input", ROOT / "test/resources/readonly_function_native_types.serializer",
                  "--language", "cpp", "--output", directory / examples.OUTPUTS["cpp"], "--cpp.format=false"])
    source = args.build / "consumers/native-types" / "cpp"
    source.mkdir(parents=True, exist_ok=True)
    (source / FILES["cpp"]).write_text(r'''#include "message.hpp"
#include <stdexcept>
#include <utility>
// An application-owned comparable determines its own exception contract.
struct throwing_value {
  bool operator==(const throwing_value&) const { throw std::runtime_error{"Comparison failure"}; }
};
using comparable_probe = generic_probe<throwing_value>;
static_assert(!noexcept(std::declval<const comparable_probe&>().expression_equal({}, {})));
static_assert(!noexcept(std::declval<const comparable_probe&>().predicate_equal({}, {})));
static_assert(!noexcept(std::declval<const string_probe&>().get()));
static_assert(!noexcept(std::declval<const record_probe&>().get()));
// Both portable body forms preserve a throwing native comparator rather than terminating.
int main() {
  comparable_probe probe{};
  bool expression_rejected = false, predicate_rejected = false;
  try { static_cast<void>(probe.expression_equal({}, {})); } catch (const std::runtime_error&) { expression_rejected = true; }
  try { static_cast<void>(probe.predicate_equal({}, {})); } catch (const std::runtime_error&) { predicate_rejected = true; }
  string_probe text{}; text.text = "owned";
  record_probe record{}; record.value.text = "copied";
  return expression_rejected && predicate_rejected && text.get() == "owned" && record.get().text == "copied" ? 0 : 1;
}
''', encoding="utf-8")
    examples.run(runner.build("cpp", "readonly_function_native_types", directory, source))
    print("PASS: cpp, throwing generic equality and owned string/record getter exception contracts", flush=True)


def main():
    """Require strict selected SDK builds and overload compatibility without silently skipping a backend."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/readonly-function-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    parser.add_argument("--overloads-only", action="store_true")
    parser.add_argument("--evaluation-only", action="store_true")
    parser.add_argument("--native-types-only", action="store_true")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    args.build = args.build.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    if not languages or len(languages) != len(set(languages)) or any(language not in FILES for language in languages):
        parser.error("Choose distinct supported language folder names")
    runner = examples.Runner(args)
    if args.evaluation_only:
        run_evaluation_once(args, languages, runner)
        return
    if args.native_types_only:
        run_native_qualifiers(args, languages, runner)
        return
    if args.overloads_only:
        run_overloads(args, languages, runner)
        return
    targets = {("js" if language == "javascript" else language):
               args.build / language / examples.OUTPUTS[language] for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/readonly_function_calls.serializer",
               "--language", ",".join(targets), "--cpp.format=false", "--go.package=main", "--kotlin.package="]
    for language, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        command += [f"--{language}.output", destination]
    examples.run(command)
    if "typescript" in languages and "javascript" in languages:
        (args.build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    for language in languages:
        source = args.build / "consumers" / language
        source.mkdir(parents=True, exist_ok=True)
        (source / FILES[language]).write_text(consumer(language), encoding="utf-8")
        examples.run(runner.build(language, "readonly_function_calls", args.build / language, source))
        print(f"PASS: {language}, readonly namespace/member calls, forward binding, exact uint64 returns and checked failures", flush=True)
    run_overloads(args, languages, runner)
    run_evaluation_once(args, languages, runner)
    run_native_qualifiers(args, languages, runner)


if __name__ == "__main__":
    main()
