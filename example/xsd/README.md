# XSD import and hand-authored binding across eleven languages

Compiler/runtime **2.7.0**, schema language **1.20.0**. These examples share one
[local XSD](order.xsd) and [XML fixture](order.xml). Both routes generate typed
XML codecs for C++, Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift,
Kotlin and C. There is no JSON intermediary or application-written XML codec.

The automatic route imports the XSD into an ordinary `.serializer` model before
generating the requested languages. The hand-authored route binds the
[application model](manual/model.serializer) using one `using schema` declaration.
Its INI selects XML and disables the optional C++ formatter; it repeats no field
names, namespace URI, attribute placement or element ordering.

```text
order.xsd -> compiler XSD import -> generated model.serializer -> language codecs
    |
    +------ hand-authored model.serializer + using schema -------^
```

Import is the default starting point when the external XSD already defines the
model. The manual route is optional: use it to choose application API names,
native field identities or supported application behavior. Neither route requires
maintaining another copy of the XSD's field-placement rules.

The explicit clients under [imported](imported/) and [manual](manual/) call their
actual generated APIs. Imported declarations use deterministic `xsd_` names and
namespace identities to disambiguate source symbols; the manual model chooses
`xsd_example::order`. Both encode the same external XML. Their separate native
binary layouts are not claimed to be interchangeable.

## C++ with CMake

The [standalone CMake project](CMakeLists.txt) imports/binds the XSD, generates
C++ headers, compiles both clients and runs their checks. It requires CMake 4.0+,
a C++20 compiler and an XML-enabled Serializer 2.7 package or source checkout.
Python and the other language SDKs are unnecessary for this workflow.

From this checkout in Windows Developer PowerShell, select your libxml2 prefix:

```powershell
$libxml2Prefix = (Resolve-Path out/build/make-Release/vcpkg_installed/x64-windows).Path
$env:PATH = "$libxml2Prefix/bin;$env:PATH"
cmake -S example/xsd -B out/build/xsd-cmake -DSERIALIZER_SOURCE_DIRECTORY="$PWD" -DCMAKE_PREFIX_PATH="$libxml2Prefix"
cmake --build out/build/xsd-cmake --config Release --parallel 2
ctest --test-dir out/build/xsd-cmake -C Release --output-on-failure
```

Change `libxml2Prefix` to the development installation in use. Its shared DLLs
must be available on `PATH` when running Windows clients. On Linux, use the same
CMake/build/CTest commands with the source checkout's absolute path and system
libxml2 or the appropriate `CMAKE_PREFIX_PATH`.

To use an installed XML-enabled Serializer package, omit
`SERIALIZER_SOURCE_DIRECTORY` and provide its installation prefix:

```sh
cmake -S example/xsd -B out/build/xsd-installed -DCMAKE_PREFIX_PATH="/path/to/serializer;/path/to/libxml2"
cmake --build out/build/xsd-installed --config Release --parallel 2
ctest --test-dir out/build/xsd-installed -C Release --output-on-failure
```

`XSD_WORKFLOW=both` is the default. Select `-DXSD_WORKFLOW=imported` or
`-DXSD_WORKFLOW=manual` for one target. Both targets link `Serializer::runtime`;
the manual target uses `serializer_generate`, while the imported target runs the
compiler's import and code-generation commands directly. Source builds normally
build their compiler as a dependency; `SERIALIZER_EXAMPLE_COMPILER` optionally
selects an existing matching host compiler.

CMake owns `generated/imported/model.serializer` under its binary directory.
Rebuilding after XSD changes may replace that exact generated file. Keep manual
edits in [manual/model.serializer](manual/model.serializer); the command-line
import protection still applies to application-authored destinations. Explicit
dependencies and compiler depfiles track XSD/model/configuration changes.

## Cross-language qualification

Build an XML-enabled Serializer compiler/runtime first. The compiler requires
libxml2, and the C/C++ clients require its development headers/libraries. Other
backend prerequisites and pinned XML parser dependencies are the same as the
[all-language XML examples](../xml/README.md). Run Windows C/C++ builds from a
Visual Studio developer shell; Kotlin can use an explicitly selected WSL SDK.
Missing selected SDKs fail the run. `SERIALIZER_KOTLINC` can select an installed
native Kotlin compiler when it is not on `PATH`, using the shared runner's SDK
override convention.

```powershell
python example/xsd/run.py `
  --compiler out/build/make-Release/Release/serializer.exe `
  --build out/build/xsd-examples `
  --libxml2-prefix out/build/make-Release/vcpkg_installed/x64-windows
```

The default runs both routes and all eleven SDKs. `--workflow imported` or
`--workflow manual` selects one route. `--language java,python,go` explicitly
narrows the SDKs; `--wsl-languages kotlin` selects a WSL Kotlin installation.
Use `--cpp-library` when the matching runtime library is not beside the compiler.
Use `--cmake-generator Ninja` when desired. On Linux, use the native compiler and
runtime paths and discover libxml2 through the system or `--libxml2-prefix`.

The underlying commands for the automatic route are:

```sh
serializer --import-xsd example/xsd/order.xsd --namespace xsd_example --root order --output out/build/xsd-import/model.serializer --depfile out/build/xsd-import/model.serializer.d
serializer --input out/build/xsd-import/model.serializer --config example/xsd/codegen.ini --language cpp --output out/build/xsd-import/model.hpp
```

Create the destination directory first. The import command protects differing
existing `.serializer` files from overwrite: import a changed contract into a
fresh path, review it and migrate retained native field identities deliberately.
Generation produces code; compiling and running the client produces XML.

The runner builds the clients with their actual SDKs, checks generated round
trips and malformed/resource-limited input, exchanges every selected language
pair within each workflow, and checks same-language exchanges in both directions
between workflows. Python ElementTree independently checks expanded names,
attribute values, child ordering and repeated values. A `qualification.json`
records the compiler version, requested scope and completed checks under the
ignored build directory, including failure status.

The fixture uses required primitive children, an unqualified numeric attribute
and an optional repeated string element. It does not demonstrate arbitrary XSD
coverage, package profiles or 3MF conformance. See the
[fixed-XSD tooling contract](../../docs/serialization/xsd_tooling.md) for the
implemented subset and explicit rejection of unsupported constructs. Generated
default checks do not imply that an external general XSD validator ran.

Verified on Windows with Serializer **2.7.0**, schema language **1.20.0** and
libxml2 **2.15.3**: both workflows compiled and ran all eleven native SDK clients
(22 builds), passed 121 producer/consumer exchanges per workflow and 22 exchanges
between workflows (264 exchanges). Kotlin used an explicitly selected installed
native compiler. The retained run report is
`out/build/xsd-examples/qualification.json`; the complete log is
`out/build/opc-doc-review/xsd-examples.log`. This run used these checked schemas
and clients, including the automatic import command above.

The standalone CMake source integration also built both C++ targets and passed
all four CTest round-trip/cross-workflow checks, using the matching prebuilt host
compiler override and a separately built runtime. The Windows run placed the
libxml2 DLL directory on `PATH`. The installed-package route is provided for
existing installations and was not separately executed in this run.
