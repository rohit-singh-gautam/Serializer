# Replacing pinned file generations

Compiler/runtime **2.34.0** adds
`rohit::replace_file_generation(source, target)` in `<rohit/file_stream.hpp>`.
It atomically makes the target name select a complete replacement while existing
readers and predecessor writers retain their original native file object.
An already open replacement writer continues to own the replacement object.
The existing `publish_file` API keeps its original behavior.

Prepare and sync the replacement before calling this function. Both paths must
belong to a filesystem supporting atomic replacement of pinned generations.
After replacement, sync the affected parent directories before acknowledging
durability. An interrupted/failed acknowledgement requires the caller's recovery
protocol; the helper does not decide which application revision is accepted.
There is no copy fallback or forced invalidation of readers.

POSIX uses the existing same-filesystem rename path. Windows uses the extended
native rename operation with replacement/POSIX semantics and a write-through
handle. See Microsoft's [rename flag contract](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/4217551b-d2c0-42cb-9dc1-69a716cf6d0c)
and [write-through metadata behavior](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-createfile2_extended_parameters).
Older Windows targets or unsupported volumes fail explicitly.

`file_generation_test` passes **3/3** Windows MSVC 19.51 C++23 Release cases:
old readers/open staging writers, predecessor-writer transfer, and 32 different
Unicode filename lengths. The filename-length regression verifies explicit
termination independently of the native record's byte count. These cases are
also part of the passing 22-command managed storage checkpoint. Linux replay
and physical device power-loss behavior require their own qualification.
