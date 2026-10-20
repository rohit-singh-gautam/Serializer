// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// Bound ordered fragment storage independently of ordinary document and map policies.
data class XmlMixedLimits(val maxFragments: Int = 100000, val maxTextBytes: Int = 8388608) {
    init { require(maxFragments >= 0 && maxTextBytes >= 0) { "Negative XML mixed limit" } }
}
// Keep counters local to one ordered sequence and validate text before retaining it.
internal class SrlXmlMixedBudget(private val limits: XmlMixedLimits, private val operation: SrlXmlBudget.Operation? = null) {
    private var fragments = 0
    private var bytes = 0L
    // Every authored fragment counts, including empty or adjacent text fragments.
    fun fragment() { if (!(fragments < limits.maxFragments)) SrlXmlBudget.limit("XML mixed fragment limit exceeded",operation); ++fragments }
    // Preserve Unicode scalar identity and check cumulative UTF8 size without overflowing.
    fun text(value: String) {
        val size = srlXmlUtf8Size(value)
        if (!(size <= limits.maxTextBytes - bytes)) SrlXmlBudget.limit("XML mixed text limit exceeded",operation); bytes += size
    }
}
)serializer"
