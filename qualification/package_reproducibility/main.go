// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
  "bufio"
  "encoding/hex"
  "encoding/json"
  "fmt"
  "os"
  "path/filepath"
  "runtime"
  "strconv"
  "strings"
  m "serializer-package-reproducibility/model"
)

const fixtureLimitBytes = 16 * 1024 * 1024
const permutationCount = 6

// require stops any invalid producer before publishing archive bytes or a success fingerprint.
func require(condition bool, message string) {
  if !condition { panic(message) }
}

// rows reads exact headerless UTF-8/LF fields without normalizing URI or Unicode spelling.
func rows(directory, kind, name string, columns int) [][]string {
  file, err := os.Open(filepath.Join(directory, kind, name))
  if err != nil { panic(err) }
  defer file.Close()
  info, err := file.Stat()
  if err != nil { panic(err) }
  require(info.Size() <= fixtureLimitBytes, "fixture TSV exceeds bound")
  scanner := bufio.NewScanner(file)
  scanner.Buffer(make([]byte, 1024), fixtureLimitBytes)
  var result [][]string
  for scanner.Scan() {
    text := scanner.Text()
    require(!strings.ContainsAny(text, "\r\x00"), "fixture requires UTF-8/LF")
    fields := strings.Split(text, "\t")
    require(len(fields) == columns, "fixture column count")
    result = append(result, fields)
  }
  if err := scanner.Err(); err != nil { panic(err) }
  return result
}

// permuted changes only caller inventory order; the public generated writer chooses output order.
func permuted[T any](values []T, selected int) []T {
  require(selected >= 0 && selected < permutationCount, "invalid fixture permutation")
  count := len(values)
  result := make([]T, 0, count)
  if selected < 4 {
    for index := 0; index < count; index++ {
      position := index
      switch selected {
      case 1: position = count - index - 1
      case 2: position = (index + 1) % count
      case 3: position = (index + count / 2) % count
      }
      result = append(result, values[position])
    }
  } else {
    for group := 0; group < 2; group++ {
      for index := 0; index < count; index++ {
        position, parity := index, group
        if selected == 5 { position, parity = count - index - 1, 1 - group }
        if position % 2 == parity { result = append(result, values[position]) }
      }
    }
  }
  require(len(result) == count, "incomplete fixture permutation")
  return result
}

// provider observes the actual SDK runtime used by the public compress/flate implementation.
func provider() map[string]any {
  return map[string]any{
    "implementation": "Go compress/flate", "version": runtime.Version(), "metadata_method": 8,
    "options": map[string]any{"level": 6, "window_bytes": 32768, "wrapper": "raw", "xml_provider": "Serializer generated XML metadata"},
  }
}

// printJSON emits machine-readable SDK evidence after all producer work has completed successfully.
func printJSON(value any) {
  data, err := json.Marshal(value)
  if err != nil { panic(err) }
  fmt.Println(string(data))
}

// main uses only public generated physical ZIP and OPC APIs from an external consuming package.
func main() {
  arguments := os.Args
  if len(arguments) == 2 && arguments[1] == "--provider" { printJSON(provider()); return }
  require(len(arguments) == 5 || len(arguments) == 6, "fixture-dir profile permutation output [force64 0|1]")
  profile := arguments[2]
  opc := profile == "opc-stored" || profile == "opc-deflate"
  require(opc || profile == "zip-stored" || profile == "zip-deflate", "unknown profile")
  selected, err := strconv.Atoi(arguments[3])
  if err != nil { panic(err) }
  require(selected >= 0 && selected < permutationCount, "invalid permutation")
  compression := "stored"
  if profile == "zip-deflate" || profile == "opc-deflate" { compression = "deflate" }
  force := len(arguments) == 6 && arguments[5] == "1"
  require(len(arguments) == 5 || arguments[5] == "0" || force, "invalid forced ZIP64 selector")
  kind := "zip"
  if opc { kind = "opc" }
  var parts []m.@p@BinaryPart
  for _, row := range rows(arguments[1], kind, "parts.tsv", 4) {
    require(row[2] == "stored" || row[2] == "deflate", "unknown TSV storage")
    data, failure := hex.DecodeString(row[3])
    if failure != nil { panic(failure) }
    parts = append(parts, m.@p@BinaryPart{Data: data, Name: row[0], ContentType: row[1], Compression: compression})
  }
  parts = permuted(parts, selected)
  var links []m.@p@Relationship
  if opc {
    for _, row := range rows(arguments[1], "opc", "links.tsv", 5) {
      require(row[4] == "internal" || row[4] == "external", "unknown target mode")
      links = append(links, m.@p@Relationship{Source: row[0], ID: row[1], Type: row[2], Target: row[3], Mode: row[4]})
    }
  }
  links = permuted(links, selected)
  limits := m.@p@DefaultLimits()
  limits.ForceZIP64 = force
  budget, err := m.@p@NewBudget(limits)
  if err != nil { panic(err) }
  var output []byte
  if opc {
    output, err = m.@p@EncodeOPC(parts, links, budget)
  } else {
    writer, failure := m.@p@NewWriter(budget)
    if failure != nil { panic(failure) }
    defer writer.Close()
    for _, part := range parts {
      if failure = writer.Add(part.Name, part.Data, part.Compression); failure != nil { panic(failure) }
    }
    output, err = writer.Finish()
  }
  if err != nil { panic(err) }
  if err = os.WriteFile(arguments[4], output, 0600); err != nil { panic(err) }
  printJSON(map[string]any{"profile": profile, "permutation": selected, "parts": len(parts),
    "relationships": len(links), "force_zip64": force, "provider": provider()})
}
