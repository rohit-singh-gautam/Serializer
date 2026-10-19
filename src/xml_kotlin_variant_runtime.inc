// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// Recognize XML 1.0 NCName starts independently of host regular-expression dialects.
internal fun srlXmlNameStart(value: Int): Boolean = value == 95 || value in 65..90 || value in 97..122 ||
    value in 0xc0..0xd6 || value in 0xd8..0xf6 || value in 0xf8..0x2ff || value in 0x370..0x37d ||
    value in 0x37f..0x1fff || value in 0x200c..0x200d || value in 0x2070..0x218f ||
    value in 0x2c00..0x2fef || value in 0x3001..0xd7ff || value in 0xf900..0xfdcf ||
    value in 0xfdf0..0xfffd || value in 0x10000..0xeffff
// Validate one nonempty NCName without allocating a code-point array.
internal fun srlXmlNcName(text: String): Boolean {
    if (text.isEmpty()) return false
    var offset = 0
    while (offset < text.length) {
        val code = Character.codePointAt(text, offset)
        if (!srlXmlNameStart(code) && (offset == 0 || !(code == 45 || code == 46 || code == 0xb7 || code in 48..57 || code in 0x300..0x36f || code in 0x203f..0x2040))) return false
        offset += Character.charCount(code)
    }
    return true
}
// Resolve one bounded QName while its provider namespace scope is still active.
internal fun srlXmlTypeQName(raw: String, namespace: (String) -> String?): SrlXmlName {
    val text = raw.trim(' ', '\t', '\r', '\n'); val colon = text.indexOf(':')
    val prefix = if (colon < 0) "" else text.substring(0, colon)
    val local = if (colon < 0) text else text.substring(colon + 1)
    require(srlXmlNcName(local) && (colon < 0 || srlXmlNcName(prefix))) { "Malformed XML type QName" }
    val uri = if (prefix == "xml") "http://www.w3.org/XML/1998/namespace" else namespace(prefix)
    require(prefix.isEmpty() || !uri.isNullOrEmpty()) { "Unbound XML type prefix" }
    return SrlXmlName(local, uri ?: "")
}
)serializer"
