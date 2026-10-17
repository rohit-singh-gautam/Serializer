# Generated output and application runtime licensing

Serializer remains open source: its schema compiler uses GPL-3.0-or-later, and its
application runtime and emitted support have upstream 0BSD permissions. Application
authors may choose proprietary terms for their own schemas and generated files,
subject to applicable input conditions. The examples below concern that application
licensing choice.
Compiler/runtime release **1.7.0** introduces this explicit separation; it does
not change schema syntax, generated APIs, or serialized formats.

| Component | Terms | Application integration |
| --- | --- | --- |
| Application runtime headers under `include/rohit/`, compiled runtime helpers, and managed support | [`LICENSE-RUNTIME`](../../LICENSE-RUNTIME), Zero-Clause BSD (0BSD), except identified compiler API headers | Link `Serializer::runtime`, or `Serializer::managed` for managed support; proprietary distribution is permitted |
| Generated application files | Terms selected by the schema/application author, subject to applicable input conditions | Preserve authored schema notices, including required included-schema notices; proprietary terms are permitted |
| Serializer-authored emitted support and embedded language runtimes | [`LICENSE-GENERATED`](../../LICENSE-GENERATED), upstream 0BSD permission for those contributions | Use under your application's chosen terms; the grant does not label the whole generated file |
| Schema compiler, parser, generators, and compiler API library | [`LICENSE`](../../LICENSE), GPL-3.0-or-later | Run the generator as a build tool; deliberate compiler API linkage uses `Serializer::serializer_lib` |
| Input `.serializer` schemas | Terms chosen by their copyright holders | Their notices and any applicable conditions remain in force |
| Third-party dependencies | Each dependency's own license | Preserve any conditions applicable to the dependencies you distribute |

The compiler API headers are `<rohit/serializer_creator.hpp>`,
`<rohit/output_options.hpp>`, and `<rohit/schema_compatibility.hpp>`. They are not
part of the permissive application runtime grant. Consult the scope in the
licensing files and individual source notices when using other repository source
directly.

## Why the license was separated

Serializer is intended to generate models that applications can use and deploy
under their chosen terms, including proprietary terms. Before the 1.7.0 split,
the runtime headers and support code emitted by generators lacked an explicit
proprietary-use permission separate from the repository's GPL license. That did
not meet the requirement that a closed-source application be able to distribute
both its generated models and the Serializer runtime used by those models.

Using a GPL compiler as a build tool does not by itself require its output to use
GPL. Serializer also supplies code that becomes part of the application: C++
runtime headers and compiled helpers, generated codec bodies, and embedded runtime
support for other languages. The permission for that code therefore needs to be
explicit. The separate runtime target also makes the boundary concrete: a
generated application can link its codec support without bringing in the GPL
parser and generator library.

The compiler remains GPL-3.0-or-later so redistribution of the compiler and its
modifications continues under that license. The application runtime and
Serializer-authored generated support receive 0BSD permissions. This allows
closed-source use, modification, and redistribution without source-disclosure,
royalty, attribution, or license-notice requirements imposed by Serializer. The
generated-output permission states the same intended application use directly,
including when emitted support originates from GPL generator source.

This is a component-specific licensing policy rather than an ownership transfer.
Your schema's existing copyright and license rights remain yours; the license
grants permission to use Serializer's independently authored contributions. A
generated file can consequently contain your schema-derived declarations and
permissively licensed Serializer support while being distributed as part of a
proprietary application. Preserved schema notices record the input authors' terms;
they do not automatically assign every part of a generated file to one owner.

The support's 0BSD terms are the upstream permission for Serializer-authored
contributions. The generator does not apply a 0BSD license label or permission
preamble to the whole generated file. Its output carries the schema authors'
notices and minimal generation provenance. Required notices from included schemas
remain applicable to their contributions; the entry schema's license does not
replace them.

Third-party schemas and dependencies keep their own conditions. These permissions
cover Serializer-authored code and do not certify that every algorithm, provider,
or application is free of third-party patents or other rights. Serializer's
[digest creation helpers](../schema/digest.md) use original 0BSD runtime implementations
without an OpenSSL dependency. Applications choosing another provider must review
that provider's actual license and any redistributed dependencies separately
from Serializer's own grant.

## Copyright and generated output

You retain copyright in the schema you authored. Generation does not assign that
copyright to Serializer. It also does not transfer ownership of Serializer's
independently authored runtime or generator contributions to you: the runtime
license and generated-code permission grant you the rights needed to use them
in your own application.

The schema/application author selects the generated output's license, including
proprietary terms. Generated files preserve the authored copyright and license
notice text, with only the comment framing and syntax escaping required by the
target language. A schema obtained from someone else retains that owner's
applicable terms. The same consideration applies to included schemas,
user-supplied code and templates.

Put copyright and license information in schema comments rather than editing
generated files. For example:

```text
// Copyright (C) 2026 Example Application Authors
// All rights reserved.
// SPDX-License-Identifier: LicenseRef-Example-Proprietary
serializer version 1.2.0;

class record {
  public uint32 sequence;
}
```

`LicenseRef-Example-Proprietary` is an example identifier; supply the corresponding
license text in your application's distribution. A notice records your chosen
terms rather than creating or validating the license itself.

A copyright line identifies the rights holder; it does not by itself specify a
proprietary or open-source license. When the named holder is also the author,
that notice already credits them and no separate `Author:` line is needed. The
tool's two-line generation provenance is separate from copyright and licensing.

Generated output carries the example's copyright and proprietary license notices
with minimal generation provenance. Serializer adds no replacement copyright,
output license identifier or permission preamble. If the input schemas contain no
notices, the output notice header contains only the two provenance lines; the
generator does not infer a license. Serializer's upstream support permission
remains available in `LICENSE-GENERATED` independently of those output notices.

Put notices in leading comments before the first schema token. A contiguous
`//` comment group or an individual `/* ... */` block is retained when it contains
`copyright` (case insensitive), `SPDX-FileCopyrightText:`, or
`SPDX-License-Identifier:`. The complete matching group/block is retained so an
accompanying license statement stays with its copyright notice. Unrelated
comment groups are not propagated.

Notices from the entry schema and included schemas appear in deterministic
first-encounter order; identical notices appear once. All generated language
outputs carry them, including TypeScript declarations: Python uses `#` comments,
and other outputs use `//`. Notice text is retained without substituting another
license or adding a Serializer permission banner. Serializer's compiler GPL notice
is not inherited by the output. Included-schema notices are preserved without
converting them to the entry schema's license or changing unrelated inputs' terms.
No schema language declaration is needed for these comments.

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

See [CMake integration](../guides/cmake_integration.md), [distribution status](distribution.md),
and the [migration guide](../../migration.md) for build and upgrade details.
The [verification record](../verification/verification-proprietary-licensing-2026-10-09.md)
documents the tested runtime separation, notice propagation, and package contents.
