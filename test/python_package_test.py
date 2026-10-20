"""Qualify generated Python OPC facades against independent ZIP/XML producers and consumers."""
import argparse
import array
import dataclasses
import importlib.util
import io
import itertools
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
import xml.etree.ElementTree as etree
import zipfile
import zlib

ROOT = Path(__file__).resolve().parent.parent
MODEL_MIME = 'application/vnd.serializer.test-model+xml'
CT = 'http://schemas.openxmlformats.org/package/2006/content-types'
RELS = 'http://schemas.openxmlformats.org/package/2006/relationships'
MC = 'http://schemas.openxmlformats.org/markup-compatibility/2006'


def independent_parts(extension=False):
    """Author standard OPC metadata independently, discovering the model at an alternate path."""
    parts = {
        'Alternate/root.xml': b'<model xmlns="urn:serializer:package-test" name="independent"><point x="1" y="2" z="3"/></model>',
        'Textures/raw.bin': b'\x00\x01\xfe\xff',
        '_rels/.rels': f'<Relationships xmlns="{RELS}"><Relationship Id="main" Type="urn:serializer:main-model" Target="Alternate/root.xml"/></Relationships>'.encode(),
        'Alternate/_rels/root.xml.rels': f'<Relationships xmlns="{RELS}"><Relationship Id="texture" Type="urn:serializer:attachment" Target="../Textures/raw.bin"/></Relationships>'.encode(),
    }
    types = {'/Alternate/root.xml': MODEL_MIME, '/Textures/raw.bin': 'application/octet-stream',
             '/_rels/.rels': 'application/vnd.openxmlformats-package.relationships+xml',
             '/Alternate/_rels/root.xml.rels': 'application/vnd.openxmlformats-package.relationships+xml'}
    if extension:
        parts['Extensions/vendor.xml'] = b'<vendor xmlns="urn:vendor" flag="kept"><opaque/></vendor>'
        types['/Extensions/vendor.xml'] = 'application/vnd.vendor+xml'
        parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'</Relationships>',
            b'<Relationship Id="vendor" Type="urn:vendor:metadata" Target="Extensions/vendor.xml"/></Relationships>')
    parts['[Content_Types].xml'] = ('<Types xmlns="' + CT + '">' + ''.join(
        '<Override PartName="' + name + '" ContentType="' + mime + '"/>' for name, mime in types.items()) + '</Types>').encode()
    return parts


def independent_external_relationships(targets, relationship_type='urn:vendor:external'):
    """Author external relationship attributes through an independent XML writer without URI normalization."""
    parts = independent_parts()
    root = etree.fromstring(parts['_rels/.rels'])
    for index, target in enumerate(targets):
        etree.SubElement(root, '{' + RELS + '}Relationship', {
            'Id': f'external_{index:02d}', 'Type': relationship_type,
            'Target': target, 'TargetMode': 'External'})
    parts['_rels/.rels'] = etree.tostring(root, encoding='utf-8')
    return parts


def independent_collection_parts(bad_index=None):
    """Author three independently framed typed parts with an optional middle scalar failure."""
    parts, declarations = {}, []
    relationships = etree.Element('{' + RELS + '}Relationships')
    for index in range(3):
        name = f'Models/model{index}.xml'
        scalar = 'invalid-double' if index == bad_index else '1'
        parts[name] = (f'<model xmlns="urn:serializer:package-test" name="item{index}">'
                       f'<point x="{scalar}" y="2" z="3"/></model>').encode()
        declarations.append('<Override PartName="/' + name + '" ContentType="' + MODEL_MIME + '"/>')
        etree.SubElement(relationships, '{' + RELS + '}Relationship',
                         {'Id': f'record{index}', 'Type': 'urn:serializer:collection-model', 'Target': name})
    parts['_rels/.rels'] = etree.tostring(relationships, encoding='utf-8')
    parts['[Content_Types].xml'] = ('<Types xmlns="' + CT + '"><Default Extension="rels" '
        'ContentType="application/vnd.openxmlformats-package.relationships+xml"/>' +
        ''.join(declarations) + '</Types>').encode()
    return parts


def independent_content_type_parts(declarations, include_manifest=True):
    """Author duplicate-preserving manifest XML independently of the generated OPC metadata writer."""
    parts = independent_parts()
    parts['Extras/part%3Aname.bin'] = b'opaque'
    if include_manifest:
        parts['[Content_Types].xml'] = ('<Types xmlns="' + CT + '">' + declarations + '</Types>').encode()
    else:
        del parts['[Content_Types].xml']
    return parts


def independent_zip(parts, descriptor=False, wide=False, attributes=None):
    """Use Python's independent ZIP provider, including nonseekable ZIP64 descriptor output."""
    class sink:
        """Provide an intentionally nonseekable sink to exercise streaming descriptors."""
        def __init__(self):
            """Own the independent producer's output."""
            self.data = bytearray()
        def write(self, data):
            """Append a complete provider write."""
            self.data.extend(data)
            return len(data)
        def flush(self):
            """Satisfy the ZIP provider's nonowning sink contract."""
    output = sink() if descriptor else io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, payload in parts.items():
            item = name
            if attributes is not None and name in attributes:
                item = zipfile.ZipInfo(name)
                item.create_system, item.external_attr = attributes[name]
                item.compress_type = zipfile.ZIP_DEFLATED
            with archive.open(item, 'w', force_zip64=wide) as entry:
                entry.write(payload)
    return bytes(output.data) if descriptor else output.getvalue()


def wire_descriptor_zip(payload, wide=False, signed=True, trailing=b''):
    """Author descriptor forms directly from ZIP wire fields, independently of either implementation."""
    name, flags, method = b'data.bin', 0x808, 8
    encoder = zlib.compressobj(wbits=-15)
    compressed = encoder.compress(payload) + encoder.flush() + trailing
    crc, sentinel = zlib.crc32(payload), 0xffffffff
    extra = struct.pack('<HHQQ', 1, 16, 0, 0) if wide else b''
    output = struct.pack('<4s5H3I2H', b'PK\x03\x04', 45 if wide else 20, flags, method, 0, 33,
                         0, sentinel if wide else 0, sentinel if wide else 0, len(name), len(extra)) + name + extra + compressed
    output += (b'PK\x07\x08' if signed else b'') + struct.pack('<IQQ' if wide else '<III', crc, len(compressed), len(payload))
    offset = len(output)
    extra = struct.pack('<HHQQ', 1, 16, len(payload), len(compressed)) if wide else b''
    central = struct.pack('<4s6H3I5H2I', b'PK\x01\x02', 45, 45 if wide else 20, flags, method, 0, 33,
                          crc, sentinel if wide else len(compressed), sentinel if wide else len(payload),
                          len(name), len(extra), 0, 0, 0, 0, 0) + name + extra
    return output + central + struct.pack('<4s4H2IH', b'PK\x05\x06', 0, 0, 1, 1, len(central), offset, 0)


class python_package_test(unittest.TestCase):
    """Exercise actual emitted models; no handwritten codec substitutes participate."""
    def fixture(self):
        """Build one generated typed model and caller-supplied binary occurrence."""
        model = self.module.ProfileModel()
        model.name = 'generated & checked'
        point = self.module.ProfilePoint()
        point.x, point.y, point.z = 1.0, 2.0, 3.0
        model.points = [point]
        return self.module.ProfileArchive.Input(main=model, attachments=(
            self.module.BinaryPart(b'\x00\x01\xfe\xff', '/Textures/raw.bin', 'application/octet-stream'),))

    def test_independent_producers_and_generated_consumer(self):
        """Read ZIP32 and forced ZIP64 local headers, with and without descriptors and seekability."""
        for descriptor in (False, True):
            for wide in (False, True):
                archive = independent_zip(independent_parts(), descriptor, wide)
                for source in (archive, io.BytesIO(archive)):
                    result = self.module.ProfileArchive.load(source)
                    self.assertEqual(result.value.main.name, 'independent')
                    self.assertEqual(result.value.main.points[0].z, 3.0)
                    self.assertEqual(result.value.attachments[0].data, b'\x00\x01\xfe\xff')

    def test_generated_output_independent_reader_and_zip64_directory(self):
        """Inspect generated ZIP64 directory/locator, descriptors and XML through independent providers."""
        for wide in (False, True):
            data = self.module.ProfileArchive.encode(self.fixture(), self.module.PackageLimits(force_zip64=wide))
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                self.assertIsNone(archive.testzip())
                self.assertEqual(archive.read('Textures/raw.bin'), b'\x00\x01\xfe\xff')
                self.assertEqual(etree.fromstring(archive.read('3D/model.xml')).attrib['name'], 'generated & checked')
                self.assertEqual(etree.fromstring(archive.read('[Content_Types].xml')).tag, '{' + CT + '}Types')
                self.assertTrue(all(item.flag_bits & 8 for item in archive.infolist()))
            self.assertEqual(b'PK\x06\x06' in data, wide)
            self.assertEqual(self.module.ProfileArchive.load(data).value.main.name, 'generated & checked')

    def test_byte_source_views_and_checked_ranges(self):
        """Interpret contiguous typed/multidimensional views in bytes and bound every random read."""
        encoded = independent_zip(independent_parts())
        if len(encoded) % 2:
            changed = bytearray(encoded)
            struct.pack_into('<H', changed, len(changed) - 2, 1)
            encoded = bytes(changed) + b'x'
        words = array.array('H')
        words.frombytes(encoded)
        expanded = bytearray(len(encoded) * 2)
        expanded[::2] = encoded
        for view in (memoryview(words), memoryview(encoded).cast('B', (2, len(encoded) // 2)),
                     memoryview(expanded)[::2]):
            with self.subTest(format=view.format, shape=view.shape, strides=view.strides):
                self.assertEqual(self.module.ProfileArchive.load(view).value.main.name, 'independent')
                with self.assertRaises(self.module.PackageError) as caught:
                    self.module.ProfileArchive.load(view, self.module.PackageLimits(max_archive_bytes=len(encoded) - 1))
                self.assertEqual(caught.exception.code, 'resource_limit')
        budget = self.module.PackageBudget()
        source = self.module._PackageSource(encoded, budget)
        try:
            self.assertEqual(source.read(len(encoded), 0), b'')
            for offset, count in ((-1, 1), (0, -1), (len(encoded) + 1, 0),
                                  (len(encoded), 1), (1, len(encoded)), (2**80, 1)):
                with self.subTest(offset=offset, count=count):
                    before = dict(budget.used)
                    with self.assertRaises(self.module.PackageError) as caught:
                        source.read(offset, count)
                    self.assertEqual(caught.exception.code, 'invalid_archive')
                    self.assertEqual(budget.used, before)
        finally:
            source.close()

    def test_source_sink_admission_and_final_cancellation(self):
        """Borrow short-reading sources, reject invalid I/O, and poll after the last sink callback."""
        encoded = independent_zip(independent_parts())
        class source:
            """Offer bounded short reads while recording caller ownership."""
            def __init__(self, result=None):
                """Select ordinary input or an invalid first callback result."""
                self.offset, self.closed, self.result = 0, False, result
            def read(self, count):
                """Return a small prefix or the selected invalid result."""
                if self.result is not None:
                    return self.result(count)
                data = encoded[self.offset:self.offset + min(count, 7)]
                self.offset += len(data)
                return data
            def close(self):
                """Detect accidental closure of a borrowed source."""
                self.closed = True
        borrowed = source()
        self.assertEqual(self.module.ProfileArchive.load(borrowed).value.main.name, 'independent')
        self.assertFalse(borrowed.closed)
        for result in (lambda count: None, lambda count: bytearray(b'x'),
                       lambda count: b'x' * (count + 1)):
            invalid = source(result)
            with self.assertRaises(self.module.PackageError) as caught:
                self.module.ProfileArchive.load(invalid)
            self.assertEqual(caught.exception.code, 'io')
            self.assertFalse(invalid.closed)
        class sink:
            """Retain writes without offering ownership or implicit completion."""
            def __init__(self, result=None):
                """Keep the callback result policy and observed bytes."""
                self.data, self.result = bytearray(), result
            def write(self, block):
                """Record one bounded callback and select its reported progress."""
                self.data.extend(block)
                return len(block) if self.result is None else self.result(block)
            def flush(self):
                """Reject a flush of caller-owned state."""
                raise AssertionError('borrowed sink was flushed')
            def close(self):
                """Reject closure of caller-owned state."""
                raise AssertionError('borrowed sink was closed')
        output = sink()
        self.module.ProfileArchive.write(output, self.fixture())
        with zipfile.ZipFile(io.BytesIO(output.data)) as archive:
            self.assertIsNone(archive.testzip())
        for result in (lambda block: 0, lambda block: len(block) - 1, lambda block: None):
            invalid = sink(result)
            with self.assertRaises(self.module.PackageError) as caught:
                self.module.ProfileArchive.write(invalid, self.fixture())
            self.assertEqual(caught.exception.code, 'io')
        cancelled = [False]
        def cancel_after_write(block):
            """Cancel from the final external callback after accepting the bytes."""
            cancelled[0] = True
            return len(block)
        with self.assertRaises(self.module.PackageError) as caught:
            self.module.ProfileArchive.write(sink(cancel_after_write), self.fixture(),
                self.module.PackageLimits(is_cancelled=lambda: cancelled[0]))
        self.assertEqual(caught.exception.code, 'cancelled')

    def test_exact_cumulative_metadata_and_archive_bounds(self):
        """Keep one shared accounting context and qualify exact versus one-tight limits."""
        encoded = independent_zip(independent_parts())
        baseline = self.module.PackageBudget()
        self.module.ProfileArchive.load(encoded, budget=baseline)
        for resource in ('io_bytes', 'metadata_bytes', 'expanded_bytes', 'entries', 'relationships'):
            limit = self.module.PackageBudget._resources[resource]
            used = baseline.used[resource]
            with self.subTest(resource=resource):
                self.module.ProfileArchive.load(encoded, self.module.PackageLimits(**{limit: used}))
                with self.assertRaises(self.module.PackageError) as caught:
                    self.module.ProfileArchive.load(encoded, self.module.PackageLimits(**{limit: used - 1}))
                self.assertEqual(caught.exception.code, 'resource_limit')
        self.module.ProfileArchive.load(encoded, self.module.PackageLimits(max_archive_bytes=len(encoded)))
        shared = self.module.PackageBudget(self.module.PackageLimits(max_entries=baseline.used['entries']))
        self.module.ProfileArchive.load(encoded, budget=shared)
        before = dict(shared.used)
        with self.assertRaises(self.module.PackageError) as caught:
            self.module.ProfileArchive.load(encoded, budget=shared)
        self.assertEqual(caught.exception.code, 'resource_limit')
        self.assertGreaterEqual(shared.used['io_bytes'], before['io_bytes'])
        self.assertEqual(shared.used['entries'], before['entries'])

    def test_seekable_borrowed_base_short_reads_and_pre_io_bounds(self):
        """Borrow random access from the current base, preserve EOF, and refuse invalid ranges before reads."""
        encoded, prefix = independent_zip(independent_parts()), b'caller-owned-prefix'
        class source(io.BytesIO):
            """Return legal short reads and expose any accidental closure of caller-owned storage."""
            def __init__(self, data):
                """Retain a small physical stream and independent read/ownership counters."""
                super().__init__(data)
                self.reads, self.did_close = 0, False
            def read(self, count=-1):
                """Return at most seven requested bytes without changing seek semantics."""
                self.reads += 1
                return super().read(min(count, 7) if count >= 0 else count)
            def close(self):
                """Record an ownership violation without hiding the position evidence."""
                self.did_close = True
                super().close()
        borrowed = source(prefix + encoded)
        borrowed.seek(len(prefix))
        with mock.patch.object(self.module._pkg_tempfile, 'SpooledTemporaryFile',
                               side_effect=AssertionError('seekable input must not spool')):
            result = self.module.ProfileArchive.load(borrowed)
        self.assertEqual(result.value.main.name, 'independent')
        self.assertEqual(borrowed.tell(), len(prefix) + len(encoded))
        self.assertFalse(borrowed.did_close)

        self.assertGreater(borrowed.reads, 1)
        borrowed.seek(len(prefix))
        before = borrowed.reads
        with self.assertRaises(self.module.PackageError) as caught:
            self.module.ProfileArchive.load(borrowed,
                self.module.PackageLimits(max_archive_bytes=len(encoded) - 1))
        self.assertEqual(caught.exception.code, 'resource_limit')
        self.assertEqual(borrowed.reads, before)
        borrowed.seek(len(prefix))
        budget = self.module.PackageBudget()
        checked = self.module._PackageSource(borrowed, budget)
        try:
            for offset, count in ((-1, 1), (0, -1), (len(encoded), 1), (1 << 64, 1)):
                with self.subTest(offset=offset, count=count):
                    with self.assertRaises(self.module.PackageError) as caught:
                        checked.read(offset, count)
                    self.assertEqual(caught.exception.code, 'invalid_archive')
                    self.assertEqual(borrowed.reads, before)
        finally:
            checked.close()
        self.assertFalse(borrowed.did_close)

    def test_unsupported_seek_probe_validation_before_fallback(self):
        """Reject an invalid post-probe position before any fallback read or ownership transfer."""
        class source:
            """Fail the end-position probe and expose deliberately invalid tell results."""
            def __init__(self, restored):
                """Keep the initial valid base distinct from the later unsupported-probe result."""
                self.restored, self.calls = restored, 0
            def tell(self):
                """Report a valid initial base and then the independently selected invalid value."""
                self.calls += 1
                return 1 if self.calls == 1 else self.restored
            def seek(self, offset, whence=0):
                """Declare unsupported positioning without consuming source bytes."""
                raise io.UnsupportedOperation('not seekable')
            def read(self, count):
                """Trip if invalid positioning incorrectly admits the private spool fallback."""
                raise AssertionError('invalid position admitted a source read')
            def close(self):
                """Trip if validation transfers ownership of a caller stream."""
                raise AssertionError('borrowed input was closed')
        for restored in (True, 1.0, -1, 2):
            with self.subTest(restored=restored):
                with self.assertRaises(self.module.PackageError) as caught:
                    self.module.ProfileArchive.load(source(restored))
                self.assertEqual(caught.exception.code, 'io')

    def test_stored_entry_writer_independent_reader(self):
        """Independently inspect stored Unicode/empty/multichunk entries and an emitted stored role."""
        expected = {'/empty.bin': b'', '/nested/caf\u00e9.bin': bytes(range(256)) * 600,
                    '/plain.txt': b'stored payload'}
        parts = [(name, data, 'stored') for name, data in expected.items()]
        for wide in (False, True):
            encoded = self.module._pkg_zip_write(parts,
                self.module.PackageBudget(self.module.PackageLimits(force_zip64=wide)))
            with zipfile.ZipFile(io.BytesIO(encoded)) as archive:
                self.assertIsNone(archive.testzip())
                self.assertEqual(len(archive.infolist()), len(expected))
                for info in archive.infolist():
                    self.assertEqual(info.compress_type, zipfile.ZIP_STORED)
                    self.assertEqual(info.file_size, info.compress_size)
                    self.assertEqual(info.CRC, zlib.crc32(expected['/' + info.filename]))
                    self.assertEqual(archive.read(info), expected['/' + info.filename])
                    self.assertEqual(info.date_time, (1980, 1, 1, 0, 0, 0))
        value = self.module.ProfileDocumentProfiles.Input(main=self.fixture().main)
        encoded = self.module.ProfileDocumentProfiles.encode(value)
        with zipfile.ZipFile(io.BytesIO(encoded)) as archive:
            self.assertEqual(archive.getinfo('model.xml').compress_type, zipfile.ZIP_STORED)
            self.assertEqual(etree.fromstring(archive.read('model.xml')).attrib['name'], 'generated & checked')

    def test_canonical_unordered_parts_and_relationships(self):
        """Permute the same opaque part/link sets while preserving semantic role occurrence order."""
        fixture = self.fixture()
        parts = tuple(self.module.BinaryPart(bytes((index,)), f'/Vendor/part{index}.bin',
                                            'application/octet-stream') for index in range(3))
        links = tuple(self.module.PackageRelationship('/', f'vendor{index}', 'urn:vendor:part',
                                                     f'Vendor/part{index}.bin') for index in range(3))
        baseline = None
        for reordered_parts in itertools.permutations(parts):
            for reordered_links in itertools.permutations(links):
                extensions = self.module.PreservedExtensions(reordered_parts, reordered_links)
                value = self.module.ProfileArchive.Input(fixture.main, (), extensions)
                encoded = self.module.ProfileArchive.encode(value)
                if baseline is None:
                    baseline = encoded
                self.assertEqual(encoded, baseline)
        with zipfile.ZipFile(io.BytesIO(baseline)) as archive:
            self.assertIsNone(archive.testzip())
            self.assertEqual(archive.namelist(), sorted(archive.namelist()))
            for info in archive.infolist():
                self.assertEqual(info.date_time, (1980, 1, 1, 0, 0, 0))
                self.assertEqual(info.comment, b'')
                self.assertEqual(info.extra, b'')
                self.assertEqual(info.external_attr, 0o100600 << 16)
            relationships = etree.fromstring(archive.read('_rels/.rels'))
            ids = [link.attrib['Id'] for link in relationships]
            self.assertEqual(ids, sorted(ids))
            types = etree.fromstring(archive.read('[Content_Types].xml'))
            names = [entry.attrib['PartName'] for entry in types]
            self.assertEqual(names, sorted(names))

    def test_private_spooling_cleanup_and_cancellation(self):
        """Force disk rollover and release the private spool after success and every source failure."""
        fixture = self.fixture()
        extra = self.module.BinaryPart(bytes(range(256)) * 8193, '/Vendor/large.bin',
                                       'application/octet-stream')
        value = self.module.ProfileArchive.Input(fixture.main, (),
            self.module.PreservedExtensions(parts=(extra,)))
        encoded = self.module.ProfileArchive.encode(value)
        policy = self.module.ExtensionPolicy('preserve', 'preserve')
        created, cancelled = [], [False]
        original_spool = self.module._pkg_tempfile.SpooledTemporaryFile
        def tracked_spool(*args, **kwargs):
            """Retain only a handle observation, leaving the standard private spool implementation intact."""
            result = original_spool(*args, **kwargs)
            created.append(result)
            return result
        class source:
            """Offer a nonseekable borrowed input with a selected late failure."""
            def __init__(self, mode):
                """Track reads without allocating a second archive copy."""
                self.offset, self.mode, self.closed = 0, mode, False
            def read(self, count):
                """Fail or cancel only after enough successful data has forced disk rollover."""
                if self.offset > 1024 * 1024:
                    if os.name != 'nt':
                        self_stat = os.fstat(created[-1].fileno())
                        if self_stat.st_mode & 0o077:
                            raise AssertionError('private spool permits group/other access')
                    if self.mode == 'throw':
                        raise OSError('late borrowed source failure')
                    if self.mode == 'cancel':
                        cancelled[0] = True
                block = encoded[self.offset:self.offset + min(count, 32768)]
                self.offset += len(block)
                return block
            def close(self):
                """Record any incorrect transfer of caller ownership."""
                self.closed = True
        with tempfile.TemporaryDirectory(prefix='private-spool-', dir=self.directory) as private:
            with mock.patch.object(self.module._pkg_tempfile, 'tempdir', private), \
                 mock.patch.object(self.module._pkg_tempfile, 'SpooledTemporaryFile', tracked_spool):
                for mode in ('success', 'throw', 'cancel', 'limit'):
                    cancelled[0] = False
                    borrowed = source(mode)
                    limits = self.module.PackageLimits(
                        max_archive_bytes=len(encoded) - 1 if mode == 'limit' else len(encoded),
                        is_cancelled=lambda: cancelled[0])
                    if mode == 'success':
                        result = self.module.ProfileArchive.load(borrowed, limits, policy=policy)
                        self.assertEqual(result.extensions.parts[0].data, extra.data)
                    else:
                        expected = OSError if mode == 'throw' else self.module.PackageError
                        with self.assertRaises(expected) as caught:
                            self.module.ProfileArchive.load(borrowed, limits, policy=policy)
                        if mode != 'throw':
                            self.assertEqual(caught.exception.code, 'cancelled' if mode == 'cancel' else 'resource_limit')
                    self.assertFalse(borrowed.closed)
                    self.assertTrue(created[-1]._rolled)
                    self.assertTrue(created[-1].closed)
                    # Windows shared observers can briefly retain a delete-pending directory entry
                    # after our own O_TEMPORARY handle has closed. Persistent leftovers still fail.
                    remaining = list(Path(private).iterdir())
                    cleanup_timeout_seconds, cleanup_poll_seconds = 1.0, 0.01
                    cleanup_deadline = time.monotonic() + cleanup_timeout_seconds
                    while os.name == 'nt' and remaining and time.monotonic() < cleanup_deadline:
                        time.sleep(cleanup_poll_seconds)
                        remaining = list(Path(private).iterdir())
                    self.assertEqual(remaining, [], f'private spool cleanup after {mode}')

    def test_save_fault_matrix_keeps_complete_publication(self):
        """Inject creation/ownership/write/sync/replace faults and independently inspect any published file."""
        destination = self.directory / 'fault-matrix.opc'
        old = b'previous complete generation'
        fixture = self.fixture()
        stages = ['create', 'fdopen', 'write', 'file_sync', 'before_publish', 'replace', 'after_publish']
        if os.name != 'nt':
            stages.append('directory_sync')
        for stage in stages:
            with self.subTest(stage=stage):
                destination.write_bytes(old)
                failure = OSError('injected package ' + stage)
                descriptors = []
                original_create = self.module._pkg_tempfile.mkstemp
                original_sync = self.module._pkg_os.fsync
                sync_calls = []
                def create(*args, **kwargs):
                    """Observe new descriptors so a failing legacy implementation cannot leak the test's handle."""
                    result = original_create(*args, **kwargs)
                    descriptors.append(result)
                    return result
                def write(stream, data, budget):
                    """Leave only an unpublished file prefix before the injected transport error."""
                    stream.write(data[:7])
                    raise failure
                def fault(boundary):
                    """Raise at exactly one documented publication boundary."""
                    if boundary == stage:
                        raise failure
                def sync(descriptor):
                    """Fail the directory durability confirmation after the file has already been published."""
                    sync_calls.append(descriptor)
                    if len(sync_calls) == 2:
                        raise failure
                    return original_sync(descriptor)
                try:
                    with mock.patch.object(self.module._pkg_tempfile, 'mkstemp', create):
                        selected = {
                            'create': (self.module._pkg_tempfile, 'mkstemp', {'side_effect': failure}),
                            'fdopen': (self.module._pkg_os, 'fdopen', {'side_effect': failure}),
                            'write': (self.module, '_pkg_write_stream', {'side_effect': write}),
                            'file_sync': (self.module._pkg_os, 'fsync', {'side_effect': failure}),
                            'replace': (self.module._pkg_os, 'replace', {'side_effect': failure}),
                            'directory_sync': (self.module._pkg_os, 'fsync', {'side_effect': sync}),
                        }
                        target = selected.get(stage)
                        if target:
                            with mock.patch.object(target[0], target[1], **target[2]):
                                expected = self.module.PackageError if stage == 'directory_sync' else OSError
                                with self.assertRaises(expected) as caught:
                                    self.module.ProfileArchive.save(destination, fixture, fault=fault)
                        else:
                            expected = self.module.PackageError if stage == 'after_publish' else OSError
                            with self.assertRaises(expected) as caught:
                                self.module.ProfileArchive.save(destination, fixture, fault=fault)
                    if stage in ('after_publish', 'directory_sync'):
                        self.assertEqual(caught.exception.code, 'publication_uncertain')
                        self.assertIs(caught.exception.__cause__, failure)
                        with zipfile.ZipFile(destination) as archive:
                            self.assertIsNone(archive.testzip())
                            self.assertEqual(archive.read('Textures/raw.bin'), b'\x00\x01\xfe\xff')
                    else:
                        self.assertIs(caught.exception, failure)
                        self.assertEqual(destination.read_bytes(), old)
                    for descriptor, _ in descriptors:
                        with self.assertRaises(OSError):
                            os.fstat(descriptor)
                    self.assertFalse(list(self.directory.glob('.serializer-opc-*')))
                finally:
                    # Close only descriptors created by this subcase if its regression assertion failed.
                    for descriptor, path in descriptors:
                        try:
                            os.close(descriptor)
                        except OSError:
                            pass
                        Path(path).unlink(missing_ok=True)

    def test_typed_media_parameters_keep_case_sensitive_value_identity(self):
        """Match names/order/quoted pairs while rejecting changed case-sensitive profile values."""
        for mime in (MODEL_MIME + ';profile=Case;mode=read',
                     MODEL_MIME.upper() + ';MODE="read";PROFILE="C\\ase"'):
            parts = independent_parts()
            escaped = mime.replace('&', '&amp;').replace('"', '&quot;')
            parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(MODEL_MIME.encode(), escaped.encode())
            result = self.parameterized.ProfileArchive.load(independent_zip(parts))
            self.assertEqual(result.value.main.name, 'independent')
        for mime in (MODEL_MIME + ';profile=case;mode=read', MODEL_MIME + ';profile=Case;mode=READ'):
            parts = independent_parts()
            parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(MODEL_MIME.encode(), mime.encode())
            with self.assertRaises(self.parameterized.PackageError): self.parameterized.ProfileArchive.load(independent_zip(parts))
        budget = self.module.PackageBudget()
        self.assertTrue(self.module._pkg_mime_equal('application/test;a=x;a=y', 'application/test;a=x;a=y', budget))
        self.assertFalse(self.module._pkg_mime_equal('application/test;a=x;a=y', 'APPLICATION/TEST;A=y;a=x', budget))

    def test_content_type_identities_and_override_precedence(self):
        """Reject duplicate identities in either order while preserving valid Default/Override precedence."""
        defaults = ('<Default Extension="bin" ContentType="application/octet-stream"/>'
                    '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>')
        main_override = '<Override PartName="/Alternate/root.xml" ContentType="' + MODEL_MIME + '"/>'
        base = defaults + main_override
        duplicates = (
            ('default identical', '<Default Extension="bin" ContentType="application/octet-stream"/>'),
            ('default conflicting', '<Default Extension="bin" ContentType="text/plain"/>'),
            ('default case', '<Default Extension="BIN" ContentType="text/plain"/>'),
            ('default percent case', '<Default Extension="bi%3An" ContentType="text/plain"/>'
             '<Default Extension="BI%3aN" ContentType="application/octet-stream"/>'),
            ('override identical', main_override),
            ('override conflicting', '<Override PartName="/Alternate/root.xml" ContentType="text/plain"/>'),
            ('override case', '<Override PartName="/ALTERNATE/ROOT.XML" ContentType="text/plain"/>'),
            ('override whitespace', '<Override PartName=" /Alternate/root.xml &#x9;" ContentType="text/plain"/>'),
            ('override percent case', '<Override PartName="/Extras/part%3Aname.bin" ContentType="text/plain"/>'
             '<Override PartName="/EXTRAS/PART%3aname.BIN" ContentType="application/octet-stream"/>'))
        policy = self.module.ExtensionPolicy('skip', 'skip')
        for label, duplicate in duplicates:
            for declarations in (base + duplicate, duplicate + base):
                with self.subTest(label=label, declarations=declarations):
                    with self.assertRaises(self.module.PackageError) as caught:
                        self.module.ProfileArchive.load(independent_zip(independent_content_type_parts(declarations)), policy=policy)
                    self.assertEqual(caught.exception.code, 'invalid_metadata')
        for declarations in (base,
                             base + '<Override PartName="/Textures/raw.bin" ContentType="application/custom+binary"/>',
                             base + '<Override PartName=" /EXTRAS/PART%3aname.BIN &#x9;" ContentType="text/plain"/>'):
            with self.subTest(valid_declarations=declarations):
                parts = independent_content_type_parts(declarations)
                result = self.module.ProfileArchive.load(independent_zip(parts), policy=policy)
                self.assertEqual(result.value.main.name, 'independent')
                self.assertEqual(result.value.attachments[0].data, b'\x00\x01\xfe\xff')
                if 'application/custom+binary' in declarations:
                    self.assertEqual(result.value.attachments[0].content_type, 'application/custom+binary')

    def test_content_type_required_fields_and_media_types(self):
        """Reject missing/malformed/reserved media declarations before a generated aggregate is published."""
        binary_default = '<Default Extension="bin" ContentType="application/octet-stream"/>'
        relationships_default = '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        main_override = '<Override PartName="/Alternate/root.xml" ContentType="' + MODEL_MIME + '"/>'
        malformed = ['', relationships_default + main_override, binary_default + relationships_default,
                     relationships_default + main_override + '<Default Extension="bin"/>',
                     relationships_default + main_override + '<Default ContentType="text/plain"/>',
                     binary_default + relationships_default + '<Override PartName="/Alternate/root.xml"/>',
                     binary_default + relationships_default + '<Override ContentType="text/plain"/>']
        for mime in ('', 'text', 'application/{invalid}', 'application/octet-stream; p=&quot;unterminated',
                     'APPLICATION/VND.OPENXMLFORMATS-PACKAGE.RELATIONSHIPS+XML',
                     'application/vnd.openxmlformats-package.core-properties+xml; charset=utf-8'):
            malformed.append(relationships_default + main_override + '<Default Extension="bin" ContentType="' + mime + '"/>')
            malformed.append(binary_default + relationships_default + '<Override PartName="/Alternate/root.xml" ContentType="' + mime + '"/>')
        policy = self.module.ExtensionPolicy('skip', 'skip')
        for declarations in malformed:
            with self.subTest(declarations=declarations):
                with self.assertRaises(self.module.PackageError) as caught:
                    self.module.ProfileArchive.load(independent_zip(independent_content_type_parts(declarations)), policy=policy)
                self.assertEqual(caught.exception.code, 'invalid_metadata')
        with self.assertRaises(self.module.PackageError) as caught:
            self.module.ProfileArchive.load(independent_zip(independent_content_type_parts('', False)), policy=policy)
        self.assertEqual(caught.exception.code, 'invalid_metadata')

    def test_metadata_encodings_and_relationship_part_positions(self):
        """Accept BOM-marked UTF16 infrastructure without misclassifying ordinary _rels descendants."""
        metadata = ('[Content_Types].xml', '_rels/.rels', 'Alternate/_rels/root.xml.rels')
        for encoding, bom in (('utf-16-le', b'\xff\xfe'), ('utf-16-be', b'\xfe\xff')):
            for declaration in ('', '<?xml version="1.0" encoding="UTF-16"?>'):
                parts = independent_parts()
                for name in metadata:
                    parts[name] = bom + (declaration + parts[name].decode()).encode(encoding)
                self.assertEqual(self.module.ProfileArchive.load(independent_zip(parts)).value.main.name, 'independent')
        for payload in (independent_parts()['[Content_Types].xml'].decode().encode('utf-16-le'),
                        b'\xff\xfe' + ('<?xml version="1.0" encoding="UTF-8"?>' + independent_parts()['[Content_Types].xml'].decode()).encode('utf-16-le'),
                        b'\xff\xfe' + ('<?xml version="1.0" encoding="UTF-16LE"?>' + independent_parts()['[Content_Types].xml'].decode()).encode('utf-16-le'),
                        independent_parts()['[Content_Types].xml'].decode().encode('utf-32'),
                        ('<?xml version="1.0" encoding="ISO-8859-1"?>' + independent_parts()['[Content_Types].xml'].decode()).encode()):
            parts = independent_parts()
            parts['[Content_Types].xml'] = payload
            with self.assertRaises(self.module.PackageError): self.module.ProfileArchive.load(independent_zip(parts))
        parts = independent_parts()
        parts['_rels/nested/plain.rels'] = b'opaque payload'
        parts['[Content_Types].xml'] = parts['[Content_Types].xml'].replace(b'</Types>',
            b'<Override PartName="/_rels/nested/plain.rels" ContentType="application/octet-stream"/></Types>')
        result = self.module.ProfileArchive.load(independent_zip(parts), policy=self.module.ExtensionPolicy('preserve', 'preserve'))
        self.assertEqual(result.extensions.parts[0].name, '/_rels/nested/plain.rels')
        output = self.module.ProfileArchive.encode(self.module.ProfileArchive.Input(result.value.main, result.value.attachments, result.extensions))
        with zipfile.ZipFile(io.BytesIO(output)) as archive:
            self.assertEqual(archive.read('_rels/nested/plain.rels'), b'opaque payload')

    def test_relationship_iri_authorities_preserve_unicode_and_ascii_delimiters(self):
        """Retain exact RFC3987 authority characters through independent input and generated Save/load."""
        targets = (
            'https://left/right', 'https://left\uff0fright',
            'https://left?right', 'https://left\uff1fright',
            'https://left#right', 'https://left\uff03right',
            'https://left@right', 'https://left\uff20right',
            'https://left:80', 'https://left\uff1a80',
            'https://user\uff20name@host.invalid/a',
            'https://host\uff0fpart.invalid:443/a?view=\ue000#section',
            'https://caf\u00e9.invalid/a', 'https://host%2Fname.invalid/a',
            'https://[2001:db8::1]:443/a', 'https://[vF.name:tail]/a',
            'https:///path', '//host\uff1fname.invalid/path')
        relationship_type = 'https://types\uff0fhost\uff1fkind\uff03name\uff20role\uff1av1.invalid/vocabulary'
        data = independent_zip(independent_external_relationships(targets, relationship_type))
        policy = self.module.ExtensionPolicy('preserve', 'preserve')
        result = self.module.ProfileArchive.load(data, policy=policy)
        self.assertEqual(tuple(link.target for link in result.extensions.relationships), targets)
        self.assertTrue(all(link.type == relationship_type for link in result.extensions.relationships))
        candidate = self.module.ProfileArchive.Input(result.value.main, result.value.attachments, result.extensions)
        destination = self.directory / 'unicode-authorities.opc'
        self.module.ProfileArchive.save(destination, candidate)
        with zipfile.ZipFile(destination) as archive:
            links = [link.attrib for link in etree.fromstring(archive.read('_rels/.rels'))
                     if link.attrib['Id'].startswith('external_')]
            self.assertEqual(tuple(link['Target'] for link in links), targets)
            self.assertTrue(all(link['Type'] == relationship_type and link['TargetMode'] == 'External'
                                for link in links))
        reopened = self.module.ProfileArchive.load(destination, policy=policy)
        self.assertEqual(reopened.extensions.relationships, result.extensions.relationships)

    def test_relationship_iri_authorities_reject_invalid_input_before_publication(self):
        """Reject malformed schemes, components and IP/port syntax while preserving an existing final file."""
        targets = (
            'https://host:port/', 'https://host:\uff11/', 'https://[::1/',
            'https://[::1]tail/', 'https://[not-ip]/', 'https://[1:2:3]/',
            'https://[fe80::1%25zone]/', 'https://host]/', 'https://[::1]]/',
            'https://user@@host/', 'https://host/%zz', 'https://host/%',
            'https://host/a#one#two', '1scheme://host/', 'https://host\\evil/',
            'https://host\ue000.invalid/', 'https://host/\ue000', 'https://host/#\ue000')
        fixture = self.fixture()
        destination = self.directory / 'invalid-authorities.opc'
        original = b'original complete generation'
        destination.write_bytes(original)
        policy = self.module.ExtensionPolicy('preserve', 'preserve')
        cases = [(target, 'urn:vendor:external') for target in targets]
        cases.extend(('https://host.invalid/', kind) for kind in (
            'relative/type', 'https://host.invalid/#fragment', 'https://host:port/type'))
        for target, relationship_type in cases:
            with self.subTest(target=ascii(target), relationship_type=ascii(relationship_type)):
                data = independent_zip(independent_external_relationships((target,), relationship_type))
                with self.assertRaises(self.module.PackageError) as caught:
                    self.module.ProfileArchive.load(data, policy=policy)
                self.assertEqual(caught.exception.code, 'invalid_relationship')
                link = self.module.PackageRelationship('/', 'external_0', relationship_type, target, 'external')
                extensions = self.module.PreservedExtensions(relationships=(link,))
                candidate = self.module.ProfileArchive.Input(fixture.main, fixture.attachments, extensions)
                with self.assertRaises(self.module.PackageError) as caught:
                    self.module.ProfileArchive.save(destination, candidate)
                self.assertEqual(caught.exception.code, 'invalid_relationship')
                self.assertEqual(destination.read_bytes(), original)
                self.assertFalse(list(self.directory.glob('.serializer-opc-*')))

    def test_internal_relationship_targets_keep_literal_path_resolution(self):
        """Resolve query/fragment-bearing part paths while rejecting every internal authority or scheme."""
        for target in ('./Alternate/root.xml?view=2#section', 'Alternate/./root.xml?#',
                       '/Alternate/root.xml?next=a/b#value?with:punctuation'):
            with self.subTest(target=target):
                parts = independent_parts()
                parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'Alternate/root.xml', target.encode())
                self.assertEqual(self.module.ProfileArchive.load(independent_zip(parts)).value.main.name, 'independent')
        for target in ('//host/Alternate/root.xml', '///Alternate/root.xml', '//',
                       'https://host/Alternate/root.xml', 'name:scheme'):
            with self.subTest(target=target):
                parts = independent_parts()
                parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'Alternate/root.xml', target.encode())
                with self.assertRaises(self.module.PackageError) as caught:
                    self.module.ProfileArchive.load(independent_zip(parts))
                self.assertEqual(caught.exception.code, 'invalid_relationship')

    def test_physical_nonparts_are_excluded_but_integrity_checked(self):
        """Ignore directories and discarded logical items without skipping CRC, budgets or target checks."""
        parts = independent_parts()
        excluded = {'Textures/': b'', 'unused/': b'', '[trash]/0000.dat': b'discarded data',
                    '[trash]/0000.dat/child.bin': b'ignored descendant', '[trash]/CASE': b'first',
                    '[trash]/case': b'second', 'has space.bin': b'spaced name',
                    'invalid%ZZ.bin': b'bad escape', 'trailing./item.bin': b'bad segment',
                    '[trash]/caf\u00e9.bin': b'not a logical part', 'dos-dir.bin': b'directory payload',
                    'dos-volume.bin': b'volume label', 'ntfs-dir.bin': b'NTFS directory',
                    'vfat-dir.bin': b'VFAT directory', 'unix-link.bin': b'symlink target',
                    'unix-dir.bin': b'Unix directory', 'darwin-socket.bin': b'socket data'}
        attributes = {'dos-dir.bin': (0, 0x10), 'dos-volume.bin': (0, 0x08),
                      'ntfs-dir.bin': (10, 0x10), 'vfat-dir.bin': (14, 0x10),
                      'unix-link.bin': (3, 0o120777 << 16), 'unix-dir.bin': (3, 0o040755 << 16),
                      'darwin-socket.bin': (19, 0o140755 << 16), 'Textures/raw.bin': (3, 0o644 << 16)}
        parts.update(excluded)
        data = independent_zip(parts, attributes=attributes)
        budget = self.module.PackageBudget()
        result = self.module.ProfileArchive.load(data, budget=budget)
        self.assertEqual(result.value.main.name, 'independent')
        self.assertFalse(result.extensions.parts)
        self.assertEqual(budget.used['preserved_bytes'], 0)
        self.assertGreaterEqual(budget.used['expanded_bytes'], sum(map(len, parts.values())))
        output = self.module.ProfileArchive.encode(self.module.ProfileArchive.Input(result.value.main, result.value.attachments))
        with zipfile.ZipFile(io.BytesIO(output)) as archive:
            self.assertTrue(set(excluded).isdisjoint(archive.namelist()))
        corrupted = bytearray(data)
        source = self.module._PackageSource(data, self.module.PackageBudget())
        try:
            archive = self.module._PackageZip(source, source.budget)
            entry = next(entry for entry in archive.physical_entries if entry[0] == '/[trash]/case')
            corrupted[entry[5]] ^= 1
        finally:
            source.close()
        with self.assertRaises(self.module.PackageError): self.module.ProfileArchive.load(corrupted)
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(data, self.module.PackageLimits(max_total_expanded_bytes=budget.used['expanded_bytes'] - 1))
        linked = dict(parts)
        linked['_rels/.rels'] = linked['_rels/.rels'].replace(b'Alternate/root.xml', b'%5Btrash%5D/0000.dat')
        with self.assertRaises(self.module.PackageError): self.module.ProfileArchive.load(independent_zip(linked, attributes=attributes))
        for name in ('caf\u00e9.bin', 'caf%C3%A9.bin'):
            unsupported = dict(parts, **{name: b'possible non-ASCII logical part'})
            with self.assertRaises(self.module.PackageError) as caught:
                self.module.ProfileArchive.load(independent_zip(unsupported, attributes=attributes))
            self.assertEqual(caught.exception.code, 'unsupported_feature')
        orphan = independent_parts()
        orphan['Missing/_rels/file.bin.rels'] = ('<Relationships xmlns="' + RELS + '"/>').encode()
        orphan['[Content_Types].xml'] = orphan['[Content_Types].xml'].replace(b'</Types>',
            b'<Override PartName="/Missing/_rels/file.bin.rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/></Types>')
        with self.assertRaises(self.module.PackageError): self.module.ProfileArchive.load(independent_zip(orphan))
        for name in ('[Content_Types].xml', 'Alternate/root.xml'):
            with self.assertRaises(self.module.PackageError):
                self.module.ProfileArchive.load(independent_zip(independent_parts(), attributes={name: (0, 0x10)}))
        orphan['Missing/file.bin'] = b'nonfile source'
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(independent_zip(orphan, attributes={'Missing/file.bin': (0, 0x10)}))

    def test_relationships_markup_compatibility_scopes_and_choices(self):
        """Apply inherited MCE identities and ordered fallback selection before OPC graph validation."""
        root = f'<Relationships xmlns="{RELS}" xmlns:r="{RELS}" xmlns:mc="{MC}" xmlns:e="urn:extension" '
        relationship = '<Relationship Id="{id}" Type="{kind}" Target="Alternate/root.xml"/>'
        body = (root + 'mc:Ignorable="e" mc:ProcessContent="e:box" mc:MustUnderstand="r" e:flag="ignored">'
            '<e:discard mc:MustUnderstand="e">unknown<bad/>content</e:discard>'
            '<e:box>' + relationship.format(id='main', kind='urn:serializer:main-model').replace('/>', '>before<e:discard/>after</Relationship>') + '</e:box>'
            '<mc:AlternateContent e:flag="ignored"><mc:Choice Requires="e" mc:MustUnderstand="e"><bad/></mc:Choice>'
            '<mc:Choice Requires="r">' + relationship.format(id='choice', kind='urn:vendor:link') + '</mc:Choice>'
            '<mc:Fallback mc:MustUnderstand="e"><bad/></mc:Fallback><e:future/></mc:AlternateContent>'
            '<mc:AlternateContent><mc:Choice Requires="e"><bad/></mc:Choice><mc:Fallback>' +
            relationship.format(id='fallback', kind='urn:vendor:link') + '</mc:Fallback></mc:AlternateContent>'
            '<mc:AlternateContent><mc:Choice Requires="e"><bad/></mc:Choice></mc:AlternateContent>'
            '<e:box xmlns:e="urn:rebound" mc:Ignorable="e" mc:ProcessContent="e:*">' +
            relationship.format(id='rebound', kind='urn:vendor:link') + '</e:box></Relationships>')
        body = body.replace('Type="urn:vendor:link" Target="Alternate/root.xml"', 'Type="urn:vendor:link" Target="Textures/raw.bin"')
        parts = independent_parts()
        parts['_rels/.rels'] = body.encode()
        policy = self.module.ExtensionPolicy('preserve', 'preserve')
        shared = self.module.PackageBudget()
        result = self.module.ProfileArchive.load(independent_zip(parts), policy=policy, budget=shared)
        self.assertEqual(result.value.main.name, 'independent')
        self.assertEqual([link.id for link in result.extensions.relationships], ['choice', 'fallback', 'rebound'])
        output = self.module.ProfileArchive.encode(self.module.ProfileArchive.Input(
            result.value.main, result.value.attachments, result.extensions))
        with zipfile.ZipFile(io.BytesIO(output)) as archive:
            self.assertNotIn(MC.encode(), archive.read('_rels/.rels'))
            self.assertEqual(len(etree.fromstring(archive.read('_rels/.rels'))), 4)
        for resource in ('work_units', 'decoder_allocation_bytes'):
            limits = self.module.PackageLimits(**{'max_' + resource: shared.used[resource] - 1})
            with self.assertRaises(self.module.PackageError):
                self.module.ProfileArchive.load(independent_zip(parts), limits, policy=policy)
            self.module.ProfileArchive.load(independent_zip(independent_parts()), limits)
        parts = independent_parts()
        parts['_rels/.rels'] = (f'<mc:AlternateContent xmlns:mc="{MC}" xmlns:r="{RELS}"><mc:Choice Requires="xml"><bad/></mc:Choice><mc:Fallback>'
            '<r:Relationships>' + relationship.format(id='main', kind='urn:serializer:main-model').replace('<Relationship', '<r:Relationship') +
            '</r:Relationships></mc:Fallback></mc:AlternateContent>').encode()
        self.assertEqual(self.module.ProfileArchive.load(independent_zip(parts)).value.main.name, 'independent')

    def test_relationships_markup_compatibility_rejects_invalid_live_contracts(self):
        """Reject malformed scope, wrapper, capability and ordering contracts without weakening OPC."""
        beginning = f'<Relationships xmlns="{RELS}" xmlns:r="{RELS}" xmlns:mc="{MC}" xmlns:e="urn:extension" '
        for body in (
            'mc:Ignorable="missing">', 'mc:Ignorable="mc">', 'mc:ProcessContent="e:box">',
            'mc:Ignorable="e" mc:ProcessContent="e">', 'mc:Ignorable="e" mc:ProcessContent="e:bad:name">',
            'mc:MustUnderstand="e">', 'mc:MustUnderstand="xml">', 'mc:Unknown="">', '><e:unknown/>', '><mc:Choice Requires="r"/>',
            '><mc:Fallback/>', '><mc:Unknown/>', '><mc:AlternateContent/>',
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
            'mc:Ignorable="e" mc:ProcessContent="e:*"><mc:AlternateContent><e:box/><mc:Choice Requires="r"/></mc:AlternateContent>',
        ):
            with self.subTest(body=body):
                parts = independent_parts()
                main = '<Relationship Id="main" Type="urn:serializer:main-model" Target="Alternate/root.xml"/>'
                parts['_rels/.rels'] = (beginning + body + main + '</Relationships>').encode()
                with self.assertRaises(ValueError): self.module.ProfileArchive.load(independent_zip(parts))

    def test_descriptor_forms_and_exact_inflater(self):
        """Reject bad CRC and trailing compressed bytes while accepting all four descriptor forms."""
        for wide in (False, True):
            for signed in (False, True):
                for payload in (b'descriptor' * 100, bytes.fromhex('ac0a7ad5')):
                    data = wire_descriptor_zip(payload, wide, signed)
                    shared = self.module.PackageBudget()
                    source = self.module._PackageSource(data, shared)
                    try:
                        self.assertEqual(self.module._PackageZip(source, shared).read('/data.bin'), payload)
                    finally:
                        source.close()
                data = wire_descriptor_zip(payload, wide, signed, b'trailing')
                shared = self.module.PackageBudget()
                source = self.module._PackageSource(data, shared)
                try:
                    with self.assertRaises(self.module.PackageError):
                        self.module._PackageZip(source, shared).read('/data.bin')
                finally:
                    source.close()

    def test_shared_independent_descriptor_fixtures(self):
        """Consume the same twelve width/signature/CRC controls and four corrupt descriptors as C++."""
        directory = ROOT / 'test/resources/package_descriptor_fixtures'
        samples = {'empty': b'', 'model': b'{"count":7}', 'signature_crc': bytes.fromhex('ac0a7ad5')}
        for wide in (False, True):
            for signed in (False, True):
                prefix = ('zip64_' if wide else 'zip32_') + ('signed_' if signed else 'unsigned_')
                for label, expected in samples.items():
                    with self.subTest(prefix=prefix, label=label):
                        budget = self.module.PackageBudget()
                        source = self.module._PackageSource((directory / (prefix + label + '.zip')).read_bytes(), budget)
                        try:
                            self.assertEqual(self.module._PackageZip(source, budget).read('/data/model.json'), expected)
                        finally:
                            source.close()
                budget = self.module.PackageBudget()
                source = self.module._PackageSource((directory / (prefix + 'bad_crc.zip')).read_bytes(), budget)
                try:
                    with self.assertRaises(self.module.PackageError) as caught:
                        self.module._PackageZip(source, budget)
                    self.assertEqual(caught.exception.code, 'integrity')
                finally:
                    source.close()

    def test_integrity_structure_and_names(self):
        """Reject malformed lengths, aliases, escaping relationships and CRC errors before publication."""
        original = independent_zip(independent_parts())
        for data in (original[:-1], original + b'trailing', original[:50]):
            with self.assertRaises((self.module.PackageError, ValueError)):
                self.module.ProfileArchive.load(data)
        parts = independent_parts()
        parts['alternate/ROOT.xml'] = parts['Alternate/root.xml']
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(independent_zip(parts))
        parts = independent_parts()
        parts['_rels/.rels'] = parts['_rels/.rels'].replace(b'Alternate/root.xml', b'../../outside.xml')
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(independent_zip(parts))
        data = bytearray(self.module.ProfileArchive.encode(self.fixture()))
        source = self.module._PackageSource(data, self.module.PackageBudget())
        try:
            entry = self.module._PackageZip(source, source.budget).entries['/textures/raw.bin']
            data[entry[5]] ^= 1
        finally:
            source.close()
        with self.assertRaises((self.module.PackageError, zlib.error)):
            self.module.ProfileArchive.load(data)
        commented = bytearray(original)
        comment = b'legal comment containing PK\x05\x06 inside'
        struct.pack_into('<H', commented, len(commented) - 2, len(comment))
        commented.extend(comment)
        self.assertEqual(self.module.ProfileArchive.load(commented).value.main.name, 'independent')

    def test_shared_malformed_directory_and_deflate_corpus(self):
        """Consume the exact native mutation files and withhold every invalid aggregate before validation."""
        directory = ROOT / 'test/resources/package_malformed_fixtures'
        cases = [line.split() for line in (directory / 'cases.txt').read_text(encoding='ascii').splitlines()]
        self.assertEqual(sum(stage == 'index' for stage, _ in cases), 6)
        self.assertEqual(sum(stage == 'integrity' for stage, _ in cases), 4)
        accepted = self.module.ProfileArchive.load((directory / 'valid.zip').read_bytes())
        self.assertEqual(accepted.value.main.name, 'independent')
        self.assertEqual(accepted.value.attachments[0].data, b'\x00\x01\xfe\xff')
        for stage, filename in cases:
            with self.subTest(stage=stage, filename=filename):
                data = (directory / filename).read_bytes()
                budget = self.module.PackageBudget()
                source = self.module._PackageSource(data, budget)
                expected = 'invalid_archive' if stage == 'index' else 'integrity'
                try:
                    if stage == 'index':
                        with self.assertRaises(self.module.PackageError) as caught:
                            self.module._PackageZip(source, budget)
                    else:
                        archive = self.module._PackageZip(source, budget)
                        with self.assertRaises(self.module.PackageError) as caught:
                            archive.read('/Alternate/root.xml')
                    self.assertEqual(caught.exception.code, expected)
                finally:
                    source.close()
                published = []
                def validate(candidate, shared):
                    """Record whether a complete candidate escaped the package integrity boundary."""
                    published.append(candidate)
                with self.assertRaises(self.module.PackageError) as caught:
                    self.module.ProfileArchive.load(data, validate=validate)
                self.assertEqual(caught.exception.code, expected)
                self.assertEqual(published, [])

    def test_extensions_and_domain_validation(self):
        """Retain whole unknown parts and links, reject required capabilities and revalidate edited output."""
        data = independent_zip(independent_parts(True))
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(data)
        policy = self.module.ExtensionPolicy('preserve', 'preserve')
        result = self.module.ProfileArchive.load(data, policy=policy)
        self.assertEqual(result.extensions.parts[0].data, independent_parts(True)['Extensions/vendor.xml'])
        rewritten = self.module.ProfileArchive.encode(self.module.ProfileArchive.Input(
            result.value.main, result.value.attachments, result.extensions))
        with zipfile.ZipFile(io.BytesIO(rewritten)) as archive:
            self.assertEqual(archive.read('Extensions/vendor.xml'), independent_parts(True)['Extensions/vendor.xml'])
        with self.assertRaises(self.module.PackageError):
            self.module.ProfileArchive.load(data, policy=self.module.ExtensionPolicy('preserve', 'preserve', lambda part: True))
        selected = []
        def validate(candidate, shared):
            """Charge domain work through the same context and fail the unpublished candidate."""
            selected.append(candidate.main.name)
            shared.consume('references', 1)
            raise ValueError('domain validation rejected')
        with self.assertRaisesRegex(ValueError, 'domain validation rejected'):
            self.module.ProfileArchive.load(data, policy=policy, validate=validate)
        self.assertEqual(selected, ['independent'])

    def test_middle_typed_part_failure_never_returns_an_aggregate(self):
        """Reject first/middle/last XML failures before domain validation or destination assignment."""
        accepted = self.module.ProfileCollectionArchive.load(independent_zip(independent_collection_parts()))
        self.assertEqual([part.value.name for part in accepted.value.models], ['item0', 'item1', 'item2'])
        for bad_index in range(3):
            with self.subTest(bad_index=bad_index):
                destination, original = accepted, accepted
                validated = []
                with self.assertRaises(self.module.PackageError) as caught:
                    destination = self.module.ProfileCollectionArchive.load(
                        independent_zip(independent_collection_parts(bad_index)),
                        validate=lambda candidate, shared: validated.append(candidate))
                self.assertEqual(caught.exception.code, 'codec')
                self.assertEqual(caught.exception.part, f'/Models/model{bad_index}.xml')
                self.assertEqual(caught.exception.relationship_id, f'record{bad_index}')
                self.assertIs(destination, original)
                self.assertEqual(validated, [])

    def test_optional_and_required_extension_policy_matrix(self):
        """Qualify each part/link reject/skip/preserve combination and both required-capability hooks."""
        data = independent_zip(independent_parts(True))
        for parts in ('reject', 'skip', 'preserve'):
            for links in ('reject', 'skip', 'preserve'):
                with self.subTest(parts=parts, relationships=links):
                    policy = self.module.ExtensionPolicy(parts, links)
                    if 'reject' in (parts, links):
                        with self.assertRaises(self.module.PackageError) as caught:
                            self.module.ProfileArchive.load(data, policy=policy)
                        self.assertEqual(caught.exception.code, 'unsupported_feature')
                    else:
                        accepted = self.module.ProfileArchive.load(data, policy=policy)
                        self.assertEqual(len(accepted.extensions.parts), int(parts == 'preserve'))
                        self.assertEqual(len(accepted.extensions.relationships), int(links == 'preserve'))
                        for required_part, required_link in ((True, False), (False, True), (True, True)):
                            required = self.module.ExtensionPolicy(parts, links,
                                (lambda part: True) if required_part else None,
                                (lambda link: True) if required_link else None)
                            with self.assertRaises(self.module.PackageError) as caught:
                                self.module.ProfileArchive.load(data, policy=required)
                            self.assertEqual(caught.exception.code, 'unsupported_feature')

    def test_edited_preserved_relationships_revalidate_before_save(self):
        """Keep opaque bytes on a valid edit and reject removed parts/sources/targets before replacement."""
        accepted = self.module.ProfileArchive.load(independent_zip(independent_parts(True)),
            policy=self.module.ExtensionPolicy('preserve', 'preserve'))
        destination = self.directory / 'preserved-edit.opc'
        original = b'previous generation'
        invalid = (
            self.module.PreservedExtensions((), accepted.extensions.relationships),
            self.module.PreservedExtensions(accepted.extensions.parts, (
                self.module.PackageRelationship('/Missing/source.xml', 'vendor', 'urn:vendor:metadata',
                                                '../Extensions/vendor.xml'),)),
            self.module.PreservedExtensions(accepted.extensions.parts, (
                self.module.PackageRelationship('/', 'vendor', 'urn:vendor:metadata', 'Missing/target.xml'),)))
        for extensions in invalid:
            with self.subTest(extensions=extensions):
                destination.write_bytes(original)
                candidate = self.module.ProfileArchive.Input(accepted.value.main,
                    accepted.value.attachments, extensions)
                with self.assertRaises(self.module.PackageError) as caught:
                    self.module.ProfileArchive.save(destination, candidate)
                self.assertEqual(caught.exception.code, 'missing_part')
                self.assertEqual(destination.read_bytes(), original)
                self.assertFalse(list(self.directory.glob('.serializer-opc-*')))
        self.module.ProfileArchive.save(destination, self.module.ProfileArchive.Input(
            accepted.value.main, accepted.value.attachments, accepted.extensions))
        with zipfile.ZipFile(destination) as archive:
            self.assertEqual(archive.read('Extensions/vendor.xml'), accepted.extensions.parts[0].data)

    def test_fixed_role_relocation_rebinds_preserved_link_scopes(self):
        """Discover an alternate typed path and keep inbound, outbound, self and external links on Save."""
        parts = independent_parts(True)
        root = etree.fromstring(parts['_rels/.rels'])
        etree.SubElement(root, '{' + RELS + '}Relationship', {
            'Id': 'alias', 'Type': 'urn:vendor:alias',
            'Target': 'alternate/ROOT.xml?view=1#selected'})
        parts['_rels/.rels'] = etree.tostring(root)
        root = etree.fromstring(parts['Alternate/_rels/root.xml.rels'])
        for identifier, target, mode in (
                ('outbound', '../Extensions/vendor.xml#payload', 'Internal'),
                ('self', '?view=2#self', 'Internal'),
                ('external', '../Alternate/root.xml?view=3#external', 'External')):
            etree.SubElement(root, '{' + RELS + '}Relationship', {
                'Id': identifier, 'Type': 'urn:vendor:' + identifier,
                'Target': target, 'TargetMode': mode})
        parts['Alternate/_rels/root.xml.rels'] = etree.tostring(root)
        root = etree.Element('{' + RELS + '}Relationships')
        etree.SubElement(root, '{' + RELS + '}Relationship', {
            'Id': 'back', 'Type': 'urn:vendor:back', 'Target': '../Alternate/root.xml#back'})
        parts['Extensions/_rels/vendor.xml.rels'] = etree.tostring(root)
        manifest = etree.fromstring(parts['[Content_Types].xml'])
        etree.SubElement(manifest, '{' + CT + '}Override', {
            'PartName': '/Extensions/_rels/vendor.xml.rels',
            'ContentType': 'application/vnd.openxmlformats-package.relationships+xml'})
        parts['[Content_Types].xml'] = etree.tostring(manifest)
        policy = self.module.ExtensionPolicy('preserve', 'preserve')
        accepted = self.module.ProfileArchive.load(independent_zip(parts), policy=policy)
        self.assertEqual(accepted.extensions.relocations, (('/Alternate/root.xml', '/3D/model.xml'),))
        candidate = self.module.ProfileArchive.Input(accepted.value.main,
            accepted.value.attachments, accepted.extensions)
        destination = self.directory / 'relocated.opc'
        self.module.ProfileArchive.save(destination, candidate)
        with zipfile.ZipFile(destination) as archive:
            self.assertNotIn('Alternate/root.xml', archive.namelist())
            self.assertNotIn('Alternate/_rels/root.xml.rels', archive.namelist())
            self.assertEqual(archive.read('Extensions/vendor.xml'), parts['Extensions/vendor.xml'])
            links = {element.attrib['Id']: element.attrib for name in (
                '_rels/.rels', '3D/_rels/model.xml.rels', 'Extensions/_rels/vendor.xml.rels')
                for element in etree.fromstring(archive.read(name))}
            self.assertEqual(links['alias']['Target'], '/3D/model.xml?view=1#selected')
            self.assertEqual(links['outbound']['Target'], '/Extensions/vendor.xml#payload')
            self.assertEqual(links['self']['Target'], '/3D/model.xml?view=2#self')
            self.assertEqual(links['back']['Target'], '/3D/model.xml#back')
            self.assertEqual(links['external']['Target'], '../Alternate/root.xml?view=3#external')
            self.assertEqual(links['external']['TargetMode'], 'External')
        reopened = self.module.ProfileArchive.load(destination, policy=policy)
        self.assertEqual(reopened.value.main.name, accepted.value.main.name)
        self.assertEqual(len(reopened.extensions.relationships), len(accepted.extensions.relationships))
        original_links = {link.id: link for link in accepted.extensions.relationships}
        self.assertEqual(original_links['outbound'].source, '/Alternate/root.xml')
        self.assertEqual(original_links['alias'].target, 'alternate/ROOT.xml?view=1#selected')

    def test_conflicting_fixed_role_relocations_preserve_the_previous_file(self):
        """Reject ambiguous rebinding before replacement, retaining the old generation and no staging files."""
        accepted = self.module.ProfileArchive.load(independent_zip(independent_parts(True)),
            policy=self.module.ExtensionPolicy('preserve', 'preserve'))
        conflicting = dataclasses.replace(accepted.extensions, relocations=(
            ('/Alternate/root.xml', '/3D/model.xml'), ('/alternate/ROOT.xml', '/Wrong/model.xml')))
        destination = self.directory / 'relocation-conflict.opc'
        destination.write_bytes(b'previous generation')
        with self.assertRaises(self.module.PackageError) as caught:
            self.module.ProfileArchive.save(destination, self.module.ProfileArchive.Input(
                accepted.value.main, accepted.value.attachments, conflicting))
        self.assertEqual(caught.exception.code, 'invalid_relationship')
        self.assertEqual(destination.read_bytes(), b'previous generation')
        self.assertFalse(list(self.directory.glob('.serializer-opc-*')))

    def test_shared_limits_cancellation_and_atomic_save(self):
        """A middle-stage failure preserves old final bytes and one budget spans ZIP, OPC and generated XML."""
        for limits in (self.module.PackageLimits(max_entries=1), self.module.PackageLimits(max_decoder_allocation_bytes=1),
                       self.module.PackageLimits(is_cancelled=lambda: True), self.module.PackageLimits(deadline=time.monotonic())):
            with self.assertRaises(self.module.PackageError):
                self.module.ProfileArchive.load(independent_zip(independent_parts()), limits)
        budget = self.module.PackageBudget()
        self.module.ProfileArchive.load(independent_zip(independent_parts()), budget=budget)
        self.assertGreater(budget.used['work_units'], 100)
        self.assertGreater(budget.used['decoder_allocation_bytes'], 100)
        destination = self.directory / 'atomic.opc'
        destination.write_bytes(b'old generation')
        def fail(stage):
            """Inject a failure at the selected publication boundary."""
            if stage == 'before_publish': raise RuntimeError('before publish')
        with self.assertRaisesRegex(RuntimeError, 'before publish'):
            self.module.ProfileArchive.save(destination, self.fixture(), fault=fail)
        self.assertEqual(destination.read_bytes(), b'old generation')
        self.assertFalse(list(self.directory.glob('.serializer-opc-*')))
        self.module.ProfileArchive.save(destination, self.fixture())
        self.assertEqual(self.module.ProfileArchive.load(destination).value.main.name, 'generated & checked')


def main():
    """Generate the actual facade with the selected compiler, import it, and run independent qualification."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='python-opc-', dir=build) as temporary:
        directory = Path(temporary)
        generated = directory / 'generated.py'
        subprocess.run([str(args.compiler.resolve()), '--input', str(ROOT / 'test/resources/opc_profile.serializer'),
                        '--language', 'python', '--python.output', str(generated)], check=True)
        spec = importlib.util.spec_from_file_location('serializer_python_opc_fixture', generated)
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        python_package_test.module = module
        python_package_test.directory = directory
        parameterized_source = directory / 'parameterized.serializer'
        parameterized_source.write_text((ROOT / 'test/resources/opc_profile.serializer').read_text(encoding='utf-8').replace(
            MODEL_MIME, MODEL_MIME + ';profile=Case;mode=read'), encoding='utf-8')
        parameterized_output = directory / 'parameterized.py'
        subprocess.run([str(args.compiler.resolve()), '--input', str(parameterized_source), '--language', 'python',
                        '--python.output', str(parameterized_output)], check=True)
        spec = importlib.util.spec_from_file_location('serializer_python_opc_parameter_fixture', parameterized_output)
        parameterized = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = parameterized
        spec.loader.exec_module(parameterized)
        python_package_test.parameterized = parameterized
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_package_test))
        if not result.wasSuccessful(): raise SystemExit(1)


if __name__ == '__main__':
    main()
