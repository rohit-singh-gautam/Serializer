// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD

// Exercise private durable properties exclusively through generated codec APIs.
fun main() {
    val input = "<record id='7' enabled='true'><label>R&amp;D</label><samples>-32768</samples><samples>32767</samples></record>"
    val value = KotlinPrivateRecord.decodeXml(input.toByteArray(Charsets.UTF_8))
    val bytes = value.encodeXml()
    val output = String(bytes, Charsets.UTF_8)
    check(output.contains("id=\"7\"") && output.contains("enabled=\"true\""))
    check(output.contains("<label>R&amp;D</label>"))
    check(output.contains("<samples>-32768</samples><samples>32767</samples>"))
    val copy = KotlinPrivateRecord.decodeXml(bytes)
    check(copy.encode(Protocol.BINARY_NONE).contentEquals(value.encode(Protocol.BINARY_NONE)))
    val absent = KotlinPrivateRecord.decodeXml("<record id='0'><label/></record>".toByteArray(Charsets.UTF_8))
    check(!String(absent.encodeXml(), Charsets.UTF_8).contains("enabled="))
    check(runCatching { KotlinPrivateRecord.decodeXml(input.replace("id='7'", "id='invalid'").toByteArray(Charsets.UTF_8)) }.isFailure)
    println("Kotlin private durable XML fields passed")
}
