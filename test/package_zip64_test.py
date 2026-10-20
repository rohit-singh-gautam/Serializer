"""Qualify ZIP64 offsets, real count transitions and budgets without multi-gigabyte storage."""
import argparse
import dataclasses
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import time
import warnings
import zipfile
import zlib

ROOT = Path(__file__).resolve().parent.parent
U32 = (1 << 32) - 1
MIB = 1024 * 1024
SIGNATURE = b'SZIP64\0\1'


@dataclasses.dataclass
class Fixture:
    """Keep independent wire segments and expected logical results, never a giant sparse allocation."""
    size: int
    segments: list
    name: str = 'boundary.bin'
    count: int = 1
    payload: bytes = b'ZIP64-boundary'
    header: int = 0
    record: int = 0
    locator: int = 0
    end: int = 0

    def patch(self, offset, data):
        """Copy one fixture and replace bytes only within an already materialized segment."""
        segments = []
        replaced = False
        for start, original in self.segments:
            if start <= offset and offset + len(data) <= start + len(original):
                position = offset - start
                original = original[:position] + data + original[position + len(data):]
                replaced = True
            segments.append((start, original))
        if not replaced:
            raise ValueError('Patch is outside a stored fixture record')
        return dataclasses.replace(self, segments=segments)

    def write(self, path):
        """Persist the small segment description consumed by the independent C++ stream buffer."""
        with path.open('wb') as output:
            output.write(SIGNATURE + struct.pack('<QI', self.size, len(self.segments)))
            for offset, data in self.segments:
                output.write(struct.pack('<QQ', offset, len(data)))
                output.write(data)


class SegmentedSource:
    """Borrow a logical seekable stream; cap actual reads independently of its multi-gigabyte extent."""
    def __init__(self, fixture, prefix=0, short=None):
        """Start at an optional nonzero archive base and retain counters for bounded-I/O assertions."""
        self.fixture, self.prefix, self.short = fixture, prefix, short
        self.position, self.closed = prefix, False
        self.read_bytes, self.read_calls, self.largest_read = 0, 0, 0

    def seekable(self):
        """Declare genuine random access without allocating or consulting a filesystem."""
        return True

    def tell(self):
        """Expose the absolute position, including any borrowed prefix before the archive."""
        return self.position

    def seek(self, offset, whence=0):
        """Address logical offsets with checked begin/current/end-relative positioning."""
        base = 0 if whence == 0 else self.position if whence == 1 else self.prefix + self.fixture.size
        position = base + offset
        if position < 0 or position > self.prefix + self.fixture.size:
            raise OSError('Seek is outside the segmented input')
        self.position = position
        return position

    def read(self, count=-1):
        """Synthesize only bounded requested bytes and optionally return legal short reads."""
        if count < 0:
            count = self.prefix + self.fixture.size - self.position
        if count > 16 * MIB:
            raise AssertionError('Reader attempted an unbounded materialization')
        if self.short is not None:
            count = min(count, self.short)
        count = min(count, self.prefix + self.fixture.size - self.position)
        self.read_calls += 1
        self.read_bytes += count
        self.largest_read = max(self.largest_read, count)
        output = bytearray(count)
        first, last = self.position - self.prefix, self.position - self.prefix + count
        for offset, data in self.fixture.segments:
            start, finish = max(first, offset), min(last, offset + len(data))
            if start < finish:
                output[start - first:finish - first] = data[start - offset:finish - offset]
        self.position += count
        return bytes(output)

    def close(self):
        """Record accidental ownership transfer; qualified readers must never call this method."""
        self.closed = True


def wire_zip64(local, directory=None, ordinary=False):
    """Author all ZIP structures directly from APPNOTE fields, independently of either runtime."""
    name, payload = b'boundary.bin', b'ZIP64-boundary'
    crc = zlib.crc32(payload)
    local_bytes = struct.pack('<4s5H3I2H', b'PK\x03\x04', 20, 0x800, 0, 0, 33,
                              crc, len(payload), len(payload), len(name), 0) + name + payload
    directory = local + len(local_bytes) if directory is None else directory
    if directory < local + len(local_bytes):
        raise ValueError('Overlapping independent records')
    extra = struct.pack('<HHQ', 1, 8, local) if local >= U32 else b''
    central = struct.pack('<4s6H3I5H2I', b'PK\x01\x02', 45, 45, 0x800, 0, 0, 33,
                          crc, len(payload), len(payload), len(name), len(extra), 0, 0, 0, 0,
                          U32 if extra else local) + name + extra
    record = directory + len(central)
    ending = struct.pack('<4sQ2H2I4Q', b'PK\x06\x06', 44, 45, 45, 0, 0, 1, 1, len(central), directory)
    locator = record + len(ending)
    ending += struct.pack('<4sIQI', b'PK\x06\x07', 0, record, 1)
    end = record + len(ending)
    ending += struct.pack('<4s4H2IH', b'PK\x05\x06', 0, 0, 1 if ordinary else 65535,
                          1 if ordinary else 65535, len(central), directory if ordinary else U32, 0)
    return Fixture(end + 22, [(local, local_bytes), (directory, central + ending)],
                   header=local, record=record, locator=locator, end=end)


def wire_classic(directory):
    """Keep a representable classic directory offset literal, even when it equals UINT32_MAX."""
    wide = wire_zip64(0, directory)
    central = wide.segments[1][1][:wide.record - directory]
    ending = struct.pack('<4s4H2IH', b'PK\x05\x06', 0, 0, 1, 1, len(central), directory, 0)
    return dataclasses.replace(wide, size=directory + len(central) + len(ending),
                               segments=[wide.segments[0], (directory, central + ending)])


def count_fixture(count, duplicate=None):
    """Use the independent standard-library writer for actual 16-bit entry-count transitions."""
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_STORED) as archive:
        for index in range(count):
            # Duplicate fixtures deliberately retain two independently valid physical records.
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', UserWarning)
                archive.writestr('part' + str(0 if index == duplicate else index), b'')
    data = output.getvalue()
    return Fixture(len(data), [(0, data)], name='part' + str(count - 1), count=count, payload=b'', header=None)


def generated_module(compiler, destination):
    """Generate the real Python OPC module, retaining its exact bytes as qualification evidence."""
    subprocess.run([str(compiler.resolve()), '--input', str(ROOT / 'test/resources/opc_profile.serializer'),
                    '--language', 'python', '--python.output', str(destination)], check=True)
    return destination


def load_module(path):
    """Import actual emitted support under a stable private test name."""
    spec = importlib.util.spec_from_file_location('serializer_zip64_fixture', path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def native_read(probe, path, fixture, limits):
    """Run the real native reader with independently configured logical resource limits."""
    args = [str(probe.resolve()), 'read', str(path), str(limits.get('archive_bytes', fixture.size)),
            str(limits.get('io_bytes', 64 * MIB)), str(limits.get('metadata_bytes', 64 * MIB)),
            str(limits.get('buffer_bytes', 16 * MIB)), str(limits.get('entries', fixture.count)), fixture.name]
    beginning = time.monotonic()
    result = subprocess.run(args, capture_output=True, text=True, check=True)
    decoded = json.loads(result.stdout)
    decoded['elapsed_seconds'] = time.monotonic() - beginning
    return decoded


def python_read(module, fixture, limits, prefix=0, short=None):
    """Exercise emitted random access, borrowed ownership and final caller position."""
    beginning = time.monotonic()
    configured = {'max_archive_bytes': fixture.size, 'max_io_bytes': 64 * MIB,
                  'max_metadata_bytes': 64 * MIB, 'max_buffer_bytes': 16 * MIB,
                  'max_entries': fixture.count}
    configured.update({'max_' + key: value for key, value in limits.items()})
    budget = module.PackageBudget(module.PackageLimits(**configured))
    borrowed = SegmentedSource(fixture, prefix, short)
    source, status = None, 'ok'
    result = {}
    try:
        source = module._PackageSource(borrowed, budget)
        archive = module._PackageZip(source, budget)
        entry = archive.entries['/' + fixture.name.lower()]
        result.update(entries=len(archive.physical_entries), header=entry[5] - 30 - len(fixture.name),
                      data=entry[5], payload=archive.read('/' + fixture.name).hex())
    except module.PackageError as error:
        status = error.code
    finally:
        if source is not None:
            source.close()
    result.update(status=status, read_bytes=borrowed.read_bytes, read_calls=borrowed.read_calls,
                  largest_read=borrowed.largest_read, io_bytes=budget.used['io_bytes'],
                  metadata_bytes=budget.used['metadata_bytes'], closed=borrowed.closed,
                  position=borrowed.position, eof=prefix + fixture.size,
                  elapsed_seconds=time.monotonic() - beginning)
    return result


def main():
    """Compare independent producers and both real backends, recording all failures before exit."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--python-module', type=Path)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--case', default='')
    args = parser.parse_args()
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    path = args.python_module
    if args.compiler:
        path = generated_module(args.compiler, build / 'generated.py')
    module = load_module(path) if path else None
    report = {'python': sys.version, 'module_sha256': hashlib.sha256(path.read_bytes()).hexdigest() if path else None,
              'cases': [], 'failures': []}

    def check(label, fixture, expected='ok', limits=None, independent=False, short=None, prefix=0):
        """Run one shared case with backend-specific counters and exact logical payload expectations."""
        if args.case and args.case not in label:
            return None
        file = build / (label + '.segments')
        fixture.write(file)
        actual = {'native': native_read(args.probe, file, fixture, limits or {})}
        if module:
            actual['python'] = python_read(module, fixture, limits or {}, prefix, short)
        if independent:
            source = SegmentedSource(fixture)
            with zipfile.ZipFile(source) as archive:
                assert len(archive.infolist()) == fixture.count
                assert archive.read(fixture.name) == fixture.payload
            assert source.read_bytes < 16 * MIB
        for backend, result in actual.items():
            errors = []
            wanted = expected[backend] if isinstance(expected, dict) else expected
            if result['status'] != wanted:
                errors.append(f"expected {wanted}, got {result['status']}")
            if wanted == result['status'] == 'ok':
                if result['entries'] != fixture.count or result['payload'] != fixture.payload.hex():
                    errors.append('wrong count or payload')
                if backend == 'native' and not result.get('order_matches', True):
                    errors.append('central-directory entry order changed')
                if fixture.header is not None and result['header'] != fixture.header:
                    errors.append('wrong logical local-header offset')
                if fixture.size > U32 and result['read_bytes'] > 256 * 1024:
                    errors.append('unbounded physical reads for virtual-offset fixture')
                if backend == 'python' and (result['closed'] or result['position'] != result['eof']):
                    errors.append('borrowed ownership or successful EOF-position mismatch')
            if errors:
                report['failures'].append({'case': label, 'backend': backend, 'errors': errors})
        report['cases'].append({'case': label, 'expected': expected, 'logical_bytes': fixture.size,
                                'stored_bytes': sum(len(data) for _, data in fixture.segments), 'results': actual})
        print(label, json.dumps(actual), flush=True)
        return actual

    beginning = time.monotonic()
    for offset in (U32 - 64, U32 - 1, U32, U32 + 1, U32 + 4096):
        check('local_' + str(offset), wire_zip64(offset), independent=True)
    for offset in (U32 - 1, U32, U32 + 1):
        check('directory_' + str(offset), wire_zip64(0, offset), independent=True)
    for offset in (U32 - 1, U32):
        check('classic_directory_' + str(offset), wire_classic(offset), independent=True)
    small = wire_zip64(0, ordinary=True)
    check('ordinary_zip64', small, independent=True)
    sample = wire_zip64(U32 + 1)
    check('short_prefixed', sample, independent=True, short=17, prefix=37)
    mutations = {
        'locator_past_end': (sample.locator + 8, '<Q', sample.size),
        'locator_u64_max': (sample.locator + 8, '<Q', (1 << 64) - 1),
        'record_too_short': (sample.record + 4, '<Q', 43),
        'record_u64_max': (sample.record + 4, '<Q', (1 << 64) - 1),
        'record_split_count': (sample.record + 24, '<Q', 2),
        'directory_past_end': (sample.record + 48, '<Q', sample.size),
        'directory_size_u64_max': (sample.record + 40, '<Q', (1 << 64) - 1),
        'narrow_size_mismatch': (sample.end + 12, '<I', 1),
        'narrow_count_mismatch': (sample.end + 8, '<HH', (2, 2)),
    }
    for label, (offset, format_string, value) in mutations.items():
        values = value if isinstance(value, tuple) else (value,)
        expected = {'native': 'unsupported_feature', 'python': 'invalid_archive'} if label == 'record_split_count' else 'invalid_archive'
        check(label, sample.patch(offset, struct.pack(format_string, *values)), expected)
    extra = sample.header + 30 + len(sample.name) + len(sample.payload) + 46 + len(sample.name)
    check('missing_offset_extra', sample.patch(extra, struct.pack('<H', 2)), 'invalid_archive')
    check('truncated_offset_extra', sample.patch(extra + 2, struct.pack('<H', 7)), 'invalid_archive')
    check('offset_extra_u64_max', sample.patch(extra + 4, struct.pack('<Q', (1 << 64) - 1)), 'invalid_archive')
    check('local_names_overlap', sample.patch(sample.header + 28, struct.pack('<H', 65535)), 'invalid_archive')
    check('short_record_extent', sample.patch(sample.locator + 8, struct.pack('<Q', sample.record + 1)), 'invalid_archive')
    check('unsupported_version', sample.patch(sample.record + 14, struct.pack('<H', 46)), 'unsupported_feature')
    for label, fixture in (('truncated_end', dataclasses.replace(sample, size=sample.size - 1,
             segments=[(offset, data[:-1] if offset + len(data) == sample.size else data) for offset, data in sample.segments])),
             ('missing_locator', sample.patch(sample.locator, b'BAD!'))):
        check(label, fixture, 'invalid_archive')
    baseline = check('budget_baseline', sample)
    if baseline:
        for resource in ('archive_bytes', 'entries', 'io_bytes', 'metadata_bytes', 'buffer_bytes'):
            exact = sample.size if resource == 'archive_bytes' else sample.count if resource == 'entries' else None
            if exact is not None:
                check('exact_' + resource, sample, limits={resource: exact})
                check('tight_' + resource, sample, 'resource_limit', {resource: exact - 1})
            else:
                # Metadata accounting includes different container overhead; qualify each backend independently.
                for backend, values in baseline.items():
                    amount = (max(65557, 65536 + len(sample.payload) * 2) if backend == 'python' else 65557) if resource == 'buffer_bytes' else values[resource]
                    file = build / 'budget_baseline.segments'
                    reader = (lambda limits: native_read(args.probe, file, sample, limits)) if backend == 'native' else (
                        lambda limits: python_read(module, sample, limits))
                    for delta, expected in ((0, 'ok'), (-1, 'resource_limit')):
                        result = reader({resource: amount + delta})
                        label = backend + ('_exact_' if delta == 0 else '_tight_') + resource
                        report['cases'].append({'case': label, 'expected': expected, 'results': {backend: result}})
                        if result['status'] != expected:
                            report['failures'].append({'case': label, 'backend': backend,
                                                       'errors': [f"expected {expected}, got {result['status']}"]})
    for count in (65534, 65535, 65536):
        if args.case and args.case not in 'count_' + str(count):
            continue
        fixture = count_fixture(count)
        check('count_' + str(count), fixture, independent=True)
        check('count_' + str(count) + '_tight', fixture, 'resource_limit', {'entries': count - 1})
    for label, index in (('early', 1), ('middle', 32768), ('late', 65535)):
        if args.case and args.case not in 'duplicate_' + label:
            continue
        check('duplicate_' + label, count_fixture(65536, index), 'duplicate_name')
    if not args.case:
        produced = build / 'native_65536.zip'
        beginning_produce = time.monotonic()
        subprocess.run([str(args.probe.resolve()), 'produce', '65536', str(produced)], check=True)
        report['native_producer_seconds'] = time.monotonic() - beginning_produce
        with zipfile.ZipFile(produced) as archive:
            assert len(archive.infolist()) == 65536 and archive.read('part65535') == b''
        fixture = Fixture(produced.stat().st_size, [(0, produced.read_bytes())], name='part65535',
                          count=65536, payload=b'', header=None)
        check('native_count_65536', fixture)
    report['elapsed_seconds'] = time.monotonic() - beginning
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'cases': len(report['cases']), 'failures': report['failures'],
                      'elapsed_seconds': report['elapsed_seconds']}, indent=2))
    raise SystemExit(bool(report['failures']))


if __name__ == '__main__':
    main()
