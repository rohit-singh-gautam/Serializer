# Generated TypeScript XML codec

The shared runner generates `schema.mjs` plus `schema.d.mts`, installs locked
dependencies, compiles `main.mts` in strict NodeNext mode, and runs `main.mjs INPUT
OUTPUT`. TypeScript uses the same generated implementation as JavaScript.

The XML parser is MIT-licensed [`@xmldom/xmldom`](https://github.com/xmldom/xmldom).
The example checks typed mapping, binary equivalence, DTD refusal, range errors and
byte limits. See the [shared commands](../README.md).
