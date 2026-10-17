// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.io.File

// Exercise generated XML codecs and retain binary_none for the independent internal channel.
fun main(arguments: Array<String>) {
    require(arguments.size == 2) { "Usage: xml-kotlin INPUT.xml OUTPUT.xml" }
    val value = XmlExampleOrder.decodeXml(File(arguments[0]).readBytes())
    check(value.id == 7u && value.customer == "R&D" && value.count == 3)
    check(value.tags == mutableListOf("A", "B"))
    val output = value.encodeXml()
    val copy = XmlExampleOrder.decodeXml(output)
    val legal = "<order xmlns='urn:serializer:xml-example' id='7'><!-- <!DOCTYPE example> --><?note <!DOCTYPE example>?><customer><![CDATA[literal <!DOCTYPE sample>]]></customer><count>3</count><tag>A</tag><tag>B</tag></order>"
    check(XmlExampleOrder.decodeXml(legal.toByteArray(Charsets.UTF_8)).customer == "literal <!DOCTYPE sample>")
    check(copy.encode(Protocol.BINARY_NONE).contentEquals(value.encode(Protocol.BINARY_NONE)))
    for (invalid in listOf("<wrong/>", "<order>", "<!DOCTYPE order SYSTEM 'file:///never-read'><order/>")) {
        check(runCatching { XmlExampleOrder.decodeXml(invalid.toByteArray(Charsets.UTF_8)) }.isFailure)
    }
    File(arguments[1]).writeBytes(output)
    println("Generated Kotlin XML and binary codecs passed")
}
