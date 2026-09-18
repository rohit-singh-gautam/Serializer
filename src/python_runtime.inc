R"serializer(
from __future__ import annotations
from enum import IntEnum as _IntEnum
import json as _json
import math as _math
import re as _re
import struct as _struct

class Protocol(_IntEnum):
    """Select the existing native wire protocol."""
    JSON = 0
    BINARY_NONE = 1
    BINARY_INTEGER = 2
    BINARY_STRING = 3

class Limits:
    """Per-message resource policy; collection entries are cumulative."""
    __slots__ = ('max_bytes', 'max_string_bytes', 'max_elements', 'max_depth')
    def __init__(self, max_bytes=64*1024*1024, max_string_bytes=16*1024*1024, max_elements=1000000, max_depth=64):
        """Validate limits before reading untrusted input."""
        for value in (max_bytes, max_string_bytes, max_elements, max_depth):
            if type(value) is not int or value < 0:
                raise ValueError('Invalid decode limit')
        self.max_bytes, self.max_string_bytes = max_bytes, max_string_bytes
        self.max_elements, self.max_depth = max_elements, max_depth

_INTEGER = _re.compile(r'-?(0|[1-9][0-9]*)\Z')
_NUMBER = _re.compile(r'-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?\Z')
_FLOAT = _struct.Struct('<f')
_DOUBLE = _struct.Struct('<d')

def _protocol(value):
    """Reject invalid selection even for empty positional objects."""
    if not isinstance(value, (int, Protocol)) or isinstance(value, bool) or value not in (0, 1, 2, 3):
        raise ValueError('Unknown protocol')
    return value

def _bounds(value, width, unsigned):
    """Reject coercions and integer values outside their declared wire width."""
    bits = width * 8
    if type(value) is not int or not (0 if unsigned else -(1 << (bits-1))) <= value <= ((1 << (bits if unsigned else bits-1))-1):
        raise ValueError('Integer out of range')
    return value

class _Writer:
    """Own a growing output buffer and write fields directly."""
    __slots__ = ('protocol', 'data', 'depth')
    def __init__(self, protocol):
        """Create one isolated encoding session."""
        self.protocol, self.data, self.depth = _protocol(protocol), bytearray(), 0
    def compact(self, value):
        """Write Serializer's two-bit length tag."""
        if not 0 <= value <= 0x3fffffff:
            raise ValueError('Compact integer out of range')
        extra = 0 if value <= 63 else 1 if value <= 16383 else 2 if value <= 4194303 else 3
        self.data.append((extra << 6) | (value >> (extra*8)))
        self.data.extend((value & ((1 << (extra*8))-1)).to_bytes(extra, 'big'))
    def integer(self, value, width, unsigned):
        """Range-check integers and preserve their full width."""
        _bounds(value, width, unsigned)
        self.data.extend(str(value).encode('ascii') if self.protocol == 0 else value.to_bytes(width, 'little', signed=not unsigned))
    def floating(self, value, single):
        """Round binary32 once and reject nonfinite JSON values."""
        if type(value) not in (int, float):
            raise ValueError('Expected number')
        codec = _FLOAT if single else _DOUBLE
        packed = codec.pack(value)
        value = codec.unpack(packed)[0]
        if self.protocol == 0:
            if not _math.isfinite(value):
                raise ValueError('Nonfinite JSON number')
            self.data.extend(repr(value).encode('ascii'))
        else:
            self.data.extend(packed)
    def boolean(self, value):
        """Write only canonical booleans."""
        if type(value) is not bool:
            raise ValueError('Expected boolean')
        self.data.extend((b'true' if value else b'false') if self.protocol == 0 else bytes((value,)))
    def text(self, value):
        """Encode strict Unicode without substituting invalid surrogates."""
        if not isinstance(value, str):
            raise ValueError('Expected string')
        encoded = value.encode('utf-8', errors='strict')
        if self.protocol == 0:
            self.data.extend(_json.dumps(value, ensure_ascii=False).encode('utf-8'))
        else:
            self.compact(len(encoded)); self.data.extend(encoded)
    def character(self, value):
        """Encode a byte or an ASCII JSON character."""
        _bounds(value, 1, True)
        if self.protocol == 0:
            if value > 127:
                raise ValueError('Non-ASCII JSON char')
            self.text(chr(value))
        else:
            self.data.append(value)
    def enumeration(self, value, names, collection):
        """Preserve context-sensitive enum representations."""
        index = int(value) if isinstance(value, _IntEnum) else value
        if type(index) is not int or not 0 <= index < len(names):
            raise ValueError('Unknown enum')
        if self.protocol == 0 or (self.protocol == 3 and not collection):
            self.text(names[index])
        else:
            self.compact(index)
    def begin_object(self):
        """Bound recursive object traversal, including cycles."""
        self.depth += 1
        if self.depth > 64:
            raise ValueError('Encode depth limit')
        if self.protocol == 0: self.data.append(123)
    def field(self, number, key, first):
        """Append a pre-encoded field key."""
        if self.protocol == 0:
            if not first: self.data.append(44)
            self.data.extend(_FIELDS[key][0])
        elif self.protocol == 2: self.compact(number)
        elif self.protocol == 3: self.data.extend(_FIELDS[key][1])
    def end_object(self):
        """Finish protocol-specific object framing."""
        if self.protocol == 0: self.data.append(125)
        elif self.protocol != 1: self.data.append(0)
        self.depth -= 1
    def begin_array(self, count):
        """Write framing without copying the source collection."""
        if self.protocol == 0: self.data.append(91)
        else: self.compact(count)
    def element(self, index):
        """Separate JSON entries."""
        if self.protocol == 0 and index: self.data.append(44)
    def end_array(self):
        """Close JSON array framing."""
        if self.protocol == 0: self.data.append(93)

def _field(name):
    """Cache binary and JSON key bytes once per generated module."""
    output = _Writer(3); output.text(name)
    return ((_json.dumps(name) + ':').encode('utf-8'), bytes(output.data))

class _Reader:
    """Borrow contiguous input during decoding; returned objects own their data."""
    __slots__ = ('data', 'protocol', 'limits', 'offset', 'depth', 'elements')
    def __init__(self, data, protocol, limits):
        """Validate the input envelope before allocation or traversal."""
        self.protocol = _protocol(protocol)
        self.limits = limits if limits is not None else Limits()
        if not isinstance(self.limits, Limits): raise ValueError('Expected Limits')
        self.data = memoryview(data).cast('B')
        self.offset = self.depth = self.elements = 0
        if len(self.data) > self.limits.max_bytes: raise ValueError('Message byte limit')
    def require(self, count):
        """Check bounds before every slice or scalar read."""
        if count < 0 or count > len(self.data)-self.offset: raise ValueError('Truncated input')
    def whitespace(self):
        """Skip exactly the four JSON whitespace bytes."""
        while self.offset < len(self.data) and self.data[self.offset] in (32, 9, 10, 13): self.offset += 1
    def expect(self, byte):
        """Require a delimiter without permitting trailing commas."""
        self.whitespace(); self.require(1)
        if self.data[self.offset] != byte: raise ValueError('Unexpected delimiter')
        self.offset += 1
    def compact(self):
        """Accept bounded compact integers including nonminimal legacy forms."""
        self.require(1); first = self.data[self.offset]; extra = first >> 6
        self.require(extra+1); value = first & 63
        for index in range(1, extra+1): value = value*256 + self.data[self.offset+index]
        self.offset += extra+1
        return value
    def token(self):
        """Retain all integer digits and reject non-ASCII number tokens."""
        self.whitespace(); start = self.offset
        while self.offset < len(self.data) and self.data[self.offset] not in (44, 125, 93, 32, 9, 10, 13): self.offset += 1
        if self.offset == start: raise ValueError('Missing token')
        return bytes(self.data[start:self.offset]).decode('ascii')
    def integer(self, width, unsigned):
        """Read bounded integers without intermediate floating-point conversion."""
        if self.protocol == 0:
            text = self.token()
            if len(text) > 20 or not _INTEGER.fullmatch(text) or (unsigned and text.startswith('-')): raise ValueError('Invalid integer')
            return _bounds(int(text), width, unsigned)
        self.require(width)
        value = int.from_bytes(self.data[self.offset:self.offset+width], 'little', signed=not unsigned)
        self.offset += width
        return value
    def floating(self, single):
        """Decode IEEE values or strictly validated finite JSON numbers."""
        codec = _FLOAT if single else _DOUBLE
        if self.protocol != 0:
            self.require(codec.size); value = codec.unpack_from(self.data, self.offset)[0]
            self.offset += codec.size
            return value
        text = self.token()
        if not _NUMBER.fullmatch(text): raise ValueError('Invalid number')
        value = float(text)
        if single: value = codec.unpack(codec.pack(value))[0]
        if not _math.isfinite(value) or (value == 0 and any(c in '123456789' for c in text.lower().split('e')[0])): raise ValueError('Floating-point range')
        return value
    def boolean(self):
        """Reject noncanonical boolean representations."""
        value = self.token() if self.protocol == 0 else self.integer(1, True)
        if value == ('true' if self.protocol == 0 else 1): return True
        if value == ('false' if self.protocol == 0 else 0): return False
        raise ValueError('Invalid boolean')
    def text(self):
        """Bound encoded strings and validate UTF-8 and escaped surrogates."""
        if self.protocol != 0:
            count = self.compact()
            if count > self.limits.max_string_bytes: raise ValueError('String limit')
            self.require(count); result = bytes(self.data[self.offset:self.offset+count]).decode('utf-8')
            self.offset += count
            return result
        self.expect(34); start = self.offset; escaped = False
        while True:
            self.require(1); byte = self.data[self.offset]; self.offset += 1
            if self.offset-start-1 > self.limits.max_string_bytes: raise ValueError('String limit')
            if not escaped and byte == 34: break
            escaped = not escaped and byte == 92
        result = _json.loads(bytes(self.data[start-1:self.offset]).decode('utf-8'))
        if len(result.encode('utf-8')) > self.limits.max_string_bytes: raise ValueError('String limit')
        return result
    def character(self):
        """Decode one ASCII JSON character or an unrestricted binary byte."""
        if self.protocol != 0: return self.integer(1, True)
        value = self.text()
        if len(value) != 1 or ord(value) > 127: raise ValueError('Invalid char')
        return ord(value)
    def enumeration(self, enum, names, collection):
        """Validate names and ordinals before constructing an enum."""
        index = names.index(self.text()) if self.protocol == 0 or (self.protocol == 3 and not collection) else self.compact()
        return enum(index)
    def fields(self, positional):
        """Yield wire keys in input order, preserving duplicate fields."""
        self.depth += 1
        if self.depth > self.limits.max_depth: raise ValueError('Depth limit')
        if self.protocol == 1:
            yield from positional
        elif self.protocol == 2:
            while True:
                key = self.compact()
                if key == 0: break
                yield key
        else:
            first = True
            if self.protocol == 0: self.expect(123)
            while True:
                if self.protocol == 0:
                    self.whitespace(); self.require(1)
                    if self.data[self.offset] == 125: self.offset += 1; break
                    if not first: self.expect(44)
                key = self.text()
                if self.protocol == 3 and not key: break
                if self.protocol == 0: self.expect(58)
                first = False
                yield key
        self.depth -= 1
    def entries(self):
        """Charge collection slots incrementally, never preallocating untrusted counts."""
        if self.protocol == 0: self.expect(91); count = None
        else:
            count = self.compact()
            if count > self.limits.max_elements-self.elements: raise ValueError('Element limit')
        index = 0
        while count is None or index < count:
            if count is None:
                self.whitespace(); self.require(1)
                if self.data[self.offset] == 93: self.offset += 1; break
                if index: self.expect(44)
            self.elements += 1
            if self.elements > self.limits.max_elements: raise ValueError('Element limit')
            yield index
            index += 1
    def finish(self):
        """Expose a result only after consuming exactly one complete message."""
        if self.protocol == 0: self.whitespace()
        if self.offset != len(self.data): raise ValueError('Trailing input')
)serializer"
