# Portable fixed-array verification — 8 October 2026

Compiler/runtime 1.6.0 extends explicit fixed owning arrays without initializers
to JavaScript/TypeScript and Python. This supersedes the corresponding unsupported
status in [the earlier dimension qualification](verification-dimensions-2026-10-04.md).
Schema language 1.2.0 and the ordinary sequence wire count remain unchanged.

The actual compiler generated native C++, JavaScript, TypeScript declarations,
and Python from `test/resources/fixed_arrays_portable.serializer`. A native C++
executable emitted twelve fixtures: all four protocols with each of absent,
20-byte, and 32-byte union alternatives. JavaScript and Python independently
constructed the values, matched the C++ bytes, and decoded/re-encoded every fixture.

`serializer_fixed_arrays_portable` passed on Windows x64 with Visual Studio 2026,
Node.js, and Python 3.14. Its checks cover 20/32-byte arrays, nested class defaults,
independent instances/elements, union wrappers, missing keyed defaults, short/extra
writer values, short/extra binary and JSON input, every truncation, and element
budgets. A wrong binary count is rejected before element reads; JSON rejects an
extra malformed element because the extent is already exhausted.

Thirty targeted core tests covering dimensions, generics, inferred arrays, and
portable backend validation passed. The existing portable CLI, runtime literal,
generic Python, compact Python, and package-version checks also passed.
`serializer --version` reports compiler
1.6.0 and supported schema language 1.2.0. Package discovery retains compatibility
with earlier versions in the same major release.

TypeScript declarations retain `T[]`; runtime cardinality is enforced by the JS
codec. Explicit initialized or inferred arrays, other portable backends, and
direct Protobuf fixed-array generation remain unsupported. No compiler/runtime
performance improvement, additional platform qualification, or TypeScript tuple
length guarantee is inferred from these codec checks.
