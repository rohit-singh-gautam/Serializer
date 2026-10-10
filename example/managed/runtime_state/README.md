# Runtime state and upgrades

Build `managed_runtime_state_example` through the root CMake build and run it.
The executable checks immutable read pins, transient updates, ordinary copies,
explicit reset clones, durable invalidation, notifications, linear navigation,
verified Save-envelope patches, checkpoint chains, opt-in delta journal replay,
resident admission limits and scoped allocation resource lifetime. It also converts every
retained state from a previous document convention and recovers a previous
journal into a distinct destination artifact.

See [the schema](document.serializer), [the consumer](main.cpp), and
[state examples](../../../docs/state_enhancements.md) for the contracts and limits.
