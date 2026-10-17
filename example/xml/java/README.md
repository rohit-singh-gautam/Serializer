# Generated Java XML

Generate `Schema.java` from the [shared schema](../model.serializer) with
[XML configuration](../codegen.ini), then place it beside `Main.java` in an
ignored build directory. Requires Java 17+.

```text
javac --release 17 -Xlint:all -Werror Schema.java Main.java
java Main INPUT.xml OUTPUT.xml
```

The example uses generated `Schema.XmlExample.Order.decodeXml()` / `encodeXml()`
and checks `binary_none` side channels, malformed input, DTD rejection, and
byte limits. Prepared runtime `Schema.XmlSchema` validation uses Java's JAXP
provider and must be enabled in generation configuration.

See the [shared example](../README.md) for the contract and build workflow.
