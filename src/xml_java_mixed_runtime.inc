// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlmixed(
/** Bound one ordered mixed sequence independently of ordinary XML/map limits. */
public static final class XmlMixedLimits {
  public final int maxFragments, maxTextBytes;
  /** Use the shared compiler/runtime defaults without mutable global policies. */
  public XmlMixedLimits() { this(SRL_XML_MIXED_MAX_FRAGMENTS, SRL_XML_MIXED_MAX_TEXT_BYTES); }
  /** Zero is an exact admission bound; negative limits are invalid. */
  public XmlMixedLimits(int maxFragments, int maxTextBytes) {
    if (maxFragments < 0 || maxTextBytes < 0) throw new IllegalArgumentException("Negative XML mixed limit");
    this.maxFragments = maxFragments; this.maxTextBytes = maxTextBytes;
  }
}
/** Keep fragment and UTF8 text counters private to one ordered native sequence. */
private static final class SrlXmlMixedBudget {
  private final XmlMixedLimits limits;
  private int fragments, bytes;
  /** Borrow one immutable operation policy without sharing mutable counters. */
  SrlXmlMixedBudget(XmlMixedLimits limits) { this.limits = limits; }
  /** Charge every child or logical text run before allocating its native fragment. */
  void fragment() {
    if (fragments >= limits.maxFragments) throw SrlXmlRule.quota("XML mixed fragment limit exceeded");
    ++fragments;
  }
  /** Admit XML Unicode and cumulative UTF8 bytes before retaining another text chunk. */
  void text(String text) {
    if (text == null) throw new IllegalArgumentException("Null XML mixed text");
    for (int offset = 0; offset < text.length();) {
      int value = text.codePointAt(offset); offset += Character.charCount(value);
      if (!(value == 9 || value == 10 || value == 13 || value >= 32 && value <= 0xd7ff || value >= 0xe000 && value <= 0xfffd || value >= 0x10000 && value <= 0x10ffff)) throw new IllegalArgumentException("Invalid XML character");
      int width = value < 0x80 ? 1 : value < 0x800 ? 2 : value < 0x10000 ? 3 : 4;
      if (width > limits.maxTextBytes - bytes) throw SrlXmlRule.quota("XML mixed text limit exceeded");
      bytes += width;
    }
  }
}
)xmlmixed"
