// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import assert from 'node:assert/strict';
import {Protocol, ShapesRecord, ShapesChild, ShapesState, ShapesCatalog, XmlLimits, type XmlValidator} from './schema.mjs';

// Verify generated declarations match complete typed XML behavior, including optional tuple shapes.
const catalogXml: string = "<catalog xmlns='urn:serializer:vocabulary'><prices><price currency='USD'>12.5</price></prices><labels><label>A&amp;B</label><label/></labels></catalog>";
const catalog: ShapesCatalog = ShapesCatalog.decodeXml(catalogXml);
assert.deepEqual(catalog.prices[0].value, 12.5); assert.deepEqual(catalog.prices[0].currency, 'USD');
assert.deepEqual(catalog.labels, ['A&B', '']);
assert.deepEqual(catalog.encode(Protocol.BINARY_NONE), ShapesCatalog.decodeXml(catalog.encodeXml()).encode(Protocol.BINARY_NONE));
assert.deepEqual(ShapesCatalog.decodeXml(new ShapesCatalog().encodeXml()).prices, []);
assert.throws(() => ShapesCatalog.decodeXml(catalogXml.replace('<prices>', "<prices extra='1'>")));
const value: ShapesRecord = new ShapesRecord();
value.big = (1n << 64n) - 1n;
value.label = '\tX\r\n\u{1f600}';
value.flag = [true];
value.nested.value = -32768;
const child: ShapesChild = new ShapesChild();
child.value = 32767;
value.children = [child];
value.states = [ShapesState.Zero, ShapesState.One];
value.amount = Infinity;
value.small = 1.25;
const encoded: string = value.encodeXml();
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
assert.deepEqual(ShapesRecord.decodeXml(value.encodeXml()).encode(Protocol.BINARY_NONE), value.encode(Protocol.BINARY_NONE));
let validations: number = 0;
const validator: XmlValidator = { validate(_document: string): void { ++validations; throw new Error('Prepared XSD rejection'); } };
assert.throws(() => ShapesRecord.decodeXml(encoded, new XmlLimits(), validator), /Prepared XSD rejection/);
assert.throws(() => value.encodeXml(new XmlLimits(), validator), /Prepared XSD rejection/);
assert.deepEqual(validations, 2);
console.log('TypeScript generated typed XML and prepared-validator hooks passed');
