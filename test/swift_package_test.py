# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compile actual generated Swift ZIP/OPC and verify it against independent Python producers/consumers."""
import argparse
import hashlib
import io
import json
import re
from pathlib import Path
import shutil
import struct
import subprocess
import time
import zipfile
import xml.etree.ElementTree as etree

import python_package_test as independent
from package_application_contract_test import build_fixtures as application_fixtures
from package_shared_codec_budget_test import fixtures as shared_fixtures

ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = bytes(index * 13 & 255 for index in range(10000))


def fixtures(build):
    """Create provider ZIPs and independently authored ZIP64/descriptor forms plus adversarial offsets."""
    rows = []
    def add(name, data, valid=True):
        """Retain exact corpus bytes and a validity contract separate from the Swift implementation."""
        (build / (name + '.zip')).write_bytes(data)
        rows.append((name + '.zip', 'valid' if valid else 'invalid'))
        if valid:
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                for item in archive.infolist():
                    archive.read(item)
        return data
    for method, label in [(zipfile.ZIP_STORED, 'stored'), (zipfile.ZIP_DEFLATED, 'deflate')]:
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w', compression=method) as archive:
            archive.writestr('data.bin', PAYLOAD)
        add(label, stream.getvalue())
    stored = (build / 'stored.zip').read_bytes()
    stream=io.BytesIO()
    with zipfile.ZipFile(stream,'w') as archive:
        archive.writestr('\u00e9.bin',b'composed'); archive.writestr('e\u0301.bin',b'decomposed')
    add('unicode-distinct',stream.getvalue())
    for wide in (False, True):
        for signed in (False, True):
            add(f'descriptor-{wide}-{signed}', independent.wire_descriptor_zip(PAYLOAD, wide=wide, signed=signed))
    add('local64', independent.independent_zip({'data.bin': PAYLOAD}, wide=True))
    # ZIP64 directory/locator fields are authored explicitly rather than invoking generated code.
    end = stored.rfind(b'PK\x05\x06')
    count, size, start = struct.unpack_from('<HII', stored, end + 10)
    wide = stored[:end] + struct.pack('<4sQHHIIQQQQ', b'PK\x06\x06', 44, 45, 45, 0, 0, count, count, size, start)
    wide += struct.pack('<4sIQI', b'PK\x06\x07', 0, end, 1)
    wide += struct.pack('<4s4H2IH', b'PK\x05\x06', 0, 0, 65535, 65535, 0xffffffff, 0xffffffff, 0)
    add('directory64', wide)
    # Force central entry sizes independently of ZIP64 directory metadata.
    central = stored.index(b'PK\x01\x02')
    header = bytearray(stored[central:central+46]); struct.pack_into('<II', header, 20, 0xffffffff, 0xffffffff); struct.pack_into('<H', header, 30, 20)
    record = bytes(header)+b'data.bin'+struct.pack('<HHQQ',1,16,len(PAYLOAD),len(PAYLOAD))
    footer = bytearray(stored[end:]); struct.pack_into('<I',footer,12,len(record))
    add('central64', stored[:central]+record+bytes(footer))
    for offset, fmt, value, label in [(6,'H',1,'encrypted'),(8,'H',99,'unsupported-method'),(26,'H',65535,'local-name-overflow'),
            (central+42,'I',0xffffffff,'missing-wide-offset'),(central+20,'I',0xffffffff,'missing-wide-size'),
            (central+28,'H',65535,'central-name-overflow'),(end+10,'H',2,'forged-count'),
            (end+12,'I',0xffffffff,'missing-wide-directory'),(end+16,'I',1,'directory-offset'),
            (central+34,'H',1,'multidisk'),(14,'I',1,'local-crc'),(18,'I',len(PAYLOAD)-1,'local-size')]:
        data = bytearray(stored); struct.pack_into('<'+fmt,data,offset,value); add(label,data,False)
    data = bytearray(stored); data[38] ^= 1; add('payload-crc',data,False)
    add('trailing',stored+b'x',False); add('truncated',stored[:-1],False)
    add('short',b'PK',False)
    add('deflate-trailing', independent.wire_descriptor_zip(PAYLOAD,trailing=b'x'),False)
    data=bytearray(stored); data[30]=255; data[central+46]=255; add('invalid-utf8-name',data,False)
    for name in ('../data.bin','a//data.bin','/data.bin','a\\data.bin'):
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,'w') as archive: archive.writestr(name,PAYLOAD)
        data=stream.getvalue()
        if '\\' in name: data=data.replace(b'a/data.bin',b'a\\data.bin')
        add('invalid-name-'+str(len(rows)),data,False)
    (build/'cases.tsv').write_text(''.join('\t'.join(row)+'\n' for row in rows),encoding='utf-8')
    return rows


def opc_fixtures(build):
    """Author standard OPC metadata and malformed payload/relationship/association contracts independently."""
    rows=[]
    def add(name,parts,invalid=False):
        """Frame independently authored parts through Python's standard ZIP provider."""
        (build/name).write_bytes(independent.independent_zip(parts))
        if invalid: rows.append((name,'invalid'))
    base=independent.independent_parts(); add('independent.opc',base)
    vendor=independent.independent_parts(extension=True)
    vendor['Alternate/_rels/root.xml.rels']=vendor['Alternate/_rels/root.xml.rels'].replace(b'</Relationships>',b'<Relationship Id="vendor-backlink" Type="urn:vendor:backlink" Target="../Extensions/vendor.xml"/></Relationships>')
    add('vendor.opc',vendor)
    mutations=[('wrong-media.opc','[Content_Types].xml',independent.MODEL_MIME.encode(),b'application/vnd.wrong+xml'),
        ('wrong-root.opc','Alternate/root.xml',b'<model ',b'<other '),
        ('bad-scalar.opc','Alternate/root.xml',b'x="1"',b'x="invalid"'),
        ('unknown-core.opc','[Content_Types].xml',b'<Override ',b'<Unknown '),
        ('escaping.opc','_rels/.rels',b'Alternate/root.xml',b'../outside.xml'),
        ('dangling.opc','_rels/.rels',b'Alternate/root.xml',b'missing.xml'),
        ('mode.opc','_rels/.rels',b'Target="Alternate',b'TargetMode="wrong" Target="Alternate'),
        ('duplicate-id.opc','_rels/.rels',b'</Relationships>',b'<Relationship Id="main" Type="urn:other" Target="Alternate/root.xml"/></Relationships>'),
        ('manifest-escape.opc','[Content_Types].xml',b'/Textures/raw.bin',b'/Textures/%72aw.bin'),
        ('duplicate-type.opc','[Content_Types].xml',b'</Types>',b'<Override PartName="/Textures/raw.bin" ContentType="application/octet-stream"/></Types>'),
        ('undeclared.opc','[Content_Types].xml',b'<Override PartName="/Textures/raw.bin" ContentType="application/octet-stream"/>',b''),
        ('dtd.opc','[Content_Types].xml',b'<Types',b'<!DOCTYPE Types [<!ENTITY x "y">]><Types')]
    for name,key,old,new in mutations:
        parts=dict(base); parts[key]=parts[key].replace(old,new); add(name,parts,True)
    for exact,namespace in ((True,'urn:\u00e9'),(False,'urn:e\u0301')):
        suffix='exact' if exact else 'distinct'
        parts=dict(base); parts['Alternate/root.xml']=f'<model xmlns="{namespace}"/>'.encode()
        add('unicode-root-'+suffix+'.opc',parts)
        parts=dict(base); parts['Alternate/root.xml']=f'<model xmlns="urn:serializer:package-test" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:n="{namespace}" xsi:type="n:target"/>'.encode()
        add('unicode-type-'+suffix+'.opc',parts)
    parts=dict(base);parts['Alternate/root.xml']=b'<digest_model xmlns="urn:linked" value="0102"/>';add('local-digest.opc',parts)
    parts=dict(base); parts['Textures/RAW.bin']=parts['Textures/raw.bin']; add('case-collision.opc',parts,True)
    parts=dict(base); parts['Textures/raw.bin/child']=b'x'; add('ancestry.opc',parts,True)
    add('middle.opc',independent.independent_collection_parts(bad_index=1))
    parts={'target.xml':b'<target xmlns="urn:linked" id="one"/>','holder.xml':b'<holder xmlns="urn:linked" ref="missing"/>',
        '_rels/.rels':f'<Relationships xmlns="{independent.RELS}"><Relationship Id="target" Type="urn:target" Target="target.xml"/><Relationship Id="holder" Type="urn:holder" Target="holder.xml"/></Relationships>'.encode(),
        '[Content_Types].xml':f'<Types xmlns="{independent.CT}"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Override PartName="/target.xml" ContentType="application/vnd.target+xml"/><Override PartName="/holder.xml" ContentType="application/vnd.holder+xml"/></Types>'.encode()}
    add('linked-missing.opc',parts)
    parts=dict(parts);parts['holder.xml']=parts['holder.xml'].replace(b'ref="missing"',b'ref="one"');add('linked-single-valid.opc',parts)
    (build/'opc-cases.tsv').write_text(''.join('\t'.join(row)+'\n' for row in rows),encoding='utf-8')
    return rows,vendor


def main():
    """Generate, strictly compile and execute the actual Swift SDK, retaining exact tools and evidence."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--swiftc',default='swiftc')
    args=parser.parse_args(); build=args.build.resolve(); compiler=args.compiler.resolve(strict=True)
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    build.mkdir(parents=True,exist_ok=True); started=time.monotonic(); commands=[]
    def run(command,label):
        """Preserve exact subprocess outcomes; missing SDKs and failures are never counted as qualification."""
        argv=[str(value) for value in command]
        result=subprocess.run(argv,capture_output=True,text=True,encoding='utf-8',timeout=1200)
        (build/(label+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        commands.append({'argv':argv,'returncode':result.returncode})
        (build/'commands.json').write_text(json.dumps(commands,indent=2)+'\n',encoding='utf-8')
        if result.returncode: print(result.stdout+result.stderr,end=''); result.check_returncode()
        return result.stdout+result.stderr
    rows=fixtures(build); opc_rows,vendor=opc_fixtures(build); application_rows=application_fixtures(build/'application')
    model=build/'package.serializer'; model.write_text((ROOT/'test/resources/opc_profile.serializer').read_text(encoding='utf-8').replace('serializer version 1.20.0;','serializer version 1.21.0;')+'''
namespace linked {
  xml(namespace("urn:linked"), scalars(attribute));
  class target { public string id; }
  class holder { public string ref; }
  class digest_model { public digest value; }
  package archive {
    part targets:target { name(dynamic); content_type("application/vnd.target+xml"); relationship("urn:target"); occurs(1,unbounded); key("objects",id); }
    part alternates:target { name(dynamic); content_type("application/vnd.target+xml"); relationship("urn:alternate"); occurs(0,unbounded); key("objects",id); }
    part holders:holder { name(dynamic); content_type("application/vnd.holder+xml"); relationship("urn:holder"); occurs(0,unbounded); reference("objects",ref,targets); }
  }
  package single_archive {
    part target:target { name("/canonical-target.xml"); content_type("application/vnd.target+xml"); relationship("urn:target"); key("objects",id); }
    part request:holder { name("/canonical-request.xml"); content_type("application/vnd.holder+xml"); relationship("urn:holder"); reference("objects",ref,target); }
  }
}
'''+(ROOT/'test/resources/package_application_contract.serializer').read_text(encoding='utf-8').replace('serializer version 1.21.0;',''),encoding='utf-8')
    run([compiler,'--input',model,'--language','swift','--output',build/'Schema.swift'],'generation')
    source=(ROOT/'test/resources/swift_package_main.swift').read_text(encoding='utf-8').replace('try zipChecks(CommandLine.arguments[1])','')
    source+=(ROOT/'test/resources/swift_package_profile_main.swift').read_text(encoding='utf-8')
    source+=(ROOT/'test/resources/swift_package_application_main.swift').read_text(encoding='utf-8')+'''
do {
    let directory=URL(fileURLWithPath:CommandLine.arguments[1],isDirectory:true)
    try zipChecks(CommandLine.arguments[1]);try profileChecks(directory);try applicationChecks(directory.appendingPathComponent("application",isDirectory:true))
} catch let failure as P.Failure {
    FileHandle.standardError.write(Data(("Swift package failure: \\(checkLabel) \\(failure.code) \\(failure.message) part=\\(failure.part) field=\\(failure.fieldPath) link=\\(failure.relationshipId)\\n").utf8));exit(1)
} catch { FileHandle.standardError.write(Data(("Swift SDK error: \\(checkLabel) \\(error)\\n").utf8));exit(1) }
'''
    (build/'main.swift').write_text(source,encoding='utf-8')
    tools=run([args.swiftc,'--version'],'swift-version')
    executable=build/'main.exe'
    run([args.swiftc,'-O','-warnings-as-errors',build/'Schema.swift',build/'main.swift','-o',executable],'compile')
    run([executable,build],'execute')
    collision=build/'collision';collision.mkdir(exist_ok=True)
    (collision/'model.serializer').write_text('''serializer version 1.21.0;
class SerializerPackage {} class SerializerCompression {}
namespace collision { class target { public uint32 id; }
  package archive {
    part extensions:target { name("/target.xml"); content_type("application/vnd.target+xml"); relationship("urn:target"); }
    part profiles:bytes { name(dynamic); content_type(dynamic); relationship("urn:binary"); source(extensions); occurs(0,unbounded); }
    part image_parts:bytes { name(dynamic); content_type(dynamic); relationship("urn:image"); source(extensions); occurs(0,unbounded); }
  }
}
''',encoding='utf-8')
    run([compiler,'--input',collision/'model.serializer','--language','swift','--output',collision/'Schema.swift'],'generation-collision')
    helper=re.search(r'Generated Swift package helper name: (\w+)',(collision/'Schema.swift').read_text(encoding='utf-8')).group(1)
    assert helper!='SerializerPackage'
    (collision/'main.swift').write_text('''import Foundation
typealias P=@helper@
var input=CollisionArchive.Input();var target=CollisionTarget();target.id=7
input.extensions=target;input.profiles=[];input.extensions_2=P.Extensions();input.profiles_2["extensions"] = .deflate
input.imageParts=[];input.profiles_2["image_parts"] = .deflate
let loaded=try CollisionArchive.decode(CollisionArchive.encode(input))
if loaded.extensions?.id != 7 {fatalError("Metadata helper displaced existing role")}
_ = try SerializerPackage().encode(.BINARY_NONE);_ = try SerializerCompression().encode(.BINARY_NONE)
'''.replace('@helper@',helper),encoding='utf-8')
    run([args.swiftc,'-O','-warnings-as-errors',collision/'Schema.swift',collision/'main.swift','-o',collision/'main.exe'],'compile-collision')
    run([collision/'main.exe'],'execute-collision')
    identity=build/'identity';identity.mkdir(exist_ok=True);shared_fixtures(identity/'fixtures')
    run([compiler,'--input',ROOT/'test/resources/opc_identity.serializer','--config',ROOT/'test/resources/opc_identity.ini',
         '--language','swift','--swift.output',identity/'Schema.swift'],'generation-identity')
    identity_source=(identity/'Schema.swift').read_text(encoding='utf-8')
    registry_type=re.search(r'public struct (\w*Registry)\b',identity_source).group(1)
    (identity/'main.swift').write_text((ROOT/'test/resources/swift_package_identity_main.swift').read_text(encoding='utf-8').replace('@registry_type@',registry_type),encoding='utf-8')
    run([args.swiftc,'-O','-warnings-as-errors',identity/'Schema.swift',identity/'main.swift','-o',identity/'main.exe'],'compile-identity')
    run([identity/'main.exe',identity/'fixtures'],'execute-identity')
    for name,status in rows:
        if status != 'valid': continue
        with zipfile.ZipFile(build/name) as archive:
            for index,item in enumerate(archive.infolist()): assert (build/(name+f'.{index}.decoded')).read_bytes()==archive.read(item)
    for index in range(8):
        data=(build/f'swift-{index}.zip').read_bytes()
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            assert archive.namelist()==['a.bin','empty.bin','z/data.bin']
            assert archive.read('z/data.bin')==PAYLOAD and archive.read('a.bin')==b'stored' and archive.read('empty.bin')==b''
            assert all(item.date_time==(1980,1,1,0,0,0) for item in archive.infolist())
            assert all(item.flag_bits & 8 == (8 if index&4 else 0) for item in archive.infolist())
            assert (b'PK\x06\x06' in data)==bool(index&2)
            assert all((item.extract_version==45)==bool(index&3) for item in archive.infolist())
    for filename in ('swift.opc','swift-preserved.opc','profile-save.opc'):
        with zipfile.ZipFile(build/filename) as archive:
            assert '3D/model.xml' in archive.namelist() and 'Alternate/root.xml' not in archive.namelist()
            root=etree.fromstring(archive.read('3D/model.xml')); assert root.get('name')=='independent'
            assert archive.read('Textures/raw.bin')==b'\x00\x01\xfe\xff'
            if filename=='swift.opc': assert archive.getinfo('3D/model.xml').compress_type==zipfile.ZIP_STORED
            if filename=='swift-preserved.opc':
                assert archive.read('Extensions/vendor.xml')==vendor['Extensions/vendor.xml']
                links=etree.fromstring(archive.read('3D/_rels/model.xml.rels'))
                assert any(link.get('Id')=='vendor-backlink' and link.get('Target')=='/Extensions/vendor.xml' for link in links)
    with zipfile.ZipFile(build/'swift-linked.opc') as archive:
        assert etree.fromstring(archive.read('holder.xml')).get('ref')=='two'
    with zipfile.ZipFile(build/'swift-unicode.zip') as archive:
        assert archive.namelist()==['e\u0301.bin','\u00e9.bin']
        assert archive.read('\u00e9.bin')==b'composed' and archive.read('e\u0301.bin')==b'decomposed'
    for label,compression in [('default',zipfile.ZIP_DEFLATED),('runtime',zipfile.ZIP_STORED)]:
        with zipfile.ZipFile(build/'application'/f'produced-{label}.opc') as archive:
            assert archive.getinfo('t.xml').compress_type==compression and archive.getinfo('fixed.xml').compress_type==zipfile.ZIP_STORED
            assert etree.fromstring(archive.read('t.xml')).attrib=={'id':'7','name':'seven & checked'}
    with zipfile.ZipFile(identity/'fixtures/produced.opc') as archive:
        for index in range(3):
            root=etree.fromstring(archive.read(f'Registries/registry{index}.xml'))
            assert len(list(root))==3
    evidence={'sdk':tools,'cases':len(rows),'opc_cases':len(opc_rows),'application_cases':len(application_rows),'writer_forms':8,'collision_helper':helper,'elapsed_seconds':time.monotonic()-started,
        'source_sha256':hashlib.sha256((build/'Schema.swift').read_bytes()).hexdigest(),
        'identity_source_sha256':hashlib.sha256((identity/'Schema.swift').read_bytes()).hexdigest(),'identity_references':15,'commands':commands}
    (build/'verification.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    print(f'PASS: actual Swift SDK, {len(rows)} independent ZIP fixtures, {len(opc_rows)} invalid OPC fixtures, {len(application_rows)} common application fixtures, eight wire output forms and generated profiles/references')


if __name__=='__main__': main()
