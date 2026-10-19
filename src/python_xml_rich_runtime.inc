// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"richpython(
import decimal as _xml_decimal

_XML_PATTERN_SCHEMAS = {}
_XML_TYPES = {}


class _XmlLexicalError(ValueError):
    """Distinguish a rejected union value from an exception raised by the operation's caller."""


def _xml_builtin_scalar(value, kind, width=0, unsigned=False):
    """Adapt ordinary scalar failures without catching operation callbacks outside this helper."""
    try:
        return _xml_scalar(value, kind, width, unsigned)
    except (ValueError, OverflowError) as error:
        raise _XmlLexicalError(str(error)) from error

def _xml_name_start(character):
    """Recognize XML1.0 fifth-edition NameStartChar excluding the separately handled colon."""
    code = ord(character)
    return character == '_' or 'A' <= character <= 'Z' or 'a' <= character <= 'z' or 0xc0 <= code <= 0xd6 or 0xd8 <= code <= 0xf6 or 0xf8 <= code <= 0x2ff or 0x370 <= code <= 0x37d or 0x37f <= code <= 0x1fff or 0x200c <= code <= 0x200d or 0x2070 <= code <= 0x218f or 0x2c00 <= code <= 0x2fef or 0x3001 <= code <= 0xd7ff or 0xf900 <= code <= 0xfdcf or 0xfdf0 <= code <= 0xfffd or 0x10000 <= code <= 0xeffff


def _xml_name_character(character):
    """Recognize the additional NameChar code points independently of first-character rules."""
    code = ord(character)
    return _xml_name_start(character) or character in '-.' or '0' <= character <= '9' or code == 0xb7 or 0x300 <= code <= 0x36f or 0x203f <= code <= 0x2040


def _xml_ncname(value):
    """Validate XML NCNames without conflating a presentation prefix with its namespace URI."""
    return bool(value) and _xml_name_start(value[0]) and all(_xml_name_character(character) for character in value[1:])


def _xml_valid_name(value, token=False):
    """Validate Name or NMTOKEN, whose colon and first-character rules differ from NCName."""
    return bool(value) and (token or value[0] == ':' or _xml_name_start(value[0])) and all(character == ':' or _xml_name_character(character) for character in value)

def _xml_resolve_qname(value, bindings):
    """Resolve a lexical QName to its expanded identity using retained in-scope declarations."""
    pieces = value.strip(' \t\r\n').split(':')
    if len(pieces) == 1 and _xml_ncname(pieces[0]):
        return (bindings.get('', ''), pieces[0])
    if len(pieces) == 2 and all(_xml_ncname(piece) for piece in pieces) and pieces[0] in bindings and bindings[pieces[0]]:
        return (bindings[pieces[0]], pieces[1])
    raise _XmlLexicalError('Invalid or unbound XML QName')

def _xml_qualify_qname(value, bindings):
    """Allocate a collision-free declaration when a schema default uses another presentation prefix."""
    uri, local = value
    if not uri:
        bindings[''] = ''
        return local
    for prefix, selected in bindings.items():
        if selected == uri:
            return prefix + ':' + local if prefix else local
    index = 0
    while 'q' + str(index) in bindings: index += 1
    prefix = 'q' + str(index)
    bindings[prefix] = uri
    return prefix + ':' + local

def _xml_normalize(value, whitespace):
    """Apply XML Schema whitespace rules using only the four XML whitespace characters."""
    if whitespace in ('replace', 'collapse'):
        value = value.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ')
    return ' '.join(piece for piece in value.split(' ') if piece) if whitespace == 'collapse' else value

def _xml_tokens(value):
    """Split lists using exactly XML whitespace, retaining other Unicode whitespace in token values."""
    return _re.findall(r'[^ \t\r\n]+', value)


def _xml_numeric(value):
    """Compare decimal bounds and integer enums exactly, including the complete unsigned64 domain."""
    if value in ('INF', '-INF', 'NaN'):
        return _xml_decimal.Decimal({'INF': 'Infinity', '-INF': '-Infinity', 'NaN': 'NaN'}[value])
    if not _XML_NUMBER.fullmatch(value):
        raise _XmlLexicalError('Invalid XML numeric lexical value')
    try:
        return _xml_decimal.Decimal(value)
    except _xml_decimal.InvalidOperation as error:
        raise _XmlLexicalError('Invalid XML numeric lexical value') from error

def _xml_floating(value, kind):
    """Round directly to the selected IEEE width, correcting a float64-to-float32 midpoint tie."""
    if value in ('INF', '-INF', 'NaN'):
        return {'INF': _math.inf, '-INF': -_math.inf, 'NaN': _math.nan}[value]
    if not _XML_NUMBER.fullmatch(value): raise _XmlLexicalError('Invalid XML floating lexical value')
    number = float(value)
    if not _math.isfinite(number): raise _XmlLexicalError('XML floating value overflows model')
    if kind == 'double' or number == 0.0: return number
    negative, magnitude = value.startswith('-'), abs(number)
    infinity_bits = 0x7f800000
    try:
        bits = _struct.unpack('<I', _FLOAT.pack(magnitude))[0]
    except OverflowError:
        bits = infinity_bits
    # Adjacent binary32 values and their midpoint fit exactly in binary64. Comparing the
    # original decimal spelling with that exact midpoint avoids the second-rounding error.
    exact = _xml_numeric(value).copy_abs()
    overflow_midpoint = 2.0 ** 128 - 2.0 ** 103
    if bits == infinity_bits:
        if exact >= _xml_decimal.Decimal.from_float(overflow_midpoint):
            raise _XmlLexicalError('XML floating value overflows model')
        bits -= 1
    else:
        selected = _FLOAT.unpack(_struct.pack('<I', bits))[0]
        if bits:
            previous = _FLOAT.unpack(_struct.pack('<I', bits - 1))[0]
            midpoint = _xml_decimal.Decimal.from_float((selected + previous) / 2.0)
            if exact < midpoint or exact == midpoint and bits & 1:
                bits -= 1
                return -_FLOAT.unpack(_struct.pack('<I', bits))[0] if negative else _FLOAT.unpack(_struct.pack('<I', bits))[0]
        following = _FLOAT.unpack(_struct.pack('<I', bits + 1))[0]
        midpoint = _xml_decimal.Decimal.from_float(overflow_midpoint if bits + 1 == infinity_bits else (selected + following) / 2.0)
        if exact > midpoint or exact == midpoint and bits & 1:
            bits += 1
            if bits == infinity_bits: raise _XmlLexicalError('XML floating value overflows model')
    result = _FLOAT.unpack(_struct.pack('<I', bits))[0]
    return -result if negative else result


def _xml_facet_number(value, kind, context=None):
    """Use selected-width floating values while retaining exact integer/decimal facet arithmetic."""
    if context is not None:
        context.consume('work_units', len(value) + 1)
        context.consume('decoder_allocation_bytes', len(value) * 4)
    return _xml_floating(value, kind) if kind in ('float', 'double') else _xml_numeric(value)


def _xml_is_nan(value):
    """Inspect either supported numeric representation without converting exact decimal values."""
    return _math.isnan(value) if isinstance(value, float) else value.is_nan()


def _xml_is_finite(value):
    """Inspect either supported numeric representation without converting exact decimal values."""
    return _math.isfinite(value) if isinstance(value, float) else value.is_finite()


def _xml_lexical_kind(value, kind, bindings):
    """Validate supported XSD primitive value spaces before checking their derived restrictions."""
    if kind in ('', 'string', 'normalizedString', 'token', 'anyURI'):
        return
    if kind == 'QName':
        _xml_resolve_qname(value, bindings)
    elif kind in ('NMTOKENS', 'IDREFS', 'ENTITIES'):
        tokens = value.split(' ')
        if not all(_xml_valid_name(token, True) if kind == 'NMTOKENS' else _xml_ncname(token) for token in tokens):
            raise _XmlLexicalError('Invalid XML name list')
    elif kind in ('NCName', 'ID', 'IDREF', 'ENTITY'):
        if not _xml_ncname(value): raise _XmlLexicalError('Invalid XML NCName')
    elif kind in ('Name', 'NMTOKEN'):
        if not _xml_valid_name(value, kind == 'NMTOKEN'):
            raise _XmlLexicalError('Invalid XML name token')
    elif kind == 'language':
        if not _re.fullmatch(r'[A-Za-z]{1,8}(?:-[A-Za-z0-9]{1,8})*', value):
            raise _XmlLexicalError('Invalid XSD language')
    elif kind == 'boolean':
        _xml_builtin_scalar(value, 'bool')
    elif kind in ('float', 'double'):
        _xml_builtin_scalar(value, kind)
    elif kind == 'decimal':
        if not _re.fullmatch(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)', value):
            raise _XmlLexicalError('Invalid XSD decimal')
    else:
        integral = {'byte': (1, False), 'unsignedByte': (1, True), 'short': (2, False), 'unsignedShort': (2, True),
                    'int': (4, False), 'unsignedInt': (4, True), 'long': (8, False), 'unsignedLong': (8, True)}
        if kind in integral:
            width, unsigned = integral[kind]
            _xml_builtin_scalar(value, 'integer', width, unsigned)
        elif kind in ('integer', 'positiveInteger', 'negativeInteger', 'nonPositiveInteger', 'nonNegativeInteger'):
            if not _XML_INTEGER.fullmatch(value): raise _XmlLexicalError('Invalid XSD integer')
            number = _xml_numeric(value)
            if kind == 'positiveInteger' and number <= 0 or kind == 'negativeInteger' and number >= 0 or kind == 'nonPositiveInteger' and number > 0 or kind == 'nonNegativeInteger' and number < 0:
                raise _XmlLexicalError('XSD integer sign restriction failed')
        else:
            raise _XmlLexicalError('Unsupported generated XSD lexical kind: ' + kind)

def _xml_pattern(value, patterns, context):
    """Use the optional prepared XSD1.0 provider for genuine XSD regex semantics, never Python re."""
    if context is not None: context.consume('work_units', len(value) + 1)
    key = tuple(patterns)
    if key not in _XML_PATTERN_SCHEMAS:
        try:
            from lxml import etree as provider
        except ImportError as error:
            raise RuntimeError('Generated XSD pattern facets require the lxml XMLSchema provider') from error
        namespace = 'http://www.w3.org/2001/XMLSchema'
        schema = provider.Element('{' + namespace + '}schema', nsmap={'xs': namespace})
        root = provider.SubElement(schema, '{' + namespace + '}element', name='value')
        simple = provider.SubElement(root, '{' + namespace + '}simpleType')
        restriction = provider.SubElement(simple, '{' + namespace + '}restriction', base='xs:string')
        for pattern in patterns: provider.SubElement(restriction, '{' + namespace + '}pattern', value=pattern)
        _XML_PATTERN_SCHEMAS[key] = (provider, provider.XMLSchema(schema))
    provider, prepared = _XML_PATTERN_SCHEMAS[key]
    root = provider.Element('value')
    root.text = value
    matched = prepared.validate(root)
    if context is not None: context.check()
    return matched

def _xml_value_equal(left, right, rule, bindings, context=None):
    """Compare enums/fixed values in their generated XSD value space, not presentation spelling."""
    kind = rule['kind']
    if context is not None: context.consume('work_units', len(left) + len(right) + 1)
    if rule['qname'] or kind == 'QName':
        return _xml_resolve_qname(left, bindings) == _xml_resolve_qname(right, bindings)
    if rule['list_item']:
        left, right = _xml_tokens(left), _xml_tokens(right)
        if len(left) != len(right): return False
        item = dict(rule, kind=rule['list_item'], list_item='', qname=False)
        return all(_xml_value_equal(a, b, item, bindings, context) for a, b in zip(left, right))
    if kind == 'boolean': return _xml_builtin_scalar(left, 'bool') == _xml_builtin_scalar(right, 'bool')
    if kind not in ('', 'string', 'normalizedString', 'token', 'anyURI', 'language', 'Name', 'NCName', 'NMTOKEN', 'NMTOKENS', 'ID', 'IDREF', 'IDREFS', 'ENTITY', 'ENTITIES'):
        a, b = _xml_facet_number(left, kind, context), _xml_facet_number(right, kind, context)
        return _xml_is_nan(a) and _xml_is_nan(b) or not _xml_is_nan(a) and not _xml_is_nan(b) and a == b
    return _xml_normalize(left, rule['whitespace']) == _xml_normalize(right, rule['whitespace'])

)richpython"
R"richpython(def _xml_checked(value, rule, bindings, context=None):
    """Validate an exact immutable scalar/list/union contract and return its normalized lexical value."""
    if context is not None:
        context.consume('work_units', len(value) + 1)
        context.consume('decoder_allocation_bytes', len(value) * 4)
    value = _xml_normalize(value, rule['whitespace'])
    if rule['union']:
        for alternative in rule['union']:
            try:
                _xml_checked(value, alternative, bindings, context)
                break
            except _XmlLexicalError:
                pass
        else: raise _XmlLexicalError('XML value matches no XSD union member')
    elif rule['list_item']:
        for item in _xml_tokens(value): _xml_lexical_kind(item, rule['list_item'], bindings)
    else:
        _xml_lexical_kind(value, rule['kind'], bindings)
    if rule['prefix_list']:
        for prefix in _xml_tokens(value):
            if not _xml_ncname(prefix) or not bindings.get(prefix): raise _XmlLexicalError('Unbound XML prefix-list token')
    if rule['qname']: _xml_resolve_qname(value, bindings)
    length = len(_xml_tokens(value)) if rule['list_item'] else len(value)
    if rule['length'] is not None and length != rule['length'] or rule['min_length'] is not None and length < rule['min_length'] or rule['max_length'] is not None and length > rule['max_length']:
        raise _XmlLexicalError('XML length facet failed')
    if rule['finite'] or any(rule[key] is not None for key in ('min_inclusive', 'max_inclusive', 'min_exclusive', 'max_exclusive')):
        number = _xml_facet_number(value, rule['kind'], context)
        if rule['finite'] and not _xml_is_finite(number): raise _XmlLexicalError('XML finite/numeric facet failed')
        for key, compare in (('min_inclusive', lambda a, b: a >= b), ('max_inclusive', lambda a, b: a <= b),
                             ('min_exclusive', lambda a, b: a > b), ('max_exclusive', lambda a, b: a < b)):
            if rule[key] is not None:
                boundary = _xml_facet_number(rule[key], rule['kind'], context)
                if _xml_is_nan(number) or _xml_is_nan(boundary):
                    accepted = key.endswith('inclusive') and _xml_is_nan(number) and _xml_is_nan(boundary)
                else:
                    accepted = compare(number, boundary)
                if not accepted: raise _XmlLexicalError('XML numeric facet failed')
    if rule['enumeration'] and not rule['qnames'] and not any(_xml_value_equal(value, allowed, rule, bindings, context) for allowed in rule['enumeration']):
        raise _XmlLexicalError('XML enumeration facet failed')
    if rule['qnames'] and _xml_resolve_qname(value, bindings) not in rule['qnames']:
        raise _XmlLexicalError('XML QName enumeration facet failed')
    for patterns in rule['patterns']:
        if not _xml_pattern(value, patterns, context): raise _XmlLexicalError('XML pattern facet failed')
    if rule['fixed_qname'] is not None:
        if _xml_resolve_qname(value, bindings) != rule['fixed_qname']: raise _XmlLexicalError('XML fixed QName failed')
    elif rule['fixed'] is not None and not _xml_value_equal(value, rule['fixed'], rule, bindings, context):
        raise _XmlLexicalError('XML fixed value failed')
    return value

def _xml_namespace(name):
    """Inspect an expanded element/attribute identity without interpreting a lexical prefix."""
    return name[1:].split('}', 1)[0] if name.startswith('{') else ''

def _xml_wildcard_namespace(uri, rule, target):
    """Match the declarative XSD wildcard namespace set exactly."""
    return any(token == '##any' or token == '##other' and bool(uri) and uri != target or
               token == '##local' and not uri or token == '##targetNamespace' and uri == target or token == uri
               for token in rule.split())

class _XmlRepeatedChoice:
    """Validate ordered maximal branch runs in constant additional space under one operation budget."""
    def __init__(self, contract, target, context):
        """Retain immutable group/branch ranges, including branches that can match empty content."""
        self.contract, self.target, self.context = contract, target, context
        self.least, self.most, self.run, self.branch = 0, 0, 0, None
        self.empty = any(branch['minimum'] == 0 for branch in contract['branches'])
        if context is not None: context.consume('work_units', len(contract['branches']))
    def select(self, name):
        """Dispatch an expanded child identity without constructing an alternative tree."""
        uri = _xml_namespace(name)
        for index, branch in enumerate(self.contract['branches']):
            if self.context is not None:
                self.context.consume('work_units', len(name) + len(branch['namespace']) + 1)
            if branch['name'] == name or branch['name'] is None and _xml_wildcard_namespace(uri, branch['namespace'], branch['target']):
                return index
        return None
    def flush(self):
        """Partition a maximal same-branch run into its feasible repetition-count interval."""
        if not self.run: return
        branch = self.contract['branches'][self.branch]
        maximum = branch['maximum']
        if maximum == 0: raise ValueError('XML choice branch forbids content')
        least = (self.run + maximum - 1) // maximum if maximum is not None else 1
        most = self.run // max(1, branch['minimum'])
        if least > most: raise ValueError('XML choice branch occurrence interval failed')
        self.least += least
        self.most += most
        self.run = 0
    def consume(self, branch):
        """Extend one selected run, flushing only when the ordered stream changes branch."""
        if self.context is not None: self.context.consume('work_units', 1)
        if self.run and self.branch != branch: self.flush()
        self.branch, self.run = branch, self.run + 1
    def finish(self):
        """Accept exactly when a permitted group count represents every ordered child run."""
        self.flush()
        selected = max(self.contract['minimum'], self.least)
        maximum = self.contract['maximum']
        if maximum is not None and selected > maximum or not self.empty and selected > self.most:
            raise ValueError('XML repeated choice occurrence failed')


class _XmlFieldsRich(_XmlFields):
    """Consume known sequence slots and explicitly allowed positioned wildcard extensions."""
    def __init__(self, node, has_text, contract, context=None):
        """Keep the candidate's extension state private until all known fields and facets succeed."""
        super().__init__(node, has_text)
        self.contract, self.context, self.position = contract, context, 0
        self.item = 0
        if context is not None: context.consume('decoder_allocation_bytes', len(contract['wildcards']) * 8)
        self.counts = [0] * len(contract['wildcards'])
        self.attributes, self.children_kept = {}, []
    def unknown(self, name, node=None):
        """Apply namespace, position, processContents and optional extension policy before skipping anything."""
        uri = _xml_namespace(name)
        selected, has_rules = None, False
        for index, rule in enumerate(self.contract['wildcards']):
            if self.context is not None: self.context.consume('work_units', len(uri) + len(rule['namespace']) + 1)
            if rule['attribute'] != (node is None): continue
            has_rules = True
            if selected is None and (node is None or rule['position'] == self.position) and _xml_wildcard_namespace(uri, rule['namespace'], rule['target']):
                selected = (index, rule)
        if selected is None:
            if has_rules or not uri or uri == self.contract['namespace'] or self.contract['unknown'] == 'reject':
                raise ValueError('Unexpected XML attribute or child namespace/position')
        else:
            index, rule = selected
            if rule['maximum'] is not None and self.counts[index] >= rule['maximum']:
                raise ValueError('XML wildcard occurrence constraint failed')
            self.counts[index] += 1
            if rule['process'] != 'skip':
                selected = _XML_TYPES.get(name) if node is not None else None
                if selected is not None:
                    selected._xml_from_node(node, self.context)
                elif rule['process'] == 'strict':
                    raise ValueError('Strict XML wildcard has no generated schema declaration')
        if self.context is not None: self.context.consume('work_units', 1)
        if self.contract['unknown'] == 'preserve':
            if self.context is not None: self.context.consume('decoder_allocation_bytes', 64)
            if node is None:
                self.attributes[name] = self.node[1][name]
            else:
                self.children_kept.append((self.position, self.item, node))
    def children(self, name, minimum, maximum):
        """Read one known slot while honoring wildcards that occur immediately before it."""
        self.item = 0
        choice = next((group for group in self.contract['choices'] if group['position'] == self.position), None)
        if choice is not None:
            validator, result = _XmlRepeatedChoice(choice, self.contract['namespace'], self.context), []
            while self.next < len(self.node[2]):
                child = self.node[2][self.next]
                if child[0] != name and child[0] in self.contract['known_children']: break
                branch = validator.select(child[0])
                if branch is None: break
                validator.consume(branch)
                if child[0] == name:
                    if self.context is not None: self.context.consume('decoder_allocation_bytes', 8)
                    result.append(child)
                    self.item += 1
                else:
                    self.unknown(child[0], child)
                self.next += 1
            validator.finish()
            if len(result) < minimum or maximum is not None and len(result) > maximum:
                raise ValueError('XML field occurrence mismatch')
            self.position += 1
            self.item = 0
            return result
        while self.next < len(self.node[2]) and self.node[2][self.next][0] != name:
            child = self.node[2][self.next]
            if child[0] in self.contract['known_children']: break
            if minimum == 0 and any(not rule['attribute'] for rule in self.contract['wildcards']) and not any(
                    not rule['attribute'] and rule['position'] == self.position and
                    _xml_wildcard_namespace(_xml_namespace(child[0]), rule['namespace'], rule['target'])
                    for rule in self.contract['wildcards']):
                break
            self.unknown(child[0], child)
            self.next += 1
        start = self.next
        while self.next < len(self.node[2]) and self.node[2][self.next][0] == name:
            if self.context is not None: self.context.consume('work_units', 1)
            self.next += 1
        count = self.next - start
        if count < minimum or maximum is not None and count > maximum:
            raise ValueError('XML field occurrence mismatch')
        if self.context is not None: self.context.consume('decoder_allocation_bytes', count * 8)
        result = self.node[2][start:self.next]
        self.position += 1
        return result
    def finish(self):
        """Validate remaining extension occurrences and attributes before releasing retained state."""
        while self.next < len(self.node[2]):
            child = self.node[2][self.next]
            self.unknown(child[0], child)
            self.next += 1
        for name in self.node[1]:
            if name not in self.used: self.unknown(name)
        for count, rule in zip(self.counts, self.contract['wildcards']):
            if count < rule['minimum'] or rule['maximum'] is not None and count > rule['maximum']:
                raise ValueError('XML wildcard occurrence constraint failed')
    def state(self):
        """Retain semantic extension slots and QName bindings independently of the original tree owner."""
        if self.context is not None: self.context.consume('decoder_allocation_bytes', len(self.node[4]) * 64)
        return (dict(self.node[4]), self.attributes, self.children_kept)

def _xml_state_open(model, node, budget):
    """Restore XML-only namespace and attribute state without adding native codec fields."""
    state = getattr(model, '_srl_xml_state', None)
    if state is not None:
        for prefix, uri in state[0].items():
            if prefix and not _xml_ncname(prefix) or prefix == 'xmlns' or prefix == 'xml' and uri != 'http://www.w3.org/XML/1998/namespace' or prefix != 'xml' and uri == 'http://www.w3.org/XML/1998/namespace' or prefix and not uri:
                raise ValueError('Invalid XML namespace binding')
            budget.text(prefix)
            budget.text(uri)
        if budget.context is not None: budget.context.consume('decoder_allocation_bytes', len(state[0]) * 64)
        node[4] = dict(state[0])
        for name, value in state[1].items(): _xml_attribute(node, name, value, budget)
    return state

def _xml_extension_children(node, state, cursor, position, item, budget, depth, maximum=None):
    """Restore retained whole semantic subtrees at their declared sequence slot."""
    if state is not None:
        def inspect(child, level):
            """Account every retained semantic descendant before it reaches the XML output provider."""
            budget.node(child[0], level)
            for name, value in child[1].items():
                budget.text(name)
                budget.text(value)
            for prefix, uri in child[4].items():
                budget.text(prefix)
                budget.text(uri)
            budget.text(child[3])
            for descendant in child[2]: inspect(descendant, level + 1)
        while cursor[0] < len(state[2]):
            slot, before_item, child = state[2][cursor[0]]
            if budget.context is not None: budget.context.consume('work_units', 1)
            if slot < position or slot == position and before_item < item:
                raise ValueError('Invalid XML extension ordering')
            if slot == position and maximum is not None and not 0 <= before_item <= maximum:
                raise ValueError('XML choice extension position is outside its group')
            if slot != position or before_item != item: return
            inspect(child, depth + 1)
            node[2].append(child)
            cursor[0] += 1

def _xml_bind_namespace(model, prefix, uri):
    """Bind a lexical QName/prefix-list declaration in XML-only model state."""
    if model._srl_xml_state is None: model._srl_xml_state = ({}, {}, [])
    model._srl_xml_state[0][prefix] = uri

)richpython"
R"richpython(def _xml_promote_lexical(value, rule, source, target):
    """Retain expanded QName identities when a scalar child's local prefix declarations leave scope."""
    if rule['qname'] or rule['kind'] == 'QName':
        return _xml_qualify_qname(_xml_resolve_qname(value, source), target)
    if rule['list_item'] == 'QName':
        return ' '.join(_xml_qualify_qname(_xml_resolve_qname(token, source), target) for token in _xml_tokens(value))
    if rule['prefix_list']:
        result = []
        for prefix in _xml_tokens(value):
            uri = source[prefix]
            chosen = next((key for key, selected in target.items() if key and selected == uri), None)
            if chosen is None:
                index = 0
                while 'q' + str(index) in target: index += 1
                chosen = 'q' + str(index)
                target[chosen] = uri
            result.append(chosen)
        return ' '.join(result)
    return value

def _xml_choices(model, groups):
    """Validate singleton choice presence from the actual optional fields before publication or emission."""
    for fields, minimum, maximum in groups:
        count = sum(bool(getattr(model, field)) for field in fields)
        if count < minimum or count > maximum: raise ValueError('XML choice occurrence constraint failed')

def _xml_extensions_validate(node, contract, context):
    """Revalidate preserved extension positions/counts after known values have been edited."""
    reader = _XmlFieldsRich(node, True, contract, context)
    reader.used.update(contract['known_attributes'])
    for name in contract['known_children']: reader.children(name, 0, None)
    reader.finish()
)richpython"
