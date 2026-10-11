"""Build exact projected record calls and lazy record selections in every ordinary SDK."""
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
    """Require the selected branch's value and guard an invalid unselected collection projection."""
    sources = {
        "cpp": '''#include "message.hpp"
#include <limits>
#include <stdexcept>
int main(){record_probe value{};value.preferred.value=std::numeric_limits<std::uint64_t>::max();value.fallback.value=7;
if(value.selected().value!=value.preferred.value || !value.matches(value.preferred.value) || value.guarded(true).value!=value.preferred.value || !projected_valid(value.preferred))return 1;
bool failed=false;try{static_cast<void>(value.guarded(false));}catch(const std::domain_error&){failed=true;}if(!failed)return 2;
value.preferred.value=0;if(value.selected().value!=7)return 3;value.child.value=9;if(!value.inherited())return 4;
value.items.push_back({9});if(!value.items_valid())return 5;value.items.push_back({0});if(value.items_valid())return 6;}
''',
        "java": '''public final class Main { public static void main(String[] args){var value=new Schema.RecordProbe();value.preferred.value=-1L;value.fallback.value=7;
if(value.selected().value!=-1L || !value.matches(-1L) || value.guarded(true).value!=-1L || !Schema.projectedValid(value.preferred))throw new AssertionError();
boolean failed=false;try{value.guarded(false);}catch(ArithmeticException error){failed=true;}if(!failed)throw new AssertionError();
value.preferred.value=0;if(value.selected().value!=7)throw new AssertionError();value.child.base0.value=9;if(!value.inherited())throw new AssertionError();
var item=new Schema.RecordLeaf();item.value=9;value.items=new Schema.RecordLeaf[]{item};if(!value.itemsValid())throw new AssertionError();item.value=0;if(value.itemsValid())throw new AssertionError();}}
''',
        "javascript": '''import {RecordProbe,RecordLeaf,ProjectedValid} from './schema.mjs';
const value=new RecordProbe();value.preferred.value=18446744073709551615n;value.fallback.value=7n;
if(value.selected().value!==value.preferred.value || !value.matches(value.preferred.value) || value.guarded(true).value!==value.preferred.value || !ProjectedValid(value.preferred))throw Error('Selected record mismatch');
let failed=false;try{value.guarded(false);}catch(error){failed=error instanceof RangeError;}if(!failed)throw Error('Reached invalid index accepted');
value.preferred.value=0n;if(value.selected().value!==7n)throw Error('Fallback mismatch');value.child.base0.value=9n;if(!value.inherited())throw Error('Inherited call mismatch');
const item=new RecordLeaf();item.value=9n;value.items=[item];if(!value.itemsValid())throw Error('Reduction mismatch');item.value=0n;if(value.itemsValid())throw Error('Invalid item accepted');
''',
        "go": '''package main
import "math"
func main(){value:=NewRecordProbe();value.Preferred.Value=math.MaxUint64;value.Fallback.Value=7;
if value.Selected().Value!=math.MaxUint64 || !value.Matches(math.MaxUint64) || value.Guarded(true).Value!=math.MaxUint64 || !ProjectedValid(value.Preferred){panic("Selected record mismatch")}
failed:=false;func(){defer func(){if recover()!=nil{failed=true}}();value.Guarded(false)}();if !failed{panic("Invalid index accepted")}
value.Preferred.Value=0;if value.Selected().Value!=7{panic("Fallback mismatch")};value.Child.Base0.Value=9;if !value.Inherited(){panic("Inherited mismatch")}
item:=NewRecordLeaf();item.Value=9;value.Items=[]*RecordLeaf{item};if !value.ItemsValid(){panic("Reduction mismatch")};item.Value=0;if value.ItemsValid(){panic("Invalid item accepted")}}
''',
        "csharp": '''using System;
using static SerializerGenerated.Schema;
internal static class Program {private static void Main(){var value=new RecordProbe();value.Preferred.Value=ulong.MaxValue;value.Fallback.Value=7;
if(value.Selected().Value!=ulong.MaxValue || !value.Matches(ulong.MaxValue) || value.Guarded(true).Value!=ulong.MaxValue || !ProjectedValid(value.Preferred))throw new Exception("Selected record mismatch");
bool failed=false;try{value.Guarded(false);}catch(ArithmeticException){failed=true;}if(!failed)throw new Exception("Invalid index accepted");
value.Preferred.Value=0;if(value.Selected().Value!=7)throw new Exception("Fallback mismatch");value.Child.Base0.Value=9;if(!value.Inherited())throw new Exception("Inherited mismatch");
var item=new RecordLeaf{Value=9};value.Items.Add(item);if(!value.ItemsValid())throw new Exception("Reduction mismatch");item.Value=0;if(value.ItemsValid())throw new Exception("Invalid item accepted");}}
''',
        "rust": '''mod schema;
use schema::*;
fn main(){let mut value=RecordProbe::default();value.preferred.value=u64::MAX;value.fallback.value=7;
assert_eq!(value.selected().value,u64::MAX);assert!(value.matches(u64::MAX));assert_eq!(value.guarded(true).value,u64::MAX);assert!(projected_valid(value.preferred.clone()));
assert!(std::panic::catch_unwind(|| value.guarded(false)).is_err());value.preferred.value=0;assert_eq!(value.selected().value,7);value.child.base0.value=9;assert!(value.inherited());
value.items.push(RecordLeaf{value:9});assert!(value.items_valid());value.items.push(RecordLeaf{value:0});assert!(!value.items_valid());}
''',
        "python": '''from schema import RecordProbe, RecordLeaf, projected_valid
value=RecordProbe();value.preferred.value=18446744073709551615;value.fallback.value=7
assert value.selected().value==value.preferred.value and value.matches(value.preferred.value)
assert value.guarded(True).value==value.preferred.value and projected_valid(value.preferred)
try:
    value.guarded(False)
except ValueError:
    pass
else:
    raise AssertionError('Invalid index accepted')
value.preferred.value=0;assert value.selected().value==7
value.child.base0.value=9;assert value.inherited()
item=RecordLeaf();item.value=9;value.items=[item];assert value.items_valid()
item.value=0;assert not value.items_valid()
''',
        "swift": '''var value=RecordProbe();value.preferred.value=UInt64.max;value.fallback.value=7;
guard try value.selected().value==UInt64.max, try value.matches(UInt64.max), try value.guarded(true).value==UInt64.max, try projectedValid(value.preferred) else {fatalError("Selected record mismatch")}
var failed=false;do{_ = try value.guarded(false)}catch{failed=true};if !failed{fatalError("Invalid index accepted")}
value.preferred.value=0;guard try value.selected().value==7 else{fatalError("Fallback mismatch")};value.child.base0.value=9;guard try value.inherited() else{fatalError("Inherited mismatch")}
var item=RecordLeaf();item.value=9;value.items=[item];if !value.itemsValid(){fatalError("Reduction mismatch")};item.value=0;value.items=[item];if value.itemsValid(){fatalError("Invalid item accepted")}
''',
        "kotlin": '''fun main(){val value=RecordProbe();value.preferred.value=ULong.MAX_VALUE;value.fallback.value=7uL;
check(value.selected().value==ULong.MAX_VALUE && value.matches(ULong.MAX_VALUE) && value.guarded(true).value==ULong.MAX_VALUE && projectedValid(value.preferred))
var failed=false;try{value.guarded(false)}catch(error:SerializerException){failed=true};check(failed)
value.preferred.value=0uL;check(value.selected().value==7uL);value.child.base0.value=9uL;check(value.inherited())
val item=RecordLeaf();item.value=9uL;value.items.add(item);check(value.itemsValid());item.value=0uL;check(!value.itemsValid())}
''',
        "c": '''#include "schema.h"
#include <stdlib.h>
int main(void){record_probe value={0};if(record_probe_init(&value)!=srl_ok)return 1;value.preferred.value=UINT64_MAX;value.fallback.value=7;
if(record_probe_selected(&value).value!=UINT64_MAX || !record_probe_matches(&value,UINT64_MAX) || record_probe_guarded(&value,true).value!=UINT64_MAX || !projected_valid(value.preferred))return 2;
record_leaf result={0};if(record_probe_guarded_checked(&value,false,&result)!=srl_range)return 3;
value.preferred.value=0;if(record_probe_selected(&value).value!=7)return 4;value.child.base0.value=9;if(!record_probe_inherited(&value))return 5;
value.items.data=malloc(sizeof(*value.items.data));if(!value.items.data)return 6;value.items.size=1;value.items.capacity=1;value.items.data[0]=(record_leaf){9};
if(!record_probe_items_valid(&value)){return 7;}value.items.data[0].value=0;if(record_probe_items_valid(&value)){return 8;}record_probe_free(&value);return 0;}
'''
    }
    if language == "typescript":
        return sources["javascript"]
    return sources[language]


def main():
    """Generate one schema then require each explicitly selected SDK to compile and execute it."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/projected-records-languages")
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
    command = [args.compiler, "--input", ROOT / "test/resources/projected_records.serializer", "--language", ",".join(targets),
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
        examples.run(runner.build(language, "projected_records", args.build / language, source))
        print(f"PASS: {language}, exact record selection, projected readonly calls and unselected-branch laziness", flush=True)


if __name__ == "__main__":
    main()
