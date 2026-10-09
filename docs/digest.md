# Digest bytes and digest creation

[Back to usage](usage.md) · [Licensing discussion](licensing.md#why-the-license-was-separated)

Compiler/runtime **1.8.0** and schema language **1.3.0** add digest fields in all
eleven native output languages. A field stores application-supplied bytes.
Declaring it does not hash another field or automatically update a hash.

## Hash, digest, and MD5

A hash function maps input bytes to a result; a message digest is the result of
a cryptographic hash function. People also call that result a hash. `digest` is
the general stored-value type in Serializer; MD5 is one algorithm that can create
a digest. MD5 and digest therefore do not mean the same thing. The SHA standards
use the same function/result distinction. See [FIPS 180-4](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf).

## Choose the storage contract

```text
serializer version 1.3.0;

class artifact stable_ids {
  public string name (1);
  public digest custom_hash (2);
  public digest[32] fixed_custom_hash (3);
  public digest(sha256) content_hash (4);
  public array digest(sha256) chunk_hashes (5);
  public map(string) digest[32] other_hashes (6);
}
```

| Form | Meaning | Initial value |
| --- | --- | --- |
| `digest` | Variable-length opaque bytes; the application chooses the algorithm and length | Empty bytes |
| `digest[N]` | Exactly `N` opaque bytes; no algorithm implied | `N` zero bytes |
| `digest(algorithm)` | Exactly the standard output length for the selected algorithm | That many zero bytes |

There is no default SHA-256 algorithm. Use the bare form for a custom hash or
externally supplied digest; use the fixed form when its length is part of your
contract. `N` is a decimal literal from 1 through 65,536, rather than a generic
dimension expression. Algorithm names use the exact lowercase spellings below.
Zero-initialized bytes are storage defaults, rather than the digest of empty
input. Compute and assign the actual value in application code.

Every file declaring these built-in forms, including an included file, requires
language `1.3.0` or newer. An existing declared type named `digest` still resolves
as that user type for a bare reference, including in older schemas. Explicit
`digest(...)` and `digest[N]` select the built-in byte type.

## Supported algorithms

| Schema algorithm / C++ enum member | Digest bytes | Digest bits |
| --- | ---: | ---: |
| `md5` | 16 | 128 |
| `sha1` | 20 | 160 |
| `sha224` | 28 | 224 |
| `sha256` | 32 | 256 |
| `sha384` | 48 | 384 |
| `sha512` | 64 | 512 |
| `sha512_224` | 28 | 224 |
| `sha512_256` | 32 | 256 |
| `sha3_224` | 28 | 224 |
| `sha3_256` | 32 | 256 |
| `sha3_384` | 48 | 384 |
| `sha3_512` | 64 | 512 |

The SHA-1/SHA-2 names and lengths follow
[FIPS 180-4](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf);
the SHA-3 family follows [FIPS 202](https://csrc.nist.gov/pubs/fips/202/final).
`sha512_224` and `sha512_256` have their own initial states; they are not merely
ordinary SHA-512 output cut to those lengths.

There are no standard SHA-128 or SHA-192 algorithms in these families, so
`digest(sha128)` and `digest(sha192)` are rejected. A custom 16- or 24-byte
value can use `digest[16]` or `digest[24]` without claiming one of those algorithm
names. SHAKE128 and SHAKE256 are different extendable-output functions: the
number denotes their security strength rather than a fixed digest length.
Serializer does not yet provide SHAKE computation or algorithm selectors.
See [FIPS 202](https://csrc.nist.gov/pubs/fips/202/final).

BLAKE2 and BLAKE3 are other established families, but this release does not
implement their selectors or computation. An application can store their output
in `digest` or `digest[N]` and use its own provider. Future named support needs
an explicit output-length contract, provider license review, and test vectors;
it is not implied by the generic digest type. See
[BLAKE2 RFC 7693](https://www.rfc-editor.org/rfc/rfc7693.html) and the
[BLAKE3 project](https://github.com/BLAKE3-team/BLAKE3).

For new interoperable cryptographic uses, SHA-256 is a practical starting point
consistent with [NIST's hash policy](https://csrc.nist.gov/projects/hash-functions/nist-policy-on-hash-functions).
MD5 and SHA-1 are provided for legacy interoperability. Do not use them when an
attacker's ability to create collisions matters; see
[MD5 security considerations](https://www.rfc-editor.org/rfc/rfc6151.html) and
[NIST's SHA-1 policy](https://csrc.nist.gov/projects/hash-functions/nist-policy-on-hash-functions).
An unkeyed digest alone does not authenticate who supplied a message, and the
Serializer API is not a password-hashing or signature API.

## Create a digest in C++

The first release includes C++ creation helpers in `<rohit/digest.hpp>`.
Link `Serializer::runtime`, also supplied automatically by `serializer_generate`.
The implementation is Serializer-authored 0BSD runtime code and adds no OpenSSL
or other cryptography-library dependency.

```cpp
#include <iostream>
#include <string_view>
#include <rohit/digest.hpp>
#include "artifact.hpp"

// Compute application bytes and display their digest as hexadecimal text.
int main() {
  artifact value{};
  value.name = "example";
  value.content_hash =
    rohit::make_digest<rohit::digest_algorithm::sha256>(std::string_view{"abc"});
  const auto text = rohit::digest_to_hex(value.content_hash);
  std::cout << text << '\n';
  return 0;
}
```

This prints `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`.

`make_digest<Algorithm>(span<const uint8_t>)` returns
`std::array<uint8_t, rohit::digest_size(Algorithm)>`. Its `std::string_view`
overload hashes every supplied byte, including embedded NULs; it performs no
text-encoding conversion. Choose the input encoding or serialization yourself.
`compute_digest(algorithm, input, output)` supports runtime selection and requires
an exactly sized output span. Invalid algorithms or output sizes throw
`std::invalid_argument` before changing output. Input/output spans may overlap;
the functions retain neither buffer.

`digest_to_hex(bytes)` creates lowercase hexadecimal text.
`digest_from_hex(text, output)` accepts either case and returns `false` without
changing output for malformed or incorrectly sized text. Prefixes, whitespace,
odd lengths, and truncation are rejected; its text and output must not overlap.
These helpers are application conversions; generated JSON still uses byte arrays.

Other languages receive digest storage and codecs, while digest computation uses
the application's chosen provider. Select the same algorithm and input bytes at
both ends. No cryptographic provider is imported by the generated portable code.
Named algorithm metadata checks size, rather than verifying that bytes were
actually produced by that algorithm.

## Generated storage and wire behavior

C++ owning `digest` uses `std::vector<std::uint8_t>`; fixed and algorithm forms
use `std::array<std::uint8_t, N>`. Other outputs use their native byte collections
and enforce the exact length for fixed forms. Digests compose as array elements,
map values, generic type arguments, and supported union alternatives.
Digest map keys, nonempty schema digest initializers, C++ buffer views, and
Protobuf mappings are explicitly unsupported. A bare digest is nontrivial C++
storage, so it also follows the existing raw-union restrictions.

The four native protocols encode a digest like an existing `uint8` sequence:
binary writes the ordinary compact element count followed by raw bytes, and JSON
writes an array of numeric byte values. Fixed digests retain that count and
reject the wrong cardinality. There is no extra algorithm tag, hexadecimal
string, or byte-order conversion. Normal sequence and decode limits still apply.
For example, a 32-byte digest starts with
count byte `20` in hexadecimal, followed by its 32 digest bytes. The surrounding
object's keys and revision use their normal representation. See
[wire format](wire_format.md).

Do not change an established field's algorithm or fixed-length semantics merely
because its byte width happens to match. Both peers must agree on the meaning of
the bytes; use the [compatibility checker](schema_evolution.md) and the
[migration notes](../migration.md#digest-fields-and-creation-9-october-2026).

## Proprietary use and algorithm rights

The C++ implementation and header are covered by
[`LICENSE-RUNTIME`](../LICENSE-RUNTIME); generated support is covered by
[`LICENSE-GENERATED`](../LICENSE-GENERATED). These permissions allow proprietary
use and redistribution. Your schema retains its copyright and license notices,
and independent providers retain their own terms. See the
[licensing rationale and scope](licensing.md).

Algorithm publication is different from a particular implementation's license.
[RFC 1321](https://www.rfc-editor.org/rfc/rfc1321.html) describes the MD5 algorithm
as public domain, while its RSA reference-code appendix carries its own
permission notice. Serializer's implementation is original runtime code; it
does not adopt that appendix's code license or OpenSSL's license. Nor does 0BSD
certify the absence of every third-party patent: FIPS 180-4 section 10 expressly
allows that implementations may be covered by patents. The project does not
claim FIPS validation or a universal patent clearance.
