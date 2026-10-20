# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated JS/TS ZIP/OPC with independent Python producers and consumers."""
import argparse
import hashlib
import io
import ipaddress
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as etree
import zipfile

from python_package_test import (CT, MC, MODEL_MIME, RELS, independent_collection_parts,
                                 independent_content_type_parts, independent_parts,
                                 independent_zip, wire_descriptor_zip, independent_external_relationships)
from package_zip64_test import count_fixture, wire_classic, wire_zip64, U32
from package_node_dependencies import prepare_node_dependencies

ROOT = Path(__file__).resolve().parent.parent


def run(command, directory):
    """Execute the real compiler/SDK and retain its ordinary diagnostics."""
    subprocess.run([str(value) for value in command], cwd=directory, check=True)


def fixtures(directory):
    """Author a common independent corpus without using generated ZIP or OPC metadata writers."""
    rows = []

    def add(name, data, expected='valid', kind='profile'):
        """Persist one decision and its intentionally selected API surface."""
        filename = name + '.zip'
        (directory / filename).write_bytes(data)
        rows.append({'name': name, 'file': filename, 'expected': expected, 'kind': kind})

    for wide in (False, True):
        for descriptor in (False, True):
            add(f'opc-{wide}-{descriptor}', independent_zip(independent_parts(), descriptor, wide))
    add('extended', independent_zip(independent_parts(True)), kind='extensions')
    add('collection', independent_zip(independent_collection_parts()), kind='collection')
    for index in range(3):
        parts = independent_collection_parts()
        name = [name for name in parts if name.startswith('Models/')][index]
        parts[name] = b'<broken'
        add('collection-broken-' + str(index), independent_zip(parts), 'invalid_metadata', 'collection')
    for wide in (False, True):
        for signed in (False, True):
            for index, payload in enumerate((b'', b'abc' * 1000, b'\xac\x0a\x7a\xd5')):
                add(f'descriptor-{wide}-{signed}-{index}', wire_descriptor_zip(payload, wide, signed), kind='descriptor')
            add(f'descriptor-tail-{wide}-{signed}', wire_descriptor_zip(b'abc', wide, signed, b'\0'), 'integrity', 'descriptor')
            data = bytearray(wire_descriptor_zip(b'abc', wide, signed))
            central = struct.unpack_from('<I', data, len(data) - 6)[0]
            descriptor_size = (20 if wide else 12) + (4 if signed else 0)
            data[central - descriptor_size + (4 if signed else 0)] ^= 1
            add(f'descriptor-crc-{wide}-{signed}', bytes(data), 'integrity', 'descriptor')
    malformed = ROOT / 'test/resources/package_malformed_fixtures'
    for row in (malformed / 'cases.txt').read_text().splitlines():
        if not row or row.startswith('#'):
            continue
        expected, name = row.split()
        add('malformed-' + Path(name).stem, (malformed / name).read_bytes(), 'invalid_archive' if expected == 'index' else expected)
    defaults = ('<Default Extension="bin" ContentType="application/octet-stream"/>'
                '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>')
    main = '<Override PartName="/Alternate/root.xml" ContentType="' + MODEL_MIME + '"/>'
    base = defaults + main
    duplicates = ('<Default Extension="bin" ContentType="application/octet-stream"/>',
                  '<Default Extension="bin" ContentType="text/plain"/>',
                  '<Default Extension="BIN" ContentType="text/plain"/>',
                  '<Default Extension="bi%3An" ContentType="text/plain"/><Default Extension="BI%3aN" ContentType="application/octet-stream"/>',
                  main, '<Override PartName="/Alternate/root.xml" ContentType="text/plain"/>',
                  '<Override PartName="/ALTERNATE/ROOT.XML" ContentType="text/plain"/>',
                  '<Override PartName=" /Alternate/root.xml &#x9;" ContentType="text/plain"/>',
                  '<Override PartName="/Extras/part%3Aname.bin" ContentType="text/plain"/><Override PartName="/EXTRAS/PART%3aname.BIN" ContentType="application/octet-stream"/>')
    for index, duplicate in enumerate(duplicates):
        for order, declarations in enumerate((base + duplicate, duplicate + base)):
            add(f'content-duplicate-{index}-{order}', independent_zip(independent_content_type_parts(declarations)), 'invalid_metadata', 'metadata')
    for index, declarations in enumerate((base, base + '<Override PartName="/Textures/raw.bin" ContentType="application/custom+binary"/>', base + '<Override PartName=" /EXTRAS/PART%3aname.BIN &#x9;" ContentType="text/plain"/>')):
        add('content-valid-' + str(index), independent_zip(independent_content_type_parts(declarations)), kind='metadata')
    binary, rels = defaults.split('/>', 1)
    binary += '/>'
    malformed_types = ['', rels + main, binary + rels, rels + main + '<Default Extension="bin"/>',
                       rels + main + '<Default ContentType="text/plain"/>', binary + rels + '<Override PartName="/Alternate/root.xml"/>', binary + rels + '<Override ContentType="text/plain"/>']
    for mime in ('', 'text', 'application/{invalid}', 'application/octet-stream; p=&quot;unterminated',
                 'APPLICATION/VND.OPENXMLFORMATS-PACKAGE.RELATIONSHIPS+XML',
                 'application/vnd.openxmlformats-package.core-properties+xml; charset=utf-8'):
        malformed_types += [rels + main + '<Default Extension="bin" ContentType="' + mime + '"/>', binary + rels + '<Override PartName="/Alternate/root.xml" ContentType="' + mime + '"/>']
    for index, declarations in enumerate(malformed_types):
        add('content-malformed-' + str(index), independent_zip(independent_content_type_parts(declarations)), 'invalid_metadata', 'metadata')
    add('content-missing', independent_zip(independent_content_type_parts('', False)), 'invalid_metadata', 'metadata')
    for encoding, bom in (('utf-16-le', b'\xff\xfe'), ('utf-16-be', b'\xfe\xff')):
        for declared in (False, True):
            parts = independent_parts()
            for name in ('[Content_Types].xml', '_rels/.rels', 'Alternate/_rels/root.xml.rels'):
                parts[name] = bom + (('<?xml version="1.0" encoding="UTF-16"?>' if declared else '') + parts[name].decode()).encode(encoding)
            add(f'utf16-{encoding}-{declared}', independent_zip(parts))
    targets = ('https://left/right', 'https://left\uff0fright', 'https://left?right', 'https://left\uff1fright',
               'https://left#right', 'https://left\uff03right', 'https://left@right', 'https://left\uff20right',
               'https://left:80', 'https://left\uff1a80', 'https://user\uff20name@host.invalid/a',
               'https://host\uff0fpart.invalid:443/a?view=\ue000#section', 'https://caf\u00e9.invalid/a',
               'https://host%2Fname.invalid/a', 'https://[2001:db8::1]:443/a', 'https://[vF.name:tail]/a',
               'https:///path', '//host\uff1fname.invalid/path', 'http://[::]/', 'http://[::1]/',
               'http://[1:2:3:4:5:6:7:8]/', 'http://[1:2:3:4:5:6:192.168.1.1]/',
               'http://[::ffff:192.168.1.1]/', 'http://[1::192.168.1.1]/')
    for target in targets:
        if target.startswith('http://['):
            ipaddress.IPv6Address(target[len('http://['):target.index(']')])
    add('external-valid', independent_zip(independent_external_relationships(targets)), kind='extensions')
    for index, target in enumerate(('https://host:port/', 'https://host:\uff11/', 'https://[::1/', 'https://[::1]tail/',
                                    'https://[not-ip]/', 'https://[1:2:3]/', 'https://[fe80::1%25zone]/',
                                    'https://host]/', 'https://[::1]]/', 'https://user@@host/', 'https://host/%zz',
                                    'https://host/%', 'https://host/a#one#two', '1scheme://host/', 'https://host\\evil/',
                                    'https://host\ue000.invalid/', 'https://host/\ue000', 'https://host/#\ue000',
                                    'http://[:::]/', 'http://[:::1]/', 'http://[1:::]/', 'http://[1::2::3]/',
                                    'http://[1:2:3:4:5:6:7:8::]/', 'http://[:1:2:3:4:5:6:7]/',
                                    'http://[1:2:3:4:5:6:7:]/', 'http://[::ffff:192.168.001.1]/')):
        add('external-invalid-' + str(index), independent_zip(independent_external_relationships((target,))), 'invalid_relationship', 'extensions')
        rows[-1]['target'] = target
        if target.startswith('http://['):
            try:
                ipaddress.IPv6Address(target[len('http://['):target.index(']')])
            except ValueError:
                pass
            else:
                raise AssertionError('Malformed independent IPv6 fixture is valid')
    for index, target in enumerate(('./Alternate/root.xml?view=2#section', 'Alternate/./root.xml?#', '/Alternate/root.xml?next=a/b#value?with:punctuation')):
        parts = independent_parts(); parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'Alternate/root.xml', target.encode())
        add('internal-valid-' + str(index), independent_zip(parts))
    for index, target in enumerate(('//host/Alternate/root.xml', '///Alternate/root.xml', '//', 'https://host/Alternate/root.xml', 'name:scheme')):
        parts = independent_parts(); parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'Alternate/root.xml', target.encode())
        add('internal-invalid-' + str(index), independent_zip(parts), 'invalid_relationship')
    parts = independent_parts()
    excluded = {'Textures/': b'', '[trash]/0000.dat': b'discarded data', '[trash]/0000.dat/child.bin': b'ignored descendant',
                '[trash]/CASE': b'first', '[trash]/case': b'second', 'has space.bin': b'spaced name',
                'invalid%ZZ.bin': b'bad escape', 'trailing./item.bin': b'bad segment',
                '[trash]/caf\u00e9.bin': b'not a logical part', 'dos-dir.bin': b'directory payload',
                'dos-volume.bin': b'volume label', 'ntfs-dir.bin': b'NTFS directory', 'vfat-dir.bin': b'VFAT directory',
                'unix-link.bin': b'symlink target', 'unix-dir.bin': b'Unix directory', 'darwin-socket.bin': b'socket data'}
    attributes = {'dos-dir.bin': (0, 0x10), 'dos-volume.bin': (0, 0x08), 'ntfs-dir.bin': (10, 0x10),
                  'vfat-dir.bin': (14, 0x10), 'unix-link.bin': (3, 0o120777 << 16), 'unix-dir.bin': (3, 0o040755 << 16),
                  'darwin-socket.bin': (19, 0o140755 << 16), 'Textures/raw.bin': (3, 0o644 << 16)}
    parts.update(excluded); add('physical-excluded', independent_zip(parts, attributes=attributes))
    for index, name in enumerate(('plain-\u00e9.bin', 'plain-%c3%a9.bin')):
        parts = independent_parts(); parts[name] = b'unsupported candidate'
        add('physical-unicode-' + str(index), independent_zip(parts), 'unsupported_feature')
    beginning = f'<Relationships xmlns="{RELS}" xmlns:r="{RELS}" xmlns:mc="{MC}" xmlns:e="urn:extension" '
    relationship = '<Relationship Id="{id}" Type="{kind}" Target="Alternate/root.xml"/>'
    body = (beginning + 'mc:Ignorable="e" mc:ProcessContent="e:box" mc:MustUnderstand="r" e:flag="ignored">'
            '<e:discard mc:MustUnderstand="e">unknown<bad/>content</e:discard><e:box>' +
            relationship.format(id='main', kind='urn:serializer:main-model').replace('/>', '>before<e:discard/>after</Relationship>') +
            '</e:box><mc:AlternateContent e:flag="ignored"><mc:Choice Requires="e" mc:MustUnderstand="e"><bad/></mc:Choice>'
            '<mc:Choice Requires="r">' + relationship.format(id='choice', kind='urn:vendor:link') +
            '</mc:Choice><mc:Fallback mc:MustUnderstand="e"><bad/></mc:Fallback><e:future/></mc:AlternateContent>'
            '<mc:AlternateContent><mc:Choice Requires="e"><bad/></mc:Choice><mc:Fallback>' +
            relationship.format(id='fallback', kind='urn:vendor:link') + '</mc:Fallback></mc:AlternateContent>'
            '<mc:AlternateContent><mc:Choice Requires="e"><bad/></mc:Choice></mc:AlternateContent>'
            '<e:box xmlns:e="urn:rebound" mc:Ignorable="e" mc:ProcessContent="e:*">' +
            relationship.format(id='rebound', kind='urn:vendor:link') + '</e:box></Relationships>')
    parts = independent_parts(); parts['_rels/.rels'] = body.encode(); add('mce-valid', independent_zip(parts), kind='extensions')
    parts = independent_parts(); parts['_rels/.rels'] = (f'<mc:AlternateContent xmlns:mc="{MC}" xmlns:r="{RELS}"><mc:Choice Requires="xml"><bad/></mc:Choice><mc:Fallback><r:Relationships>' + relationship.format(id='main', kind='urn:serializer:main-model').replace('<Relationship', '<r:Relationship') + '</r:Relationships></mc:Fallback></mc:AlternateContent>').encode()
    add('mce-root-valid', independent_zip(parts))
    for index, body in enumerate(('mc:Ignorable="missing">', 'mc:Ignorable="mc">', 'mc:ProcessContent="e:box">',
            'mc:Ignorable="e" mc:ProcessContent="e">', 'mc:Ignorable="e" mc:ProcessContent="e:bad:name">',
            'mc:MustUnderstand="e">', 'mc:MustUnderstand="xml">', 'mc:Unknown="">', '><e:unknown/>',
            '><mc:Choice Requires="r"/>', '><mc:Fallback/>', '><mc:Unknown/>', '><mc:AlternateContent/>',
            '><mc:AlternateContent><mc:Fallback/></mc:AlternateContent>', '><mc:AlternateContent><mc:Choice/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires=""/></mc:AlternateContent>', '><mc:AlternateContent><mc:Choice Requires="missing"/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="mc"/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="r"/><mc:Fallback/><mc:Fallback/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="r"/><mc:Fallback/><mc:Choice Requires="r"/></mc:AlternateContent>',
            '><mc:AlternateContent bad="x"><mc:Choice Requires="r"/></mc:AlternateContent>',
            '><mc:AlternateContent xml:lang="en"><mc:Choice Requires="r"/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="r" e:flag="x"/></mc:AlternateContent>',
            '><mc:AlternateContent><mc:Choice Requires="r" mc:MustUnderstand="e"/></mc:AlternateContent>',
            'mc:Ignorable="e" mc:ProcessContent="e:*"><e:box xml:base="x"/>',
            'mc:Ignorable="e" mc:ProcessContent="e:*"><e:box xml:lang="en"/>',
            'mc:Ignorable="e" mc:ProcessContent="e:*"><e:box xml:space="preserve"/>',
            'mc:Ignorable="e" mc:ProcessContent="e:*"><mc:AlternateContent><e:box/><mc:Choice Requires="r"/></mc:AlternateContent>')):
        parts = independent_parts(); parts['_rels/.rels'] = (beginning + body + relationship.format(id='main', kind='urn:serializer:main-model') + '</Relationships>').encode()
        add('mce-invalid-' + str(index), independent_zip(parts), 'invalid_metadata', 'extensions')
    for count in (65534, 65535, 65536):
        fixture = count_fixture(count)
        add('count-' + str(count), fixture.segments[0][1], kind='count')
    for label, index in (('early', 1), ('middle', 32768), ('late', 65535)):
        add('count-duplicate-' + label, count_fixture(65536, index).segments[0][1], 'invalid_archive', 'count')
    segmented = []
    cases = [('local-' + str(offset), wire_zip64(offset), 'valid') for offset in (U32 - 1, U32, U32 + 1)]
    cases += [('directory-' + str(offset), wire_zip64(0, offset), 'valid') for offset in (U32 - 1, U32, U32 + 1)]
    cases += [('classic-max', wire_classic(U32), 'valid'), ('ordinary-wide', wire_zip64(0, ordinary=True), 'valid')]
    sample = wire_zip64(U32 + 1)
    for label, offset, pattern, value, expected in (
            ('locator-past-end', sample.locator + 8, '<Q', sample.size, 'invalid_archive'),
            ('locator-u64-max', sample.locator + 8, '<Q', (1 << 64) - 1, 'resource_limit'),
            ('record-short', sample.record + 4, '<Q', 43, 'invalid_archive'),
            ('record-u64-max', sample.record + 4, '<Q', (1 << 64) - 1, 'resource_limit'),
            ('split-count', sample.record + 24, '<Q', 2, 'invalid_archive'),
            ('directory-past-end', sample.record + 48, '<Q', sample.size, 'invalid_archive'),
            ('directory-size-u64-max', sample.record + 40, '<Q', (1 << 64) - 1, 'resource_limit'),
            ('narrow-size-mismatch', sample.end + 12, '<I', 1, 'invalid_archive'),
            ('narrow-count-mismatch', sample.end + 8, '<HH', (2, 2), 'invalid_archive')):
        values = value if isinstance(value, tuple) else (value,)
        cases.append((label, sample.patch(offset, struct.pack(pattern, *values)), expected))
    for name, fixture, expected in cases:
        item = {'name': name, 'size': fixture.size, 'payload': fixture.payload.hex(), 'segments': [], 'expected': expected}
        for index, (offset, data) in enumerate(fixture.segments):
            filename = f'{name}-{index}.segment'; (directory / filename).write_bytes(data); item['segments'].append([offset, filename])
        segmented.append(item)
    (directory / 'fixtures.json').write_text(json.dumps(rows))
    (directory / 'segmented.json').write_text(json.dumps(segmented))
    return len(rows), len(segmented)


JAVASCRIPT = r'''import assert from 'node:assert/strict';
import {readFile,writeFile,readdir,mkdir} from 'node:fs/promises';
import {join} from 'node:path';
import * as M from './schema.mjs';
const P=M.__PACKAGE__,C=M.__COMPRESSION__,A=M.ProfileArchive,Collection=M.ProfileCollectionArchive;
const directory=process.argv[2],provider=await C.nodeProvider(),options={compressionProvider:provider};
const bytes=async name=>new Uint8Array(await readFile(join(directory,name)));
const skip=new P.ExtensionPolicy({parts:'skip',relationships:'skip'});
const preserve=new P.ExtensionPolicy({parts:'preserve',relationships:'preserve'});
async function reject(code,action){await assert.rejects(action,error=>error instanceof P.Error&&error.code===code);}
async function same(error,action){await assert.rejects(action,actual=>actual===error);}
let decisions=0;
for(const row of JSON.parse(await readFile(join(directory,'fixtures.json'),'utf8'))){
 const data=await bytes(row.file);
 const action=async()=>{
  if(row.kind==='descriptor'||row.kind==='count'){
   const reader=await P.ZipReader.open(data,{...options,limits:row.kind==='count'?{maxEntries:70000,maxMetadataBytes:32*1024*1024,maxDecoderAllocationBytes:64*1024*1024}:undefined});
   try{if(row.kind==='descriptor')await reader.read('data.bin');else{assert.equal(reader.entries.length,Number(row.name.slice(6)));await reader.read(reader.entries.at(-1).name);}}finally{await reader.close();}
  }else{
   const facade=row.kind==='collection'?Collection:A;
   const result=await facade.load(data,{...options,policy:row.kind==='metadata'?skip:row.kind==='extensions'?preserve:undefined});
   if(row.kind==='collection')assert.equal(result.value.models.length,3);else assert.equal(result.value.main.name,'independent');
  }
 };
 if(row.expected==='valid')await action();else await reject(row.expected,action);++decisions;
}
for(const row of JSON.parse(await readFile(join(directory,'segmented.json'),'utf8'))){
 const segments=await Promise.all(row.segments.map(async([at,file])=>[at,await bytes(file)]));let maximum=0,calls=0,total=0;
 const source={size:row.size,readAt(at,count){assert.ok(count<=32768);++calls;maximum=Math.max(maximum,count);total+=count;const result=new Uint8Array(count);for(const[start,data]of segments){const first=Math.max(at,start),last=Math.min(at+count,start+data.length);if(first<last)result.set(data.subarray(first-start,last-start),first-at);}return result;}};
 const action=async()=>{const reader=await P.ZipReader.open(source,{limits:{maxArchiveBytes:row.size,maxIoBytes:1024*1024}});try{assert.equal(Buffer.from(await reader.read('boundary.bin')).toString('hex'),row.payload);}finally{await reader.close();}};
 if(row.expected==='valid')await action();else await reject(row.expected,action);assert.ok(total<1024*1024&&maximum<=32768&&calls>0);++decisions;
}
let outputs=0;
for(const compression of ['stored','deflate'])for(const descriptor of ['none','signed','unsigned'])for(const forceZip64 of [false,true]){
 const writer=new P.ZipWriter({...options,descriptor,forceZip64});await writer.add({name:'z.bin',data:new Uint8Array([0,1,254,255]),compression});await writer.add({name:'a.txt',data:new TextEncoder().encode('abc'.repeat(1000)),compression});await writer.add({name:'empty.bin',data:new Uint8Array(),compression});
 await writeFile(join(directory,`output-${compression}-${descriptor}-${forceZip64}.zip`),await writer.finish());++outputs;
}
const control=await bytes('opc-False-False.zip'), loaded=await A.load(control,options);
const input=new A.Input({main:loaded.value.main,attachments:loaded.value.attachments});
const first=await A.encode(input,options);await writeFile(join(directory,'output-profile.opc'),first);assert.equal((await A.load(first,options)).value.main.name,'independent');
const extended=await A.load(await bytes('extended.zip'),{...options,policy:preserve});assert.equal(extended.extensions.parts.length,1);assert.equal(extended.extensions.relationships.length,1);
await writeFile(join(directory,'output-preserved.opc'),await A.encode(extended.value,options));
// Reject/skip/preserve are independent for whole opaque parts and source-scoped links.
for(const parts of ['reject','skip','preserve'])for(const relationships of ['reject','skip','preserve']){
 const policy=new P.ExtensionPolicy({parts,relationships});const action=()=>A.load(awaitableExtended,{...options,policy});
 var awaitableExtended=await bytes('extended.zip');
 if(parts==='reject')await reject(P.Code.UnknownPart,action);else if(relationships==='reject')await reject(P.Code.UnknownRelationship,action);else{const result=await action();assert.equal(result.extensions.parts.length,parts==='preserve'?1:0);assert.equal(result.extensions.relationships.length,relationships==='preserve'?1:0);}
}
for(const kind of ['Part','Relationship'])await reject(kind==='Part'?P.Code.UnknownPart:P.Code.UnknownRelationship,()=>A.load(awaitableExtended,{...options,policy:new P.ExtensionPolicy({parts:'skip',relationships:'skip',['isRequired'+kind]:()=>true})}));
// Preserved links are revalidated when editing removes the corresponding whole part.
extended.value.extensions=new P.Extensions([],extended.extensions.relationships);await reject(P.Code.MissingPart,()=>A.encode(extended.value,options));
// Output is stable under authored order permutations, including extension/link groups.
const pp=[new P.BinaryPart(new Uint8Array([1]),'/a.bin','application/octet-stream'),new P.BinaryPart(new Uint8Array([2]),'/b.bin','application/octet-stream')];
const rr=[new P.Relationship('/','b','urn:test:b','/b.bin'),new P.Relationship('/','a','urn:test:a','/a.bin')];
assert.deepEqual(await P.encode(pp,rr,options),await P.encode(pp.slice().reverse(),rr.slice().reverse(),options));
// Snapshot byte input before the first await and keep borrowed source/sink lifecycle untouched.
const mutable=new Uint8Array([1,2,3]);const writer=new P.ZipWriter(options);const adding=writer.add({name:'data',data:mutable});mutable.fill(0);await adding;const snapshot=await writer.finish();const snapshotReader=await P.ZipReader.open(snapshot);assert.deepEqual(await snapshotReader.read('data'),new Uint8Array([1,2,3]));await snapshotReader.close();
let reads=0,closes=0;const short={size:control.length,readAt(at,count){++reads;return control.subarray(at,at+Math.min(count,7));},close(){++closes;}};await A.load(short,options);assert.ok(reads>0);assert.equal(closes,0);
const cancellation=new Error('original cancellation'),transport=new Error('original transport');
await same(cancellation,()=>A.load(control,{...options,limits:{checkCancelled(){throw cancellation;}}}));
await same(transport,()=>A.load({size:control.length,readAt(){throw transport;}},options));
let writes=0;await same(transport,()=>A.write({write(){++writes;throw transport;}},input,options));assert.equal(writes,1);
let validated=0;await same(cancellation,()=>A.load(control,{...options,validate:async()=>{++validated;throw cancellation;}}));assert.equal(validated,1);
// One shared operation ledger admits exact limits and rejects one-tight cumulative limits before publication.
const ledger=new P.Budget();await A.load(control,{...options,budget:ledger});
const limitFor={ioBytes:'maxIoBytes',metadataBytes:'maxMetadataBytes',expandedBytes:'maxTotalExpandedBytes',entries:'maxEntries',relationships:'maxRelationships',decoderAllocationBytes:'maxDecoderAllocationBytes',workUnits:'maxWorkUnits',references:'maxReferences'};
for(const[resource,limit]of Object.entries(limitFor)){
 await A.load(control,{...options,limits:{[limit]:ledger.used[resource]}});
 if(ledger.used[resource]){let published=false;await reject(P.Code.ResourceLimit,()=>A.load(control,{...options,limits:{[limit]:ledger.used[resource]-1},validate(){published=true;}}));assert.equal(published,false);}
}
// Missing provider admission occurs before native encoder or sink policy callbacks.
let effects=0;const unavailable=new A.Input({main:loaded.value.main,attachments:[]});const originalEncoder=unavailable.main.encodeXml;
unavailable.main.encodeXml=()=>{++effects;return originalEncoder.call(unavailable.main);};
await reject(P.Code.Unavailable,()=>A.write({write(){++effects;}},unavailable,{limits:{checkCancelled(){++effects;}}}));assert.equal(effects,0);unavailable.main.encodeXml=originalEncoder;
// Close at every observed cancellation boundary, including after the final borrowed sink callback.
let fencedBoundaries=0;
for(const operation of ['add','finish','writeTo']){
 let total=0,armed=false;let w=new P.ZipWriter({...options,limits:{checkCancelled(){if(armed)++total;}}});
 if(operation!=='add')await w.add({name:'part',data:new Uint8Array([1,2,3]),compression:'deflate'});armed=true;
 if(operation==='add')await w.add({name:'part',data:new Uint8Array([1,2,3]),compression:'deflate'});else if(operation==='finish')await w.finish();else await w.writeTo({write(){}});
 for(let at=1;at<=total;++at){
  let count=0,closed=false,enabled=false,calls=0;let writer;
  writer=new P.ZipWriter({...options,limits:{checkCancelled(){if(enabled&&++count===at){closed=true;writer.close();}}}});
  if(operation!=='add')await writer.add({name:'part',data:new Uint8Array([1,2,3]),compression:'deflate'});enabled=true;
  const sink={write(){assert.equal(closed,false);++calls;}};
  await reject(P.Code.InvalidOptions,()=>operation==='add'?writer.add({name:'part',data:new Uint8Array([1,2,3]),compression:'deflate'}):operation==='finish'?writer.finish():writer.writeTo(sink));
  const before=calls;await reject(P.Code.InvalidOptions,()=>writer.writeTo(sink));assert.equal(calls,before);++fencedBoundaries;
 }
}
// Reentry poisons the active operation even when the application handles the nested rejection.
let nested,armed=false;const reentrant=new P.ZipWriter({...options,limits:{checkCancelled(){if(armed&&!nested)nested=reentrant.finish();}}});
await reentrant.add({name:'a',data:new Uint8Array([1]),compression:'deflate'});armed=true;
const outer=await reentrant.finish().then(()=>{throw Error('outer succeeded');},error=>error);const inner=await nested.then(()=>{throw Error('nested succeeded');},error=>error);assert.equal(outer,inner);await same(inner,()=>reentrant.add({name:'b',data:new Uint8Array()}));
// Source callback closure/reentry is observed before any subsequent provider or read call.
let activeReader,sourceClosed=false,sourceCalls=0,closeOnRead=false;
const guarded={size:snapshot.length,readAt(at,count){assert.equal(sourceClosed,false);++sourceCalls;if(closeOnRead){sourceClosed=true;void activeReader.close();}return snapshot.subarray(at,at+count);}};
activeReader=await P.ZipReader.open(guarded);closeOnRead=true;await reject(P.Code.InvalidOptions,()=>activeReader.read('data'));const priorCalls=sourceCalls;await reject(P.Code.InvalidOptions,()=>activeReader.read('data'));assert.equal(sourceCalls,priorCalls);
// Explicit Node spooling is bounded and cleans private files on success, cancellation and failure.
const host=await P.nodeHost(),spool=join(directory,'spool');await mkdir(spool);let position=0;
const invalidDestination=join(directory,'invalid-uri.opc'),invalidOriginal=new Uint8Array([5,4,3]);await writeFile(invalidDestination,invalidOriginal);
for(const row of JSON.parse(await readFile(join(directory,'fixtures.json'),'utf8')))if(row.target){
 const invalid=new A.Input({main:loaded.value.main,attachments:loaded.value.attachments,extensions:new P.Extensions([],[new P.Relationship('/','invalid','urn:test:external',row.target,'external')])});
 await reject(P.Code.InvalidRelationship,()=>A.encode(invalid,options));
 await reject(P.Code.InvalidRelationship,()=>A.save(invalidDestination,invalid,{...options,host}));assert.deepEqual(new Uint8Array(await readFile(invalidDestination)),invalidOriginal);
}
function sequential(fail=false){position=0;return{read(maximum){if(fail)throw transport;if(position===control.length)return null;const chunk=control.subarray(position,position+Math.min(maximum,7));position+=chunk.length;return chunk;},close(){++closes;}};}
await A.load(sequential(),{...options,host,spoolDirectory:spool,maxSpoolBytes:control.length});assert.deepEqual(await readdir(spool),[]);assert.equal(closes,0);
await reject(P.Code.ResourceLimit,()=>A.load(sequential(),{...options,host,spoolDirectory:spool,maxSpoolBytes:control.length-1}));assert.deepEqual(await readdir(spool),[]);
await same(transport,()=>A.load(sequential(true),{...options,host,spoolDirectory:spool}));assert.deepEqual(await readdir(spool),[]);
let cancelSpool=false,spoolCalls=0;
await same(cancellation,()=>A.load({read(maximum){++spoolCalls;cancelSpool=true;return control.subarray(0,Math.min(maximum,7));},close(){++closes;}},{...options,host,spoolDirectory:spool,limits:{checkCancelled(){if(cancelSpool)throw cancellation;}}}));assert.equal(spoolCalls,1);assert.equal(closes,0);assert.deepEqual(await readdir(spool),[]);
// Close while a borrowed sink promise is pending; no later chunk may escape.
let awaken,started;const gate=new Promise(resolve=>{awaken=resolve;}),announced=new Promise(resolve=>{started=resolve;});let delayedCalls=0;
const delayed=new P.ZipWriter(options);await delayed.add({name:'data',data:new Uint8Array(40000)});const writing=delayed.writeTo({async write(){++delayedCalls;started();await gate;}});await announced;delayed.close();awaken();await reject(P.Code.InvalidOptions,()=>writing);assert.equal(delayedCalls,1);await reject(P.Code.InvalidOptions,()=>delayed.finish());
// Invalid asynchronous policy hooks reject without creating detached unhandled rejections.
const unhandled=[];const onUnhandled=error=>unhandled.push(error);process.on('unhandledRejection',onUnhandled);
try{
 for(const kind of ['Part','Relationship'])await reject(P.Code.InvalidOptions,()=>A.load(awaitableExtended,{...options,policy:new P.ExtensionPolicy({parts:'skip',relationships:'skip',['isRequired'+kind]:async()=>{throw cancellation;}})}));
 await reject(P.Code.InvalidOptions,()=>A.load(control,{...options,limits:{clock:async()=>{throw cancellation;},deadline:1}}));
 await new Promise(resolve=>setImmediate(resolve));assert.deepEqual(unhandled,[]);
}finally{process.off('unhandledRejection',onUnhandled);}
// Atomic save faults leave the exact original file or a complete independently readable replacement.
const destination=join(directory,'saved.opc'),original=new Uint8Array([9,8,7]);
for(const stage of ['before_create','before_file_sync','before_replace','after_replace','after_directory_sync']){
 await writeFile(destination,original);const action=()=>A.save(destination,input,{...options,host,fault(actual){if(actual===stage)throw transport;}});
 if(stage.startsWith('after_')){await reject(P.Code.PublicationUncertain,action);assert.equal((await A.load(destination,{...options,host})).value.main.name,'independent');}else{await same(transport,action);assert.deepEqual(new Uint8Array(await readFile(destination)),original);}
 assert.deepEqual((await readdir(directory)).filter(name=>name.startsWith('.serializer-package-')),[]);
}
await A.save(destination,input,{...options,host});assert.equal((await A.load(destination,{...options,host})).value.main.name,'independent');
console.log(JSON.stringify({independentInputDecisions:decisions,independentZipOutputs:outputs,profileOutputs:2,fencedBoundaries}));
'''


TYPESCRIPT = r'''import {__PACKAGE__ as P,__COMPRESSION__ as C,ProfileArchive as A,ProfileModel} from './schema.mjs';
async function check():Promise<void>{
 const provider=await C.nodeProvider(),host=await P.nodeHost();
 const model=new ProfileModel();model.name='typed';
 const input=new A.Input({main:model,attachments:[new P.BinaryPart(new Uint8Array(),'/a.bin','application/octet-stream')]});
 const bytes:Uint8Array=await A.encode(input,{compressionProvider:provider});
 const result:P.Result<A.Value>=await A.load(bytes,{compressionProvider:provider,validate:async(value,budget)=>{const typed:ProfileModel|null=value.main;budget.consume('references',1);void typed;}});
 await A.save('typed.opc',result.value,{compressionProvider:provider,host});
 const writer=new P.ZipWriter({compressionProvider:provider,forceZip64:true,descriptor:'unsigned'});await writer.add({name:'a',data:bytes,compression:'deflate'});const archive=await writer.finish();const reader=await P.ZipReader.open(archive,{compressionProvider:provider});await reader.read('a');await reader.close();
 // @ts-expect-error unrelated native payload is not a model role
 new A.Input({main:17});
 // @ts-expect-error explicit filesystem host is required for save
 await A.save('invalid.opc',input,{});
}
void check;
'''


BROWSER = r'''import assert from 'node:assert/strict';import{readFile}from'node:fs/promises';
import{SourceTextModule,SyntheticModule,createContext}from'node:vm';
import{SaxesParser}from'saxes';import{DOMParser}from'@xmldom/xmldom';
const context=createContext({TextEncoder,TextDecoder,Uint8Array,DataView});let dynamic=0;
const source=await readFile(new URL('schema.mjs',import.meta.url),'utf8');
const module=new SourceTextModule(source,{context,importModuleDynamically(){++dynamic;throw Error('Node unavailable');}});
await module.link(async specifier=>{const values=specifier==='saxes'?{SaxesParser}:specifier==='@xmldom/xmldom'?{DOMParser}:['xmlchars/xmlns/1.0/ed3.js','xmlchars/xml/1.0/ed5.js'].includes(specifier)?await import(specifier):null;if(!values)throw Error('Unexpected browser import '+specifier);return new SyntheticModule(Object.keys(values),function(){for(const[key,value]of Object.entries(values))this.setExport(key,value);},{context});});await module.evaluate();
const P=module.namespace.__PACKAGE__,C=module.namespace.__COMPRESSION__;assert.equal(dynamic,0);
const writer=new P.ZipWriter();await writer.add({name:'browser.bin',data:new Uint8Array([1,2,3])});const data=await writer.finish();const reader=await P.ZipReader.open(data);assert.deepEqual(Array.from(await reader.read('browser.bin')),[1,2,3]);await reader.close();assert.equal(dynamic,0);
await assert.rejects(()=>C.nodeProvider(),error=>error.code==='unavailable');assert.equal(dynamic,1);
console.log('PASS: browser context imports no Node provider or host; stored ZIP works without either');
'''


def main():
    """Generate real JS and declarations, execute Node, and independently inspect every emitted ZIP."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--native-producer', type=Path)
    parser.add_argument('--cpp-example', type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    def dependency_run(command, name):
        """Restore the declared XML SDK lock when supplied editor dependencies are incomplete."""
        del name
        run(command, build)

    modules = prepare_node_dependencies(build, args.node_dependencies or ROOT, dependency_run)
    for package, version in (('saxes', '6.0.0'), ('xmlchars', '2.2.0'), ('typescript', '5.9.3'), ('@xmldom/xmldom', '0.9.12')):
        if json.loads((modules / package / 'package.json').read_text(encoding='utf8'))['version'] != version:
            raise RuntimeError('Actual Node dependency differs from the maintained exact lock: ' + package)
    if not args.node:
        parser.error('Actual Node SDK is required')
    with tempfile.TemporaryDirectory(prefix='javascript-package-', dir=build) as temporary:
        directory = Path(temporary)
        count, segmented = fixtures(directory)
        run([args.compiler.resolve(), '--input', ROOT / 'test/resources/opc_profile.serializer', '--language', 'js,typescript', '--js.output', directory / 'schema.mjs', '--typescript.output', directory / 'schema.d.mts'], directory)
        generated = (directory / 'schema.mjs').read_text(encoding='utf8')
        package = re.search(r'Generated JS/TS package helper name: (\w+)', generated)[1]
        compression = re.search(r'Generated JS/TS compression helper name: (\w+)', generated)[1]
        (directory / 'test.mjs').write_text(JAVASCRIPT.replace('__PACKAGE__', package).replace('__COMPRESSION__', compression), encoding='utf8')
        (directory / 'consumer.mts').write_text(TYPESCRIPT.replace('__PACKAGE__', package).replace('__COMPRESSION__', compression), encoding='utf8')
        run([args.node, directory / 'test.mjs', directory], directory)
        (directory / 'browser.mjs').write_text(BROWSER.replace('__PACKAGE__', package).replace('__COMPRESSION__', compression), encoding='utf8')
        run([args.node, '--experimental-vm-modules', directory / 'browser.mjs'], directory)
        run([args.node, modules / 'typescript/bin/tsc', '--strict', '--noEmit', '--target', 'ES2022', '--module', 'NodeNext', '--moduleResolution', 'NodeNext', directory / 'consumer.mts'], directory)
        interop = 0
        if args.native_producer:
            native = directory / 'native'; native.mkdir()
            run([args.native_producer.resolve(), native], directory)
            (directory / 'native.mjs').write_text("import{readFile}from'node:fs/promises';import{__PACKAGE__ as P}from'./schema.mjs';for(const name of ['canonical-first.zip','canonical-second.zip','forced-zip64.zip']){const r=await P.ZipReader.open(new Uint8Array(await readFile(new URL('native/'+name,import.meta.url))));try{if(new TextDecoder().decode(await r.read('a.json'))!=='{\"count\":7}')throw Error('native JSON changed');const b=await r.read('z.bin');if(b.length!==3||b[0]!==97||b[1]!==0||b[2]!==98)throw Error('native binary changed');}finally{await r.close();}}".replace('__PACKAGE__', package), encoding='utf8')
            run([args.node, directory / 'native.mjs'], directory); interop += 3
        if args.cpp_example:
            run([args.compiler.resolve(), '--input', ROOT / 'example/opc/model.serializer', '--language', 'js', '--js.output', directory / 'example.mjs'], directory)
            cpp_path, js_path = directory / 'cpp.opc', directory / 'js.opc'
            run([args.cpp_example.resolve(), cpp_path], directory)
            (directory / 'exchange.mjs').write_text("import{readFile,writeFile}from'node:fs/promises';import{SerializerPackage as P,SerializerCompression as C,OpcExampleDocument as A}from'./example.mjs';const provider=await C.nodeProvider();const result=await A.load(new Uint8Array(await readFile(new URL('cpp.opc',import.meta.url))),{compressionProvider:provider});if(result.value.main.title!=='Parts & tools'||result.value.main.items.length!==2||result.value.main.items[0].id!==7||result.value.main.items[0].label!=='Bolt <M6>'||result.value.main.items[1].id!==8||result.value.main.items[1].label!=='Nut'||result.value.attachments[0].name!=='/Attachments/sample.bin')throw Error('native typed value changed');await writeFile(new URL('js.opc',import.meta.url),await A.encode(result.value,{compressionProvider:provider}));", encoding='utf8')
            run([args.node, directory / 'exchange.mjs'], directory)
            run([args.cpp_example.resolve(), '--read', js_path], directory)
            for path in (cpp_path, js_path):
                with zipfile.ZipFile(path) as archive:
                    assert archive.testzip() is None
                    assert archive.read('Attachments/sample.bin') == b'\0\1\xfe\xff'
                    model = etree.fromstring(archive.read('Catalog/catalog.xml'))
                    assert model.attrib == {'title': 'Parts & tools'}
                    assert [child.attrib for child in model] == [{'id': '7', 'label': 'Bolt <M6>'}, {'id': '8', 'label': 'Nut'}]
            interop += 2
        outputs = 0
        for path in directory.glob('output-*.zip'):
            with zipfile.ZipFile(path) as archive:
                assert archive.testzip() is None
                assert {name: archive.read(name) for name in archive.namelist()} == {'a.txt': b'abc' * 1000, 'z.bin': b'\0\1\xfe\xff', 'empty.bin': b''}
            outputs += 1
        for path in directory.glob('output-*.opc'):
            with zipfile.ZipFile(path) as archive:
                assert archive.testzip() is None
                manifest = etree.fromstring(archive.read('[Content_Types].xml'))
                assert manifest.tag == '{' + CT + '}Types'
                model = etree.fromstring(archive.read('3D/model.xml'))
                assert model.attrib['name'] == 'independent'
                assert archive.read('Textures/raw.bin') == b'\0\1\xfe\xff'
                if 'preserved' in path.name:
                    assert archive.read('Extensions/vendor.xml') == independent_parts(True)['Extensions/vendor.xml']
            outputs += 1
        profiles = {}
        for label, classes, expected in (('collision', 'class SerializerPackage {} class SerializerPackage_2 {} class SerializerPackage_3ErrorBase {} class SerializerCompression {}', 'SerializerPackage_4'), ('builtins', 'class PromiseLike {} class globalThis {} class ReadonlyArray {} class parseInt {}', 'SerializerPackage')):
            case = directory / label; case.mkdir()
            schema = case / 'collision.serializer'
            schema.write_text('serializer version 1.20.0; ' + classes + ' package bundle {part attachments:bytes {name(dynamic);content_type(dynamic);relationship("urn:test:part");occurs(0,unbounded);}}', encoding='utf8')
            run([args.compiler.resolve(), '--input', schema, '--language', 'js,typescript', '--js.naming', 'preserve', '--js.output', case / 'schema.mjs', '--typescript.output', case / 'schema.d.mts'], directory)
            source = (case / 'schema.mjs').read_text(encoding='utf8'); helper = re.search(r'Generated JS/TS package helper name: (\w+)', source)[1]; compression_helper = re.search(r'Generated JS/TS compression helper name: (\w+)', source)[1]
            assert helper == expected
            smoke = "import {" + helper + " as P," + compression_helper + " as C,bundle as A}from'./schema.mjs';const provider=await C.nodeProvider();const input=new A.Input({attachments:[new P.BinaryPart(new Uint8Array([1]),'/a.bin','application/octet-stream')]});const data=await A.encode(input,{compressionProvider:provider});const result=await A.load(data,{compressionProvider:provider});if(result.value.attachments[0].data[0]!==1)throw Error('collision changed value');"
            (case / 'smoke.mjs').write_text(smoke, encoding='utf8'); (case / 'consumer.mts').write_text(smoke, encoding='utf8')
            run([args.node, case / 'smoke.mjs'], case)
            run([args.node, modules / 'typescript/bin/tsc', '--strict', '--noEmit', '--target', 'ES2022', '--module', 'NodeNext', '--moduleResolution', 'NodeNext', case / 'consumer.mts'], case)
            profiles[label] = len(source.encode())
        report = {'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(), 'node': subprocess.check_output([args.node, '--version'], text=True).strip(), 'typescript': subprocess.check_output([args.node, modules / 'typescript/bin/tsc', '--version'], text=True).strip(), 'independent_input_decisions': count + segmented, 'independent_output_decisions': outputs, 'native_interchange_decisions': interop, 'generated_bytes': len(generated.encode()), 'profiles': profiles}
    (build / 'verification.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2), flush=True)
    print('PASS: actual generated JS/TS package independent framing, profiles, lifecycle and strict types', flush=True)


if __name__ == '__main__':
    main()
