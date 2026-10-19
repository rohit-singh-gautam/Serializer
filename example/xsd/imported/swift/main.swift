// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import Foundation

// Exercise generated namespace/attribute/child mapping and the independent binary codec.
let arguments = CommandLine.arguments
guard arguments.count == 3 else { throw SerializerError.invalid("Usage: xml-swift INPUT.xml OUTPUT.xml") }
let bytes = Array(try Data(contentsOf: URL(fileURLWithPath: arguments[1])))
let value = try XsdExampleXsdOrder196281168.decodeXml(bytes)
guard value.xsdId == 7 && value.xsdCustomer == "R&D" && value.xsdCount == 3 && value.xsdTag == ["A", "B"] else {
    throw SerializerError.invalid("XML fixture mapping mismatch")
}
let output = try value.encodeXml()
let copy = try XsdExampleXsdOrder196281168.decodeXml(output)
let legal = "<order xmlns='urn:serializer:xsd-example' id='7'><!-- <!DOCTYPE example> --><?note <!DOCTYPE example>?><customer><![CDATA[literal <!DOCTYPE sample>]]></customer><count>3</count><tag>A</tag><tag>B</tag></order>"
guard try XsdExampleXsdOrder196281168.decodeXml(Array(legal.utf8)).xsdCustomer == "literal <!DOCTYPE sample>" else {
    throw SerializerError.invalid("Legal CDATA/comment/PI rejected")
}
guard try copy.encode(.BINARY_NONE) == value.encode(.BINARY_NONE) else {
    throw SerializerError.invalid("XML/binary round trip mismatch")
}
for invalid in ["<wrong/>", "<order>", "<!DOCTYPE order SYSTEM 'file:///never-read'><order/>"] {
    var rejected = false
    do { _ = try XsdExampleXsdOrder196281168.decodeXml(Array(invalid.utf8)) }
    catch { rejected = true }
    guard rejected else { throw SerializerError.invalid("Malformed XML accepted") }
}
try Data(output).write(to: URL(fileURLWithPath: arguments[2]))
print("Generated Swift XML and binary codecs passed")
