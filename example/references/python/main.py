"""Export existing payload storage through transparent borrowed and shared fields."""
from schema import *

# Both ordinary records retain the same independent value; no parent owner is shared.
value = Payload()
value.values = [7.0, 8.0]
a = SharedOwner()
a.b.bind(value)
c = SharedValueConsumer()
c.d.bind(value)
assert a.b.read_value() is value and c.d.read_value() is value
value = None
a.b.unbind()
assert c.d.read_value().values == [7.0, 8.0]
independent_expected = OwningConsumer()
independent_expected.d = c.d.read_value()
for protocol in Protocol:
    assert c.encode(protocol) == independent_expected.encode(protocol)

owner = Owner()
owner.b.values = [1.0, 2.0, 3.0]
owner.b.codes = [0, 1, 2]
borrowed = BorrowedConsumer()
borrowed.d.bind(owner)
shared = SharedConsumer()
shared.d.bind(owner)
weak = WeakConsumer()
weak.d.bind(owner)
assert shared.d.read_value() is owner.b
assert shared.d.read_value().values is owner.b.values
owner.b.values[0] = 4.0
expected = OwningConsumer()
expected.d = owner.b
for protocol in Protocol:
    encoded = expected.encode(protocol)
    assert borrowed.encode(protocol) == encoded
    assert shared.encode(protocol) == encoded
    assert weak.encode(protocol) == encoded
    # The translator borrows the original payload just like the reference fields.
    def check_projection(projected):
        """Compare the scoped export while its source remains alive."""
        assert projected.get_d() is owner.b
        assert projected.encode(protocol) == encoded
    owner.with_as_consumer(check_projection)
    loaded = OwningConsumer.decode(encoded, protocol)
    assert loaded.encode(protocol) == encoded
    loaded.d.values[0] = 99.0
    assert owner.b.values[0] == 4.0
print(shared.encode(Protocol.JSON).decode())
