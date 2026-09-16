# Configuration schema

`config.serializer` demonstrates namespaces, owning records, and inheritance.
From the repository root:

```sh
serializer --input example/config/config.serializer --output config.hpp
```

The default C++ profile requires clang-format at generation time. See the
[usage guide](../../docs/usage.md) for compiling and invoking generated codecs.
