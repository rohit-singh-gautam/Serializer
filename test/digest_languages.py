"""Qualify digest storage in real SDKs with independent wire and invalid-length fixtures."""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("compact", ROOT / "test/compact_languages.py")
compact = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(compact)
examples = compact.examples

# Standard output sizes are specification data, independent of the compiler/runtime table.
FIELDS = (("raw", 0), ("manual", 32), ("md5_value", 16), ("sha1_value", 20),
          ("sha224_value", 28), ("sha256_value", 32), ("sha384_value", 48),
          ("sha512_value", 64), ("sha512_224_value", 28), ("sha512_256_value", 32),
          ("sha3_224_value", 28), ("sha3_256_value", 32), ("sha3_384_value", 48),
          ("sha3_512_value", 64))


def prefix(value):
    """Encode the independently specified 30-bit unsigned native count prefix."""
    width = next(n for n in range(1, 5) if value < 1 << (8 * n - 2))
    return ((width - 1) << (8 * width - 2) | value).to_bytes(width, "big")


def record(nonzero=False):
    """Create canonical native JSON values, including nested digest collections."""
    value = {name: [((index * 17 + 1) & 255) if nonzero else 0 for index in range(size)]
             for name, size in FIELDS}
    if nonzero:
        value["raw"] = [0, 128, 255, 1]
    value["values"] = [[0, 128, 255, 1] * 4] if nonzero else []
    value["lookup"] = [{"key": "entry", "value": [0, 128, 255, 1]}] if nonzero else []
    value["choice:present" if nonzero else "choice:absent"] = [0, 128, 255, 1] * 4 if nonzero else 0
    return value


def wire(protocol, value):
    """Assemble native digest wire bytes without invoking a Serializer codec."""
    output = bytearray()
    for identifier, (name, _) in enumerate(FIELDS, 1):
        if protocol == 2:
            output.extend(prefix(identifier))
        elif protocol == 3:
            text = name.encode()
            output.extend(prefix(len(text)) + text)
        output.extend(prefix(len(value[name])) + bytes(value[name]))
    for identifier, name in ((15, "values"), (16, "lookup"), (17, "choice")):
        if protocol == 2:
            output.extend(prefix(identifier))
        elif protocol == 3:
            text = name.encode()
            if name == "choice":
                text += b":present" if "choice:present" in value else b":absent"
            output.extend(prefix(len(text)) + text)
        if name == "values":
            output.extend(prefix(len(value[name])))
            for item in value[name]:
                output.extend(prefix(len(item)) + bytes(item))
        elif name == "lookup":
            output.extend(prefix(len(value[name])))
            for item in value[name]:
                key = item["key"].encode()
                output.extend(prefix(len(key)) + key + prefix(len(item["value"])) + bytes(item["value"]))
        elif "choice:present" in value:
            if protocol != 3:
                output.extend(prefix(1))
            output.extend(prefix(16) + bytes(value["choice:present"]))
        else:
            if protocol != 3:
                output.extend(prefix(0))
            output.append(0)
    if protocol != 1:
        output.append(0)
    return bytes(output)


def consumer(language):
    """Reuse tested SDK setup and enforce exact re-encoding of independent valid fixtures."""
    text = compact.consumer(language).replace("CompactRecord", "DigestRecord")
    text = text.replace("compact_record", "digest_record").replace("compact integer", "digest")
    replacements = {
        "javascript": ("else if(mode==='verify') rejected=true;",
                       "else if(mode==='verify') { if(Array.from(value.encode(Number(args[3]))).toString()!==Array.from(bytes).toString())throw Error('Digest wire mismatch'); rejected=true; }"),
        "typescript": ("else if(mode==='verify') rejected=true;",
                       "else if(mode==='verify') { if(Array.from(value.encode(Number(args[3]) as Protocol)).toString()!==Array.from(bytes).toString())throw Error('Digest wire mismatch'); rejected=true; }"),
        "python": ("        if mode=='overflow': value.encode(Protocol(int(args[2])))",
                   "        if mode=='overflow': value.encode(Protocol(int(args[2])))\n            elif mode=='verify': assert value.encode(Protocol(int(args[2])))==Path(args[3]).read_bytes()"),
        "java": ('else if(args[0].equals("verify"))rejected=true;',
                 'else if(args[0].equals("verify")) { if(!java.util.Arrays.equals(bytes,value.encode(Schema.Protocol.values()[Integer.parseInt(args[1])])))throw new AssertionError("Digest wire mismatch"); rejected=true; }'),
        "csharp": ('else if(args[0]=="verify")rejected=true;',
                   'else if(args[0]=="verify") { if(!bytes.AsSpan().SequenceEqual(value.Encode((Protocol)int.Parse(args[1]))))throw new Exception("Digest wire mismatch"); rejected=true; }'),
        "go": ('if os.Args[1]=="verify" {return err}',
               'if os.Args[1]=="verify" {if err!=nil{return err};encoded,err:=value.Encode(Protocol(p));if err!=nil{return err};if !bytes.Equal(data,encoded){return fmt.Errorf("Digest wire mismatch")};return nil}'),
        "rust": ('if args[1]=="verify" {DigestRecord::decode(&bytes,protocols[p])?;}',
                 'if args[1]=="verify" {assert_eq!(DigestRecord::decode(&bytes,protocols[p])?.encode(protocols[p])?,bytes);}'),
        "swift": ('else if args[1]=="verify" { rejected=true }',
                  'else if args[1]=="verify" { let encoded = try value.encode(Protocol(rawValue:Int(args[2])!)!); precondition(encoded == data); rejected=true }'),
        "kotlin": ('else if(args[0]=="verify")rejected=true',
                   'else if(args[0]=="verify") {check(value.encode(Protocol.entries[args[1].toInt()]).contentEquals(data));rejected=true}'),
        "cpp": ('try { (void)decode<Protocol>(bytes); return mode=="verify"; }',
                'try { const auto value=decode<Protocol>(bytes); if(mode=="verify" && encode<Protocol>(value)!=bytes)throw std::logic_error("Digest wire mismatch"); return mode=="verify"; }'),
        "c": ('require(digest_record_decode(&value,bytes,(size_t)size,(srl_protocol)atoi(argv[2]),NULL)==srl_ok);',
              'require(digest_record_decode(&value,bytes,(size_t)size,(srl_protocol)atoi(argv[2]),NULL)==srl_ok);\n      srl_buffer encoded={0};require(digest_record_encode(&value,(srl_protocol)atoi(argv[2]),&encoded)==srl_ok);require(encoded.size==(size_t)size && !memcmp(encoded.data,bytes,(size_t)size));srl_buffer_free(&encoded);'),
    }
    old, new = replacements[language]
    text = compact.replace_once(text, old, new)
    # Mutable byte containers must also reject application changes to their fixed extent.
    mutations = {
        "javascript": ("if(mode==='overflow') value.encode(Number(args[3]));",
                       "if(mode==='overflow') { value.manual=Array(31).fill(0); value.encode(Number(args[3])); }"),
        "typescript": ("if(mode==='overflow') value.encode(Number(args[3]) as Protocol);",
                       "if(mode==='overflow') { value.manual=Array(31).fill(0); value.encode(Number(args[3]) as Protocol); }"),
        "python": ("if mode=='overflow': value.encode(Protocol(int(args[2])))",
                   "if mode=='overflow': value.manual=[0]*31; value.encode(Protocol(int(args[2])))"),
        "java": ('if(args[0].equals("overflow"))value.encode(Schema.Protocol.values()[Integer.parseInt(args[1])]);',
                 'if(args[0].equals("overflow")) {value.manual=new byte[31];value.encode(Schema.Protocol.values()[Integer.parseInt(args[1])]);}'),
        "csharp": ('if(args[0]=="overflow")value.Encode((Protocol)int.Parse(args[1]));',
                   'if(args[0]=="overflow") {value.Manual=new byte[31];value.Encode((Protocol)int.Parse(args[1]));}'),
        "go": ('if os.Args[1]=="overflow" {if _,err:=value.Encode(Protocol(p));err!=nil{return nil}}',
               'if os.Args[1]=="overflow" {value.Manual=make([]byte,31);if _,err:=value.Encode(Protocol(p));err!=nil{return nil}}'),
        "rust": ('else if args[1]=="overflow" {assert!(DigestRecord::decode(&bytes,Protocol::Json)?.encode(protocols[p]).is_err());}',
                 'else if args[1]=="overflow" {let mut value=DigestRecord::decode(&bytes,Protocol::Json)?;value.manual=vec![0;31];assert!(value.encode(protocols[p]).is_err());}'),
        "swift": ('if args[1]=="overflow" { _ = try value.encode(Protocol(rawValue:Int(args[2])!)!) }',
                  'if args[1]=="overflow" { value.manual=[UInt8](repeating:0,count:31); _ = try value.encode(Protocol(rawValue:Int(args[2])!)!) }'),
        "kotlin": ('if(args[0]=="overflow")value.encode(Protocol.entries[args[1].toInt()])',
                   'if(args[0]=="overflow") {value.manual=UByteArray(31);value.encode(Protocol.entries[args[1].toInt()])}'),
        "c": ('srl_buffer encoded={0};require(digest_record_encode(&value,(srl_protocol)atoi(argv[2]),&encoded)!=srl_ok);',
              'value.manual.size=31;srl_buffer encoded={0};require(digest_record_encode(&value,(srl_protocol)atoi(argv[2]),&encoded)!=srl_ok);'),
    }
    if language in mutations:
        old, new = mutations[language]
        text = compact.replace_once(text, old, new)
    if language == "swift":
        text = compact.replace_once(text, "do { let value = try DigestRecord.decode",
                                    "do { var value = try DigestRecord.decode")
    if language == "kotlin":
        text = "@file:OptIn(ExperimentalUnsignedTypes::class)\n" + text
    return text


def cases():
    """Test standard sizes, wrong sizes, bad bytes, nesting, truncation, and native framing."""
    for nonzero in (False, True):
        value = record(nonzero)
        yield "verify", 0, json.dumps(value, separators=(",", ":")).encode()
        for protocol in (1, 2, 3):
            yield "verify", protocol, wire(protocol, value)
    baseline = record()
    for protocol in range(4):
        yield "overflow", protocol, json.dumps(baseline).encode()
    for name, size in FIELDS:
        if size == 0:
            continue
        for length in (0, size - 1, size + 1):
            changed = dict(baseline, **{name: [0] * length})
            for protocol in range(4):
                data = json.dumps(changed).encode() if protocol == 0 else wire(protocol, changed)
                yield "reject", protocol, data
    for invalid in (-1, 256, 1.5, True, "1", None):
        yield "reject", 0, json.dumps(dict(baseline, raw=[invalid])).encode()
    for key, value in (("values", [[0] * 15]), ("lookup", [{"key": "entry", "value": [1]}]),
                       ("choice:present", [0] * 17)):
        changed = dict(baseline)
        if key.startswith("choice"):
            del changed["choice:absent"]
        changed[key] = value
        yield "reject", 0, json.dumps(changed).encode()
    for protocol in (1, 2, 3):
        data = wire(protocol, baseline)
        for end in (0, 1, 2, 8, 33, len(data) // 2, len(data) - 1):
            yield "reject", protocol, data[:end]
        yield "reject", protocol, data + b"\0"


def main():
    """Generate each backend, compile its real SDK consumer, and verify independent fixtures."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/digest-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    args.build = args.build.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if language == "javascript" else language):
               args.build / language / examples.OUTPUTS[language] for language in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/digest_language.serializer",
               "--language", ",".join(targets), "--cpp.format=false", "--go.package=main",
               "--kotlin.package="]
    for language, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        command += [f"--{language}.output", destination]
    examples.run(command)
    if "typescript" in languages and "javascript" in languages:
        (args.build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    runner = examples.Runner(args)
    fixtures = args.build / "fixtures"
    fixtures.mkdir(parents=True, exist_ok=True)
    queued = []
    for index, (mode, protocol, data) in enumerate(cases()):
        path = fixtures / f"{index}.bin"
        path.write_bytes(data)
        queued.append((mode, protocol, path))
    for language in languages:
        directory = args.build / language
        source = args.build / "consumers" / language
        source.mkdir(parents=True, exist_ok=True)
        (source / compact.FILES[language]).write_text(consumer(language), encoding="utf-8")
        executable = runner.build(language, "digest", directory, source)
        output = directory / "result"
        examples.run([*executable, runner.path(language, output)])
        produced = [Path(str(output) + f".{p}").read_bytes() for p in range(4)]
        assert json.loads(produced[0]) == record(), language
        for protocol in (1, 2, 3):
            assert produced[protocol] == wire(protocol, record()), (language, protocol, "frozen wire mismatch")
        selected = [case for case in queued if language != "cpp" or case[0] != "overflow"]
        compact.run_cases(runner, language, executable, selected)
        print(f"PASS: {language}, digest storage, nested values, frozen wire, {len(selected)} cases", flush=True)


if __name__ == "__main__":
    main()
