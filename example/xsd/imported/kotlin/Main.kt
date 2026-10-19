// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.io.File

// Exercise generated XML codecs and retain binary_none for the independent internal channel.
fun main(arguments: Array<String>) {
    require(arguments.size == 2) { "Usage: xml-kotlin INPUT.xml OUTPUT.xml" }
    val value = XsdExampleXsdOrder196281168.decodeXml(File(arguments[0]).readBytes())
    check(value.xsdId == 7u && value.xsdCustomer == "R&D" && value.xsdCount == 3)
    check(value.xsdTag == mutableListOf("A", "B"))
    val output = value.encodeXml()
    val copy = XsdExampleXsdOrder196281168.decodeXml(output)
    val legal = "<order xmlns='urn:serializer:xsd-example' id='7'><!-- <!DOCTYPE example> --><?note <!DOCTYPE example>?><customer><![CDATA[literal <!DOCTYPE sample>]]></customer><count>3</count><tag>A</tag><tag>B</tag></order>"
    check(XsdExampleXsdOrder196281168.decodeXml(legal.toByteArray(Charsets.UTF_8)).xsdCustomer == "literal <!DOCTYPE sample>")
    check(copy.encode(Protocol.BINARY_NONE).contentEquals(value.encode(Protocol.BINARY_NONE)))
    for (invalid in listOf("<wrong/>", "<order>", "<!DOCTYPE order SYSTEM 'file:///never-read'><order/>")) {
        check(runCatching { XsdExampleXsdOrder196281168.decodeXml(invalid.toByteArray(Charsets.UTF_8)) }.isFailure)
    }
    File(arguments[1]).writeBytes(output)
    println("Generated Kotlin XML and binary codecs passed")
}
