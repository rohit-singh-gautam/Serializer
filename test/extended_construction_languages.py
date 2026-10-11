"""Compile and run fixed-array bindings and fresh default overloads in every native SDK."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("construction_examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)
FILES = {"cpp": "main.cpp", "java": "Main.java", "javascript": "main.mjs", "typescript": "main.mts",
         "go": "main.go", "csharp": "Program.cs", "rust": "main.rs", "python": "main.py",
         "swift": "main.swift", "kotlin": "Main.kt", "c": "main.c"}


def consumer(language):
    """Exercise real generated defaults, cardinality, and independently owned record values."""
    sources = {
        "cpp": '''#include "message.hpp"
#include <array>
int main() {
  static_assert(bound_defaults::make().value == 5);
  auto pair = bound_pair::from_values(std::array<double, 2>{3, 4});
  auto first = bound_record_default::make_default_0();
  auto second = bound_record_default::make_default_0();
  first.value.x = 9;
  return pair.values[1] != 4 || second.value.x != 2 || bound_defaults{}.value != 1;
}
''',
        "java": '''public final class Main {
  public static void main(String[] args) {
    var pair = Schema.BoundPair.fromValues(new double[]{3, 4});
    var first = Schema.BoundRecordDefault.make_default_0();
    var second = Schema.BoundRecordDefault.make_default_0();
    first.value.x = 9;
    if (pair.values[1] != 4 || second.value.x != 2 || Schema.BoundDefaults.make_default_0().value != 5) throw new AssertionError();
    try { Schema.BoundPair.fromValues(new double[]{1}); throw new AssertionError(); } catch (IllegalArgumentException expected) { }
  }
}
''',
        "javascript": '''import {BoundPair, BoundDefaults, BoundRecordDefault} from './schema.mjs';
const pair = BoundPair.fromValues([3, 4]);
const first = BoundRecordDefault.make_default_0(), second = BoundRecordDefault.make_default_0();
first.value.x = 9;
if (pair.values[1] !== 4 || second.value.x !== 2 || BoundDefaults.make_default_0().value !== 5) throw Error('Construction mismatch');
let failed = false; try { BoundPair.fromValues([1]); } catch { failed = true; }
if (!failed) throw Error('Missing cardinality check');
''',
        "go": '''package main
func main() {
  pair := BoundPair_FromValues([]float64{3, 4})
  first, second := BoundRecordDefault_Make_default_0(), BoundRecordDefault_Make_default_0()
  first.Value.X = 9
  if pair.Values[1] != 4 || second.Value.X != 2 || BoundDefaults_Make_default_0().Value != 5 { panic("Construction mismatch") }
}
''',
        "csharp": '''using System;
using System.Collections.Generic;
using static SerializerGenerated.Schema;
internal static class Program {
  private static void Main() {
    var pair = BoundPair.FromValues(new List<double>{3, 4});
    var first = BoundRecordDefault.Make_default_0(); var second = BoundRecordDefault.Make_default_0();
    first.Value.X = 9;
    if (pair.Values[1] != 4 || second.Value.X != 2 || BoundDefaults.Make_default_0().Value != 5) throw new Exception("Construction mismatch");
    try { BoundPair.FromValues(new List<double>{1}); throw new Exception("Missing cardinality check"); } catch (ArgumentException) { }
  }
}
''',
        "rust": '''mod schema;
use schema::*;
fn main() {
  let pair = BoundPair::from_values(vec![3.0, 4.0]);
  let mut first = BoundRecordDefault::make_default_0(); let second = BoundRecordDefault::make_default_0();
  first.value.x = 9.0;
  assert_eq!(first.value.x, 9.0); assert_eq!(second.value.x, 2.0);
  assert_eq!(pair.values[1], 4.0); assert_eq!(BoundDefaults::make_default_0().value, 5);
}
''',
        "python": '''from schema import BoundPair, BoundDefaults, BoundRecordDefault
pair = BoundPair.from_values([3, 4])
first, second = BoundRecordDefault.make_default_0(), BoundRecordDefault.make_default_0()
first.value.x = 9
assert pair.values[1] == 4 and second.value.x == 2 and BoundDefaults.make_default_0().value == 5
try:
    BoundPair.from_values([1])
except ValueError:
    pass
else:
    raise AssertionError('Missing cardinality check')
''',
        "swift": '''import Foundation
let pair = BoundPair.fromValues([3, 4])
var first = BoundRecordDefault.make_default_0(); let second = BoundRecordDefault.make_default_0()
first.value.x = 9
precondition(first.value.x == 9 && second.value.x == 2 && pair.values[1] == 4 && BoundDefaults.make_default_0().value == 5)
''',
        "kotlin": '''fun main() {
  val pair = BoundPair.fromValues(doubleArrayOf(3.0, 4.0))
  val first = BoundRecordDefault.make_default_0(); val second = BoundRecordDefault.make_default_0()
  first.value.x = 9.0
  check(pair.values[1] == 4.0 && second.value.x == 2.0 && BoundDefaults.make_default_0().value == 5u)
  try { BoundPair.fromValues(doubleArrayOf(1.0)); error("Missing cardinality check") } catch (_: IllegalArgumentException) { }
}
''',
        "c": '''#include "schema.h"
#include <stdlib.h>
int main(void) {
  bound_pair pair = {0}; bound_defaults defaults = {0}; bound_record_default first = {0}, second = {0};
  bound_pair_values_array input = {0}; input.size = input.capacity = 2;
  input.data = (double*)malloc(input.size * sizeof(*input.data)); if (!input.data) return 1;
  input.data[0] = 3; input.data[1] = 4;
  if (bound_pair_from_values(&pair, &input) != srl_ok || input.size != 0 || pair.values.data[1] != 4) return 2;
  bound_pair_values_array invalid = {0}; invalid.size = invalid.capacity = 1;
  invalid.data = (double*)malloc(sizeof(*invalid.data)); if (!invalid.data) return 6;
  invalid.data[0] = 8;
  if (bound_pair_from_values(&pair, &invalid) != srl_invalid || invalid.size != 1 || invalid.data[0] != 8 || pair.values.data[1] != 4) return 7;
  bound_pair_values_array_clear(&invalid);
  if (bound_defaults_make_default_0(&defaults) != srl_ok || defaults.value != 5) return 3;
  if (bound_record_default_make_default_0(&first) != srl_ok || bound_record_default_make_default_0(&second) != srl_ok) return 4;
  first.value.x = 9; if (second.value.x != 2) return 5;
  bound_pair_free(&pair); bound_defaults_free(&defaults); bound_record_default_free(&first); bound_record_default_free(&second);
  return 0;
}
''',
    }
    return sources["javascript" if language == "typescript" else language]


def main():
    """Regenerate each selected language, build its SDK consumer, and execute it."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/extended-construction-languages")
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
    command = [args.compiler, "--input", ROOT / "test/resources/extended_construction_portable.serializer",
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
        examples.run(runner.build(language, "extended-construction", args.build / language, source))
        print(f"PASS: {language}, fixed-array bindings and fresh default overloads", flush=True)


if __name__ == "__main__":
    main()
