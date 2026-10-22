# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify Go native part pipelines, profile choices and scoped XML capabilities with real SDK output."""
import argparse
import io
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import xml.etree.ElementTree as etree
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CT = 'http://schemas.openxmlformats.org/package/2006/content-types'
REL = 'http://schemas.openxmlformats.org/package/2006/relationships'
NS = 'urn:serializer:package-pipeline'
MC = 'http://schemas.openxmlformats.org/markup-compatibility/2006'
XSI = 'http://www.w3.org/2001/XMLSchema-instance'


def archive(payload, extra=None, external=None):
    """Author complete ZIP/OPC bytes independently of generated package or native helpers."""
    manifest = etree.Element('{' + CT + '}Types')
    links = etree.Element('{' + REL + '}Relationships')
    parts = {'/main.data': payload} if payload is not None else {}
    parts.update(extra or {})
    for name in parts:
        etree.SubElement(manifest, '{' + CT + '}Override', PartName=name,
                         ContentType='application/vnd.serializer.pipeline')
        etree.SubElement(links, '{' + REL + '}Relationship', Id='authored_' + name[1:].replace('.', '_'),
                         Type='urn:serializer:pipeline:' + ('fixed' if name == '/fixed.data' else 'main'), Target=name)
    if external is not None:
        etree.SubElement(links, '{' + REL + '}Relationship', Id='authored_external',
                         Type='urn:serializer:pipeline:external', Target=external, TargetMode='External')
    etree.SubElement(manifest, '{' + CT + '}Override', PartName='/_rels/.rels',
                     ContentType='application/vnd.openxmlformats-package.relationships+xml')
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_STORED) as output:
        for name, data in parts.items():
            output.writestr(name[1:], data)
        output.writestr('_rels/.rels', etree.tostring(links, encoding='utf-8'))
        output.writestr('[Content_Types].xml', etree.tostring(manifest, encoding='utf-8'))
    return stream.getvalue()


CONSUMER = r'''
package main
import("bytes";"encoding/json";"errors";"fmt";"os";"path/filepath";m "serializer-pipeline/model";j "serializer-pipeline/jsonmodel";b "serializer-pipeline/binarymodel";r "serializer-pipeline/requiredmodel";e "serializer-pipeline/externalmodel")
var checks int
// check makes every SDK assertion independent of compiler optimization.
func check(ok bool,message string){if !ok{panic(message)};checks++}
// budget creates an independent public operation ledger.
func budget(limits ...m.@p@Limits)*m.@p@Budget{value,err:=m.@p@NewBudget(limits...);if err!=nil{panic(err)};return value}
// main drives actual public generated APIs while retaining independent native control bytes.
func main(){directory:=os.Args[1];read:=func(name string)[]byte{data,err:=os.ReadFile(filepath.Join(directory,name));if err!=nil{panic(err)};return data};source:=func(data []byte)m.@p@Source{return m.@p@Source{Reader:bytes.NewReader(data),SizeBytes:int64(len(data))}}
  value:=&m.PipelineArchive{Main:m.NewPipelinePayload()};value.Main.Id=17;value.Main.Label="a<&\nλ\u2028";value.Main.Values=[]uint32{1,2,0xffffffff}
  original,_:=json.Marshal(value)
  for _,codec:=range []string{"xml","json","binary_none"}{for _,storage:=range []string{"stored","deflate"}{profiles:=map[string]m.@p@PipelineProfile{"main":{Codec:codec,Compression:storage}};data,err:=m.PipelineArchiveEncodeWithPipelineProfiles(value,profiles);if err!=nil{panic(err)};check(len(data)>0,"profile output");loaded,err:=m.PipelineArchiveLoadWithPipelineProfiles(source(data),profiles);if err!=nil{panic(err)};check(loaded.Main.Id==17&&loaded.Main.Label==value.Main.Label&&len(loaded.Main.Values)==3,"profile round trip");name:=codec+"-"+storage+".opc";if err=os.WriteFile(filepath.Join(directory,name),data,0600);err!=nil{panic(err)}
    saved:=filepath.Join(directory,"saved-"+name);if err=m.PipelineArchiveSaveWithPipelineProfiles(saved,value,profiles,m.PipelineArchiveSaveOptions{});err!=nil{panic(err)};check(bytes.Equal(read("saved-"+name),data),"save revalidates selected envelope")
    if codec!="xml"{protocol:=m.JSON;if codec=="binary_none"{protocol=m.BINARY_NONE};control,err:=value.Main.Encode(protocol);if err!=nil{panic(err)};if err=os.WriteFile(filepath.Join(directory,codec+"-native.control"),control,0600);err!=nil{panic(err)}
      for _,decode:=range []bool{false,true}{measured:=budget();if decode{_,err=m.PipelineArchiveLoadWithPipelineProfiles(source(data),profiles,m.PipelineArchiveLoadOptions{Budget:measured})}else{_,err=m.PipelineArchiveEncodeWithPipelineProfiles(value,profiles,measured)};if err!=nil{panic(err)};defaults:=m.@p@DefaultLimits();for _,resource:=range []m.@p@Resource{m.@p@WorkUnits,m.@p@DecoderAllocationBytes}{for tight:=int64(0);tight<2;tight++{limits:=defaults;if resource==m.@p@WorkUnits{limits.MaxWorkUnits-=measured.Remaining(resource)+tight}else{limits.MaxDecoderAllocationBytes-=measured.Remaining(resource)+tight};if decode{_,err=m.PipelineArchiveLoadWithPipelineProfiles(source(data),profiles,m.PipelineArchiveLoadOptions{Budget:budget(limits)})}else{_,err=m.PipelineArchiveEncodeWithPipelineProfiles(value,profiles,budget(limits))};check((err==nil)==(tight==0),"exact/one-tight native operation admission");if err!=nil{var failure *m.@p@Error;check(errors.As(err,&failure)&&failure.Code==m.@p@ResourceLimit,"located native resource failure")}}}}
    }
  }}
  after,_:=json.Marshal(value);check(bytes.Equal(original,after),"profile operations changed source")
  for _,profiles:=range []map[string]m.@p@PipelineProfile{{"missing":{Codec:"json"}},{"main":{Codec:"gzip"}},{"fixed":{Codec:"binary_none"}},{"fixed":{Compression:"deflate"}}}{_,err:=m.PipelineArchiveEncodeWithPipelineProfiles(value,profiles);check(err!=nil,"closed profile override")}
  _,err:=m.PipelineArchiveLoadWithPipelineProfiles(source(read("json-stored.opc")),map[string]m.@p@PipelineProfile{"main":{Codec:"binary_none"}});check(err!=nil,"wrong selected native codec")
  for _,name:=range []string{"json-trailing.opc","json-invalid.opc","binary-trailing.opc","binary-count.opc"}{codec:="json";if name[:6]=="binary"{codec="binary_none"};_,err=m.PipelineArchiveLoadWithPipelineProfiles(source(read(name)),map[string]m.@p@PipelineProfile{"main":{Codec:codec}});check(err!=nil,"invalid independent native input")}
  for _,codec:=range []string{"json","binary_none"}{limits:=m.@p@DefaultLimits();sentinel:=errors.New("cancel native traversal");calls:=0;limits.CheckCancelled=func()error{calls++;if calls==40{return sentinel};return nil};_,err=m.PipelineArchiveEncodeWithPipelineProfiles(value,map[string]m.@p@PipelineProfile{"main":{Codec:codec}},budget(limits));check(errors.Is(err,sentinel),"native cancellation identity")}
  staticJSON,err:=j.PipelineArchiveLoad(j.@jp@Source{Reader:bytes.NewReader(read("independent-json.opc")),SizeBytes:int64(len(read("independent-json.opc")))});check(err==nil&&staticJSON.Main.Id==17,"static INI JSON pipeline")
  staticBinary,err:=b.PipelineArchiveLoad(b.@bp@Source{Reader:bytes.NewReader(read("independent-binary.opc")),SizeBytes:int64(len(read("independent-binary.opc")))});check(err==nil&&staticBinary.Main.Id==17,"static INI binary pipeline")
  for _,item:=range []struct{name string;valid bool}{{"cap-valid.opc",true},{"cap-unsupported.opc",false},{"cap-undeclared.opc",false},{"cap-other-attribute.opc",false},{"cap-missing-type.opc",false},{"cap-wrong-type.opc",false}}{data:=read(item.name);_,err:=r.PipelineArchiveLoad(r.@rp@Source{Reader:bytes.NewReader(data),SizeBytes:int64(len(data))});check((err==nil)==item.valid,"scoped package capability/type dispatch "+item.name+fmt.Sprint(err,errors.Unwrap(err)))}
  required:=&r.PipelineArchive{Main:r.NewPipelinePayload()};required.Main.Id=17;required.Main.Label="required";data,err:=r.PipelineArchiveEncode(required);if err!=nil{panic(err)};check(os.WriteFile(filepath.Join(directory,"required-output.opc"),data,0600)==nil,"required type output")
  externalURI:="https://example.invalid/model?raw=%2f#still-inert";external:=&e.PipelineLinks{Assets:[]e.@ep@BinaryPart{{Name:externalURI,Data:[]byte{},ContentType:""},{Name:"/internal.bin",Data:[]byte{1,2},ContentType:"application/octet-stream"}}};data,err=e.PipelineLinksEncode(external);if err!=nil{panic(err)};check(os.WriteFile(filepath.Join(directory,"external-output.opc"),data,0600)==nil,"external output");loadedExternal,err:=e.PipelineLinksLoad(e.@ep@Source{Reader:bytes.NewReader(data),SizeBytes:int64(len(data))});check(err==nil&&len(loadedExternal.Assets)==2,"external and internal bytes role");found:=false;for _,item:=range loadedExternal.Assets{if item.ContentType==""{found=item.Name==externalURI&&len(item.Data)==0}};check(found,"external URI never fetched");check(len(e.PipelineLinksDynamicPaths(external))==1,"external URI omitted from internal provenance");external.Assets[0].Data=[]byte{1};_,err=e.PipelineLinksEncode(external);check(err!=nil,"external payload bytes rejected")
  fmt.Printf("PASS: Go part pipelines, capabilities, external targets and native operation admission (%d assertions)\n",checks)
}
'''


def main():
    """Generate five real Go packages, compile one external consumer, then independently inspect its archives."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--build', required=True)
    parser.add_argument('--go', default='go')
    args = parser.parse_args()
    build = Path(args.build).resolve()
    build.mkdir(parents=True, exist_ok=True)
    fixture = build / 'fixtures'
    fixture.mkdir(exist_ok=True)
    model = (ROOT / 'test/resources/package_pipeline.serializer').read_text(encoding='utf-8')
    source = build / 'pipeline.serializer'
    source.write_text(model + '''
namespace pipeline { package links {
  part assets : bytes { name(dynamic); content_type(dynamic); relationship("urn:serializer:pipeline:external"); occurs(0, unbounded); }
} }
''', encoding='utf-8')
    replacements = {}
    for directory, profile, token in [('model', '', '@p@'), ('jsonmodel', 'main.codec=json\n', '@jp@'),
                                       ('binarymodel', 'main.codec=binary_none\n', '@bp@'),
                                       ('requiredmodel', 'main.requires_type=true\nmain.vocabularies=urn:extension\n', '@rp@'),
                                       ('externalmodel', '', '@ep@')]:
        output = build / directory / 'schema.go'
        output.parent.mkdir(exist_ok=True)
        options = output.parent / 'options.ini'
        options.write_text('[package.pipeline::archive]\n' + profile +
                           '[package.pipeline::links]\nassets.target_mode=both\n', encoding='utf-8')
        subprocess.run([args.compiler, '--input', source, '--language', 'go', '--go.output', output,
                        '--go.package', directory, '--config', options, '--xml.enabled=true'], check=True)
        replacements[token] = re.search(r'// Generated package helper prefix: (\w+)', output.read_text(encoding='utf-8')).group(1)
    text = CONSUMER
    for token, value in replacements.items():
        text = text.replace(token, value)
    (build / 'main.go').write_text(text, encoding='utf-8')
    (build / 'go.mod').write_text('module serializer-pipeline\n\ngo 1.23\n', encoding='utf-8')
    label = 'a<&\nλ\u2028'
    native_json = json.dumps({'id': 17, 'label': label, 'values': [1, 2, 0xffffffff]}, ensure_ascii=False, separators=(',', ':')).encode()
    encoded = label.encode()
    native_binary = struct.pack('<I', 17) + bytes([len(encoded)]) + encoded + bytes([3]) + struct.pack('<III', 1, 2, 0xffffffff)
    for name, data in [('independent-json', native_json), ('independent-binary', native_binary),
                       ('json-trailing', native_json + b'0'), ('json-invalid', b'{"id":-1,"label":"bad","values":[]}'),
                       ('binary-trailing', native_binary + b'\0'), ('binary-count', struct.pack('<I', 17) + b'\0\xfc')]:
        (fixture / (name + '.opc')).write_bytes(archive(data))
    base = f'<payload xmlns="{NS}" xmlns:xsi="{XSI}" xmlns:p="urn:serializer:native:pipeline" xmlns:mc="{MC}" xmlns:e="urn:extension" id="17" label="cap" xsi:type="p:payload" mc:MustUnderstand="e"/>'
    cases = {'cap-valid': base, 'cap-unsupported': base.replace('urn:extension', 'urn:unsupported'),
             'cap-undeclared': base.replace('xmlns:e="urn:extension" ', ''),
             'cap-other-attribute': base.replace('mc:MustUnderstand="e"', 'mc:Ignorable="e"'),
             'cap-missing-type': base.replace('xsi:type="p:payload" ', ''),
             'cap-wrong-type': base.replace('xsi:type="p:payload"', 'xsi:type="p:other"')}
    for name, data in cases.items():
        (fixture / (name + '.opc')).write_bytes(archive(data.encode()))
    environment = dict(os.environ, GOWORK='off', GOFLAGS='-mod=readonly', GOCACHE=str(build / 'gocache'))
    executable = build / ('consumer.exe' if os.name == 'nt' else 'consumer')
    subprocess.run([args.go, 'build', '-o', executable, '.'], cwd=build, env=environment, check=True)
    subprocess.run([executable, fixture], cwd=build, env=environment, check=True)
    for codec in ('xml', 'json', 'binary_none'):
        for storage, method in [('stored', zipfile.ZIP_STORED), ('deflate', zipfile.ZIP_DEFLATED)]:
            with zipfile.ZipFile(fixture / (codec + '-' + storage + '.opc')) as output:
                assert output.testzip() is None and output.getinfo('main.data').compress_type == method
                data = output.read('main.data')
                if codec == 'json':
                    assert json.loads(data) == json.loads(native_json)
                elif codec == 'binary_none':
                    assert data == native_binary
                else:
                    root = etree.fromstring(data)
                    assert root.tag == '{' + NS + '}payload' and root.attrib['label'] == label
                if codec != 'xml':
                    assert data == (fixture / (codec + '-native.control')).read_bytes()
    with zipfile.ZipFile(fixture / 'required-output.opc') as output:
        data = output.read('main.data')
        root = etree.fromstring(data)
        bindings = dict(item for _, item in etree.iterparse(io.BytesIO(data), events=('start-ns',)))
        prefix, local = root.attrib['{' + XSI + '}type'].split(':')
        assert bindings[prefix] == 'urn:serializer:native:pipeline' and local == 'payload'
    with zipfile.ZipFile(fixture / 'external-output.opc') as output:
        assert set(output.namelist()) == {'[Content_Types].xml', '_rels/.rels', 'internal.bin'}
        links = etree.fromstring(output.read('_rels/.rels'))
        external = [item for item in links if item.attrib.get('TargetMode') == 'External']
        assert len(external) == 1 and external[0].attrib['Target'] == 'https://example.invalid/model?raw=%2f#still-inert'
    print('PASS: independent ZIP/JSON/BINARY_NONE/XML and inert external relationship observers')


if __name__ == '__main__':
    main()
