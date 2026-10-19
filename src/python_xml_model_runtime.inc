// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"modelpython(
_XML_MODEL_NAMESPACE = 'urn:serializer:xml:model'
_XML_MODEL_PREFIX = '{' + _XML_MODEL_NAMESPACE + '}'
_XML_MODEL_REVISION = _XML_MODEL_PREFIX + 'revision'
_XML_MODEL_MAGIC = _XML_MODEL_PREFIX + 'magic'
_XML_REVISION_NUMBER = _re.compile(r'(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?\Z')
_XML_DIGEST_MAX_BYTES = 1024 * 1024


def _xml_revision(value, kind, width):
    """Parse the shared canonical revision spelling with exact integer/dotted and wire-float values."""
    if type(value) is not str:
        raise ValueError('XML revision requires lexical text')
    if kind.startswith('version'):
        return _version_parts(value, int(kind[-1])), value
    value = value.strip(' \t\r\n')
    if not _XML_REVISION_NUMBER.fullmatch(value):
        raise ValueError('Invalid canonical XML revision')
    if kind.startswith('uint'):
        if not value.isascii() or not value.isdecimal() or len(value) > len(str((1 << (width * 8)) - 1)):
            raise ValueError('XML integer revision overflows its native type')
        selected = _bounds(int(value), width, True)
    else:
        selected = _xml_floating(value, kind)
        if not _math.isfinite(selected) or selected == 0 and not _xml_numeric(value).is_zero():
            raise ValueError('XML floating revision overflows or underflows its native type')
    return selected, selected


def _xml_model_select(model, lexical, context=None):
    """Select active fields before returning any candidate; immutable metadata owns interval boundaries."""
    meta, fields = model._xml_model, model._xml_token_fields
    revision = meta['revision']
    if context is not None:
        context.consume('work_units', len(fields) + len(lexical or '') + 1)
        context.consume('decoder_allocation_bytes', len(fields))
    if revision is None:
        if lexical is not None:
            raise ValueError('Unversioned XML model cannot carry revision metadata')
        return None, (True,) * len(fields)
    if lexical is None:
        raise ValueError('XML model revision metadata is required')
    comparison, native = _xml_revision(lexical, revision['kind'], revision['width'])
    if comparison < revision['minimum'] or comparison > revision['current']:
        raise ValueError('XML revision is outside the accepted historical/current interval')
    active = tuple((item['created'] is None or comparison >= item['created']) and
                   (item['obsolete'] is None or comparison < item['obsolete']) for item in fields)
    return native, active


def _xml_model_input(model, attributes, context=None):
    """Check reserved expanded metadata and remove it only from the local attribute inventory."""
    meta = model._xml_model
    for name in attributes:
        if name.startswith(_XML_MODEL_PREFIX) and name not in (_XML_MODEL_REVISION, _XML_MODEL_MAGIC):
            raise ValueError('Unknown reserved XML model metadata')
    magic = attributes.get(_XML_MODEL_MAGIC)
    if (meta['magic'] is None) != (magic is None) or magic is not None and magic != meta['magic']:
        raise ValueError('XML immutable model magic is missing or incorrect')
    native, active = _xml_model_select(model, attributes.get(_XML_MODEL_REVISION), context)
    ordinary = {name: value for name, value in attributes.items()
                if name not in (_XML_MODEL_REVISION, _XML_MODEL_MAGIC)}
    return ordinary, native, active


def _xml_model_output(model, context=None):
    """Derive metadata from the model's existing discriminator and its immutable declared magic."""
    meta, attributes, lexical = model._xml_model, [], None
    revision = meta['revision']
    if revision is not None:
        value = _xml_token_get(model, revision['member'])
        if revision['kind'].startswith('version'):
            lexical = value
        else:
            lexical = _xml_scalar(value, 'integer' if revision['kind'].startswith('uint') else
                                  revision['kind'], revision['width'], True, True)
        attributes.append((_XML_MODEL_REVISION, lexical))
    unused, active = _xml_model_select(model, lexical, context)
    if meta['magic'] is not None:
        attributes.append((_XML_MODEL_MAGIC, meta['magic']))
    return attributes, active


def _xml_digest(value, extent, encode=False):
    """Project exact digest bytes as hexadecimal without computing hashes or changing native storage."""
    if encode:
        if not hasattr(value, '__len__') or len(value) > _XML_DIGEST_MAX_BYTES or extent and len(value) != extent:
            raise ValueError('XML digest does not match its declared extent or byte bound')
        if any(type(byte) is not int or not 0 <= byte <= 255 for byte in value):
            raise ValueError('XML digest requires exact byte values')
        return bytes(value).hex()
    if type(value) is not str or len(value) % 2 or len(value) // 2 > _XML_DIGEST_MAX_BYTES:
        raise ValueError('XML digest requires bounded hexadecimal byte pairs')
    if extent and len(value) // 2 != extent:
        raise ValueError('XML digest does not match its declared extent')
    if any(character not in '0123456789abcdefABCDEF' for character in value):
        raise ValueError('XML digest requires exact hexadecimal bytes')
    return list(bytes.fromhex(value))

)modelpython"
