// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {readFileSync, writeFileSync} from 'node:fs';
import {XmlExampleOrder, Protocol} from './schema.mjs';

// Keep the same source valid as strict TypeScript against generated declarations.
const input = new Uint8Array(readFileSync(process.argv[2]));
const mode = process.argv[3];
const repetitions = Number(process.argv[5]);
// BENCHMARK_SETUP
const canonical = value.encode(Protocol.BINARY_NONE);
let retained;
const samples = [];
// Warm three batches; the result of every call escapes into the retained slot.
for (let sample = -3; sample < 5; ++sample) {
  const start = process.hrtime.bigint();
  for (let iteration = 0; iteration < repetitions; ++iteration) {
    switch (mode) {
      // BENCHMARK_XML_OPERATIONS
      case 'native-encode': retained = value.encode(Protocol.BINARY_NONE); break;
      case 'native-decode': retained = XmlExampleOrder.decode(canonical, Protocol.BINARY_NONE); break;
      default: throw new Error('Unknown benchmark operation');
    }
  }
  if (sample >= 0) samples.push(Number(process.hrtime.bigint() - start));
}
let result;
if (retained instanceof XmlExampleOrder) result = retained;
else if (typeof retained === 'string' || retained instanceof Uint8Array) {
  // BENCHMARK_RESULT
} else throw new Error('No measured result');
const actual = result.encode(Protocol.BINARY_NONE);
if (actual.length !== canonical.length || actual.some((value, index) => value !== canonical[index])) {
  throw new Error('Measured result changed native values');
}
writeFileSync(process.argv[4], canonical);
// BENCHMARK_XML_OUTPUT
console.log(JSON.stringify({wall_ns: samples, repetitions, sdk: process.version,
  allocation_method: 'No exact JavaScript allocation counter; isolated OS process high-water residency only'}));
