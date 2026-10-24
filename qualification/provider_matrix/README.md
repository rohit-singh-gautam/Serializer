# Native optional-provider matrix

`../provider_matrix.py` builds the actual runtime as a source dependency and
executes a C++23 consumer for every combination of Zstandard, LZ4, zlib and XML.
The sixteen profiles retain exact configure/build/run commands, failures,
compiler diagnostics, source and consumer hashes, and per-profile semantic counts.
Profiles execute sequentially; each build uses one worker by default.

```text
python -B qualification/provider_matrix.py --build out/provider-matrix --dependency-prefix /path/to/native/providers --jobs 1
```

Use an MSVC developer shell on Windows. Linux may need a separate dependency
prefix for extracted Zstandard/LZ4 packages; repeat `--dependency-prefix` as needed.
`--source` selects an independently archived source tree, and `--cxx` selects the
actual native compiler. `--profiles` is diagnostic: only the exact complete set
of sixteen distinct profiles can produce a full `passed` report.

The consumer checks actual compiled availability, standard-frame round trips,
exact input/output and one-tight bounds, disabled-provider error categories,
and namespace-aware XML values or the disabled-provider diagnostic. Enabled
formats must produce identical bytes across every combination on the same host.
Identity remains a byte-for-byte non-OPC control. Caller custom adapters are
per-operation application dependencies, outside this built-in provider inventory.

This gate does not claim arbitrary third-party providers, another ABI/CPU,
cross-host compressed-byte equality with differing provider versions, or hard
provider-internal memory limits. Retained initial failures and current results
belong in the dated release qualification record.
