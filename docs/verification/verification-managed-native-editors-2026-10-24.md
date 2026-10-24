# Managed native SDK editor verification, 24 October 2026

Both Rohit Serializer extensions are **2.0.22**, supporting compiler/runtime
2.34.0 and schema language 1.24.0. Their usage and Marketplace payloads now
distinguish C++ managed behavior from the explicit Python, Java and
JavaScript/TypeScript native profiles and their separate capability limits.

## Executed checks

- **170 shared editor tests pass**, with zero failures, skips or cancellations,
  using Node 22.23.1 and one test worker. Existing grammar, command, navigation,
  unsaved-content and selection-boundary controls remain passing.
- Fresh generated JavaScript ESM and TypeScript declaration SDKs pass **1,158
  bidirectional type/field navigation checks**. Profiles use uint32 compact IDs
  with all codecs and uint64 fixed IDs with BINARY_NONE.
- The actual Visual Studio Jint adapter passes **560 managed native navigation
  checks** against those generated modules and **110 existing index checks**.
  Its existing 256 MiB interpreter limit is unchanged.
- `./make.ps1 extension` builds both 2.0.22 packages. The Visual Studio package
  validator passes; both archive manifests independently report 2.0.22.

Reproduce generated navigation with
`node editors/vscode/test/managed_native_navigation.cjs`, setting
`SERIALIZER_COMPILER` to the built compiler. Build the existing
`editors/visual_studio/test/navigation_test.csproj`, then run its adapter with
the repository path, `out/extension-tests/managed-native` and
`--managed-native-only`. This uses generated outputs from the Node check.

## Retained evidence and limits

Logs remain in `out/build/managed-native-editors`; generated navigation inputs
remain in `out/extension-tests/managed-native`. Package SHA-256 values:

- `serializer-vscode-2.0.22.vsix`:
  `0d2f68e88fa44123dddf7c704d66e846a7b0281b612b1df8b51c764d1150a0a0e`.
- `serializer-visual-studio-2.0.22.vsix`:
  `c2201d6d267eab58a6e7f9c14310365bd43b800e041c1fecec415a039e7f2389`.

The ignored orchestration helper initially returned after packaging because
PowerShell had not captured the child exit code. Packaging output and fresh
artifacts were retained; the helper was corrected and the subsequent unit,
adapter and hash checks completed successfully. No repository wrapper or
dependency policy changed.

These are shared-provider and actual native-adapter checks. Interactive editor
host qualification remains separate. Packages were neither installed nor
published, and no Marketplace update is claimed.
