"""Read an owner's live field, then translate it during one synchronous operation."""
import gc

from schema import BorrowedB, Consumer, Owner, ProjectedValue, Protocol, WeakB


def project_json(value):
    """Return owned bytes; neither the selected field nor its projection escapes."""
    return value.with_to_wire(lambda projected: projected.encode(Protocol.JSON))


def main():
    """Bind application identities explicitly and resolve addresses after loading owners."""
    storage = Owner()
    storage.b.text, storage.b.count = "original", 1
    links = Consumer()
    links.d.bind(storage, "document-1", 7, 1)
    borrowed, weak = BorrowedB(), WeakB()
    borrowed.bind(storage, "document-1", 7, 1)
    weak.bind(storage, "document-1", 7, 1)

    # d selects the current b slot rather than an earlier snapshot of its contents.
    storage.b.text, storage.b.count = "updated", 2
    payload = links.d.with_value(project_json)
    received = ProjectedValue.decode(payload, Protocol.JSON)
    assert (received.label, received.count) == ("updated", 2)
    print(payload.decode("utf-8"))

    # The application persists owners separately and supplies the identity lookup.
    loaded_owner = Owner.decode(storage.encode(Protocol.JSON), Protocol.JSON)
    loaded = Consumer.decode(links.encode(Protocol.JSON), Protocol.JSON)
    assert not loaded.d.is_bound()
    assert loaded.d.resolve(loaded_owner, "document-1", 7, 1)
    assert loaded.d.with_value(lambda value: value.text) == "updated"
    loaded.d.unbind()

    # Shared retains the original owner; borrowed and weak do not retain it.
    del storage
    gc.collect()
    assert links.d.is_bound()
    links.d.unbind()
    gc.collect()
    assert not borrowed.is_bound() and not weak.is_bound()


if __name__ == "__main__":
    main()
