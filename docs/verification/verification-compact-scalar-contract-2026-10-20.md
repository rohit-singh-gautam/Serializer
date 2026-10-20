# Compact scalar design qualification, 20 October 2026

[Selected payload contract](../design/compact_scalar_contract.md) ?
[Implemented unsigned scalars](../schema/compact_integers.md) ?
[Verification index](README.md)

This record qualifies **SC01's design prerequisite**, including all five proposed
numeric encodings. It does not enable syntax or claim generated codec support.
Compiler/runtime **2.14.0**, schema language **1.20.0**, existing unsigned compact
annotations and ordinary native binary/JSON/Protobuf behavior remain unchanged.
The next implementation requires a schema-language feature release and actual
backend/editor/wire qualification described by the design.

## Executed reference checks

The standard-library-only [reference runner](../../qualification/compact_scalar_contract.py)
uses exact rational arithmetic and actual Python **3.14.3**. Its complete run
exits zero with these results:

| Check | Result |
| --- | --- |
| Fixed golden vectors | **76** signed, binary floating and scaled vectors |
| Binary16 patterns | **65,536/65,536**, including zero signs, subnormals, infinities and every NaN encoding |
| Bfloat16 patterns | **65,536/65,536**, including canonical re-encoding of every NaN |
| Independent finite/special conversions | **10,725** checks against Python's standard IEEE binary16/binary32 packing |
| Independent finite overflow | **9,275** oracle rejection checks |
| Fixed-seed source sample | **10,000** binary64 patterns, two target conversions each |
| Signed endpoints | **36** vectors across int8/int16/int32/int64, exact shortest varints and reverse transforms |
| Malformed integer payloads | **24** truncated, nonminimal, trailing, oversized/overflowing controls reject |
| Proposed compiler keywords | **5/5** reject before replacing an existing output sentinel |

Exact boundary vectors distinguish direct binary64-to-binary16/bfloat16 rounding
from incorrect binary32 intermediates. The binary16 65519/65520 boundary checks
finite rounding versus overflow. Other vectors cover even/odd halfway rounding,
signed underflow, canonical positive quiet NaN, exact scaled ties, negative values,
range/nonfinite/invalid-scale rejection and scaled canonical zero. An exact-decimal
scale 0.1 applied to actual binary64 0.15 produces q=1, demonstrating the specified
native-bit input rather than decimal-looking input interpretation.

The exhaustive 16-bit sweeps are payload-reference round trips; binary16 finite
and infinity patterns are also checked through independent standard-library
unpacking and packing. The independent sample uses fixed seed `0x5343414c4152`.
No throughput, generated-source-size, hard-allocation or cross-language runtime
claim is made. The payload definitions and vectors form a future implementation
oracle; actual backend qualification remains required before enabling a format.

Scaled reconstruction is qualified for **both binary32 and binary64** directly
from exact rationals. A review regression pairs maximum finite binary32 with
scale2^128: q1 reconstructs finite binary64 but must reject binary32 overflow.
Another exact scale lies just above the binary32 halfway point while rounding to
that halfway point in binary64; the direct path produces bits0x3f800001 rather
than the incorrect intermediate result0x3f800000. Signed nonzero underflow and
canonical exact zero are checked separately. The original review probe remains
in `current-regression/scalar-contract-review.json`; the maintained reference now
covers this gap without rounding rational reconstruction through binary64.

## Compiler boundary and provenance

The actual immutable compiler SHA-256 is
`3533d50e7b9ac33bae04e969080c63ff80402bca69332381d1044f129262804e`.
For each proposed modifier, the runner writes a schema selecting current language
1.20.0 and a preexisting output sentinel, invokes the actual C++ compiler, requires
a nonzero result and verifies the sentinel bytes are unchanged. The immutable
compiler hash is checked again afterward. No parser keyword, editor grammar,
wire discriminator or schema version was added by this design task.

The runner SHA-256 is
`edf15e9c0b7450592fc768e8e8eea280dce4304e5bba8d07e1ac8afde16830c3`.
Its final report SHA-256 is
`673de238aacfd9c50bfaa15d0638529a0b382d49e80263b93307cbb6331bda61`.
The report retains all 76 exact golden payload vectors, provider-free metadata,
Python version, runner/compiler hashes and rejected keyword names.

```powershell
python -B qualification/compact_scalar_contract.py `
  --compiler out/build/backlog-2026-10-20/compiler-current-combined-3533d50e/serializer.exe `
  --build out/build/backlog-2026-10-20/compact-scalar-contract
```

Retained evidence is `compact-scalar-contract.log` and
`compact-scalar-contract/verification.json` plus five compiler diagnostic logs
under ignored `out/build/backlog-2026-10-20/`. Existing output sentinels are
intentional qualification fixtures. Primary numeric definitions are linked in
the design; Serializer's overflow, canonicalization and endian choices are
explicit design decisions rather than inferred provider defaults.
