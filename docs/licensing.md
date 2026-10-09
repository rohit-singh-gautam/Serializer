# Generated output and application runtime licensing

Serializer permits proprietary applications to use and distribute its generated
models and application runtime. The schema compiler remains GPL-3.0-or-later.
Compiler/runtime release **1.7.0** introduces this explicit separation; it does
not change schema syntax, generated APIs, or serialized formats.

| Component | Terms | Application integration |
| --- | --- | --- |
| Application runtime headers under `include/rohit/`, compiled runtime helpers, and managed support | [`LICENSE-RUNTIME`](../LICENSE-RUNTIME), Zero-Clause BSD (0BSD), except identified compiler API headers | Link `Serializer::runtime`, or `Serializer::managed` for managed support; proprietary distribution is permitted |
| Serializer-authored generated models and embedded language runtime support | [`LICENSE-GENERATED`](../LICENSE-GENERATED), explicit permission to distribute under any license | Choose your application's license, including proprietary terms |
| Schema compiler, parser, generators, and compiler API library | [`LICENSE`](../LICENSE), GPL-3.0-or-later | Run the generator as a build tool; deliberate compiler API linkage uses `Serializer::serializer_lib` |
| Input `.serializer` schemas | Terms chosen by their copyright holders | Their notices and any applicable conditions remain in force |
| Third-party dependencies | Each dependency's own license | Preserve any conditions applicable to the dependencies you distribute |

The compiler API headers are `<rohit/serializer_creator.hpp>`,
`<rohit/output_options.hpp>`, and `<rohit/schema_compatibility.hpp>`. They are not
part of the permissive application runtime grant. Consult the scope in the
licensing files and individual source notices when using other repository source
directly.

## Copyright and generated output

You retain copyright in the schema you authored. Generation does not assign that
copyright to Serializer. It also does not transfer ownership of Serializer's
independently authored runtime or generator contributions to you: the runtime
license and generated-code permission grant you the rights needed to use them
in your own application.

Consequently, an application built from your proprietary schema may keep
proprietary output and remain closed source. A schema obtained from someone else
retains that owner's applicable terms; generating code is not a way to remove
those terms. The same consideration applies to user-supplied code or templates.

Put copyright and license information in schema comments rather than editing
generated files. For example:

```text
// Copyright (c) 2026 Example Application Authors
// SPDX-License-Identifier: LicenseRef-Example-Proprietary
serializer version 1.2.0;

class record {
  public uint32 sequence;
}
```

`LicenseRef-Example-Proprietary` is an example identifier; supply the corresponding
license text in your application's distribution. A notice records your chosen
terms rather than creating or validating the license itself.

Put notices in leading comments before the first schema token. A contiguous
`//` comment group or an individual `/* ... */` block is retained when it contains
`copyright` (case insensitive), `SPDX-FileCopyrightText:`, or
`SPDX-License-Identifier:`. The complete matching group/block is retained so an
accompanying license statement stays with its copyright notice. Unrelated
comment groups are not propagated.

Notices from the entry schema and included schemas appear in deterministic
first-encounter order; identical notices appear once. All generated language
outputs carry them, including TypeScript declarations: Python uses `#` comments,
and other outputs use `//`. They appear under `Input schema notices:`, alongside
the Serializer support permission banner. Serializer's compiler GPL notice is not
inherited by the output. A notice from an included schema does not change the license of
unrelated inputs or override Serializer's generated-code grant. No schema
language declaration is needed for these comments.

Notices are rendered as valid comments in the selected language. Embedded CR,
NEL, U+2028, and U+2029 line separators become separately commented lines. Java
represents source backslashes as `\u005c` to prevent Unicode preprocessing from
turning notice text into source code. Other `//` outputs escape backslashes at a
line ending, including those followed by trailing whitespace, to prevent line
continuation; Python comments preserve backslashes. These syntax escapes preserve
the notice's meaning while keeping its text inside comments.

The CLI preserves file-level notices even for a schema with no declarations.
Compiler API callers should pass the complete `parser::parsed_schema` returned
by `parser::parse_file` to `writer::cpp::generate_schema`,
`writer::java::generate_schema`, or `writer::portable::generate_schema`.
Existing statement-vector `generate` overloads
preserve notices attached to declarations, but an empty vector has no file-level
metadata. The compiler API itself retains its GPL license.

## C++ targets and packages

For generated models or hand-written use of the application codec APIs:

```cmake
find_package(Serializer CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE Serializer::runtime)
```

`serializer_generate` and `serializer_generate_variants` supply this linkage
automatically. Use `Serializer::managed` for managed state support; it links the
application runtime. These targets do not link the GPL parser/generator library
into the application. Keep generated headers and linked runtime binaries from a
compatible release and rebuild consumers after runtime changes.

`Serializer::serializer_lib` remains the GPL compiler API library for existing
parser, generator, and schema-compatibility callers. Running
`Serializer::serializer` to produce files at build time is distinct from linking
the compiler library into your application. Distribution of the compiler itself
continues to require its GPL terms.

Source and installed packages include `LICENSE`, `LICENSE-RUNTIME`, and
`LICENSE-GENERATED`. Verify the resolved package revision and license files:
older published packages may predate the split. Third-party compression libraries
and other dependencies retain their own terms. These permissions do not promise
that every possible application, schema, or dependency is free of third-party
patent rights.

See [CMake integration](cmake_integration.md), [distribution status](distribution.md),
and the [migration guide](../migration.md) for build and upgrade details.
The [verification record](verification-proprietary-licensing-2026-10-09.md)
documents the tested runtime separation, notice propagation, and package contents.
