// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlmixed(
// Bound one ordered mixed sequence without changing ordinary XML/map policies.
public sealed class XmlMixedLimits {
  public int MaxFragments { get; }
  public int MaxTextBytes { get; }
  // Use the shared runtime defaults without mutable process-wide policy.
  public XmlMixedLimits() : this(SrlXmlMixedMaxFragments, SrlXmlMixedMaxTextBytes) { }
  // Zero is exact admission; negative limits are invalid.
  public XmlMixedLimits(int maxFragments, int maxTextBytes) {
    if (maxFragments < 0 || maxTextBytes < 0) throw new System.ArgumentOutOfRangeException(nameof(maxFragments));
    MaxFragments = maxFragments; MaxTextBytes = maxTextBytes;
  }
}
// Keep all admission state local to one native ordered sequence.
internal sealed class SrlXmlMixedBudget {
  private readonly XmlMixedLimits limits;private readonly Limits.IOperation? operation;
  private int fragments, bytes;
  // Borrow immutable policy while owning independent counters.
  internal SrlXmlMixedBudget(XmlMixedLimits limits,Limits.IOperation? operation=null) { this.limits = limits;this.operation=operation; }
  // Admit every child/logical text run before its native fragment allocation.
  internal void Fragment() {
    if (fragments >= limits.MaxFragments) throw SrlXmlBudget.Limit("XML mixed fragment limit exceeded",operation);
    ++fragments;
  }
  // Admit XML Unicode and UTF8 bytes before retaining another provider text chunk.
  internal void Text(string text) {
    if (text is null) throw new System.FormatException("Null XML mixed text");
    try { System.Xml.XmlConvert.VerifyXmlChars(text); }
    catch (System.Xml.XmlException failure) { throw new System.FormatException("Invalid XML character", failure); }
    int count = SrlUtf8.GetByteCount(text);
    if (count > limits.MaxTextBytes - bytes) throw SrlXmlBudget.Limit("XML mixed text limit exceeded",operation);
    bytes += count;
  }
}
)xmlmixed"
