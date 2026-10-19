"""Compile scoped readonly translators against every real generated language SDK."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)

CONSUMERS = {
    "cpp": ("main.cpp", r'''
#include "message.hpp"
#include <rohit/serializer.hpp>
#include <cassert>
#include <memory>
#include <string>
#include <type_traits>
// Internal source and default storage must remain behind the readonly getter API.
template <typename View>
concept exposes_source = requires(View& view) { view.srl_source; };
template <typename View>
concept exposes_defaults = requires(View& view) { view.srl_default_defaulted; };
// Encode either owning values or borrowed translator views through the same public API.
template <template <rohit::serializer::serialize_type> class Protocol>
std::string encoded(const auto& value) {
  rohit::full_stream_auto_alloc output;
  value.template serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}
// Compare projected encoding with owning conversion and verify exact native field identity.
template <template <rohit::serializer::serialize_type> class Protocol>
void verify(translator_source& source) {
  const auto expected = encoded<Protocol>(source.materialize());
  source.with_project([&](const auto& view) {
    assert(&view.get_label() == &source.details.label);
    assert(&view.get_child() == &source.details);
    assert(view.get_number() == 41 && view.get_defaulted() == 7 && view.get_cached() == 9);
    assert(encoded<Protocol>(view) == expected);
  });
  auto document = std::make_shared<translator_document>();
  document->model = source;
  translator_source_reference selected;
  selected.bind(document, "translator-document", 41, 1);
  selected.with_value([&](const auto& borrowed_source) {
    assert(&borrowed_source == &document->model);
    borrowed_source.with_project([&](const auto& view) {
      assert(&view.get_child() == &document->model.details);
      assert(encoded<Protocol>(view) == expected);
    });
  });
}
// Exercise all native protocols and fresh scoped borrows after source mutation.
int main() {
  translator_source source{}; source.number = 41; source.details.label = "original";
  using projected_type = decltype(source.project());
  static_assert(!exposes_source<projected_type> && !exposes_defaults<projected_type>);
  static_assert(!std::is_convertible_v<projected_type*, translator_base*>);
  verify<rohit::serializer::binary_none>(source);
  verify<rohit::serializer::binary_integer>(source);
  verify<rohit::serializer::binary_string>(source);
  verify<rohit::serializer::json>(source);
  const auto view = source.project(); source.details.label = "updated";
  assert(view.get_label() == "updated");
  assert(source.materialize().label == "updated");
  translator_short generic{}; generic.value = 17;
  generic.with_project_value([](const auto& projected) {
    assert(projected.get_value() == 17 && projected.get_generation() == 2);
    const auto bytes = encoded<rohit::serializer::binary_none>(projected);
    assert(bytes.size() == 6 && bytes[0] == 17 && bytes[2] == 2);
  });
  return 0;
}
'''),
    "python": ("main.py", r'''
from schema import TranslatorSource, TranslatorShort, Protocol
source = TranslatorSource()
source.number = 41
source.details.label = "original"
for protocol in Protocol:
    expected = source.materialize().encode(protocol)
    def verify(view):
        assert view.get_number() == 41 and view.get_defaulted() == 7 and view.get_cached() == 9
        assert view.get_child() is source.details
        assert view.encode(protocol) == expected
    source.with_project(verify)
source.details.label = "updated"
source.with_project(lambda view: (_ for _ in ()).throw(AssertionError()) if view.get_label() != "updated" else None)
generic = TranslatorShort()
generic.value = 17
generic.with_project_value(lambda view: None if view.get_value() == 17 and view.get_generation() == 2 and view.encode(Protocol.BINARY_NONE) == bytes([17, 0, 2, 0, 0, 0]) else (_ for _ in ()).throw(AssertionError()))
'''),
    "javascript": ("main.mjs", r'''
import { TranslatorSource, TranslatorShort, Protocol } from './schema.mjs';
const source = new TranslatorSource(); source.number = 41; source.details.label = 'original';
for (const protocol of Object.values(Protocol)) {
  const expected = source.materialize().encode(protocol);
  source.withProject(view => {
    if (view.getNumber() !== 41 || view.getDefaulted() !== 7 || view.getCached() !== 9 || view.getChild() !== source.details) throw Error('Borrow');
    if (String(view.encode(protocol)) !== String(expected)) throw Error('Wire');
  });
}
source.details.label = 'updated';
source.withProject(view => { if (view.getLabel() !== 'updated') throw Error('Stale'); });
const generic = new TranslatorShort(); generic.value = 17;
generic.withProjectValue(view => { if (view.getValue() !== 17 || view.getGeneration() !== 2 || String(view.encode(Protocol.BINARY_NONE)) !== '17,0,2,0,0,0') throw Error('Generic'); });
'''),
    "typescript": ("main.mts", r'''
import { TranslatorSource, TranslatorShort, Protocol } from './schema.mjs';
const source = new TranslatorSource(); source.number = 41; source.details.label = 'original';
for (const protocol of Object.values(Protocol)) {
  const expected = source.materialize().encode(protocol);
  source.withProject(view => {
    if (view.getNumber() !== 41 || view.getDefaulted() !== 7 || view.getCached() !== 9 || view.getChild() !== source.details) throw Error('Borrow');
    if (String(view.encode(protocol)) !== String(expected)) throw Error('Wire');
  });
}
source.details.label = 'updated';
source.withProject(view => { if (view.getLabel() !== 'updated') throw Error('Stale'); });
const generic = new TranslatorShort(); generic.value = 17;
generic.withProjectValue(view => { if (view.getValue() !== 17 || view.getGeneration() !== 2 || String(view.encode(Protocol.BINARY_NONE)) !== '17,0,2,0,0,0') throw Error('Generic'); });
'''),
    "java": ("Main.java", r'''
public final class Main {
  // Verify live source projection and exact ordinary target framing in every protocol.
  public static void main(String[] args) {
    var source = new Schema.TranslatorSource(); source.number = 41; source.details.label = "original";
    for (var protocol : Schema.Protocol.values()) {
      var expected = source.materialize().encode(protocol);
      source.withProject(view -> {
        if (view.getNumber() != 41 || view.getDefaulted() != 7 || view.getCached() != 9 || view.getChild() != source.details) throw new AssertionError("Borrow");
        if (!java.util.Arrays.equals(view.encode(protocol), expected)) throw new AssertionError("Wire");
        return 0;
      });
    }
    source.details.label = "updated";
    source.withProject(view -> { if (!view.getLabel().equals("updated")) throw new AssertionError("Stale"); return 0; });
    var generic = new Schema.TranslatorShort(); generic.value = 17;
    generic.withProjectValue(view -> {
      if (view.getValue() != 17 || view.getGeneration() != 2 || !java.util.Arrays.equals(view.encode(Schema.Protocol.BINARY_NONE), new byte[]{17, 0, 2, 0, 0, 0})) throw new AssertionError("Generic");
      return 0;
    });
  }
}
'''),
    "csharp": ("Program.cs", r'''
using System;
using System.Linq;
using static SerializerGenerated.Schema;
internal static class Program {
  // Compare every projected protocol with its independent owning conversion.
  private static void Main() {
    var source = new TranslatorSource(); source.Number = 41; source.Details.Label = "original";
    foreach (Protocol protocol in Enum.GetValues(typeof(Protocol))) {
      var expected = source.Materialize().Encode(protocol);
      source.WithProject(view => {
        if (view.GetNumber() != 41 || view.GetDefaulted() != 7 || view.GetCached() != 9 || !ReferenceEquals(view.GetChild(), source.Details)) throw new Exception("Borrow");
        if (!view.Encode(protocol).SequenceEqual(expected)) throw new Exception("Wire");
        return 0;
      });
    }
    source.Details.Label = "updated";
    source.WithProject(view => { if (view.GetLabel() != "updated") throw new Exception("Stale"); return 0; });
    var generic = new TranslatorShort(); generic.Value = 17;
    generic.WithProjectValue(view => {
      if (view.GetValue() != 17 || view.GetGeneration() != 2 || !view.Encode(Protocol.BINARY_NONE).SequenceEqual(new byte[]{17, 0, 2, 0, 0, 0})) throw new Exception("Generic");
      return 0;
    });
  }
}
'''),
    "go": ("main.go", r'''
package main
import "bytes"
// main verifies borrowed child identity, defaults and native protocol byte equivalence.
func main() {
  source := NewTranslatorSource(); source.Number = 41; source.Details.Label = "original"
  for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
    expected, err := source.Materialize().Encode(protocol); if err != nil { panic(err) }
    source.WithProject(func(view TranslatorSourceProjectTranslator) {
      if view.GetNumber() != 41 || view.GetDefaulted() != 7 || view.GetCached() != 9 || view.GetChild() != source.Details { panic("Borrow") }
      actual, failure := view.Encode(protocol); if failure != nil { panic(failure) }; if !bytes.Equal(actual, expected) { panic("Wire") }
    })
  }
  source.Details.Label = "updated"
  source.WithProject(func(view TranslatorSourceProjectTranslator) { if view.GetLabel() != "updated" { panic("Stale") } })
  generic := NewTranslatorShort(); generic.Value = 17
  generic.WithProjectValue(func(view TranslatorShortProjectValueTranslator) {
    actual, err := view.Encode(BINARY_NONE)
    if err != nil || view.GetValue() != 17 || view.GetGeneration() != 2 || !bytes.Equal(actual, []byte{17, 0, 2, 0, 0, 0}) { panic("Generic") }
  })
}
'''),
    "rust": ("main.rs", r'''
mod schema;
use schema::*;
// Compare all native target encodings and borrow exact nested storage with compiler lifetimes.
fn main() {
  let mut source = TranslatorSource::default(); source.number = 41; source.details.label = "original".into();
  for protocol in [Protocol::Json, Protocol::BinaryNone, Protocol::BinaryInteger, Protocol::BinaryString] {
    let expected = source.materialize().encode(protocol).unwrap();
    source.with_project(|view| {
      assert_eq!(view.get_number(), 41); assert_eq!(view.get_defaulted(), 7); assert_eq!(view.get_cached(), 9);
      assert!(std::ptr::eq(view.get_child(), &source.details));
      assert_eq!(view.encode(protocol).unwrap(), expected);
    });
  }
  source.details.label = "updated".into();
  assert_eq!(source.project().get_label(), "updated");
  let mut generic = TranslatorShort::default(); generic.value = 17;
  generic.with_project_value(|view| {
    assert_eq!(view.get_value(), 17); assert_eq!(view.get_generation(), 2);
    assert_eq!(view.encode(Protocol::BinaryNone).unwrap(), vec![17, 0, 2, 0, 0, 0]);
  });
}
'''),
    "swift": ("main.swift", r'''
var source = TranslatorSource(); source.number = 41; source.details.label = "original"
for protocolValue in [Protocol.JSON, .BINARY_NONE, .BINARY_INTEGER, .BINARY_STRING] {
  let expected = try source.materialize().encode(protocolValue)
  try source.withProject { view in
    precondition(view.getNumber() == 41 && view.getDefaulted() == 7 && view.getCached() == 9)
    let actual = try view.encode(protocolValue)
    precondition(actual == expected)
  }
}
source.details.label = "updated"
source.withProject { view in precondition(view.getLabel() == "updated") }
var generic = TranslatorShort(); generic.value = 17
try generic.withProjectValue { view in
  let bytes = try view.encode(.BINARY_NONE)
  precondition(view.getValue() == 17 && view.getGeneration() == 2 && bytes == [17, 0, 2, 0, 0, 0])
}
'''),
    "kotlin": ("Main.kt", r'''
fun main() {
  val source = TranslatorSource(); source.number = 41u; source.details.label = "original"
  for (protocol in Protocol.entries) {
    val expected = source.materialize().encode(protocol)
    source.withProject { view ->
      check(view.getNumber() == 41u && view.getDefaulted() == 7u.toUByte() && view.getCached() == 9u)
      check(view.getChild() === source.details)
      check(view.encode(protocol).contentEquals(expected))
    }
  }
  source.details.label = "updated"
  source.withProject { view -> check(view.getLabel() == "updated") }
  val generic = TranslatorShort(); generic.value = 17u
  generic.withProjectValue { view ->
    check(view.getValue() == 17u.toUShort() && view.getGeneration() == 2u)
    check(view.encode(Protocol.BINARY_NONE).contentEquals(byteArrayOf(17, 0, 2, 0, 0, 0)))
  }
}
'''),
    "c": ("main.c", r'''
#include "schema.h"
#include <assert.h>
#include <string.h>
typedef struct { const translator_source* source; const srl_buffer* expected; srl_protocol protocol; } context;
/* Borrow native storage and compare complete target bytes without retaining the scoped view. */
static void inspect(const translator_source_project_translator* view, void* raw) {
  const context* data = (const context*)raw;
  assert(*translator_source_project_translator_get_number(view) == 41);
  assert(*translator_source_project_translator_get_defaulted(view) == 7);
  assert(*translator_source_project_translator_get_cached(view) == 9);
  assert(translator_source_project_translator_get_child(view) == &data->source->details);
  srl_buffer actual = {0}; assert(translator_source_project_translator_encode(view, data->protocol, &actual) == srl_ok);
  assert(actual.size == data->expected->size && memcmp(actual.data, data->expected->data, actual.size) == 0);
  srl_buffer_free(&actual);
}
/* Pin the exact six-byte generic projection independently of an owning conversion. */
static void inspect_generic(const translator_short_project_value_translator* view, void* raw) {
  (void)raw;
  assert(*translator_short_project_value_translator_get_value(view) == 17);
  assert(*translator_short_project_value_translator_get_generation(view) == 2);
  srl_buffer bytes = {0}; assert(translator_short_project_value_translator_encode(view, srl_binary_none, &bytes) == srl_ok);
  const uint8_t expected[] = {17, 0, 2, 0, 0, 0}; assert(bytes.size == sizeof(expected) && memcmp(bytes.data, expected, sizeof(expected)) == 0);
  srl_buffer_free(&bytes);
}
/* Exercise all native protocols with initialized ownership and fresh projected defaults. */
int main(void) {
  translator_source source = {0}; assert(translator_source_init(&source) == srl_ok); source.number = 41;
  assert(srl_string_set(&source.details.label, "original", 8) == srl_ok);
  for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
    translator_target target = {0}; assert(translator_source_materialize(&target, &source) == srl_ok);
    srl_buffer expected = {0}; assert(translator_target_encode(&target, (srl_protocol)protocol, &expected) == srl_ok);
    context data = {&source, &expected, (srl_protocol)protocol};
    assert(translator_source_with_project(&source, inspect, &data) == srl_ok);
    srl_buffer_free(&expected); translator_target_free(&target);
  }
  translator_source_free(&source);
  translator_short generic = {0}; assert(translator_short_init(&generic) == srl_ok); generic.value = 17;
  assert(translator_short_with_project_value(&generic, inspect_generic, NULL) == srl_ok);
  translator_short_free(&generic); return 0;
}
'''),
}


def main():
    """Generate each backend, compile its actual consumer and compare all native wire protocols."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/translator-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    args.sanitize = False
    selected = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language): args.build / language / examples.OUTPUTS[language] for language in selected}
    if "typescript" in selected and "javascript" not in selected:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/translator.serializer", "--language", ",".join(targets),
               "--cpp.format=false", "--kotlin.package=", "--go.package=main"]
    for language, output in targets.items():
        output.parent.mkdir(parents=True, exist_ok=True)
        command.extend([f"--{language}.output", output])
    examples.run(command)
    if "typescript" in selected and "javascript" in selected:
        (args.build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    runner = examples.Runner(args)
    for language in selected:
        source = args.build / "consumers" / language
        source.mkdir(parents=True, exist_ok=True)
        filename, contents = CONSUMERS[language]
        (source / filename).write_text(contents, encoding="utf-8")
        examples.run(runner.build(language, "translators", args.build / language, source=source))
        print(f"PASS: {language}, scoped readonly projections, borrowed children, defaults and native wire equivalence", flush=True)


if __name__ == "__main__":
    main()
