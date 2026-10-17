// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {readFileSync, writeFileSync} from 'node:fs';
import assert from 'node:assert/strict';
import {XmlExampleOrder, Protocol, XmlLimits} from './schema.mjs';

// Compile against generated XML declarations and use the same generated JavaScript implementation.
const input: string = readFileSync(process.argv[2], 'utf8');
const value: XmlExampleOrder = XmlExampleOrder.decodeXml(input);
assert.deepEqual([value.id, value.customer, value.count, value.tags], [7, 'R&D', 3, ['A', 'B']]);
const canonical: Uint8Array = value.encode(Protocol.BINARY_NONE);
const output: string = value.encodeXml();
assert.deepEqual(XmlExampleOrder.decodeXml(output).encode(Protocol.BINARY_NONE), canonical);
assert.throws(() => XmlExampleOrder.decodeXml('<wrong/>'));
assert.throws(() => XmlExampleOrder.decodeXml('<!DOCTYPE order><order/>'));
assert.throws(() => XmlExampleOrder.decodeXml(input, new XmlLimits({maxBytes: 1})));
assert.throws(() => XmlExampleOrder.decodeXml(input.replace('id="7"', 'id="4294967296"')));
const body: string = input.replace(/^\uFEFF?<\?xml[^?]*\?>/, '');
assert.throws(() => XmlExampleOrder.decodeXml('<?xml version="1.0" encoding="UTF-16"?>' + body));
assert.throws(() => XmlExampleOrder.decodeXml('<?xml version="1.1"?>' + body));
writeFileSync(process.argv[3], output);
console.log('Generated TypeScript XML codec passed');
