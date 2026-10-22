// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"extensionpython(
_XML_EXTENSION_METADATA_MAX_BYTES = 8 * 1024 * 1024

class XmlExtensionLimits:
    """Bound logical foreign-vocabulary slots, items and retained semantic bytes."""
    def __init__(self, max_slots=128, max_items=100000, max_preserved_bytes=8 * 1024 * 1024):
        """Validate explicit nonnegative bounds; equality remains admitted."""
        for key, value in locals().copy().items():
            if key == 'self':
                continue
            if type(value) is not int or value < 0:
                raise ValueError('Invalid XML extension limit: ' + key)
            setattr(self, key, value)


class XmlExtensionOptions:
    """Declare understood vocabularies independently of opaque preservation and optional prefix requirements."""
    def __init__(self, understood_namespaces=(), required_prefixes='', limits=None):
        """Own a bounded URI inventory and borrow one explicit extension admission policy."""
        self.limits = limits or XmlExtensionLimits()
        if not isinstance(self.limits, XmlExtensionLimits) or len(understood_namespaces) > self.limits.max_items:
            raise ValueError('XML understood-vocabulary item limit exceeded')
        if any(type(uri) is not str for uri in understood_namespaces) or type(required_prefixes) is not str:
            raise ValueError('XML extension capabilities require URI and prefix text')
        metadata_bytes = 0
        for text in (*understood_namespaces, required_prefixes):
            if len(text) > _XML_EXTENSION_METADATA_MAX_BYTES - metadata_bytes:
                raise ValueError('XML capability metadata byte limit exceeded')
            _xml_check_text(text)
            metadata_bytes += len(text.encode('utf-8'))
            if metadata_bytes > _XML_EXTENSION_METADATA_MAX_BYTES:
                raise ValueError('XML capability metadata byte limit exceeded')
        self.understood_namespaces = tuple(understood_namespaces)
        self.required_prefixes = required_prefixes


def _xml_extension_bindings(scope):
    """Reject edited namespace declarations that could corrupt opaque lexical QName meanings."""
    for prefix, uri in scope.items():
        _xml_check_text(uri)
        if type(prefix) is not str or prefix and not _xml_ncname(prefix) or prefix == 'xmlns' or (
            prefix == 'xml') != (uri == 'http://www.w3.org/XML/1998/namespace') or prefix and not uri or (
            uri == 'http://www.w3.org/2000/xmlns/'):
            raise ValueError('Invalid XML extension namespace binding')


def _xml_extension_name(name):
    """Validate an opaque expanded element/attribute name before passing it to the token writer."""
    if type(name) is not str:
        raise ValueError('Invalid XML extension expanded name')
    if name.startswith('{'):
        if '}' not in name:
            raise ValueError('Invalid XML extension expanded name')
        uri, local = name[1:].split('}', 1)
        if not uri or uri == 'http://www.w3.org/2000/xmlns/':
            raise ValueError('Invalid XML extension name namespace')
        _xml_check_text(uri)
    else:
        local = name
    if not _xml_ncname(local):
        raise ValueError('Invalid XML extension local name')


class _XmlExtensions:
    """Validate one owner scope while retaining only explicitly preserved foreign payloads."""
    def __init__(self, meta, scope, options=None, context=None):
        """Require vocabulary understanding before selecting any unknown extension payload."""
        self.meta, self.scope, self.context = meta, scope, context
        self.options = options or XmlExtensionOptions()
        profile = getattr(context, '_xml_package_vocabularies', None)
        if profile is not None:
            self.options = XmlExtensionOptions(tuple(dict.fromkeys((*self.options.understood_namespaces, *profile))), self.options.required_prefixes, self.options.limits)
        if not isinstance(self.options, XmlExtensionOptions):
            raise ValueError('Expected XmlExtensionOptions')
        self.bounds = self.options.limits
        if len(meta['slots']) > self.bounds.max_slots or len(meta['known']) > self.bounds.max_items or len(meta.get('known_attributes', ())) > self.bounds.max_items or len(meta['required']) > self.bounds.max_items:
            _xml_limit(self.context, 'XML extension slot limit exceeded')
        self.counts, self.items, self.bytes, self.state = [0] * len(meta['slots']), 0, 0, None
        self.process = 0
        self.validation_limits = None
        self.temporary_depth = 1 if getattr(context, '_xml_validation', False) else 0
        self.temporary_bytes = 0
        self._work(len(meta['slots']) + 1)
        if context is not None:
            context.consume('decoder_allocation_bytes', len(meta['slots']) * 8)
        required = list(meta['required'])
        self._work(len(self.options.required_prefixes) + 1)
        prefix_count = 0
        # Iterate bounded tokens without splitting an entire hostile prefix list into owning strings.
        for matched in _re.finditer(r'[^ \t\r\n]+', self.options.required_prefixes):
            if prefix_count >= self.bounds.max_items:
                _xml_limit(self.context, 'XML required-vocabulary item limit exceeded')
            prefix_count += 1
            prefix = matched.group()
            self._work(len(prefix) + 1)
            if not _xml_ncname(prefix) or prefix == 'xmlns' or not scope.get(prefix):
                raise ValueError('Unbound or invalid required XML capability prefix')
            required.append(scope[prefix])
        for uri in required:
            self._work(len(uri) + 1)
            if uri != meta['core'] and not any(self._understands(uri, capability)
                                               for capability in self.options.understood_namespaces):
                raise ValueError('Required XML capability is not understood')
    def _work(self, count=1):
        """Preserve application cancellation and cumulative work failures during every policy traversal."""
        if self.context is not None:
            self.context.consume('work_units', count)
    def _understands(self, uri, capability):
        """Charge bounded URI identity comparisons without treating a permitted slot as understanding."""
        self._work(min(len(uri), len(capability)) + 1)
        return uri == capability
    def charge(self, size):
        """Admit retained logical bytes before allocating corresponding opaque storage."""
        if self.temporary_depth:
            limits = self.validation_limits or Limits()
            maximum = limits.max_bytes + limits.max_string_bytes + limits.max_elements * 64
            if size > maximum - self.temporary_bytes:
                _xml_limit(self.context, 'XML declaration temporary-byte limit exceeded')
            self.temporary_bytes += size
        else:
            if size > self.bounds.max_preserved_bytes - self.bytes:
                _xml_limit(self.context, 'XML extension preserved-byte limit exceeded')
            self.bytes += size
        if self.context is not None:
            if not self.temporary_depth:
                self.context.consume('preserved_bytes', size)
            self.context.consume('decoder_allocation_bytes', size)
    def slot(self, name, attribute, position=None):
        """Resolve exactly one explicit URI/position slot; core and known names remain closed."""
        self._work(len(name) + len(self.meta['known']) + 1)
        uri = _xml_namespace(name)
        known = self.meta.get('known_attributes', self.meta['known']) if attribute else self.meta['known']
        if uri == 'urn:serializer:xml:model' or attribute and name in known:
            return None
        for index, slot in enumerate(self.meta['slots']):
            self._work(len(slot['namespaces']) + 1)
            if not attribute and name in known:
                positions = self.meta.get('known_positions', ())
                known_index = known.index(name)
                if slot.get('match', 0) in (0, 4) or not (positions and positions[known_index] >= 0 and positions[known_index] < position or self.counts[index] < slot['minimum']):
                    continue
            contained = uri in slot['namespaces']
            matched = slot.get('match', 0) == 1 or (not contained if slot.get('match', 0) in (3, 4) else contained)
            if slot['attribute'] == attribute and (attribute or slot['position'] < 0 or slot['position'] == position) and matched:
                return index
        return None
    def admit(self, name, attribute, position=None):
        """Check policy/count/item bounds before accepting one complete unknown owner item."""
        index = self.slot(name, attribute, position)
        if index is None or self.meta['slots'][index]['policy'] == 'reject':
            raise ValueError('XML extension has no permitted slot')
        slot = self.meta['slots'][index]
        if self.items >= self.bounds.max_items or slot['maximum'] is not None and self.counts[index] >= slot['maximum']:
            _xml_limit(self.context, 'XML extension item or occurrence limit exceeded')
        self.items += 1
        self.counts[index] += 1
        self.process = slot.get('process', 0)
        if self.process:
            declarations = self.meta.get('declarations')
            if declarations is None or (attribute, name) not in declarations():
                if self.process == 2:
                    raise ValueError('Strict XML wildcard requires a global declaration')
                self.process = 0
        return slot['policy'] == 'preserve'
    def storage(self):
        """Materialize XML-only owner state lazily when the first preserved payload is admitted."""
        if self.state is None:
            self.charge(64 + sum(64 + len(prefix.encode('utf-8')) + len(uri.encode('utf-8'))
                                 for prefix, uri in self.scope.items()))
            self.state = (dict(self.scope), {}, [])
        return self.state
    def attribute(self, name, value):
        """Apply exact attribute policy without retaining skipped values or the owner tree."""
        preserve = self.admit(name, True)
        if self.process:
            self.validate([name, {name: value}, [], '', self.scope, []], True, self.process)
        if preserve:
            self.charge(48 + len(name.encode('utf-8')) + len(value.encode('utf-8')))
            self.storage()[1][name] = value
    def child(self, position, before_item, node):
        """Attach only a completely captured foreign subtree to the private owner's positioned state."""
        if node is not None:
            self.charge(24)
            self.storage()[2].append((position, before_item, node))
    def measure(self, name, attrs, scope):
        """Measure one foreign node before copying or emitting its full semantic namespace context."""
        _xml_extension_name(name)
        _xml_extension_bindings(scope)
        size = 64 + len(name.encode('utf-8'))
        for key, value in attrs.items():
            _xml_extension_name(key)
            _xml_check_text(value)
            size += 48 + len(key.encode('utf-8')) + len(value.encode('utf-8'))
        size += sum(64 + len(prefix.encode('utf-8')) + len(uri.encode('utf-8')) for prefix, uri in scope.items())
        self.charge(size)
    def node(self, name, attrs, scope):
        """Copy only one already measured foreign node into owning extension storage."""
        self.measure(name, attrs, scope)
        return [name, dict(attrs), [], '', dict(scope), []]
    def finish(self):
        """Require every declared slot minimum before exposing the owning candidate or output bytes."""
        for count, slot in zip(self.counts, self.meta['slots']):
            self._work()
            if count < slot['minimum']:
                raise ValueError('Required XML extension occurrence missing')
    def validate(self, node, attribute, process):
        """Apply strict/lax global declaration dispatch through the original bounded operation."""
        if not process:
            return
        self._work()
        declarations = self.meta.get('declarations')
        selected = declarations().get((attribute, node[0])) if declarations is not None else None
        if selected is None:
            if process == 2:
                raise ValueError('Strict XML wildcard requires a global declaration')
            return
        model, nillable, abstract, nil_model, fixed, declared_type, codec_root, abstract_type, types = selected
        if abstract:
            raise ValueError('Abstract XML declaration cannot appear directly')
        selected_type = node[1].get(_XML_TOKEN_TYPE_NAME)
        if selected_type is not None:
            identity = _xml_name(*_xml_resolve_qname(selected_type, node[4]))
            if identity != declared_type:
                selected = types.get(identity)
                if selected is None:
                    raise ValueError('Unexpected declared XML type')
                model, nil_model, abstract_type, codec_root = selected
            if self.context is not None:
                self.context.consume('decoder_allocation_bytes', 48 * len(node[1]))
            node = [node[0], {key: value for key, value in node[1].items() if key != _XML_TOKEN_TYPE_NAME}, *node[2:]]
        if abstract_type:
            raise ValueError('Abstract XML type cannot appear directly')
        if node[0] != codec_root:
            node = [codec_root, *node[1:]]
        nil = node[1].get(_XML_TOKEN_NIL_NAME)
        if nil is not None:
            nil = nil.strip(' \t\r\n')
            if not nillable or nil not in ('true', 'false', '1', '0'):
                raise ValueError('Invalid declared XML nil state')
            if nil in ('true', '1'):
                if node[2] or node[3] or fixed:
                    raise ValueError('Nil XML declaration contains a value')
                if nil_model is None:
                    raise ValueError('Missing nil XML validation codec')
                model = nil_model)extensionpython"
R"extensionpython(
            node = [node[0], {key: value for key, value in node[1].items() if key != _XML_TOKEN_NIL_NAME}, *node[2:]]
        self.temporary_depth += 1
        try:
            context = _XmlValidationContext(self.context)
            writer = _XmlTokenOutput(self.validation_limits, context)
            _xml_extension_emit(writer, self, node, 1)
            data = b''.join(writer.parts)
            model.decode_xml(data, limits=writer.budget.limits, context=context)
        finally:
            self.temporary_depth -= 1


class _XmlValidationContext:
    """Share caller admission while excluding outer package-root permissions from private validation."""
    _xml_root_type = _xml_package_vocabularies = None
    _xml_package_requires_type = False
    _xml_validation = True
    def __init__(self, context):
        """Borrow the active operation without copying its counters or exception identities."""
        self.context = context
    def __getattr__(self, name):
        """Forward all ordinary namespace, diagnostic and cancellation operations unchanged."""
        return getattr(self.context, name)
    def consume(self, resource, amount):
        """Keep validation-only state marked even when the caller supplies no operation callbacks."""
        if self.context is not None:
            self.context.consume(resource, amount)
    def check(self):
        """Forward the original interruption boundary when an outer context exists."""
        if self.context is not None:
            self.context.check()
    def check_buffer(self, amount):
        """Keep optional simultaneous-buffer admission attached to the original operation."""
        callback = getattr(self.context, 'check_buffer', None)
        if callback is not None:
            callback(amount)


class _XmlForeignFrame:
    """Capture only a permitted foreign subtree, or retain no payload for a skip slot."""
    def __init__(self, name, attrs, scope, tracker, preserve, validation=0):
        """Keep provider state bounded independently of all known owning model destinations."""
        self.name, self.scope, self.tracker = name, scope, tracker
        self.retain, self.validation = preserve, validation
        self.preserve = preserve or bool(validation)
        self.temporary = bool(validation) and not preserve
        if self.temporary:
            tracker.temporary_depth += 1
        self.context = tracker.context
        self.model, self.identity, self.nil, self.wrapper = None, None, False, False
        self.fields, self.fragments, self.pending = (), [], bytearray()
        self.value = tracker.node(name, attrs, scope) if self.preserve else None
    def flush(self):
        """Join one provider-split text run only at semantic child boundaries."""
        if self.pending:
            self.tracker.charge(24)
            self.value[5].append((len(self.value[2]), self.pending.decode('utf-8')))
            self.pending.clear()
    def child(self, name):
        """Continue the already admitted foreign subtree without interpreting its vocabulary."""
        if self.preserve:
            self.flush()
        return {'foreign': (self.tracker, self.preserve)}, None
    def text(self, value):
        """Retain bounded UTF-8 only for preserve; skipped content remains transient provider data."""
        if self.preserve:
            data = value.encode('utf-8')
            self.tracker.charge(len(data) * 2)
            self.pending.extend(data)
    def append(self, node):
        """Link a completed foreign descendant only inside the captured extension payload."""
        if self.preserve:
            self.value[2].append(node)
    def finish(self):
        """Return one semantic foreign node with complete mixed-text order, or no skipped payload."""
        if self.preserve:
            self.flush()
            self.value[3] = ''.join(text for unused, text in self.value[5])
        try:
            self.tracker.validate(self.value, False, self.validation)
        finally:
            if self.temporary:
                self.tracker.temporary_depth -= 1
        return self.value if self.retain else None

)extensionpython"
R"extensionpython(
def _xml_extension_output(model, writer):
    """Revalidate existing opaque owner state without constructing a tree for known model values."""
    meta = model._xml_extensions
    if meta is None:
        return None, None, (), ()
    state = getattr(model, '_srl_xml_state', None)
    if state is not None and (not isinstance(state, tuple) or len(state) != 3 or
                              not isinstance(state[0], dict) or not isinstance(state[1], dict) or
                              not isinstance(state[2], list)):
        raise ValueError('Invalid XML extension owner state')
    scope = dict(writer.scopes[-1]) if writer.scopes else {'xml': 'http://www.w3.org/XML/1998/namespace'}
    if state is not None:
        _xml_extension_bindings(state[0])
        scope.update(state[0])
    tracker = _XmlExtensions(meta, scope, writer.budget.limits.xml_extensions, writer.budget.context)
    tracker.validation_limits = writer.budget.limits
    attributes = []
    if state is not None:
        if state[1] or state[2]:
            tracker.storage()
        for name, value in state[1].items():
            _xml_extension_name(name)
            _xml_check_text(value)
            if not tracker.admit(name, True):
                raise ValueError('Preserved XML attribute no longer has a preserve slot')
            tracker.validate([name, {name: value}, [], '', scope, []], True, tracker.process)
            tracker.charge(48 + len(name.encode('utf-8')) + len(value.encode('utf-8')))
            attributes.append((name, value))
        previous = (-1, -1)
        for selected in state[2]:
            if not isinstance(selected, tuple) or len(selected) != 3:
                raise ValueError('Invalid XML extension child position')
            position, item, node = selected
            if type(position) is not int or type(item) is not int or position < 0 or item < 0 or (
                position > len(model._xml_token_fields) + (len(model._xml_mixed['children']) if model._xml_mixed else 0)) or (position, item) < previous:
                raise ValueError('Invalid XML extension child order')
            previous = position, item
    return tracker, state, attributes, scope


def _xml_extension_emit(writer, tracker, node, depth):
    """Validate and emit one retained foreign subtree while borrowing all opaque payload storage."""
    if depth > writer.budget.limits.max_depth or not isinstance(node, list) or len(node) != 6 or (
        not isinstance(node[1], dict)) or not isinstance(node[2], list) or type(node[3]) is not str or (
        not isinstance(node[4], dict)) or not isinstance(node[5], list):
        raise ValueError('Invalid or too deep preserved XML extension subtree')
    tracker.measure(node[0], node[1], node[4])
    tracker.charge(len(node[3].encode('utf-8')))
    previous, offset = 0, 0
    for selected in node[5]:
        if not isinstance(selected, tuple) or len(selected) != 2:
            raise ValueError('Invalid XML extension mixed-text segment')
        position, text = selected
        if type(position) is not int or type(text) is not str or position < previous or position > len(node[2]) or (
            not node[3].startswith(text, offset)):
            raise ValueError('Invalid XML extension mixed-text order')
        tracker.charge(24 + len(text.encode('utf-8')))
        previous, offset = position, offset + len(text)
    if node[5] and offset != len(node[3]) or node[2] and node[3] and not node[5]:
        raise ValueError('Incomplete XML extension mixed-text order')
    writer.start(node[0], tuple(node[1].items()), node[4])
    if not node[5]:
        writer.text(node[3])
    segment = 0
    for index in range(len(node[2]) + 1):
        while segment < len(node[5]) and node[5][segment][0] == index:
            writer.text(node[5][segment][1])
            segment += 1
        if index < len(node[2]):
            _xml_extension_emit(writer, tracker, node[2][index], depth + 1)
    writer.end()


def _xml_extension_at(writer, tracker, state, cursor, position, item=0):
    """Emit only extensions belonging to this exact static field and repeated-child boundary."""
    if tracker is None or state is None:
        return
    while cursor[0] < len(state[2]):
        selected, before_item, node = state[2][cursor[0]]
        tracker._work()
        if (selected, before_item) < (position, item):
            raise ValueError('Preserved XML extension no longer fits its edited field position')
        if (selected, before_item) != (position, item):
            return
        if not isinstance(node, list) or not node:
            raise ValueError('Invalid preserved XML extension child')
        if not tracker.admit(node[0], False, position):
            raise ValueError('Preserved XML child no longer has a preserve slot')
        tracker.validate(node, False, tracker.process)
        tracker.charge(24)
        _xml_extension_emit(writer, tracker, node, len(writer.stack) + 1)
        cursor[0] += 1


def _xml_extension_position_end(tracker, state, cursor, position):
    """Reject retained child boundaries beyond the native values left after an edit."""
    if tracker is not None and state is not None and cursor[0] < len(state[2]) and state[2][cursor[0]][0] == position:
        raise ValueError('Preserved XML extension no longer fits its edited field position')

)extensionpython"
