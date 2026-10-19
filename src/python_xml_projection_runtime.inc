// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"pyprojection(
class _XmlProjectionAccess:
    """Borrow translator getters during one synchronous export without constructing or copying its owning target."""
    __slots__ = ('_target', '_getters', '_state_owner')
    def __init__(self, target, getters, state_owner):
        """Retain only immutable target metadata, bound getter handles and XML-only namespace presentation."""
        self._target, self._getters, self._state_owner = target, getters, state_owner
    def __getattr__(self, name):
        """Resolve a native top-level field lazily; nested objects and containers remain the original borrowed values."""
        selected = self._getters.get(name)
        if selected is not None:
            return selected()
        if name.startswith('_xml_'):
            return getattr(self._target, name)
        raise AttributeError(name)
    @property
    def _srl_xml_state(self):
        """Borrow only the view's XML namespace/opaque state; source field values remain unchanged."""
        return getattr(self._state_owner, '_srl_xml_state', None)
    @_srl_xml_state.setter
    def _srl_xml_state(self, value):
        """Retain explicitly authored XML presentation on the scoped view rather than its owning source."""
        self._state_owner._srl_xml_state = value


class _XmlReferenceValues:
    """Borrow a native sequence of references without materializing its selected owning values."""
    __slots__ = ('_values',)
    def __init__(self, values):
        """Keep the original container only during the current synchronous XML traversal."""
        self._values = values
    def __len__(self):
        """Expose the original collection's checked cardinality."""
        return len(self._values)
    def __iter__(self):
        """Pin each selected owner while its original field value is visited."""
        for value in self._values:
            yield value.read_value()
    def __getitem__(self, index):
        """Borrow one indexed selected value without creating a copied collection."""
        return self._values[index].read_value()


def _xml_export_only(model, active=None):
    """Refuse XML input for graphs whose readonly references cannot invent or bind an external owner."""
    if getattr(model, '_xml_export_only', False):
        return True
    active = set() if active is None else active
    if model in active:
        return False
    active.add(model)
    def projected(item):
        """Include mapped payloads whose native handles differ from their selected XML value types."""
        if item.get('value_projection') or item.get('projection_payload'):
            return True
        if item['map'] is not None:
            return projected(item['map']['key']) or projected(item['map']['value'])
        return item.get('variant') is not None and any(projected(value) for value in item['variant']['alternatives'])
    if any(projected(item) for item in model._xml_token_fields):
        return True
    return any(_xml_export_only(selected(), active)
               for item in model._xml_token_fields for selected in _xml_token_models(item)) or (
                   model._xml_mixed is not None and any(_xml_export_only(item['model'](), active)
                       for item in model._xml_mixed['children']))
)pyprojection"
