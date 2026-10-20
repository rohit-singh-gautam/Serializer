#!/usr/bin/env python3
# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Independently author fixtures and inspect generated ZIP/OPC wire records."""
import argparse
import binascii
import hashlib
import json
import posixpath
import struct
import xml.etree.ElementTree as etree
import zlib
from pathlib import Path

MANIFEST = Path(__file__).with_name('manifest.json')
CONTENT_NAMESPACE = 'http://schemas.openxmlformats.org/package/2006/content-types'
RELATIONSHIP_NAMESPACE = 'http://schemas.openxmlformats.org/package/2006/relationships'
RELATIONSHIP_MEDIA = 'application/vnd.openxmlformats-package.relationships+xml'
UINT16_SENTINEL = (1 << 16) - 1
UINT32_SENTINEL = (1 << 32) - 1
MAX_ARCHIVE_BYTES = 16 * 1024 * 1024
MAX_MEMBER_BYTES = 1024 * 1024
MAX_ENTRY_COUNT = 1024


def require(condition, message):
    """Keep wire assertions active when Python is invoked with optimization."""
    if not condition:
        raise ValueError(message)


def load_manifest(path=MANIFEST):
    """Read frozen fixture literals and independent backend wire policies."""
    return json.loads(Path(path).read_text(encoding='utf-8'))


def sha256(data):
    """Return a stable byte fingerprint without Unicode or newline normalization."""
    return hashlib.sha256(data).hexdigest()


def materialize(directory, manifest=None):
    """Write headerless UTF8/LF fixture tables without a language-specific parser dependency."""
    selected = manifest or load_manifest()
    directory = Path(directory)
    files = {}
    for kind, fixture in selected['fixtures'].items():
        destination = directory / kind
        destination.mkdir(parents=True, exist_ok=True)
        for key, name in (('parts', 'parts.tsv'), ('links', 'links.tsv')):
            rows = fixture[key]
            for row in rows:
                require(all('\t' not in field and '\n' not in field and '\r' not in field for field in row), 'Unescaped TSV delimiter')
            payload = ''.join('\t'.join(row) + '\n' for row in rows).encode('utf-8')
            path = destination / name
            path.write_bytes(payload)
            files[f'{kind}/{name}'] = sha256(payload)
    return {'files': files, 'zip_parts': len(selected['fixtures']['zip']['parts']),
            'opc_parts': len(selected['fixtures']['opc']['parts']),
            'opc_relationships': len(selected['fixtures']['opc']['links'])}


def permutation(items, index):
    """Apply the six specified inventory permutations independently of a producer implementation."""
    require(0 <= index < 6, 'Invalid permutation index')
    values = list(items)
    if index == 0:
        return values
    if index == 1:
        return values[::-1]
    if index in (2, 3):
        offset = 1 if index == 2 else len(values) // 2
        return values[offset:] + values[:offset]
    if index == 4:
        return values[::2] + values[1::2]
    return values[1::2][::-1] + values[::2][::-1]


def order_key(value, policy):
    """Compare names by the selected legacy byte/code-unit contract, including supplementary names."""
    if policy == 'utf16':
        return value.encode('utf-16-be')
    if policy == 'utf8':
        return value.encode('utf-8')
    require(policy == 'unicode-scalars', 'Unknown frozen name ordering policy')
    return tuple(map(ord, value))


def unpack(data, offset, layout):
    """Reject truncated fixed-width records before asking struct to read them."""
    size = struct.calcsize(layout)
    require(0 <= offset <= len(data) and size <= len(data) - offset, 'Truncated ZIP record')
    return struct.unpack_from(layout, data, offset)


def extras(data):
    """Decode complete nonduplicate extra fields, without consulting generated ZIP helpers."""
    result = {}
    position = 0
    while position < len(data):
        identifier, length = unpack(data, position, '<HH')
        position += 4
        require(length <= len(data) - position, 'Truncated ZIP extra payload')
        require(identifier not in result, 'Duplicate ZIP extra identifier')
        result[identifier] = data[position:position + length]
        position += length
    require(position == len(data), 'Trailing ZIP extra bytes')
    return result


def zip64_values(extra, expanded, compressed, local):
    """Read ZIP64 values only in the order selected by their corresponding wire sentinels."""
    fields = extras(extra)
    required = (expanded == UINT32_SENTINEL, compressed == UINT32_SENTINEL, local == UINT32_SENTINEL)
    require(set(fields) <= {1}, 'Unexpected ZIP extension field')
    count = sum(required)
    require((1 in fields) == bool(count), 'Unexpected or missing ZIP64 extra')
    values = iter(unpack(fields[1], 0, '<' + 'Q' * count)) if count else iter(())
    if count:
        require(len(fields[1]) == count * 8, 'Noncanonical ZIP64 extra extent')
    return tuple(next(values) if wanted else value for value, wanted in zip((expanded, compressed, local), required))


def raw_zip(data):
    """Verify local/central records, offsets, descriptors, CRCs and exact directory boundaries."""
    require(22 <= len(data) <= MAX_ARCHIVE_BYTES, 'Missing or oversized ZIP end record')
    end_offset = len(data) - 22
    end = unpack(data, end_offset, '<I4H2IH')
    require(end[0] == 0x06054B50 and end[-1] == 0, 'Archive comment or trailing ZIP bytes')
    require(end[1:3] == (0, 0) and end[3] == end[4], 'ZIP disk/count disagreement')
    count, central_size, central_offset = end[4], end[5], end[6]
    wide_directory = end_offset >= 20 and unpack(data, end_offset - 20, '<I')[0] == 0x07064B50
    directory_end = end_offset
    wide_versions = None
    if wide_directory:
        locator = unpack(data, end_offset - 20, '<IIQI')
        require(locator[1] == 0 and locator[3] == 1, 'Invalid ZIP64 locator disk')
        wide_offset = locator[2]
        wide = unpack(data, wide_offset, '<IQ2H2I4Q')
        require(wide[0] == 0x06064B50 and wide[1] == 44, 'Noncanonical ZIP64 end record')
        require(wide_offset + 56 == end_offset - 20, 'Unexpected bytes around ZIP64 end')
        require(wide[4:6] == (0, 0) and wide[6] == wide[7], 'ZIP64 disk/count disagreement')
        require(count in (UINT16_SENTINEL, wide[7]) and central_size in (UINT32_SENTINEL, wide[8]) and central_offset in (UINT32_SENTINEL, wide[9]), 'Classic and ZIP64 end disagreement')
        count, central_size, central_offset = wide[7], wide[8], wide[9]
        wide_versions = list(wide[2:4])
        directory_end = wide_offset
    else:
        require(count != UINT16_SENTINEL and central_size != UINT32_SENTINEL and central_offset != UINT32_SENTINEL, 'Unresolved ZIP64 directory sentinel')
    require(count <= MAX_ENTRY_COUNT, 'Unexpectedly large qualification inventory')
    require(central_offset + central_size == directory_end, 'Directory extent or trailing bytes mismatch')
    position, records = central_offset, []
    for _ in range(count):
        record = unpack(data, position, '<I6H3I5H2I')
        require(record[0] == 0x02014B50, 'Missing ZIP central signature')
        made, needed, flags, method, clock, date, crc, packed, expanded, name_size, extra_size, comment_size, disk, internal, external, local = record[1:]
        require(disk == 0 and comment_size == 0, 'Entry disk or comment metadata changed')
        require(flags & ~0x808 == 0, 'Unexpected encryption or ZIP flags')
        require(flags & 0x800, 'UTF8 ZIP name flag missing')
        start = position + 46
        require(start + name_size + extra_size <= central_offset + central_size, 'Central variable field overflow')
        name_bytes = data[start:start + name_size]
        name = name_bytes.decode('utf-8', errors='strict')
        central_extra = data[start + name_size:start + name_size + extra_size]
        wire_expanded, wire_packed, wire_local = expanded, packed, local
        expanded, packed, local = zip64_values(central_extra, expanded, packed, local)
        require(expanded <= MAX_MEMBER_BYTES, 'Unexpectedly large qualification payload')
        local_record = unpack(data, local, '<I5H3I2H')
        require(local_record[0] == 0x04034B50, 'Missing ZIP local signature')
        local_needed, local_flags, local_method, local_clock, local_date, local_crc, local_packed, local_expanded, local_name_size, local_extra_size = local_record[1:]
        require((local_flags, local_method, local_clock, local_date) == (flags, method, clock, date), 'Local/central metadata mismatch')
        local_name = data[local + 30:local + 30 + local_name_size]
        require(local_name == name_bytes, 'Local/central name mismatch')
        local_extra = data[local + 30 + local_name_size:local + 30 + local_name_size + local_extra_size]
        local_sizes = zip64_values(local_extra, local_expanded, local_packed, 0)
        payload_offset = local + 30 + local_name_size + local_extra_size
        require(payload_offset + packed <= central_offset, 'Payload crosses central directory')
        payload = data[payload_offset:payload_offset + packed]
        if method == 0:
            plain = payload
        else:
            require(method == 8, 'Unexpected compression method')
            decoder = zlib.decompressobj(wbits=-15)
            plain = decoder.decompress(payload, expanded + 1)
            require(len(plain) <= expanded and not decoder.unconsumed_tail, 'DEFLATE exceeds declared payload extent')
            plain += decoder.flush()
            require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail, 'Trailing or unfinished raw DEFLATE stream')
        require(len(plain) == expanded and binascii.crc32(plain) & UINT32_SENTINEL == crc, 'Payload CRC or expanded extent mismatch')
        finish = payload_offset + packed
        descriptor_size = 0
        if flags & 8:
            wide = wire_expanded == UINT32_SENTINEL or wire_packed == UINT32_SENTINEL
            descriptor = unpack(data, finish, '<IIQQ' if wide else '<IIII')
            require(descriptor == (0x08074B50, crc, packed, expanded), 'Unsigned or incorrect data descriptor')
            descriptor_size = 24 if wide else 16
            require(local_crc == 0 and local_sizes[0] in (0, expanded) and local_sizes[1] in (0, packed), 'Unexpected descriptor local placeholders')
        else:
            require((local_crc, local_sizes[1], local_sizes[0]) == (crc, packed, expanded), 'Local CRC or extent mismatch')
        records.append({'name': name, 'made_by': made, 'needed': needed, 'flags': flags,
                        'method': method, 'dos_time': clock, 'dos_date': date,
                        'crc32': crc, 'compressed_bytes': packed, 'expanded_bytes': expanded,
                        'local_offset': local, 'local_needed': local_needed,
                        'local_extra_hex': local_extra.hex(), 'central_extra_hex': central_extra.hex(),
                        'wire_central_expanded': wire_expanded, 'wire_central_compressed': wire_packed,
                        'wire_central_offset': wire_local, 'local_extra_sizes': list(local_sizes[:2]),
                        'wire_local_expanded': local_expanded, 'wire_local_compressed': local_packed,
                        'internal_attributes': internal, 'external_attributes': external,
                        'descriptor_bytes': descriptor_size, 'end_offset': finish + descriptor_size,
                        'payload': plain})
        position = start + name_size + extra_size + comment_size
    require(position == central_offset + central_size, 'Unparsed central directory records')
    require(len({item['name'] for item in records}) == len(records), 'Duplicate physical ZIP name')
    expected_offset = 0
    for item in records:
        require(item['local_offset'] == expected_offset, 'Reordered, overlapping or unclaimed local ZIP extent')
        expected_offset = item['end_offset']
    require(expected_offset == central_offset, 'Unclaimed bytes before directory')
    return {'entries': records, 'zip64_directory': wide_directory, 'zip64_versions': wide_versions,
            'classic_end_values': list(end[3:7]), 'directory_offset': central_offset,
            'directory_bytes': central_size}


def directory_policy(parsed, forced, policy):
    """Match every classic end field to the selected legacy ZIP64 directory policy."""
    require(parsed['zip64_directory'] == bool(forced), 'ZIP64 directory selection changed')
    if forced:
        require(parsed['zip64_versions'] == [45, 45], 'ZIP64 end version changed')
        if policy.get('force_end_sentinels', True):
            expected = [UINT16_SENTINEL, UINT16_SENTINEL, UINT32_SENTINEL, UINT32_SENTINEL]
        else:
            expected = [len(parsed['entries']), len(parsed['entries']), parsed['directory_bytes'], parsed['directory_offset']]
        require(parsed['classic_end_values'] == expected, 'Classic end ZIP64 sentinel policy changed')


def self_test():
    """Check independent literal empty-stream framing and reject contradictory raw ZIP records."""
    name, complete_stream = b'a', b'\x03\x00'
    local = struct.pack('<I5H3I2H', 0x04034B50, 20, 0x800, 8, 0, 33, 0, 2, 0, 1, 0) + name + complete_stream
    central = struct.pack('<I6H3I5H2I', 0x02014B50, 20, 20, 0x800, 8, 0, 33, 0, 2, 0, 1, 0, 0, 0, 0, 0, 0) + name
    end = struct.pack('<I4H2IH', 0x06054B50, 0, 0, 1, 1, len(central), len(local), 0)
    valid = local + central + end
    require(raw_zip(valid)['entries'][0]['payload'] == b'', 'Literal complete empty raw DEFLATE rejected')
    corrupted = {}
    for label, offset, value in [('local-name', 30, ord('b')), ('local-crc', 14, 1),
                                 ('central-crc', len(local) + 16, 1),
                                 ('central-flags', len(local) + 8, 1),
                                 ('central-offset', len(local) + 42, 1),
                                 ('archive-comment', len(valid) - 2, 1)]:
        payload = bytearray(valid)
        payload[offset] = value
        corrupted[label] = bytes(payload)
    corrupted['trailing-byte'] = valid + b'\x00'
    # A zero-byte "compressed" empty member is not a complete raw DEFLATE stream.
    invalid_local = struct.pack('<I5H3I2H', 0x04034B50, 20, 0x800, 8, 0, 33, 0, 0, 0, 1, 0) + name
    invalid_central = struct.pack('<I6H3I5H2I', 0x02014B50, 20, 20, 0x800, 8, 0, 33, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0) + name
    invalid_end = struct.pack('<I4H2IH', 0x06054B50, 0, 0, 1, 1, len(invalid_central), len(invalid_local), 0)
    corrupted['unfinished-empty-deflate'] = invalid_local + invalid_central + invalid_end
    for label, payload in corrupted.items():
        try:
            raw_zip(payload)
        except ValueError:
            continue
        raise ValueError('Oracle accepted corruption: ' + label)
    # C++ forces entry sizes while retaining actual small classic-directory values.
    wide_extra = struct.pack('<HHQQ', 1, 16, 0, 2)
    wide_local = struct.pack('<I5H3I2H', 0x04034B50, 45, 0x800, 8, 0, 33, 0,
                             UINT32_SENTINEL, UINT32_SENTINEL, 1, len(wide_extra)) + name + wide_extra + complete_stream
    wide_central = struct.pack('<I6H3I5H2I', 0x02014B50, 45, 45, 0x800, 8, 0, 33, 0,
                               UINT32_SENTINEL, UINT32_SENTINEL, 1, len(wide_extra), 0, 0, 0, 0, 0) + name + wide_extra
    wide_end_offset = len(wide_local) + len(wide_central)
    wide_end = struct.pack('<IQ2H2I4Q', 0x06064B50, 44, 45, 45, 0, 0, 1, 1, len(wide_central), len(wide_local))
    locator = struct.pack('<IIQI', 0x07064B50, 0, wide_end_offset, 1)
    classic_end = struct.pack('<I4H2IH', 0x06054B50, 0, 0, 1, 1, len(wide_central), len(wide_local), 0)
    wide_valid = wide_local + wide_central + wide_end + locator + classic_end
    directory_policy(raw_zip(wide_valid), True, {'force_end_sentinels': False})
    mixed_end = bytearray(wide_valid)
    struct.pack_into('<HH', mixed_end, len(mixed_end) - 22 + 8, UINT16_SENTINEL, UINT16_SENTINEL)
    try:
        directory_policy(raw_zip(mixed_end), True, {'force_end_sentinels': False})
    except ValueError:
        corrupted['mixed-classic-zip64-sentinels'] = bytes(mixed_end)
    else:
        raise ValueError('Oracle accepted mixed classic end sentinels under the actual-value policy')
    return {'positive_records': 2, 'rejected_corruptions': sorted(corrupted)}


def relationship_name(source):
    """Map an OPC source into its physical Relationships part using URI path rules."""
    if source == '/':
        return '_rels/.rels'
    return posixpath.join(posixpath.dirname(source), '_rels', posixpath.basename(source) + '.rels').lstrip('/')


def resolve(source, target):
    """Resolve internal target paths independently, retaining selectors as inert original text elsewhere."""
    delimiters = [target.index(marker) for marker in ('?', '#') if marker in target]
    path = target[:min(delimiters)] if delimiters else target
    if not path:
        return source
    result = posixpath.normpath(path if path.startswith('/') else posixpath.join(posixpath.dirname(source), path))
    require(result.startswith('/') and not result.startswith('/../'), 'Target escapes package root')
    return result


def xml_root(payload):
    """Parse only bounded authored metadata bytes, rejecting DTD/entity declarations explicitly."""
    require(b'<!DOCTYPE' not in payload and b'<!ENTITY' not in payload, 'Unexpected XML declaration facility')
    return etree.fromstring(payload)


def inspect_opc(entries, fixture, policy):
    """Verify complete content declarations, source-local ID order and exact inert relationship text."""
    parts = {row[0]: (row[1], bytes.fromhex(row[3])) for row in fixture['parts']}
    groups = {}
    for row in fixture['links']:
        groups.setdefault(row[0], []).append(row)
    relationship_names = {relationship_name(source): source for source in groups}
    inventory = {name.lstrip('/') for name in parts} | set(relationship_names) | {'[Content_Types].xml'}
    require(set(entries) == inventory, 'OPC physical inventory differs from authored fixture')
    for name, (_, payload) in parts.items():
        require(entries[name.lstrip('/')]['payload'] == payload, 'OPC payload bytes changed')
    root = xml_root(entries['[Content_Types].xml']['payload'])
    require(root.tag == '{' + CONTENT_NAMESPACE + '}Types' and not root.attrib and not (root.text or '').strip(), 'Content type root mismatch')
    expected = {**{name: value[0] for name, value in parts.items()},
                **{'/' + name: RELATIONSHIP_MEDIA for name in relationship_names}}
    declarations, defaults, order = {}, {}, []
    for item in root:
        require(not list(item) and not (item.text or '').strip() and not (item.tail or '').strip(), 'Unexpected content declaration content')
        if item.tag == '{' + CONTENT_NAMESPACE + '}Default':
            require(list(item.attrib) == ['Extension', 'ContentType'], 'Default attribute order changed')
            require(set(item.attrib) == {'Extension', 'ContentType'}, 'Unknown content declaration attribute')
            key = item.attrib['Extension']
            require(key not in defaults, 'Duplicate content default')
            defaults[key] = item.attrib['ContentType']
            order.append(('Default', key))
        else:
            require(item.tag == '{' + CONTENT_NAMESPACE + '}Override', 'Unknown core content declaration')
            require(list(item.attrib) == policy.get('override_attributes', policy['content_type_attributes']), 'Override attribute order changed')
            key = item.attrib['PartName']
            require(key not in declarations, 'Duplicate content override')
            declarations[key] = item.attrib['ContentType']
            order.append(('Override', key))
    for name, media in expected.items():
        require(declarations.get(name, defaults.get(name.rsplit('.', 1)[-1])) == media, 'Content type coverage/value mismatch')
    require(set(declarations) <= set(expected), 'Content override without physical payload')
    sorted_parts = sorted(parts)
    sorted_relationships = sorted('/' + name for name in relationship_names)
    kind = policy['content_type_order']
    if kind == 'all-overrides':
        expected_order = [('Override', name) for name in sorted(expected)]
    elif kind == 'payload-then-relationships':
        expected_order = [('Override', name) for name in sorted_parts + sorted_relationships]
    else:
        require(kind == 'rels-default-then-payload', 'Unknown content declaration ordering policy')
        expected_order = [('Default', 'rels')] + [('Override', name) for name in sorted_parts]
    require(order == expected_order, 'Content declaration ordering policy changed')
    for name, source in relationship_names.items():
        root = xml_root(entries[name]['payload'])
        require(root.tag == '{' + RELATIONSHIP_NAMESPACE + '}Relationships' and not root.attrib and not (root.text or '').strip(), 'Relationship root mismatch')
        expected_links = sorted(groups[source], key=lambda row: row[1])
        require(len(root) == len(expected_links), 'Relationship count mismatch')
        for item, row in zip(root, expected_links):
            _, identifier, relationship_type, target, mode = row
            attrs = {'Id': identifier, 'Type': relationship_type, 'Target': target}
            if mode == 'external':
                attrs['TargetMode'] = 'External'
            require(item.tag == '{' + RELATIONSHIP_NAMESPACE + '}Relationship' and dict(item.attrib) == attrs, 'Relationship semantics or raw URI text changed')
            require(list(item.attrib) == policy['relationship_attributes'][mode], 'Relationship attribute order changed')
            require(not list(item) and not (item.text or '').strip() and not (item.tail or '').strip(), 'Unexpected relationship content')
            if mode == 'internal':
                require(resolve(source, target).lower() in {name.lower() for name in parts}, 'Dangling internal target')
    return {'payload_parts': len(parts), 'relationship_parts': len(groups),
            'relationships': len(fixture['links']), 'content_type_order': order}


def inspect_archive(path, language, profile, force_zip64=False, manifest=None):
    """Check frozen backend metadata plus exact authored payload/OPC semantics and return fingerprints."""
    selected = manifest or load_manifest()
    require(profile in selected['profiles'] and language in selected['backends'], 'Unknown qualification selector')
    kind, storage = profile.split('-')
    fixture, policy = selected['fixtures'][kind], selected['backends'][language]
    path = Path(path)
    data = path.read_bytes()
    parsed = raw_zip(data)
    entries = parsed['entries']
    indexed = {entry['name']: entry for entry in entries}
    payloads = {row[0].lstrip('/'): bytes.fromhex(row[3]) for row in fixture['parts']}
    metadata_names = set(indexed) - set(payloads)
    expected_names = sorted(indexed, key=lambda name: order_key(name, policy['name_order']))
    if kind == 'opc' and policy.get('opc_physical_order') == 'payload-then-relationships-then-manifest':
        expected_names = sorted(payloads) + sorted(metadata_names - {'[Content_Types].xml'}) + ['[Content_Types].xml']
    require([entry['name'] for entry in entries] == expected_names, 'Physical ZIP ordering policy changed')
    for entry in entries:
        name = entry['name']
        is_metadata = name in metadata_names
        expected_method = policy['metadata_method'] if is_metadata else (0 if storage == 'stored' else 8)
        require(entry['method'] == expected_method, 'Payload/infrastructure compression selection changed')
        require((entry['dos_time'], entry['dos_date'], entry['internal_attributes']) == (0, 33, 0), 'Deterministic time/internal attributes changed')
        require(entry['flags'] == policy['flags'] and entry['external_attributes'] == policy['external_attributes'], 'Frozen flags/platform attributes changed')
        wide = force_zip64 and (not is_metadata or policy.get('force_metadata_entries', True))
        needed = 45 if wide else 20
        made = policy.get('forced_made_by', policy['made_by']) if wide else policy['made_by']
        require(entry['needed'] == needed and entry['local_needed'] == needed and entry['made_by'] == made, 'Frozen ZIP version fields changed')
        require((entry['wire_local_expanded'] == UINT32_SENTINEL) == wide and (entry['wire_local_compressed'] == UINT32_SENTINEL) == wide, 'Local ZIP64 size policy changed')
        require((entry['wire_central_expanded'] == UINT32_SENTINEL) == wide and (entry['wire_central_compressed'] == UINT32_SENTINEL) == wide, 'Central ZIP64 size policy changed')
        wide_offset = wide and policy.get('force_central_offsets', True)
        require((entry['wire_central_offset'] == UINT32_SENTINEL) == wide_offset, 'Central ZIP64 offset policy changed')
        if wide:
            placeholders = policy.get('forced_local_sizes', 'actual')
            expected_sizes = {'actual': [entry['expanded_bytes'], entry['compressed_bytes']],
                              'zero': [0, 0], 'expanded-only': [entry['expanded_bytes'], 0]}[placeholders]
            require(entry['local_extra_sizes'] == expected_sizes, 'Forced local ZIP64 extra values changed')
        elif entry['flags'] & 8:
            require(entry['local_extra_sizes'] == [0, 0], 'Classic descriptor local placeholders changed')
        if not is_metadata:
            require(name in payloads and entry['payload'] == payloads[name], 'Authored ZIP payload changed')
    directory_policy(parsed, force_zip64, policy)
    if kind == 'opc':
        semantics = inspect_opc(indexed, fixture, policy)
    else:
        require(set(indexed) == set(payloads), 'ZIP payload inventory changed')
        semantics = {'payload_parts': len(payloads)}
    projection = []
    for entry in entries:
        projection.append({key: entry[key] for key in ('name', 'made_by', 'needed', 'flags', 'method', 'dos_time', 'dos_date', 'crc32', 'expanded_bytes', 'internal_attributes', 'external_attributes')})
        projection[-1]['payload_sha256'] = sha256(entry['payload'])
        projection[-1]['local_extra_bytes'] = len(bytes.fromhex(entry['local_extra_hex']))
        projection[-1]['central_extra_bytes'] = len(bytes.fromhex(entry['central_extra_hex']))
        projection[-1]['descriptor_bytes'] = entry['descriptor_bytes']
    return {'archive': str(path), 'language': language, 'profile': profile, 'force_zip64': force_zip64,
            'archive_bytes': len(data), 'archive_sha256': sha256(data),
            'projection_sha256': sha256(json.dumps(projection, sort_keys=True, ensure_ascii=True).encode('utf-8')),
            'entries': len(entries), 'metadata_methods': sorted({indexed[name]['method'] for name in metadata_names}),
            'zip64_directory': parsed['zip64_directory'], 'semantics': semantics}


def main():
    """Expose fixture publication and one-archive validation to independent manual qualification."""
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest='command', required=True)
    fixture = subparsers.add_parser('materialize')
    fixture.add_argument('directory', type=Path)
    archive = subparsers.add_parser('inspect')
    archive.add_argument('archive', type=Path)
    archive.add_argument('language')
    archive.add_argument('profile')
    archive.add_argument('--force-zip64', action='store_true')
    subparsers.add_parser('self-test')
    args = parser.parse_args()
    if args.command == 'self-test':
        result = self_test()
    elif args.command == 'materialize':
        result = materialize(args.directory)
    else:
        result = inspect_archive(args.archive, args.language, args.profile, args.force_zip64)
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
