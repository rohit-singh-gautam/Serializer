# python: optional values

The [shared schema](../../schemas/optional/model.serializer) demonstrates
absent and present scalars, records, nested optionals, optional arrays/maps,
and collections of optionals. The consumer decodes the independent JSON fixture
and verifies all four native protocols through the generated model.

```sh
python example/run.py --compiler build/serializer --language python --example optional
```

Use `.exe` for the compiler on Windows. See the [optional contract](../../../docs/schema/optional.md)
for the standard target representation and presence semantics. The payload
version is fixed at 1; future version 2 and malformed optional frames reject.
