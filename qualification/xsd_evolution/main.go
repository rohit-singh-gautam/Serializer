// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
  "fmt"
  "os"
  "path/filepath"
  m "serializer-xsd-evolution/model"
)

// require stops invalid inputs before opening any output artifact.
func require(condition bool, message string) {
  if !condition { panic(message) }
}

// checkAuthored checks native fields and portable behavior independently of encoded bytes.
func checkAuthored(value *m.EvolutionOrder) {
  require(value.Id == 7 && value.Customer == "R&D", "authored scalar mismatch")
  require(value.Note != nil && *value.Note == "memo", "authored optional mismatch")
  require(value.HasId(7) && !value.HasId(8), "authored behavior mismatch")
}

// nativeBytes calls the existing native encoder and preserves any actual codec failure.
func nativeBytes(value *m.EvolutionOrder, protocol m.Protocol) []byte {
  bytes, err := value.Encode(protocol)
  if err != nil { panic(err) }
  return bytes
}

// writeFile publishes only complete validated bytes supplied by an ordinary public codec.
func writeFile(path string, bytes []byte) {
  if err := os.WriteFile(path, bytes, 0600); err != nil { panic(err) }
}

// main qualifies schema defaults, unchanged native identity and version-specific XML order/names.
func main() {
  require(len(os.Args) >= 4 && os.Args[1] == "@variant@", "VARIANT emit OUTDIR | check-xml/check-native INPUT OUTPUT")
  defaults := m.NewEvolutionOrder()
  require(defaults.Id == 7 && defaults.Customer == "native default" && defaults.Note == nil, "schema defaults mismatch")
  require(defaults.HasId(7) && !defaults.HasId(8), "default behavior mismatch")
  if os.Args[2] == "emit" {
    require(len(os.Args) == 4, "emit requires output directory")
    note := "memo"
    value := &m.EvolutionOrder{Id: 7, Customer: "R&D", Note: &note}
    checkAuthored(value)
    xml, err := value.EncodeXML()
    if err != nil { panic(err) }
    projection, err := m.DecodeEvolutionOrderXML(xml)
    if err != nil { panic(err) }
    checkAuthored(projection)
    artifacts := []struct{name string; bytes []byte}{
      {"authored.xml", xml},
      {"authored.bin", nativeBytes(value, m.BINARY_NONE)},
      {"authored-integer.bin", nativeBytes(value, m.BINARY_INTEGER)},
      {"authored-string.bin", nativeBytes(value, m.BINARY_STRING)},
      {"default.bin", nativeBytes(defaults, m.BINARY_NONE)},
      {"xml-projection.bin", nativeBytes(projection, m.BINARY_NONE)},
    }
    for _, artifact := range artifacts {
      writeFile(filepath.Join(os.Args[3], artifact.name), artifact.bytes)
    }
  } else {
    require(len(os.Args) == 5, "check operation requires input and output")
    input, err := os.ReadFile(os.Args[3])
    if err != nil { panic(err) }
    var value *m.EvolutionOrder
    if os.Args[2] == "check-xml" {
      value, err = m.DecodeEvolutionOrderXML(input)
    } else {
      require(os.Args[2] == "check-native", "unknown operation")
      value, err = m.DecodeEvolutionOrder(input, m.BINARY_NONE)
    }
    if err != nil { panic(err) }
    checkAuthored(value)
    output := nativeBytes(value, m.BINARY_NONE)
    if os.Args[2] == "check-native" {
      output, err = value.EncodeXML()
      if err != nil { panic(err) }
    }
    writeFile(os.Args[4], output)
  }
  fmt.Println("Go authored XSD evolution passed")
}
