// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlpython(
import xml.parsers.expat as _xml_expat

_XML_NUMBER = _re.compile(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z')
_XML_INTEGER = _re.compile(r'[+-]?[0-9]+\Z')

def _xml_check_text(value):
    """Require valid XML 1.0 Unicode without normalizing application whitespace."""
    if type(value) is not str:
        raise ValueError('XML requires string values')
    for character in value:
        code = ord(character)
        if not (code in (9, 10, 13) or 0x20 <= code <= 0xd7ff or 0xe000 <= code <= 0xfffd or 0x10000 <= code <= 0x10ffff):
            raise ValueError('Invalid XML 1.0 character')
    return value

class _XmlBudget:
    """Bound a complete tree before retaining model or parser text and nodes."""
    def __init__(self, limits=None, context=None):
        """Copy a validated immutable per-operation logical resource policy."""
        self.limits = limits or Limits()
        self.context = context
        self.nodes, self.text_bytes, self.attributes = 0, 0, 0
        self.fragments = 0
    def node(self, name, depth):
        """Charge a node and its expanded name before adding descendants."""
        if depth > self.limits.max_depth or self.nodes >= self.limits.max_elements:
            raise ValueError('XML node or depth limit exceeded')
        self.nodes += 1
        if self.context is not None:
            self.context.consume('work_units', 1)
            self.context.consume('decoder_allocation_bytes', 64)
        self.text(name)
        return [name, {}, [], '', {}, []]
    def text(self, value):
        """Charge cumulative UTF-8 text, names, namespaces and attribute values."""
        _xml_check_text(value)
        size = len(value.encode('utf-8'))
        if size > self.limits.max_string_bytes - self.text_bytes:
            raise ValueError('XML text limit exceeded')
        self.text_bytes += size
        if self.context is not None:
            self.context.consume('work_units', size)
            self.context.consume('decoder_allocation_bytes', size)
        return value

def _xml_name(uri, local):
    """Construct an expanded name independent of a document prefix."""
    return '{' + uri + '}' + local if uri else local

def _xml_parse(data, limits=None, context=None, original_size=None):
    """Parse exactly one bounded UTF-8 XML1.0 document without DTD/entity resolution."""
    budget = _XmlBudget(limits, context)
    if not isinstance(data, (bytes, bytearray)) or (len(data) if original_size is None else original_size) > budget.limits.max_bytes:
        raise ValueError('XML byte limit exceeded')
    if data[:2] in (b'\xff\xfe', b'\xfe\xff') or b'\x00' in data[:4]:
        raise ValueError('XML1.0 UTF-8 required')
    parser = _xml_expat.ParserCreate(encoding='UTF-8', namespace_separator='\x1f')
    stack, roots, scopes, declarations = [], [], [], {}
    attributes = 0
    def expanded(value):
        """Normalize provider namespace names to the generated expanded-name spelling."""
        parts = value.split('\x1f')
        return _xml_name(*parts) if len(parts) == 2 else value
    def prohibited(*unused):
        """Reject resource-loading and entity constructs before provider expansion."""
        raise ValueError('XML DTDs and entities are prohibited')
    def declaration(version, encoding, standalone):
        """Only the declared XML1.0 UTF-8 contract is supported."""
        if version != '1.0' or (encoding and encoding.lower().replace('-', '') != 'utf8'):
            raise ValueError('XML1.0 UTF-8 required')
    def namespace(prefix, uri):
        """Bound each declaration before retaining the prefix map used by lexical QName values."""
        nonlocal attributes
        attributes += 1
        if attributes > budget.limits.max_elements:
            raise ValueError('XML attribute limit exceeded')
        budget.text(prefix or '')
        budget.text(uri or '')
        declarations[prefix or ''] = uri or ''
    def start(name, attrs):
        """Bound each element/attribute before linking it into the owning tree."""
        nonlocal attributes
        node = budget.node(expanded(name), len(stack) + 1)
        inherited = scopes[-1] if scopes else {'xml': 'http://www.w3.org/XML/1998/namespace'}
        for prefix, uri in inherited.items():
            budget.text(prefix)
            budget.text(uri)
        if context is not None:
            context.consume('decoder_allocation_bytes', 64 * (len(inherited) + len(declarations)))
        scope = dict(inherited)
        scope.update(declarations)
        declarations.clear()
        node[4] = scope
        scopes.append(scope)
        attributes += len(attrs)
        if attributes > budget.limits.max_elements:
            raise ValueError('XML attribute limit exceeded')
        for key, value in attrs.items():
            node[1][budget.text(expanded(key))] = budget.text(value)
        if stack:
            stack[-1][2].append(node)
        else:
            roots.append(node)
        stack.append(node)
    def text(value):
        """Retain only cumulatively bounded character data."""
        budget.text(value)
        if stack:
            if value:
                budget.fragments += 1
                if budget.fragments > budget.limits.max_elements:
                    raise ValueError('XML text fragment limit exceeded')
                if context is not None: context.consume('decoder_allocation_bytes', 64)
                stack[-1][5].append((len(stack[-1][2]), value))
        elif value.strip(' \t\r\n'):
            raise ValueError('XML text outside root')
    def end(name):
        """Join bounded character fragments while retaining their semantic mixed-content positions."""
        node = stack.pop()
        if context is not None:
            context.consume('decoder_allocation_bytes', sum(len(value.encode('utf-8')) for unused, value in node[5]))
        node[3] = ''.join(value for unused, value in node[5])
        scopes.pop()
    parser.XmlDeclHandler = declaration
    parser.StartNamespaceDeclHandler = namespace
    parser.StartElementHandler, parser.EndElementHandler = start, end
    parser.CharacterDataHandler = text
    parser.StartDoctypeDeclHandler = prohibited
    parser.EntityDeclHandler = prohibited
    parser.ExternalEntityRefHandler = prohibited
    parser.SetParamEntityParsing(_xml_expat.XML_PARAM_ENTITY_PARSING_NEVER)
    for offset in range(0, len(data), 65536):
        if context is not None:
            context.consume('work_units', min(65536, len(data) - offset))
        parser.Parse(memoryview(data)[offset:offset + 65536], False)
    parser.Parse(b'', True)
    if len(roots) != 1 or stack:
        raise ValueError('XML requires exactly one root')
    return roots[0]

def _xml_scalar(value, kind, width=0, unsigned=False, encode=False):
    """Convert a strict XML lexical value and check the model's fixed-width range."""
    if kind == 'string':
        return _xml_check_text(value)
    if encode:
        if kind == 'bool':
            if type(value) is not bool:
                raise ValueError('Invalid XML Boolean')
            return 'true' if value else 'false'
        if kind == 'integer':
            return str(_bounds(value, width, unsigned))
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise ValueError('Invalid XML floating value')
        value = float(value)
        if kind == 'float':
            value = _FLOAT.unpack(_FLOAT.pack(value))[0]
        return 'NaN' if _math.isnan(value) else '-INF' if value == -_math.inf else 'INF' if value == _math.inf else repr(value)
    value = value.strip(' \t\r\n')
    if kind == 'bool':
        if value not in ('true', 'false', '1', '0'):
            raise ValueError('Invalid XML Boolean')
        return value in ('true', '1')
    if kind == 'integer':
        if not _XML_INTEGER.fullmatch(value):
            raise ValueError('Invalid XML integer')
        negative = value.startswith('-')
        digits = value[1:] if value[0] in '+-' else value
        digits = digits.lstrip('0') or '0'
        if len(digits) > len(str(1 << (width * 8))):
            raise ValueError('XML integer overflows model')
        number = int(digits)
        return _bounds(-number if negative else number, width, unsigned)
    return _xml_floating(value, kind)

def _xml_enum(value, names, enumeration):
    """Reject undeclared enum ordinals before indexing the fixed schema name table."""
    if not isinstance(value, enumeration) or not 0 <= int(value) < len(names):
        raise ValueError('Invalid XML enum')
    return names[int(value)]

def _xml_read_scalar(node):
    """A scalar field cannot silently discard attributes or nested elements."""
    if node[1] or node[2]:
        raise ValueError('XML scalar contains attributes or children')
    return node[3]

def _xml_attribute(node, name, value, budget):
    """Add one bounded fixed attribute without losing its namespace identity."""
    if budget.attributes >= budget.limits.max_elements:
        raise ValueError('XML attribute limit exceeded')
    if name in node[1]:
        raise ValueError('Duplicate XML attribute')
    budget.attributes += 1
    node[1][budget.text(name)] = budget.text(value)

class _XmlFields:
    """Consume exact attributes and ordered children from one declared record."""
    def __init__(self, node, has_text=False):
        """Reject record text while preserving insignificant indentation."""
        if not has_text and node[3].strip(' \t\r\n'):
            raise ValueError('Unexpected XML record text')
        self.node, self.used, self.next = node, set(), 0
    def text(self):
        """Read simple content while rejecting all element children."""
        if self.node[2]:
            raise ValueError('XML simple content contains children')
        return self.node[3]
    def attribute(self, name, optional):
        """Read one exact required or optional attribute without coercing absence."""
        if name not in self.node[1]:
            if optional:
                return None
            raise ValueError('Required XML attribute missing')
        if name in self.used:
            raise ValueError('Duplicate XML attribute mapping')
        self.used.add(name)
        return self.node[1][name]
    def children(self, name, minimum, maximum):
        """Take only contiguous occurrences in the statically selected sequence."""
        start = self.next
        while self.next < len(self.node[2]) and self.node[2][self.next][0] == name:
            self.next += 1
        count = self.next - start
        if count < minimum or (maximum is not None and count > maximum):
            raise ValueError('XML field occurrence mismatch')
        return self.node[2][start:self.next]
    def wrapped(self, wrapper, name, minimum, maximum):
        """Require one wrapper and consume its complete ordered item sequence."""
        fields = _XmlFields(self.children(wrapper, 1, 1)[0])
        result = fields.children(name, minimum, maximum)
        fields.finish()
        return result
    def finish(self):
        """Reject unknown, duplicate, or reordered members before publishing a model."""
        if len(self.used) != len(self.node[1]) or self.next != len(self.node[2]):
            raise ValueError('Unexpected XML attribute or child')

def _xml_write(root, limits, context=None):
    """Emit one deterministic escaped UTF-8 document within the complete byte bound."""
    prefixes, parts, size, reserved = {}, [], 0, set()
    def uri_name(name):
        """Split only compiler-generated expanded names."""
        if name.startswith('{'):
            uri, local = name[1:].split('}', 1)
            return uri, local
        return '', name
    def inspect(node):
        """Collect only namespace URIs used by fixed element/attribute identities."""
        reserved.update(node[4])
        for child in node[2]:
            inspect(child)
    def collect(node):
        """Avoid every retained lexical QName prefix when allocating element-name presentation prefixes."""
        for name in [node[0], *node[1]]:
            uri, local = uri_name(name)
            if uri and uri not in prefixes:
                index = len(prefixes)
                while 'n' + str(index) in reserved: index += 1
                prefixes[uri] = 'xml' if uri == 'http://www.w3.org/XML/1998/namespace' else 'n' + str(index)
        for child in node[2]:
            collect(child)
    def append(text):
        """Check encoded length before retaining an escaped output fragment."""
        nonlocal size
        size += len(text.encode('utf-8'))
        if size > limits.max_bytes:
            raise ValueError('XML output byte limit exceeded')
        if context is not None:
            context.consume('work_units', len(text))
            context.check_buffer(size * 2)
        parts.append(text)
    def escaped(text, attribute=False):
        """Preserve normalized XML whitespace through explicit character references."""
        begin = 0
        for index, character in enumerate(text):
            replacement = {'&': '&amp;', '<': '&lt;', '>': '&gt;', '\r': '&#13;'}.get(character,
                          '&quot;' if attribute and character == '"' else '&#10;' if attribute and character == '\n' else '&#9;' if attribute and character == '\t' else None)
            if replacement is not None:
                append(text[begin:index]); append(replacement); begin = index + 1
        append(text[begin:])
    def qualified(name):
        """Choose a presentation prefix without changing expanded-name identity."""
        uri, local = uri_name(name)
        return prefixes[uri] + ':' + local if uri else local
    def emit(node, is_root=False):
        """Serialize one bounded data-XML node in declared field order."""
        append('<' + qualified(node[0]))
        if is_root:
            for uri, prefix in prefixes.items():
                if prefix != 'xml':
                    append(' xmlns:' + prefix + '="'); escaped(uri, True); append('"')
        for prefix, uri in node[4].items():
            if prefix != 'xml':
                append(' xmlns' + (':' + prefix if prefix else '') + '="'); escaped(uri, True); append('"')
        for name, value in node[1].items():
            append(' ' + qualified(name) + '="'); escaped(value, True); append('"')
        if not node[2] and not node[3]:
            append('/>'); return
        append('>')
        if node[5]:
            text = {}
            for position, value in node[5]: text.setdefault(position, []).append(value)
            for position in range(len(node[2]) + 1):
                for value in text.get(position, ()): escaped(value)
                if position < len(node[2]): emit(node[2][position])
        else:
            escaped(node[3])
            for child in node[2]: emit(child)
        append('</' + qualified(node[0]) + '>')
    inspect(root)
    collect(root)
    emit(root, True)
    return ''.join(parts).encode('utf-8')

)xmlpython"
R"xmlpython(class XmlSchema:
    """Optional prepared runtime XSD1.0 validator; constructing it requires lxml."""
    def __init__(self, xsd, limits=None):
        """Compile a trusted standalone schema; external schema references are prohibited."""
        tree = _xml_parse(xsd, limits)
        def inspect(node):
            """Reject provider loading hooks before calling the optional validator."""
            if node[0] in ('{http://www.w3.org/2001/XMLSchema}' + kind for kind in ('include', 'import', 'redefine', 'override')):
                raise ValueError('Runtime XSD external references are unsupported')
            for child in node[2]:
                inspect(child)
        inspect(tree)
        try:
            from lxml import etree
        except ImportError as error:
            raise RuntimeError('Runtime XML schema validation requires lxml') from error
        self._provider = etree
        self._schema = etree.XMLSchema(etree.fromstring(xsd, etree.XMLParser(resolve_entities=False, load_dtd=False, no_network=True)))
    def validate(self, data, limits=None, context=None):
        """Validate changing values using a prepared schema after bounded secure XML parsing."""
        _xml_parse(data, limits, context)
        if context is not None: context.consume('decoder_allocation_bytes', len(data) * 2)
        tree = self._provider.fromstring(data, self._provider.XMLParser(resolve_entities=False, load_dtd=False, no_network=True))
        if not self._schema.validate(tree):
            raise ValueError('XML does not satisfy the selected XSD')
        if context is not None: context.check()
)xmlpython"
