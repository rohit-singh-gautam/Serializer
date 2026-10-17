# Generated Rust XML codec

The shared runner generates `schema.rs`, copies this Cargo project into the
ignored build directory, builds with the locked
[`roxmltree` parser](https://github.com/RazrFalcon/roxmltree), and runs
`serializer-xml-example INPUT OUTPUT`.

```text
XML -> generated decoder
          |
       owning model
          |
     generated encoder -> XML
```

`roxmltree` is dual MIT/Apache-2.0 licensed. Parsing disables DTDs and external
resolution; generated code checks names, order, occurrences and scalar ranges.
Encoding writes bounded escaped text directly. This is generated XML support,
without a JSON intermediate. See the [shared commands](../README.md).
