# Combined managed editor profiles: 24 October 2026

Both Rohit Serializer extensions are **2.0.21**, supporting compiler/runtime
2.34.0 and schema language 1.24.0. This combines the compiler/backend and
XML/package profiles with local managed scalar indexes.

The single-worker Node 22 shared grammar/navigation replay passes **170 tests**,
with zero failures, skips or cancellations. It includes index keywords, qualified
enum types, declaration/selection boundaries, transitive includes, unsaved
contents and ordinary types named `index`. Existing ordered XML, package metadata,
bit-field and ordinary navigation controls remain present after merge.

The actual Visual Studio Jint navigation adapter passes **110 index checks**
with its existing interpreter memory limit. This is adapter execution, not
interactive Visual Studio/VS Code host qualification or Marketplace publication.

`./make.ps1 extension` rebuilds both packages, and the Visual Studio package
validator passes. Packages are neither installed nor published:

- `serializer-vscode-2.0.21.vsix`: SHA-256
  `4e6feb9971d84971ef3b6b1c1224af384477bdfd50c63a9e6ceb148f7300911d`.
- `serializer-visual-studio-2.0.21.vsix`: SHA-256
  `fa26e6b21cdaeeb68bbde36ee1e298be2e753a8efaf2a9e67413c17466a67e8b`.

Retained output: `out/extensions` and `out/build/compiler-merge-editors`.
The first PowerShell capture stopped on npm's nonfatal install-script warning;
capturing native stdout/stderr at the process boundary allows packaging to finish.
The packaging wrapper then waited on an idle C# compiler server; the completed
packages were preserved and the task helper released before the final passing
unit/adapter run. No repository build-wrapper defaults or dependencies changed.
