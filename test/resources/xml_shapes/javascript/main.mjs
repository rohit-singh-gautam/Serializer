// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import assert from 'node:assert/strict';
import {Protocol, ShapesRecord, ShapesChild, ShapesState, ShapesCatalog, XmlLimits} from './schema.mjs';

// Qualify generated nested, optional, fixed, enum and exact scalar mappings without a JSON bridge.
const catalogXml = "<catalog xmlns='urn:serializer:vocabulary'><prices><price currency='USD'>12.5</price></prices><labels><label>A&amp;B</label><label/></labels></catalog>";
const catalog = ShapesCatalog.decodeXml(catalogXml);
assert.equal(catalog.prices[0].value, 12.5); assert.equal(catalog.prices[0].currency, 'USD');
assert.deepEqual(catalog.labels, ['A&B', '']);
assert.deepEqual(catalog.encode(Protocol.BINARY_NONE), ShapesCatalog.decodeXml(catalog.encodeXml()).encode(Protocol.BINARY_NONE));
assert.deepEqual(ShapesCatalog.decodeXml(new ShapesCatalog().encodeXml()).prices, []);
assert.throws(() => ShapesCatalog.decodeXml(catalogXml.replace('<prices>', "<prices extra='1'>")));
const value = new ShapesRecord();
value.big = (1n << 64n) - 1n;
value.label = '\tX\r\n\u{1f600}';
value.flag = [true];
value.nested.value = -32768;
const child = new ShapesChild();
child.value = 32767;
value.children = [child];
value.states = [ShapesState.Zero, ShapesState.One];
value.amount = Infinity;
value.small = 1.25;
const encoded = value.encodeXml();
assert.deepEqual(ShapesRecord.decodeXml(encoded).encode(Protocol.BINARY_NONE), value.encode(Protocol.BINARY_NONE));
value.flag = [false];
assert.deepEqual(ShapesRecord.decodeXml(value.encodeXml()).flag, [false]);
value.flag = [];
assert.deepEqual(ShapesRecord.decodeXml(value.encodeXml()).flag, []);
assert.deepEqual(ShapesRecord.decodeXml(encoded.replace('big="18446744073709551615"', 'big="-000"')).big, 0n);
assert.throws(() => ShapesRecord.decodeXml(encoded.replace('big="18446744073709551615"', 'big="-"')));
assert.throws(() => ShapesRecord.decodeXml(encoded.replace('<value>-32768</value>', '<value>\u00a0-32768</value>')));
assert.throws(() => ShapesRecord.decodeXml(encoded.replace('<states>one</states>', '')));
assert.throws(() => ShapesRecord.decodeXml(encoded.replace('<states>one</states>', '<states>unknown</states>')));
assert.throws(() => value.encodeXml(new XmlLimits({maxDepth: 1})));
value.small = -0;
const negativeZero = ShapesRecord.decodeXml(value.encodeXml());
assert.deepEqual(negativeZero.encode(Protocol.BINARY_NONE), value.encode(Protocol.BINARY_NONE));
// The caller's prepared validator is invoked in both configured directions.
let validations = 0;
const validator = { validate(document) { ++validations; throw new Error('Prepared XSD rejection'); } };
assert.throws(() => ShapesRecord.decodeXml(encoded, new XmlLimits(), validator), /Prepared XSD rejection/);
assert.throws(() => value.encodeXml(new XmlLimits(), validator), /Prepared XSD rejection/);
assert.deepEqual(validations, 2);
console.log('JavaScript generated typed XML and prepared-validator hooks passed');
