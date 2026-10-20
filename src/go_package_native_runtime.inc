// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagego(
// srlPackageNativeXML narrows existing per-document XML defaults before invoking generated native codecs.
// Native node/suballocation counters remain independent; complete automatic cumulative accounting is separate.
func srlPackageNativeXML(budget *SerializerPackageBudget) XmlOptions {
  options:=DefaultXmlOptions();limits:=&options.Limits
  narrow:=func(current int,maximum int64)int{if maximum<int64(current){return int(maximum)};return current}
  limits.MaxBytes=narrow(limits.MaxBytes,budget.limits.MaxPartBytes)
  limits.MaxTextBytes=narrow(limits.MaxTextBytes,budget.limits.MaxPartBytes)
  limits.MaxNodes=narrow(limits.MaxNodes,budget.Remaining(SerializerPackageWorkUnits))
  limits.MaxAttributes=narrow(limits.MaxAttributes,budget.Remaining(SerializerPackageWorkUnits))
  return options
}
)packagego"
