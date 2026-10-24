// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlpython(
import xml.parsers.expat as _xml_expat

_XML_NUMBER = _re.compile(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z')
_XML_INTEGER = _re.compile(r'[+-]?[0-9]+\Z')
_XML_DIAGNOSTIC_CONTEXT_BYTES = 64


class _XmlFieldContext:
    """Borrow optional package diagnostics without changing ordinary XML callback resources."""
    __slots__ = ('context', '_xml_field')
    def __init__(self, context, field):
        """Retain only a bounded linked path of generated member names, never model values."""
        self.context, self._xml_field = context, field
    def __getattr__(self, name):
        """Preserve optional pattern, namespace, extension and caller policies."""
        if name == 'retain_namespace':
            retain = getattr(self.context, '_xml_retain_namespace_at', None)
            if retain is not None:
                return lambda bindings: retain(bindings, self)
        if name == 'retain_namespace_addition':
            retain = getattr(self.context, '_xml_retain_namespace_addition_at', None)
            if retain is not None:
                return lambda prefix, uri: retain(prefix, uri, self)
        return getattr(self.context, name)
    def consume(self, resource, amount):
        """Charge through the original operation with this field's logical location."""
        self.context._xml_consume_at(resource, amount, self._xml_field)
    def check(self):
        """Keep cancellation and deadline diagnostics attached to the active field."""
        self.context._xml_check_at(self._xml_field)
    def check_buffer(self, amount):
        """Apply the shared simultaneous-buffer bound at the active field."""
        self.context._xml_buffer_at(amount, self._xml_field)
    def admit_reference(self):
        """Admit one complete identity tuple before retaining an index entry."""
        self.context._xml_reference_at(self._xml_field)


def _xml_field_context(context, member):
    """Add diagnostics only for an opted-in bridge; ordinary callbacks retain their fast path."""
    callback = getattr(context, '_xml_consume_at', None)
    if callback is None or not member:
        return context
    field = (member, getattr(context, '_xml_field', None))
    callback('decoder_allocation_bytes', _XML_DIAGNOSTIC_CONTEXT_BYTES, field)
    context._xml_check_at(field)
    return _XmlFieldContext(context, field)


def _xml_limit(context, message, requested=0, available=0):
    """Classify explicit native/provider bounds before leaving their active field scope."""
    callback = getattr(context, '_xml_limit_at', None)
    if callback is not None:
        callback(message, requested, available, getattr(context, '_xml_field', None))
    raise ValueError(message)


def _xml_node_context(node, context, attribute=None):
    """Replay optional immutable field provenance without allocating on ordinary XML paths."""
    if len(node) > 6:
        return node[6][1].get(attribute, node[6][0]) if attribute is not None else node[6][0]
    return context

)xmlpython"
R"xmlpython(def _xml_check_text(value):
    """Require valid XML 1.0 Unicode without normalizing application whitespace."""
    if type(value) is not str:
        raise ValueError('XML requires string values')
    for character in value:
        code = ord(character)
        if not (code in (9, 10, 13) or 0x20 <= code <= 0xd7ff or 0xe000 <= code <= 0xfffd or 0x10000 <= code <= 0x10ffff):
            raise ValueError('Invalid XML 1.0 character')
    return value

class _XmlBudget:
    """Bound a complete XML operation before retaining models, lexical fragments or nodes."""
    def __init__(self, limits=None, context=None):
        """Copy a validated immutable per-operation logical resource policy."""
        self.limits = limits or Limits()
        self.context = _xml_pattern_context(self.limits, context)
        self.nodes, self.text_bytes, self.attributes = 0, 0, 0
        self.fragments = 0
    def node(self, name, depth):
        """Charge a node and its expanded name before adding descendants."""
        self.element(name, depth)
        node = [name, {}, [], '', {}, []]
        if getattr(self.context, '_xml_consume_at', None) is not None:
            self.context.consume('decoder_allocation_bytes', _XML_DIAGNOSTIC_CONTEXT_BYTES)
            node.append([self.context, {}])
        return node
    def element(self, name, depth):
        """Charge an element without allocating a retained XML node."""
        if depth > self.limits.max_depth or self.nodes >= self.limits.max_elements:
            _xml_limit(self.context, 'XML node or depth limit exceeded')
        self.nodes += 1
        if self.context is not None:
            self.context.consume('work_units', 1)
            self.context.consume('decoder_allocation_bytes', 64)
        self.text(name)
    def text(self, value):
        """Charge cumulative UTF-8 text, names, namespaces and attribute values."""
        _xml_check_text(value)
        size = len(value.encode('utf-8'))
        if size > self.limits.max_string_bytes - self.text_bytes:
            _xml_limit(self.context, 'XML text limit exceeded')
        self.text_bytes += size
        if self.context is not None:
            self.context.consume('work_units', size)
            self.context.consume('decoder_allocation_bytes', size)
        return value

def _xml_charge_scope_copy(bindings, context):
    """Account retained scope copies without recounting logical XML declarations as text."""
    if context is not None:
        for prefix, uri in bindings.items():
            size = len(prefix.encode('utf-8')) + len(uri.encode('utf-8'))
            context.consume('work_units', size)
            context.consume('decoder_allocation_bytes', size)


def _xml_name(uri, local):
    """Construct an expanded name independent of a document prefix."""
    return '{' + uri + '}' + local if uri else local


class _XmlPackageCapabilityError(ValueError):
    """Identify package-owned capability validation without translating caller exceptions."""


def _xml_package_root(attributes, scope, context):
    """Consume only an authorized package root capability marker in its original namespace scope."""
    understood = getattr(context, '_xml_package_vocabularies', None)
    if understood is None:
        return
    if getattr(context, '_xml_package_requires_type', False) and '{http://www.w3.org/2001/XMLSchema-instance}type' not in attributes:
        raise _XmlPackageCapabilityError('Required package XML type is missing')
    selected = '{http://schemas.openxmlformats.org/markup-compatibility/2006}MustUnderstand'
    if selected not in attributes:
        return
    lexical, count = attributes[selected], 0
    context.consume('work_units', len(lexical))
    for token in _re.finditer(r'[^ \t\r\n]+', lexical):
        context.consume('decoder_allocation_bytes', token.end() - token.start())
        prefix = token.group()
        context.consume('work_units', len(prefix) + 1)
        if not _xml_ncname(prefix) or prefix not in scope or not scope[prefix]:
            raise _XmlPackageCapabilityError('Invalid or unbound package capability prefix')
        uri = scope[prefix]
        found = False
        for candidate in understood:
            context.consume('work_units', len(uri) + len(candidate))
            if uri == candidate:
                found = True
                break
        if not found:
            raise _XmlPackageCapabilityError('Required package vocabulary is not understood')
        count += 1
    if not count:
        raise _XmlPackageCapabilityError('Empty package capability declaration')
    attributes.pop(selected)

def _xml_parse(data, limits=None, context=None, original_size=None):
    """Parse exactly one bounded UTF-8 XML1.0 document without DTD/entity resolution."""
    budget = _XmlBudget(limits, context)
    if not isinstance(data, (bytes, bytearray)):
        raise ValueError('XML requires UTF-8 bytes')
    if (len(data) if original_size is None else original_size) > budget.limits.max_bytes:
        _xml_limit(budget.context, 'XML byte limit exceeded')
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
            _xml_limit(budget.context, 'XML attribute limit exceeded')
        budget.text(prefix or '')
        budget.text(uri or '')
        declarations[prefix or ''] = uri or ''
    def start(name, attrs):
        """Bound each element/attribute before linking it into the owning tree."""
        nonlocal attributes
        budget.element(expanded(name), len(stack) + 1)
        node = [expanded(name), {}, [], '', {}, []]
        inherited = scopes[-1] if scopes else {'xml': 'http://www.w3.org/XML/1998/namespace'}
        _xml_charge_scope_copy(inherited, budget.context)
        if context is not None:
            context.consume('decoder_allocation_bytes', 64 * (len(inherited) + len(declarations)))
        scope = dict(inherited)
        scope.update(declarations)
        declarations.clear()
        node[4] = scope
        scopes.append(scope)
        attributes += len(attrs)
        if attributes > budget.limits.max_elements:
            _xml_limit(budget.context, 'XML attribute limit exceeded')
        for key, value in attrs.items():
            node[1][budget.text(expanded(key))] = budget.text(value)
        if not stack:
            _xml_package_root(node[1], scope, context)
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
                    _xml_limit(budget.context, 'XML text fragment limit exceeded')
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
        _xml_limit(budget.context, 'XML attribute limit exceeded')
    if name in node[1]:
        raise ValueError('Duplicate XML attribute')
    budget.attributes += 1
    node[1][budget.text(name)] = budget.text(value)
    if len(node) > 6:
        budget.context.consume('decoder_allocation_bytes', _XML_DIAGNOSTIC_CONTEXT_BYTES)
        node[6][1][name] = budget.context

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
    active_context = context
    def uri_name(name):
        """Split only compiler-generated expanded names."""
        if name.startswith('{'):
            uri, local = name[1:].split('}', 1)
            return uri, local
        return '', name
    def inspect(node):
        """Collect only namespace URIs used by fixed element/attribute identities."""
        nonlocal active_context
        previous, active_context = active_context, _xml_node_context(node, context)
        reserved.update(node[4])
        # A new element prefix must not bind a previously unbound QName-looking
        # union string and silently select an earlier QName alternative on reload.
        for name, value in [(None, node[3]), *node[1].items()]:
            selected = _xml_node_context(node, active_context, name)
            if selected is not None: selected.consume('work_units', len(value))
            reserved.update(match.group(1) for match in _re.finditer(r'(n[0-9]+):', value))
        for child in node[2]:
            inspect(child)
        active_context = previous
    def collect(node):
        """Avoid every retained lexical QName prefix when allocating element-name presentation prefixes."""
        for name in [node[0], *node[1]]:
            uri, local = uri_name(name)
            if uri and uri not in prefixes:
                index = len(prefixes)
                while 'n' + str(index) in reserved: index += 1
                prefixes[uri] = 'xml' if uri == 'http://www.w3.org/XML/1998/namespace' else 'n' + str(index)
                reserved.add(prefixes[uri])
        for child in node[2]:
            collect(child)
    def append(text):
        """Check encoded length before retaining an escaped output fragment."""
        nonlocal size
        size += len(text.encode('utf-8'))
        if size > limits.max_bytes:
            _xml_limit(active_context, 'XML output byte limit exceeded')
        if active_context is not None:
)xmlpython"
R"xmlpython(            active_context.consume('work_units', len(text))
            active_context.check_buffer(size * 2)
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
        nonlocal active_context
        previous, active_context = active_context, _xml_node_context(node, context)
        append('<' + qualified(node[0]))
        if is_root:
            for uri, prefix in prefixes.items():
                if prefix != 'xml':
                    append(' xmlns:' + prefix + '="'); escaped(uri, True); append('"')
        for prefix, uri in node[4].items():
            if prefix != 'xml':
                append(' xmlns' + (':' + prefix if prefix else '') + '="'); escaped(uri, True); append('"')
        for name, value in node[1].items():
            active_context = _xml_node_context(node, context, name)
            append(' ' + qualified(name) + '="'); escaped(value, True); append('"')
        active_context = _xml_node_context(node, context)
        if not node[2] and not node[3]:
            append('/>'); active_context = previous; return
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
        active_context = previous
    inspect(root)
    collect(root)
    emit(root, True)
    return ''.join(parts).encode('utf-8')

)xmlpython"
R"xmlpython(def _xml_token_capable(model, active=None, scoped=False):
    """Check the complete generated graph before selecting the tree-free codec."""
    if not model._xml_token_supported:
        return False
    if active is None:
        scoped = _xml_token_required(model)
        active = set()
    if getattr(model, '_xml_token_root_namespace', False) and not scoped:
        return False
    if model in active:
        return True
    active.add(model)
    return all(_xml_token_capable(selected(), active, scoped)
               for item in model._xml_token_fields for selected in _xml_token_models(item)) and (
                   model._xml_mixed is None or all(_xml_token_capable(item['model'](), active, scoped) for item in model._xml_mixed['children']))


def _xml_tree_capable(model, active=None):
    """Admit only complete graphs implemented by the established bounded owning-tree adapter."""
    if not getattr(model, '_xml_tree_supported', False):
        return False
    active = set() if active is None else active
    if model in active:
        return True
    active.add(model)
    return all(_xml_tree_capable(selected(), active)
               for item in model._xml_token_fields for selected in _xml_token_models(item))


def _xml_token_has_namespace(model, active=None):
    """Select one shared lexical retention ledger only for graphs that need root scalar scopes."""
    active = set() if active is None else active
    if model in active:
        return False
    active.add(model)
    return getattr(model, '_xml_token_root_namespace', False) or any(
        _xml_token_has_namespace(selected(), active)
        for item in model._xml_token_fields for selected in _xml_token_models(item)) or (
            model._xml_mixed is not None and any(_xml_token_has_namespace(item['model'](), active)
                                               for item in model._xml_mixed['children']))


def _xml_token_models(item):
    """Enumerate the complete owning child graph, including mapped container payloads."""
    if item['map'] is not None:
        selected = item['map']['value']['model']
        return () if selected is None else (selected,)
    if item.get('variant') is not None:
        return tuple(selected['model'] for selected in item['variant']['alternatives'] if selected['model'] is not None)
    return () if item['model'] is None else (item['model'],)


def _xml_token_required(model, active=None):
    """Prevent advanced token contracts from entering a legacy path that cannot enforce them."""
    active = set() if active is None else active
    if model in active:
        return False
    active.add(model)
    return bool(model._xml_identity or model._xml_token_all or model._xml_token_inherited or model._xml_mixed or model._xml_model['required'] or model._xml_extensions) or any(item['presence'] is not None or item['map'] is not None or item.get('variant') is not None for item in model._xml_token_fields) or any(
        _xml_token_required(selected(), active)
        for item in model._xml_token_fields for selected in _xml_token_models(item))


class XmlMapLimits:
    """Bound native-key admission separately from whole-document XML token accounting."""
    def __init__(self, max_entries=1024, max_key_bytes=1024*1024):
        """Validate explicit nonnegative entry and cumulative lexical/canonical key-byte bounds."""
        if type(max_entries) is not int or max_entries < 0 or type(max_key_bytes) is not int or max_key_bytes < 0:
            raise ValueError('Invalid XML map limit')
        self.max_entries, self.max_key_bytes = max_entries, max_key_bytes


class XmlMixedLimits:
    """Bound the complete owning ordered mixed fragment sequence and cumulative UTF-8 text."""
    def __init__(self, max_fragments=100000, max_text_bytes=8*1024*1024):
        """Treat zero as a real admission bound and permit exact-boundary input/output."""
        if type(max_fragments) is not int or max_fragments < 0 or type(max_text_bytes) is not int or max_text_bytes < 0:
            raise ValueError('Invalid XML mixed-content limit')
        self.max_fragments, self.max_text_bytes = max_fragments, max_text_bytes


class _XmlTokenOutput:
    """Write escaped lexical tokens directly, retaining only output bytes and open names."""
    def __init__(self, limits, context):
        """Share cumulative logical budgets across every generated nested record."""
        self.budget = _XmlBudget(limits, context)
        self.parts, self.size, self.stack, self.prefixes = [], 0, [], {}
        self.identities = []
        self.scopes = []
    def append(self, value):
        """Bound each escaped UTF-8 fragment before adding it to the final message."""
        data = value.encode('utf-8')
        if len(data) > self.budget.limits.max_bytes - self.size:
            _xml_limit(self.budget.context, 'XML output byte limit exceeded')
        self.size += len(data)
        if self.budget.context is not None:
            self.budget.context.consume('work_units', len(value))
            self.budget.context.check_buffer(self.size * 2)
            if getattr(self.budget.context, '_xml_validation', False):
                self.budget.context.consume('decoder_allocation_bytes', len(data) * 2)
        self.parts.append(data)
    def escaped(self, value, attribute=False):
        """Escape without losing XML attribute whitespace or carriage returns."""
        _xml_check_text(value)
        begin = 0
        for index, character in enumerate(value):
            replacement = {'&': '&amp;', '<': '&lt;', '>': '&gt;', '\r': '&#13;'}.get(character)
            if attribute:
                replacement = {'"': '&quot;', '\n': '&#10;', '\t': '&#9;'}.get(character, replacement)
            if replacement is not None:
                self.append(value[begin:index])
                self.append(replacement)
                begin = index + 1
        self.append(value[begin:])
    def qualified(self, name, declarations, attribute=False):
        """Reuse preserved namespace scopes before allocating an unbound presentation prefix."""
        if not name.startswith('{'):
            return name
        uri, local = name[1:].split('}', 1)
        if uri == 'http://www.w3.org/XML/1998/namespace':
            return 'xml:' + local
        inherited = self.scopes[-1] if self.scopes else {}
        available = dict(inherited)
        available.update(declarations)
        prefix = next((key for key, value in available.items() if value == uri and (key or not attribute)), None)
        if prefix is None:
            cached = self.prefixes.get(uri)
            if cached is not None and (cached or not attribute) and cached not in available:
                prefix = cached
        if prefix is None:
            suffix = len(self.prefixes)
            while 'n' + str(suffix) in available or 'n' + str(suffix) in self.prefixes.values():
                suffix += 1
            prefix = 'n' + str(suffix)
        self.prefixes[uri] = prefix
        if declarations.get(prefix, self.scopes[-1].get(prefix) if self.scopes else None) != uri:
            declarations[prefix] = uri
        return prefix + ':' + local if prefix else local
    def start(self, name, attributes=(), bindings=(), attribute_contexts=None):
        """Emit one complete start token after checking its attributes and element bound."""
        parent_context = self.budget.context
        self.budget.element(name, len(self.stack) + 1)
        declarations = dict(bindings)
        _xml_extension_bindings(declarations)
        if not name.startswith('{') and declarations.get('', self.scopes[-1].get('') if self.scopes else ''):
            declarations[''] = ''
        qualified = self.qualified(name, declarations)
        selected, used = [], set()
        selected_contexts = [] if attribute_contexts is not None else None
        for key, value in attributes:
            self.budget.context = attribute_contexts.get(key, parent_context) if attribute_contexts else parent_context
            if key in used:
                raise ValueError('Duplicate XML attribute')
            used.add(key)
            self.budget.attributes += 1
            if self.budget.attributes > self.budget.limits.max_elements:
                _xml_limit(self.budget.context, 'XML attribute limit exceeded')
            self.budget.text(key)
            self.budget.text(value)
            selected.append((self.qualified(key, declarations, True), value))
            if selected_contexts is not None: selected_contexts.append(self.budget.context)
        self.budget.context = parent_context
        for identity in self.identities:
            scope = dict(self.scopes[-1]) if self.scopes else {}
            scope.update(declarations)
            identity.start(name, dict(attributes), scope)
        self.append('<' + qualified)
        for prefix, uri in declarations.items():
            self.budget.attributes += 1
            if self.budget.attributes > self.budget.limits.max_elements:
                _xml_limit(self.budget.context, 'XML attribute limit exceeded')
            self.budget.text(prefix)
            self.budget.text(uri)
            self.append(' xmlns' + (':' + prefix if prefix else '') + '="')
            self.escaped(uri, True)
            self.append('"')
        for index, (key, value) in enumerate(selected):
            self.budget.context = selected_contexts[index] if selected_contexts is not None else parent_context
            self.append(' ' + key + '="')
            self.escaped(value, True)
            self.append('"')
        self.budget.context = parent_context
        self.append('>')
        self.stack.append(qualified)
        scope = dict(self.scopes[-1]) if self.scopes else {}
        scope.update(declarations)
        self.scopes.append(scope)
    def text(self, value):
        """Emit one checked scalar/text token without retaining a semantic XML node."""
        self.budget.text(value)
        for identity in self.identities:
            identity.text(value)
        self.escaped(value)
    def end(self):
        """Close the most recent generated start token."""
        for identity in self.identities:
            identity.end()
        self.append('</' + self.stack.pop() + '>')
        self.scopes.pop()


_XML_TOKEN_NIL = object()
_XML_TOKEN_NIL_NAME = '{http://www.w3.org/2001/XMLSchema-instance}nil'
_XML_TOKEN_TYPE_NAME = '{http://www.w3.org/2001/XMLSchema-instance}type'


def _xml_token_get(model, path):
    """Read a generated direct member or existing composed base-member storage path."""
    if '.' not in path:
        return getattr(model, path)
    for member in path.split('.'):
        model = getattr(model, member)
    return model


def _xml_token_set(model, path, value):
    """Set only the existing leaf slot while preserving the owning model's base-object layout."""
    if '.' not in path:
        setattr(model, path, value)
    else:
        owner, unused, member = path.rpartition('.')
        setattr(_xml_token_get(model, owner), member, value)


def _xml_token_values(model, item):
    """Validate native optional storage and expose only its present ordinary values."""
    value = _xml_token_get(model, item['member'])
    if item['optional']:
        if not isinstance(value, tuple) or len(value) > 1:
            raise ValueError('Invalid XML optional')
        if not value:
            return (_XML_TOKEN_NIL,) if item['presence'] == 'nil' else ()
        value = value[0]
        if item['presence'] == 'absent_or_nil':
            if not isinstance(value, tuple) or len(value) > 1:
                raise ValueError('Invalid XML optional')
            if not value:
                return (_XML_TOKEN_NIL,)
            value = value[0]
    values = value if item['repeated'] else (value,)
    if item.get('value_projection'):
        values = _XmlReferenceValues(values)
    count = len(values)
    if count < item['minimum'] or item['maximum'] is not None and count > item['maximum']:
        raise ValueError('XML field occurrence mismatch')
    if item['extent'] and count != item['extent']:
        raise ValueError('XML fixed extent mismatch')
    return values


)xmlpython"
R"xmlpython(def _xml_token_encode(model, name, limits=None, context=None):
    """Traverse generated members directly into bounded escaped output tokens."""
    # Projection instances forward metadata; their wrapper class has none of the target graph.
    metadata = model._target if isinstance(model, _XmlProjectionAccess) else type(model)
    if _xml_token_has_namespace(metadata):
        context = _xml_namespace_context(limits, context)
    writer = _XmlTokenOutput(limits, context)
    context = writer.budget.context
    def payload(item, selected, attributes=(), bindings=()):
        """Write one statically typed scalar or owning record payload directly."""
        if item.get('projection_payload'):
            selected = selected.read_value()
        if item['model'] is not None:
            record(selected, item['name'], attributes, bindings)
        else:
            writer.start(item['name'], attributes, bindings)
            writer.text(item['write'](selected, {}, writer.budget.context))
            writer.end()
    def mapping(item, values):
        """Write a bounded map wrapper in native iteration order with exact typed keys."""
        if not values:
            return
        selected, meta = values[0], item['map']
        bounds = writer.budget.limits.xml_map or XmlMapLimits()
        if not isinstance(selected, dict) or len(selected) > bounds.max_entries:
            _xml_limit(writer.budget.context, 'XML map entry limit exceeded')
        writer.start(item['wrapper'])
        key_bytes = 0
        projected_keys = set() if meta['key'].get('projection_payload') else None
        for key, value in selected.items():
            if projected_keys is not None:
                key = key.read_value()
            lexical = meta['key']['write'](key, {}, writer.budget.context)
            if projected_keys is not None:
                canonical = meta['key']['read'](lexical, {}, writer.budget.context)
                if canonical in projected_keys:
                    raise ValueError('Duplicate XML map key')
                projected_keys.add(canonical)
            size = len(lexical.encode('utf-8'))
            if size > bounds.max_key_bytes - key_bytes:
                _xml_limit(writer.budget.context, 'XML map key-byte limit exceeded')
            key_bytes += size
            writer.start(meta['entry'], ((meta['key']['name'], lexical),) if meta['attribute'] else ())
            if not meta['attribute']:
                writer.start(meta['key']['name'])
                writer.text(lexical)
                writer.end()
            payload(meta['value'], value)
            writer.end()
        writer.end()
    def variant(value, item):
        """Write exactly one selected native alternative with its declared tag or QName inventory."""
        index = _xml_token_get(value, item['member'] + '_index')
        alternatives = item['variant']['alternatives']
        if type(index) is not int or not 0 <= index < len(alternatives):
            raise ValueError('Invalid XML variant alternative')
        selected = alternatives[index]
        native = _xml_token_get(value, selected['member'])
        writer.start(item['wrapper'])
        if item['variant']['selection'] == 'xsi_type':
            selected = dict(selected)
            selected['name'] = item['wrapper']
            uri, local = selected['type']
            prefix, suffix = 't', 0
            state = getattr(native, '_srl_xml_state', None) if selected['model'] is not None else None
            while state is not None and prefix in state[0] and state[0][prefix] != uri:
                suffix += 1
                prefix = 't' + str(suffix)
            bindings = {prefix: uri} if uri else {}
            payload(selected, native, ((_XML_TOKEN_TYPE_NAME, prefix + ':' + local if uri else local),), bindings)
        else:
            payload(selected, native)
        writer.end()
    def mixed(value, meta):
        """Visit every authored fragment in order, including empty and adjacent text values."""
        fragments = _xml_token_get(value, meta['member'])
        bounds = writer.budget.limits.xml_mixed or XmlMixedLimits()
        if len(fragments) > bounds.max_fragments:
            _xml_limit(writer.budget.context, 'XML mixed fragment limit exceeded')
        if meta['element_only'] and meta['extent'] and len(fragments) != meta['extent']:
            raise ValueError('XML ordered fixed extent mismatch')
        text_bytes, ordered_state = 0, 1
        for fragment in fragments:
            index = _xml_token_get(fragment, meta['index'])
            if type(index) is not int:
                raise ValueError('Invalid XML mixed alternative')
            if not meta['element_only'] and index == meta['text_index']:
                text = _xml_check_text(_xml_token_get(fragment, meta['text_member']))
                size = len(text.encode('utf-8'))
                if size > bounds.max_text_bytes - text_bytes:
                    _xml_limit(writer.budget.context, 'XML mixed text limit exceeded')
                text_bytes += size
                writer.text(text)
            else:
                selected = next((item for item in meta['children'] if item['alternative'] == index), None)
                if selected is None:
                    raise ValueError('Invalid XML mixed alternative')
                if meta['grammar']:
                    ordered_state = meta['grammar'][ordered_state][index]
                    if not ordered_state:
                        raise ValueError('Ordered XSD content mismatch')
                payload(selected, _xml_token_get(fragment, selected['member']))
        if meta['grammar'] and ordered_state not in meta['accepting']:
            raise ValueError('Incomplete ordered XSD content')
    def record(value, qualified, extra_attributes=(), bindings=()):
        """Visit one owning record in its fixed schema sequence without constructing XML nodes."""
        parent_context = writer.budget.context
        attribute_contexts = {} if getattr(parent_context, '_xml_consume_at', None) is not None else None
        attributes, active = _xml_model_output(value, parent_context)
        extensions, state, foreign_attributes, foreign_bindings = _xml_extension_output(value, writer)
        cursor = [0]
        attributes.extend(foreign_attributes)
        scope = dict(foreign_bindings)
        if getattr(value, '_xml_token_root_namespace', False):
            state = getattr(value, '_srl_xml_state', None)
            scope = dict(writer.scopes[-1]) if writer.scopes else {'xml': 'http://www.w3.org/XML/1998/namespace'}
            if state is not None:
                if not isinstance(state, tuple) or len(state) not in (3, 4, 5) or not isinstance(state[0], dict):
                    raise ValueError('Invalid XML lexical namespace state')
                scope.update(_xml_scope_copy(state[0], writer.budget))
        scope.update(bindings)
        fields = tuple(item for index, item in enumerate(value._xml_token_fields) if active[index])
        _xml_choices(value, value._xml_choices, fields)
        identity = None
        if value._xml_identity is not None:
            identity = _XmlTokenIdentity(value._xml_identity, writer.budget.context, writer.budget.limits.xml_identity)
            writer.identities.append(identity)
        attributes.extend(extra_attributes)
        for field_index, item in enumerate(value._xml_token_fields):
            if not active[field_index]:
                continue
            if item['mode'] == 'attribute':
                writer.budget.context = _xml_field_context(parent_context, item['member'])
                for selected in _xml_token_values(value, item):
                    lexical = item['write'](selected, {}, writer.budget.context) if not item.get('namespace') else None
                    if item.get('namespace'):
                        raw = item['raw'](selected)
                        rule = item['rule']()
                        _xml_scalar_scope_open(state, field_index, 0, scope, writer.budget, rule, raw)
                        lexical = _xml_root_scoped_lexical(raw, rule, state, field_index, scope, writer.budget)
                    attributes.append((item['name'], lexical))
                    if attribute_contexts is not None:
                        writer.budget.context.consume('decoder_allocation_bytes', _XML_DIAGNOSTIC_CONTEXT_BYTES)
                        attribute_contexts[item['name']] = writer.budget.context
        writer.budget.context = parent_context
        writer.start(qualified, attributes, scope, attribute_contexts)
        if value._xml_mixed is not None:
            mixed(value, value._xml_mixed)
        for position, item in enumerate(value._xml_token_fields):
            writer.budget.context = _xml_field_context(parent_context, item['member'])
            _xml_extension_at(writer, extensions, state, cursor, position)
            if not active[position] or item['mode'] == 'attribute':
                _xml_extension_position_end(extensions, state, cursor, position)
                continue
            if item.get('variant') is not None:
                variant(value, item)
                _xml_extension_position_end(extensions, state, cursor, position)
                continue
            values = _xml_token_values(value, item)
            if item['map'] is not None:
                mapping(item, values)
                _xml_extension_position_end(extensions, state, cursor, position)
                continue
            if item['wrapper'] is not None:
                if item['presence'] in ('absent', 'absent_or_nil') and not _xml_token_get(value, item['member']):
                    _xml_extension_position_end(extensions, state, cursor, position)
                    continue
                if values == (_XML_TOKEN_NIL,):
                    writer.start(item['wrapper'], ((_XML_TOKEN_NIL_NAME, 'true'),))
                    writer.end()
                    _xml_extension_position_end(extensions, state, cursor, position)
                    continue
                writer.start(item['wrapper'])
            for index, selected in enumerate(values):
                if item['repeated'] and item['wrapper'] is None:
                    _xml_extension_at(writer, extensions, state, cursor, position, index)
                if selected is _XML_TOKEN_NIL:
                    writer.start(item['name'], ((_XML_TOKEN_NIL_NAME, 'true'),))
                    writer.end()
                elif item['mode'] == 'text':
                    writer.text(item['write'](selected, {}, writer.budget.context))
                else:
                    payload(item, selected)
            if item['wrapper'] is not None:
                writer.end()
            elif item['repeated']:
                _xml_extension_at(writer, extensions, state, cursor, position, len(values))
            _xml_extension_position_end(extensions, state, cursor, position)
        writer.budget.context = parent_context
        end_position = len(value._xml_token_fields) + (len(value._xml_mixed['children']) if value._xml_mixed else 0)
        _xml_extension_at(writer, extensions, state, cursor, end_position)
        _xml_extension_position_end(extensions, state, cursor, end_position)
        if extensions is not None:
            extensions.finish()
        writer.end()
        if identity is not None:
            identity.finish()
            writer.identities.pop()
    record(model, name)
    return b''.join(writer.parts)


)xmlpython"
R"xmlpython(class _XmlTokenFrame:
    """Hold one unpublished owning record, scalar lexical buffer, or collection wrapper."""
    def __init__(self, model, name, attributes, scope, context, item=None, identity_limits=None, wrapper=False, mixed_limits=None, extension_options=None, validation_limits=None):
        """Validate attributes immediately; keep only open-frame lexical state and native models."""
        self.name, self.scope, self.context, self.item = name, scope, context, item
        if item is not None and item.get('variant_payload') and _XML_TOKEN_TYPE_NAME in attributes:
            attributes = dict(attributes)
            attributes.pop(_XML_TOKEN_TYPE_NAME)
        self.nil = False
        if _XML_TOKEN_NIL_NAME in attributes:
            policy = item['presence'] if item is not None and (wrapper or item['wrapper'] is None) else None
            if policy not in ('nil', 'absent_or_nil'):
                raise ValueError('XML xsi:nil is not permitted by presence policy')
            attributes = dict(attributes)
            nil = attributes.pop(_XML_TOKEN_NIL_NAME).strip(' \t\r\n')
            if nil not in ('true', '1', 'false', '0'):
                raise ValueError('Invalid XML xsi:nil boolean value')
            self.nil = nil in ('true', '1')
            if self.nil:
                model = None
        revision, self.active = None, ()
        if model is not None:
            attributes, revision, self.active = _xml_model_input(model, attributes, context)
        self.extensions = _XmlExtensions(model._xml_extensions, scope, extension_options, context) if model is not None and model._xml_extensions is not None else None
        if self.extensions is not None:
            self.extensions.validation_limits = validation_limits
        self.model = model() if model is not None else None
        if model is not None and getattr(model, '_xml_token_root_namespace', False):
            context.retain_namespace(scope)
            retained = dict(scope)
            retained.setdefault('', '')
            self.model._srl_xml_state = (retained, {}, [], {})
        if model is not None and model._xml_model['revision'] is not None:
            _xml_token_set(self.model, model._xml_model['revision']['member'], revision)
        self.identity = _XmlTokenIdentity(model._xml_identity, context, identity_limits) if model is not None and model._xml_identity is not None else None
        self.fields = model._xml_token_fields if model is not None else ()
        self.all_groups = tuple((tuple(index for index in group if self.active[index]), minimum)
                                for group, minimum in model._xml_token_all
                                if any(self.active[index] for index in group)) if model is not None else ()
        self.mixed = model._xml_mixed if model is not None else None
        self.ordered_state = 1
        self.mixed_bounds = (mixed_limits or XmlMixedLimits()) if self.mixed is not None else None
        self.mixed_pending, self.mixed_text_bytes = bytearray(), 0
        self.counts, self.position, self.fragments = [0] * len(self.fields), 0, []
        self.retained_bytes = 0
        self.wrapper = wrapper
        self.values = []
        if model is None:
            if attributes:
                raise ValueError('XML scalar contains attributes or children')
            return
        if self.mixed is not None:
            _xml_token_set(self.model, self.mixed['member'], [])
        known = set()
        for index, selected in enumerate(self.fields):
            if not self.active[index]:
                continue
            if selected['repeated'] and selected['mode'] == 'child' and not selected['optional']:
                _xml_token_set(self.model, selected['member'], [])
            if selected['mode'] != 'attribute':
                continue
            known.add(selected['name'])
            value = attributes.get(selected['name'])
            if value is None and selected['minimum']:
                raise ValueError('Required XML attribute missing')
            if value is None:
                value = selected['default']
            if value is None:
                if not selected['optional']:
                    raise ValueError('Required XML attribute missing')
                continue
            native = selected['read'](value, scope, _xml_field_context(context, selected['member']))
            self.store(index, native)
            if selected.get('namespace'):
                # Keep an immutable source scope separate from editable owner presentation bindings.
                context.retain_namespace(scope)
                original = dict(scope)
                original.setdefault('', '')
                self.model._srl_xml_state[3][index, 0] = (original, selected['raw'](native))
        for key, value in attributes.items():
            if key not in known:
                if self.extensions is None:
                    raise ValueError('Unexpected XML attribute or child')
                self.extensions.attribute(key, value)
    def store(self, index, value, wrapper=False):
        """Install a completed field only in its still-private candidate owner."""
        if isinstance(index, tuple):
            selected = next(item for item in self.mixed['children'] if item['alternative'] == index[1])
            self.mixed_add(index[1], selected['member'], value)
            return
        item = self.fields[index]
        self.counts[index] += 1
        if item['maximum'] is not None and self.counts[index] > item['maximum']:
            raise ValueError('XML field occurrence mismatch')
        if item.get('variant') is not None:
            selected, native = value
            _xml_token_set(self.model, item['member'] + '_index', selected)
            _xml_token_set(self.model, item['variant']['alternatives'][selected]['member'], native)
        elif value is _XML_TOKEN_NIL:
            _xml_token_set(self.model, item['member'], ((),) if item['presence'] == 'absent_or_nil' else ())
        elif item['repeated'] and not wrapper:
            _xml_token_get(self.model, item['member']).append(value)
            if self.context is not None:
                _xml_field_context(self.context, item['member']).consume('decoder_allocation_bytes', 8)
        else:
            selected = ((value,),) if item['presence'] == 'absent_or_nil' else (value,) if item['optional'] else value
            _xml_token_set(self.model, item['member'], selected)
    def mixed_add(self, index, member, value):
        """Admit one owning fragment before constructing its native variant storage."""
        if self.mixed['grammar']:
            self.ordered_state = self.mixed['grammar'][self.ordered_state][index]
            if not self.ordered_state:
                raise ValueError('Ordered XSD content mismatch')
        values = _xml_token_get(self.model, self.mixed['member'])
        if len(values) >= self.mixed_bounds.max_fragments:
            _xml_limit(self.context, 'XML mixed fragment limit exceeded')
        if self.context is not None:
            self.context.consume('decoder_allocation_bytes', 72)
        fragment = self.mixed['fragment']()()
        _xml_token_set(fragment, self.mixed['index'], index)
        _xml_token_set(fragment, member, value)
        values.append(fragment)
    def mixed_text(self, value):
        """Buffer only one bounded text run so arbitrary UTF-8/token splits have the same meaning."""
        if getattr(self, 'mixed_closed', False) and value and not self.mixed['element_only']:
            raise ValueError('Mixed XML text follows an end extension slot')
        if self.mixed['element_only']:
            if value.strip(' \t\r\n'):
                raise ValueError('Text in element-only XML content')
            return
        if not value:
            return
        if not self.mixed_pending and len(_xml_token_get(self.model, self.mixed['member'])) >= self.mixed_bounds.max_fragments:
            _xml_limit(self.context, 'XML mixed fragment limit exceeded')
        size = len(value.encode('utf-8'))
        if size > self.mixed_bounds.max_text_bytes - self.mixed_text_bytes:
            _xml_limit(self.context, 'XML mixed text limit exceeded')
        self.mixed_text_bytes += size
        if self.context is not None:
            self.context.consume('decoder_allocation_bytes', size)
        self.mixed_pending.extend(value.encode('utf-8'))
    def mixed_flush(self):
        """Complete text before the next child or owner close without retaining XML descendants."""
        if self.mixed_pending:
            if self.context is not None:
                self.context.consume('decoder_allocation_bytes', len(self.mixed_pending))
            self.mixed_add(self.mixed['text_index'], self.mixed['text_member'], self.mixed_pending.decode('utf-8'))
            self.mixed_pending.clear()
    def child(self, name):
        """Select the next generated child slot without buffering earlier XML siblings."""
        if self.model is None:
            if self.nil:
                raise ValueError('Nil XML element cannot contain text or child elements')
            if self.wrapper and name == self.item['name']:
                return self.item, None
            raise ValueError('XML scalar contains attributes or children')
        if self.mixed is not None:
            self.mixed_flush()
            for item in self.mixed['children']:
                if self.context is not None:
                    self.context.consume('work_units', 1)
                if item['name'] == name:
                    if getattr(self, 'mixed_closed', False):
                        raise ValueError('Known XML child follows an end extension slot')
                    if len(_xml_token_get(self.model, self.mixed['member'])) >= self.mixed_bounds.max_fragments:
                        _xml_limit(self.context, 'XML mixed fragment limit exceeded')
                    return item, ('mixed', item['alternative'])
            foreign = self.foreign(name, len(self.fields) + len(self.mixed['children']))
            if foreign is not None:
                self.mixed_closed = True
                return foreign
            raise ValueError('Unknown XML mixed child')
        while self.position < len(self.fields):
            index = self.position
            item = self.fields[index]
            if not self.active[index] or item['mode'] != 'child':
                foreign = self.foreign(name, index)
                if foreign is not None:
                    return foreign
                self.position += 1
                continue
            group = next((selected for selected in self.all_groups if index in selected[0]), None)
            if group is not None:
                alternatives, minimum = group
                for selected in alternatives:
                    member = self.fields[selected]
                    if (member['wrapper'] or member['name']) == name:
                        if self.counts[selected]:
                            raise ValueError('XML field occurrence mismatch')
                        return member, selected
                if minimum or any(self.counts[selected] for selected in alternatives):
                    if any(self.counts[selected] < self.fields[selected]['minimum'] for selected in alternatives):
                        raise ValueError('XML field occurrence mismatch')
                self.position = max(alternatives) + 1
                continue
            expected = item['wrapper'] or item['name']
            foreign = self.foreign(name, index, self.counts[index] if item['repeated'] and not item['wrapper'] else 0)
            if foreign is not None:
                return foreign
            if expected == name:
                maximum = 1 if item['wrapper'] else item['maximum']
                if maximum is not None and self.counts[index] >= maximum:
                    if self.extensions is not None:
                        self.position += 1
                        continue
                    raise ValueError('XML field occurrence mismatch')
                return item, index
            maximum = 1 if item['wrapper'] else item['maximum']
            if maximum is not None and self.counts[index] >= maximum:
                self.position += 1
                continue
            foreign = self.foreign(name, index, self.counts[index] if item['repeated'] and not item['wrapper'] else 0)
            if foreign is not None:
                return foreign
            minimum = (0 if item['presence'] in ('absent', 'absent_or_nil') or item['map'] is not None and item['map']['optional'] else 1) if item['wrapper'] else item['minimum']
            if self.counts[index] < minimum:
                raise ValueError('XML field occurrence mismatch')
            self.position += 1
        foreign = self.foreign(name, len(self.fields))
        if foreign is not None:
            return foreign
        raise ValueError('Unexpected XML attribute or child')
    def foreign(self, name, position, before_item=0):
        """Admit one positioned foreign root while leaving all known values on their direct typed path."""
        if self.extensions is None:
            return None
        selected = self.extensions.slot(name, False, position)
        if selected is None:
            return None
        if position < len(self.fields) and self.counts[position] and (
                not self.fields[position]['repeated'] or self.fields[position]['wrapper'] or
                self.extensions.meta['slots'][selected].get('match', 0) not in (0, 4)):
            return None
        preserve = self.extensions.admit(name, False, position)
        if any(item['mode'] == 'text' for item in self.fields):
            self.scalar_closed = True
        return {'foreign': (self.extensions, preserve, self.extensions.process)}, ('extension', position, before_item)
    def finish(self):
        """Validate complete lexical/record content before passing its value to a parent."""
        text = ''.join(self.fragments)
        if self.model is None:
            if self.nil:
                return _XML_TOKEN_NIL
            if self.wrapper:
                if text.strip(' \t\r\n'):
                    raise ValueError('Unexpected XML record text')
                count = len(self.values)
                if count < self.item['minimum'] or self.item['maximum'] is not None and count > self.item['maximum']:
                    raise ValueError('XML field occurrence mismatch')
                if self.item['extent'] and count != self.item['extent']:
                    raise ValueError('XML fixed extent mismatch')
                return self.values
            return self.item['read'](text or self.item['default'] or '', self.scope, self.context)
        text_field = False
        if self.mixed is not None:
            self.mixed_flush()
            if self.mixed['grammar'] and self.ordered_state not in self.mixed['accepting']:
                raise ValueError('Incomplete ordered XSD content')
            if self.mixed['element_only'] and self.mixed['extent'] and len(_xml_token_get(self.model, self.mixed['member'])) != self.mixed['extent']:
                raise ValueError('XML ordered fixed extent mismatch')
        for index, item in enumerate(self.fields):
            if not self.active[index]:
                continue
            if item['mode'] == 'text':
                text_field = True
                self.store(index, item['read'](text or item['default'] or '', self.scope,
                           _xml_field_context(self.context, item['member'])))
            minimum = (0 if item['presence'] in ('absent', 'absent_or_nil') or item['map'] is not None and item['map']['optional'] else 1) if item['wrapper'] else item['minimum']
            group = next((selected for selected in self.all_groups if index in selected[0]), None)
            if group is not None and group[1] == 0 and not any(self.counts[selected] for selected in group[0]):
                minimum = 0
            if self.counts[index] < minimum:
                raise ValueError('XML field occurrence mismatch')
            if item['extent'] and not item['wrapper'] and self.counts[index] != item['extent']:
                raise ValueError('XML fixed extent mismatch')
        if not text_field and text.strip(' \t\r\n'):
            raise ValueError('Unexpected XML record text')
        _xml_choices(self.model, self.model._xml_choices,
                     tuple(item for index, item in enumerate(self.fields) if self.active[index]))
        if self.extensions is not None:
            self.extensions.finish()
            self.model._srl_xml_state = self.extensions.state
        if self.identity is not None:
            self.identity.finish()
        return self.model


)xmlpython"
R"xmlpython(class _XmlTokenMapFrame:
    """Retain the destination map and cumulative native-key bounds, without XML entries."""
    def __init__(self, item, name, attributes, scope, context, bounds):
        """Require the exact attribute-free wrapper before admitting any entries."""
        if attributes:
            raise ValueError('Unexpected XML map wrapper attribute')
        self.item, self.name, self.scope, self.context = item, name, scope, context
        self.bounds, self.key_bytes, self.values = bounds or XmlMapLimits(), 0, {}
        self.model, self.identity, self.nil, self.wrapper = True, None, False, False
        self.fields, self.fragments = (), []
    def child(self, name):
        """Admit only the selected entry identity while enforcing the entry count before allocation."""
        if name != self.item['map']['entry']:
            raise ValueError('Unexpected XML map entry')
        if len(self.values) >= self.bounds.max_entries:
            _xml_limit(self.context, 'XML map entry limit exceeded')
        return {'map_entry': self.item['map'], 'key_remaining': self.bounds.max_key_bytes - self.key_bytes}, None
    def append(self, entry):
        """Reject duplicate native keys and cumulative lexical/canonical sizes before insertion."""
        key, value, size = entry
        if key in self.values:
            raise ValueError('Duplicate XML map key')
        if size > self.bounds.max_key_bytes - self.key_bytes:
            _xml_limit(self.context, 'XML map key-byte limit exceeded')
        self.key_bytes += size
        if self.context is not None:
            self.context.consume('decoder_allocation_bytes', 64)
        self.values[key] = value
    def finish(self):
        """Return only the complete private owning map, including its explicitly empty state."""
        return self.values


class _XmlTokenMapEntry:
    """Hold exactly one typed key and value until their complete entry succeeds."""
    def __init__(self, selected, name, attributes, scope, context):
        """Validate the key placement and all entry attributes before reading child values."""
        self.meta, self.remaining = selected['map_entry'], selected['key_remaining']
        self.name, self.scope, self.context = name, scope, context
        self.model, self.identity, self.nil, self.wrapper = True, None, False, False
        self.fields, self.fragments, self.values, self.position, self.key_size = (), [], {}, 0, 0
        if self.meta['attribute']:
            key = self.meta['key']['name']
            if len(attributes) != 1 or key not in attributes:
                raise ValueError('XML map entry requires exactly one key attribute')
            raw = attributes[key]
            if len(raw.encode('utf-8')) > self.remaining:
                _xml_limit(self.context, 'XML map key-byte limit exceeded')
            self.values[0] = self.meta['key']['read'](raw, scope, context)
            self.key_size = max(len(raw.encode('utf-8')), len(self.meta['key']['write'](self.values[0], scope, context).encode('utf-8')))
            self.position = 1
        elif attributes:
            raise ValueError('Unexpected XML map entry attribute')
    def child(self, name):
        """Select exactly one key and value independently of their child presentation order."""
        if not self.meta['attribute'] and name == self.meta['key']['name']:
            selected = 0
        elif name == self.meta['value']['name']:
            selected = 1
        else:
            raise ValueError('Unexpected XML map entry member')
        if selected in self.values:
            raise ValueError('Duplicate XML map entry member')
        item = self.meta['key'] if selected == 0 else self.meta['value']
        if selected == 0:
            item = dict(item)
            item['map_key_bytes'] = self.remaining
        return item, selected
    def store(self, index, value, frame):
        """Keep a completed value and charge keys by their larger original/canonical spelling."""
        if index in self.values:
            raise ValueError('Duplicate XML map entry member')
        self.values[index] = value
        if index == 0:
            lexical = ''.join(frame.fragments)
            canonical = self.meta['key']['write'](value, frame.scope, self.context)
            self.key_size = max(len(lexical.encode('utf-8')), len(canonical.encode('utf-8')))
    def finish(self):
        """Reject incomplete entries before they can mutate the private map candidate."""
        if len(self.values) != 2:
            raise ValueError('XML map entry requires one key and value')
        return self.values[0], self.values[1], self.key_size


class _XmlTokenVariantFrame:
    """Own exactly one selected native payload until its required wrapper closes."""
    def __init__(self, item, name, attributes, scope, context):
        """Reject wrapper attributes before inspecting any tag/type alternative."""
        if attributes:
            raise ValueError('Unexpected XML variant wrapper attribute')
        self.item, self.name, self.scope, self.context = item, name, scope, context
        self.model, self.identity, self.nil, self.wrapper = True, None, False, False
        self.fields, self.fragments, self.selected, self.has_value, self.value = (), [], None, False, None
    def select(self, name, attributes, scope):
        """Resolve complete in-scope QName identity before selecting a payload decoder."""
        if self.selected is not None or _XML_TOKEN_NIL_NAME in attributes:
            raise ValueError('XML variant requires exactly one nonnil payload')
        qualified = _xml_resolve_qname(attributes[_XML_TOKEN_TYPE_NAME], scope) if _XML_TOKEN_TYPE_NAME in attributes else None
        mode = self.item['variant']['selection']
        for index, alternative in enumerate(self.item['variant']['alternatives']):
            if self.context is not None:
                self.context.consume('work_units', 1)
            matches = alternative['name'] == name if mode == 'element' else qualified == alternative['type']
            if not matches:
                continue
            if qualified is not None and qualified != alternative['type'] or mode == 'xsi_type' and name != self.item['wrapper']:
                raise ValueError('Conflicting XML variant tag/type identity')
            self.selected = index
            item = dict(alternative)
            item['name'] = name
            return item, index
        raise ValueError('Unknown or missing XML variant tag/type identity')
    def append(self, value):
        """Retain one checked owning payload without publishing it through the parent record."""
        if self.has_value:
            raise ValueError('XML variant requires exactly one payload')
        self.value, self.has_value = value, True
    def finish(self):
        """Reject an empty wrapper before exposing the selected alternative to its candidate owner."""
        if not self.has_value:
            raise ValueError('XML variant requires exactly one payload')
        return self.selected, self.value


def _xml_token_decode(model, root, chunks, limits=None, context=None):
    """Decode arbitrary UTF-8 slices into one candidate; publish only after exact final EOF."""
    if _xml_token_has_namespace(model):
        context = _xml_namespace_context(limits, context)
    budget = _XmlBudget(limits, context)
    context = budget.context
    parser = _xml_expat.ParserCreate(encoding='UTF-8', namespace_separator='\x1f')
    stack, declarations = [], {}
    result, size, prefix = None, 0, b''
    def expanded(value):
        """Normalize provider names before static generated dispatch."""
        parts = value.split('\x1f')
        return _xml_name(*parts) if len(parts) == 2 else value
    def prohibited(*unused):
        """Prevent DTDs and external/user-defined entities from entering the provider."""
        raise ValueError('XML DTDs and entities are prohibited')
    def declaration(version, encoding, standalone):
        """Keep the incremental producer restricted to XML1.0 UTF-8."""
        if version != '1.0' or encoding and encoding.lower().replace('-', '') != 'utf8':
            raise ValueError('XML1.0 UTF-8 required')
    def namespace(selected, uri):
        """Bound namespace declarations before retaining the current open-element scope."""
        budget.attributes += 1
        if budget.attributes > budget.limits.max_elements:
            _xml_limit(budget.context, 'XML attribute limit exceeded')
        declarations[budget.text(selected or '')] = budget.text(uri or '')
    def start(raw_name, attributes):
        """Construct only the destination model or current scalar lexical buffer."""
        name = expanded(raw_name)
        budget.element(name, len(stack) + 1)
        inherited = stack[-1][0].scope if stack else {'xml': 'http://www.w3.org/XML/1998/namespace'}
        _xml_charge_scope_copy(inherited, budget.context)
        scope = dict(inherited)
        scope.update(declarations)
        declarations.clear()
        budget.attributes += len(attributes)
        if budget.attributes > budget.limits.max_elements:
            _xml_limit(budget.context, 'XML attribute limit exceeded')
        attrs = {budget.text(expanded(key)): budget.text(value) for key, value in attributes.items()}
        if context is not None:
            context.consume('decoder_allocation_bytes', 64 * len(scope))
        if not stack:
            if name != root or result is not None:
                raise ValueError('Unexpected XML root or namespace')
            _xml_package_root(attrs, scope, context)
            expected_type = getattr(context, '_xml_root_type', None)
            if expected_type is not None and _XML_TOKEN_TYPE_NAME in attrs:
                if _xml_name(*_xml_resolve_qname(attrs[_XML_TOKEN_TYPE_NAME], scope)) != expected_type:
                    raise ValueError('Unexpected package XML root type')
                attrs.pop(_XML_TOKEN_TYPE_NAME)
            frame, index = _XmlTokenFrame(model, name, attrs, scope, context, identity_limits=budget.limits.xml_identity, mixed_limits=budget.limits.xml_mixed, extension_options=budget.limits.xml_extensions, validation_limits=budget.limits), None
        else:
            parent = stack[-1][0]
            item, index = parent.select(name, attrs, scope) if isinstance(parent, _XmlTokenVariantFrame) else parent.child(name)
            member = item.get('member', item.get('name', '').rsplit('}', 1)[-1])
            child_context = parent.context if parent.wrapper else _xml_field_context(parent.context, member)
            if item.get('foreign') is not None:
                frame = _XmlForeignFrame(name, attrs, scope, *item['foreign'])
            elif item.get('map_entry') is not None:
                frame = _XmlTokenMapEntry(item, name, attrs, scope, child_context)
            else:
                wrapped = not stack[-1][0].wrapper and item['wrapper'] == name
                if wrapped and item['map'] is not None:
                    frame = _XmlTokenMapFrame(item, name, attrs, scope, child_context, budget.limits.xml_map)
                elif wrapped and item.get('variant') is not None:
                    frame = _XmlTokenVariantFrame(item, name, attrs, scope, child_context)
                else:
                    selected = item['model']() if not wrapped and item['model'] is not None else None
                    frame = _XmlTokenFrame(selected, name, attrs, scope, child_context, item, budget.limits.xml_identity, wrapped, budget.limits.xml_mixed, budget.limits.xml_extensions, budget.limits)
        stack.append((frame, index))
        if getattr(frame, 'extensions', None) is not None:
            frame.extensions.validation_limits = budget.limits
        budget.context = getattr(frame, 'context', context)
        for current, unused in stack:
            if current.identity is not None:
                current.identity.start(name, attrs, scope)
    def text(value):
        """Retain bounded scalar/simple-content fragments; ignore record indentation immediately."""
        budget.text(value)
        for current, unused in stack:
            if current.identity is not None:
                current.identity.text(value)
        if not stack:
            if value.strip(' \t\r\n'):
                raise ValueError('XML text outside root')
            return
        frame = stack[-1][0]
        if value and getattr(frame, 'scalar_closed', False):
            raise ValueError('Scalar text follows an XML extension suffix')
        if isinstance(frame, _XmlForeignFrame):
            frame.text(value)
            return
        if frame.nil and value:
            raise ValueError('Nil XML element cannot contain text or child elements')
        if getattr(frame, 'mixed', None) is not None:
            frame.mixed_text(value)
            return
        retains = frame.model is None and not frame.wrapper or any(item['mode'] == 'text' for item in frame.fields)
        if not retains:
            if value.strip(' \t\r\n'):
                raise ValueError('Unexpected XML record text')
            return
        if value:
            if frame.item is not None and frame.item.get('map_key_bytes') is not None:
                size = len(value.encode('utf-8'))
                if size > frame.item['map_key_bytes'] - frame.retained_bytes:
                    _xml_limit(budget.context, 'XML map key-byte limit exceeded')
                frame.retained_bytes += size
            budget.fragments += 1
            if budget.fragments > budget.limits.max_elements:
                _xml_limit(budget.context, 'XML text fragment limit exceeded')
            if frame.context is not None:
                frame.context.consume('decoder_allocation_bytes', 64)
            frame.fragments.append(value)
    def end(unused):
        """Attach a completely checked child to its private parent; retain no XML subtree."""
        nonlocal result
        for current, unused_index in stack:
            if current.identity is not None:
                current.identity.end()
        frame, index = stack.pop()
        frame_context = getattr(frame, 'context', context)
        if frame_context is not None:
            frame_context.consume('decoder_allocation_bytes', sum(len(value.encode('utf-8')) for value in frame.fragments))
        value = frame.finish()
        if stack:
)xmlpython"
R"xmlpython(            parent = stack[-1][0]
            if isinstance(frame, _XmlForeignFrame):
                if isinstance(parent, _XmlForeignFrame):
                    parent.append(value)
                else:
                    parent.extensions.child(index[1], index[2], value)
            elif isinstance(parent, _XmlTokenMapFrame):
                parent.append(value)
            elif isinstance(parent, _XmlTokenMapEntry):
                parent.store(index, value, frame)
            elif isinstance(parent, _XmlTokenVariantFrame):
                parent.append(value)
            elif parent.wrapper:
                if parent.item['maximum'] is not None and len(parent.values) >= parent.item['maximum']:
                    raise ValueError('XML field occurrence mismatch')
                parent.values.append(value)
                if frame_context is not None:
                    frame_context.consume('decoder_allocation_bytes', 8)
            elif frame.wrapper:
                parent.store(index, value, True)
            else:
                parent.store(index, value)
        else:
            result = value
        budget.context = getattr(stack[-1][0], 'context', context) if stack else context
    parser.XmlDeclHandler = declaration
    parser.StartNamespaceDeclHandler = namespace
    parser.StartElementHandler, parser.EndElementHandler = start, end
    parser.CharacterDataHandler = text
    parser.StartDoctypeDeclHandler = prohibited
    parser.EntityDeclHandler = prohibited
    parser.ExternalEntityRefHandler = prohibited
    parser.SetParamEntityParsing(_xml_expat.XML_PARAM_ENTITY_PARSING_NEVER)
    for chunk in chunks:
        if not isinstance(chunk, (bytes, bytearray, memoryview)):
            raise ValueError('XML requires UTF-8 byte chunks')
        if isinstance(chunk, memoryview):
            if not chunk.c_contiguous:
                raise ValueError('XML requires contiguous UTF-8 byte chunks')
            # Buffer-provider item counts can differ from byte counts; normalize without copying.
            chunk = chunk.cast('B')
        if len(chunk) > budget.limits.max_bytes - size:
            _xml_limit(budget.context, 'XML byte limit exceeded')
        size += len(chunk)
        if len(prefix) < 4:
            prefix += bytes(chunk[:4 - len(prefix)])
            if prefix[:2] in (b'\xff\xfe', b'\xfe\xff') or b'\x00' in prefix:
                raise ValueError('XML1.0 UTF-8 required')
        if context is not None:
            context.consume('work_units', len(chunk))
        parser.Parse(chunk, False)
    parser.Parse(b'', True)
    if stack or result is None:
        raise ValueError('XML requires exactly one root')
    return result

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
