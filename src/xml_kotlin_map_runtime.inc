// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"ktmap(
// Independent per-wrapper map policy; existing XmlLimits constructors and decoder defaults stay compatible.
data class XmlMapLimits(val maxEntries: Int = 1024, val maxKeyBytes: Int = 1048576) {
    // Reject invalid policies before an operation can retain map entries or key text.
    init { require(maxEntries >= 0 && maxKeyBytes >= 0) { "Negative XML map limit" } }
    // Charge the larger lexical/canonical UTF8 key length before retaining a decoded entry.
    internal fun keyBytes(lexical: String, canonical: String, used: Int, operation: SrlXmlBudget.Operation? = null): Int {
        val count = maxOf(srlXmlUtf8Size(lexical), srlXmlUtf8Size(canonical))
        if (!(used >= 0 && used <= maxKeyBytes && count <= maxKeyBytes - used)) SrlXmlBudget.limit("XML map key byte limit exceeded",operation)
        return used + count.toInt()
    }
}
)ktmap"
