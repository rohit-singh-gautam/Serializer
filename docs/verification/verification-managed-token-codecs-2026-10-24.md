# Python managed token codec qualification - 24 October 2026

Compiler/runtime 2.34.0 adds an explicit Python wire-record mode under MR03.
The [supported contract](../managed/token_codecs.md) is distinct from native
managed engines. It adds no schema syntax or language-version requirement beyond
the existing language 1.20 typed tokens.

## Implemented test scope

`test/managed_token_codecs_test.py` generates actual Python output in four ID
profiles: uint32/uint64 and fixed/compact_varint. Its independent fixture builder
checks JSON, positional binary, integer-key binary and string-key binary. Cases
include full-width document/object IDs, 127/128 compact boundaries, optional/array/
map wrappers, self tokens, invalid scope/type/zero IDs, duplicate/missing metadata,
all truncations, trailing input, resource limits and CLI/INI selection. Default
rejection and failed-output preservation remain controls.

`test/managed_token_exchange.cpp` independently decodes/re-encodes these token
messages through generated C++ models. The Python runner accepts one `--exchange`
argument per native profile and compares each direction against independent bytes.
`managed_lowering_test.cpp` covers explicit selection, source AST preservation and
unsupported contracts, including nested required-token omissions.

## Executed status

At initial implementation handoff, the qualification script passes Python syntax
validation and the changed-file whitespace check passes. Full generated Python,
native C++ exchanges and core tests await the integrated rebuild; these checks
must be recorded as executed before treating this implementation slice as qualified.

The broader MR03 requirement remains open for other generated outputs and named
generic shared/mutual selection. This mode explicitly rejects generic declarations,
behavior/bindings, indirect managed-occurrence synthesis, separated values,
XML/packages, caches, indexes, readonly exposure and computed class helpers.
It provides envelope validation rather than live entity resolution, identity
allocation or any native managed engine. No cross-platform qualification is
claimed by this initial record.
