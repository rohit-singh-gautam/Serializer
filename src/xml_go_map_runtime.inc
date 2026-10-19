// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// XmlMapLimits bounds each map wrapper without changing positional XmlLimits/XmlOptions literals.
type XmlMapLimits struct { MaxEntries, MaxKeyBytes int }
// DefaultXmlMapLimits returns an independent map policy derived from the shared XML contract.
func DefaultXmlMapLimits() XmlMapLimits { return XmlMapLimits{srlXmlMapMaxEntries,srlXmlMapMaxKeyBytes} }
// Validate explicit map policies; zero is a real admission bound.
func srlXmlMapPolicy(limits XmlMapLimits)error { if limits.MaxEntries<0||limits.MaxKeyBytes<0{return fmt.Errorf("Negative XML map limit")};return nil }
// Charge the larger lexical/native key representation before inserting the entry.
func srlXmlMapKeyBytes(lexical,canonical string,used *int,limits XmlMapLimits)error {
  count:=len(lexical);if len(canonical)>count{count=len(canonical)}
  if count>limits.MaxKeyBytes-*used{return fmt.Errorf("XML map key byte limit exceeded")};*used+=count;return nil
}
)serializer"
