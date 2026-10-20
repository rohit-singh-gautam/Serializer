# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual emitted Go OPC facades with independent ZIP/XML, policy and native controls."""
import argparse
import hashlib
import json
import io
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as etree
import zipfile

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'test'))
import java_package_test as shared
import go_package_test as physical
import python_package_test as independent


def author(directory):
    """Create shared wire fixtures while explicitly excluding higher typed-role semantics at this layer."""
    directory.mkdir(exist_ok=True)
    rows=[]
    profile_rows=[]
    for row in shared.build_fixtures(directory):
        label,operation,path,code,name,value=row
        if operation not in ('package','preserve','collection','skip','parameterized'):
            continue
        profile_rows.append(row)
        # These are typed-role/unknown-policy failures, beyond the metadata-only layer under test.
        if code=='CODEC' or label=='extension-reject':
            code='OK'
        rows.append((label,operation,path,code,name,value))
    def mutation(label,parts,core='OK',profile=None):
        """Keep scope-local graph failures independent from typed-role dispatch decisions."""
        path=label+'.zip'
        (directory/path).write_bytes(independent.independent_zip(parts))
        rows.append((label,'package',path,core,'','independent'))
        profile_rows.append((label,'package',path,core if profile is None else profile,'','independent'))
    def root_mutation(label,change,core='OK',profile=None):
        """Author a fresh Relationships tree through the standard XML writer."""
        parts=dict(independent.independent_parts())
        root=etree.fromstring(parts['_rels/.rels'])
        change(root)
        parts['_rels/.rels']=etree.tostring(root,encoding='utf-8')
        mutation(label,parts,core,profile)
    root_mutation('scope-duplicate-id',lambda root:etree.SubElement(root,'{'+independent.RELS+'}Relationship',{
        'Id':'main','Type':'urn:vendor:other','Target':'Textures/raw.bin'}),'INVALID_RELATIONSHIP')
    root_mutation('scope-missing-target',lambda root:root[0].set('Target','/Absent.bin'),'MISSING_PART')
    root_mutation('scope-double-claimed',lambda root:etree.SubElement(root,'{'+independent.RELS+'}Relationship',{
        'Id':'second','Type':'urn:serializer:main-model','Target':'Alternate/root.xml'}),profile='INVALID_RELATIONSHIP')
    def external(root):
        """Make a syntactically valid external URI invalid for one declared internal role."""
        root[0].set('Target','https://host.invalid/value')
        root[0].set('TargetMode','External')
    root_mutation('scope-declared-external',external,profile='INVALID_RELATIONSHIP')
    for label,value in (('colon','bad:id'),('empty',''),('slash','bad/name')):
        root_mutation('scope-invalid-id-'+label,lambda root,value=value:root[0].set('Id',value),'INVALID_RELATIONSHIP')
    root_mutation('scope-unicode-id',lambda root:root[0].set('Id','\u03bb'))
    parts=dict(independent.independent_parts())
    parts['Alternate/_rels/root.xml.rels']=parts['Alternate/_rels/root.xml.rels'].replace(b'Id="texture"',b'Id="main"')
    mutation('scope-reused-id-other-source',parts)
    parts=dict(independent.independent_parts())
    parts['alternate/ROOT.xml']=parts['Alternate/root.xml']
    mutation('part-case-alias',parts,'DUPLICATE_NAME')
    parts=dict(independent.independent_parts())
    parts['Textures/raw.bin/child.bin']=b'child'
    parts['[Content_Types].xml']=parts['[Content_Types].xml'].replace(b'</Types>',b'<Override PartName="/Textures/raw.bin/child.bin" ContentType="application/octet-stream"/></Types>')
    mutation('part-file-ancestor',parts,'INVALID_NAME')
    parts=dict(independent.independent_parts())
    parts['Missing/_rels/root.xml.rels']=f'<Relationships xmlns="{independent.RELS}"/>'.encode()
    parts['[Content_Types].xml']=parts['[Content_Types].xml'].replace(b'</Types>',f'<Override PartName="/Missing/_rels/root.xml.rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/></Types>'.encode())
    mutation('scope-missing-source',parts,'MISSING_PART')
    parts=dict(independent.independent_parts())
    parts['[Content_Types].xml']=parts['[Content_Types].xml'].replace(independent.MODEL_MIME.encode(),(independent.MODEL_MIME+';p=x;P=x').encode())
    mutation('mime-duplicate-parameter',parts,'INVALID_METADATA')
    parts=dict(independent.independent_parts())
    root=etree.fromstring(parts['_rels/.rels'])
    relationship=etree.tostring(root[0],encoding='unicode')
    parts['_rels/.rels']=(f'<Relationships xmlns="{independent.RELS}" xmlns:r="{independent.RELS}" '
        f'xmlns:mc="{independent.MC}" xmlns:e="urn:extension" mc:Ignorable="e" mc:ProcessContent="e:box">'
        '<e:box>'+' '*16384+relationship+'</e:box></Relationships>').encode()
    mutation('mce-buffer-large',parts)
    (directory/'opc_cases.tsv').write_text(''.join('\t'.join(row)+'\n' for row in rows),encoding='utf-8')
    (directory/'profile_cases.tsv').write_text(''.join('\t'.join(row)+'\n' for row in profile_rows),encoding='utf-8')
    (directory/'fixture-provenance.json').write_text(json.dumps({'metadata_cases':len(rows),'typed_cases':len(profile_rows),'authoring':'Independent standard ZIP/XML providers and shared explicit wire producers'},indent=2)+'\n',encoding='utf-8')
    return rows,profile_rows


def verify(directory):
    """Inspect actual Go output through independent ZIP framing and standard XML oracles."""
    results=[]
    for path in sorted(directory.glob('output-*.opc')):
        data=path.read_bytes()
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            assert archive.testzip() is None
            expected=independent.independent_parts()
            assert set(archive.namelist())==set(expected)
            physical.inspect_directory(data,len(expected),'64' in path.name)
            for name in ('Alternate/root.xml','Textures/raw.bin'):
                assert archive.read(name)==expected[name]
            types=etree.fromstring(archive.read('[Content_Types].xml'))
            assert types.tag=='{'+independent.CT+'}Types'
            assert [item.attrib['PartName'] for item in types]==sorted(item.attrib['PartName'] for item in types)
            for name in ('_rels/.rels','Alternate/_rels/root.xml.rels'):
                root=etree.fromstring(archive.read(name))
                original=etree.fromstring(expected[name])
                assert root.tag==original.tag
                assert sorted(dict(item.attrib).items() for item in root)==sorted(dict(item.attrib).items() for item in original)
        results.append(path.name)
    assert len(results)==6
    for name,preserved in (('typed.opc',False),('preserved.opc',True)):
        path=directory/name
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            physical.inspect_directory(path.read_bytes(),len(archive.infolist()),False)
            model=etree.fromstring(archive.read('3D/model.xml'))
            assert model.tag=='{urn:serializer:package-test}model' and model.attrib=={'name':'independent'}
            assert len(model)==1 and model[0].attrib=={'x':'1','y':'2','z':'3'}
            assert archive.read('Textures/raw.bin')==b'\0\1\xfe\xff'
            if preserved:
                assert archive.read('Extensions/vendor.xml')==independent.independent_parts(True)['Extensions/vendor.xml']
                assert archive.read('Extensions/second.bin')==b'second whole opaque part'
            manifest=etree.fromstring(archive.read('[Content_Types].xml'))
            assert manifest.tag=='{'+independent.CT+'}Types'
            assert [node.attrib['PartName'] for node in manifest]==sorted(node.attrib['PartName'] for node in manifest)
        results.append(name)
    return results


def run(command,directory,environment,log):
    """Retain exact offline compiler/SDK/native diagnostics and require the actual process to succeed."""
    with log.open('w',encoding='utf-8') as output:
        result=subprocess.run([str(item) for item in command],cwd=directory,env=environment,
                              stdout=output,stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'{log.name}: exit {result.returncode}\n'+log.read_text(encoding='utf-8',errors='replace')[-18000:])


def generate(args,directory,source,extra=()):
    """Emit actual full model/package source and discover its collision-safe public helper family."""
    directory.mkdir(parents=True,exist_ok=True)
    schema=directory/'model.serializer'
    schema.write_text(source,encoding='utf-8')
    run([args.compiler,'--input',schema,'--language','go','--go.output',directory/'schema.go',
         '--go.package','main',*extra],directory,args.environment,directory/'generate.log')
    text=(directory/'schema.go').read_text(encoding='utf-8')
    match=re.search(r'Generated package helper prefix: ([A-Za-z_][A-Za-z_0-9]*)',text)
    if not match or 'type '+match[1]+'ExtensionPolicy struct' not in text:
        raise AssertionError('Actual compiler did not emit OPC runtime support')
    (directory/'go.mod').write_text('module serializer-go-opc\n\ngo 1.22\n',encoding='utf-8')
    return match[1]


def profile_schema():
    """Extend the maintained schema with a second declared MIME identity without handwritten binding code."""
    source=(ROOT/'test/resources/opc_profile.serializer').read_text(encoding='utf-8')
    start=source.index('  package archive')
    end=source.index('\n  // Existing package names',start)
    clone=source[start:end].replace('package archive','package parameterized_archive',1).replace(
        independent.MODEL_MIME,independent.MODEL_MIME+';profile=\\"a  b\\";mode=read')
    dynamic=source[start:end].replace('package archive','package dynamic_mime_archive',1).replace(
        'content_type("'+independent.MODEL_MIME+'");','content_type(dynamic);')
    return source[:source.rfind('\n}')]+ '\n'+clone+'\n'+dynamic+'\n}\n'


def sdk_profiles(args,build):
    """Execute actual collision and binary-only package profiles and reject companion-name aliases transactionally."""
    results=[]
    collision='''serializer version 1.20.0;
class SerializerPackage {} class SerializerPackage_2Reader {}
class SerializerCompression {} class SerializerCompression_2Format {}
package Bundle { part Attachments:bytes {name(dynamic);content_type(dynamic);
relationship("urn:serializer:test-part");occurs(0,unbounded);} }
'''
    for name,source,extra in (('collision',collision,('--go.naming','preserve')),
                              ('binary',profile_schema(),('--protocols','binary_none'))):
        directory=build/name
        prefix=generate(args,directory,source,extra)
        if name=='collision':
            assert prefix=='SerializerPackage_3'
            assert 'Generated compression helper prefix: SerializerCompression_3' in (directory/'schema.go').read_text(encoding='utf-8')
            smoke='''package main
func main(){value:=&Bundle{Attachments:[]@helper@BinaryPart{{Data:[]byte{1,2,3},Name:"/data.bin",ContentType:"application/octet-stream"}}};data,err:=BundleEncode(value);if err!=nil{panic(err)};candidate,err:=BundleLoad(data);if err!=nil||len(candidate.Attachments)!=1||candidate.Attachments[0].Data[2]!=3{panic("collision profile changed")};_ = NewSerializerPackage();_ = NewSerializerPackage_2Reader()}
'''.replace('@helper@',prefix)
        else:
            smoke='''package main
func main(){value:=&ProfileArchiveInput{Main:&ProfileModel{Name:"binary-only"}};data,err:=ProfileArchiveEncode(value);if err!=nil{panic(err)};candidate,err:=ProfileArchiveLoad(data);if err!=nil||candidate.Main.Name!="binary-only"{panic("binary-only OPC changed")};if data,err:=candidate.Main.Encode(JSON);data!=nil||err==nil{panic("disabled native protocol accepted")}}
'''
        (directory/'main.go').write_text(smoke,encoding='utf-8')
        run([args.go,'run','.'],directory,args.environment,directory/'sdk.log')
        results.append({'profile':name,'prefix':prefix,'generated_sha256':hashlib.sha256((directory/'schema.go').read_bytes()).hexdigest()})
    directory=build/'companion-collision'
    directory.mkdir(exist_ok=True)
    source=directory/'model.serializer'
    source.write_text('serializer version 1.20.0;class BundleLoadOptions{} package Bundle {part Attachments:bytes{name(dynamic);content_type(dynamic);relationship("urn:test:part");occurs(0,unbounded);}}',encoding='utf-8')
    destination=directory/'schema.go'
    sentinel=b'Existing output must survive companion-name rejection.\n'
    destination.write_bytes(sentinel)
    result=subprocess.run([str(args.compiler),'--input',str(source),'--language','go','--go.output',str(destination),'--go.package','main','--go.naming','preserve'],cwd=directory,env=args.environment,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (directory/'generate.log').write_bytes(result.stdout)
    assert result.returncode and destination.read_bytes()==sentinel
    assert b'Go package companion name collision: BundleLoadOptions' in result.stdout
    return results


EXCHANGE=r'''package main
import("os")
// main exchanges actual typed owning values with the independently built C++ facade.
func main(){source,err:=os.ReadFile(os.Args[1]);if err!=nil{panic(err)};value,err:=OpcExampleDocumentLoad(source);if err!=nil{panic(err)}
if value.Main.Title!="Parts & tools"||len(value.Main.Items)!=2||value.Main.Items[0].Id!=7||value.Main.Items[0].Label!="Bolt <M6>"||value.Main.Items[1].Id!=8||value.Main.Items[1].Label!="Nut"||len(value.Attachments)!=1||value.Attachments[0].Name!="/Attachments/sample.bin"{panic("C++/Go typed values changed")}
data,err:=OpcExampleDocumentEncode(value);if err!=nil{panic(err)};if err=os.WriteFile(os.Args[2],data,0600);err!=nil{panic(err)}}
'''


def native_exchange(args,build):
    """Use an actual C++ typed OPC facade as producer/consumer, with independent ZIP/XML output inspection."""
    if not args.cpp_example:
        return 0
    directory=build/'native-exchange'
    generate(args,directory,(ROOT/'example/opc/model.serializer').read_text(encoding='utf-8'))
    (directory/'main.go').write_text(EXCHANGE,encoding='utf-8')
    source,destination=directory/'cpp.opc',directory/'go.opc'
    run([args.cpp_example,source],directory,args.environment,directory/'cpp-producer.log')
    run([args.go,'run','.',source,destination],directory,args.environment,directory/'go-exchange.log')
    run([args.cpp_example,'--read',destination],directory,args.environment,directory/'cpp-consumer.log')
    for path in (source,destination):
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None and archive.read('Attachments/sample.bin')==b'\0\1\xfe\xff'
            model=etree.fromstring(archive.read('Catalog/catalog.xml'))
            assert model.attrib=={'title':'Parts & tools'}
            assert [item.attrib for item in model]==[{'id':'7','label':'Bolt <M6>'},{'id':'8','label':'Nut'}]
    return 2


def main():
    """Qualify actual compiler output with an offline Go1.22-language SDK and retain complete binary provenance."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path)
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--go',default=shutil.which('go'))
    parser.add_argument('--native-producer',type=Path)
    parser.add_argument('--cpp-example',type=Path)
    args=parser.parse_args()
    if not args.go:
        parser.error('An actual Go1.22+ SDK is required')
    args.compiler=args.compiler.resolve()
    args.build=args.build.resolve()
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository test output must remain beneath ignored out/')
    args.build.mkdir(parents=True,exist_ok=True)
    binaries={'compiler':args.compiler}
    for name in ('native_producer','cpp_example'):
        path=getattr(args,name)
        if path:
            path=path.resolve();setattr(args,name,path);binaries[name]=path
    hashes={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in binaries.items()}
    args.environment=os.environ.copy()
    args.environment.update(GOTOOLCHAIN='local',GOWORK='off',GOPROXY='off',GOSUMDB='off',GOENV='off',
        GOFLAGS='',CGO_ENABLED='0',GOMAXPROCS='4',GOCACHE=str(args.build/'go-cache'),GOTMPDIR=str(args.build/'go-temp'))
    Path(args.environment['GOTMPDIR']).mkdir(exist_ok=True)
    beginning=time.monotonic()
    rows,profile_rows=author(args.build/'fixtures')
    native_inputs=0
    if args.native_producer:
        directory=args.build/'native-producer'
        directory.mkdir(exist_ok=True)
        run([args.native_producer,directory],directory,args.environment,directory/'producer.log')
        with (args.build/'fixtures/opc_cases.tsv').open('a',encoding='utf-8') as manifest:
            for name in ('canonical-first.opc','canonical-second.opc'):
                path=directory/name
                destination=args.build/'fixtures'/('native-'+name)
                shutil.copy2(path,destination)
                manifest.write('\t'.join(('native-'+name,'catalog',destination.name,'OK','',''))+'\n')
                native_inputs+=1
    prefix=generate(args,args.build,profile_schema())
    driver=ROOT/'test/resources/package_go_opc_test.go'
    (args.build/'opc_test.go').write_text(driver.read_text(encoding='utf-8').replace('SerializerPackage',prefix),encoding='utf-8')
    run([args.go,'test','-count=1','-v','.'],args.build,args.environment,args.build/'sdk.log')
    sdk_log=(args.build/'sdk.log').read_text(encoding='utf-8')
    additional_groups=len(re.findall(r'^--- PASS: ',sdk_log,re.M))-2
    api_subcases=len(re.findall(r'^\s+--- PASS: (?!TestOPCIndependent/|TestTypedIndependent/)',sdk_log,re.M))
    outputs=verify(args.build)
    profiles=sdk_profiles(args,args.build)
    exchanges=native_exchange(args,args.build)
    for name,path in binaries.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest()==hashes[name],'Qualification binary changed during SDK execution'
    report={'staged_runtime':False,'compiler_sha256':hashes['compiler'],
        'binaries':{name:{'path':str(path),'sha256':hashes[name]} for name,path in binaries.items()},
        'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'driver_sha256':hashlib.sha256(driver.read_bytes()).hexdigest(),
        'independent_metadata_decisions':len(rows),'independent_typed_decisions':len(profile_rows),
        'additional_sdk_test_groups':additional_groups,'named_api_subcases':api_subcases,
        'native_metadata_inputs':native_inputs,'native_typed_exchange_decisions':exchanges,
        'independent_outputs':len(outputs),'outputs':{name:hashlib.sha256((args.build/name).read_bytes()).hexdigest() for name in outputs},
        'go':subprocess.check_output([args.go,'version'],env=args.environment,text=True).strip(),
        'language_baseline':'go 1.22','prefix':prefix,'profiles':profiles,
        'generated_sha256':hashlib.sha256((args.build/'schema.go').read_bytes()).hexdigest(),
        'elapsed_seconds':round(time.monotonic()-beginning,3)}
    (args.build/'verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()
