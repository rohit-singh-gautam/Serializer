# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated Java ZIP/OPC support with independent archives and SDK execution."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import xml.etree.ElementTree as etree
import zipfile

import package_zip64_test as wide_zip
import python_package_test as independent
import package_annex_b_corpus as annex_b
import package_mce_corpus as mce_corpus
import package_opc_uri_corpus as uri_corpus
import package_iri_corpus as iri_corpus
import package_utf16_corpus as utf16_corpus

ROOT = Path(__file__).resolve().parent.parent
MODEL_MIME = independent.MODEL_MIME


def build_fixtures(directory):
    """Persist independent physical/metadata inputs and explicit admission decisions for the SDK."""
    directory.mkdir(parents=True, exist_ok=True)
    rows = []

    def archive(label, operation, parts, code='OK', attributes=None, value='independent'):
        """Author standard ZIP bytes without depending on the implementation under test."""
        path = directory / (label + '.zip')
        path.write_bytes(independent.independent_zip(parts, attributes=attributes))
        rows.append((label, operation, path.name, code, '', value))

    def raw(label, data, name='data.bin', payload=b'', code='OK'):
        """Retain a deliberately authored physical archive and its exact owning payload expectation."""
        path = directory / (label + '.zip')
        path.write_bytes(data)
        rows.append((label, 'zip', path.name, code, name, payload.hex()))

    base = independent.independent_parts()
    for descriptor in (False, True):
        for wide in (False, True):
            label = f'profile-descriptor-{int(descriptor)}-wide-{int(wide)}'
            path = directory / (label + '.zip')
            path.write_bytes(independent.independent_zip(base, descriptor, wide))
            rows.append((label, 'package', path.name, 'OK', '', 'independent'))
    archive('extension', 'preserve', independent.independent_parts(True))
    archive('extension-reject', 'package', independent.independent_parts(True), 'UNSUPPORTED_FEATURE')
    archive('collection', 'collection', independent.independent_collection_parts())
    archive('collection-middle-invalid', 'collection', independent.independent_collection_parts(1), 'CODEC')

    for wide in (False, True):
        for signed in (False, True):
            for payload in (b'', b'boundary-data' * 9):
                label = f'descriptor-{int(wide)}-{int(signed)}-{len(payload)}'
                raw(label, independent.wire_descriptor_zip(payload, wide, signed), payload=payload)
    descriptor_dir = ROOT / 'test/resources/package_descriptor_fixtures'
    for path in sorted(descriptor_dir.glob('*.zip')):
        with zipfile.ZipFile(path) as provider:
            item = provider.infolist()[0]
            try:
                payload = provider.read(item)
                code = 'OK'
            except zipfile.BadZipFile:
                payload, code = b'', 'INTEGRITY'
        # zipfile deliberately ignores descriptors, so its successful payload read cannot
        # bless a contradictory descriptor CRC authored by the shared wire fixture.
        if path.stem.endswith('_bad_crc'):
            code = 'INTEGRITY'
        raw('shared-' + path.stem, path.read_bytes(), item.filename, payload, code)
    malformed_dir = ROOT / 'test/resources/package_malformed_fixtures'
    for line in (malformed_dir / 'cases.txt').read_text(encoding='ascii').splitlines():
        stage, filename = line.split()
        # Isolated structural and CRC failures retain their public diagnostic category.
        code = 'INVALID_ARCHIVE' if stage == 'index' else 'INVALID_ARCHIVE,INTEGRITY'
        raw('shared-' + Path(filename).stem, (malformed_dir / filename).read_bytes(), code=code)

    # APPNOTE ZIP64 boundary inputs keep only their actual records; holes are synthesized by Java.
    for label, fixture in (
            ('offset-before-u32', wide_zip.wire_zip64(wide_zip.U32 - 64)),
            ('offset-u32', wide_zip.wire_zip64(wide_zip.U32)),
            ('offset-after-u32', wide_zip.wire_zip64(wide_zip.U32 + 1)),
            ('directory-u32-classic', wide_zip.wire_classic(wide_zip.U32)),
            ('ordinary-zip64', wide_zip.wire_zip64(0, ordinary=True))):
        path = directory / (label + '.segments')
        fixture.write(path)
        rows.append((label, 'segment', path.name, 'OK', fixture.name, fixture.payload.hex()))
    bad = wide_zip.wire_zip64(wide_zip.U32 + 1)
    for label, fixture in (
            ('zip64-locator-pointer', bad.patch(bad.locator + 8, struct.pack('<Q', bad.record + 1))),
            ('zip64-record-length', bad.patch(bad.record + 4, struct.pack('<Q', 45))),
            ('zip64-record-disk', bad.patch(bad.record + 16, struct.pack('<I', 1))),
            ('zip64-narrow-count', bad.patch(bad.end + 8, struct.pack('<HH', 2, 2)))):
        path = directory / (label + '.segments')
        fixture.write(path)
        rows.append((label, 'segment', path.name, 'INVALID_ARCHIVE', '', ''))
    for count in (65534, 65535, 65536):
        path = directory / f'count-{count}.zip'
        path.write_bytes(wide_zip.count_fixture(count).segments[0][1])
        rows.append((f'count-{count}', 'count', path.name, 'OK', str(count), ''))
    for duplicate in (1, 32768, 65535):
        raw(f'duplicate-{duplicate}', wide_zip.count_fixture(65536, duplicate).segments[0][1], code='DUPLICATE_NAME')

    defaults = ('<Default Extension="bin" ContentType="application/octet-stream"/>'
                '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>')
    main = '<Override PartName="/Alternate/root.xml" ContentType="' + MODEL_MIME + '"/>'
    normal = defaults + main
    duplicates = (
        '<Default Extension="bin" ContentType="application/octet-stream"/>',
        '<Default Extension="BIN" ContentType="text/plain"/>',
        '<Default Extension="bi%3An" ContentType="text/plain"/>'
        '<Default Extension="BI%3aN" ContentType="application/octet-stream"/>',
        main,
        '<Override PartName="/Alternate/root.xml" ContentType="text/plain"/>',
        '<Override PartName="/ALTERNATE/ROOT.XML" ContentType="text/plain"/>',
        '<Override PartName=" /Alternate/root.xml &#x9;" ContentType="text/plain"/>',
        '<Override PartName="/Extras/part%3Aname.bin" ContentType="text/plain"/>'
        '<Override PartName="/EXTRAS/PART%3aname.BIN" ContentType="application/octet-stream"/>')
    for index, duplicate in enumerate(duplicates):
        for reverse in (False, True):
            declarations = duplicate + normal if reverse else normal + duplicate
            archive(f'content-type-duplicate-{index}-{int(reverse)}', 'skip',
                    independent.independent_content_type_parts(declarations), 'INVALID_METADATA')
    malformed = ['', defaults, main,
                 normal + '<Default Extension="other"/>',
                 normal + '<Default ContentType="text/plain"/>',
                 defaults + '<Override PartName="/Alternate/root.xml"/>',
                 normal + '<Override ContentType="text/plain"/>']
    for mime in ('', 'text', 'application/{invalid}',
                 'application/octet-stream; p=&quot;unterminated',
                 'APPLICATION/VND.OPENXMLFORMATS-PACKAGE.RELATIONSHIPS+XML',
                 'application/vnd.openxmlformats-package.core-properties+xml; charset=utf-8'):
        malformed.extend(('<Default Extension="bin" ContentType="' + mime + '"/>' +
                          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>' + main,
                          defaults + '<Override PartName="/Alternate/root.xml" ContentType="' + mime + '"/>'))
    for index, declarations in enumerate(malformed):
        archive(f'content-type-invalid-{index}', 'skip',
                independent.independent_content_type_parts(declarations), 'INVALID_METADATA')
    for index, declarations in enumerate((normal, normal + '<Override PartName="/Textures/raw.bin" '
                                         'ContentType="application/custom+binary"/>')):
        archive(f'content-type-valid-{index}', 'skip', independent.independent_content_type_parts(declarations))
    archive('content-types-missing', 'skip', independent.independent_content_type_parts('', False), 'INVALID_METADATA')

    metadata = ('[Content_Types].xml', '_rels/.rels', 'Alternate/_rels/root.xml.rels')
    for encoding, bom in (('utf-16-le', b'\xff\xfe'), ('utf-16-be', b'\xfe\xff')):
        for declaration in ('', '<?xml version="1.0" encoding="UTF-16"?>'):
            parts = dict(base)
            for name in metadata:
                parts[name] = bom + (declaration + parts[name].decode()).encode(encoding)
            archive(f'metadata-{encoding}-{int(bool(declaration))}', 'package', parts)
    for index, payload in enumerate((
            base['[Content_Types].xml'].decode().encode('utf-16-le'),
            b'\xff\xfe' + ('<?xml version="1.0" encoding="UTF-8"?>' + base['[Content_Types].xml'].decode()).encode('utf-16-le'),
            b'\xff\xfe' + ('<?xml version="1.0" encoding="UTF-16LE"?>' + base['[Content_Types].xml'].decode()).encode('utf-16-le'),
            base['[Content_Types].xml'].decode().encode('utf-32'),
            ('<?xml version="1.0" encoding="ISO-8859-1"?>' + base['[Content_Types].xml'].decode()).encode())):
        parts = dict(base)
        parts['[Content_Types].xml'] = payload
        archive(f'metadata-encoding-invalid-{index}', 'package', parts, 'INVALID_METADATA')
    for declaration in ('<!DOCTYPE Types [<!ENTITY injected "outside">]>',
                        '<!DOCTYPE Types SYSTEM "file:///serializer-package-does-not-exist">'):
        parts = dict(base)
        parts['[Content_Types].xml'] = declaration.encode() + parts['[Content_Types].xml']
        archive('doctype-' + str(len(rows)), 'package', parts, 'INVALID_METADATA')

    parts = dict(base)
    for name in metadata:
        parts[name] = b'\xef\xbb\xbf' + parts[name]
    archive('metadata-utf8-bom', 'package', parts)
    parts = dict(base)
    parts['_RELS/.RELS'] = parts.pop('_rels/.rels')
    parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(b'/_rels/.rels', b'/_RELS/.RELS')
    archive('infrastructure-casefold-root', 'package', parts)
    parts = dict(base)
    parts['Alternate/_RELS/root.xml.RELS'] = parts.pop('Alternate/_rels/root.xml.rels')
    parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(b'/Alternate/_rels/root.xml.rels', b'/Alternate/_RELS/root.xml.RELS')
    archive('infrastructure-casefold-part', 'package', parts)
    parts = dict(base)
    parts['_rels/nested/plain.rels'] = b'opaque ordinary descendant'
    parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(b'</Types>',
        b'<Override PartName="/_rels/nested/plain.rels" ContentType="application/octet-stream"/></Types>')
    archive('ordinary-rels-descendant', 'preserve', parts)

    good_iris = ('https://left/right', 'https://left\uff0fright', 'https://left?right',
                 'https://left\uff1fright', 'https://left#right', 'https://left\uff03right',
                 'https://left@right', 'https://left\uff20right', 'https://left:80',
                 'https://left\uff1a80', 'https://user\uff20name@host.invalid/a',
                 'https://caf\u00e9.invalid/a', 'https://host%2Fname.invalid/a',
                 'https://[2001:db8::1]:443/a', 'https://[vF.name:tail]/a', 'https:///path')
    archive('external-iris', 'preserve', independent.independent_external_relationships(good_iris))
    (directory / 'external-targets.txt').write_text('\n'.join(good_iris) + '\n', encoding='utf-8')
    bad_iris = ('https://host:port/', 'https://host:\uff11/', 'https://[::1/',
                'https://[::1]tail/', 'https://[not-ip]/', 'https://[1:2:3]/',
                'https://[fe80::1%25zone]/', 'https://host]/', 'https://[::1]]/',
                'https://user@@host/', 'https://host/%zz', 'https://host/%',
                'https://host/a#one#two', '1scheme://host/', 'https://host\\evil/',
                'https://host\ue000.invalid/', 'https://host/\ue000', 'https://host/#\ue000')
    for index, target in enumerate(bad_iris):
        archive(f'external-invalid-{index}', 'preserve',
                independent.independent_external_relationships((target,)), 'INVALID_RELATIONSHIP')
    for index, kind in enumerate(('relative/type', 'https://host.invalid/#fragment', 'https://host:port/type')):
        archive(f'external-type-invalid-{index}', 'preserve',
                independent.independent_external_relationships(('https://host.invalid/',), kind), 'INVALID_RELATIONSHIP')
    for index, target in enumerate(('./Alternate/root.xml?view=2#section', 'Alternate/./root.xml?#',
                                    '/Alternate/root.xml?next=a/b#value?with:punctuation')):
        parts = dict(base)
        parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'Alternate/root.xml', target.encode())
        archive(f'internal-valid-{index}', 'package', parts)
    for index, target in enumerate(('//host/Alternate/root.xml', '///Alternate/root.xml', '//',
                                    'https://host/Alternate/root.xml', 'name:scheme')):
        parts = dict(base)
        parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'Alternate/root.xml', target.encode())
        archive(f'internal-invalid-{index}', 'package', parts, 'INVALID_RELATIONSHIP')

    # MCE is evaluated before OPC graph interpretation. Nonselected malformed content
    # must stay inert while invalid live capability/order contracts must reject.
    mc, rels = independent.MC, independent.RELS
    beginning = f'<Relationships xmlns="{rels}" xmlns:r="{rels}" xmlns:mc="{mc}" xmlns:e="urn:extension" '
    relationship = '<Relationship Id="main" Type="urn:serializer:main-model" Target="Alternate/root.xml"/>'
    parts = dict(base)
    parts['_rels/.rels'] = (beginning + 'mc:Ignorable="e" mc:ProcessContent="e:box" mc:MustUnderstand="r">'
        '<e:discard mc:MustUnderstand="e"><bad/></e:discard><e:box>' + relationship + '</e:box>'
        '<mc:AlternateContent><mc:Choice Requires="e"><bad/></mc:Choice><mc:Choice Requires="r">'
        '<Relationship Id="selected" Type="urn:vendor:link" Target="Textures/raw.bin"/></mc:Choice>'
        '<mc:Fallback><bad/></mc:Fallback></mc:AlternateContent></Relationships>').encode()
    archive('mce-selected-and-ignored', 'preserve', parts)
    parts = dict(base)
    parts['_rels/.rels'] = (f'<mc:AlternateContent xmlns:mc="{mc}" xmlns:r="{rels}">'
        '<mc:Choice Requires="xml"><bad/></mc:Choice><mc:Fallback><r:Relationships>' +
        relationship.replace('<Relationship', '<r:Relationship') +
        '</r:Relationships></mc:Fallback></mc:AlternateContent>').encode()
    archive('mce-root-fallback', 'package', parts)
    invalid_mce = (
        'mc:Ignorable="missing">', 'mc:Ignorable="mc">', 'mc:ProcessContent="e:box">',
        'mc:Ignorable="e" mc:ProcessContent="e">', 'mc:Ignorable="e" mc:ProcessContent="e:bad:name">',
        'mc:MustUnderstand="e">', 'mc:MustUnderstand="xml">', 'mc:Unknown="">', '><e:unknown/>',
        '><mc:Choice Requires="r"/>', '><mc:Fallback/>', '><mc:Unknown/>', '><mc:AlternateContent/>',
        '><mc:AlternateContent><mc:Fallback/></mc:AlternateContent>',
        '><mc:AlternateContent><mc:Choice/></mc:AlternateContent>',
        '><mc:AlternateContent><mc:Choice Requires=""/></mc:AlternateContent>',
        '><mc:AlternateContent><mc:Choice Requires="missing"/></mc:AlternateContent>',
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
        'mc:Ignorable="e" mc:ProcessContent="e:*"><mc:AlternateContent><e:box/><mc:Choice Requires="r"/></mc:AlternateContent>')
    for index, body in enumerate(invalid_mce):
        parts = dict(base)
        parts['_rels/.rels'] = (beginning + body + relationship + '</Relationships>').encode()
        archive(f'mce-invalid-{index}', 'package', parts, 'INVALID_METADATA,UNSUPPORTED_FEATURE')

    # Non-file physical records and discarded namespaces are not logical OPC parts.
    parts = dict(base)
    parts.update({'Textures/': b'', '[trash]/opaque.bin': b'discarded',
                  'unix-link.bin': b'not-followed', 'dos-dir.bin': b'not-a-part'})
    archive('physical-nonparts', 'package', parts,
            attributes={'unix-link.bin': (3, 0o120777 << 16), 'dos-dir.bin': (0, 0x10)})
    for name in ('caf\u00e9.bin', 'caf%C3%A9.bin'):
        parts = dict(base)
        parts[name] = b'non-ASCII logical URI policy'
        archive('non-ascii-' + str(len(rows)), 'package', parts, 'UNSUPPORTED_FEATURE')

    # Quoted MIME values are case-sensitive and retain significant internal whitespace.
    for index, mime in enumerate((MODEL_MIME + ';profile=&quot;a  b&quot;;mode=read',
                                  MODEL_MIME.upper() + ';MODE=read;PROFILE=&quot;a  b&quot;',
                                  MODEL_MIME + ';profile=&quot;a b&quot;;mode=read',
                                  MODEL_MIME + ';profile=&quot;a  b&quot;;mode=READ')):
        parts = dict(base)
        parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(MODEL_MIME.encode(), mime.encode())
        archive(f'parameterized-{index}', 'parameterized', parts, 'OK' if index < 2 else 'CODEC')

    # Corrupt an unknown discarded item without altering central/local metadata.
    parts = dict(base)
    parts['[trash]/discard.bin'] = b'private-unselected-payload' * 3
    data = bytearray(independent.independent_zip(parts))
    with zipfile.ZipFile(io.BytesIO(data)) as reader:
        item = reader.getinfo('[trash]/discard.bin')
        name_bytes, extra_bytes = struct.unpack_from('<HH', data, item.header_offset + 26)
        data[item.header_offset + 30 + name_bytes + extra_bytes] ^= 1
    path = directory / 'discarded-integrity.zip'
    path.write_bytes(data)
    rows.append(('discarded-integrity', 'package', path.name, 'INVALID_ARCHIVE,INTEGRITY', '', ''))
    for label, data, accepted in annex_b.cases():
        raw(label, data, 'data.bin', annex_b.PAYLOAD, 'OK' if accepted else 'INVALID_ARCHIVE')
    for label, data, accepted in mce_corpus.cases():
        filename = label + '.zip'; (directory / filename).write_bytes(data)
        rows.append((label,'package',filename,'OK' if accepted else 'INVALID_METADATA','','independent'))
    for module, operation in ((uri_corpus,'package'),(iri_corpus,'preserve')):
        for label, data, accepted in module.cases():
            filename = label + '.zip'; (directory / filename).write_bytes(data)
            rows.append((label,operation,filename,'OK' if accepted else 'ANY','','independent'))
    for label,parts,accepted in utf16_corpus.cases(independent.independent_parts()):
        filename = label + '.zip'; (directory / filename).write_bytes(independent.independent_zip(parts))
        rows.append((label,'package',filename,'OK' if accepted else 'ANY','','independent'))
    (directory / 'cases.tsv').write_text(''.join('\t'.join(row) + '\n' for row in rows), encoding='utf-8')
    return rows


MAIN = r'''
import java.io.*;
import java.nio.file.*;
import java.util.*;

/** Independent SDK adapter: use only public generated package/model entry points. */
public final class Main {
  private static int passed, failed;
  private static Path root;
  private static String filter="";
  @FunctionalInterface interface Checked { void run() throws Exception; }
  /** Reject a false acceptance without relying on disabled JVM assert statements. */
  private static void require(boolean condition,String message) {
    if(!condition)throw new AssertionError(message);
  }
  /** Keep independent failures visible while completing the remaining regression decisions. */
  private static void test(String name,Checked body) {
    if(!name.contains(filter))return;
    try {body.run();++passed;}
    catch(Throwable failure){++failed;System.err.println("FAIL "+name+": "+failure);failure.printStackTrace(System.err);}
  }
  /** Capture an expected failure without substituting an implementation-specific codec. */
  private static Throwable failure(Checked body) throws Exception {
    try{body.run();}catch(Throwable observed){return observed;}
    throw new AssertionError("Operation unexpectedly succeeded");
  }
  /** Require the public diagnostic category for an independently isolated invalid input. */
  private static void code(Throwable observed,String expected) {
    String actual=observed instanceof PKG.PackageError value?value.code.name():observed instanceof Parameterized.SerializerPackage.PackageError value?value.code.name():null;
    require(actual!=null,"Missing package diagnostic: "+observed);
    if(expected.equals("ANY"))return;
    require(Arrays.asList(expected.split(",")).contains(actual),"Expected "+expected+"; got "+actual+": "+observed);
  }
  /** Relax only count/offset policy for real 65K counts and small segmented large-offset fixtures. */
  private static PKG.Limits generous() {
    var limits=new PKG.Limits();limits.maxArchiveBytes=1L<<34;limits.maxEntries=100000;
    limits.maxMetadataBytes=128L<<20;limits.maxBufferBytes=256L<<20;return limits;
  }
  /** Map public cumulative resource fields without reaching into private ledger state. */
  private static long maximum(PKG.Limits limits,PKG.Resource resource) {
    return switch(resource) {
      case IO_BYTES -> limits.maxIoBytes;
      case METADATA_BYTES -> limits.maxMetadataBytes;
      case EXPANDED_BYTES -> limits.maxTotalExpandedBytes;
      case ENTRIES -> limits.maxEntries;
      case RELATIONSHIPS -> limits.maxRelationships;
      case DECODER_ALLOCATION_BYTES -> limits.maxDecoderAllocationBytes;
      case WORK_UNITS -> limits.maxWorkUnits;
      case REFERENCES -> limits.maxReferences;
      case PRESERVED_BYTES -> limits.maxPreservedBytes;
    };
  }
  /** Change exactly one selected ceiling for independently measured exact/one-tight controls. */
  private static void setMaximum(PKG.Limits limits,PKG.Resource resource,long value) {
    switch(resource) {
      case IO_BYTES -> limits.maxIoBytes=value;
      case METADATA_BYTES -> limits.maxMetadataBytes=value;
      case EXPANDED_BYTES -> limits.maxTotalExpandedBytes=value;
      case ENTRIES -> limits.maxEntries=value;
      case RELATIONSHIPS -> limits.maxRelationships=value;
      case DECODER_ALLOCATION_BYTES -> limits.maxDecoderAllocationBytes=value;
      case WORK_UNITS -> limits.maxWorkUnits=value;
      case REFERENCES -> limits.maxReferences=value;
      case PRESERVED_BYTES -> limits.maxPreservedBytes=value;
    }
  }
  /** Track bounded short reads independently of the generated parser's source adapter. */
  private static class Source implements PKG.RandomSource,AutoCloseable {
    final byte[] bytes;int calls,largest;long readBytes;boolean closed;
    Runnable callback;int shortRead=17;IOException sentinel;
    /** Borrow immutable independently produced bytes. */
    Source(byte[] bytes){this.bytes=bytes;}
    /** Expose the exact archive extent before any read admission. */
    public long size(){return bytes.length;}
    /** Return legal short reads only after checking callback extents and activity. */
    public byte[] readAt(long offset,int maximum) throws IOException {
      require(!closed,"Borrowed source closed");require(maximum>0&&maximum<=32768,"Unbounded borrowed read");
      require(offset>=0&&offset<=bytes.length&&maximum<=bytes.length-offset,"Unchecked borrowed extent");
      ++calls;largest=Math.max(largest,maximum);if(sentinel!=null)throw sentinel;
      int count=Math.min(maximum,shortRead);readBytes+=count;
      if(callback!=null)callback.run();return Arrays.copyOfRange(bytes,(int)offset,(int)offset+count);
    }
    /** Record forbidden borrowed ownership transfer. */
    public void close(){closed=true;}
  }
  /** Synthesize a multi-gigabyte extent from an independently authored tiny segment descriptor. */
  private static final class Segments implements PKG.RandomSource {
    final long extent;final long[] offsets;final byte[][] segments;long actualBytes;int largest;
    /** Read tiny wire segments, never their much larger logical holes. */
    Segments(Path path) throws IOException {
      try(var input=new DataInputStream(Files.newInputStream(path))) {
        require(Arrays.equals(input.readNBytes(8),new byte[]{'S','Z','I','P','6','4',0,1}),"Segment signature");
        extent=Long.reverseBytes(input.readLong());int count=Integer.reverseBytes(input.readInt());
        require(count>=0&&count<16,"Segment count");offsets=new long[count];segments=new byte[count][];
        for(int index=0;index<count;++index){offsets[index]=Long.reverseBytes(input.readLong());long size=Long.reverseBytes(input.readLong());require(size>=0&&size<1048576,"Materialized segment limit");segments[index]=input.readNBytes((int)size);require(segments[index].length==size,"Truncated descriptor");}
        require(input.read()==-1,"Descriptor suffix");
      }
    }
    /** Return the independent signed-long logical archive extent. */
    public long size(){return extent;}
    /** Synthesize a bounded short slice and enforce an independent total physical-I/O cap. */
    public byte[] readAt(long offset,int maximum) {
      require(offset>=0&&offset<=extent&&maximum>0&&maximum<=extent-offset,"Unchecked logical extent");
      require(maximum<=32768,"Unbounded logical read");largest=Math.max(largest,maximum);
      int count=Math.min(maximum,17);actualBytes+=count;require(actualBytes<1048576,"Excessive physical work");
      var result=new byte[count];
      for(int index=0;index<offsets.length;++index){long first=Math.max(offset,offsets[index]),last=Math.min(offset+count,offsets[index]+segments[index].length);if(first<last)System.arraycopy(segments[index],(int)(first-offsets[index]),result,(int)(first-offset),(int)(last-first));}
      return result;
    }
  }
  /** Snapshot defaults separately for each complete profile load. */
  private static PKG.ExtensionPolicy policy(PKG.Unknown mode) {return new PKG.ExtensionPolicy(mode,mode,null,null);}
  /** Read every selected physical entry so CRC/extent controls are not index-only tests. */
  private static void readZip(Object source,String name,String hex,PKG.Limits limits) throws Exception {
    try(var archive=PKG.ZipReader.open(source,new PKG.Budget(limits))) {
      for(var entry:archive.entries()){var data=archive.read(entry.name);require(data.length==entry.expandedBytes,"Expanded extent");}
      if(!name.isEmpty())require(Arrays.equals(archive.read(name),HexFormat.of().parseHex(hex)),"Owning payload mismatch");
    }
  }
  /** Execute one independent ZIP/OPC fixture decision through the actual emitted public API. */
  private static void row(String[] values) throws Exception {
    String operation=values[1];Path path=root.resolve("fixtures").resolve(values[2]);
    Checked action=()->{
      if(operation.equals("segment")){var source=new Segments(path);readZip(source,values[4],values[5],generous());require(source.actualBytes<1048576,"Sparse materialization");}
      else if(operation.equals("zip")){readZip(Files.readAllBytes(path),values[4],values[5],generous());}
      else if(operation.equals("count")){try(var archive=PKG.ZipReader.open(path,new PKG.Budget(generous()))){int count=Integer.parseInt(values[4]);var entries=archive.entries();require(entries.size()==count,"Entry count narrowed");for(int index=0;index<count;++index)require(entries.get(index).name.equals("part"+index),"Entry order changed");require(archive.read("part"+(count-1)).length==0,"Last entry");}}
      else if(operation.equals("collection")){var value=ARCHIVE_COLLECTION.load(path).value;require(value.models.size()==3,"Incomplete collection");require(value.models.get(2).value.name.equals("item2"),"Collection decode");}
      else if(operation.equals("parameterized")){require(Parameterized.Profile.Archive.load(path).value.main.name.equals(values[5]),"Parameterized discovery");}
      else {var mode=operation.equals("preserve")?PKG.Unknown.PRESERVE:operation.equals("skip")?PKG.Unknown.SKIP:PKG.Unknown.REJECT;var result=ARCHIVE.load(path,new PKG.Budget(),policy(mode),null);require(result.value.main.name.equals(values[5]),"Typed model discovery");require(result.value.attachments.size()==1&&Arrays.equals(result.value.attachments.get(0).data,new byte[]{0,1,(byte)254,(byte)255}),"Attachment payload");}
    };
    if(values[3].equals("OK"))action.run();else code(failure(action),values[3]);
  }
  /** Create actual native values for independent consumer and transactional Save checks. */
  private static ARCHIVE.Input model() {
    var value=new ARCHIVE.Input();value.main=new Schema.Profile.Model();value.main.name="generated & checked";
    var point=new Schema.Profile.Point();point.x=1;point.y=2;point.z=3;value.main.points=new Schema.Profile.Point[]{point};
    value.attachments=List.of(new PKG.BinaryPart(new byte[]{0,1,(byte)254,(byte)255},"/Textures/raw.bin","application/octet-stream"));return value;
  }
  /** Preserve caller sink ownership and record final-callback cancellation/reentry boundaries. */
  private static class Sink extends ByteArrayOutputStream {
    boolean closed,flushed;Runnable callback;
    /** Record only bounded writes and invoke a caller-controlled late callback. */
    @Override public synchronized void write(byte[] bytes,int offset,int length){require(length<=32768,"Unbounded borrowed write");super.write(bytes,offset,length);if(callback!=null)callback.run();}
    /** Record forbidden borrowed ownership transfer. */
    @Override public void close(){closed=true;}
    /** Record forbidden implicit transport flushing. */
    @Override public void flush(){flushed=true;}
  }
  /** Check public lifetime, budget, callback, ownership and publication contracts independently. */
  private static void contracts() throws Exception {
    byte[] archive=Files.readAllBytes(root.resolve("fixtures/profile-descriptor-0-wide-0.zip"));
    test("borrowed short source",()->{var source=new Source(archive);try(var reader=PKG.ZipReader.open(source,new PKG.Budget())){require(ARCHIVE.load(source).value.main.name.equals("independent"),"Short source model");require(reader.read("Textures/raw.bin").length==4,"Short source payload");}require(!source.closed&&source.calls>10,"Borrowed ownership");});
    test("byte array snapshot",()->{byte[] mutable=archive.clone();try(var reader=PKG.ZipReader.open(mutable,new PKG.Budget())){Arrays.fill(mutable,(byte)0);require(reader.read("Textures/raw.bin").length==4,"Source not snapshotted");}});
    test("immutable entries",()->{try(var reader=PKG.ZipReader.open(archive)){failure(()->reader.entries().clear());require(reader.entries().size()==5,"Mutable entry catalog");}});
    test("invalid policies before source",()->{var source=new Source(archive);var limits=new PKG.Limits();limits.maxArchiveBytes=-1;code(failure(()->PKG.ZipReader.open(source,new PKG.Budget(limits))),"INVALID_OPTIONS");require(source.calls==0,"Invalid policy touched source");});
    test("archive limit before source",()->{var source=new Source(archive);var limits=new PKG.Limits();limits.maxArchiveBytes=archive.length-1;code(failure(()->PKG.ZipReader.open(source,new PKG.Budget(limits))),"RESOURCE_LIMIT");require(source.calls==0,"Archive bound read source");});
    test("policy snapshot",()->{var limits=new PKG.Limits();limits.maxArchiveBytes=archive.length-1;var budget=new PKG.Budget(limits);limits.maxArchiveBytes=Long.MAX_VALUE;budget.configured().maxArchiveBytes=Long.MAX_VALUE;code(failure(()->PKG.ZipReader.open(archive,budget)),"RESOURCE_LIMIT");});
    test("exact cumulative package limits",()->{
      var path=root.resolve("fixtures/extension.zip");var measured=new PKG.Budget();
      ARCHIVE.load(path,measured,policy(PKG.Unknown.PRESERVE),null);
      for(var resource:PKG.Resource.values()) {
        long used=maximum(measured.configured(),resource)-measured.remaining(resource);
        if(used==0)continue;
        var exact=new PKG.Limits();setMaximum(exact,resource,used);
        ARCHIVE.load(path,new PKG.Budget(exact),policy(PKG.Unknown.PRESERVE),null);
        setMaximum(exact,resource,used-1);
        code(failure(()->ARCHIVE.load(path,new PKG.Budget(exact),policy(PKG.Unknown.PRESERVE),null)),"RESOURCE_LIMIT");
      }
    });
    test("exact simultaneous buffer limit",()->{
      long lower=0,upper=1048576;
      while(lower<upper){long middle=lower+(upper-lower)/2;var limits=new PKG.Limits();limits.maxBufferBytes=middle;
        try{ARCHIVE.load(archive,new PKG.Budget(limits),null,null);upper=middle;}
        catch(PKG.PackageError expected){require(expected.code==PKG.Code.RESOURCE_LIMIT,"Unexpected buffer search failure");lower=middle+1;}}
      require(lower>0&&lower<1048576,"Buffer admission has no finite boundary");
      var limits=new PKG.Limits();limits.maxBufferBytes=lower;ARCHIVE.load(archive,new PKG.Budget(limits),null,null);
      limits.maxBufferBytes=lower-1;code(failure(()->ARCHIVE.load(archive,new PKG.Budget(limits),null,null)),"RESOURCE_LIMIT");
    });
    test("transport exception identity",()->{var source=new Source(archive);var expected=new IOException("source-marker");source.sentinel=expected;require(failure(()->PKG.ZipReader.open(source,new PKG.Budget()))==expected,"Source exception replaced");require(!source.closed,"Failed source closed");});
    test("reader sticky first transport failure",()->{
      var source=new Source(archive);var reader=PKG.ZipReader.open(source,new PKG.Budget());
      var expected=new IOException("payload-source-marker");source.sentinel=expected;
      require(failure(()->reader.read("Textures/raw.bin"))==expected,"Active payload failure replaced");int calls=source.calls;
      require(failure(()->reader.read("Textures/raw.bin")).getCause()==expected,"Inactive reader lost first cause");
      require(source.calls==calls&&!source.closed,"Failed reader resumed/closed borrowed I/O");reader.close();
    });
    for(int mode=0;mode<3;++mode){final int invalid=mode;test("invalid source result "+mode,()->{PKG.RandomSource source=new PKG.RandomSource(){public long size(){return archive.length;}public byte[] readAt(long offset,int maximum){return invalid==0?null:invalid==1?new byte[0]:new byte[maximum+1];}};failure(()->PKG.ZipReader.open(source,new PKG.Budget()));});}
    test("cancellation first identity",()->{var expected=new IllegalStateException("cancel-marker");var limits=new PKG.Limits();limits.checkCancelled=()->{throw expected;};require(failure(()->new PKG.Budget(limits))==expected,"Cancellation replaced");});
    test("expired deadline",()->{var limits=new PKG.Limits();limits.deadlineNanos=System.nanoTime()-1;code(failure(()->new PKG.Budget(limits)),"DEADLINE_EXCEEDED");});
    test("reader close fences borrowed I/O",()->{var source=new Source(archive);var reader=PKG.ZipReader.open(source,new PKG.Budget());source.callback=()->{try{reader.close();}catch(IOException caught){throw new UncheckedIOException(caught);}};failure(()->reader.read("Textures/raw.bin"));int calls=source.calls;failure(()->reader.read("Textures/raw.bin"));require(source.calls==calls&&!source.closed,"Read after callback close");reader.close();});
    test("reader swallowed reentry fences borrowed I/O",()->{var source=new Source(archive);var reader=PKG.ZipReader.open(source,new PKG.Budget());source.callback=()->{try{reader.read("Textures/raw.bin");}catch(IOException expected){}};failure(()->reader.read("Textures/raw.bin"));int calls=source.calls;failure(()->reader.read("Textures/raw.bin"));require(source.calls==calls,"Read after swallowed reentry");reader.close();});
    test("loader close during nested source read",()->{
      var source=new Source(archive);var loader=new PKG.Loader(source,new PKG.Budget());int before=source.calls;
      var role=new PKG.Role("main","/3D/model.xml","application/vnd.serializer.test-model+xml","/","urn:serializer:main-model",null,"deflate",1,1);
      source.callback=()->{try{loader.close();}catch(IOException caught){throw new UncheckedIOException(caught);}};
      failure(()->loader.single(role,(bytes,limits)->Schema.Profile.Model.decodeXml(bytes,limits,null)));
      int stopped=source.calls;require(stopped==before+1,"Nested source read continued after loader close");
      failure(()->loader.single(role,(bytes,limits)->Schema.Profile.Model.decodeXml(bytes,limits,null)));
      require(source.calls==stopped&&!source.closed,"Disposed loader used/closed borrowed source");loader.close();
    });
    test("loader swallowed nested reentry",()->{
      var source=new Source(archive);var loader=new PKG.Loader(source,new PKG.Budget());int before=source.calls;
      var role=new PKG.Role("main","/3D/model.xml","application/vnd.serializer.test-model+xml","/","urn:serializer:main-model",null,"deflate",1,1);
      var first=new Throwable[1];
      source.callback=()->{try{loader.finish(new Object(),null,null);}catch(IOException expected){first[0]=expected;}};
      require(failure(()->loader.single(role,(bytes,limits)->Schema.Profile.Model.decodeXml(bytes,limits,null)))==first[0],"Nested loader failure replaced");
      require(failure(()->loader.finish(new Object(),null,null)).getCause()==first[0],"Inactive loader lost first cause");
      require(source.calls==before+1&&!source.closed,"Nested source read continued after loader reentry");loader.close();
    });
    test("writer reentry first failure",()->{var holder=new PKG.ZipWriter[1];var armed=new boolean[1];var limits=new PKG.Limits();limits.checkCancelled=()->{if(armed[0])try{holder[0].finish();}catch(IOException expected){}};holder[0]=new PKG.ZipWriter(new PKG.Budget(limits));holder[0].add("data.bin",new byte[]{1});armed[0]=true;failure(()->holder[0].finish());armed[0]=false;failure(()->holder[0].finish());holder[0].close();});
    test("writer close from policy",()->{var holder=new PKG.ZipWriter[1];var armed=new boolean[1];var limits=new PKG.Limits();limits.checkCancelled=()->{if(armed[0])holder[0].close();};holder[0]=new PKG.ZipWriter(new PKG.Budget(limits));holder[0].add("data.bin",new byte[]{1});armed[0]=true;failure(()->holder[0].finish());});
    test("writer snapshot and finished state",()->{byte[] bytes={1,2,3};try(var writer=new PKG.ZipWriter()){writer.add("data.bin",bytes);Arrays.fill(bytes,(byte)9);byte[] result=writer.finish();try(var reader=PKG.ZipReader.open(result)){require(Arrays.equals(reader.read("data.bin"),new byte[]{1,2,3}),"Writer input not snapshotted");}failure(()->writer.finish());failure(()->writer.add("new.bin",new byte[0]));}});
    test("canonical physical order",()->{
      byte[] first,second;
      try(var writer=new PKG.ZipWriter()){writer.add("z.bin",new byte[]{3,4},"deflate");writer.add("a.bin",new byte[]{1,2});first=writer.finish();}
      try(var writer=new PKG.ZipWriter()){writer.add("a.bin",new byte[]{1,2});writer.add("z.bin",new byte[]{3,4},"deflate");second=writer.finish();}
      require(Arrays.equals(first,second),"Equivalent physical order changed canonical ZIP bytes");
    });
    test("canonical explicit extension order",()->{
      var value=model();
      var first=new PKG.BinaryPart(new byte[]{1},"/Extensions/a.bin","application/octet-stream");
      var second=new PKG.BinaryPart(new byte[]{2},"/Extensions/b.bin","application/octet-stream");
      var firstLink=new PKG.Relationship("/","vendor_a","urn:serializer:extra","Extensions/a.bin");
      var secondLink=new PKG.Relationship("/","vendor_b","urn:serializer:extra","Extensions/b.bin");
      value.extensions=new PKG.Extensions(List.of(first,second),List.of(firstLink,secondLink));byte[] canonical=ARCHIVE.encode(value);
      value.extensions=new PKG.Extensions(List.of(second,first),List.of(secondLink,firstLink));
      require(Arrays.equals(canonical,ARCHIVE.encode(value)),"Equivalent explicit extension order changed canonical OPC bytes");
    });
    test("sink borrowed ownership",()->{var sink=new Sink();ARCHIVE.write(sink,model(),new PKG.Budget());require(sink.size()>0&&!sink.closed&&!sink.flushed,"Borrowed sink closed/flushed");});
    test("final sink cancellation",()->{var expected=new IllegalStateException("late-cancel");var cancelled=new boolean[1];var limits=new PKG.Limits();limits.checkCancelled=()->{if(cancelled[0])throw expected;};var sink=new Sink();sink.callback=()->cancelled[0]=true;require(failure(()->ARCHIVE.write(sink,model(),new PKG.Budget(limits)))==expected,"Late cancellation replaced");require(sink.size()>0&&!sink.closed,"Late output ownership");});
    test("sink I/O exception identity",()->{var expected=new IOException("sink-marker");OutputStream sink=new OutputStream(){public void write(int value)throws IOException{throw expected;}public void write(byte[] bytes,int start,int size)throws IOException{throw expected;}};require(failure(()->ARCHIVE.write(sink,model(),new PKG.Budget()))==expected,"Sink exception replaced");});
    for(long capacity:new long[]{0,archive.length-1})test("spool pre-I/O admission "+capacity,()->{
      var directory=root.resolve("spool-admission");Files.createDirectories(directory);var consumed=new long[1];
      var input=new ByteArrayInputStream(archive){
        /** Record actual borrowed reads independently of the generated ledger. */
        @Override public synchronized int read(byte[] bytes,int offset,int requested) {
          require(requested>0&&requested<=32768,"Unbounded private spool request");
          int count=super.read(bytes,offset,Math.min(requested,17));if(count>0)consumed[0]+=count;return count;
        }
      };
      var limits=new PKG.Limits();limits.allowSpool=true;limits.spoolDirectory=directory;
      limits.maxIoBytes=capacity;limits.maxSpoolBytes=archive.length;
      code(failure(()->PKG.ZipReader.open(input,new PKG.Budget(limits))),"RESOURCE_LIMIT");
      require(consumed[0]<=capacity,"Spooling consumed borrowed bytes beyond admitted I/O capacity: "+consumed[0]+">"+capacity);
      try(var paths=Files.list(directory)){require(paths.findAny().isEmpty(),"Private spool admission failure leaked a file");}
    });
    test("spool preflight precedes private creation",()->{
      var limits=new PKG.Limits();limits.allowSpool=true;limits.maxIoBytes=0;
      limits.spoolDirectory=root.resolve("absent-spool-parent/never-created");
      require(!Files.exists(limits.spoolDirectory.getParent()),"Preflight probe parent already exists");
      code(failure(()->PKG.ZipReader.open(new ByteArrayInputStream(archive),new PKG.Budget(limits))),"RESOURCE_LIMIT");
      require(!Files.exists(limits.spoolDirectory.getParent()),"Resource preflight created a directory");
    });
    test("spool explicit admission and cleanup",()->{var directory=root.resolve("spool");Files.createDirectories(directory);var input=new ByteArrayInputStream(archive){boolean closed;@Override public void close(){closed=true;}};code(failure(()->PKG.ZipReader.open(input,new PKG.Budget())),"INVALID_OPTIONS");require(input.available()==archive.length,"Implicit spool consumed input");var limits=new PKG.Limits();limits.allowSpool=true;limits.spoolDirectory=directory;limits.maxSpoolBytes=archive.length;try(var reader=PKG.ZipReader.open(input,new PKG.Budget(limits))){require(reader.read("Textures/raw.bin").length==4,"Spool decode");}require(!input.closed,"Spool closed source");try(var paths=Files.list(directory)){require(paths.findAny().isEmpty(),"Private spool leaked");}limits.maxSpoolBytes=archive.length-1;code(failure(()->PKG.ZipReader.open(new ByteArrayInputStream(archive),new PKG.Budget(limits))),"RESOURCE_LIMIT");try(var paths=Files.list(directory)){require(paths.findAny().isEmpty(),"Failed private spool leaked");}});
    test("extensions before validator",()->{var path=root.resolve("fixtures/extension.zip");var invoked=new boolean[1];var result=ARCHIVE.load(path,new PKG.Budget(),policy(PKG.Unknown.PRESERVE),(value,budget)->{invoked[0]=true;require(value.main!=null&&value.attachments.size()==1,"Incomplete required roles");require(value.extensions.parts.size()==1&&value.extensions.relationships.size()==1,"Validator did not receive complete extensions");});require(invoked[0]&&result.extensions==result.value.extensions,"Result/candidate extension disagreement");});
    test("unknown policy matrix",()->{var path=root.resolve("fixtures/extension.zip");for(var mode:PKG.Unknown.values()){if(mode==PKG.Unknown.REJECT)code(failure(()->ARCHIVE.load(path,new PKG.Budget(),policy(mode),null)),"UNSUPPORTED_FEATURE");else{var value=ARCHIVE.load(path,new PKG.Budget(),policy(mode),null);require(value.extensions.parts.size()==(mode==PKG.Unknown.PRESERVE?1:0),"Extension policy not applied");}}var required=new PKG.ExtensionPolicy(PKG.Unknown.SKIP,PKG.Unknown.SKIP,part->true,null);code(failure(()->ARCHIVE.load(path,new PKG.Budget(),required,null)),"UNSUPPORTED_FEATURE");});
    test("external URI spelling",()->{var value=ARCHIVE.load(root.resolve("fixtures/external-iris.zip"),new PKG.Budget(),policy(PKG.Unknown.PRESERVE),null);var expected=Files.readAllLines(root.resolve("fixtures/external-targets.txt"));require(value.extensions.relationships.size()==expected.size(),"External relationship count");for(int index=0;index<expected.size();++index)require(value.extensions.relationships.get(index).target.equals(expected.get(index)),"External URI spelling changed");Files.write(root.resolve("external-roundtrip.opc"),ARCHIVE.encode(value.value));});
    for(String boundary:new String[]{"before_temporary","after_temporary","before_file_sync","after_file_sync","before_replace","after_replace","after_directory_sync"})test("atomic Save "+boundary,()->{var destination=root.resolve("save-"+boundary+".opc");byte[] old="original-complete-generation".getBytes(java.nio.charset.StandardCharsets.UTF_8);Files.write(destination,old);var expected=new IOException("save-marker");Throwable observed=failure(()->ARCHIVE.save(destination,model(),new PKG.Budget(),null,point->{if(point.equals(boundary))throw expected;}));boolean replaced=boundary.equals("after_replace")||boundary.equals("after_directory_sync");if(replaced){code(observed,"PUBLICATION_UNCERTAIN");require(observed.getCause()==expected,"Post-replace cause replaced");require(ARCHIVE.load(destination).value.main.name.equals("generated & checked"),"Incomplete published file");}else{require(observed==expected,"Pre-replace cause replaced");require(Arrays.equals(Files.readAllBytes(destination),old),"Old generation changed");}try(var paths=Files.list(root)){require(paths.noneMatch(path->path.getFileName().toString().startsWith(".serializer-save-")),"Save temporary leaked");}});
    test("semantic Save rejection precedes filesystem",()->{var destination=root.resolve("semantic-save.opc");byte[] old={7,8,9};Files.write(destination,old);var expected=new IOException("semantic-marker");var hooks=new int[1];require(failure(()->ARCHIVE.save(destination,model(),new PKG.Budget(),(value,budget)->{throw expected;},point->++hooks[0]))==expected,"Validator exception replaced");require(hooks[0]==0&&Arrays.equals(Files.readAllBytes(destination),old),"Semantic failure touched publication");});
    // Encode with every standard ZIP envelope selection and independently inspect the products in Python.
    for(boolean wide:new boolean[]{false,true})for(boolean descriptor:new boolean[]{false,true})test("independent output "+wide+" "+descriptor,()->{var limits=new PKG.Limits();limits.forceZip64=wide;limits.descriptor=descriptor;Files.write(root.resolve("produced-"+wide+"-"+descriptor+".opc"),ARCHIVE.encode(model(),new PKG.Budget(limits)));try(var writer=new PKG.ZipWriter(new PKG.Budget(limits))){writer.add("stored.bin",new byte[]{0,1,2});writer.add("empty.bin",new byte[0],"deflate");Files.write(root.resolve("produced-"+wide+"-"+descriptor+".zip"),writer.finish());}});
  }
  /** Run all maintained independent decisions in one strict Java17-targeted SDK process. */
  public static void main(String[] arguments) throws Exception {
    root=Path.of(arguments[0]);if(arguments.length>1)filter=arguments[1];
    for(String line:Files.readAllLines(root.resolve("fixtures/cases.tsv"))) {String[] values=line.split("\t",-1);test(values[0],()->row(values));}
    contracts();System.out.println("{\"passed\":"+passed+",\"failed\":"+failed+"}");
    if(failed!=0)throw new AssertionError(failed+" Java package checks failed");
  }
}
'''


def run(command, log):
    """Retain exact SDK/compiler diagnostics and fail when the real subprocess fails."""
    with log.open('w', encoding='utf-8') as output:
        result = subprocess.run([str(value) for value in command], stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(encoding='utf-8', errors='replace'), end='')
        raise subprocess.CalledProcessError(result.returncode, command)


def inspect_outputs(directory):
    """Use independent ZIP/XML consumers to reject self-consistent but malformed emitted packages."""
    inspected = 0
    for path in sorted(directory.glob('produced-*.*')):
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            if path.suffix == '.opc':
                assert archive.read('Textures/raw.bin') == b'\0\1\xfe\xff'
                model = etree.fromstring(archive.read('3D/model.xml'))
                assert model.attrib['name'] == 'generated & checked'
                assert etree.fromstring(archive.read('[Content_Types].xml')).tag == '{' + independent.CT + '}Types'
            else:
                assert archive.read('stored.bin') == b'\0\1\2'
                assert archive.read('empty.bin') == b''
            wide, descriptor = path.stem.split('-')[1:]
            assert all(bool(item.flag_bits & 8) == (descriptor == 'true') for item in archive.infolist())
            assert (b'PK\x06\x06' in path.read_bytes()) == (wide == 'true')
            inspected += 1
    assert inspected == 8, inspected
    with zipfile.ZipFile(directory / 'external-roundtrip.opc') as archive:
        links = etree.fromstring(archive.read('_rels/.rels'))
        targets = [link.attrib['Target'] for link in links if link.attrib.get('TargetMode') == 'External']
        assert targets == (directory / 'fixtures/external-targets.txt').read_text(encoding='utf-8').splitlines()
    return inspected + 1


EXCHANGE = r'''
import java.nio.file.*;
/** Exchange actual typed native values with the separately built C++ example. */
public final class Exchange {
  /** Load the C++ producer, verify public values, and emit a fresh complete Java package. */
  public static void main(String[] arguments) throws Exception {
    var input=Example.OpcExample.Document.load(Path.of(arguments[0]));
    var value=input.value;
    if(!value.main.title.equals("Parts & tools")||value.main.items.length!=2||
        value.main.items[0].id!=7||!value.main.items[0].label.equals("Bolt <M6>")||
        value.main.items[1].id!=8||!value.main.items[1].label.equals("Nut")||
        value.attachments.size()!=1||!value.attachments.get(0).name.equals("/Attachments/sample.bin"))
      throw new AssertionError("C++/Java typed values changed");
    Files.write(Path.of(arguments[1]),Example.OpcExample.Document.encode(value));
  }
}
'''


def native_exchange(args, build, classes):
    """Use the real C++ package facade as both producer and consumer, plus independent ZIP/XML checks."""
    if not args.cpp_example:
        return 0
    run([args.compiler, '--input', ROOT / 'example/opc/model.serializer', '--language', 'java',
         '--output', build / 'Example.java'], build / 'generate-example.log')
    (build / 'Exchange.java').write_text(EXCHANGE, encoding='utf-8')
    run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d', classes,
         build / 'Example.java', build / 'Exchange.java'], build / 'javac-exchange.log')
    cpp_path, java_path = build / 'cpp.opc', build / 'java.opc'
    run([args.cpp_example.resolve(), cpp_path], build / 'cpp-producer.log')
    run([args.java, '-cp', classes, 'Exchange', cpp_path, java_path], build / 'java-exchange.log')
    run([args.cpp_example.resolve(), '--read', java_path], build / 'cpp-consumer.log')
    for path in (cpp_path, java_path):
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            assert archive.read('Attachments/sample.bin') == b'\0\1\xfe\xff'
            model = etree.fromstring(archive.read('Catalog/catalog.xml'))
            assert model.attrib == {'title': 'Parts & tools'}
            assert [child.attrib for child in model] == [
                {'id': '7', 'label': 'Bolt <M6>'}, {'id': '8', 'label': 'Nut'}]
    return 2


def collision_profile(args, build):
    """Compile and execute a real schema that occupies successive public helper names."""
    directory = build / 'collision'
    directory.mkdir(exist_ok=True)
    schema = directory / 'collision.serializer'
    schema.write_text('serializer version 1.20.0; class SerializerPackage {} class SerializerPackage_2 {} '
        'class SerializerCompression {} '
        'package bundle {part attachments:bytes {name(dynamic);content_type(dynamic);'
        'relationship("urn:serializer:test-part");occurs(0,unbounded);}}', encoding='utf-8')
    run([args.compiler, '--input', schema, '--language', 'java', '--java.naming', 'preserve',
         '--output', directory / 'Collision.java'], directory / 'generate.log')
    generated = (directory / 'Collision.java').read_text(encoding='utf-8')
    helper = re.search(r'Checked ZIP/OPC operations; generated helper name: (\w+)', generated)[1]
    assert helper == 'SerializerPackage_3', helper
    source = '''
/** Verify selected public helper families do not capture user models or Java runtime types. */
public final class CollisionMain {
  /** Exercise a complete generated package facade after strict compilation of all collisions. */
  public static void main(java.lang.String[] arguments) throws java.lang.Exception {
    var value=new Collision.bundle.Input();
    value.attachments=java.util.List.of(new Collision.@helper@.BinaryPart(
        new byte[]{1,2,3},"/data.bin","application/octet-stream"));
    var result=Collision.bundle.load(Collision.bundle.encode(value));
    if(result.value.attachments.size()!=1||result.value.attachments.get(0).data[2]!=3)
      throw new java.lang.AssertionError("Collision model changed");
  }
}
'''
    (directory / 'CollisionMain.java').write_text(source.replace('@helper@', helper), encoding='utf-8')
    run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d', directory,
         directory / 'Collision.java', directory / 'CollisionMain.java'], directory / 'javac.log')
    run([args.java, '-cp', directory, 'CollisionMain'], directory / 'java.log')
    outer_profiles = []
    for outer in ('Code', 'Role'):
        case = directory / outer.lower()
        case.mkdir(exist_ok=True)
        run([args.compiler, '--input', ROOT / 'test/resources/opc_profile.serializer', '--language', 'java',
             '--output', case / (outer + '.java')], case / 'generate.log')
        smoke = '''
/** Check an output container whose name also occurs in package support's nested public types. */
public final class Smoke {
  /** Construct and round-trip an actual schema-bound model through qualified helper references. */
  public static void main(String[] arguments) throws Exception {
    var value=new @outer@.Profile.Archive.Input();value.main=new @outer@.Profile.Model();
    value.main.name="outer collision";value.main.points=new @outer@.Profile.Point[0];
    if(!@outer@.Profile.Archive.load(@outer@.Profile.Archive.encode(value)).value.main.name.equals("outer collision"))
      throw new AssertionError("Outer collision changed model");
  }
}
'''
        (case / 'Smoke.java').write_text(smoke.replace('@outer@', outer), encoding='utf-8')
        run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d', case,
             case / (outer + '.java'), case / 'Smoke.java'], case / 'javac.log')
        run([args.java, '-cp', case, 'Smoke'], case / 'java.log')
        outer_profiles.append(outer)
    return {'helper': helper, 'generated_sha256': hashlib.sha256(generated.encode()).hexdigest(),
            'outer_profiles': outer_profiles}


def main():
    """Generate actual Java, compile with the supported release contract, and qualify independent I/O."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--java', default='java')
    parser.add_argument('--cpp-example', type=Path, help='Actual native OPC example executable for producer/consumer exchange')
    parser.add_argument('--generated', type=Path, help='Reuse a captured actual emitted Schema.java for a staged SDK replay')
    args = parser.parse_args()
    build = args.build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    rows = build_fixtures(build / 'fixtures')
    if args.generated:
        shutil.copyfile(args.generated, build / 'Schema.java')
    else:
        run([args.compiler, '--input', ROOT / 'test/resources/opc_profile.serializer', '--language', 'java',
             '--output', build / 'Schema.java'], build / 'generate.log')
    parameterized = build / 'parameterized.serializer'
    parameterized.write_text((ROOT / 'test/resources/opc_profile.serializer').read_text(encoding='utf-8').replace(
        MODEL_MIME, MODEL_MIME + ';profile=\\"a  b\\";mode=read'), encoding='utf-8')
    run([args.compiler, '--input', parameterized, '--language', 'java', '--output', build / 'Parameterized.java'],
        build / 'generate-parameterized.log')
    source = MAIN.replace('ARCHIVE_COLLECTION', 'Schema.Profile.CollectionArchive').replace('ARCHIVE', 'Schema.Profile.Archive')
    source = source.replace('PKG', 'Schema.SerializerPackage')
    (build / 'Main.java').write_text(source, encoding='utf-8')
    classes = build / 'classes'
    classes.mkdir(exist_ok=True)
    run([args.javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-d', classes,
         build / 'Schema.java', build / 'Parameterized.java', build / 'Main.java'], build / 'javac.log')
    run([args.java, '-cp', classes, 'Main', build], build / 'java.log')
    counts = json.loads((build / 'java.log').read_text(encoding='utf-8').splitlines()[-1])
    inspected = inspect_outputs(build)
    exchanged = native_exchange(args, build, classes)
    collision = collision_profile(args, build)
    report = {'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'reused_generated_module': bool(args.generated),
              'generated_sha256': {name: hashlib.sha256((build / name).read_bytes()).hexdigest()
                                   for name in ('Schema.java', 'Parameterized.java', 'Main.java')},
              'fixture_decisions': len(rows), 'java': counts, 'independent_outputs': inspected,
              'native_exchanges': exchanged, 'collision_profile': collision,
              'java_version': subprocess.check_output([args.java, '-version'], stderr=subprocess.STDOUT, text=True).strip(),
              'javac_version': subprocess.check_output([args.javac, '-version'], stderr=subprocess.STDOUT, text=True).strip(),
              'elapsed_seconds': round(time.monotonic() - started, 3)}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
