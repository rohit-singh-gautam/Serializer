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
    def __init__(self, limits=None):
        """Copy a validated immutable per-operation logical resource policy."""
        self.limits = limits or Limits()
        self.nodes, self.text_bytes, self.attributes = 0, 0, 0
    def node(self, name, depth):
        """Charge a node and its expanded name before adding descendants."""
        if depth > self.limits.max_depth or self.nodes >= self.limits.max_elements:
            raise ValueError('XML node or depth limit exceeded')
        self.nodes += 1
        self.text(name)
        return [name, {}, [], '']
    def text(self, value):
        """Charge cumulative UTF-8 text, names, namespaces and attribute values."""
        _xml_check_text(value)
        size = len(value.encode('utf-8'))
        if size > self.limits.max_string_bytes - self.text_bytes:
            raise ValueError('XML text limit exceeded')
        self.text_bytes += size
        return value

def _xml_name(uri, local):
    """Construct an expanded name independent of a document prefix."""
    return '{' + uri + '}' + local if uri else local

def _xml_parse(data, limits=None):
    """Parse exactly one bounded UTF-8 XML1.0 document without DTD/entity resolution."""
    budget = _XmlBudget(limits)
    if not isinstance(data, (bytes, bytearray)) or len(data) > budget.limits.max_bytes:
        raise ValueError('XML byte limit exceeded')
    data.decode('utf-8', 'strict')
    parser = _xml_expat.ParserCreate(encoding='UTF-8', namespace_separator='\x1f')
    stack, roots = [], []
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
        """Charge declarations even though fixed typed mappings need no retained prefix map."""
        nonlocal attributes
        attributes += 1
        if attributes > budget.limits.max_elements:
            raise ValueError('XML attribute limit exceeded')
        budget.text(prefix or '')
        budget.text(uri or '')
    def start(name, attrs):
        """Bound each element/attribute before linking it into the owning tree."""
        nonlocal attributes
        node = budget.node(expanded(name), len(stack) + 1)
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
            stack[-1][3] += value
        elif value.strip(' \t\r\n'):
            raise ValueError('XML text outside root')
    def end(name):
        """Reject mixed content before publishing the complete parsed tree."""
        node = stack.pop()
        if node[2] and node[3].strip(' \t\r\n'):
            raise ValueError('XML mixed content is unsupported')
    parser.XmlDeclHandler = declaration
    parser.StartNamespaceDeclHandler = namespace
    parser.StartElementHandler, parser.EndElementHandler = start, end
    parser.CharacterDataHandler = text
    parser.StartDoctypeDeclHandler = prohibited
    parser.EntityDeclHandler = prohibited
    parser.ExternalEntityRefHandler = prohibited
    parser.SetParamEntityParsing(_xml_expat.XML_PARAM_ENTITY_PARSING_NEVER)
    parser.Parse(data, True)
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
        return _bounds(int(value), width, unsigned)
    if value in ('INF', '-INF', 'NaN'):
        result = {'INF': _math.inf, '-INF': -_math.inf, 'NaN': _math.nan}[value]
    else:
        if not _XML_NUMBER.fullmatch(value):
            raise ValueError('Invalid XML floating value')
        result = float(value)
        if not _math.isfinite(result):
            raise ValueError('XML floating value overflows model')
    return _FLOAT.unpack(_FLOAT.pack(result))[0] if kind == 'float' else result

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

def _xml_write(root, limits):
    """Emit one deterministic escaped UTF-8 document within the complete byte bound."""
    prefixes, parts, size = {}, [], 0
    def uri_name(name):
        """Split only compiler-generated expanded names."""
        if name.startswith('{'):
            uri, local = name[1:].split('}', 1)
            return uri, local
        return '', name
    def inspect(node):
        """Collect only namespace URIs used by fixed element/attribute identities."""
        for name in [node[0], *node[1]]:
            uri, local = uri_name(name)
            if uri and uri not in prefixes:
                prefixes[uri] = 'xml' if uri == 'http://www.w3.org/XML/1998/namespace' else 'n' + str(len(prefixes))
        for child in node[2]:
            inspect(child)
    def append(text):
        """Check encoded length before retaining an escaped output fragment."""
        nonlocal size
        size += len(text.encode('utf-8'))
        if size > limits.max_bytes:
            raise ValueError('XML output byte limit exceeded')
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
        for name, value in node[1].items():
            append(' ' + qualified(name) + '="'); escaped(value, True); append('"')
        if not node[2] and not node[3]:
            append('/>'); return
        append('>'); escaped(node[3])
        for child in node[2]:
            emit(child)
        append('</' + qualified(node[0]) + '>')
    inspect(root)
    emit(root, True)
    return ''.join(parts).encode('utf-8')

class XmlSchema:
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
    def validate(self, data, limits=None):
        """Validate changing values using a prepared schema after bounded secure XML parsing."""
        _xml_parse(data, limits)
        tree = self._provider.fromstring(data, self._provider.XMLParser(resolve_entities=False, load_dtd=False, no_network=True))
        if not self._schema.validate(tree):
            raise ValueError('XML does not satisfy the selected XSD')
)xmlpython"
