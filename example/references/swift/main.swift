// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD

// Independent shared values can outlive either parent record without sharing the parents.
var independent: SrlReferenceOwner<Payload>? = SrlReferenceOwner(Payload())
independent!.value.values = [11]
var first: SharedOwner? = SharedOwner()
var second = SharedValueConsumer()
first!.b.bind(independent!)
second.d.bind(independent!)
first = nil
independent = nil
let independentValue = try second.d.readValue()
precondition(independentValue.values == [11])

// Stable boxed identity lets Swift value records participate in export-only live bindings.
var source: SrlReferenceOwner<Owner>? = SrlReferenceOwner(Owner())
source!.value.b.values = [0, 1, 2]
source!.value.b.codes = [0, 1, 2]
var borrowed = BorrowedConsumer()
borrowed.d.bind(source!)
var shared = SharedConsumer()
shared.d.bind(source!)
var weak = WeakConsumer()
weak.d.bind(source!)
source!.value.b.values[0] = 9

// Swift arrays keep their copy-on-write buffers while a scoped value is read.
try shared.d.withValue { selected in
  selected.values.withUnsafeBufferPointer { selectedBuffer in
    source!.value.b.values.withUnsafeBufferPointer { originalBuffer in
      precondition(selectedBuffer.baseAddress == originalBuffer.baseAddress)
    }
  }
}
for protocolValue in [Protocol.JSON, .BINARY_NONE, .BINARY_INTEGER, .BINARY_STRING] {
  // The ordinary owning value is only a wire-equivalence baseline.
  var expected = OwningConsumer()
  expected.d = source!.value.b
  let bytes = try expected.encode(protocolValue)
  let borrowedBytes = try borrowed.encode(protocolValue)
  let sharedBytes = try shared.encode(protocolValue)
  let weakBytes = try weak.encode(protocolValue)
  precondition(borrowedBytes == bytes && sharedBytes == bytes && weakBytes == bytes)
  try source!.value.withAsConsumer { projected in
    let projectedBytes = try projected.encode(protocolValue)
    precondition(projectedBytes == bytes)
  }
  // Output-only references reject input; an ordinary target owns decoded buffers.
  var rejectedReferenceInput = false
  do { _ = try SharedConsumer.decode(bytes, protocolValue) } catch { rejectedReferenceInput = true }
  precondition(rejectedReferenceInput)
  let decoded = try OwningConsumer.decode(bytes, protocolValue)
  precondition(decoded.d.values == expected.d.values)
  decoded.d.values.withUnsafeBufferPointer { selectedBuffer in
    source!.value.b.values.withUnsafeBufferPointer { originalBuffer in
      precondition(selectedBuffer.baseAddress != originalBuffer.baseAddress)
    }
  }
  source!.value.b.values[0] = 10
  let decodedBytes = try decoded.encode(protocolValue)
  precondition(decodedBytes == bytes)
  source!.value.b.values[0] = 9
}
print(String(decoding: try shared.encode(.JSON), as: UTF8.self))
source = nil
precondition(shared.d.isBound() && weak.d.isBound())
shared.d.unbind()
precondition(!borrowed.d.isBound() && !weak.d.isBound())
var rejectedExpiredOwner = false
do { _ = try weak.encode(.JSON) } catch { rejectedExpiredOwner = true }
precondition(rejectedExpiredOwner)
