// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"htmlpython(
class HtmlLimits:
    """Bound the approved HTML fragment adapter independently of XML parsing."""
    def __init__(self, max_input_bytes=8*1024*1024, max_output_bytes=16*1024*1024,
                 max_events=200000, max_depth=128, max_attributes_per_element=256,
                 max_text_bytes=8*1024*1024):
        """Reject invalid limits before scanning untrusted HTML."""
        for name, value in locals().copy().items():
            if name != 'self':
                if type(value) is not int or value < 0:
                    raise ValueError('Invalid HTML limit')
                setattr(self, name, value)


class HtmlAttribute:
    """Own one lowercase HTML attribute; hidden uses explicit Boolean presence."""
    def __init__(self, name, value='', boolean=False):
        """Retain attribute data for checked output without applying URL policy."""
        self.name, self.value, self.boolean = name, value, boolean


class HtmlEvent:
    """Own a fragment/start/text/end event without XML namespaces or xsi metadata."""
    def __init__(self, kind, name='', attributes=(), text=''):
        """Copy the attribute sequence while retaining immutable string payloads."""
        self.kind, self.name, self.attributes, self.text = kind, name, list(attributes), text


class Html:
    """Parse and write the shared explicitly balanced HTML fragment subset."""
    _tags = frozenset('div p span b i strong em a ul ol li section article h1 h2 h3 h4 h5 h6 br hr img small s u code blockquote nav'.split())
    _void = frozenset(('br', 'hr', 'img'))
    _phrasing = frozenset('span b i strong em a br img small s u code'.split())
    _space = ' \t\r\n\f'
    _name = _re.compile(r'[A-Za-z0-9-]+')
    _entities = {'amp': '&', 'lt': '<', 'gt': '>', 'quot': '"', 'apos': "'", 'nbsp': '\xa0'}
    @staticmethod
    def supported_subset():
        """Describe the exact boundary; this adapter is neither general HTML5 nor a sanitizer."""
        return ('UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; '
                'br/hr/img void tags; ASCII case-insensitive names; quoted/unquoted and hidden '
                'Boolean attributes; amp/lt/gt/quot/apos/nbsp and numeric references; comments '
                'omitted; no doctype, implied closes, raw text, foreign content or browser recovery')
    @staticmethod
    def _charge(context, resource, count):
        """Share work/allocation accounting and cancellation with a caller-owned operation."""
        if context is not None:
            context.consume(resource, count)
    @staticmethod
    def _text(value):
        """Require Unicode scalars in the shared HTML subset, including HTML form-feed whitespace."""
        if type(value) is not str:
            raise ValueError('HTML requires string text')
        for character in value:
            code = ord(character)
            if code == 0 or 0xd800 <= code <= 0xdfff or 0x7f <= code <= 0x9f or code < 0x20 and character not in '\t\n\r\f':
                raise ValueError('Unsupported HTML control character')
        return value
    @staticmethod
    def _decode(value):
        """Decode only approved terminated entities and normalize source CR/CRLF to LF."""
        value = value.replace('\r\n', '\n').replace('\r', '\n')
        result, position = [], 0
        while position < len(value):
            if value[position] != '&':
                result.append(value[position])
                position += 1
                continue
            end = value.find(';', position + 1)
            if end < 0 or end - position > 12:
                raise ValueError('HTML entities require a supported semicolon-terminated spelling')
            entity = value[position + 1:end]
            if entity in Html._entities:
                decoded = Html._entities[entity]
            elif entity.startswith('#'):
                hexadecimal = entity[1:2].lower() == 'x'
                digits = entity[2:] if hexadecimal else entity[1:]
                if not digits or not _re.fullmatch('[0-9a-fA-F]+' if hexadecimal else '[0-9]+', digits):
                    raise ValueError('Malformed HTML numeric reference')
                code = int(digits, 16 if hexadecimal else 10)
                if code > 0x10ffff:
                    raise ValueError('Unsupported HTML character reference')
                decoded = chr(code)
            else:
                raise ValueError('HTML entity is outside the approved subset')
            result.append(Html._text(decoded))
            position = end + 1
        return Html._text(''.join(result))
    @staticmethod
    def _child(open_names, child):
        """Reject content that an independent HTML parser would implicitly rearrange."""
        if child not in Html._tags:
            raise ValueError('HTML tag is outside the approved fragment subset')
        if child == 'li' and (not open_names or open_names[-1] not in ('ul', 'ol')):
            raise ValueError('HTML list items require a direct list parent')
        if open_names:
            parent = open_names[-1]
            if parent in Html._void or parent in ('ul', 'ol') and child != 'li' or (parent in Html._phrasing or parent == 'p' or parent in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6')) and child not in Html._phrasing:
                raise ValueError('HTML child would require unsupported browser recovery')
        if child == 'a' and 'a' in open_names:
            raise ValueError('Nested HTML anchors are outside the approved subset')
    @staticmethod
    def write_fragment(events, limits=None, context=None):
        """Validate an owning event trace before returning deterministic ordinary HTML bytes."""
        limits = limits or HtmlLimits()
        if len(events) > limits.max_events or len(events) < 2 or events[0].kind != 'start_fragment' or events[-1].kind != 'end_fragment':
            raise ValueError('HTML event trace requires bounded explicit fragment boundaries')
        parts, open_names, size, text_bytes = [], [], 0, 0
        def append(value):
            """Bound encoded output before retaining each escaped fragment."""
            nonlocal size
            count = len(value.encode('utf-8'))
            if count > limits.max_output_bytes - size:
                raise ValueError('HTML output byte limit exceeded')
            size += count
            Html._charge(context, 'decoder_allocation_bytes', count)
            parts.append(value)
        def escape(value, attribute=False):
            """Escape semantic HTML text without interpreting its URL or application meaning."""
            Html._text(value)
            append(value.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;').replace('"', '&quot;' if attribute else '"').replace('\r', '&#13;'))
        for index, event in enumerate(events):
            Html._charge(context, 'work_units', 1 + len(event.name) + len(event.text))
            if event.kind in ('start_fragment', 'end_fragment'):
                if index not in (0, len(events) - 1) or open_names or event.name or event.attributes or event.text:
                    raise ValueError('Malformed HTML fragment boundary')
            elif event.kind == 'start_element':
                Html._child(open_names, event.name)
                if event.text or len(open_names) >= limits.max_depth or len(event.attributes) > limits.max_attributes_per_element:
                    raise ValueError('HTML element payload/depth/attribute limit exceeded')
                append('<' + event.name)
                names = set()
                for attribute in event.attributes:
                    key = attribute.name
                    allowed = key in ('id', 'class', 'title', 'lang', 'dir', 'hidden') or event.name == 'a' and key in ('href', 'rel') or event.name == 'img' and key in ('src', 'alt', 'width', 'height')
                    if not allowed or key in names or type(attribute.boolean) is not bool or attribute.boolean != (key == 'hidden') or attribute.boolean and attribute.value:
                        raise ValueError('Unsupported or duplicate HTML attribute')
                    names.add(key)
                    Html._charge(context, 'work_units', len(key) + len(attribute.value))
                    append(' ' + key)
                    if not attribute.boolean:
                        append('="')
                        escape(attribute.value, True)
                        append('"')
                append('>')
                Html._charge(context, 'decoder_allocation_bytes', 64 + len(event.name))
                open_names.append(event.name)
            elif event.kind == 'end_element':
                if not open_names or open_names[-1] != event.name or event.attributes or event.text:
                    raise ValueError('Unbalanced HTML element close')
                if event.name not in Html._void:
                    append('</' + event.name + '>')
                open_names.pop()
            elif event.kind == 'text':
                count = len(event.text.encode('utf-8'))
                if event.name or event.attributes or open_names and open_names[-1] in Html._void or count > limits.max_text_bytes - text_bytes:
                    raise ValueError('Invalid HTML text event or cumulative text bound')
                if open_names and open_names[-1] in ('ul', 'ol') and event.text.strip(Html._space):
                    raise ValueError('HTML lists permit only whitespace between list items')
                text_bytes += count
                escape(event.text)
            else:
                raise ValueError('Unknown HTML event kind')
        return ''.join(parts).encode('utf-8')

)htmlpython"
R"htmlpython(    @staticmethod
    def parse_fragment(data, limits=None, context=None):
        """Parse the approved HTML syntax transactionally without any XML provider or tree."""
        limits = limits or HtmlLimits()
        if not isinstance(data, (bytes, bytearray)) or len(data) > limits.max_input_bytes:
            raise ValueError('HTML input byte limit exceeded')
        Html._charge(context, 'work_units', len(data))
        source = Html._text(bytes(data).decode('utf-8'))
        events, open_names, position = [], [], 0
        def add(event):
            """Bound event retention before storing each owning token."""
            if len(events) >= limits.max_events:
                raise ValueError('HTML event count limit exceeded')
            Html._charge(context, 'decoder_allocation_bytes', 64 + len(event.name) + len(event.text.encode('utf-8')))
            events.append(event)
        def skip_space(offset):
            """Consume the HTML ASCII whitespace set without accepting Unicode separators."""
            while offset < len(source) and source[offset] in Html._space:
                offset += 1
            return offset
        add(HtmlEvent('start_fragment'))
        while position < len(source):
            if source[position] != '<':
                end = source.find('<', position)
                end = len(source) if end < 0 else end
                add(HtmlEvent('text', text=Html._decode(source[position:end])))
                position = end
                continue
            if source.startswith('<!--', position):
                end = source.find('-->', position + 4)
                if end < 0 or '--' in source[position + 4:end]:
                    raise ValueError('Malformed HTML comment')
                position = end + 3
                continue
            position += 1
            closing = position < len(source) and source[position] == '/'
            if closing:
                position += 1
            match = Html._name.match(source, position)
            if match is None:
                raise ValueError('Unsupported HTML declaration or tag syntax')
            name, position = match[0].lower(), match.end()
            if closing:
                position = skip_space(position)
                if position >= len(source) or source[position] != '>' or name in Html._void or not open_names or open_names[-1] != name:
                    raise ValueError('HTML close requires an explicit matching nonvoid start')
                position += 1
                open_names.pop()
                add(HtmlEvent('end_element', name))
                continue
            Html._child(open_names, name)
            if len(open_names) >= limits.max_depth:
                raise ValueError('HTML nesting limit exceeded')
            attributes, self_closing = [], False
            while True:
                separator = position
                position = skip_space(position)
                if position >= len(source):
                    raise ValueError('Truncated HTML start tag')
                if source[position] == '>':
                    position += 1
                    break
                if source.startswith('/>', position):
                    self_closing, position = True, position + 2
                    break
                if separator == position or len(attributes) >= limits.max_attributes_per_element:
                    raise ValueError('HTML attributes require separation and bounded cardinality')
                match = Html._name.match(source, position)
                if match is None:
                    raise ValueError('Unsupported HTML attribute name')
                key, position = match[0].lower(), match.end()
                key_end = position
                position = skip_space(position)
                assigned = position < len(source) and source[position] == '='
                value = ''
                if assigned:
                    position = skip_space(position + 1)
                    if position >= len(source):
                        raise ValueError('Truncated HTML attribute')
                    quote = source[position] if source[position] in ('"', "'") else None
                    if quote:
                        position += 1
                        end = source.find(quote, position)
                        if end < 0:
                            raise ValueError('Unterminated HTML attribute')
                    else:
                        end = position
                        while end < len(source) and source[end] not in Html._space + '>':
                            if source[end] in '\"\'< =`':
                                raise ValueError('Invalid unquoted HTML attribute')
                            end += 1
                        if end == position:
                            raise ValueError('Empty unquoted HTML attribute')
                    value = Html._decode(source[position:end])
                    position = end + (1 if quote else 0)
                else:
                    position = key_end
                if key == 'hidden':
                    if assigned and value and value.lower() != 'hidden':
                        raise ValueError('HTML hidden requires a boolean spelling')
                    value = ''
                elif not assigned:
                    raise ValueError('Only approved boolean HTML attributes may omit a value')
                attributes.append(HtmlAttribute(key, value, key == 'hidden'))
            if self_closing and name not in Html._void:
                raise ValueError('HTML self-closing nonvoid elements require browser recovery')
            add(HtmlEvent('start_element', name, attributes))
            if name in Html._void:
                add(HtmlEvent('end_element', name))
            else:
                open_names.append(name)
        if open_names:
            raise ValueError('HTML fragment has unclosed elements')
        add(HtmlEvent('end_fragment'))
        Html.write_fragment(events, limits, context)
        return events

)htmlpython"
