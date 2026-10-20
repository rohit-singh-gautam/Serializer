// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagepython(
import dataclasses as _pkg_dataclasses
import ipaddress as _pkg_ip
import io as _pkg_io
import os as _pkg_os
import tempfile as _pkg_tempfile
import time as _pkg_time
import zlib as _pkg_zlib

_PKG_CT = 'http://schemas.openxmlformats.org/package/2006/content-types'
_PKG_RELS = 'http://schemas.openxmlformats.org/package/2006/relationships'
_PKG_RELS_MIME = 'application/vnd.openxmlformats-package.relationships+xml'
_PKG_CHUNK = 65536
_PKG_U32 = 0xffffffff
_PKG_ASCII_FOLD = str.maketrans('ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz')
_PKG_MIME_TOKEN = r"[!#$%&'*+.^_`|~0-9A-Za-z-]+"
_PKG_MIME_QUOTED = r'"(?:[\t\x20-\x21\x23-\x5b\x5d-\x7e\x80-\xff]|\\[\t\x20-\x7e\x80-\xff])*"'
_PKG_MIME_PARAMETER = _re.compile(r'[ \t]*;[ \t]*(' + _PKG_MIME_TOKEN + ')=(' + _PKG_MIME_TOKEN + '|' + _PKG_MIME_QUOTED + ')')
_PKG_MIME_PATTERN = _re.compile(_PKG_MIME_TOKEN + '/' + _PKG_MIME_TOKEN + '(?:' + _PKG_MIME_PARAMETER.pattern + ')*')

class PackageError(ValueError):
    """Bounded package diagnostics never include model or attachment payloads."""
    def __init__(self, code, message, part='', relationship_id='', reference='', field_path=''):
        """Retain the failing operation's logical source and relationship context."""
        super().__init__(message)
        self.code, self.part = code, part
        self.relationship_id, self.reference, self.field_path = relationship_id, reference, field_path

@_pkg_dataclasses.dataclass(frozen=True)
class PackageLimits:
    """Logical operation limits; retained Python/container overhead is not a process-memory cap."""
    max_archive_bytes: int = 256 * 1024 * 1024
    max_io_bytes: int = 1024 * 1024 * 1024
    max_metadata_bytes: int = 16 * 1024 * 1024
    max_part_bytes: int = 64 * 1024 * 1024
    max_total_expanded_bytes: int = 256 * 1024 * 1024
    max_buffer_bytes: int = 128 * 1024 * 1024
    max_entries: int = 4096
    max_relationships: int = 16384
    max_decoder_allocation_bytes: int = 128 * 1024 * 1024
    max_work_units: int = 512 * 1024 * 1024
    max_references: int = 65536
    max_preserved_bytes: int = 64 * 1024 * 1024
    is_cancelled: object = None
    deadline: object = None
    force_zip64: bool = False

class PackageBudget:
    """One cumulative budget follows ZIP, metadata, generated XML and domain validation."""
    _resources = {'io_bytes': 'max_io_bytes', 'metadata_bytes': 'max_metadata_bytes',
                  'expanded_bytes': 'max_total_expanded_bytes', 'entries': 'max_entries',
                  'relationships': 'max_relationships', 'decoder_allocation_bytes': 'max_decoder_allocation_bytes',
                  'work_units': 'max_work_units', 'references': 'max_references', 'preserved_bytes': 'max_preserved_bytes'}
    def __init__(self, limits=None):
        """Validate immutable bounds before reading or creating package storage."""
        self.limits = limits or PackageLimits()
        for field in _pkg_dataclasses.fields(self.limits):
            value = getattr(self.limits, field.name)
            if field.name.startswith('max_') and (type(value) is not int or value < 0):
                raise PackageError('resource_limit', 'Invalid package limit')
        self.used = dict.fromkeys(self._resources, 0)
        if self.limits.is_cancelled is not None and not callable(self.limits.is_cancelled):
            raise PackageError('invalid_state', 'Package cancellation predicate must be callable')
        self.check()
    def check(self):
        """Check monotonic deadline and cancellation at each bounded processing boundary."""
        if self.limits.is_cancelled is not None and self.limits.is_cancelled():
            raise self._failure('cancelled', 'Package operation cancelled')
        if self.limits.deadline is not None and _pkg_time.monotonic() >= self.limits.deadline:
            raise self._failure('deadline_exceeded', 'Package operation deadline exceeded')
    @staticmethod
    def _failure(code, message, part=''):
        """Identify runtime-created budget failures without changing their public exception class."""
        error = PackageError(code, message, part)
        error._budget_failure = True
        return error
    def remaining(self, resource):
        """Expose unspent cumulative capacity without resetting failed work."""
        return getattr(self.limits, self._resources[resource]) - self.used[resource]
    def consume(self, resource, amount, part=''):
        """Charge a nonnegative resource before allocating or exposing its result."""
        self.check()
        if type(amount) is not int or amount < 0 or amount > self.remaining(resource):
            raise self._failure('resource_limit', 'Package ' + resource + ' limit exceeded', part)
        self.used[resource] += amount
    def check_buffer(self, amount, part=''):
        """Bound simultaneously materialized byte buffers before their allocation."""
        self.check()
        if amount > self.limits.max_buffer_bytes:
            raise self._failure('resource_limit', 'Package buffer limit exceeded', part)
    def xml_limits(self):
        """Narrow nested XML limits without replacing this operation's shared counters."""
        return Limits(max_bytes=self.limits.max_part_bytes, max_string_bytes=self.limits.max_part_bytes,
                      max_elements=min(1000000, self.remaining('work_units')), max_depth=64)
    def xml_context(self, part='', relationship_id=''):
        """Borrow cumulative codec/reference accounting for a synchronous custom typed-part codec."""
        return _PackagePartContext(self, part, relationship_id)

@_pkg_dataclasses.dataclass(frozen=True)
class PackageRelationship:
    """A source-local relationship keeps external targets inert."""
    source: str
    id: str
    type: str
    target: str
    mode: str = 'internal'

@_pkg_dataclasses.dataclass(frozen=True)
class BinaryPart:
    """Borrow binary data for Save or own decoded data with its discovered URI and MIME type."""
    data: object
    name: str
    content_type: str
    source_index: int = 0
    compression: str = 'stored'

@_pkg_dataclasses.dataclass(frozen=True)
class ModelPart:
    """Supply one dynamic typed role occurrence and its parent occurrence."""
    value: object
    name: str
    content_type: str
    source_index: int = 0

@_pkg_dataclasses.dataclass(frozen=True)
class PreservedExtensions:
    """Retain unknown whole parts verbatim and their original scoped relationships."""
    parts: tuple = ()
    relationships: tuple = ()
    relocations: tuple = ()

@_pkg_dataclasses.dataclass(frozen=True)
class ExtensionPolicy:
    """Required unknown capabilities reject even when optional extensions are skipped or retained."""
    parts: str = 'reject'
    relationships: str = 'reject'
    is_required_part: object = None
    is_required_relationship: object = None

@_pkg_dataclasses.dataclass(frozen=True)
class PackageResult:
    """Publish only a fully validated fresh typed aggregate and its retained extensions."""
    value: object
    extensions: PreservedExtensions

)packagepython"
R"packagepython(class _PackagePartContext:
    """Borrow one operation budget while annotating nested XML failures with their part and link."""
    def __init__(self, shared, part, relationship_id=''):
        """Keep a synchronous borrowed context without copying or resetting shared counters."""
        self.shared, self.part, self.relationship_id = shared, part, relationship_id
        self.limits = shared.limits
    def consume(self, resource, amount, part=''):
        """Charge before work and preserve package diagnostics through generated XML callbacks."""
        self._xml_consume_at(resource, amount, None, part)
    def check_buffer(self, amount, part=''):
        """Retain the source URI when a nested writer exhausts its byte workspace."""
        self._xml_buffer_at(amount, None, part)
    def check(self):
        """Apply the original operation's deadline and cancellation predicate."""
        self._xml_check_at(None)
    def _locate(self, error, field):
        """Fill missing structured locations only on the package runtime's own failures."""
        if not getattr(error, '_budget_failure', False): return
        if not error.part: error.part = self.part
        if not error.relationship_id: error.relationship_id = self.relationship_id
        if not error.field_path and field is not None:
            names = []
            while field is not None:
                names.append(field[0])
                field = field[1]
            error.field_path = '.'.join(reversed(names))
    def _xml_consume_at(self, resource, amount, field, part=''):
        """Charge before work without swallowing unrelated provider or caller exceptions."""
        try:
            self.shared.consume(resource, amount, part or self.part)
        except PackageError as error:
            self._locate(error, field)
            raise
    def _xml_buffer_at(self, amount, field, part=''):
        """Keep the field and link on buffer-limit, cancellation and deadline failures."""
        try:
            self.shared.check_buffer(amount, part or self.part)
        except PackageError as error:
            self._locate(error, field)
            raise
    def _xml_check_at(self, field):
        """Apply interruption at bounded field transitions without resetting the budget."""
        try:
            self.shared.check()
        except PackageError as error:
            self._locate(error, field)
            raise
    def _xml_reference_at(self, field):
        """Share tuple admission with archive relationships and caller reference validation."""
        self._xml_consume_at('references', 1, field)
    def admit_reference(self):
        """Admit a root identity tuple when no generated field scope is active."""
        self._xml_reference_at(None)

def _pkg_budget(limits, budget):
    """Keep exactly one caller-selected operation context rather than silently resetting nested limits."""
    if budget is not None:
        if limits is not None or not isinstance(budget, PackageBudget):
            raise PackageError('invalid_state', 'Supply either package limits or one shared budget')
        return budget
    return PackageBudget(limits)

def _pkg_name(name, payload=True):
    """Validate the canonical ASCII OPC URI subset without filesystem interpretation or repeated unescaping."""
    if type(name) is not str or len(name) < 2 or len(name) > 65536 or not name.startswith('/') or name.endswith('/'):
        raise PackageError('invalid_name', 'Invalid OPC part URI')
    if any(character in name for character in '?#\\'):
        raise PackageError('invalid_name', 'Invalid OPC part URI', name)
    for segment in name[1:].split('/'):
        if not segment or segment.endswith('.'):
            raise PackageError('invalid_name', 'Invalid OPC URI segment', name)
        index = 0
        while index < len(segment):
            character = segment[index]
            if character == '%':
                escape = segment[index + 1:index + 3]
                if len(escape) != 2 or not _re.fullmatch('[0-9a-fA-F]{2}', escape):
                    raise PackageError('invalid_name', 'Malformed OPC URI escape', name)
                decoded = chr(int(escape, 16))
                if decoded.isascii() and (decoded.isalnum() or decoded in '-._~/\\') or ord(decoded) >= 0x80:
                    raise PackageError('invalid_name', 'Forbidden OPC URI escape', name)
                index += 3
            elif character.isascii() and (character.isalnum() or character in "-._~!$&'()*+,;=:@"):
                index += 1
            else:
                raise PackageError('invalid_name', 'Invalid OPC URI character', name)
    if payload and (name.lower() == '/[content_types].xml' or _pkg_relationship_source(name) is not None):
        raise PackageError('invalid_name', 'OPC infrastructure names are reserved', name)
    return name

def _pkg_zip_item_name(name):
    """Retain safe relative physical ZIP names before the independent OPC logical-part mapping."""
    if not name or name.startswith('/') or '\\' in name or '\x00' in name:
        raise PackageError('invalid_name', 'Invalid ZIP item name')
    segments = name.split('/')
    if any(segment in ('', '.', '..') for segment in segments[:-1]) or segments[-1] in ('.', '..'):
        raise PackageError('invalid_name', 'Invalid ZIP item path segment')
    return '/' + name


def _pkg_physical_file(entry):
    """Interpret only established DOS/Unix creator attributes when mapping a physical ZIP file."""
    if entry[0].endswith('/'): return False
    creator, attributes = entry[6], entry[7]
    dos_creators, unix_creators = (0, 10, 14), (3, 19)
    dos_directory_or_volume, unix_type_mask, unix_regular_file = 0x18, 0o170000, 0o100000
    if creator in dos_creators and attributes & dos_directory_or_volume: return False
    if creator in unix_creators:
        kind = (attributes >> 16) & unix_type_mask
        if kind and kind != unix_regular_file: return False
    return True


def _pkg_maps_to_part(name, budget):
    """Exclude nonfile/non-PartName items while keeping unsupported Unicode candidates fail-closed."""
    budget.consume('work_units', len(name) + 1)
    if name.endswith('/'): return False
    unsupported = not name.isascii() or _re.search(r'%[89a-fA-F][0-9a-fA-F]', name) is not None
    if unsupported:
        budget.consume('decoder_allocation_bytes', len(name) * 12)
        probe = _re.sub(r'%[89a-fA-F][0-9a-fA-F]', '%20', name)
        probe = ''.join(character if character.isascii() else '_' for character in probe)
    else:
        probe = name
    try:
        _pkg_name(probe, False)
    except PackageError as error:
        if error.code != 'invalid_name': raise
        return False
    if unsupported:
        raise PackageError('unsupported_feature', 'Non-ASCII OPC part names are unsupported', name)
    return True


def _pkg_relationship_source(name):
    """Recover the source of standard relationship infrastructure without following any target."""
    if name.lower() == '/_rels/.rels':
        return '/'
    lowered = name.lower()
    basename_start = lowered.rfind('/')
    separator = lowered.rfind('/', 0, basename_start)
    if lowered[separator + 1:basename_start] != '_rels' or not lowered.endswith('.rels'):
        return None
    basename = name[basename_start + 1:-len('.rels')]
    if not basename or '/' in basename:
        raise PackageError('invalid_metadata', 'Invalid relationship part name', name)
    return _pkg_name(name[:separator + 1] + basename, False)

def _pkg_mime(value):
    """Require one MIME type with syntactically valid token or quoted parameters."""
    if type(value) is not str or not _PKG_MIME_PATTERN.fullmatch(value):
        raise PackageError('invalid_metadata', 'Invalid OPC content type')
    if ';' in value and value.split(';', 1)[0].strip(' \t').lower() in (
            _PKG_RELS_MIME, 'application/vnd.openxmlformats-package.core-properties+xml',
            'application/vnd.openxmlformats-package.digital-signature-xmlsignature+xml',
            'application/vnd.openxmlformats-package.digital-signature-origin',
            'application/vnd.openxmlformats-package.digital-signature-certificate'):
        raise PackageError('invalid_metadata', 'Reserved OPC content types cannot have parameters')
    return value

def _pkg_mime_equal(left, right, budget):
    """Compare typed media identities without erasing case-sensitive parameter values."""
    budget.consume('work_units', len(left) + len(right))
    if left == right: return True
    if ';' not in left and ';' not in right:
        budget.consume('decoder_allocation_bytes', len(left) + len(right))
        return left.translate(_PKG_ASCII_FOLD) == right.translate(_PKG_ASCII_FOLD)
    if (';' in left) != (';' in right): return False
    def identity(value):
        """Normalize parameter names/order and quoted pairs while retaining exact decoded values."""
        budget.consume('decoder_allocation_bytes', len(value) * 4)
        _pkg_mime(value)
        separator = value.find(';')
        base = value[:separator].strip(' \t').translate(_PKG_ASCII_FOLD)
        parameters = []
        for match in _PKG_MIME_PARAMETER.finditer(value, separator):
            budget.consume('decoder_allocation_bytes', 64)
            name, selected = match[1].translate(_PKG_ASCII_FOLD), match[2]
            if selected.startswith('"'):
                selected = _re.sub(r'\\(.)', r'\1', selected[1:-1])
            parameters.append((name, selected))
        budget.consume('work_units', len(value) * (len(parameters).bit_length() + 1))
        parameters.sort()
        if any(parameters[index - 1][0] == parameters[index][0] for index in range(1, len(parameters))):
            return None
        return (base, parameters)
    selected = identity(left)
    return selected is not None and selected == identity(right)


)packagepython"
R"packagepython(def _pkg_iri_component(value, extra='', query=False):
    """Validate RFC3987 Unicode scalars and percent triplets within one URI component."""
    index = 0
    while index < len(value):
        character, scalar = value[index], ord(value[index])
        if character == '%':
            if not _re.fullmatch('[0-9A-Fa-f]{2}', value[index + 1:index + 3]):
                raise PackageError('invalid_relationship', 'Malformed relationship URI escape')
            index += 3
            continue
        ordinary = 0xa0 <= scalar <= 0xd7ff or 0xf900 <= scalar <= 0xfdcf or 0xfdf0 <= scalar <= 0xffef or 0x10000 <= scalar <= 0xdfffd and scalar & 0xffff <= 0xfffd or 0xe1000 <= scalar <= 0xefffd
        private = query and (0xe000 <= scalar <= 0xf8ff or 0xf0000 <= scalar <= 0xffffd or 0x100000 <= scalar <= 0x10fffd)
        if not (character.isascii() and (character.isalnum() or character in "-._~!$&'()*+,;=" + extra) or ordinary or private):
            raise PackageError('invalid_relationship', 'Invalid relationship IRI character')
        index += 1

def _pkg_split_uri(value):
    """Split only literal ASCII delimiters, preserving Unicode spelling and empty component presence."""
    path, has_fragment, fragment = value.partition('#')
    path, has_query, query = path.partition('?')
    colon, slash = path.find(':'), path.find('/')
    scheme, authority = None, None
    if colon >= 0 and (slash < 0 or colon < slash):
        scheme, path = path[:colon], path[colon + 1:]
    if path.startswith('//'):
        authority, separator, path = path[2:].partition('/')
        path = '/' + path if separator else ''
    return scheme, authority, path, query if has_query else None, fragment if has_fragment else None


def _pkg_uri(value, absolute=False):
    """Validate inert RFC3986/3987 references without Unicode normalization or external resolution."""
    if type(value) is not str or any(ord(character) <= 0x20 for character in value):
        raise PackageError('invalid_relationship', 'Invalid relationship URI')
    scheme, authority, path, query, fragment = _pkg_split_uri(value)
    if scheme is not None and not _re.fullmatch('[A-Za-z][A-Za-z0-9+.-]*', scheme):
        raise PackageError('invalid_relationship', 'Invalid relationship URI scheme')
    if absolute and (scheme is None or fragment is not None):
        raise PackageError('invalid_relationship', 'Relationship type requires an absolute IRI without a fragment')
    authority = authority if authority is not None else ''
    if '@' in authority:
        user, authority = authority.split('@', 1)
        _pkg_iri_component(user, ':')
    if authority.startswith('['):
        close = authority.find(']')
        if close < 0:
            raise PackageError('invalid_relationship', 'Invalid relationship URI authority')
        literal = authority[1:close]
        try:
            if literal[:1].lower() == 'v':
                if not _re.fullmatch(r'[vV][0-9A-Fa-f]+\.[A-Za-z0-9._~!$&\x27()*+,;=:-]+', literal):
                    raise ValueError('Invalid IPvFuture')
            else:
                # RFC3986 IP literals exclude scoped-address zone identifiers accepted by ipaddress.
                if '%' in literal:
                    raise ValueError('Invalid IPv6 zone identifier')
                _pkg_ip.IPv6Address(literal)
        except ValueError as error:
            raise PackageError('invalid_relationship', 'Invalid relationship IP literal') from error
        suffix = authority[close + 1:]
        if suffix and not _re.fullmatch(':[0-9]*', suffix):
            raise PackageError('invalid_relationship', 'Invalid relationship URI port')
    else:
        host, separator, port = authority.partition(':')
        _pkg_iri_component(host)
        if separator and port and not _re.fullmatch('[0-9]+', port):
            raise PackageError('invalid_relationship', 'Invalid relationship URI port')
    _pkg_iri_component(path, ':@/')
    _pkg_iri_component(query or '', ':@/?', True)
    _pkg_iri_component(fragment or '', ':@/?')
    return value

def _pkg_target(source, target):
    """Resolve RFC dot segments to a whole part while retaining authored query/fragment text in its link."""
    _pkg_uri(target)
    scheme, authority, path, _, _ = _pkg_split_uri(target)
    if scheme is not None or authority is not None:
        raise PackageError('invalid_relationship', 'Invalid internal relationship target', source, reference=target)
    target = path
    if not target:
        return _pkg_name(source)
    if target.endswith('/') or target.rsplit('/', 1)[-1] in ('.', '..'):
        raise PackageError('invalid_relationship', 'Relationship target identifies a directory', source)
    pieces = [] if target.startswith('/') else source.split('/')[1:-1]
    for segment in target.split('/'):
        if segment in ('', '.'):
            continue
        if segment == '..':
            if pieces: pieces.pop()
        else:
            pieces.append(segment)
    return _pkg_name('/' + '/'.join(pieces))

def _pkg_unique(names, name):
    """Reject case aliases and file/descendant collisions before retaining a part."""
    key = name.lower()
    if key in names:
        raise PackageError('duplicate_name', 'Duplicate OPC part identity', name)
    if any(key.startswith(old + '/') or old.startswith(key + '/') for old in names):
        raise PackageError('invalid_name', 'OPC part is an ancestor of another part', name)
    names.add(key)

class _PackageSource:
    """Borrow checked random access or own a bounded spool; caller streams are never closed."""
    def __init__(self, source, budget):
        """Address seekable input from its current position; privately spool genuine nonseekable input."""
        self.budget, self.file, self.data, self.size = budget, None, None, 0
        self.base, self.owned = 0, False
        if isinstance(source, (bytes, bytearray, memoryview)):
            view = memoryview(source)
            self.size = view.nbytes
            if self.size > budget.limits.max_archive_bytes:
                raise PackageError('resource_limit', 'Archive byte limit exceeded')
            if not view.c_contiguous:
                budget.check_buffer(self.size)
                view = memoryview(view.tobytes())
            self.data = view.cast('B')
            return
        if self._borrow(source):
            return
        self.file = _pkg_tempfile.SpooledTemporaryFile(max_size=min(1024 * 1024, budget.limits.max_buffer_bytes), mode='w+b')
        self.owned = True
        try:
            while True:
                budget.check_buffer(_PKG_CHUNK)
                data = source.read(_PKG_CHUNK)
                if not isinstance(data, bytes):
                    raise PackageError('io', 'Package source must return bytes')
                if len(data) > _PKG_CHUNK:
                    raise PackageError('io', 'Package source exceeded requested read size')
                if not data:
                    break
                self.size += len(data)
                if self.size > budget.limits.max_archive_bytes:
                    raise PackageError('resource_limit', 'Archive byte limit exceeded')
                budget.consume('io_bytes', len(data))
                self.file.write(data)
        except BaseException:
            self.close()
            raise
    def _borrow(self, source):
        """Probe positioning without reading; a failed unsupported seek must leave the original base intact."""
        seek, tell = getattr(source, 'seek', None), getattr(source, 'tell', None)
        if not callable(seek) or not callable(tell):
            return False
        seekable = getattr(source, 'seekable', None)
        try:
            if callable(seekable) and not seekable():
                return False
            base = tell()
        except _pkg_io.UnsupportedOperation:
            return False
        if type(base) is not int or base < 0:
            raise PackageError('io', 'Invalid package source position')
        try:
            seek(0, _pkg_os.SEEK_END)
        except _pkg_io.UnsupportedOperation:
            # Fall back only when the failed capability probe demonstrably consumed no input.
            restored = tell()
            if type(restored) is not int or restored != base:
                raise PackageError('io', 'Unsupported package seek changed the source position')
            return False
        end = tell()
        if type(end) is not int or end < base:
            raise PackageError('io', 'Invalid package source extent')
        size = end - base
        if size > self.budget.limits.max_archive_bytes:
            raise PackageError('resource_limit', 'Archive byte limit exceeded')
        self.file, self.base, self.size = source, base, size
        return True
    def read(self, offset, count):
        """Admit complete ranges before I/O, accumulate legal short reads, and leave borrowed input at EOF."""
        if offset < 0 or count < 0 or offset > self.size or count > self.size - offset:
            raise PackageError('invalid_archive', 'ZIP offset or length is outside the archive')
        self.budget.consume('io_bytes', count)
        self.budget.check_buffer(count)
        if self.data is not None:
            return bytes(self.data[offset:offset + count])
        self.file.seek(self.base + offset)
        chunks, remaining = [], count
        while remaining:
            self.budget.check()
            requested = min(_PKG_CHUNK, remaining)
            data = self.file.read(requested)
            self.budget.check()
            if not isinstance(data, bytes) or not data or len(data) > requested:
                raise PackageError('io', 'Invalid or truncated package source read')
            chunks.append(data)
            remaining -= len(data)
        if not self.owned:
            self.file.seek(self.base + self.size)
            self.budget.check()
        return b''.join(chunks)
    def close(self):
        """Release only the private spool owned by this operation."""
        if self.file is not None and self.owned:
            self.file.close()
        self.file = None

def _pkg_zip64(extra, required):
    """Read ordered ZIP64 values only when their 32-bit sentinel requires them."""
    offset, selected = 0, None
    while offset < len(extra):
        if len(extra) - offset < 4:
            raise PackageError('invalid_archive', 'Truncated ZIP extra field')
        kind, size = _struct.unpack_from('<HH', extra, offset)
        offset += 4
        if size > len(extra) - offset:
            raise PackageError('invalid_archive', 'Invalid ZIP extra length')
        if kind == 1:
            if selected is not None:
                raise PackageError('invalid_archive', 'Duplicate ZIP64 extra field')
            selected = extra[offset:offset + size]
        offset += size
    if not any(required):
        return ()
    if selected is None or len(selected) < 8 * sum(required):
        raise PackageError('invalid_archive', 'Missing ZIP64 extra values')
    return iter(_struct.unpack_from('<' + 'Q' * sum(required), selected))

)packagepython"
R"packagepython(class _PackageZip:
    """Strict ZIP32/ZIP64 directory and descriptor validation precedes payload decompression."""
    def __init__(self, source, budget):
        """Validate independent ZIP producer offsets, entry identities and nonoverlapping local records."""
        self.source, self.budget, self.entries, self.physical_entries = source, budget, {}, []
        tail_offset = max(0, source.size - 65557)
        tail = source.read(tail_offset, source.size - tail_offset)
        end = len(tail)
        while True:
            end = tail.rfind(b'PK\x05\x06', 0, end)
            if end < 0:
                raise PackageError('invalid_archive', 'ZIP end directory is missing')
            if len(tail) - end >= 22 and end + 22 + _struct.unpack_from('<H', tail, end + 20)[0] == len(tail):
                break
        _, disk, central_disk, disk_count, count, size, offset, comment = _struct.unpack_from('<4s4H2IH', tail, end)
        end += tail_offset
        if disk or central_disk or disk_count != count or end + 22 + comment != source.size:
            raise PackageError('invalid_archive', 'Multidisk or trailing ZIP data is unsupported')
        boundary = end
        if end >= 20:
            locator = source.read(end - 20, 20)
            if locator.startswith(b'PK\x06\x07'):
                signature, disk, location, disks = _struct.unpack('<4sIQI', locator)
                if disk or disks != 1 or location > end - 20 or end - 20 - location < 56:
                    raise PackageError('invalid_archive', 'Invalid ZIP64 locator')
                record = source.read(location, 56)
                fields = _struct.unpack('<4sQ2H2I4Q', record)
                if fields[0] != b'PK\x06\x06' or fields[1] < 44 or fields[4] or fields[5] or fields[6] != fields[7] or fields[1] != end - 20 - location - 12:
                    raise PackageError('invalid_archive', 'Invalid ZIP64 directory')
                if fields[3] > 45:
                    raise PackageError('unsupported_feature', 'Unsupported ZIP64 extraction version')
                if (count != 65535 and count != fields[7] or size != _PKG_U32 and size != fields[8] or
                        offset != _PKG_U32 and offset != fields[9]):
                    raise PackageError('invalid_archive', 'ZIP32 and ZIP64 directory records disagree')
                count, size, offset, boundary = fields[7], fields[8], fields[9], location
        # Without a locator, ordinary fields retain their full unsigned range, including the maximum.
        if offset > boundary or size != boundary - offset:
            raise PackageError('invalid_archive', 'ZIP central directory boundary mismatch')
        budget.consume('entries', count)
        budget.consume('metadata_bytes', size)
        central = source.read(offset, size)
        position, intervals, names = 0, [], set()
        for unused in range(count):
            budget.consume('work_units', 1)
            if len(central) - position < 46:
                raise PackageError('invalid_archive', 'Truncated ZIP central entry')
            fields = _struct.unpack_from('<4s6H3I5H2I', central, position)
            if fields[0] != b'PK\x01\x02':
                raise PackageError('invalid_archive', 'Invalid ZIP central signature')
            flags, method, crc, compressed, expanded = fields[3], fields[4], fields[7], fields[8], fields[9]
            name_size, extra_size, comment_size, disk = fields[10:14]
            local = fields[16]
            length = 46 + name_size + extra_size + comment_size
            if length > len(central) - position:
                raise PackageError('invalid_archive', 'Truncated ZIP central entry fields')
            if disk or flags & ~(0x800 | 8 | 6) or method not in (0, 8):
                raise PackageError('unsupported_feature', 'Unsupported ZIP entry')
            name_bytes = central[position + 46:position + 46 + name_size]
            try:
                name = name_bytes.decode('utf-8' if flags & 0x800 else 'cp437', 'strict')
            except UnicodeError as error:
                raise PackageError('invalid_name', 'ZIP filename has invalid text encoding') from error
            part_name = _pkg_zip_item_name(name)
            if part_name in names:
                raise PackageError('duplicate_name', 'Duplicate physical ZIP item', part_name)
            names.add(part_name)
            extra = central[position + 46 + name_size:position + 46 + name_size + extra_size]
            wide = expanded == _PKG_U32 or compressed == _PKG_U32
            values = _pkg_zip64(extra, (expanded == _PKG_U32, compressed == _PKG_U32, local == _PKG_U32))
            if expanded == _PKG_U32: expanded = next(values)
            if compressed == _PKG_U32: compressed = next(values)
            if local == _PKG_U32: local = next(values)
            if expanded > budget.limits.max_part_bytes:
                raise PackageError('resource_limit', 'ZIP expanded part limit exceeded', part_name)
            if local > offset or offset - local < 30:
                raise PackageError('invalid_archive', 'ZIP local header overlaps directory', part_name)
            header = source.read(local, 30)
            values = _struct.unpack('<4s5H3I2H', header)
            if values[0] != b'PK\x03\x04' or values[2] != flags or values[3] != method:
                raise PackageError('invalid_archive', 'ZIP local and central headers disagree', part_name)
            local_name, local_extra = values[9], values[10]
            data = local + 30 + local_name + local_extra
            if data > offset or compressed > offset - data:
                raise PackageError('invalid_archive', 'ZIP payload overlaps directory', part_name)
            if source.read(local + 30, local_name) != name_bytes:
                raise PackageError('invalid_archive', 'ZIP local filename mismatch', part_name)
            extra = source.read(local + 30 + local_name, local_extra)
            local_sizes = _pkg_zip64(extra, (values[8] == _PKG_U32, values[7] == _PKG_U32))
            local_expanded = next(local_sizes) if values[8] == _PKG_U32 else values[8]
            local_compressed = next(local_sizes) if values[7] == _PKG_U32 else values[7]
            finish = data + compressed
            if flags & 8:
                wide = wide or values[7] == _PKG_U32 or values[8] == _PKG_U32
                descriptor_size = 20 if wide else 12
                descriptor = source.read(finish, descriptor_size)
                actual = _struct.unpack('<IQQ' if wide else '<III', descriptor)
                signed = False
                if actual != (crc, compressed, expanded) and descriptor.startswith(b'PK\x07\x08'):
                    descriptor = source.read(finish + 4, descriptor_size)
                    actual = _struct.unpack('<IQQ' if wide else '<III', descriptor)
                    signed = True
                if actual != (crc, compressed, expanded) or values[6] not in (0, crc) or local_expanded not in (0, expanded) or local_compressed not in (0, compressed):
                    raise PackageError('integrity', 'ZIP descriptor mismatch', part_name)
                finish += descriptor_size + (4 if signed else 0)
            elif (values[6], local_compressed, local_expanded) != (crc, compressed, expanded):
                raise PackageError('integrity', 'ZIP local sizes or CRC mismatch', part_name)
            if local >= offset or finish > offset:
                raise PackageError('invalid_archive', 'ZIP entry overlaps directory', part_name)
            intervals.append((local, finish))
            budget.consume('decoder_allocation_bytes', len(name_bytes) + 128, part_name)
            entry = (part_name, method, crc, compressed, expanded, data, fields[1] >> 8, fields[15])
            budget.consume('decoder_allocation_bytes', _struct.calcsize('P'), part_name)
            self.physical_entries.append(entry)
            self.entries.setdefault(part_name.lower(), entry)
            position += length
        if position != len(central):
            raise PackageError('invalid_archive', 'Unexpected ZIP directory records')
        intervals.sort()
        if any(left[1] > right[0] for left, right in zip(intervals, intervals[1:])):
            raise PackageError('invalid_archive', 'ZIP local records overlap')
    def read(self, name):
        """Inflate exactly one entry and check framing, expanded size and CRC before returning bytes."""
        return self.read_entry(self.entries[name.lower()])
    def read_entry(self, entry):
        """Validate one exact physical item, including excluded names that share a folded spelling."""
        part, method, crc = entry[0], entry[1], entry[2]
        compressed, expanded, offset = entry[3], entry[4], entry[5]
        self.budget.check_buffer(expanded * 2 + _PKG_CHUNK, part)
        self.budget.consume('decoder_allocation_bytes', expanded * 2, part)
        output, checksum = bytearray(), 0
        decoder = _pkg_zlib.decompressobj(-15) if method == 8 else None
        for begin in range(0, compressed, _PKG_CHUNK):
            data = self.source.read(offset + begin, min(_PKG_CHUNK, compressed - begin))
            self.budget.consume('work_units', len(data), part)
            while data:
                try:
                    block = decoder.decompress(data, min(_PKG_CHUNK, expanded - len(output) + 1)) if decoder else data
                except _pkg_zlib.error as error:
                    raise PackageError('integrity', 'Invalid ZIP DEFLATE stream', part) from error
                if len(block) > expanded - len(output):
                    raise PackageError('integrity', 'ZIP expanded size mismatch', part)
                self.budget.consume('expanded_bytes', len(block), part)
                checksum = _pkg_zlib.crc32(block, checksum)
                output.extend(block)
                data = decoder.unconsumed_tail if decoder else b''
                if decoder and decoder.unused_data:
                    raise PackageError('integrity', 'Trailing ZIP DEFLATE data', part)
        if len(output) != expanded or checksum != crc or decoder and not decoder.eof:
            raise PackageError('integrity', 'ZIP CRC, size or DEFLATE framing mismatch', part)
        return bytes(output)

def _pkg_zip_write(parts, budget):
    """Emit canonical UTF-8 stored/DEFLATE ZIP entries with signed ZIP32/ZIP64 descriptors."""
    output, central = bytearray(), []
    retained_bytes = sum(len(part[1]) for part in parts)
    def append(data):
        """Bound output growth before every ZIP header, compressed chunk or directory append."""
        if len(data) > budget.limits.max_archive_bytes - len(output):
            raise PackageError('resource_limit', 'Archive output byte limit exceeded')
        budget.check_buffer(retained_bytes + (len(output) + len(data)) * 2 + _PKG_CHUNK)
        budget.consume('io_bytes', len(data))
        output.extend(data)
    for name, data, compression in sorted(parts, key=lambda item: item[0]):
        budget.consume('entries', 1, name)
        budget.check_buffer(len(data) + _PKG_CHUNK, name)
        if len(data) > budget.limits.max_part_bytes:
            raise PackageError('resource_limit', 'Package part byte limit exceeded', name)
        encoded = name[1:].encode('utf-8')
        wide = budget.limits.force_zip64 or len(data) >= _PKG_U32
        method, flags, start = 8 if compression == 'deflate' else 0, 0x808, len(output)
        if compression not in ('stored', 'deflate'):
            raise PackageError('unsupported_feature', 'Unsupported ZIP compression', name)
        extra = _struct.pack('<HHQQ', 1, 16, len(data), 0) if wide else b''
        append(_struct.pack('<4s5H3I2H', b'PK\x03\x04', 45 if wide else 20, flags, method, 0, 33,
                            0, _PKG_U32 if wide else 0, _PKG_U32 if wide else 0, len(encoded), len(extra)) + encoded + extra)
        begin, crc = len(output), 0
        encoder = _pkg_zlib.compressobj(wbits=-15) if method else None
        for position in range(0, len(data), _PKG_CHUNK):
            block = memoryview(data)[position:position + _PKG_CHUNK]
            budget.consume('expanded_bytes', len(block), name)
            budget.consume('work_units', len(block), name)
            crc = _pkg_zlib.crc32(block, crc)
            append(encoder.compress(block) if encoder else block)
        if encoder:
            append(encoder.flush())
        compressed = len(output) - begin
        append(_struct.pack('<4sIQQ' if wide else '<4sIII', b'PK\x07\x08', crc, compressed, len(data)))
        extra = _struct.pack('<HHQQQ', 1, 24, len(data), compressed, start) if wide else b''
        record = _struct.pack('<4s6H3I5H2I', b'PK\x01\x02', 45 if wide else 20, 45 if wide else 20,
                              flags, method, 0, 33, crc, _PKG_U32 if wide else compressed,
                              _PKG_U32 if wide else len(data), len(encoded), len(extra), 0, 0, 0,
                              0o100600 << 16, _PKG_U32 if wide else start) + encoded + extra
        budget.consume('metadata_bytes', len(record), name)
        central.append(record)
    start = len(output)
    for record in central: append(record)
    size = len(output) - start
    wide = budget.limits.force_zip64 or len(central) >= 65535 or max(size, start) >= _PKG_U32
    if wide:
        location = len(output)
        append(_struct.pack('<4sQ2H2I4Q', b'PK\x06\x06', 44, 45, 45, 0, 0, len(central), len(central), size, start))
        append(_struct.pack('<4sIQI', b'PK\x06\x07', 0, location, 1))
    append(_struct.pack('<4s4H2IH', b'PK\x05\x06', 0, 0, 65535 if wide else len(central),
                        65535 if wide else len(central), _PKG_U32 if wide else size, _PKG_U32 if wide else start, 0))
    return bytes(output)
def _pkg_metadata_node(name, attrs, children, budget):
    """Build bounded standard infrastructure through the same secure XML writer as typed parts."""
    xml_budget = _XmlBudget(budget.xml_limits(), budget)
    node = xml_budget.node(name, 1)
    for key, value in attrs.items(): _xml_attribute(node, key, value, xml_budget)
    for child_name, attributes in children:
        child = xml_budget.node(child_name, 2)
        for key, value in attributes.items(): _xml_attribute(child, key, value, xml_budget)
        node[2].append(child)
    data = _xml_write(node, xml_budget.limits, budget)
    budget.consume('metadata_bytes', len(data))
    return data

)packagepython"
R"packagepython(def _pkg_metadata_read(data, root, budget, allow_text=False):
    """Reject DTDs, mixed content and unexpected root metadata before retaining OPC declarations."""
    budget.consume('metadata_bytes', len(data))
    original_size = len(data)
    if data[:2] in (b'\xff\xfe', b'\xfe\xff'):
        budget.consume('work_units', len(data))
        budget.consume('decoder_allocation_bytes', len(data) * 4)
        budget.check_buffer(len(data) * 3)
        try:
            encoding = 'utf-16-le' if data[:2] == b'\xff\xfe' else 'utf-16-be'
            text = data[2:].decode(encoding, errors='strict')
            declaration = _re.match(r'\A<\?xml(?=[ \t\r\n]).*?\?>', text, _re.DOTALL)
            if declaration:
                pattern = r'''encoding[ \t\r\n]*=[ \t\r\n]*(['"])([^'"]*)\1'''
                if any(match[1].lower() != 'utf-16' for match in _re.findall(pattern, declaration[0])):
                    raise ValueError('BOM-marked UTF16 metadata requires a matching UTF-16 declaration')
                replacement = _re.sub(pattern, 'encoding="UTF-8"', declaration[0])
                text = replacement + text[declaration.end():]
            data = text.encode('utf-8', errors='strict')
        except (UnicodeError, ValueError) as error:
            raise PackageError('invalid_metadata', 'Invalid UTF16 OPC metadata') from error
    try:
        node = _xml_parse(data, budget.xml_limits(), budget, original_size)
        if allow_text:
            node = _pkg_relationships_compatibility(node, budget)
    except ValueError as error:
        if hasattr(error, 'code'): raise
        raise PackageError('invalid_metadata', 'Invalid OPC infrastructure XML') from error
    if node[0] != root or node[1] or node[3].strip(' \t\r\n'):
        raise PackageError('invalid_metadata', 'Invalid OPC infrastructure XML root')
    for child in node[2]:
        if child[2] or not allow_text and child[3].strip(' \t\r\n'):
            raise PackageError('invalid_metadata', 'Unexpected OPC infrastructure content')
    return node[2]

def _pkg_relationships(links, parts, budget):
    """Validate source-local IDs, source membership and every internal target before use or publication."""
    seen, result = set(), []
    for link in links:
        try:
            budget.consume('relationships', 1, link.source)
            budget.consume('references', 1, link.source)
        except PackageError as error:
            if not getattr(error, '_budget_failure', False): raise
            if not error.part: error.part = link.source
            if not error.relationship_id: error.relationship_id = link.id
            if not error.reference: error.reference = link.target
            if not error.field_path: error.field_path = 'relationships'
            raise
        if not _xml_ncname(link.id):
            raise PackageError('invalid_relationship', 'Invalid relationship ID', link.source)
        source = '/' if link.source == '/' else _pkg_name(link.source)
        if source != '/' and source.lower() not in parts:
            raise PackageError('missing_part', 'Relationship source is missing', source)
        key = (source.lower(), link.id)
        if key in seen:
            raise PackageError('invalid_relationship', 'Duplicate relationship ID', source, link.id)
        seen.add(key)
        _pkg_uri(link.type, True)
        if link.mode == 'internal':
            target = _pkg_target(source, link.target)
            if target.lower() not in parts:
                raise PackageError('missing_part', 'Relationship target is missing', source, link.id, target)
            target = link.target
        elif link.mode == 'external':
            target = _pkg_uri(link.target)
        else:
            raise PackageError('invalid_relationship', 'Invalid relationship target mode', source, link.id)
        result.append(PackageRelationship(source, link.id, link.type, target, link.mode))
    return result

def _pkg_opc_read(archive, budget):
    """Resolve content types and relationship sets without opening any external target."""
    manifest = '/[content_types].xml'
    found_manifest = False
    for entry in archive.physical_entries:
        budget.consume('work_units', 1)
        if len(entry[0]) == len(manifest) and entry[0].lower() == manifest:
            if not _pkg_physical_file(entry):
                raise PackageError('invalid_metadata', 'OPC content type manifest is not a physical file')
            if found_manifest: raise PackageError('duplicate_name', 'Duplicate OPC content type manifest')
            found_manifest = True
    if not found_manifest:
        raise PackageError('invalid_metadata', 'OPC content type manifest is missing')
    defaults, overrides = {}, {}
    for child in _pkg_metadata_read(archive.read(manifest), '{' + _PKG_CT + '}Types', budget):
        attrs = child[1]
        if child[0] == '{' + _PKG_CT + '}Default' and set(attrs) == {'Extension', 'ContentType'}:
            key = attrs['Extension'].lower()
            if not _re.fullmatch(r"(?:[A-Za-z0-9_~!$&'()*+,:=@-]|%[0-9A-Fa-f]{2})+", key) or key in defaults:
                raise PackageError('invalid_metadata', 'Invalid or duplicate OPC default extension')
            defaults[key] = _pkg_mime(attrs['ContentType'])
        elif child[0] == '{' + _PKG_CT + '}Override' and set(attrs) == {'PartName', 'ContentType'}:
            key = _pkg_name(_xml_normalize(attrs['PartName'], 'collapse'), False).lower()
            if key in overrides:
                raise PackageError('invalid_metadata', 'Duplicate OPC content type override')
            overrides[key] = _pkg_mime(attrs['ContentType'])
        else:
            raise PackageError('invalid_metadata', 'Unknown OPC content type declaration')
    if not defaults and not overrides:
        raise PackageError('invalid_metadata', 'Empty OPC content type manifest')
    parts, relationship_parts, mapped, identities = {}, [], set(), set()
    for entry in archive.physical_entries:
        key = entry[0].lower()
        if key == manifest:
            continue
        name = entry[0]
        if not _pkg_physical_file(entry) or not _pkg_maps_to_part(name, budget):
            archive.read_entry(entry)
            continue
        _pkg_unique(identities, name)
        mapped.add(key)
        content_type = overrides.get(key, defaults.get(name.rsplit('.', 1)[-1].lower(), ''))
        if not content_type:
            raise PackageError('invalid_metadata', 'OPC part has no content type', name)
        source = _pkg_relationship_source(name)
        if source is not None:
            if content_type.lower() != _PKG_RELS_MIME:
                raise PackageError('invalid_metadata', 'Relationship part has the wrong content type', name)
            relationship_parts.append((name, source))
        else:
            _pkg_name(name)
            if content_type.lower() == _PKG_RELS_MIME:
                raise PackageError('invalid_metadata', 'Relationship MIME type requires relationship infrastructure', name)
            parts[key] = BinaryPart(None, name, content_type)
    if any(key not in mapped for key in overrides):
        raise PackageError('invalid_metadata', 'Content type override names a missing or reserved part')
    links = []
    for name, source in relationship_parts:
        if source != '/' and source.lower() not in parts:
            raise PackageError('missing_part', 'Relationship-part source is not a mapped OPC part', name)
        for child in _pkg_metadata_read(archive.read(name), '{' + _PKG_RELS + '}Relationships', budget, True):
            attrs = child[1]
            if child[0] != '{' + _PKG_RELS + '}Relationship' or not {'Id', 'Type', 'Target'} <= set(attrs) or set(attrs) - {'Id', 'Type', 'Target', 'TargetMode'}:
                raise PackageError('invalid_metadata', 'Invalid OPC relationship declaration', name)
            mode = _xml_normalize(attrs.get('TargetMode', 'Internal'), 'collapse')
            if mode not in ('Internal', 'External'):
                raise PackageError('invalid_relationship', 'Invalid TargetMode', name)
            links.append(PackageRelationship(source, _xml_normalize(attrs['Id'], 'collapse'),
                _xml_normalize(attrs['Type'], 'collapse'), _xml_normalize(attrs['Target'], 'collapse'), mode.lower()))
    return parts, _pkg_relationships(links, parts, budget)

def _pkg_opc_write(parts, links, budget):
    """Own standard manifests and relationship assembly; callers supply only actual payload parts."""
    identities, lookup = set(), {}
    for part in parts:
        _pkg_name(part.name)
        _pkg_mime(part.content_type)
        if part.content_type.lower() == _PKG_RELS_MIME:
            raise PackageError('invalid_metadata', 'Relationship MIME type requires relationship infrastructure', part.name)
        _pkg_unique(identities, part.name)
        lookup[part.name.lower()] = part
    links = _pkg_relationships(links, lookup, budget)
    entries = [(part.name, part.data, part.compression) for part in parts]
    children = [('{'+ _PKG_CT + '}Override', {'PartName': part.name, 'ContentType': part.content_type})
                for part in sorted(parts, key=lambda part: part.name)]
    if not children and not links:
        children.append(('{' + _PKG_CT + '}Default', {'Extension': 'rels', 'ContentType': _PKG_RELS_MIME}))
    groups = {}
    for link in links: groups.setdefault(link.source, []).append(link)
    for source, group in sorted(groups.items()):
        name = '/_rels/.rels' if source == '/' else source.rsplit('/', 1)[0] + '/_rels/' + source.rsplit('/', 1)[1] + '.rels'
        _pkg_unique(identities, name)
        children.append(('{' + _PKG_CT + '}Override', {'PartName': name, 'ContentType': _PKG_RELS_MIME}))
        declarations = []
        for link in sorted(group, key=lambda link: link.id):
            attrs = {'Id': link.id, 'Type': link.type, 'Target': link.target}
            if link.mode == 'external': attrs['TargetMode'] = 'External'
            declarations.append(('{' + _PKG_RELS + '}Relationship', attrs))
        entries.append((name, _pkg_metadata_node('{' + _PKG_RELS + '}Relationships', {}, declarations, budget), 'deflate'))
    entries.append(('/[Content_Types].xml', _pkg_metadata_node('{' + _PKG_CT + '}Types', {}, children, budget), 'deflate'))
    return _pkg_zip_write(entries, budget)

def _pkg_occurs(count, role):
    """Enforce aggregate role cardinalities before writing and before publishing decoded values."""
    if count < role['minimum'] or role['maximum'] is not None and count > role['maximum']:
        raise PackageError('semantic', 'Package role occurrence constraint failed', field_path=role['field'])

def _pkg_relocated_links(extensions, budget):
    """Resolve old link scopes before applying simultaneous fixed-role relocations; retain inert external URIs."""
    destinations = {}
    for original, canonical in extensions.relocations:
        budget.consume('work_units', len(original) + len(canonical))
        _pkg_name(original)
        _pkg_name(canonical)
        key = original.lower()
        budget.consume('metadata_bytes', len(key) + len(canonical))
        if key in destinations and destinations[key] != canonical:
            raise PackageError('invalid_relationship', 'Conflicting typed part relocations', original)
        destinations[key] = canonical
    for link in extensions.relationships:
        budget.check()
        if not destinations:
            yield link
            continue
        budget.consume('work_units', len(link.source) + len(link.target))
        source = destinations.get(link.source.lower(), link.source)
        target = link.target
        if link.mode == 'internal':
            original_target = _pkg_target(link.source, link.target)
            destination = destinations.get(original_target.lower(), original_target)
            if source != link.source or destination != original_target:
                selector = min((index for index in (target.find('?'), target.find('#')) if index >= 0), default=len(target))
                target = destination + target[selector:]
        yield PackageRelationship(source, link.id, link.type, target, link.mode)

def _pkg_profile_encode(value, roles, budget):
    """Compose declarative typed and binary roles; neither standard metadata nor runtime side channels are application-owned."""
    sources, parts, links = {}, [], []
    for role in roles:
        supplied = getattr(value, role['field'])
        items = (() if supplied is None else (supplied,)) if role['singleton'] else supplied
        _pkg_occurs(len(items), role)
        discovered = []
        for index, item in enumerate(items):
            budget.check()
            name = role['name'] if role['name'] is not None else item.name
            content_type = role['mime'] if role['mime'] is not None else item.content_type
            parent = 0 if role['singleton'] else item.source_index
            parents = ('/',) if role['source'] == '/' else sources[role['source']]
            if type(parent) is not int or parent < 0 or parent >= len(parents):
                raise PackageError('invalid_relationship', 'Package source occurrence is missing', name)
            _pkg_name(name)
            _pkg_mime(content_type)
            relationship_id = role['id'] or 'srl_' + role['role'] + '_' + str(index)
            if role['model'] is not None:
                model = item if role['singleton'] else item.value
                if not isinstance(model, role['model']):
                    raise PackageError('codec', 'Package typed resource is unbound or has the wrong type', name)
                try:
                    data = model.encode_xml(limits=budget.xml_limits(), context=_PackagePartContext(budget, name, relationship_id))
                except PackageError:
                    raise
                except (ValueError, OverflowError) as error:
                    raise PackageError('codec', 'Package model failed generated XML validation', name) from error
                budget.consume('decoder_allocation_bytes', len(data), name)
            else:
                data = item.data
                if not isinstance(data, (bytes, bytearray, memoryview)):
                    raise PackageError('codec', 'Package binary resource requires bytes', name)
            parts.append(BinaryPart(data, name, content_type, parent, role['compression']))
            links.append(PackageRelationship(parents[parent], relationship_id, role['relationship'], name))
            discovered.append(name)
        sources[role['field']] = discovered
    extensions = value.extensions or PreservedExtensions()
    parts.extend(extensions.parts)
    links.extend(_pkg_relocated_links(extensions, budget))
    return _pkg_opc_write(parts, links, budget)

)packagepython"
R"packagepython(def _pkg_profile_load(source, roles, value_type, budget, policy=None, validate=None):
    """Decode an unpublished aggregate, validate every entry, then return its complete result atomically."""
    policy = policy or ExtensionPolicy()
    if policy.parts not in ('reject', 'skip', 'preserve') or policy.relationships not in ('reject', 'skip', 'preserve'):
        raise PackageError('invalid_state', 'Invalid package extension policy')
    owned = _PackageSource(source, budget)
    try:
        archive = _PackageZip(owned, budget)
        parts, links = _pkg_opc_read(archive, budget)
        candidate, sources, decoded, selected = value_type(), {}, set(), set()
        relocations = []
        for role in roles:
            values, discovered = [], []
            parents = ('/',) if role['source'] == '/' else sources[role['source']]
            for parent_index, parent in enumerate(parents):
                for link_index, link in enumerate(links):
                    if link.source.lower() != parent.lower() or link.type != role['relationship']:
                        continue
                    if link.mode != 'internal':
                        raise PackageError('invalid_relationship', 'Typed package role requires an internal target', parent, link.id)
                    target = _pkg_target(link.source, link.target).lower()
                    metadata = parts[target]
                    if role['mime'] is not None and not _pkg_mime_equal(metadata.content_type, role['mime'], budget):
                        raise PackageError('codec', 'Package role content type mismatch', metadata.name, link.id)
                    if target in decoded:
                        raise PackageError('invalid_relationship', 'A package part is claimed by multiple role occurrences', metadata.name, link.id)
                    selected.add(link_index)
                    decoded.add(target)
                    data = archive.read(metadata.name)
                    try:
                        if role['model'] is not None:
                            model = role['model'].decode_xml(data, limits=budget.xml_limits(), context=_PackagePartContext(budget, metadata.name, link.id))
                            values.append(model if role['singleton'] else ModelPart(model, metadata.name, metadata.content_type, parent_index))
                        else:
                            values.append(BinaryPart(data, metadata.name, metadata.content_type, parent_index,
                                                     'deflate' if archive.entries[metadata.name.lower()][1] == 8 else 'stored'))
                    except PackageError:
                        raise
                    except (ValueError, OverflowError, _xml_expat.ExpatError) as error:
                        raise PackageError('codec', 'Package XML part failed generated validation', metadata.name, link.id) from error
                    discovered.append(metadata.name)
            _pkg_occurs(len(values), role)
            setattr(candidate, role['field'], (values[0] if values else None) if role['singleton'] else values)
            sources[role['field']] = discovered
            if role['singleton'] and discovered:
                budget.consume('metadata_bytes', len(discovered[0]) + len(role['name']))
                relocations.append((discovered[0], role['name']))
        if validate is not None:
            budget.check()
            validate(candidate, budget)
        opaque, preserved_links = [], []
        for key, metadata in parts.items():
            if key in decoded: continue
            required = policy.is_required_part is not None and policy.is_required_part(metadata)
            if required or policy.parts == 'reject':
                raise PackageError('unsupported_feature', 'Unknown required or rejected package part', metadata.name)
            data = archive.read(metadata.name)
            if policy.parts == 'preserve':
                budget.consume('preserved_bytes', len(data), metadata.name)
                opaque.append(BinaryPart(data, metadata.name, metadata.content_type, 0,
                                         'deflate' if archive.entries[key][1] == 8 else 'stored'))
        for index, link in enumerate(links):
            if index in selected: continue
            required = policy.is_required_relationship is not None and policy.is_required_relationship(link)
            if required or policy.relationships == 'reject':
                raise PackageError('unsupported_feature', 'Unknown required or rejected package relationship', link.source, link.id)
            if policy.relationships == 'preserve':
                budget.consume('preserved_bytes', sum(len(value.encode('utf-8')) for value in (link.source, link.id, link.type, link.target)))
                preserved_links.append(link)
        budget.check()
        return PackageResult(candidate, PreservedExtensions(tuple(opaque), tuple(preserved_links), tuple(relocations)))
    finally:
        owned.close()

def _pkg_write_stream(destination, data, budget):
    """Write a fully prepared archive; an underlying sink failure can retain a written prefix."""
    for offset in range(0, len(data), _PKG_CHUNK):
        block = memoryview(data)[offset:offset + _PKG_CHUNK]
        budget.consume('io_bytes', len(block))
        if destination.write(block) != len(block):
            raise PackageError('io', 'Package sink performed a short write')
        budget.check()

def _pkg_save(path, data, budget, fault=None):
    """Sync a private sibling file before atomic replacement; report post-publication uncertainty honestly."""
    path = _pkg_os.path.abspath(_pkg_os.fspath(path))
    descriptor, temporary = _pkg_tempfile.mkstemp(prefix='.serializer-opc-', dir=_pkg_os.path.dirname(path))
    published = False
    try:
        stream = _pkg_os.fdopen(descriptor, 'wb')
        # fdopen owns the descriptor only after it has returned successfully.
        descriptor = None
        with stream:
            _pkg_write_stream(stream, data, budget)
            stream.flush()
            _pkg_os.fsync(stream.fileno())
        budget.check()
        if fault is not None: fault('before_publish')
        _pkg_os.replace(temporary, path)
        published = True
        if fault is not None: fault('after_publish')
        if _pkg_os.name != 'nt':
            directory = _pkg_os.open(_pkg_os.path.dirname(path), _pkg_os.O_RDONLY | getattr(_pkg_os, 'O_DIRECTORY', 0))
            try: _pkg_os.fsync(directory)
            finally: _pkg_os.close(directory)
    except BaseException as error:
        if published:
            raise PackageError('publication_uncertain', 'Package was published but durability confirmation failed') from error
        raise
    finally:
        if descriptor is not None:
            _pkg_os.close(descriptor)
        if not published:
            try: _pkg_os.unlink(temporary)
            except FileNotFoundError: pass
)packagepython"
