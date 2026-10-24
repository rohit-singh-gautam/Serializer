# Embedded Java runtime literal bounds - 24 October 2026

Compiler/runtime **2.32.1** repairs the source-token bound for the generated Java
runtime. Schema language **1.23.0**, public APIs, wire formats and editor packages
**2.0.17** remain unchanged.

The full Windows run at `dfa8b46` completes with **314/315** checks passing in
**1,645.70 seconds**. The one failure is `serializer_runtime_literals`: the first
Java runtime token contains **16,449 bytes**, exceeding the existing **16,380-byte**
bound for older Visual Studio hosts. MSVC 19.51 builds this source; that success
does not replace the explicit older-host portability check.

Splitting that token at a declaration boundary yields **8,055** and **8,394** bytes.
The other Java tokens remain 3,900, 2,566, 1,719, 5,646 and 3,245 bytes. The
concatenated normalized runtime bytes retain SHA-256
`18957577ced65bc7b290b3e869df11b9e2063a89894d61fc47f0ce8ae21a7ca7`.

The complete embedded-runtime literal inventory passes after the split. Rebuilt
MSVC 19.51 and GCC 15.2/libstdc++ 15 profiles pass **626/626** and **623/623** core
tests respectively, plus **25/25** constructor cases on each host. Both CLIs
report 2.32.1 / language 1.23.0 and both generated CMake package-version files
pass same-major compatibility and future/different-major rejection controls.

Each host passes **99,111** actual Java assertions (Windows JDK 17 and Linux
JDK 21), including the previous-compiler wire oracle. **26 generated files per
host** match 2.32.0 byte for byte: all eleven outputs and five additional Java
option/profile controls. The repair changes C++ token boundaries only.

This record does not yet claim a passing full-suite replay. The initial failed
run remains recorded above; a later full replay must identify its actual source
snapshot. Actual older-MSVC execution is separate from the token-bound inventory.

Baseline compilers, token inventories, comparison controls and verification logs
remain under ignored `out/build/java-literal-bound-overnight/`. The original
failed full run remains under `out/build/java-buffer-overnight/windows-full.log`.
