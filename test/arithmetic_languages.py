"""Generate and execute owning readonly arithmetic contracts in every ordinary output language."""

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
    """Require exact owning unit results, unchanged input, defaults, and checked overflow rejection."""
    sources = {
        "cpp": '''#include "message.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
static_assert((-distance{12}).value == -12);
int main(){const distance first{12},second{7};
if(first.negated().value!=-12 || first.added(second).value!=19 || first.baseline().value!=0 || first.value!=12 || duration{9}.negated().value!=-9)return 1;
bool rejected=false;try{(void)distance{std::numeric_limits<std::int64_t>::max()}.added(distance{1});}catch(const std::domain_error&){rejected=true;}
return rejected?0:2;}
''',
        "java": '''public final class Main {public static void main(String[] args){
var first=Schema.Distance.create(12);var second=Schema.Distance.create(7);
if(first.negated().value!=-12 || first.added(second).value!=19 || first.baseline().value!=0 || first.value!=12 || Schema.Duration.create(9).negated().value!=-9)throw new AssertionError();
boolean rejected=false;try{Schema.Distance.create(Long.MAX_VALUE).added(Schema.Distance.create(1));}catch(ArithmeticException failure){rejected=true;}if(!rejected)throw new AssertionError();}}
''',
        "javascript": '''import {Distance,Duration} from './schema.mjs';
const first=Distance.create(12n),second=Distance.create(7n);
if(first.negated().value!==-12n || first.added(second).value!==19n || first.baseline().value!==0n || first.value!==12n || Duration.create(9n).negated().value!==-9n)throw Error('owning arithmetic mismatch');
let rejected=false;try{Distance.create(9223372036854775807n).added(Distance.create(1n));}catch{rejected=true;}if(!rejected)throw Error('overflow accepted');
''',
        "go": '''package main
import "math"
func main(){first:=Distance_Create(12);second:=Distance_Create(7)
if first.Negated().Value != -12 || first.Added(second).Value != 19 || first.Baseline().Value != 0 || first.Value != 12 || Duration_Create(9).Negated().Value != -9{panic("owning arithmetic mismatch")}
rejected:=func()(failed bool){defer func(){if recover()!=nil{failed=true}}();Distance_Create(math.MaxInt64).Added(Distance_Create(1));return}();if !rejected{panic("overflow accepted")}}
''',
        "csharp": '''using System;using static SerializerGenerated.Schema;
internal static class Program{private static void Main(){var first=Distance.Create(12);var second=Distance.Create(7);
if(first.Negated().Value!=-12 || first.Added(second).Value!=19 || first.Baseline().Value!=0 || first.Value!=12 || Duration.Create(9).Negated().Value!=-9)throw new Exception("owning arithmetic mismatch");
bool rejected=false;try{Distance.Create(long.MaxValue).Added(Distance.Create(1));}catch(ArithmeticException){rejected=true;}if(!rejected)throw new Exception("overflow accepted");}}
''',
        "rust": '''mod schema;use schema::*;
fn main(){let first=Distance::create(12);let second=Distance::create(7);
assert_eq!(first.negated().value,-12);assert_eq!(first.added(second).value,19);assert_eq!(first.baseline().value,0);assert_eq!(first.value,12);assert_eq!(Duration::create(9).negated().value,-9);
assert!(std::panic::catch_unwind(||{Distance::create(i64::MAX).added(Distance::create(1));}).is_err());}
''',
        "python": '''from schema import Distance,Duration
first=Distance.create(12);second=Distance.create(7)
assert first.negated().value==-12 and first.added(second).value==19 and first.baseline().value==0 and first.value==12 and Duration.create(9).negated().value==-9
try:Distance.create(9223372036854775807).added(Distance.create(1))
except (ArithmeticError,ValueError):pass
else:raise AssertionError('overflow accepted')
''',
        "swift": '''import Foundation
let first=Distance.create(12),second=Distance.create(7)
let negated=try first.negated(),added=try first.added(second),baseline=try first.baseline(),duration=try Duration.create(9).negated()
precondition(negated.value == -12 && added.value == 19 && baseline.value == 0 && first.value == 12 && duration.value == -9)
var rejected=false;do{_=try Distance.create(Int64.max).added(Distance.create(1))}catch{rejected=true};precondition(rejected)
''',
        "kotlin": '''fun main(){val first=Distance.create(12);val second=Distance.create(7)
check(first.negated().value == -12L && first.added(second).value == 19L && first.baseline().value == 0L && first.value == 12L && Duration.create(9).negated().value == -9L)
check(runCatching{Distance.create(Long.MAX_VALUE).added(Distance.create(1))}.isFailure)}
''',
        "c": '''#include "schema.h"
int main(void){distance first={0},second={0},result={0};duration time={0};
if(distance_create(&first,12)!=srl_ok || distance_create(&second,7)!=srl_ok || duration_create(&time,9)!=srl_ok)return 1;
if(distance_negated(&first).value!=-12 || distance_added(&first,second).value!=19 || distance_baseline(&first).value!=0 || first.value!=12 || duration_negated(&time).value!=-9)return 2;
first.value=INT64_MAX;second.value=1;result.value=99;
if(distance_added_checked(&first,second,&result)!=srl_range || result.value!=99)return 3;
return 0;}
''',
    }
    return sources["javascript" if language == "typescript" else language]


def main():
    """Compile real generated sources through the maintained SDK runner; missing SDKs remain explicit."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/arithmetic-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language): args.build / language / examples.OUTPUTS[language]
               for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/arithmetic_portable.serializer",
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
        examples.run(runner.build(language, "arithmetic", args.build / language, source))
        print(f"PASS: {language}, owning results, unit identity, default construction and checked rejection", flush=True)


if __name__ == "__main__":
    main()
