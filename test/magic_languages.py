"""Compile all requested SDKs and qualify static magic plus per-format omissions."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('examples', ROOT / 'example/run.py')
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)
FILES = {'cpp': 'main.cpp', 'java': 'Main.java', 'javascript': 'main.mjs', 'typescript': 'main.mts',
         'go': 'main.go', 'csharp': 'Program.cs', 'rust': 'main.rs', 'python': 'main.py',
         'swift': 'main.swift', 'kotlin': 'Main.kt', 'c': 'main.c'}
INVALID_JSON = ('{"revision":2}', '{"magic":"wrong","revision":2}',
                '{"magic":"SRL\\u0000UTF8","magic":"SRL\\u0000UTF8","revision":2}',
                '{"magic":"SRL\\u0000UTF8","revision":2,"diagnostic":1}')
INVALID_FLEXIBLE_HEADERS = ('{"magic":"OPTIONAL","value":23}',
                            '{"magic":"wrong","value":23}',
                            '{"magic":"OPTIONAL","magic":"OPTIONAL","value":23}')
VALID_FLEXIBLE_HEADER = '{"value":23,"future":{"nested":[1,true,null]}}'


def flexible_checks(language):
    """Reject excluded known metadata even when unrelated JSON extensions are allowed."""
    bad = ', '.join(json.dumps(value) for value in INVALID_FLEXIBLE_HEADERS)
    valid = json.dumps(VALID_FLEXIBLE_HEADER)
    ordinary = json.dumps('{"json_removed":1}')
    if language in ('javascript', 'typescript'):
        return f'''
{{
  const limits=new Limits(undefined,undefined,undefined,undefined,ReadPolicy.FLEXIBLE);
  if(magicModels.MagicTestOmittedHeader.decode(new TextEncoder().encode({valid}),Protocol.JSON,limits).value!==23)throw Error('Flexible unknown extension rejected');
  for(const text of [{bad}]){{let rejected=false;try{{magicModels.MagicTestOmittedHeader.decode(new TextEncoder().encode(text),Protocol.JSON,limits);}}catch{{rejected=true;}}if(!rejected)throw Error('Flexible excluded magic accepted');}}
  let rejected=false;try{{magicModels.OmissionFirst.decode(new TextEncoder().encode({ordinary}),Protocol.JSON,limits);}}catch{{rejected=true;}}if(!rejected)throw Error('Flexible excluded payload accepted');
}}
'''
    if language == 'python':
        return f'''
limits=Limits(read_policy=ReadPolicy.FLEXIBLE)
assert magic_models.MagicTestOmittedHeader.decode({valid}.encode(),Protocol.JSON,limits).value==23
for text in [{bad}]:
    try: magic_models.MagicTestOmittedHeader.decode(text.encode(),Protocol.JSON,limits)
    except ValueError: pass
    else: raise AssertionError('Flexible excluded magic accepted')
try: magic_models.OmissionFirst.decode({ordinary}.encode(),Protocol.JSON,limits)
except ValueError: pass
else: raise AssertionError('Flexible excluded payload accepted')
'''
    if language in ('java', 'csharp'):
        java = language == 'java'
        limits = 'new Schema.Limits(64*1024*1024,16*1024*1024,1000000,64,Schema.ReadPolicy.FLEXIBLE)' if java else 'new Limits(readPolicy:ReadPolicy.FLEXIBLE)'
        header = 'Schema.MagicTest.OmittedHeader' if java else 'MagicTestOmittedHeader'
        fields = 'Schema.OmissionFirst' if java else 'OmissionFirst'
        protocol = 'Schema.Protocol.JSON' if java else 'Protocol.JSON'
        encode = lambda text: f'{text}.getBytes(java.nio.charset.StandardCharsets.UTF_8)' if java else f'System.Text.Encoding.UTF8.GetBytes({text})'
        decode = 'decode' if java else 'Decode'
        loop = f'for(var text : new String[] {{{bad}}})' if java else f'foreach(var text in new string[] {{{bad}}})'
        boolean = 'boolean' if java else 'bool'
        error = 'new AssertionError' if java else 'new Exception'
        return f'''
{{
  var limits={limits};
  if({header}.{decode}({encode(valid)},{protocol},limits).value!=23)throw {error}("Flexible unknown extension rejected");
  {loop}{{{boolean} rejected=false;try{{{header}.{decode}({encode('text')},{protocol},limits);}}catch(Exception error){{rejected=true;}}if(!rejected)throw {error}("Flexible excluded magic accepted");}}
  {boolean} payloadRejected=false;try{{{fields}.{decode}({encode(ordinary)},{protocol},limits);}}catch(Exception error){{payloadRejected=true;}}if(!payloadRejected)throw {error}("Flexible excluded payload accepted");
}}
'''
    if language == 'go':
        return f'''
{{
 limits:=DefaultLimits();limits.ReadPolicy=FLEXIBLE
 value,err:=DecodeMagicTestOmittedHeader([]byte({valid}),JSON,limits);if err!=nil{{return err}};if value.Value!=23{{return fmt.Errorf("Flexible unknown extension rejected")}}
 for _,text:=range []string{{{bad}}}{{if _,err:=DecodeMagicTestOmittedHeader([]byte(text),JSON,limits);err==nil{{return fmt.Errorf("Flexible excluded magic accepted")}}}}
 if _,err:=DecodeOmissionFirst([]byte({ordinary}),JSON,limits);err==nil{{return fmt.Errorf("Flexible excluded payload accepted")}}
}}
'''
    if language == 'rust':
        return f'''
{{
 let limits=Limits{{read_policy:ReadPolicy::Flexible,..Limits::default()}};
 assert_eq!(MagicTestOmittedHeader::decode_with_limits({valid}.as_bytes(),Protocol::Json,limits)?.value,23);
 for text in [{bad}]{{assert!(MagicTestOmittedHeader::decode_with_limits(text.as_bytes(),Protocol::Json,limits).is_err(),"Flexible excluded magic accepted");}}
 assert!(OmissionFirst::decode_with_limits({ordinary}.as_bytes(),Protocol::Json,limits).is_err(),"Flexible excluded payload accepted");
}}
'''
    if language == 'swift':
        return f'''
do {{
 let limits=Limits(readPolicy:.FLEXIBLE)
 let header=try MagicTestOmittedHeader.decode(Array({valid}.utf8),.JSON,limits)
 if header.value != 23 {{throw SerializerError.invalid("Flexible unknown extension rejected")}}
 for text in [{bad}]{{var rejected=false;do{{_ = try MagicTestOmittedHeader.decode(Array(text.utf8),.JSON,limits)}}catch{{rejected=true}};if !rejected{{throw SerializerError.invalid("Flexible excluded magic accepted")}}}}
 var rejected=false;do{{_ = try OmissionFirst.decode(Array({ordinary}.utf8),.JSON,limits)}}catch{{rejected=true}};if !rejected{{throw SerializerError.invalid("Flexible excluded payload accepted")}}
}}
'''
    if language == 'kotlin':
        return f'''
run {{
 val limits=Limits(readPolicy=ReadPolicy.FLEXIBLE)
 check(MagicTestOmittedHeader.decode({valid}.toByteArray(Charsets.UTF_8),Protocol.JSON,limits).value==23u)
 for(text in arrayOf({bad})){{var rejected=false;try{{MagicTestOmittedHeader.decode(text.toByteArray(Charsets.UTF_8),Protocol.JSON,limits)}}catch(error:Exception){{rejected=true}};check(rejected){{"Flexible excluded magic accepted"}}}}
 var rejected=false;try{{OmissionFirst.decode({ordinary}.toByteArray(Charsets.UTF_8),Protocol.JSON,limits)}}catch(error:Exception){{rejected=true}};check(rejected){{"Flexible excluded payload accepted"}}
}}
'''
    if language == 'cpp':
        return f'''
{{
 if(decode<flexible_magic_json,magic_test::omitted_header>({valid}).value!=23)throw std::runtime_error{{"Flexible unknown extension rejected"}};
 for(const auto* text : {{{bad}}}){{bool rejected=false;try{{(void)decode<flexible_magic_json,magic_test::omitted_header>(text);}}catch(const std::exception&){{rejected=true;}}if(!rejected)throw std::runtime_error{{"Flexible excluded magic accepted"}};}}
 bool rejected=false;try{{(void)decode<flexible_magic_json,omission_first>({ordinary});}}catch(const std::exception&){{rejected=true;}}if(!rejected)throw std::runtime_error{{"Flexible excluded payload accepted"}};
}}
'''
    if language == 'c':
        return f'''
{{
 srl_limits limits=srl_default_limits();limits.read_policy=srl_read_flexible;
 magic_test_omitted_header header={{0}};const char* valid={valid};
 require(magic_test_omitted_header_decode(&header,(const uint8_t*)valid,strlen(valid),srl_json,&limits)==srl_ok&&header.value==23,"Flexible unknown extension rejected");magic_test_omitted_header_free(&header);
 const char* invalid[]={{{bad}}};for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i){{require(magic_test_omitted_header_decode(&header,(const uint8_t*)invalid[i],strlen(invalid[i]),srl_json,&limits)!=srl_ok,"Flexible excluded magic accepted");magic_test_omitted_header_free(&header);}}
 omission_first fields={{0}};const char* omitted={ordinary};require(omission_first_decode(&fields,(const uint8_t*)omitted,strlen(omitted),srl_json,&limits)!=srl_ok,"Flexible excluded payload accepted");omission_first_free(&fields);
}}
'''
    raise ValueError(f'Unsupported qualification language: {language}')


def block(language):
    """Exercise public generated APIs within maintained, independently authored consumers."""
    bad = ', '.join(json.dumps(value) for value in INVALID_JSON)
    if language in ('javascript', 'typescript'):
        return f'''
// Verify required fixed JSON metadata, binary prefixes, and omitted-field defaults.
const magicRecord = new magicModels.MagicTestRecord(); magicRecord.kept = 42; magicRecord.human = 'custom'; magicRecord.diagnostic = 99;
for (const protocol of [Protocol.JSON, Protocol.BINARY_NONE, Protocol.BINARY_INTEGER, Protocol.BINARY_STRING]) {{
  const bytes = magicRecord.encode(protocol); const copy = magicModels.MagicTestRecord.decode(bytes, protocol);
  if (copy.kept !== 42 || copy.human !== (protocol === Protocol.JSON ? 'custom' : 'default') || copy.diagnostic !== (protocol === Protocol.JSON ? 7 : 99)) throw Error('Magic omission defaults');
  if (protocol !== Protocol.JSON) {{
    if (bytes.slice(0,8).toString() !== '83,82,76,0,85,84,70,56') throw Error('Raw magic prefix');
    const bad = bytes.slice(); bad[0] ^= 1; let failed = false;
    try {{ magicModels.MagicTestRecord.decode(bad,protocol); }} catch {{ failed = true; }} if (!failed) throw Error('Incorrect magic accepted');
    for (let size=0;size<8;++size) {{ let rejected=false;try {{ magicModels.MagicTestRecord.decode(bytes.slice(0,size),protocol); }} catch {{ rejected=true; }}if(!rejected)throw Error('Truncated magic accepted'); }}
  }}
  writeFileSync(process.argv[3] + '.magic.' + protocol, bytes);
  const fields = new magicModels.OmissionFirst(); fields.jsonRemoved=100; fields.positionalRemoved=200; fields.integerRemoved=300; fields.stringRemoved=400; fields.visible=500;
  const fieldsBytes = fields.encode(protocol); const restored = magicModels.OmissionFirst.decode(fieldsBytes,protocol);
  if (restored.jsonRemoved !== (protocol===Protocol.JSON?7:100) || restored.positionalRemoved !== (protocol===Protocol.BINARY_NONE?9:200) || restored.integerRemoved !== (protocol===Protocol.BINARY_INTEGER?11:300) || restored.stringRemoved !== (protocol===Protocol.BINARY_STRING?13:400)) throw Error('Ordinary omission defaults');
  writeFileSync(process.argv[3] + '.fields.' + protocol, fieldsBytes);
}}
for (const text of [{bad}]) {{ let rejected=false;try {{ magicModels.MagicTestRecord.decode(new TextEncoder().encode(text),Protocol.JSON); }} catch {{ rejected=true; }}if(!rejected)throw Error('Invalid magic JSON accepted'); }}
const excluded = new magicModels.MagicTestOmittedHeader(); excluded.value=23;
if (new TextDecoder().decode(excluded.encode(Protocol.JSON)) !== '{{"value":23}}') throw Error('JSON header omission');
for(const protocol of [Protocol.BINARY_NONE,Protocol.BINARY_INTEGER,Protocol.BINARY_STRING]) {{ const bytes=excluded.encode(protocol); if (magicModels.MagicTestOmittedHeader.decode(bytes,protocol).value!==23) throw Error('Header omission decode'); if((bytes[0]===79)===(protocol===Protocol.BINARY_INTEGER))throw Error('Header omitted in wrong protocol'); }}
'''
    if language == 'python':
        return f'''
# Validate immutable metadata with actual generated encoders and exact decoders.
value = magic_models.MagicTestRecord(); value.kept=42; value.human='custom'; value.diagnostic=99
for protocol in Protocol:
    data=value.encode(protocol); copy=magic_models.MagicTestRecord.decode(data,protocol)
    assert (copy.kept,copy.human,copy.diagnostic)==(42,'custom' if protocol==Protocol.JSON else 'default',7 if protocol==Protocol.JSON else 99)
    if protocol != Protocol.JSON:
        assert data[:8]==b'SRL\\0UTF8'
        for bad in [bytes([data[0]^1])+data[1:], *(data[:size] for size in range(8))]:
            try: magic_models.MagicTestRecord.decode(bad,protocol)
            except ValueError: pass
            else: raise AssertionError('Invalid magic accepted')
    Path(sys.argv[2]+'.magic.'+str(int(protocol))).write_bytes(data)
    fields=magic_models.OmissionFirst(); fields.json_removed=100;fields.positional_removed=200;fields.integer_removed=300;fields.string_removed=400;fields.visible=500
    data=fields.encode(protocol); copy=magic_models.OmissionFirst.decode(data,protocol)
    assert (copy.json_removed,copy.positional_removed,copy.integer_removed,copy.string_removed)==(7 if protocol==Protocol.JSON else 100,9 if protocol==Protocol.BINARY_NONE else 200,11 if protocol==Protocol.BINARY_INTEGER else 300,13 if protocol==Protocol.BINARY_STRING else 400)
    Path(sys.argv[2]+'.fields.'+str(int(protocol))).write_bytes(data)
for text in [{bad}]:
    try: magic_models.MagicTestRecord.decode(text.encode(),Protocol.JSON)
    except ValueError: pass
    else: raise AssertionError('Invalid JSON magic accepted')
value=magic_models.MagicTestOmittedHeader();value.value=23
assert value.encode(Protocol.JSON)==b'{{"value":23}}'
for protocol in (Protocol.BINARY_NONE,Protocol.BINARY_INTEGER,Protocol.BINARY_STRING):
    data=value.encode(protocol);assert magic_models.MagicTestOmittedHeader.decode(data,protocol).value==23
    assert data.startswith(b'OPTIONAL') == (protocol!=Protocol.BINARY_INTEGER)
'''
    if language in ('java', 'csharp'):
        java = language == 'java'
        prefix = 'Schema.' if java else ''
        encode, decode = ('encode', 'decode') if java else ('Encode', 'Decode')
        declaration = 'for (var protocol : Schema.Protocol.values()) {' if java else 'foreach (var protocol in Enum.GetValues<Protocol>()) {'
        save = 'Files.write(Path.of(args[1]+".magic."+protocol.ordinal()),bytes);' if java else 'File.WriteAllBytes(args[1]+".magic."+(int)protocol,bytes);'
        save_fields = save.replace('.magic.', '.fields.').replace(',bytes', ',fieldsBytes')
        condition = f'!copy.human.equals(protocol=={prefix}Protocol.JSON?"custom":"default")' if java else f'copy.human != (protocol=={prefix}Protocol.JSON?"custom":"default")'
        return f'''
// Validate fixed metadata and default restoration for every public native codec.
{{
  var value=new {prefix}MagicTestRecord(); value.kept=42; value.human="custom"; value.diagnostic=99;
  {declaration}
    var bytes=value.{encode}(protocol);var copy={prefix}MagicTestRecord.{decode}(bytes,protocol);
    if(copy.kept!=42 || {condition} || copy.diagnostic!=(protocol=={prefix}Protocol.JSON?7:99)) throw new Exception("Magic omission defaults");
    if(protocol!={prefix}Protocol.JSON) {{
      if(bytes[0]!=83 || bytes[3]!=0 || bytes[7]!=56)throw new Exception("Raw prefix");
      var bad=(byte[])bytes.Clone(); bad[0]^=1;
      bool rejected=false;
      try {{ {prefix}MagicTestRecord.{decode}(bad,protocol); }}catch(Exception error) {{ rejected=true; }}
      if(!rejected)throw new Exception("Incorrect magic accepted");
    }}
    {save}
    var fields=new {prefix}OmissionFirst();fields.jsonRemoved=100;fields.positionalRemoved=200;fields.integerRemoved=300;fields.stringRemoved=400;fields.visible=500;
    var fieldsBytes=fields.{encode}(protocol);var restored={prefix}OmissionFirst.{decode}(fieldsBytes,protocol);
    if(restored.jsonRemoved!=(protocol=={prefix}Protocol.JSON?7:100) || restored.positionalRemoved!=(protocol=={prefix}Protocol.BINARY_NONE?9:200) || restored.integerRemoved!=(protocol=={prefix}Protocol.BINARY_INTEGER?11:300) || restored.stringRemoved!=(protocol=={prefix}Protocol.BINARY_STRING?13:400))throw new Exception("Ordinary omission defaults");
    {save_fields}
  }}
  foreach(var text in new string[] {{{bad}}}) {{ bool rejected=false;try {{ {prefix}MagicTestRecord.{decode}(System.Text.Encoding.UTF8.GetBytes(text),{prefix}Protocol.JSON); }}catch(Exception error) {{ rejected=true; }}if(!rejected)throw new Exception("Invalid JSON magic accepted"); }}
}}
'''.replace('var bad=(byte[])bytes.Clone()', 'var bad=bytes.clone()' if java else 'var bad=(byte[])bytes.Clone()').replace('bool rejected', 'boolean rejected' if java else 'bool rejected').replace('foreach(var text in new string[]', 'for(var text : new String[]' if java else 'foreach(var text in new string[]').replace('System.Text.Encoding.UTF8.GetBytes(text)', 'text.getBytes(java.nio.charset.StandardCharsets.UTF_8)' if java else 'System.Text.Encoding.UTF8.GetBytes(text)')
    if language == 'go':
        return f'''
// Verify static bytes and excluded payload fields using actual generated codecs.
{{
 value:=NewMagicTestRecord();value.Kept=42;value.Human="custom";value.Diagnostic=99
 for _,protocol:=range []Protocol{{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING}} {{
  data,err:=value.Encode(protocol);if err!=nil {{return err}};copy,err:=DecodeMagicTestRecord(data,protocol);if err!=nil {{return err}}
  human:="default";diagnostic:=uint16(99);if protocol==JSON {{human="custom";diagnostic=7}}
  if copy.Kept!=42 || copy.Human!=human || copy.Diagnostic!=diagnostic {{return fmt.Errorf("Magic omission defaults")}}
  if protocol!=JSON {{if string(data[:8])!="SRL\\x00UTF8" {{return fmt.Errorf("Raw prefix")}};bad:=append([]byte(nil),data...);bad[0]^=1;if _,err:=DecodeMagicTestRecord(bad,protocol);err==nil {{return fmt.Errorf("Incorrect magic accepted")}}}}
  if err:=os.WriteFile(os.Args[2]+fmt.Sprintf(".magic.%d",protocol),data,0600);err!=nil {{return err}}
  fields:=NewOmissionFirst();fields.JsonRemoved=100;fields.PositionalRemoved=200;fields.IntegerRemoved=300;fields.StringRemoved=400;fields.Visible=500
  data,err=fields.Encode(protocol);if err!=nil {{return err}};restored,err:=DecodeOmissionFirst(data,protocol);if err!=nil {{return err}}
  expected:=[4]uint32{{100,200,300,400}};expected[protocol]=[4]uint32{{7,9,11,13}}[protocol]
  if restored.JsonRemoved!=expected[0] || restored.PositionalRemoved!=expected[1] || restored.IntegerRemoved!=expected[2] || restored.StringRemoved!=expected[3] {{return fmt.Errorf("Ordinary omission defaults")}}
  if err:=os.WriteFile(os.Args[2]+fmt.Sprintf(".fields.%d",protocol),data,0600);err!=nil {{return err}}
 }}
 for _,text:=range []string{{{bad}}} {{if _,err:=DecodeMagicTestRecord([]byte(text),JSON);err==nil {{return fmt.Errorf("Invalid JSON magic accepted")}}}}
}}
'''
    if language == 'rust':
        return f'''
// Check exact immutable metadata and native omission defaults with generated readers.
{{
 let mut value=MagicTestRecord::default();value.kept=42;value.human="custom".into();value.diagnostic=99;
 for (p,protocol) in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString].iter().enumerate() {{
  let data=value.encode(*protocol)?;let copy=MagicTestRecord::decode(&data,*protocol)?;
  assert_eq!(copy.kept,42);assert_eq!(copy.human,if p==0{{"custom"}}else{{"default"}});assert_eq!(copy.diagnostic,if p==0{{7}}else{{99}});
  if p!=0 {{assert_eq!(&data[..8],b"SRL\\0UTF8");let mut bad=data.clone();bad[0]^=1;assert!(MagicTestRecord::decode(&bad,*protocol).is_err());}}
  fs::write(format!("{{}}.magic.{{p}}",args[2]),data)?;
  let mut fields=OmissionFirst::default();fields.json_removed=100;fields.positional_removed=200;fields.integer_removed=300;fields.string_removed=400;fields.visible=500;
  let data=fields.encode(*protocol)?;let restored=OmissionFirst::decode(&data,*protocol)?;let mut wanted=[100,200,300,400];wanted[p]=[7,9,11,13][p];
  assert_eq!([restored.json_removed,restored.positional_removed,restored.integer_removed,restored.string_removed],wanted);
  fs::write(format!("{{}}.fields.{{p}}",args[2]),data)?;
 }}
 for text in [{bad}] {{assert!(MagicTestRecord::decode(text.as_bytes(),Protocol::Json).is_err());}}
}}
'''
    if language in ('swift', 'kotlin'):
        swift = language == 'swift'
        if swift:
            return f'''
// Check fixed-byte prefixes and omission defaults through every generated protocol.
var magicValue=MagicTestRecord();magicValue.kept=42;magicValue.human="custom";magicValue.diagnostic=99
for p in 0..<4 {{
 let protocolValue=Protocol(rawValue:p)!;let data=try magicValue.encode(protocolValue);let copy=try MagicTestRecord.decode(data,protocolValue)
 precondition(copy.kept==42 && copy.human==(p==0 ? "custom":"default") && copy.diagnostic==(p==0 ? 7:99))
 if p != 0 {{precondition(Array(data.prefix(8))==[83,82,76,0,85,84,70,56]);var bad=data;bad[0]^=1;var rejected=false;do {{_ = try MagicTestRecord.decode(bad,protocolValue)}}catch{{rejected=true}};precondition(rejected)}}
 try Data(data).write(to:URL(fileURLWithPath:args[2]+".magic.\\(p)"))
 var fields=OmissionFirst();fields.jsonRemoved=100;fields.positionalRemoved=200;fields.integerRemoved=300;fields.stringRemoved=400;fields.visible=500
 let fieldsBytes=try fields.encode(protocolValue);let restored=try OmissionFirst.decode(fieldsBytes,protocolValue);var wanted:[UInt32]=[100,200,300,400];wanted[p]=[7,9,11,13][p]
 precondition([restored.jsonRemoved,restored.positionalRemoved,restored.integerRemoved,restored.stringRemoved]==wanted)
 try Data(fieldsBytes).write(to:URL(fileURLWithPath:args[2]+".fields.\\(p)"))
}}
for text in [{bad}] {{var rejected=false;do {{_ = try MagicTestRecord.decode(Array(text.utf8),.JSON)}}catch{{rejected=true}};precondition(rejected)}}
'''
        return f'''
// Verify immutable metadata and defaults with the actual generated native codecs.
val magicValue=MagicTestRecord();magicValue.kept=42u;magicValue.human="custom";magicValue.diagnostic=99u
for(protocol in Protocol.entries) {{
 val p=protocol.ordinal;val data=magicValue.encode(protocol);val copy=MagicTestRecord.decode(data,protocol)
 check(copy.kept==42u && copy.human==(if(p==0)"custom" else "default") && copy.diagnostic==(if(p==0)7u.toUShort() else 99u.toUShort()))
 if(p!=0) {{check(data.sliceArray(0 until 8).contentEquals(byteArrayOf(83,82,76,0,85,84,70,56)));val bad=data.copyOf();bad[0]=(bad[0].toInt() xor 1).toByte();var rejected=false;try{{MagicTestRecord.decode(bad,protocol)}}catch(error:IllegalArgumentException){{rejected=true}};check(rejected)}}
 File(args[1]+".magic.$p").writeBytes(data)
 val fields=OmissionFirst();fields.jsonRemoved=100u;fields.positionalRemoved=200u;fields.integerRemoved=300u;fields.stringRemoved=400u;fields.visible=500u
 val bytes=fields.encode(protocol);val restored=OmissionFirst.decode(bytes,protocol);val wanted=mutableListOf(100u,200u,300u,400u);wanted[p]=listOf(7u,9u,11u,13u)[p]
 check(listOf(restored.jsonRemoved,restored.positionalRemoved,restored.integerRemoved,restored.stringRemoved)==wanted)
 File(args[1]+".fields.$p").writeBytes(bytes)
}}
for(text in listOf({bad})) {{var rejected=false;try{{MagicTestRecord.decode(text.toByteArray(),Protocol.JSON)}}catch(error:IllegalArgumentException){{rejected=true}};check(rejected)}}
'''
    if language == 'cpp':
        return '''
    // Verify independent binary prefixes and compile-time format exclusions.
    {
      magic_test::record value{}; value.kept=42; value.human="custom"; value.diagnostic=99;
      const std::string records[]{encode<rohit::serializer::json>(value), encode<rohit::serializer::binary_none>(value), encode<rohit::serializer::binary_integer>(value), encode<rohit::serializer::binary_string>(value)};
      omission_first fields{}; fields.json_removed=100; fields.positional_removed=200; fields.integer_removed=300; fields.string_removed=400; fields.visible=500;
      const std::string entries[]{encode<rohit::serializer::json>(fields), encode<rohit::serializer::binary_none>(fields), encode<rohit::serializer::binary_integer>(fields), encode<rohit::serializer::binary_string>(fields)};
      for(int p=0;p<4;++p) {
        if(p && records[p].substr(0,8)!=std::string("SRL\\0UTF8",8))throw std::runtime_error("Raw prefix");
        std::ofstream output(std::string(argv[2])+".magic."+std::to_string(p),std::ios::binary);output.write(records[p].data(),static_cast<std::streamsize>(records[p].size()));
        std::ofstream omitted(std::string(argv[2])+".fields."+std::to_string(p),std::ios::binary);omitted.write(entries[p].data(),static_cast<std::streamsize>(entries[p].size()));
      }
      const auto positional=decode<rohit::serializer::binary_none,magic_test::record>(records[1]);
      if(positional.human!="default" || positional.diagnostic!=99)throw std::runtime_error("Omission defaults");
      const auto json=decode<rohit::serializer::json,magic_test::record>(records[0]);
      if(json.human!="custom" || json.diagnostic!=7)throw std::runtime_error("JSON omission defaults");
    }
'''
    if language == 'c':
        return '''
  /* Verify actual native prefixes and explicit default restoration. */
  {
    magic_test_record value={0}; require(magic_test_record_init(&value)==srl_ok,"Magic init");value.kept=42;value.diagnostic=99;require(srl_string_set_cstr(&value.human,"custom")==srl_ok,"Magic text");
    omission_first fields={0};require(omission_first_init(&fields)==srl_ok,"Omission init");fields.json_removed=100;fields.positional_removed=200;fields.integer_removed=300;fields.string_removed=400;fields.visible=500;
    for(int p=0;p<4;++p) {
      srl_buffer data={0};require(magic_test_record_encode(&value,(srl_protocol)p,&data)==srl_ok,"Magic encode");
      magic_test_record copy={0};require(magic_test_record_decode(&copy,data.data,data.size,(srl_protocol)p,NULL)==srl_ok,"Magic decode");require(copy.kept==42 && copy.diagnostic==(p?99:7) && srl_string_equal(&copy.human,p?"default":"custom"),"Magic defaults");magic_test_record_free(&copy);
      if(p){require(data.size>=8 && memcmp(data.data,"SRL\\0UTF8",8)==0,"Raw prefix");data.data[0]^=1;require(magic_test_record_decode(&copy,data.data,data.size,(srl_protocol)p,NULL)!=srl_ok,"Wrong magic accepted");data.data[0]^=1;magic_test_record_free(&copy);}
      char path[4096];snprintf(path,sizeof(path),"%s.magic.%d",argv[2],p);FILE* output=fopen(path,"wb");require(output!=NULL,"Magic output");require(fwrite(data.data,1,data.size,output)==data.size,"Magic write");fclose(output);srl_buffer_free(&data);
      require(omission_first_encode(&fields,(srl_protocol)p,&data)==srl_ok,"Omission encode");omission_first restored={0};require(omission_first_decode(&restored,data.data,data.size,(srl_protocol)p,NULL)==srl_ok,"Omission decode");require(restored.json_removed==(p==0?7:100) && restored.positional_removed==(p==1?9:200) && restored.integer_removed==(p==2?11:300) && restored.string_removed==(p==3?13:400),"Omission defaults");omission_first_free(&restored);
      snprintf(path,sizeof(path),"%s.fields.%d",argv[2],p);output=fopen(path,"wb");require(output!=NULL,"Omission output");require(fwrite(data.data,1,data.size,output)==data.size,"Omission write");fclose(output);srl_buffer_free(&data);
    }magic_test_record_free(&value);omission_first_free(&fields);
  }
'''
    raise ValueError(language)


def consumer(language, text):
    """Add isolated schema checks to the maintained versioning consumer without replacing it."""
    added = block(language) + flexible_checks(language)
    if language == 'cpp':
        text = text.replace('// Compatibility accepts', '''// Flexible JSON still rejects excluded known metadata.
template <rohit::serializer::serialize_type Direction>
using flexible_magic_json = rohit::serializer::json<Direction, rohit::stream,
                                                 rohit::serializer::read_policy::flexible>;

// Compatibility accepts''', 1)
    unicode_output = {
        'java': 'Files.write(Path.of(args[1]+".unicode"),new Schema.UnicodeMagic().encode(Schema.Protocol.JSON));',
        'csharp': 'File.WriteAllBytes(args[1]+".unicode",new UnicodeMagic().Encode(Protocol.JSON));',
        'go': '{ data,err:=NewUnicodeMagic().Encode(JSON);if err!=nil{return err};if err:=os.WriteFile(os.Args[2]+".unicode",data,0600);err!=nil{return err} }',
        'rust': 'fs::write(format!("{}.unicode",args[2]),UnicodeMagic::default().encode(Protocol::Json)?)?;',
        'swift': 'try Data(UnicodeMagic().encode(.JSON)).write(to:URL(fileURLWithPath:args[2]+".unicode"))',
        'kotlin': 'File(args[1]+".unicode").writeBytes(UnicodeMagic().encode(Protocol.JSON))',
        'python': 'Path(sys.argv[2]+".unicode").write_bytes(magic_models.UnicodeMagic().encode(Protocol.JSON))',
        'javascript': 'writeFileSync(process.argv[3]+".unicode",new magicModels.UnicodeMagic().encode(Protocol.JSON));',
        'typescript': 'writeFileSync(process.argv[3]+".unicode",new magicModels.UnicodeMagic().encode(Protocol.JSON));',
        'cpp': '{ const auto bytes=encode<rohit::serializer::json>(unicode_magic{});std::ofstream output(std::string(argv[2])+".unicode",std::ios::binary);output.write(bytes.data(),static_cast<std::streamsize>(bytes.size())); }',
        'c': '{ unicode_magic value={0};require(unicode_magic_init(&value)==srl_ok,"Unicode init");srl_buffer bytes={0};require(unicode_magic_encode(&value,srl_json,&bytes)==srl_ok,"Unicode encode");char path[4096];snprintf(path,sizeof(path),"%s.unicode",argv[2]);FILE* output=fopen(path,"wb");require(output!=NULL,"Unicode output");require(fwrite(bytes.data,1,bytes.size,output)==bytes.size,"Unicode write");fclose(output);srl_buffer_free(&bytes);unicode_magic_free(&value); }',
    }
    comment_prefix = '#' if language == 'python' else '//'
    added += f'\n{comment_prefix} Verify UTF-8 metadata and non-public static declaration generation.\n'
    added += unicode_output[language] + '\n'
    if language == 'java':
        added = added.replace('throw new Exception(', 'throw new AssertionError(').replace(
            'Schema.MagicTestRecord', 'Schema.MagicTest.Record')
    if language == 'csharp':
        added = added.replace('catch(Exception error)', 'catch(Exception)')
        added = added.replace('var value=', 'var magicValue=').replace('value.', 'magicValue.')
        for name in ('kept', 'human', 'diagnostic', 'jsonRemoved', 'positionalRemoved',
                     'integerRemoved', 'stringRemoved', 'visible', 'value'):
            added = added.replace('.' + name, '.' + name[0].upper() + name[1:])
    if language == 'c':
        for expression in ('p==0?7:100', 'p==1?9:200', 'p==2?11:300', 'p==3?13:400'):
            left, values = expression.split('?')
            first, second = values.split(':')
            added = added.replace(expression, left + '?' + first + 'u:' + second + 'u')
    if language in ('java', 'csharp'):
        marker = 'public static void main(String[] args) throws Exception {' if language == 'java' else 'private static void Main(string[] args) {'
        return text.replace(marker, marker + '\n' + added, 1)
    if language == 'go':
        return text.replace('func run() error {', 'func run() error {\n' + added, 1)
    if language == 'rust':
        return text.replace('let args: Vec<String> = env::args().collect();', 'let args: Vec<String> = env::args().collect();\n' + added, 1)
    if language == 'kotlin':
        return text.replace('fun main(args: Array<String>) {', 'fun main(args: Array<String>) {\n' + added, 1)
    if language in ('cpp', 'c'):
        marker = 'int main(int argc, char** argv) {'
        return text.replace(marker, marker + '\n' + added, 1)
    if language == 'python':
        return text + '\nimport schema as magic_models\n' + added
    if language in ('javascript', 'typescript'):
        return text + '\nimport * as magicModels from "./schema.mjs";\n' + added
    return text + '\n' + added


def main():
    """Generate, compile, and compare artifacts from the selected real language toolchains."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--cpp-library', type=Path)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/magic-languages')
    parser.add_argument('--language', default='all')
    parser.add_argument('--wsl-languages', default='')
    args = parser.parse_args(); args.sanitize = False
    args.compiler = args.compiler.resolve(); args.build = args.build.resolve()
    languages = list(examples.LANGUAGES if args.language == 'all' else args.language.split(','))
    targets = {('js' if name == 'javascript' else name): args.build / name / examples.OUTPUTS[name] for name in languages}
    if 'typescript' in languages and 'javascript' not in languages:
        targets['js'] = args.build / 'typescript/schema.mjs'
    command = [args.compiler, '--input', ROOT / 'test/resources/magic_language.serializer', '--language', ','.join(targets), '--cpp.format=false', '--go.package=main', '--kotlin.package=']
    for name, destination in targets.items():
        destination.parent.mkdir(parents=True, exist_ok=True); command += [f'--{name}.output', destination]
    examples.run(command)
    if 'typescript' in languages and 'javascript' in languages:
        (args.build / 'typescript/schema.mjs').write_bytes(targets['js'].read_bytes())
    runner = examples.Runner(args)
    artifacts = []
    for language in languages:
        source_directory = args.build / 'consumers' / language; source_directory.mkdir(parents=True, exist_ok=True)
        name = FILES[language]
        text = (ROOT / 'example' / language / 'versioning' / name).read_text(encoding='utf-8')
        (source_directory / name).write_text(consumer(language, text), encoding='utf-8')
        executable = runner.build(language, 'magic', args.build / language, source_directory)
        output = args.build / language / 'result.json'
        examples.run([*executable, runner.path(language, ROOT / 'example/schemas/versioning/fixture.json'), runner.path(language, output)])
        assert json.loads(output.read_bytes()) == {'version': 10, 'id': 42, 'enabled': True, 'name': 'Ada'}
        assert json.loads(Path(str(output) + '.unicode').read_bytes()) == {'magic': 'UTF8é🚀', 'value': 5}
        for p in range(4):
            magic = Path(str(output) + f'.magic.{p}').read_bytes()
            fields = Path(str(output) + f'.fields.{p}').read_bytes()
            if p == 0:
                assert json.loads(magic) == {'magic': 'SRL\0UTF8', 'revision': 2, 'kept': 42, 'human': 'custom'}
                assert json.loads(fields) == {'positional_removed': 200, 'integer_removed': 300, 'string_removed': 400, 'visible': 500}
            else:
                assert magic.startswith(b'SRL\0UTF8')
                if p == 1:
                    assert magic == b'SRL\0UTF8' + struct.pack('<HIH', 2, 42, 99)
                    assert fields == struct.pack('<IIII', 100, 300, 400, 500)
                if artifacts:
                    assert magic == artifacts[0][p][0] and fields == artifacts[0][p][1], (language, p, 'binary mismatch')
        artifacts.append([(Path(str(output) + f'.magic.{p}').read_bytes(), Path(str(output) + f'.fields.{p}').read_bytes()) for p in range(4)])
        print(f'PASS: {language} magic, omissions, native byte agreement and existing version migration', flush=True)


if __name__ == '__main__':
    main()
