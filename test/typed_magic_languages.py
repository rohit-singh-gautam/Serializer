"""Qualify scalar/enum magic through real SDKs, frozen bytes, and malformed input."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)
FILES = {"cpp": "main.cpp", "java": "Main.java", "javascript": "main.mjs",
         "typescript": "main.mts", "go": "main.go", "csharp": "Program.cs",
         "rust": "main.rs", "python": "main.py", "swift": "main.swift",
         "kotlin": "Main.kt", "c": "main.c"}
HEADERS = (
    ("bool_header", "?", False), ("character_header", "B", 81),
    ("signed8_header", "b", -128), ("unsigned8_header", "B", 255),
    ("signed16_header", "h", -32768), ("unsigned16_header", "H", 65535),
    ("signed32_header", "i", -2147483648), ("unsigned32_header", "I", 4294967295),
    ("signed64_header", "q", -9223372036854775808),
    ("unsigned64_header", "Q", 18446744073709551615),
    ("single_header", "f", 0.1), ("double_header", "d", 1.25),
    ("enum_header", "B", 1), ("zero_header", "I", 0), ("omitted_header", "I", 123),
)


def consumer(language):
    """Provide independent consumers for exact round trips and explicit rejection."""
    if language in ("javascript", "typescript"):
        text = '''
import { readFileSync, writeFileSync } from "node:fs";
import { TypedMagicRecord, Protocol } from "./schema.mjs";
if(process.argv.length>4) {
  const bytes=readFileSync(process.argv[4]);let rejected=false;
  try { TypedMagicRecord.decode(bytes,Number(process.argv[3])); } catch { rejected=true; }
  if(!rejected)throw Error("Invalid typed magic accepted");
} else {
  const value=new TypedMagicRecord();
  for(let p=0;p<4;++p) {
    const bytes=value.encode(p),copy=TypedMagicRecord.decode(bytes,p);
    if(bytes.toString()!==copy.encode(p).toString())throw Error("Typed magic round trip");
    writeFileSync(process.argv[2]+"."+p,bytes);
  }
}
'''
        if language == "typescript":
            text = text.replace("Number(process.argv[3])", "Number(process.argv[3]) as Protocol")
            text = text.replace("value.encode(p)", "value.encode(p as Protocol)")
            text = text.replace("decode(bytes,p)", "decode(bytes,p as Protocol)")
            text = text.replace("copy.encode(p)", "copy.encode(p as Protocol)")
        return text
    if language == "python":
        return '''
from pathlib import Path
import sys
from schema import TypedMagicRecord, Protocol
if len(sys.argv)>3:
    try: TypedMagicRecord.decode(Path(sys.argv[3]).read_bytes(),Protocol(int(sys.argv[2])))
    except ValueError: pass
    else: raise AssertionError("Invalid typed magic accepted")
else:
    value=TypedMagicRecord()
    for protocol in Protocol:
        data=value.encode(protocol)
        assert TypedMagicRecord.decode(data,protocol).encode(protocol)==data
        Path(sys.argv[1]+"."+str(int(protocol))).write_bytes(data)
'''
    if language == "java":
        return '''
import java.nio.file.Files;
import java.nio.file.Path;
public final class Main {
  /** Round-trip generated values and reject requested malformed input. */
  public static void main(String[] args) throws Exception {
    if(args.length>2) {
      var bytes=Files.readAllBytes(Path.of(args[2]));boolean rejected=false;
      try { Schema.TypedMagicRecord.decode(bytes,Schema.Protocol.values()[Integer.parseInt(args[1])]); }
      catch(IllegalArgumentException error) { rejected=true; }
      if(!rejected)throw new AssertionError("Invalid typed magic accepted");
    } else {
      var value=new Schema.TypedMagicRecord();
      for(var protocol:Schema.Protocol.values()) {
        var bytes=value.encode(protocol);
        if(!java.util.Arrays.equals(bytes,Schema.TypedMagicRecord.decode(bytes,protocol).encode(protocol)))throw new AssertionError("Typed magic round trip");
        Files.write(Path.of(args[0]+"."+protocol.ordinal()),bytes);
      }
    }
  }
}
'''
    if language == "csharp":
        return '''
using System;
using System.IO;
using static SerializerGenerated.Schema;
public static class Program {
  // Round-trip generated values and reject requested malformed input.
  private static void Main(string[] args) {
    if(args.Length>2) {
      var bytes=File.ReadAllBytes(args[2]);bool rejected=false;
      try { TypedMagicRecord.Decode(bytes,(Protocol)int.Parse(args[1])); } catch(FormatException) { rejected=true; }
      if(!rejected)throw new Exception("Invalid typed magic accepted");
    } else {
      var value=new TypedMagicRecord();
      foreach(var protocol in Enum.GetValues<Protocol>()) {
        var bytes=value.Encode(protocol);
        if(!bytes.AsSpan().SequenceEqual(TypedMagicRecord.Decode(bytes,protocol).Encode(protocol)))throw new Exception("Typed magic round trip");
        File.WriteAllBytes(args[0]+"."+(int)protocol,bytes);
      }
    }
  }
}
'''
    if language == "go":
        return '''
package main
import ("bytes";"fmt";"os";"strconv")
// run propagates I/O and generated codec failures.
func run() error {
  if len(os.Args)>3 {
    data,err:=os.ReadFile(os.Args[3]);if err!=nil{return err};p,err:=strconv.Atoi(os.Args[2]);if err!=nil{return err}
    if _,err:=DecodeTypedMagicRecord(data,Protocol(p));err==nil{return fmt.Errorf("Invalid typed magic accepted")}
    return nil
  }
  value:=NewTypedMagicRecord()
  for p:=0;p<4;p++ {
    protocol:=Protocol(p);data,err:=value.Encode(protocol);if err!=nil{return err}
    copy,err:=DecodeTypedMagicRecord(data,protocol);if err!=nil{return err};encoded,err:=copy.Encode(protocol);if err!=nil{return err}
    if !bytes.Equal(data,encoded){return fmt.Errorf("Typed magic round trip")}
    if err:=os.WriteFile(os.Args[1]+fmt.Sprintf(".%d",p),data,0600);err!=nil{return err}
  }
  return nil
}
// main exposes qualification failures as a nonzero exit.
func main() { if err:=run();err!=nil{fmt.Fprintln(os.Stderr,err);os.Exit(1)} }
'''
    if language == "rust":
        return '''
mod schema;
use schema::*;
use std::{env,fs};
/// Round-trip generated values and reject requested malformed input.
fn main() -> Result<(),Box<dyn std::error::Error>> {
  let args:Vec<String>=env::args().collect();
  let protocols=[Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString];
  if args.len()>3 {
    let bytes=fs::read(&args[3])?;let p:usize=args[2].parse()?;
    assert!(TypedMagicRecord::decode(&bytes,protocols[p]).is_err(),"Invalid typed magic accepted");
  } else {
    let value=TypedMagicRecord::default();
    for (p,protocol) in protocols.iter().enumerate() {
      let data=value.encode(*protocol)?;
      assert_eq!(TypedMagicRecord::decode(&data,*protocol)?.encode(*protocol)?,data);
      fs::write(format!("{}.{p}",args[1]),data)?;
    }
  }
  Ok(())
}
'''
    if language == "swift":
        return '''
import Foundation
let args=CommandLine.arguments
if args.count>3 {
  let data=Array(try Data(contentsOf:URL(fileURLWithPath:args[3])))
  var rejected=false
  do { _ = try TypedMagicRecord.decode(data,Protocol(rawValue:Int(args[2])!)!) } catch { rejected=true }
  precondition(rejected,"Invalid typed magic accepted")
} else {
  let value=TypedMagicRecord()
  for p in 0..<4 {
    let protocolValue=Protocol(rawValue:p)!
    let data=try value.encode(protocolValue)
    let restored=try TypedMagicRecord.decode(data,protocolValue).encode(protocolValue)
    precondition(restored==data)
    try Data(data).write(to:URL(fileURLWithPath:args[1]+".\\(p)"))
  }
}
'''
    if language == "kotlin":
        return '''
import java.io.File
// Round-trip generated values and reject requested malformed input.
fun main(args:Array<String>) {
  if(args.size>2) {
    val data=File(args[2]).readBytes();var rejected=false
    try { TypedMagicRecord.decode(data,Protocol.entries[args[1].toInt()]) } catch(error:IllegalArgumentException) { rejected=true }
    check(rejected){"Invalid typed magic accepted"}
  } else {
    val value=TypedMagicRecord()
    for(protocol in Protocol.entries) {
      val data=value.encode(protocol)
      check(TypedMagicRecord.decode(data,protocol).encode(protocol).contentEquals(data))
      File(args[0]+"."+protocol.ordinal).writeBytes(data)
    }
  }
}
'''
    if language == "cpp":
        return '''
#include <fstream>
#include <iterator>
#include <message.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>
#include <stdexcept>
#include <string>
// Encode through the public protocol API.
template <template <rohit::serializer::serialize_type> typename Protocol>
std::string encode(const typed_magic_record& value) {
  rohit::full_stream_auto_alloc output{};
  value.serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()),output.current_offset()};
}
// Decode a fresh value and enforce complete input consumption.
template <template <rohit::serializer::serialize_type> typename Protocol>
typed_magic_record decode(const std::string& bytes) {
  const auto input=rohit::make_constant_full_stream(bytes.data(),bytes.size());
  Protocol<rohit::serializer::serialize_type::in> reader{input};
  typed_magic_record value{};reader.serialize_in(value);reader.finish();return value;
}
// Write and round-trip one complete protocol artifact.
template <template <rohit::serializer::serialize_type> typename Protocol>
void emit(const std::string& prefix,int index) {
  const auto data=encode<Protocol>(typed_magic_record{});
  if(encode<Protocol>(decode<Protocol>(data))!=data)throw std::runtime_error{"Typed magic round trip"};
  std::ofstream output{prefix+"."+std::to_string(index),std::ios::binary};output.write(data.data(),static_cast<std::streamsize>(data.size()));
  if(!output)throw std::runtime_error{"Cannot write typed magic artifact"};
}
// Check whether malformed data fails complete decoding.
template <template <rohit::serializer::serialize_type> typename Protocol>
bool rejects(const std::string& bytes) {
  try { (void)decode<Protocol>(bytes); } catch(const std::exception&) { return true; }return false;
}
// Exercise every generated native protocol.
int main(int argc,char** argv) {
  if(argc>3) {
    std::ifstream input{argv[3],std::ios::binary};if(!input)return 1;
    const std::string bytes{std::istreambuf_iterator<char>{input},{}};
    switch(std::stoi(argv[2])) {
    case 0:return rejects<rohit::serializer::json>(bytes)?0:1;
    case 1:return rejects<rohit::serializer::binary_none>(bytes)?0:1;
    case 2:return rejects<rohit::serializer::binary_integer>(bytes)?0:1;
    case 3:return rejects<rohit::serializer::binary_string>(bytes)?0:1;
    default:return 1;
    }
  }
  emit<rohit::serializer::json>(argv[1],0);emit<rohit::serializer::binary_none>(argv[1],1);
  emit<rohit::serializer::binary_integer>(argv[1],2);emit<rohit::serializer::binary_string>(argv[1],3);
  return 0;
}
'''
    if language == "c":
        return '''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
// Abort qualification on a failed codec or I/O operation.
static void require(bool value) { if(!value){fprintf(stderr,"Typed magic qualification failed\\n");exit(1);} }
// Exercise transactional generated C decoders and exact native writers.
int main(int argc,char** argv) {
  typed_magic_record value={0};require(typed_magic_record_init(&value)==srl_ok);
  if(argc>3) {
    FILE* input=fopen(argv[3],"rb");require(input!=NULL);require(fseek(input,0,SEEK_END)==0);
    const long size=ftell(input);require(size>=0);rewind(input);uint8_t* bytes=(uint8_t*)malloc((size_t)size+1);require(bytes!=NULL);
    require(fread(bytes,1,(size_t)size,input)==(size_t)size);fclose(input);
    require(typed_magic_record_decode(&value,bytes,(size_t)size,(srl_protocol)atoi(argv[2]),NULL)!=srl_ok);
    free(bytes);
  } else {
    for(int p=0;p<4;++p) {
      srl_buffer bytes={0},restored={0};typed_magic_record copy={0};
      require(typed_magic_record_encode(&value,(srl_protocol)p,&bytes)==srl_ok);
      require(typed_magic_record_decode(&copy,bytes.data,bytes.size,(srl_protocol)p,NULL)==srl_ok);
      require(typed_magic_record_encode(&copy,(srl_protocol)p,&restored)==srl_ok);
      require(restored.size==bytes.size && memcmp(restored.data,bytes.data,bytes.size)==0);
      char path[4096];snprintf(path,sizeof(path),"%s.%d",argv[1],p);FILE* output=fopen(path,"wb");require(output!=NULL);
      require(fwrite(bytes.data,1,bytes.size,output)==bytes.size);fclose(output);
      typed_magic_record_free(&copy);srl_buffer_free(&bytes);srl_buffer_free(&restored);
    }
  }
  typed_magic_record_free(&value);return 0;
}
'''
    raise ValueError(language)


def malformed_json():
    """Mutate required headers separately, including duplicate and excluded metadata."""
    record = {name: {"magic": ("Q" if name == "character_header" else
                               "record" if name == "enum_header" else value), "value": 7}
              for name, _, value in HEADERS if name != "omitted_header"}
    record["omitted_header"] = {"value": 7}
    for name, _, value in HEADERS:
        candidate = copy.deepcopy(record)
        if name == "omitted_header":
            candidate[name]["magic"] = value
            yield json.dumps(candidate).encode()
            continue
        del candidate[name]["magic"]
        yield json.dumps(candidate).encode()
        candidate = copy.deepcopy(record)
        candidate[name]["magic"] = ("R" if name == "character_header" else
                                    "unknown" if name == "enum_header" else
                                    not value if isinstance(value, bool) else
                                    0 if value != 0 else 1)
        yield json.dumps(candidate).encode()
        text = json.dumps(record)
        original = json.dumps(record[name]["magic"])
        fragment = json.dumps(name) + ': {"magic": ' + original
        yield text.replace(fragment, fragment + ', "magic": ' + original, 1).encode()


def verify_json(data):
    """Validate native scalar JSON representations independent of float formatting."""
    record = json.loads(data)
    assert set(record) == {name for name, _, _ in HEADERS}
    for name, codec, value in HEADERS:
        assert set(record[name]) == ({"value"} if name == "omitted_header" else {"magic", "value"})
        assert record[name]["value"] == 7
        if name == "omitted_header":
            assert "magic" not in record[name]
        elif name == "character_header":
            assert record[name]["magic"] == "Q"
        elif name == "enum_header":
            assert record[name]["magic"] == "record"
        elif codec == "f":
            assert struct.pack("<f", record[name]["magic"]) == struct.pack("<f", value)
        else:
            assert record[name]["magic"] == value


def main():
    """Build SDKs, pin wire bytes, and run independently mutated failure fixtures."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/typed-magic-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language):
               args.build / language / examples.OUTPUTS[language] for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/typed_magic_language.serializer",
               "--language", ",".join(targets), "--cpp.format=false", "--go.package=main",
               "--kotlin.package="]
    for language, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        command += [f"--{language}.output", destination]
    examples.run(command)
    if "typescript" in languages and "javascript" in languages:
        (args.build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    runner = examples.Runner(args)
    expected = b"".join(struct.pack("<" + codec, value) + struct.pack("<H", 7)
                        for _, codec, value in HEADERS)
    mutation_cases = [(0, data) for data in malformed_json()]
    offset = 0
    for _, codec, _ in HEADERS:
        width = struct.calcsize("<" + codec)
        changed = bytearray(expected)
        # Alter the most significant byte so float rounding cannot hide the mismatch.
        changed[offset + width - 1] ^= 1
        mutation_cases.append((1, bytes(changed)))
        mutation_cases.append((1, expected[:offset + width - 1]))
        offset += width + struct.calcsize("<H")
    invalid = args.build / "invalid"
    invalid.mkdir(parents=True, exist_ok=True)
    cases = []
    for index, (protocol, data) in enumerate(mutation_cases):
        path = invalid / f"{index}.bin"
        path.write_bytes(data)
        cases.append((protocol, path))
    baseline = None
    for language in languages:
        directory = args.build / language
        source = args.build / "consumers" / language
        source.mkdir(parents=True, exist_ok=True)
        (source / FILES[language]).write_text(consumer(language), encoding="utf-8")
        executable = runner.build(language, "typed_magic", directory, source)
        output = directory / "result"
        examples.run([*executable, runner.path(language, output)])
        produced = [Path(str(output) + f".{p}").read_bytes() for p in range(4)]
        verify_json(produced[0])
        assert produced[1] == expected, (language, "positional wire mismatch")
        if baseline:
            assert produced[1:] == baseline[1:], (language, "binary wire mismatch")
        baseline = produced
        language_cases = list(cases)
        for p in (2, 3):
            for index, data in enumerate((bytes([produced[p][0] ^ 1]) + produced[p][1:], b"")):
                path = invalid / f"first-{p}-{index}.bin"
                path.write_bytes(data)
                language_cases.append((p, path))
        if language in runner.wsl:
            # A single WSL launch avoids repeating service startup for each malformed fixture.
            remote_directory = runner.path(language, invalid)
            remote_cases = [(protocol, remote_directory + "/" + path.name)
                            for protocol, path in language_cases]
            script = ("import json,subprocess,sys; base=json.loads(sys.argv[1]); "
                      "cases=json.loads(sys.argv[2]); "
                      "[subprocess.run(base+['reject',str(protocol),path],check=True) "
                      "for protocol,path in cases]")
            examples.run(["wsl", "--exec", "python3", "-c", script,
                          json.dumps(executable[2:]), json.dumps(remote_cases)])
        else:
            for protocol, path in language_cases:
                examples.run([*executable, "reject", protocol, runner.path(language, path)])
        print(f"PASS: {language} scalar/enum magic, defaults, omissions, rejection and wire agreement", flush=True)


if __name__ == "__main__":
    main()
