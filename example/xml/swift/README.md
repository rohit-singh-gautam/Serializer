# Generated Swift XML example

Generate the shared model with `--language swift --config example/xml/codegen.ini`
and output `Schema.swift` in an ignored build directory. Compile that file and
`main.swift` together with `swiftc`. Foundation and FoundationXML are required.

Run `xml-swift example/xml/order.xml OUTPUT.xml`. Checks cover the generated XML
mapping, round trip, independent binary codec, malformed roots and DTD rejection.
