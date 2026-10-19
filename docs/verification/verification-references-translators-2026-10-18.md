# Field reference and translator verification — 18–19 October 2026

Compiler/runtime **2.5.0**, schema language **1.18.0**. These checks qualify the
[implemented field reference and scoped translator contract](../schema/references.md).

## Results

| Check | Result |
| --- | --- |
| Reference SDK consumers | All eleven backends passed. Ten non-C SDK runs plus the final C run are tied to the final all-eleven regeneration by byte-identical generated-source SHA-256 checks. |
| Translator SDK consumers | All eleven backends passed the final expanded inherited/generic fixture against the final compiler. |
| Reference/translator composition | C++ passed shared reference → synchronous field borrow → target projection → direct encoding in JSON and all three native binary protocols. |
| Runtime lifetime checks | Clang C++20 AddressSanitizer passed reentrant shared-owner release, rebinding, copy/move assignment and borrowed moved-from detachment. No weak allocation pins retained the freed storage. |
| Editor unit tests | **144/144** passed, including references to explicitly typed and implicitly typed named version fields. |
| Visual Studio grammar | **452** installed-asset syntax/boundary checks passed with the TextMate tokenizer. This is not an editor-host UI test. |
| Visual Studio navigation | **1,229** source/output checks passed using the actual .NET shared resolver across all eleven languages, including named revision references. |
| Generated navigation | **42,256** bidirectional checks passed across all eleven outputs against the final compiler. |
| Editor packages | Both **2.0.3** VSIX packages built and validated through `./make.ps1 extension`; neither was installed or published. |
| Version output | CLI reports compiler **2.5.0** and language **1.18.0**. |
| Windows native regression | **137/137** CTest entries passed, including CLI, package-version and installed-package checks. The final C-only identifier fix has separate SDK qualification below. |
| Maintained examples | All eleven SDK consumers passed. The new C++ example also built through CMake and passed its focused CTest entry; native Go generate/build/test passed. |

Reference consumers cover live inherited field slots, shared retention,
borrowed/weak expiry or explicit scope closure, null addresses, all four native
codecs, identity mismatch, malformed paths and metadata edits. Selected backends
also exercise same-address decode detachment, missing intermediate records,
wrong native owner types, helper-name collisions and preserved Go naming.
Boolean, character, version3 and enum reference APIs compile across the SDKs;
Java executes each corresponding callback shape.

Translator consumers compare direct projected output with an independent owning
conversion in all four native protocols. The fixture includes target magic and
revision metadata, omissions, a wire-only derived size, transient defaults,
borrowed nested records, public inherited defaults and a concrete generic target.
Generic positional bytes also have an independent fixed-size/value check.
C++ additionally checks private projection storage and base inheritance. C
reference consumers also check typed scope rejection, reentrant unbind/free/rebind
and public/internal decode callbacks, plus preservation of an existing binding
when public decode rejects malformed input.

The [durable address examples](../../example/address_references/README.md) cover `owner.b`,
`consumer.d`, three ownership modes, explicit address resolution and nested
reference/translator callbacks. The new C++ CTest entry was checked separately
after the 137-entry regression run. The examples exposed a C typedef collision
for valid owner types named `owner` and `value`; generated reference helpers now
use reserved local/parameter names. That C-only output change is qualified by
fresh C SDK checks and unchanged non-C generated-source comparisons rather than
another complete native CTest run.

Final C qualification passed the maintained example, typed-reference matrix,
translator matrix and valid `scope`/`value` owner-type compilation probes.
Fresh all-eleven generation for examples, references and translators matched
every SDK-qualified source: ten unchanged non-C outputs plus each newly qualified
C header. These are separate SDK runs and byte-equality proofs, not a claim that
all SDKs were rebuilt after a C-only identifier change.

## Reproduce

Use Visual Studio x64 Developer PowerShell for the Windows native build:

```powershell
./make.ps1 test -BuildDirectory out/build/ReleaseWindows -Configuration RelWithDebInfo -Jobs 4 -CMakeArgs '-DVCPKG_MANIFEST_INSTALL=OFF'

python -B test/reference_languages.py --compiler out/build/ReleaseWindows/serializer.exe --build out/reference-languages --language all
python -B test/translator_languages.py --compiler out/build/ReleaseWindows/serializer.exe --build out/translator-qualification --language all
./make.ps1 extension
```

The cached native configuration enables managed support and compression, with
XML disabled and `SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=OFF`; the two maintained
SDK runners above execute language qualification separately. Disabling vcpkg
manifest installation reuses this workstation's already installed dependencies.
A fresh dependency installation should use the normal wrapper defaults.

The wrapper built all native targets. Final generator/runtime corrections are
refreshed through the CMake `serializer`, `serializer_lib` and `core_serializer_test`
targets, followed by a complete CTest rerun, avoiding another build of unchanged
managed examples on the shared workstation:

```powershell
cmake --build out/build/ReleaseWindows --config RelWithDebInfo --target serializer serializer_lib core_serializer_test --parallel 4
ctest --test-dir out/build/ReleaseWindows -C RelWithDebInfo --output-on-failure --parallel 1
```

The core executable ran 579 cases with one optional formatter test skipped because
`SERIALIZER_TEST_CLANG_FORMAT` was not set. This skip is separate from the passing
generated-code build and editor checks.

SDKs used: MSVC **19.51.36257**, Python **3.14.3**, Node.js **22.23.1**,
TypeScript **7.0.2**, Java **26** (`--release 17`), Go **1.26.4**,
.NET SDK **10.0.402** (`net8.0` consumer), Rust **1.99.0**, Swift **6.4** and
Kotlin/JVM **2.3.10** with JDK **17.0.18**. Put the selected SDKs and cached
TypeScript compiler on `PATH`, configure the Windows Swift SDK/runtime paths,
and provide a working Kotlin compiler launcher. This workstation used the
installed Kotlin compiler through its direct JDK launcher because its packaged
preloader failed. Selected missing SDKs fail rather than silently skipping.

## Scope and retained evidence

References serialize durable typed addresses and require explicit application
owner lookup/resolution after loading. They do not automatically persist an
arbitrary object graph or define distributed ownership. Collection-element
identity, managed storage and reference-to-reference paths are unsupported.
Reference and translator schemas reject optional XML generation in every backend.
The guide records C++ profile restrictions, native lifetime rules and translator
construction/default limitations. No performance benchmark or complete Linux
CTest run is claimed.

Useful build outputs, SDK modules/caches, package files and verification logs
remain under ignored `out/`; native Go generation also retains its specifically
ignored model outputs in `example/address_references/go/`. Temporary diagnostic source probes and obsolete
launchers are removed after their checks complete. Final evidence includes:

- `out/reference-native-refresh.log` and `out/reference-native-qualified.log`.
- `out/reference-languages/verification-ten-final.log`, `verification-c-final.log`,
  `verification-final.log` and `qualified-sources.json`.
- `out/translator-final-qualification.log` and `out/reference-runtime-asan.log`.
- `out/reference-editor-tests.log`, `out/reference-visual-studio-navigation.log`,
  `out/reference-generated-navigation-final.log` and `out/reference-extension-packaging.log`.
- Both `out/extensions/serializer-*-2.0.3.vsix` packages.
- `out/reference-example-native.log`, `out/reference-c-generator-qualified.log`
  and final logs/manifests under `out/reference-examples/`.
- `out/reference-generation-final.log` records all three final source-equality proofs.
