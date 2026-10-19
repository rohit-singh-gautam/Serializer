// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {readFileSync, writeFileSync} from 'node:fs';
import assert from 'node:assert/strict';
import {XsdExampleOrder, Protocol, XmlLimits} from './schema.mjs';

// Exercise generated XML methods and preserve the complete model through the binary codec.
const input = readFileSync(process.argv[2], 'utf8');
const value = XsdExampleOrder.decodeXml(input);
assert.deepEqual([value.id, value.customer, value.count, value.tag], [7, 'R&D', 3, ['A', 'B']]);
const canonical = value.encode(Protocol.BINARY_NONE);
const output = value.encodeXml();
assert.deepEqual(XsdExampleOrder.decodeXml(output).encode(Protocol.BINARY_NONE), canonical);
assert.throws(() => XsdExampleOrder.decodeXml('<wrong/>'));
assert.throws(() => XsdExampleOrder.decodeXml('<!DOCTYPE order><order/>'));
assert.throws(() => XsdExampleOrder.decodeXml(input, new XmlLimits({maxBytes: 1})));
assert.throws(() => XsdExampleOrder.decodeXml(input.replace('id="7"', 'id="4294967296"')));
const body = input.replace(/^\uFEFF?<\?xml[^?]*\?>/, '');
assert.throws(() => XsdExampleOrder.decodeXml('<?xml version="1.0" encoding="UTF-16"?>' + body));
assert.throws(() => XsdExampleOrder.decodeXml('<?xml version="1.1"?>' + body));
writeFileSync(process.argv[3], output);
console.log('Generated JavaScript XML codec passed');
