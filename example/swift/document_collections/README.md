# Document collections

Run from the repository root:

```text
python example/run.py --compiler <serializer-executable> --language swift --example document_collections
```

Uses independent fixed arrays of nested page records, integers and strings;
checks all four wire formats and a memory-only lookup cache. The shared runner
also rejects shortened and oversized arrays without producing an output file.
