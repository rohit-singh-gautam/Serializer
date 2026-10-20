// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"identitypython(
class Limits(Limits):
    """Extend native message limits with an optional XML identity-index admission policy."""
    __slots__ = ('xml_identity', 'xml_map', 'xml_mixed', 'xml_pattern', 'xml_extensions')
    def __init__(self, max_bytes=64*1024*1024, max_string_bytes=16*1024*1024,
                 max_elements=1000000, max_depth=64, read_policy=ReadPolicy.STRICT,
                 xml_identity=None, xml_map=None, xml_mixed=None, xml_pattern=None, xml_extensions=None):
        """Keep ordinary limit arguments compatible and expose XML-only tuple bounds explicitly."""
        super().__init__(max_bytes, max_string_bytes, max_elements, max_depth, read_policy)
        self.xml_identity = xml_identity
        self.xml_map = xml_map
        self.xml_mixed = xml_mixed
        self.xml_pattern = xml_pattern
        self.xml_extensions = xml_extensions


class XmlIdentityLimits:
    """Logical admission bounds for one generated XSD key/unique/keyref owner scope."""
    def __init__(self, max_constraints=128, max_fields=32, max_depth=128,
                 max_selected_nodes=100000, max_tuples=100000,
                 max_tuple_bytes=8 * 1024 * 1024, max_work_units=64 * 1024 * 1024):
        """Validate explicit nonnegative bounds; zero is a real bound and equality is permitted."""
        for key, value in locals().copy().items():
            if key == 'self':
                continue
            if type(value) is not int or value < 0:
                raise ValueError('Invalid XML identity limit: ' + key)
            setattr(self, key, value)


def _xml_identity_name(value):
    """Normalize supported expanded event-name spellings without using presentation prefixes."""
    if isinstance(value, tuple):
        return value
    if value.startswith('{'):
        uri, local = value[1:].split('}', 1)
        return uri, local
    if '\x1f' in value:
        return tuple(value.split('\x1f', 1))
    return '', value


def _xml_identity_decimal(value):
    """Canonicalize exact finite decimal keys without integer-size or decimal-context rounding."""
    negative = value.startswith('-')
    value = value.lstrip('+-')
    integer, unused, fraction = value.partition('.')
    integer = integer.lstrip('0') or '0'
    fraction = fraction.rstrip('0')
    sign = '-' if negative and (integer != '0' or fraction) else ''
    return sign + integer + ('.' + fraction if fraction else '')


class _XmlTokenIdentity:
    """Retain bounded scalar tuples and open selections, never element descendants or a tree."""
    def __init__(self, contract, context=None, limits=None):
        """Borrow checked generated metadata and cumulative context through complete-scope success."""
        self.contract = contract
        self.context = context
        self.limits = limits or XmlIdentityLimits()
        self.path, self.active = [], []
        self.selected, self.tuples, self.tuple_bytes, self.work = 0, 0, 0, 0
        self.started, self.complete, self.failed = False, False, False
        constraints = contract['constraints']
        if len(constraints) > self.limits.max_constraints:
            raise ValueError('XML identity constraint limit exceeded')
        self.tables = [set() for unused in constraints]
        self.targets = {value['id']: index for index, value in enumerate(constraints)}
        for value in constraints:
            if not value['fields'] or len(value['fields']) > self.limits.max_fields:
                raise ValueError('XML identity field limit exceeded')
            if value['kind'] == 'keyref' and value['refer'] not in self.targets:
                raise ValueError('XML keyref target is outside the owner scope')
        self._allocate(64 * (len(self.tables) + len(self.targets)))

    def _work(self, count=1):
        """Check interruption and bounded matching work before inspecting the next input portion."""
        if count > self.limits.max_work_units - self.work:
            raise ValueError('XML identity work limit exceeded')
        self.work += count
        if self.context is not None:
            self.context.consume('work_units', count)

    def _allocate(self, count):
        """Charge retained logical tuple/path bytes before each corresponding allocation."""
        if self.context is not None:
            self.context.consume('decoder_allocation_bytes', count)

    def _matches(self, actual, expected):
        """Bound expanded-name comparisons, including long namespace URI common prefixes."""
        self._work()
        if len(actual) != len(expected):
            return False
        for left, right in zip(actual, expected):
            self._work(sum(min(len(a.encode('utf-8')), len(b.encode('utf-8')))
                           for a, b in zip(left, right)) + 1)
            if left != right:
                return False
        return True

    def _key(self, lexical, field, scope):
        """Preserve exact XSD value-space equality, including URI families and expanded QNames."""
        size = len(lexical.encode('utf-8'))
        if size > self.limits.max_tuple_bytes:
            raise ValueError('XML identity field-byte limit exceeded')
        self._work(size + 1)
        self._allocate(size)
        kind = field['kind']
        policy = 'preserve' if kind == 'string' else 'replace' if kind == 'normalizedString' else 'collapse'
        value = _xml_normalize(lexical, policy)
        _xml_lexical_kind(value, kind, scope)
        if kind == 'boolean':
            result = ('boolean', 'true' if value in ('true', '1') else 'false')
        elif kind in ('decimal', 'integer', 'positiveInteger', 'nonNegativeInteger',
                      'negativeInteger', 'nonPositiveInteger', 'byte', 'short', 'int', 'long',
                      'unsignedByte', 'unsignedShort', 'unsignedInt', 'unsignedLong'):
            result = ('decimal', _xml_identity_decimal(value))
        elif kind == 'QName':
            result = ('QName',) + _xml_resolve_qname(value, scope)
        else:
            result = ('uri' if kind == 'anyURI' else 'text', value)
        self._allocate(sum(len(part.encode('utf-8')) for part in result[1:]))
        return result

    def start(self, name, attrs, scope=None):
        """Select current nodes and matching scalar fields using only the open expanded path."""
        try:
            self._start(name, attrs, scope or {})
        except BaseException:
            self.failed = True
            raise

    def _start(self, name, attrs, scope):
        """Admit one start token before any selected value or candidate storage is retained."""
        if self.failed or self.complete:
            raise ValueError('XML identity validator is not active')
        selected = _xml_identity_name(name)
        self._work(sum(len(part) for part in selected) + 1)
        if len(self.path) >= self.limits.max_depth:
            raise ValueError('XML identity depth limit exceeded')
        if not self.started:
            if selected != self.contract['owner']:
                raise ValueError('XML identity owner scope mismatch')
            self.started = True
        self._allocate(sum(len(part.encode('utf-8')) for part in selected) + 32)
        self.path.append(selected)
        relative = tuple(self.path[1:])
        self._allocate(len(attrs) * 64)
        attributes = {_xml_identity_name(key): value for key, value in attrs.items()}
        for candidate in self.active:
            for capture in candidate['captures']:
                if capture is not None:
                    raise ValueError('XML identity field has complex child content')
        for index, constraint in enumerate(self.contract['constraints']):
            selector = constraint['selector']
            steps = selector['steps']
            self._work(len(relative) + len(steps) + 1)
            matches = (not steps or self._matches(relative[-len(steps):], steps)) if selector['descendants'] else self._matches(relative, steps)
            if matches:
                if self.selected >= self.limits.max_selected_nodes:
                    raise ValueError('XML identity selected-node limit exceeded')
                self.selected += 1
                count = len(constraint['fields'])
                self._allocate(64 + count * 32 + len(self.path) * 8)
                self.active.append({'index': index, 'path': tuple(self.path), 'values': [None] * count,
                                    'seen': [False] * count, 'captures': [None] * count, 'bytes': 0})
        for candidate in self.active:
            relative = tuple(self.path[len(candidate['path']):])
            constraint = self.contract['constraints'][candidate['index']]
            for index, field in enumerate(constraint['fields']):
                self._work(len(relative) + len(field['path']['steps']) + 1)
                if not self._matches(relative, field['path']['steps']):
                    continue
                attribute = field['path']['attribute']
                if attribute is not None and attribute not in attributes:
                    continue
                if constraint['kind'] == 'key' and field.get('nillable', False):
                    raise ValueError('XSD key field declaration must not be nillable')
                if candidate['seen'][index]:
                    raise ValueError('XML identity field selects multiple values')
                candidate['seen'][index] = True
                if attribute is not None:
                    self._store(candidate, index, self._key(attributes[attribute], field, scope))
                else:
                    nil = attributes.get(('http://www.w3.org/2001/XMLSchema-instance', 'nil'), 'false').strip(' \t\r\n')
                    if nil not in ('true', '1', 'false', '0'):
                        raise ValueError('Malformed XML identity nil value')
                    self._allocate(sum(len(key) + len(value) for key, value in scope.items()))
                    candidate['captures'][index] = {'parts': [], 'bytes': 0, 'scope': dict(scope),
                                                     'nil': nil in ('true', '1'), 'depth': len(self.path)}

    def _store(self, candidate, index, value):
        """Admit the complete candidate tuple including fields of a later incomplete tuple."""
        size = sum(len(part.encode('utf-8')) for part in value[1:])
        if size > self.limits.max_tuple_bytes - candidate['bytes']:
            raise ValueError('XML identity candidate tuple-byte limit exceeded')
        candidate['bytes'] += size
        candidate['values'][index] = value

    def text(self, value):
        """Accumulate only selected simple field text and bound split-token scalar retention."""
        try:
            if self.failed or self.complete or not self.path:
                raise ValueError('XML identity validator is not active')
            self._work(len(value) + 1)
            size = len(value.encode('utf-8'))
            for candidate in self.active:
                for capture in candidate['captures']:
                    self._work()
                    if capture is None:
                        continue
                    if capture['nil'] and value:
                        raise ValueError('Nil XML identity field contains text')
                    if size > self.limits.max_tuple_bytes - capture['bytes']:
                        raise ValueError('XML identity field-byte limit exceeded')
                    self._allocate(size + 16)
                    capture['bytes'] += size
                    capture['parts'].append(value)
        except BaseException:
            self.failed = True
            raise

    def end(self):
        """Finalize selected tuples as nodes close and retain forward references until scope EOF."""
        try:
            self._end()
        except BaseException:
            self.failed = True
            raise

    def _end(self):
        """Complete scalar captures before checking the selected element's entire key tuple."""
        if self.failed or self.complete or not self.path:
            raise ValueError('Unbalanced XML identity token')
        self._work()
        for candidate in self.active:
            constraint = self.contract['constraints'][candidate['index']]
            for index, capture in enumerate(candidate['captures']):
                self._work()
                if capture is not None and capture['depth'] == len(self.path):
                    if not capture['nil']:
                        self._allocate(capture['bytes'])
                        self._store(candidate, index, self._key(''.join(capture['parts']),
                                    constraint['fields'][index], capture['scope']))
                    candidate['captures'][index] = None
        retained = []
        for candidate in self.active:
            if len(candidate['path']) != len(self.path):
                retained.append(candidate)
                continue
            constraint = self.contract['constraints'][candidate['index']]
            if any(value is None for value in candidate['values']):
                if constraint['kind'] == 'key':
                    raise ValueError('XSD key tuple is incomplete or nil')
                continue
            if self.tuples >= self.limits.max_tuples:
                raise ValueError('XML identity tuple-count limit exceeded')
            if candidate['bytes'] > self.limits.max_tuple_bytes - self.tuple_bytes:
                raise ValueError('XML identity tuple-byte limit exceeded')
            if getattr(self.context, 'admit_reference', None) is not None:
                _xml_field_context(self.context, constraint['id'][1]).admit_reference()
            self.tuples += 1
            self.tuple_bytes += candidate['bytes']
            self._work(candidate['bytes'] + 1)
            table = self.tables[candidate['index']]
            value = tuple(candidate['values'])
            if value in table and constraint['kind'] != 'keyref':
                raise ValueError('Duplicate XSD key/unique tuple')
            self._allocate(64 + len(value) * 16)
            table.add(value)
        self.active = retained
        self.path.pop()
        if not self.path:
            self.complete = True

    def finish(self):
        """Resolve all complete keyrefs after keys exist, before publishing the owning model."""
        if self.failed or not self.complete or self.path or self.active:
            raise ValueError('Incomplete or failed XML identity scope')
        try:
            self._work()
            for index, constraint in enumerate(self.contract['constraints']):
                if constraint['kind'] != 'keyref':
                    continue
                target = self.tables[self.targets[constraint['refer']]]
                for value in self.tables[index]:
                    self._work(sum(len(part.encode('utf-8')) for field in value for part in field[1:]) + 1)
                    if value not in target:
                        raise ValueError('Unresolved XSD keyref tuple in owner scope')
        except BaseException:
            self.failed = True
            raise
)identitypython"
