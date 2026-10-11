"""Application-owned scoped IDs over ordinary generated records; no native managed runtime is implied."""
from schema import CustomId, EntityBinding, RegistryState, Protocol

WIDGET_DOMAIN = 1
GADGET_DOMAIN = 2


def make_id(scope, domain, local):
    """Create an independent value key, without using an object's runtime address."""
    result = CustomId()
    result.scope, result.domain, result.local = scope, domain, local
    return result


def key(value):
    """Use the complete scope/domain/local tuple as the application identity."""
    return value.scope, value.domain, value.local


def valid(value):
    """Reserve zero values and accept only documented stable domain tags."""
    return value.scope != 0 and value.local != 0 and value.domain in (WIDGET_DOMAIN, GADGET_DOMAIN)


class ApplicationRegistry:
    """A single-threaded host owns binding, lookup, and permanent retirement policy."""
    def __init__(self, state=None):
        """Take an owned decoded state and validate it before exposing lookups."""
        self.state = state if state is not None else RegistryState()
        if not self.validate():
            raise ValueError("Invalid custom-ID registry state")

    def validate(self):
        """Reject invalid, duplicate, and simultaneously live/retired identities."""
        ids = [binding.id for binding in self.state.bindings] + list(self.state.retired)
        return all(valid(value) for value in ids) and len({key(value) for value in ids}) == len(ids)

    def lookup(self, identity, expected_domain):
        """Wrong domains and absent/retired keys return no payload."""
        if valid(identity) and identity.domain == expected_domain:
            for binding in self.state.bindings:
                if key(binding.id) == key(identity):
                    return binding.payload
        return None

    def bind(self, identity, payload):
        """Copy a new key; caller mutation cannot redirect the owned binding."""
        if not valid(identity) or self.lookup(identity, identity.domain) is not None or any(key(value) == key(identity) for value in self.state.retired):
            return False
        binding = EntityBinding()
        binding.id = make_id(*key(identity))
        binding.payload = payload
        self.state.bindings.append(binding)
        return True

    def rebind(self, identity, payload):
        """Replace payload while preserving the complete logical identity."""
        for binding in self.state.bindings:
            if key(binding.id) == key(identity):
                binding.payload = payload
                return True
        return False

    def retire(self, identity):
        """Persist a tombstone before removing the live binding; retired keys cannot be reused."""
        for index, binding in enumerate(self.state.bindings):
            if key(binding.id) == key(identity):
                self.state.retired.append(make_id(*key(binding.id)))
                self.state.bindings.pop(index)
                return True
        return False


def main():
    """Run host policy checks and revalidate every fresh codec restoration."""
    registry = ApplicationRegistry()
    first, scoped, typed = make_id(7, WIDGET_DOMAIN, 42), make_id(8, WIDGET_DOMAIN, 42), make_id(7, GADGET_DOMAIN, 42)
    assert registry.bind(first, 10) and registry.bind(scoped, 20) and registry.bind(typed, 30)
    assert not registry.bind(first, 99) and registry.lookup(first, WIDGET_DOMAIN) == 10
    assert registry.lookup(typed, WIDGET_DOMAIN) is None
    assert registry.lookup(make_id(9, WIDGET_DOMAIN, 42), WIDGET_DOMAIN) is None
    assert registry.lookup(make_id(7, WIDGET_DOMAIN, 99), WIDGET_DOMAIN) is None
    assert registry.rebind(first, 11) and registry.lookup(first, WIDGET_DOMAIN) == 11
    assert key(registry.state.bindings[0].id) == key(first)
    assert registry.retire(first) and registry.lookup(first, WIDGET_DOMAIN) is None
    assert not registry.bind(first, 12) and not registry.rebind(first, 12)
    canonical = registry.state.encode(Protocol.BINARY_NONE)
    for protocol in Protocol:
        restored = ApplicationRegistry(RegistryState.decode(registry.state.encode(protocol), protocol))
        assert restored.state.encode(Protocol.BINARY_NONE) == canonical
        assert not restored.bind(first, 12) and restored.lookup(scoped, WIDGET_DOMAIN) == 20
    print("Scoped custom IDs and four protocols passed")


if __name__ == "__main__":
    main()
