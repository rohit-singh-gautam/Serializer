# Generated Go XML codec

The shared runner generates `internal/model/model_gen.go` into a staged native
Go module, then builds and runs `main.go INPUT.xml OUTPUT.xml`. The generated
codec uses Go's standard `encoding/xml`; no cgo or third-party parser is required.

```text
order.xml -> DecodeXmlExampleOrderXML
                    |
               owning value
                    |
                EncodeXML -> output.xml
```

The example checks attribute/ordered-child mapping, repeated tags, binary
equivalence, complete-object round trips and malformed/range/resource failures.
Use `XmlOptions` for explicit limits and an optional caller-prepared `XmlValidator`
when runtime XSD support was enabled during generation. The default path uses
generated mapping checks. See [shared commands](../README.md).
