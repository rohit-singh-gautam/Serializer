// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {readFile, writeFile} from 'node:fs/promises';
import {IdentityPackageArchive as A, IdentityXmlRegistry as Model,
        SerializerPackage as P, SerializerCompression as C} from '@module@';

let passed = 0;
// Keep public SDK checks independent of runtime assertion flags.
function check(condition, message) { if (!condition) throw new Error(message); ++passed; }
const bytes = new Uint8Array(await readFile('fixtures/valid.opc'));
const provider = await C.nodeProvider();
const host = await P.nodeHost();
const baseline = new P.Budget();
const ordinary = await A.load(bytes, {budget: baseline, compressionProvider: provider});
check(ordinary.value.registries.length === 3 && baseline.used.references === 15, 'old default profile shares fifteen references');
let emptyCalls = 0;
const emptyDecoder = {registries: {decode(text, limits) { ++emptyCalls; return Model.decodeXml(text, limits); }}};
const empty = await A.load(new Uint8Array(await readFile('fixtures/empty.opc')), {compressionProvider: provider, codecs: emptyDecoder});
check(empty.value.registries.length === 3 && emptyCalls === 3, 'small valid empty XML passes typed dispatch');
emptyCalls = 0;
try {
  await A.load(new Uint8Array(await readFile('fixtures/wrong-root.opc')), {compressionProvider: provider, codecs: emptyDecoder});
  throw new Error('wrong XML root accepted');
} catch (failure) { check(failure instanceof P.Error && emptyCalls === 0, 'wrong root rejects before selected native callback'); }
const codecs = {registries: {decode(text, limits, context) {
  check(!!context.part && !!context.relationshipId && context.role === 'registries', 'public selected native origin');
  Model.decodeXml(text, limits);
  return Model.decodeXml(text, limits);
}}};
const measured = new P.Budget();
const composed = await A.load(bytes, {budget: measured, compressionProvider: provider, codecs});
check(composed.value.registries.length === 3 && measured.used.references === 27, 'selected native decoders share twenty-seven references');
for (const resource of ['references', 'workUnits', 'decoderAllocationBytes']) {
  const limit = {references: 'maxReferences', workUnits: 'maxWorkUnits', decoderAllocationBytes: 'maxDecoderAllocationBytes'}[resource];
  for (const tight of [false, true]) {
    try {
      await A.load(bytes, {compressionProvider: provider, budget: new P.Budget(new P.Limits({[limit]: measured.used[resource] - Number(tight)})), codecs});
      check(!tight, 'one-tight selected resource accepted');
    } catch (failure) {
      check(tight && failure instanceof P.Error && failure.code === 'resource_limit', 'selected native resource classification');
      check(!!failure.part && !!failure.fieldPath && !!failure.relationshipId, 'selected native quota location');
    }
  }
}
const marker = new Error('original caller error');
let calls = 0;
try {
  await A.load(bytes, {compressionProvider: provider, codecs: {registries: {decode(text, limits, context) {
    if (++calls === 2) {
      check(context.part === '/Registries/registry1.xml' && context.relationshipId === 'record1', 'middle selected caller origin');
      throw marker;
    }
    return Model.decodeXml(text, limits);
  }}}});
  throw new Error('caller failure accepted');
} catch (failure) { check(failure === marker && calls === 2, 'original selected callback exception identity'); }
let swallowed;
try {
  await A.load(bytes, {compressionProvider: provider, budget: new P.Budget(new P.Limits({maxReferences: 3})), codecs: {registries: {decode(text, limits) {
    try { return Model.decodeXml(text, limits); }
    catch (failure) { swallowed = failure; return new Model(); }
  }}}});
  throw new Error('swallowed native resource failure accepted');
} catch (failure) { check(swallowed !== undefined && failure === swallowed, 'selected callbacks cannot suppress first native budget failure'); }
let cancel = false;
calls = 0;
const cancelled = new P.Budget(new P.Limits({checkCancelled() { if (cancel) throw marker; }}));
try {
  await A.load(bytes, {budget: cancelled, compressionProvider: provider, codecs: {registries: {decode(text, limits) {
    if (++calls === 2) cancel = true;
    return Model.decodeXml(text, limits);
  }}}});
  throw new Error('middle native cancellation accepted');
} catch (failure) { check(failure === marker && calls === 2, 'original middle native cancellation identity'); }
await writeFile('fixtures/atomic.opc', 'previous destination');
calls = 0;
try {
  await A.save('fixtures/atomic.opc', ordinary.value, {host, compressionProvider: provider, codecs: {registries: {encode(model, limits) {
    if (++calls === 2) throw marker;
    return model.encodeXml(limits);
  }}}});
  throw new Error('middle selected Save failure accepted');
} catch (failure) {
  check(failure === marker && await readFile('fixtures/atomic.opc', 'utf8') === 'previous destination', 'selected middle Save failure is atomic');
}
for (const value of [{unknown: {}}, {registries: {decode: async () => new Model()}}]) {
  try { await A.load(bytes, {compressionProvider: provider, codecs: value}); throw new Error('invalid selected callback accepted'); }
  catch (failure) { check(failure instanceof P.Error && failure.code === 'invalid_options', 'closed role and synchronous callback contracts'); }
}
const output = await A.encode(ordinary.value, {compressionProvider: provider, codecs: {registries: {encode(model, limits) { return model.encodeXml(limits); }}}});
await writeFile('fixtures/produced.opc', output);
@typed_api@
console.log(JSON.stringify({passed, default_references: 15, composed_references: 27}));
