// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlmodel(
// Bound each owning digest independently of document, list and mixed-content policies.
data class XmlDigestLimits(val maxBytes: Int = 1048576) {
    init { require(maxBytes >= 0) { "Negative XML digest limit" } }
}
// Identify model metadata independently of presentation namespace prefixes.
internal const val srlXmlModelNamespace = "urn:serializer:xml:model"
// Validate the canonical native revision grammar before range conversion or layout selection.
internal fun srlXmlRevisionLexical(raw: String, kind: String): String {
    if (kind.startsWith("version")) {
        try { srlVersionParts(raw,kind.last()-'0') }
        catch (failure: SerializerException) { throw IllegalArgumentException("Invalid XML dotted revision",failure) }
        return raw
    }
    val text=raw.trim(' ','\t','\r','\n')
    require(Regex("(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?").matches(text)) { "Noncanonical XML revision" }
    if (kind.startsWith("uint")) {
        require(text.all { it in '0'..'9' }) { "Noncanonical XML integer revision" }
    } else {
        val value=if (kind == "float") srlXmlFloat32(text).toDouble() else srlXmlFloat(text)
        val mantissa=text.substringBefore('e').substringBefore('E')
        require(value.isFinite() && (value != 0.0 || mantissa.none { it in '1'..'9' })) { "XML revision overflow or underflow" }
    }
    return text
}
// Convert one strict ASCII hexadecimal digit without accepting whitespace or Unicode aliases.
internal fun srlXmlHexDigit(value: Char): Int = when(value) {
    in '0'..'9' -> value-'0'
    in 'a'..'f' -> value-'a'+10
    in 'A'..'F' -> value-'A'+10
    else -> -1
}
// Validate all hex pairs and extent before allocating the private owning byte candidate.
internal fun srlXmlDigestRead(text: String, extent: Int, limits: XmlDigestLimits): UByteArray {
    val size=text.length/2
    require(text.length%2 == 0 && (extent == 0 || size == extent)) { "XML digest extent or hexadecimal pairs" }
    require(size <= limits.maxBytes) { "XML digest byte limit" }
    require(text.all { srlXmlHexDigit(it) >= 0 }) { "Invalid XML digest hexadecimal" }
    return UByteArray(size) { index -> (srlXmlHexDigit(text[index*2])*16+srlXmlHexDigit(text[index*2+1])).toUByte() }
}
// Export exact borrowed native bytes as lowercase hex without computing or changing any digest.
internal fun srlXmlDigestText(value: UByteArray, extent: Int, limits: XmlDigestLimits): String {
    require(extent == 0 || value.size == extent) { "XML digest extent" }
    require(value.size <= limits.maxBytes && value.size <= Int.MAX_VALUE/2) { "XML digest byte limit" }
    val digits="0123456789abcdef"; val output=StringBuilder(value.size*2)
    for (item in value) { val byte=item.toInt(); output.append(digits[byte ushr 4]); output.append(digits[byte and 15]) }
    return output.toString()
}
)xmlmodel"
