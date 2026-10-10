"""Qualify native generic optionals with independent wire fixtures and real SDKs."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("compact", ROOT / "test/compact_languages.py")
compact = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(compact)
examples = compact.examples

OPTIONAL_INT = ("optional", "int32")
OPTIONAL_STRING = ("optional", "string")
OPTIONAL_UINT = ("optional", "uint32")
DETAIL = ("record", (("name", "string"), ("numbers", ("array", "uint16"))))
FIELDS = (("scalar", OPTIONAL_INT), ("nested", ("optional", OPTIONAL_STRING)),
          ("detail", ("optional", DETAIL)),
          ("values", ("optional", ("array", ("optional", "uint16")))),
          ("lookup", ("optional", ("map", "string", OPTIONAL_UINT))),
          ("labels", ("array", OPTIONAL_STRING)), ("thresholds", ("map", "string", OPTIONAL_UINT)),
          ("color", ("optional", "enum")), ("colors", ("array", ("optional", "enum"))),
          ("result", ("record", (("value", OPTIONAL_STRING),))))


def prefix(value):
    """Encode the independently specified unsigned native count prefix."""
    width = next(n for n in range(1, 5) if value < 1 << (8 * n - 2))
    return ((width - 1) << (8 * width - 2) | value).to_bytes(width, "big")


def record(present=False):
    """Exercise every absence level, empty presence, and optional collection shape."""
    empty = {name: [] for name, _ in FIELDS}
    empty["result"] = {"value": []}
    if not present:
        return empty
    return {"scalar": [-42], "nested": [[]],
            "detail": [{"name": "owned", "numbers": [1, 256]}],
            "values": [[[], [9], [0]]],
            "lookup": [[{"key": "a", "value": []}, {"key": "b", "value": [7]}]],
            "labels": [[], [""], ["seen"]],
            "thresholds": [{"key": "missing", "value": []}, {"key": "present", "value": [0]}],
            "color": ["green"], "colors": [["red"], [], ["blue"]],
            "result": {"value": ["generic"]}}


def encode(value_type, value, protocol, collection=False):
    """Assemble binary payloads without invoking generated or framework codecs."""
    if isinstance(value_type, tuple):
        kind, *arguments = value_type
        if kind == "optional":
            return b"\0" if not value else b"\1" + encode(arguments[0], value[0], protocol, collection)
        if kind == "array":
            return prefix(len(value)) + b"".join(encode(arguments[0], item, protocol, True) for item in value)
        if kind == "map":
            return prefix(len(value)) + b"".join(encode(arguments[0], item["key"], protocol, True) +
                                                 encode(arguments[1], item["value"], protocol, True)
                                                 for item in value)
        output = bytearray()
        for identifier, (name, field_type) in enumerate(arguments[0], 1):
            if protocol == 2:
                output.extend(prefix(identifier))
            elif protocol == 3:
                output.extend(encode("string", name, protocol))
            output.extend(encode(field_type, value[name], protocol))
        if protocol != 1:
            output.append(0)
        return bytes(output)
    if value_type == "string":
        text = value.encode()
        return prefix(len(text)) + text
    if value_type == "enum":
        return encode("string", value, protocol) if protocol == 3 and not collection else prefix(("red", "green", "blue").index(value))
    return struct.pack({"int32": "<i", "uint32": "<I", "uint16": "<H"}[value_type], value)


def wire(protocol, value):
    """Keep JSON optionals canonical and freeze all three binary protocol layouts."""
    return json.dumps(value, separators=(",", ":")).encode() if protocol == 0 else encode(("record", FIELDS), value, protocol)


def consumer(language):
    """Reuse SDK consumers and require exact valid-fixture re-encoding."""
    text = compact.consumer(language).replace("CompactRecord", "OptionalRecord")
    text = text.replace("compact_record", "optional_record").replace("compact integer", "optional")
    replacements = {
        "javascript": ("else if(mode==='verify') rejected=true;", "else if(mode==='verify') { if(Array.from(value.encode(Number(args[3]))).toString()!==Array.from(bytes).toString())throw Error('Optional wire mismatch'); rejected=true; }"),
        "typescript": ("else if(mode==='verify') rejected=true;", "else if(mode==='verify') { if(Array.from(value.encode(Number(args[3]) as Protocol)).toString()!==Array.from(bytes).toString())throw Error('Optional wire mismatch'); rejected=true; }"),
        "python": ("        if mode=='overflow': value.encode(Protocol(int(args[2])))", "        if mode=='overflow': value.encode(Protocol(int(args[2])))\n            elif mode=='verify': assert value.encode(Protocol(int(args[2])))==Path(args[3]).read_bytes()"),
        "java": ('else if(args[0].equals("verify"))rejected=true;', 'else if(args[0].equals("verify")) { if(!java.util.Arrays.equals(bytes,value.encode(Schema.Protocol.values()[Integer.parseInt(args[1])])))throw new AssertionError("Optional wire mismatch"); rejected=true; }'),
        "csharp": ('else if(args[0]=="verify")rejected=true;', 'else if(args[0]=="verify") { if(!bytes.AsSpan().SequenceEqual(value.Encode((Protocol)int.Parse(args[1]))))throw new Exception("Optional wire mismatch"); rejected=true; }'),
        "go": ('if os.Args[1]=="verify" {return err}', 'if os.Args[1]=="verify" {if err!=nil{return err};encoded,err:=value.Encode(Protocol(p));if err!=nil{return err};if !bytes.Equal(data,encoded){return fmt.Errorf("Optional wire mismatch")};return nil}'),
        "rust": ('if args[1]=="verify" {OptionalRecord::decode(&bytes,protocols[p])?;}', 'if args[1]=="verify" {assert_eq!(OptionalRecord::decode(&bytes,protocols[p])?.encode(protocols[p])?,bytes);}'),
        "swift": ('else if args[1]=="verify" { rejected=true }', 'else if args[1]=="verify" { let encoded = try value.encode(Protocol(rawValue:Int(args[2])!)!); precondition(encoded == data); rejected=true }'),
        "kotlin": ('else if(args[0]=="verify")rejected=true', 'else if(args[0]=="verify") {check(value.encode(Protocol.entries[args[1].toInt()]).contentEquals(data));rejected=true}'),
        "cpp": ('try { (void)decode<Protocol>(bytes); return mode=="verify"; }', 'try { const auto value=decode<Protocol>(bytes); if(mode=="verify" && encode<Protocol>(value)!=bytes)throw std::logic_error("Optional wire mismatch"); return mode=="verify"; }'),
        "c": ('require(optional_record_decode(&value,bytes,(size_t)size,(srl_protocol)atoi(argv[2]),NULL)==srl_ok);', 'require(optional_record_decode(&value,bytes,(size_t)size,(srl_protocol)atoi(argv[2]),NULL)==srl_ok);\n      srl_buffer encoded={0};require(optional_record_encode(&value,(srl_protocol)atoi(argv[2]),&encoded)==srl_ok);require(encoded.size==(size_t)size && !memcmp(encoded.data,bytes,(size_t)size));srl_buffer_free(&encoded);'),
    }
    old, new = replacements[language]
    text = compact.replace_once(text, old, new)
    if language in ("javascript", "typescript"):
        text = text.replace("OptionalRecord, Protocol }", "OptionalRecord, Protocol, Limits }")
        protocol = "Number(args[3])" + (" as Protocol" if language == "typescript" else "")
        old = f"OptionalRecord.decode(bytes,mode==='overflow'?Protocol.JSON:{protocol})"
        new = old[:-1] + ",new Limits(undefined,undefined,mode==='elements'?0:undefined,mode==='depth'?1:undefined))"
        text = compact.replace_once(text, old, new)
    elif language == "python":
        text = text.replace("OptionalRecord, Protocol", "OptionalRecord, Protocol, Limits")
        old = "OptionalRecord.decode(Path(args[3]).read_bytes(),Protocol.JSON if mode=='overflow' else Protocol(int(args[2])))"
        text = compact.replace_once(text, old, old[:-1] + ",Limits(max_depth=1) if mode=='depth' else Limits(max_elements=0) if mode=='elements' else None)")
    elif language == "java":
        old = 'Schema.OptionalRecord.decode(bytes,args[0].equals("overflow")?Schema.Protocol.JSON:Schema.Protocol.values()[Integer.parseInt(args[1])])'
        text = compact.replace_once(text, old, old[:-1] + ',new Schema.Limits(67108864,16777216,args[0].equals("elements")?0:1000000,args[0].equals("depth")?1:64))')
    elif language == "csharp":
        old = 'OptionalRecord.Decode(bytes,args[0]=="overflow"?Protocol.JSON:(Protocol)int.Parse(args[1]))'
        text = compact.replace_once(text, old, old[:-1] + ',new Limits(maxElements:args[0]=="elements"?0:1000000,maxDepth:args[0]=="depth"?1:64))')
    elif language == "go":
        old = 'value,err:=DecodeOptionalRecord(data,protocol)'
        new = 'value,err:=DecodeOptionalRecord(data,protocol,func() Limits {limits:=DefaultLimits();if os.Args[1]=="depth" {limits.MaxDepth=1};if os.Args[1]=="elements" {limits.MaxElements=0};return limits}())'
        text = compact.replace_once(text, old, new)
    elif language == "rust":
        text = text.replace('OptionalRecord::decode(&bytes,protocols[p])', 'OptionalRecord::decode_with_limits(&bytes,protocols[p],Limits {max_depth:if args[1]=="depth" {1}else{64},max_elements:if args[1]=="elements" {0}else{1000000},..Limits::default()})')
    elif language == "swift":
        old = 'OptionalRecord.decode(data,args[1]=="overflow" ? .JSON : Protocol(rawValue:Int(args[2])!)!)'
        text = compact.replace_once(text, old, old[:-1] + ',Limits(maxElements:args[1]=="elements" ? 0 : 1000000,maxDepth:args[1]=="depth" ? 1 : 64))')
    elif language == "kotlin":
        old = 'OptionalRecord.decode(data,if(args[0]=="overflow")Protocol.JSON else Protocol.entries[args[1].toInt()])'
        text = compact.replace_once(text, old, old[:-1] + ',Limits(maxElements=if(args[0]=="elements")0 else 1000000,maxDepth=if(args[0]=="depth")1 else 64))')
    elif language == "c":
        text = compact.replace_once(text, "  if(argc>3) {", '  if(argc>3) {\n    srl_limits limits=srl_default_limits();\n    if(strcmp(argv[1],"depth")==0)limits.max_depth=1;\n    if(strcmp(argv[1],"elements")==0)limits.max_elements=0;')
        text = text.replace('(srl_protocol)atoi(argv[2]),NULL)', '(srl_protocol)atoi(argv[2]),&limits)')
    elif language == "cpp":
        text = compact.replace_once(text, "optional_record decode(const std::string& bytes) {", "optional_record decode(const std::string& bytes,const std::string& mode={}) {")
        text = compact.replace_once(text, "  Protocol<rohit::serializer::serialize_type::in> reader{input};", '  rohit::serializer::decode_limits limits{};\n  if(mode=="depth")limits.max_nesting_depth=1;\n  if(mode=="elements")limits.max_collection_elements=0;\n  Protocol<rohit::serializer::serialize_type::in> reader{input,limits};')
        text = compact.replace_once(text, "const auto value=decode<Protocol>(bytes);", "const auto value=decode<Protocol>(bytes,mode);")
    return text


def cases():
    """Reject nulls, multiple payloads, malformed inner values, tags, and exact-input failures."""
    for present in (False, True):
        for protocol in range(4):
            yield "verify", protocol, wire(protocol, record(present))
            yield "depth", protocol, wire(protocol, record(present))
            if present:
                yield "elements", protocol, wire(protocol, record(present))
    baseline = record(True)
    for name, value in (("scalar", None), ("scalar", [1, 2]), ("scalar", [None]),
                        ("nested", [None]), ("nested", [["a", "b"]]), ("detail", [None]),
                        ("values", [[None]]), ("lookup", [[{"key": "missing"}]]),
                        ("labels", [None]), ("thresholds", [{"key": "x", "value": None}]),
                        ("color", ["unknown"]), ("result", {"value": None})):
        changed = copy.deepcopy(baseline)
        changed[name] = value
        yield "reject", 0, wire(0, changed)
    for protocol in (1, 2, 3):
        valid = wire(protocol, baseline)
        first_payload = 0 if protocol == 1 else 1 if protocol == 2 else 7
        for tag in (2, 127, 255):
            changed = bytearray(valid)
            changed[first_payload] = tag
            yield "reject", protocol, bytes(changed)
        for size in range(len(valid)):
            yield "reject", protocol, valid[:size]
        yield "reject", protocol, valid + b"\0"
    yield "reject", 0, wire(0, baseline) + b"[]"


def variant_record():
    """Select the default optional owning variant alternative with an absent value."""
    return {"choice:scalar": []}


def variant_wire(protocol, value):
    """Assemble independent owning-variant selectors and transparent optional payloads."""
    if protocol == 0:
        return json.dumps(value, separators=(",", ":")).encode()
    name, payload = next(iter(value.items()))
    nested = name == "choice:nested"
    field_type = ("optional", OPTIONAL_STRING) if nested else OPTIONAL_INT
    key = prefix(1) if protocol == 2 else encode("string", name, protocol) if protocol == 3 else b""
    selector = b"" if protocol == 3 else prefix(int(nested))
    return key + selector + encode(field_type, payload, protocol, True) + (b"\0" if protocol != 1 else b"")


def variant_cases():
    """Exercise both owning alternatives, every nesting state, and malformed framing."""
    values = (variant_record(), {"choice:scalar": [0]}, {"choice:nested": []},
              {"choice:nested": [[]]}, {"choice:nested": [["payload"]]})
    for value in values:
        for protocol in range(4):
            yield "verify", protocol, variant_wire(protocol, value)
    for value in ({"choice:scalar": None}, {"choice:scalar": [None]}, {"choice:scalar": [1, 2]},
                  {"choice:nested": [None]}, {"choice:nested": [["a", "b"]]}):
        yield "reject", 0, variant_wire(0, value)
    for protocol in (1, 2, 3):
        valid = variant_wire(protocol, values[-1])
        for size in range(len(valid)):
            yield "reject", protocol, valid[:size]
        yield "reject", protocol, valid + b"\0"
        tag_offset = 1 if protocol == 1 else 2 if protocol == 2 else len(encode("string", "choice:nested", protocol))
        changed = bytearray(valid)
        changed[tag_offset] = 2
        yield "reject", protocol, bytes(changed)


def main():
    """Generate all requested targets, compile real SDKs, and exercise independent fixtures."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cpp-library", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "out/optional-languages")
    parser.add_argument("--language", default="all")
    parser.add_argument("--wsl-languages", default="")
    parser.add_argument("--variant-only", action="store_true")
    args = parser.parse_args()
    args.sanitize = False
    args.compiler = args.compiler.resolve()
    if args.cpp_library:
        args.cpp_library = args.cpp_library.resolve()
    args.build = args.build.resolve()
    languages = list(examples.LANGUAGES if args.language == "all" else args.language.split(","))
    targets = {("js" if name == "javascript" else name): args.build / name / examples.OUTPUTS[name] for name in languages}
    if "typescript" in languages and "javascript" not in languages:
        targets["js"] = args.build / "typescript/schema.mjs"
    command = [args.compiler, "--input", ROOT / "test/resources/optional_language.serializer", "--language", ",".join(targets),
               "--cpp.format=false", "--go.package=main", "--kotlin.package="]
    for language, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        command += [f"--{language}.output", destination]
    examples.run(command)
    if "typescript" in languages and "javascript" in languages:
        (args.build / "typescript/schema.mjs").write_bytes(targets["js"].read_bytes())
    runner = examples.Runner(args)
    case_source = variant_cases if args.variant_only else cases
    default_record = variant_record if args.variant_only else record
    encode_record = variant_wire if args.variant_only else wire
    model_name = "OptionalVariantRecord" if args.variant_only else "OptionalRecord"
    fixtures = args.build / ("variant-fixtures" if args.variant_only else "fixtures")
    fixtures.mkdir(parents=True, exist_ok=True)
    queued = []
    for index, (mode, protocol, data) in enumerate(case_source()):
        path = fixtures / f"{index}.bin"
        path.write_bytes(data)
        queued.append((mode, protocol, path))
    for language in languages:
        directory = args.build / language
        source = args.build / "consumers" / language
        source.mkdir(parents=True, exist_ok=True)
        model_consumer = consumer(language).replace("OptionalRecord", model_name).replace("optional_record", "optional_variant_record" if args.variant_only else "optional_record")
        (source / compact.FILES[language]).write_text(model_consumer, encoding="utf-8")
        executable = runner.build(language, "optional", directory, source)
        output = directory / ("variant-result" if args.variant_only else "result")
        examples.run([*executable, runner.path(language, output)])
        produced = [Path(str(output) + f".{p}").read_bytes() for p in range(4)]
        assert json.loads(produced[0]) == default_record(), language
        for protocol in (1, 2, 3):
            assert produced[protocol] == encode_record(protocol, default_record()), (language, protocol, "Frozen wire mismatch")
        compact.run_cases(runner, language, executable, queued)
        feature = "owning optional variants" if args.variant_only else "generic/nested optionals"
        print(f"PASS: {language}, {feature}, independent wire, {len(queued)} cases", flush=True)


if __name__ == "__main__":
    main()
