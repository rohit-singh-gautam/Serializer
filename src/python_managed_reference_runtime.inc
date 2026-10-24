// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
R"serializer(

class ManagedReference:
    """Nonowning wire token; decoding establishes no target liveness or ownership."""
    __slots__ = ('document_high', 'document_low', 'class_id', 'object_id')
    def __init__(self, document_high=0, document_low=0, class_id=0, object_id=0):
        """Store untrusted envelope values; the owning field validates its exact target."""
        self.document_high, self.document_low = document_high, document_low
        self.class_id, self.object_id = class_id, object_id
    def _validate(self, expected_class_id, id_bytes):
        """Check widths, nonempty scope and the field's exact static class discriminator."""
        _bounds(self.document_high, 8, True)
        _bounds(self.document_low, 8, True)
        _bounds(self.class_id, 8, True)
        _bounds(self.object_id, id_bytes, True)
        if not (self.document_high or self.document_low) or not self.object_id or self.class_id != expected_class_id:
            raise ValueError('Invalid managed entity reference envelope')
    def _write(self, out, expected_class_id, id_bytes, compact, keys):
        """Write the C++ token envelope after validation, preserving all identity values."""
        self._validate(expected_class_id, id_bytes)
        out.begin_object()
        for index, value in enumerate((self.document_high, self.document_low, self.class_id, self.object_id)):
            out.field(index + 1, keys[index], index == 0)
            if index == 3 and compact: out.compact_value(value, id_bytes, False, True)
            else: out.integer(value, id_bytes if index == 3 else 8, True)
        out.end_object()
    @classmethod
    def _read(cls, input, expected_class_id, id_bytes, compact):
        """Decode a fresh complete envelope; duplicates and unknown metadata always fail."""
        names = ('document_high', 'document_low', 'class_id', 'object_id')
        values, seen = [0] * len(names), set()
        for key in input.fields((1, 2, 3, 4)):
            if type(key) is int:
                index = key - 1
                if not 0 <= index < len(names): raise ValueError('Unknown managed reference metadata field')
            else:
                if key not in names: raise ValueError('Unknown managed reference metadata name')
                index = names.index(key)
            if index in seen: raise ValueError('Duplicate managed reference metadata field')
            seen.add(index)
            values[index] = input.compact_value(id_bytes, False) if index == 3 and compact else input.integer(id_bytes if index == 3 else 8, True)
        if len(seen) != len(names): raise ValueError('Managed reference metadata is incomplete')
        result = cls(*values)
        result._validate(expected_class_id, id_bytes)
        return result
)serializer"
