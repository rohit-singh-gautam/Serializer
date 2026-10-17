"""Qualify native codec selection with each actual generated-language SDK."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('serializer_examples', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdk)
FILES = {'cpp': 'main.cpp', 'java': 'Main.java', 'javascript': 'main.mjs', 'typescript': 'main.mts',
         'go': 'main.go', 'csharp': 'Program.cs', 'rust': 'main.rs', 'python': 'main.py',
         'swift': 'main.swift', 'kotlin': 'Main.kt', 'c': 'main.c'}


def consumer_source(language, restricted):
    """Round-trip enabled formats and require both excluded directions to fail."""
    enabled = 'false' if restricted else 'true'
    if language in ('javascript', 'typescript'):
        source = '''import {Sample,Protocol} from './schema.mjs';
const value=new Sample();
const allowedAll=ENABLED;
for(const raw of Object.values(Protocol)) {
  const protocol=raw;
  const allowed=allowedAll || raw===Protocol.BINARY_NONE;
  let bytes=new Uint8Array([1,0,0,0,7,0,0,0]);
  let rejected=false;
  try { bytes=value.encode(protocol); } catch { rejected=true; }
  if(rejected===allowed)throw Error('Encode selection mismatch');
  rejected=false;
  try { const copy=Sample.decode(bytes,protocol);if(copy.value!==7)throw Error('Value changed'); }
  catch { rejected=true; }
  if(rejected===allowed)throw Error('Decode selection mismatch');
}
'''
        if language == 'typescript':
            source = source.replace('const protocol=raw;', 'const protocol=raw as 1;')
            source = source.replace('let bytes=', 'let bytes: Uint8Array=')
            if restricted:
                source += '''// Compile-time selection must also reject excluded native formats.
const compileContract=()=>{
  // @ts-expect-error JSON was excluded during generation.
  value.encode(Protocol.JSON);
  // @ts-expect-error keyed binary was excluded during generation.
  Sample.decode(new Uint8Array(),Protocol.BINARY_INTEGER);
};
'''
        return source.replace('ENABLED', enabled)
    if language == 'python':
        return '''from schema import Sample, Protocol
value=Sample()
for protocol in Protocol:
    allowed=ENABLED or protocol==Protocol.BINARY_NONE
    data=bytes([1,0,0,0,7,0,0,0])
    rejected=False
    try: data=value.encode(protocol)
    except ValueError: rejected=True
    assert rejected != allowed, 'Encode selection mismatch'
    rejected=False
    try: assert Sample.decode(data,protocol).value==7
    except ValueError: rejected=True
    assert rejected != allowed, 'Decode selection mismatch'
'''.replace('ENABLED', str(not restricted))
    if language == 'java':
        return '''public final class Main {
  /** Qualify the configured protocol boundary in both directions. */
  public static void main(String[] args) {
    var value=new Schema.Sample();
    for(var protocol:Schema.Protocol.values()) {
      boolean allowed=ENABLED || protocol==Schema.Protocol.BINARY_NONE;
      byte[] bytes={1,0,0,0,7,0,0,0};boolean rejected=false;
      try { bytes=value.encode(protocol); } catch(IllegalArgumentException error) { rejected=true; }
      if(rejected==allowed)throw new AssertionError("Encode selection mismatch");
      rejected=false;
      try { if(Schema.Sample.decode(bytes,protocol).value!=7)throw new AssertionError("Value changed"); }
      catch(IllegalArgumentException error) { rejected=true; }
      if(rejected==allowed)throw new AssertionError("Decode selection mismatch");
    }
  }
}
'''.replace('ENABLED', enabled)
    if language == 'csharp':
        return '''using System;
using static SerializerGenerated.Schema;
public static class Program {
  // Exercise both selected and excluded format entry points.
  private static void Main() {
    var value=new Sample();
    foreach(var protocol in Enum.GetValues<Protocol>()) {
      bool allowed=ENABLED || protocol==Protocol.BINARY_NONE;
      byte[] bytes={1,0,0,0,7,0,0,0};bool rejected=false;
      try { bytes=value.Encode(protocol); } catch(ArgumentException) { rejected=true; }
      if(rejected==allowed)throw new Exception("Encode selection mismatch");
      rejected=false;
      try { if(Sample.Decode(bytes,protocol).Value!=7)throw new Exception("Value changed"); }
      catch(ArgumentException) { rejected=true; }
      if(rejected==allowed)throw new Exception("Decode selection mismatch");
    }
  }
}
'''.replace('ENABLED', enabled)
    if language == 'go':
        return '''package main
import "fmt"
// main checks generated native protocol gates and unchanged selected values.
func main() {
  value:=NewSample()
  for protocol:=JSON;protocol<=BINARY_STRING;protocol++ {
    allowed:=ENABLED || protocol==BINARY_NONE
    data,err:=value.Encode(protocol)
    if (err==nil)!=allowed { panic("Encode selection mismatch") }
    if !allowed { data=[]byte{1,0,0,0,7,0,0,0} }
    copy,err:=DecodeSample(data,protocol)
    if (err==nil)!=allowed { panic("Decode selection mismatch") }
    if allowed && copy.Value!=7 { panic(fmt.Sprint("Value changed: ",copy.Value)) }
  }
}
'''.replace('ENABLED', enabled)
    if language == 'rust':
        return '''mod schema;
use schema::*;
/// Exercise native Result failures without accepting an excluded wire format.
fn main() {
  let value=Sample::default();
  for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] {
    let allowed=ENABLED || protocol==Protocol::BinaryNone;
    let data=value.encode(protocol);
    assert_eq!(data.is_ok(),allowed);
    let bytes=data.unwrap_or_else(|_|vec![1,0,0,0,7,0,0,0]);
    let copy=Sample::decode(&bytes,protocol);
    assert_eq!(copy.is_ok(),allowed);
    if allowed { assert_eq!(copy.unwrap().value,7); }
  }
}
'''.replace('ENABLED', enabled)
    if language == 'swift':
        return '''import Foundation
let value=Sample()
for raw in 0..<4 {
  let protocolValue=Protocol(rawValue:raw)!
  let allowed=ENABLED || protocolValue == .BINARY_NONE
  var bytes:[UInt8]=[1,0,0,0,7,0,0,0]
  var rejected=false
  do { bytes=try value.encode(protocolValue) } catch { rejected=true }
  precondition(rejected != allowed,"Encode selection mismatch")
  rejected=false
  do { let copy=try Sample.decode(bytes,protocolValue);precondition(copy.value==7) } catch { rejected=true }
  precondition(rejected != allowed,"Decode selection mismatch")
}
'''.replace('ENABLED', enabled)
    if language == 'kotlin':
        return '''// Exercise direct generated format entry points, including excluded input.
fun main() {
  val value=Sample()
  for(protocol in Protocol.entries) {
    val allowed=ENABLED || protocol==Protocol.BINARY_NONE
    var bytes=byteArrayOf(1,0,0,0,7,0,0,0);var rejected=false
    try { bytes=value.encode(protocol) } catch(error:IllegalArgumentException) { rejected=true }
    check(rejected!=allowed){"Encode selection mismatch"}
    rejected=false
    try { check(Sample.decode(bytes,protocol).value==7L.toUInt()) } catch(error:IllegalArgumentException) { rejected=true }
    check(rejected!=allowed){"Decode selection mismatch"}
  }
}
'''.replace('ENABLED', enabled)
    if language == 'c':
        return '''#include "schema.h"
#include <assert.h>
// Confirm rejected decodes leave the caller's existing aggregate unchanged.
int main(void) {
  sample value={0};assert(sample_init(&value)==srl_ok);
  for(int raw=0;raw<4;++raw) {
    const srl_protocol protocol=(srl_protocol)raw;
    const bool allowed=ENABLED || protocol==srl_binary_none;
    srl_buffer bytes={0};sample copy={0};assert(sample_init(&copy)==srl_ok);copy.value=123;
    assert((sample_encode(&value,protocol,&bytes)==srl_ok)==allowed);
    const uint8_t fallback[]={1,0,0,0,7,0,0,0};
    assert((sample_decode(&copy,allowed?bytes.data:fallback,allowed?bytes.size:sizeof(fallback),protocol,NULL)==srl_ok)==allowed);
    assert(copy.value==(allowed?7u:123u));
    sample_free(&copy);srl_buffer_free(&bytes);
  }
  sample_free(&value);return 0;
}
'''.replace('ENABLED', enabled)
    if language == 'cpp':
        source = '''#include "message.hpp"
#include <rohit/serializer.hpp>
#include <cassert>
namespace codec=rohit::serializer;
// Check concrete protocol availability at compile time.
template<class Protocol> concept supports_output=requires(const sample& value,Protocol& out){value.serialize_out(out);};
static_assert(supports_output<codec::binary_none<codec::serialize_type::out>>);
static_assert(supports_output<codec::json<codec::serialize_type::out>> == ENABLED);
// Round-trip one selected native protocol through the public generated APIs.
template<template<codec::serialize_type> class Protocol> void qualify(){
  sample value{};rohit::full_stream_auto_alloc out{};value.serialize_out<Protocol>(out);
  const auto bytes=rohit::make_constant_full_stream(out.begin(),out.current_offset());
  Protocol<codec::serialize_type::in> input{bytes};sample copy{};input.serialize_in(copy);input.finish();assert(copy.value==7);
}
int main(){qualify<codec::binary_none>();MORE}
'''.replace('ENABLED', enabled)
        return source.replace('MORE', '' if restricted else 'qualify<codec::json>();qualify<codec::binary_integer>();qualify<codec::binary_string>();')
    raise ValueError(language)


def consumer(language, restricted, fixtures):
    """Use valid excluded-format messages so ordinary malformed-input checks cannot mask a missing gate."""
    source = consumer_source(language, restricted)
    values = lambda data, signed=False: ','.join(str(byte - 256 if signed and byte >= 128 else byte) for byte in data)
    if language in ('javascript', 'typescript'):
        declaration = 'const fixtures=[' + ','.join('new Uint8Array([' + values(data) + '])' for data in fixtures) + '];\n'
        return source.replace('const value=', declaration + 'const value=').replace('new Uint8Array([1,0,0,0,7,0,0,0])', 'fixtures[raw]')
    if language == 'python':
        return source.replace('value=Sample()', 'fixtures=' + repr(tuple(fixtures)) + '\nvalue=Sample()').replace('bytes([1,0,0,0,7,0,0,0])', 'fixtures[int(protocol)]')
    if language == 'java':
        declaration = 'byte[][] fixtures={' + ','.join('{' + values(data, True) + '}' for data in fixtures) + '};\n    '
        return source.replace('var value=', declaration + 'var value=').replace('byte[] bytes={1,0,0,0,7,0,0,0}', 'byte[] bytes=fixtures[protocol.ordinal()]')
    if language == 'csharp':
        declaration = 'byte[][] fixtures={' + ','.join('new byte[]{' + values(data) + '}' for data in fixtures) + '};\n    '
        return source.replace('var value=', declaration + 'var value=').replace('byte[] bytes={1,0,0,0,7,0,0,0}', 'byte[] bytes=fixtures[(int)protocol]')
    if language == 'go':
        declaration = 'fixtures:=[][]byte{' + ','.join('{' + values(data) + '}' for data in fixtures) + '}\n  '
        return source.replace('value:=', declaration + 'value:=').replace('[]byte{1,0,0,0,7,0,0,0}', 'fixtures[int(protocol)]')
    if language == 'rust':
        declaration = 'let fixtures: [&[u8];4]=[' + ','.join('&[' + values(data) + ']' for data in fixtures) + '];\n  '
        source = source.replace('let value=', declaration + 'let value=').replace('for protocol in [', 'for (index,protocol) in [').replace('Protocol::BinaryString] {', 'Protocol::BinaryString].into_iter().enumerate() {')
        return source.replace('vec![1,0,0,0,7,0,0,0]', 'fixtures[index].to_vec()')
    if language == 'swift':
        declaration = 'let fixtures:[[UInt8]]=[' + ','.join('[' + values(data) + ']' for data in fixtures) + ']\n'
        return source.replace('let value=', declaration + 'let value=').replace('[1,0,0,0,7,0,0,0]', 'fixtures[raw]')
    if language == 'kotlin':
        declaration = 'val fixtures=arrayOf(' + ','.join('byteArrayOf(' + values(data, True) + ')' for data in fixtures) + ')\n  '
        return source.replace('val value=', declaration + 'val value=').replace('byteArrayOf(1,0,0,0,7,0,0,0)', 'fixtures[protocol.ordinal]')
    if language == 'c':
        declaration = ''.join('static const uint8_t fixture_' + str(index) + '[]={' + values(data) + '};\n' for index, data in enumerate(fixtures))
        declaration += 'static const uint8_t* const fixtures[]={fixture_0,fixture_1,fixture_2,fixture_3};\n'
        declaration += 'static const size_t fixture_sizes[]={sizeof(fixture_0),sizeof(fixture_1),sizeof(fixture_2),sizeof(fixture_3)};\n'
        return source.replace('// Confirm rejected', declaration + '// Confirm rejected').replace('const uint8_t fallback[]={1,0,0,0,7,0,0,0};', 'const uint8_t* fallback=fixtures[raw];').replace('allowed?bytes.size:sizeof(fallback)', 'allowed?bytes.size:fixture_sizes[raw]')
    return source


def main():
    """Compile each profile with strict SDK checks; missing selected SDKs are failures."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--cpp-library', type=Path)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/build/protocol-selection')
    parser.add_argument('--language', default='all')
    parser.add_argument('--wsl-languages', default='')
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    languages = sdk.LANGUAGES if args.language == 'all' else args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(language not in sdk.LANGUAGES for language in languages):
        parser.error('Select distinct supported language names')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    schema = args.build / 'model.serializer'
    schema.write_text('''serializer version 1.9.0;
class child { public string name { "hello" }; }
class sample stable_ids {
  public magic uint16 (9) { 51966 };
  public version uint32 (1) { 1 };
  public uint32 value (2) { 7 };
  public child nested (3);
  public optional<string> label (4);
  public array uint16 samples (5);
  public map(string) uint32 counts (6);
  public variant(uint16 = number, string = text) payload (7);
  public uint32 encoded_size (10) auto_size(binary_none, binary_integer, binary_string)
    size_scope(payload) omit(all_except(binary_none, binary_integer, binary_string));
}
''', encoding='utf-8')
    fixture_source = args.build / 'fixture_schema.py'
    sdk.run([args.compiler, '--input', schema, '--language', 'python', '--protocols', 'all',
             '--python.output', fixture_source])
    fixture_spec = importlib.util.spec_from_file_location('serializer_selection_fixture', fixture_source)
    fixture_module = importlib.util.module_from_spec(fixture_spec)
    fixture_spec.loader.exec_module(fixture_module)
    fixtures = [fixture_module.Sample().encode(protocol) for protocol in fixture_module.Protocol]
    runner = sdk.Runner(args)
    for restricted in (False, True):
        profile = 'binary_none' if restricted else 'all'
        directory = args.build / profile
        directory.mkdir(parents=True, exist_ok=True)
        config = directory / 'options.ini'
        config.write_text(f'[output]\nprotocols = {profile}\n', encoding='utf-8')
        targets = {('js' if language == 'javascript' else language): directory / language / sdk.OUTPUTS[language] for language in languages}
        if 'typescript' in languages and 'javascript' not in languages:
            targets['js'] = directory / 'typescript/schema.mjs'
        command = [args.compiler, '--input', schema, '--config', config, '--language', ','.join(targets),
                   '--cpp.format=false', '--go.package=main', '--kotlin.package=']
        for language, destination in targets.items():
            destination.parent.mkdir(parents=True, exist_ok=True)
            command += [f'--{language}.output', destination]
        sdk.run(command)
        if 'typescript' in languages and 'javascript' in languages:
            (directory / 'typescript/schema.mjs').write_bytes(targets['js'].read_bytes())
        for language in languages:
            source = directory / 'consumers' / language
            source.mkdir(parents=True, exist_ok=True)
            (source / FILES[language]).write_text(consumer(language, restricted, fixtures), encoding='utf-8')
            sdk.run(runner.build(language, 'protocol_selection', directory / language, source))
            print(f'PASS: {language}, {profile}, selected round trips and excluded encode/decode', flush=True)
    # The shared command-line option overrides INI selection regardless of its position.
    for profile, inherited in (('binary_none', 'all'), ('all', 'binary_none')):
        destination = args.build / f'override-{profile}.py'
        sdk.run([args.compiler, '--protocols', profile, '--input', schema, '--language', 'python',
                 '--python.output', destination, '--config', args.build / inherited / 'options.ini'])
        source = destination.read_text(encoding='utf-8')
        if ('Protocol disabled during generation' in source) != (profile == 'binary_none'):
            raise AssertionError('Command-line protocol selection did not override the INI')
    print('PASS: common command-line selection overrides both INI profiles', flush=True)


if __name__ == '__main__':
    main()
