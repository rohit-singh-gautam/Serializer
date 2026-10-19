// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD

// The box supplies stable owner identity for generated Swift value-type records.
var storage: SrlReferenceOwner<Owner>? = SrlReferenceOwner(Owner())
storage!.value.b.text = "original"
storage!.value.b.count = 1
var links = Consumer()
try links.d.bind(storage!, "document-1", 7, 1)
var borrowed = BorrowedB()
try borrowed.bind(storage!, "document-1", 7, 1)
var weak = WeakB()
try weak.bind(storage!, "document-1", 7, 1)

storage!.value.b.text = "updated"
storage!.value.b.count = 2
let payload = try links.d.withValue { selected in
  // Swift field reads follow value semantics; only owned encoded bytes escape the scope.
  var value = selected
  return try value.withToWire { projected in try projected.encode(.JSON) }
}
let received = try ProjectedValue.decode(payload, .JSON)
precondition(received.label == "updated" && received.count == 2)
print(String(decoding: payload, as: UTF8.self))

// Restore owner values first, then resolve the separately decoded address explicitly.
let ownerBytes = try storage!.value.encode(.JSON)
let loadedOwner = SrlReferenceOwner(try Owner.decode(ownerBytes, .JSON))
var loaded = try Consumer.decode(links.encode(.JSON), .JSON)
precondition(!loaded.d.isBound())
let resolved = loaded.d.resolve(loadedOwner, "document-1", 7, 1)
precondition(resolved)
let loadedText = try loaded.d.withValue { $0.text }
precondition(loadedText == "updated")

// Shared retains the original box; borrowed and weak expire after its final strong release.
storage = nil
precondition(links.d.isBound())
links.d.unbind()
precondition(!borrowed.isBound() && !weak.isBound())
