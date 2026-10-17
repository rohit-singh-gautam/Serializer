# Generated C# XML

Generate `Schema.cs` from the [shared schema](../model.serializer) with
[XML configuration](../codegen.ini), then compile it with `Program.cs` in an
ignored build directory. Use .NET 8+ and enable nullable reference checking.

```text
dotnet run --project Example.csproj -- INPUT.xml OUTPUT.xml
```

The example uses generated `XmlExampleOrder.DecodeXml()` / `EncodeXml()` and
checks the same model through `binary_none`, malformed input, DTD rejection,
and byte limits. Optional prepared runtime `XmlSchema` validation uses
`System.Xml.Schema` and must be enabled in generation configuration.

See the [shared example](../README.md) for the contract and build workflow.
