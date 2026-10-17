// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.util.Optional

// Qualify typed XML values and both prepared runtime-XSD directions.
fun main() {
    val catalogXml = "<catalog xmlns='urn:serializer:vocabulary'><prices><price currency='USD'>12.5</price></prices><labels><label>A&amp;B</label><label/></labels></catalog>"
    val catalog = ShapesCatalog.decodeXml(catalogXml.toByteArray(Charsets.UTF_8))
    check(catalog.prices.size == 1 && catalog.prices[0].value == 12.5 && catalog.prices[0].currency == "USD" && catalog.labels == mutableListOf("A&B", ""))
    check(catalog.encode(Protocol.BINARY_NONE).contentEquals(ShapesCatalog.decodeXml(catalog.encodeXml()).encode(Protocol.BINARY_NONE)))
    check(ShapesCatalog.decodeXml(ShapesCatalog().encodeXml()).prices.isEmpty())
    check(runCatching { ShapesCatalog.decodeXml(catalogXml.replace("<prices>", "<prices extra='1'>").toByteArray(Charsets.UTF_8)) }.isFailure)
    val value = ShapesRecord()
    value.big = ULong.MAX_VALUE
    value.label = "\tX\r\n\ud83d\ude00"
    value.flag = Optional.of(true)
    value.nested.value = Short.MIN_VALUE
    val child = ShapesChild()
    child.value = Short.MAX_VALUE
    value.children.add(child)
    value.states = mutableListOf(ShapesState.Zero, ShapesState.One)
    value.amount = Double.POSITIVE_INFINITY
    value.small = 1.25f
    val encoded = value.encodeXml()
    val copy = ShapesRecord.decodeXml(encoded)
    check(copy.encode(Protocol.BINARY_NONE).contentEquals(value.encode(Protocol.BINARY_NONE)))
    value.flag = Optional.empty()
    check(!ShapesRecord.decodeXml(value.encodeXml()).flag.isPresent)
    val zero = "<record big='-000'><label>x</label><nested><value>1</value></nested><states>zero</states><states>one</states><amount>INF</amount><small>1.25</small></record>"
    check(ShapesRecord.decodeXml(zero.toByteArray(Charsets.UTF_8)).big == 0uL)
    val xsd = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='record' type='xs:string'/></xs:schema>"
    val schema = PreparedXmlSchema(xsd.toByteArray(Charsets.UTF_8))
    check(runCatching { ShapesRecord.decodeXml(encoded, validator = schema) }.isFailure)
    check(runCatching { value.encodeXml(validator = schema) }.isFailure)
    println("Kotlin generated typed XML and prepared XSD checks passed")
}
