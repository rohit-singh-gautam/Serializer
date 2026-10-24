// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
R"jsReference(
const srlManagedNames = Object.freeze(['document_high', 'document_low', 'class_id', 'object_id']);
const srlManagedKeys = Object.freeze(srlManagedNames.map(srlMakeField));
/** Nonowning exact wire coordinates. Decoding never establishes liveness. */
export class ManagedReference {
  constructor(documentHigh, documentLow, classId, objectId) {
    this.documentHigh = documentHigh; this.documentLow = documentLow;
    this.classId = classId; this.objectId = objectId; Object.freeze(this);
  }
  /** Validate raw metadata against the generated target and selected native ID width. */
  validate(expectedClassId, idBytes) {
    const unsigned64 = value => typeof value === 'bigint' && value >= 0n && value <= 0xffffffffffffffffn;
    if (!unsigned64(this.documentHigh) || !unsigned64(this.documentLow) ||
        (this.documentHigh | this.documentLow) === 0n || !unsigned64(this.classId) ||
        this.classId === 0n || this.classId !== expectedClassId ||
        (idBytes === 4 ? typeof this.objectId !== 'number' || !Number.isInteger(this.objectId) ||
          this.objectId < 1 || this.objectId > 0xffffffff :
          !unsigned64(this.objectId) || this.objectId === 0n)) {
      throw new RangeError('Invalid managed reference envelope');
    }
  }
}
Object.freeze(ManagedReference.prototype); Object.freeze(ManagedReference);
/** Validate exact instances before a field can expose any outgoing reference bytes. */
function srlManagedValidate(value, expectedClassId, idBytes) {
  if (!value || Object.getPrototypeOf(value) !== ManagedReference.prototype) throw new TypeError('Expected exact native managed reference');
  value.validate(expectedClassId, idBytes);
}
/** Preserve C++ envelope framing; only object_id follows the compact identity policy. */
function srlManagedWrite(output, value, expectedClassId, idBytes, compact) {
  srlManagedValidate(value, expectedClassId, idBytes); output.beginObject();
  output.field(1, srlManagedKeys[0], true); output.integer(value.documentHigh, 8, true);
  output.field(2, srlManagedKeys[1], false); output.integer(value.documentLow, 8, true);
  output.field(3, srlManagedKeys[2], false); output.integer(value.classId, 8, true);
  output.field(4, srlManagedKeys[3], false);
  if (compact) output.compactVarint(value.objectId, idBytes); else output.integer(value.objectId, idBytes, true);
  output.endObject();
}
/** Reject every unknown, missing or duplicate metadata field independently of outer read policy. */
function srlManagedRead(input, expectedClassId, idBytes, compact) {
  input.beginObject(); const values = []; let seen = 0;
  const names = srlManagedNames;
  if (input.protocol === Protocol.BINARY_NONE) {
    for (let field = 0; field < names.length; ++field) values[field] = field === 3 && compact ?
      input.compactVarint(idBytes) : input.integer(field === 3 ? idBytes : 8, true);
    seen = 15;
  } else {
    let first = true;
    while (true) {
      let field;
      if (input.protocol === Protocol.BINARY_INTEGER) { field = input.compact(); if (field === 0) break; }
      else {
        if (input.json() && !input.nextObject(first)) break;
        first = false; const key = input.key(); if (!input.json() && key === '') break;
        field = names.indexOf(key) + 1;
      }
      if (field < 1 || field > names.length) input.fail('Unknown managed reference metadata');
      const mask = 1 << (field - 1); if (seen & mask) input.fail('Duplicate managed reference metadata'); seen |= mask;
      values[field - 1] = field === 4 && compact ? input.compactVarint(idBytes) : input.integer(field === 4 ? idBytes : 8, true);
    }
  }
  input.endObject(); if (seen !== 15) input.fail('Incomplete managed reference metadata');
  const result = new ManagedReference(...values); result.validate(expectedClassId, idBytes); return result;
}
)jsReference"
