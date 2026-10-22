# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Exercise C/Rust namespace companions across wrappers, nil, groups and lifecycle metadata."""
import argparse
from pathlib import Path
import shutil
import tempfile

import native_xml_namespace_test as shared

ROOT, sdk, xml_runner = shared.ROOT, shared.sdk, shared.xml_runner
ORACLE_XSD = False
ORACLE_PRESENTATION = None
MODEL = '''serializer version 1.21.0;
namespace graph {
  xml(namespace("urn:graph"));
  class q {
    public string value xml(text,qname);
    public string name xml(attribute,qname,namespace("urn:attribute"),name("名称"));
  }
  class wrapped { public array<q> values; }
  class nullable { public optional<q> value; }
  class scalar { public optional<string> value xml(qname); }
  class life {
    public version uint16 ver { 3 } compatibility { 1 };
    public string value xml(qname);
  }
  class marked {
    private magic (99) { 'SRL' };
    public string value xml(qname);
  }
  class fragment { private variant(string=text,q=q) value; }
  class mixed { public array<fragment> content; }
  class other { public uint32 id xml(attribute); }
  class ordered_fragment { private variant(q=q,other=other) value; }
  class ordered { public array<ordered_fragment> content; }
}
namespace all {
  using schema "all.xsd";
  xml(namespace("urn:graph"));
  class record { public string first; public string second; }
}
namespace wrapped {
  using schema "wrapped.xsd";
  xml(namespace("urn:graph"));
  class root { public optional<array<string>> values; }
}
'''
CONFIG = '''[xml.class.graph::wrapped]
wrapper.values=values
name.values=item
[xml.class.graph::nullable]
presence.value=nil
[xml.class.graph::scalar]
presence.value=nil
[xml.class.graph::mixed]
mixed=content
[xml.class.graph::ordered]
ordered=content
[xml.class.wrapped::root]
presence.values=nil
'''
XSD_HEAD = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:graph" elementFormDefault="qualified">'
ALL_XSD = XSD_HEAD + '''<xs:element name="record"><xs:complexType><xs:all>
<xs:element name="first" type="xs:QName"/><xs:element name="second" type="xs:QName"/>
</xs:all></xs:complexType></xs:element></xs:schema>'''
WRAPPED_XSD = XSD_HEAD + '''<xs:element name="root"><xs:complexType><xs:sequence>
<xs:element name="values" nillable="true"><xs:complexType><xs:sequence>
<xs:element name="item" type="xs:QName" minOccurs="0" maxOccurs="unbounded"/>
</xs:sequence></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element></xs:schema>'''
KINDS = [('wrapped', 'GraphWrapped', 'graph_wrapped'), ('nullable', 'GraphNullable', 'graph_nullable'),
         ('scalar', 'GraphScalar', 'graph_scalar'), ('life', 'GraphLife', 'graph_life'),
         ('marked', 'GraphMarked', 'graph_marked'),
         ('mixed', 'GraphMixed', 'graph_mixed'), ('ordered', 'GraphOrdered', 'graph_ordered'),
         ('all', 'AllRecord', 'all_record'), ('list', 'WrappedRoot', 'wrapped_root')]


def probes():
    """Keep each occurrence's shadowed, default and explicitly absent namespace independent."""
    bindings = ' xmlns:g="urn:graph" xmlns:a="urn:attribute" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"'
    def q(name, uri):
        """Render qualified attributes and QName text using one local alias."""
        return f'<g:{name} xmlns:p="{uri}" a:名称="p:Attribute">p:Text</g:{name}>'
    values = [
        ('wrapped', '<g:wrapped'+bindings+'><g:values>'+q('item', 'urn:first')+q('item', 'urn:second')+'</g:values></g:wrapped>'),
        ('nullable', '<g:nullable'+bindings+'>'+q('value', 'urn:nil-record')+'</g:nullable>'),
        ('nullable', '<g:nullable'+bindings+'><g:value xsi:nil=" true "/></g:nullable>'),
        ('scalar', '<g:scalar'+bindings+'><g:value xmlns:p="urn:scalar">p:Text</g:value></g:scalar>'),
        ('scalar', '<g:scalar'+bindings+'><g:value xsi:nil="1"/></g:scalar>'),
        ('life', '<g:life'+bindings+' xmlns:m="urn:serializer:xml:model" m:revision="3"><g:value xmlns:p="urn:life">p:Text</g:value></g:life>'),
        ('marked', '<g:marked'+bindings+' xmlns:m="urn:serializer:xml:model" m:magic="53524c"><g:value xmlns:p="urn:marked">p:Text</g:value></g:marked>'),
        ('mixed', '<g:mixed'+bindings+'>before'+q('q', 'urn:first')+'between'+q('q', 'urn:second')+'after</g:mixed>'),
        ('ordered', '<g:ordered'+bindings+'>'+q('q', 'urn:first')+'<g:other id="7"/>'+q('q', 'urn:second')+'</g:ordered>'),
        ('all', '<g:record'+bindings+'><g:second xmlns:p="urn:second">p:Text</g:second><g:first xmlns:p="urn:first">p:Text</g:first></g:record>'),
        ('list', '<g:root'+bindings+'><g:values><g:item xmlns:p="urn:first">p:Text</g:item><g:item xmlns="">Local</g:item></g:values></g:root>'),
        ('list', '<g:root'+bindings+'><g:values xsi:nil="true"/></g:root>'),
    ]
    result = [(kind, document, True) for kind, document in values]
    for kind, document in values:
        if 'p:Text' in document:
            result.append((kind, document.replace('p:Text', 'missing:Text', 1), False))
    result.append(('scalar', '<g:scalar'+bindings+'><g:value xsi:nil="true">bad</g:value></g:scalar>', False))
    result.append(('marked', next(document for kind, document in values if kind == 'marked').replace('53524c', '53524C'), False))
    return result


def identity(document):
    """Resolve only namespace-valued payloads, independently of generated companion metadata."""
    from lxml import etree
    result = []
    root = etree.fromstring(document)
    for node in root.iter():
        if node.text and node.text.strip() and node.tag not in ('{urn:graph}mixed',):
            result.append((node.tag, '#text', shared.expanded(node, node.text, False)))
        for name, value in node.attrib.items():
            if name.startswith('{urn:attribute}'):
                result.append((node.tag, name, shared.expanded(node, value, False)))
    # all groups may legitimately emit their declaration order.
    return sorted(result) if root.tag == '{urn:graph}record' else result


def rust_consumer():
    """Generate concrete typed branches so companions cannot fall back to a diagnostic parser."""
    branches = []
    for kind, native, _ in KINDS:
        branches.append(f'''"{kind}" => {{
let loaded={native}::decode_xml_document(&input,limits,None,&options);
let document=match loaded{{Ok(value)=>{{assert!(valid,"Invalid input accepted: {{path}}");value}},Err(error)=>{{if valid{{return Err(format!("{{path}}: input: {{error}}").into())}};continue}}}};
drop(input);let output=document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options).map_err(|error|format!("{{path}}: output: {{error}}"))?;
let copy={native}::decode_xml_document(&output,limits,None,&options)?;
let exact=document.extensions.preserved_bytes();
let mut bounded=options.clone();bounded.limits.max_preserved_bytes=exact;
document.value.encode_xml_with_extensions(&document.extensions,limits,None,&bounded)?;
if exact>0{{bounded.limits.max_preserved_bytes=exact-1;
assert!(document.value.encode_xml_with_extensions(&document.extensions,limits,None,&bounded).is_err());
assert!({native}::decode_xml_document(&output,limits,None,&bounded).is_err());}}
assert_eq!(output,document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options)?);
for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{{assert_eq!(document.value.encode(protocol)?,copy.value.encode(protocol)?);}}
std::fs::write(format!("{{path}}.rust.xml"),output)?;
}}''')
    return '''mod schema;use schema::*;
/// Release source bytes before replay, retaining only the independently owned namespace companion.
fn main()->Result<(),Box<dyn std::error::Error>>{
for path in std::env::args().skip(1){let valid=path.ends_with(".valid.xml");let input=std::fs::read_to_string(&path)?;
let name=std::path::Path::new(&path).file_name().unwrap().to_str().unwrap();let kind=name.split('-').next().unwrap();
let limits=XmlLimits::default();let options=XmlExtensionOptions::default();match kind{
''' + ',\n'.join(branches) + ''',_=>panic!("Unknown probe")}}
Ok(())}'''


def c_consumer():
    """Require unchanged native bytes and atomic document destinations with real C ownership."""
    checks = []
    for kind, _, native in KINDS:
        checks.append(f'''/* Exercise {kind} without borrowing the original XML buffer. */
static void check_{kind}(const char* path,bool valid){{
size_t size=0;uint8_t* input=read_file(path,&size);{native}_xml_document document={{0}},copy={{0}};
require({native}_xml_document_init(&document)==srl_ok,"Init");require({native}_xml_document_init(&copy)==srl_ok,"Copy init");
const {native} previous=document.value;const srl_xml_extension_state* state=document.extensions;
srl_status status={native}_decode_xml_document(&document,input,size,NULL,NULL);free(input);
require((status==srl_ok)==valid,"Input decision");
if(!valid){{require(!memcmp(&previous,&document.value,sizeof(previous))&&state==document.extensions,"Atomic input");}}
else{{srl_buffer output={{0}};require({native}_encode_xml_document(&document,&output,NULL,NULL)==srl_ok,"Output");
require({native}_decode_xml_document(&copy,output.data,output.size,NULL,NULL)==srl_ok,"Replay");
size_t exact=srl_xml_extension_preserved_bytes(document.extensions);
srl_xml_extension_options bounded=srl_xml_default_extension_options();bounded.limits.max_preserved_bytes=exact;
srl_buffer retry={{0}};require({native}_encode_xml_document(&document,&retry,NULL,&bounded)==srl_ok,"Exact scope budget");srl_buffer_free(&retry);
if(exact){{--bounded.limits.max_preserved_bytes;require({native}_encode_xml_document(&document,&retry,NULL,&bounded)!=srl_ok&&!retry.data,"One-tight scope budget");}}
require({native}_encode_xml_document(&document,&retry,NULL,NULL)==srl_ok&&retry.size==output.size&&!memcmp(retry.data,output.data,output.size),"Fresh recovery");srl_buffer_free(&retry);
const srl_protocol protocols[]={{srl_json,srl_binary_none,srl_binary_integer,srl_binary_string}};
for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){{srl_buffer first={{0}},second={{0}};
require({native}_encode(&document.value,protocols[index],&first)==srl_ok&&{native}_encode(&copy.value,protocols[index],&second)==srl_ok,"Native encode");
require(first.size==second.size&&!memcmp(first.data,second.data,first.size),"Native identity");srl_buffer_free(&first);srl_buffer_free(&second);}}
char* destination=(char*)malloc(strlen(path)+sizeof(".c.xml"));require(destination!=NULL,"Path");strcpy(destination,path);strcat(destination,".c.xml");FILE* file=fopen(destination,"wb");require(file!=NULL,"Open output");require(fwrite(output.data,1,output.size,file)==output.size&&fclose(file)==0,"Write output");free(destination);srl_buffer_free(&output);}}
{native}_xml_document_free(&document);{native}_xml_document_free(&copy);
}}''')
    dispatch = '\n'.join(f'if(!strncmp(name,"{kind}-",{len(kind)+1})){{check_{kind}(path,valid);continue;}}' for kind, _, _ in KINDS)
    return shared.C.split('int main(')[0] + '\n'.join(checks) + '''
/* Dispatch the maintained corpus through the corresponding generated typed facade. */
int main(int argc,char** argv){for(int index=1;index<argc;++index){const char* path=argv[index];const char* name=strrchr(path,'/');if(!name){name=strrchr(path,'\\\\');}name=name?name+1:path;bool valid=strstr(path,".valid.xml")!=NULL;
''' + dispatch + '\nrequire(false,"Unknown probe");}return EXIT_SUCCESS;}\n'


def main():
    """Compile both actual SDKs and observe all saved expanded payload identities independently."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--language', default='rust,c')
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.set_defaults(wsl_languages='', cpp_library=None, cpp_executable=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Output must be under ignored out/')
    languages = args.language.split(',')
    if not languages or len(set(languages)) != len(languages) or any(value not in ('c', 'rust') for value in languages):
        parser.error('Select distinct c,rust consumers')
    args.build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='scopes-', dir=args.build) as directory:
        args.build = Path(directory)
        for name, text in [('model.serializer', MODEL), ('options.ini', CONFIG), ('all.xsd', ALL_XSD), ('wrapped.xsd', WRAPPED_XSD)]:
            (args.build / name).write_text(text, encoding='utf-8')
        command = [args.compiler, '--input', args.build / 'model.serializer', '--config', args.build / 'options.ini', '--language', ','.join(languages)]
        if ORACLE_XSD:
            command += ['--xml.export-xsd', args.build / 'exported.xsd']
        for language in languages:
            target = args.build / language
            target.mkdir()
            command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
        sdk.run(command)
        paths = []
        for index, (kind, document, valid) in enumerate(probes()):
            path = args.build / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'
            path.write_text(document, encoding='utf-8')
            paths.append(path)
        runner = sdk.Runner(args)
        oracle_command = None
        # Provider presentation controls stay separate from the untouched SDK inputs and outputs.
        def oracle_paths(selected):
            """Retain original bytes unless an explicitly supplied provider-boundary control applies."""
            if not ORACLE_PRESENTATION:
                return selected
            directory = args.build / 'oracle-presentation'
            directory.mkdir(exist_ok=True)
            result = []
            changed = 0
            for path in selected:
                original = path.read_bytes()
                control = ORACLE_PRESENTATION(original, '.valid.xml' in path.name)
                if control == original:
                    result.append(path)
                else:
                    target = directory / path.name
                    target.write_bytes(control)
                    result.append(target)
                    changed += 1
            print(f'Independent provider presentation controls: {changed}/{len(selected)}; SDK bytes unchanged', flush=True)
            return result
        if ORACLE_XSD:
            import go_xml_qname_constraints_test as qnames
            oracle_directory = args.build / 'oracle'
            oracle_directory.mkdir()
            (oracle_directory / 'Main.java').write_text(qnames.JAVA_ORACLE, encoding='utf-8')
            sdk.run(runner.command('java', 'javac', '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
                                   '-d', runner.path('java', oracle_directory), runner.path('java', oracle_directory / 'Main.java')))
            oracle_command = runner.command('java', 'java', '-cp', runner.path('java', oracle_directory), 'Main')
            controls = oracle_paths(paths)
            for schema in ('all.xsd', 'exported.xsd'):
                sdk.run([*oracle_command, runner.path('java', args.build / schema), *[runner.path('java', path) for path in controls]])
        for language in languages:
            source = args.build / 'source' / language
            source.mkdir(parents=True)
            (source / ('main.rs' if language == 'rust' else 'main.c')).write_text(rust_consumer() if language == 'rust' else c_consumer(), encoding='utf-8')
            if language == 'rust':
                for name in ('Cargo.toml', 'Cargo.lock'):
                    shutil.copyfile(ROOT / 'example/xml/rust' / name, source / name)
            launch = xml_runner.build_language(args, runner, language, args.build / language, source_root=source.parent)
            sdk.run([*launch, *[runner.path(language, path) for path in paths]])
            for (_, document, valid), path in zip(probes(), paths):
                output = Path(str(path) + f'.{language}.xml')
                if valid:
                    assert identity(document.encode()) == identity(output.read_bytes()), path
                else:
                    assert not output.exists(), path
            if oracle_command:
                outputs = [Path(str(path) + f'.{language}.xml') for (_, _, valid), path in zip(probes(), paths) if valid]
                controls = oracle_paths(outputs)
                for schema in ('all.xsd', 'exported.xsd'):
                    sdk.run([*oracle_command, runner.path('java', args.build / schema), *[runner.path('java', path) for path in controls]])
            print(f'PASS: {language}: {len(paths)} namespace graph decisions', flush=True)


if __name__ == '__main__':
    main()
