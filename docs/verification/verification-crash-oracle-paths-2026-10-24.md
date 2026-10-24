# Crash-oracle paths in deep Windows build trees — 24 October 2026

[Verification index](README.md) · [Qualification requirements](../pending.md#qualification-and-distribution)

The current compiler/runtime 2.33.0 native Windows suite initially passed
303/310 tests. Two failures were the independent Python crash oracle reaching
Windows' legacy path limit while inspecting nested sidecar journals beneath the
deep qualification build directory. The native runtime had created the journal;
Python inspection and subsequent temporary-directory cleanup failed.

`test/managed_reference_crash_test.py` now places its private crash fixtures in
the operating system's temporary directory. Requested output directories still
retain `commands.json` and `verification.json`. The crash helper, native managed
runtime, storage contracts and wire bytes are unchanged.

The targeted replay on MSVC 19.51.36260.0, Windows x64 Release C++23 passed
`managed_reference_crash_fixed_separate32_test` and
`managed_reference_crash_fixed_separate64_test`: **474 semantic checks,
174 scenarios and 942 fresh helper processes**, including both appended and
sidecar storage. CTest elapsed time was 44.38 seconds. Each profile retained its
independent CRC64/footer/chain inspection and abrupt-process fault points.

The passing oracle source SHA-256 is
`821b5566670463627dad4fb4c5d6632812a77794b1bf7aec0cd6494561a829b2`.
The two helper image hashes are
`d7a9a1434c0992147150a7acee0ea20642b4d2bbf06b1c450ede3a8d9d90073d`
and `0abe518888db22db97a185a047b578aab9a2a22bbf06f2e76a83fe394418cd5a`.
Raw evidence remains under ignored `out/qualification-2026-10-24/`:
`windows-authorized-test.log`, `windows-crash-oracle-replay.log`, and the two
`windows-msvc/test/managed_reference_crash_fixed_separate*/verification.json`
reports. Other initial suite failures have separate deployment/prefix replays;
this targeted pass alone does not close QA01.

README and the integration skill were reviewed. Public usage and production
behavior did not change, so their instructions and release versions are unchanged.
This record does not claim power-loss durability or qualify Windows-mounted WSL
appended-journal discard behavior.

## Native Linux executable/report separation

The full native Linux CTest run exposes a separate launcher-path collision:
all eight crash helpers have no executable suffix, while their registered report
directories use the same path. Python fails `mkdir` with `FileExistsError` before
launching the helper. Windows' `.exe` suffix avoids that collision.

The registered CMake report directories now end in `_oracle`, leaving helper
target names, runtime code and protocol behavior unchanged. All eight unchanged
frozen GCC helpers/oracles replay successfully with distinct native report paths:
**1,896 semantic checks, 696 scenarios and 3,768 fresh processes**, including
appended and sidecar modes. This is an executed failure/replay, not an inferred
storage success from the initial launcher failure.

`linux-crash-replays.json` records every actual command, exit code, elapsed time
and log hash beneath ignored `out/qualification-2026-10-24/`; its SHA-256 is
`291ce227828c423181cf1401d1145c34329a43e7613e29e64d2a84e708de82c8`.
The original serial-suite failure remains in `linux-build-test-link-path.log`.
Per-profile reports and useful native builds are retained on the exclusively
owned native temporary filesystem. The final patched CMake replay uses the
new report-directory registration.
