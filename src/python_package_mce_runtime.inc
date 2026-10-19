// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"mcepython(
_PKG_MC = 'http://schemas.openxmlformats.org/markup-compatibility/2006'
_PKG_XML = 'http://www.w3.org/XML/1998/namespace'


class _PackageRelationshipsCompatibility:
    """Apply OPC's Relationships MCE processing model using the existing owning tree and ledger."""
    def __init__(self, budget):
        """Borrow one synchronous operation context; application vocabularies remain unclaimed."""
        self.budget = budget
    def prefix(self, node, value):
        """Resolve an NCName prefix in its original scope without changing inherited URI identities."""
        self.budget.consume('work_units', len(value) + 1)
        if not _xml_ncname(value): raise ValueError('Invalid OPC compatibility prefix')
        uri = _PKG_XML if value == 'xml' else node[4].get(value)
        if not uri or uri == _PKG_MC: raise ValueError('Unbound OPC compatibility prefix')
        return uri
    def tokens(self, value):
        """Visit exactly XML-whitespace-delimited declarations with bounded temporary token storage."""
        self.budget.consume('work_units', len(value) + 1)
        self.budget.consume('decoder_allocation_bytes', len(value) * 8)
        return _xml_tokens(value)
    def understood(self, uri):
        """Use OPC's exact application configuration: only the Relationships vocabulary is understood."""
        return uri == _PKG_RELS
    def ignorable(self, scope, uri):
        """Search borrowed ancestor declarations without copying accumulated scope sets."""
        while scope is not None:
            for candidate in scope[1]:
                self.budget.consume('work_units', 1)
                if candidate == uri: return True
            scope = scope[0]
        return False
    def process_content(self, scope, name):
        """Match ProcessContent expanded names or namespace wildcards across prefix rebindings."""
        uri, local = _xml_namespace(name), name.rsplit('}', 1)[-1]
        while scope is not None:
            for selected_uri, selected_local in scope[2]:
                self.budget.consume('work_units', 1)
                if selected_uri == uri and selected_local in ('*', local): return True
            scope = scope[0]
        return False
    def declarations(self, node, parent):
        """Validate local syntax even on discarded nodes, then retain only local borrowed rule values."""
        self.budget.consume('decoder_allocation_bytes', 64)
        scope = (parent, [], [])
        for name, value in node[1].items():
            self.budget.consume('work_units', 1)
            if name == '{' + _PKG_MC + '}Ignorable':
                for token in self.tokens(value):
                    self.budget.consume('decoder_allocation_bytes', 8)
                    scope[1].append(self.prefix(node, token))
        for name, value in node[1].items():
            if _xml_namespace(name) != _PKG_MC: continue
            local = name.rsplit('}', 1)[-1]
            if local == 'ProcessContent':
                for token in self.tokens(value):
                    prefix, separator, selected = token.partition(':')
                    if not separator or not prefix or selected != '*' and not _xml_ncname(selected):
                        raise ValueError('Invalid OPC ProcessContent name')
                    uri = self.prefix(node, prefix)
                    if not self.ignorable(scope, uri): raise ValueError('OPC ProcessContent namespace is not ignorable')
                    self.budget.consume('decoder_allocation_bytes', 64)
                    scope[2].append((uri, selected))
            elif local == 'MustUnderstand':
                for token in self.tokens(value): self.prefix(node, token)
            elif local != 'Ignorable':
                raise ValueError('Unknown OPC compatibility attribute')
        return scope
    def must_understand(self, node):
        """Check capabilities only on retained, unwrapped or selected content."""
        value = node[1].get('{' + _PKG_MC + '}MustUnderstand')
        if value is not None:
            for token in self.tokens(value):
                if not self.understood(self.prefix(node, token)):
                    raise ValueError('Unsupported required OPC compatibility namespace')
    def wrapper(self, node, scope, choice):
        """Validate complete Choice/Fallback attributes even when their branch will not be selected."""
        requires = None
        for name, value in node[1].items():
            self.budget.consume('work_units', 1)
            uri = _xml_namespace(name)
            if not uri:
                if not choice or name != 'Requires': raise ValueError('Unexpected OPC compatibility wrapper attribute')
                requires = value
            elif uri != _PKG_MC and (uri == _PKG_XML or not self.ignorable(scope, uri)):
                raise ValueError('Invalid qualified OPC compatibility wrapper attribute')
        if not choice: return False
        if requires is None or not requires.strip(' \t\r\n'):
            raise ValueError('OPC Choice requires namespace prefixes')
        supported = True
        for token in self.tokens(requires):
            if not self.understood(self.prefix(node, token)): supported = False
        return supported
    def children(self, node, scope):
        """Flatten selected fragments in linear time while retaining exact mixed-text positions."""
        if not node[2]: return
        self.budget.consume('decoder_allocation_bytes', len(node[2]) * 8)
        actions, changed = [], False
        child_count, segment_count, text_characters = 0, len(node[5]), len(node[3])
        for child in node[2]:
            action = self.process(child, scope)
            actions.append(action)
            changed = changed or action != 'keep'
            if action == 'keep': child_count += 1
            elif action == 'unwrap':
                child_count += len(child[2])
                segment_count += len(child[5])
                text_characters += len(child[3])
        if not changed: return
        self.budget.consume('decoder_allocation_bytes', child_count * 8 + segment_count * 64 + text_characters * 4)
        children, segments, next_segment = [], [], 0
        for index in range(len(node[2]) + 1):
            while next_segment < len(node[5]) and node[5][next_segment][0] == index:
                value = node[5][next_segment][1]
                self.budget.consume('work_units', len(value) + 1)
                segments.append((len(children), value))
                next_segment += 1
            if index == len(node[2]): break
            child, action = node[2][index], actions[index]
            if action == 'keep': children.append(child)
            elif action == 'unwrap':
                offset = len(children)
                for position, value in child[5]:
                    self.budget.consume('work_units', len(value) + 1)
                    segments.append((offset + position, value))
                children.extend(child[2])
        node[2], node[5] = children, segments
        node[3] = ''.join(value for unused, value in segments)
    def alternate(self, node, scope):
        """Choose the first satisfied Requires set, otherwise Fallback, after checking sibling order."""
        self.wrapper(node, scope, False)
        if node[3].strip(' \t\r\n'): raise ValueError('Text in OPC AlternateContent wrapper')
        choices, selected, fallback = 0, None, False
        for index, child in enumerate(node[2]):
            child_scope = self.declarations(child, scope)
            uri, local = _xml_namespace(child[0]), child[0].rsplit('}', 1)[-1]
            if not self.understood(uri) and self.ignorable(child_scope, uri) and not self.process_content(child_scope, child[0]): continue
            if uri != _PKG_MC: raise ValueError('Unexpected OPC AlternateContent child')
            if local == 'Choice' and not fallback:
                choices += 1
                supported = self.wrapper(child, child_scope, True)
                if selected is None and supported: selected = index
            elif local == 'Fallback' and choices and not fallback:
                fallback = True
                self.wrapper(child, child_scope, False)
                if selected is None: selected = index
            else:
                raise ValueError('Invalid OPC Choice/Fallback ordering')
        if not choices: raise ValueError('OPC AlternateContent has no Choice')
        self.must_understand(node)
        if selected is None:
            node[2], node[3], node[5] = [], '', []
        else:
            branch = node[2][selected]
            self.process(branch, scope, True)
            node[2], node[3], node[5] = branch[2], branch[3], branch[5]
    def process(self, node, parent, selected_branch=False):
        """Apply scoped capabilities before any discarded or unselected subtree can affect OPC."""
        self.budget.consume('work_units', 1)
        scope = self.declarations(node, parent)
        uri, local = _xml_namespace(node[0]), node[0].rsplit('}', 1)[-1]
        if not self.understood(uri) and self.ignorable(scope, uri):
            if not self.process_content(scope, node[0]): return 'discard'
            if any(name in node[1] for name in ('{' + _PKG_XML + '}' + selected for selected in ('base', 'lang', 'space'))):
                raise ValueError('XML context attribute on unwrapped OPC element')
            self.must_understand(node)
            self.children(node, scope)
            return 'unwrap'
        if uri == _PKG_MC:
            if local == 'AlternateContent':
                self.alternate(node, scope)
                return 'unwrap'
            if not selected_branch or local not in ('Choice', 'Fallback'):
                raise ValueError('Unexpected OPC compatibility element')
            self.wrapper(node, scope, local == 'Choice')
            self.must_understand(node)
            self.children(node, scope)
            return 'unwrap'
        self.must_understand(node)
        self.children(node, scope)
        self.budget.consume('decoder_allocation_bytes', len(node[1]) * 64)
        node[1] = {name: value for name, value in node[1].items() if _xml_namespace(name) != _PKG_MC and
                   (self.understood(_xml_namespace(name)) or not self.ignorable(scope, _xml_namespace(name)))}
        return 'keep'
    def apply(self, document):
        """Allow AlternateContent at the root only when processing yields exactly one XML document."""
        self.budget.consume('decoder_allocation_bytes', 128)
        container = ['', {}, [document], '', {}, []]
        self.children(container, None)
        if len(container[2]) != 1 or container[3].strip(' \t\r\n'):
            raise ValueError('OPC compatibility output must have exactly one root')
        return container[2][0]


def _pkg_relationships_compatibility(document, budget):
    """Keep the ordinary Relationships path cheap while bounding the metadata-only compatibility pass."""
    def present(node):
        """Detect only compatibility vocabulary, charging the complete preflight traversal."""
        budget.consume('work_units', len(node[1]) + 1)
        return _xml_namespace(node[0]) == _PKG_MC or any(_xml_namespace(name) == _PKG_MC for name in node[1]) or any(present(child) for child in node[2])
    return _PackageRelationshipsCompatibility(budget).apply(document) if present(document) else document
)mcepython"
