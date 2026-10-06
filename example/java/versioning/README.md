# java versioning example

Generate and run this example with `python example/run.py --compiler <serializer> --example versioning --language java`.
The [shared schema](../../schemas/versioning/model.serializer) declares uint8/16/32/64, float/double and two/three/four-component versions, custom names, visibility, IDs, compatibility, lifecycle annotations, replacement metadata and reservations.

The program reads revision 8 with the compatible read policy, round-trips its historical positional binary layout, explicitly copies `old_name` into `name`, sets revision 10, and verifies all four native protocols with strict reads. Replacement conversion is application code; retained definitions describe historical bytes.

Every consumer also constructs all eight explicit revision types, checks independent positional byte fixtures, and round-trips each type through all four protocols. The float uses `0.1` to verify declared binary32 precision; uint64 uses its maximum value.

The shared schema also declares release dates and nested `any`/`all` policies with age, count, and compatibility leaves. They resolve to minimum version 8 entirely during generation. Add `--version-policy-as-of 2026-10-06` to pin the example; generated consumers contain no dates, catalogs, clocks, or policy trees.
