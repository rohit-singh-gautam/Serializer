// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import ("bytes"; "encoding/json"; "errors"; "os"; "path/filepath"; "strconv"; "time")

// Preserve failures while requiring complete owning values from successful calls.
func checked[T any](value T, err error) T { if err != nil { panic(err) }; return value }

// Exercise the public operation callback and recover after every failed invocation.
func main() {
  for index, path := range os.Args[2:] {
    input := checked(os.ReadFile(path))
    value := checked(DecodeXmlExampleOrderXML(input))
    canonical := checked(value.Encode(BINARY_NONE))
    verified := 0
    for _, writing := range []bool{false, true} {
      for _, threshold := range []int{1, 1000, 0} {
        polls := 0; deadline := time.Now().Add(-time.Nanosecond); marker := errors.New("Caller XML interruption")
        options := XmlOperationOptions{Options: XmlOptions{DefaultXmlLimits(), nil}, Operation: func(string,int64,string) error {
          polls++; if threshold != 0 && polls >= threshold || threshold == 0 && !time.Now().Before(deadline) { return marker }; return nil
        }}
        var failure error
        if writing { candidate, err := value.EncodeXMLWithOperation(options); failure = err; if candidate != nil { panic("Failed encode published bytes") } } else {
          candidate, err := DecodeXmlExampleOrderXMLWithOperation(input, options); failure = err; if candidate != nil { panic("Failed decode published model") }
        }
        if failure != marker || threshold != 0 && polls != threshold { panic("Original caller failure lost") }
        recovered := checked(DecodeXmlExampleOrderXML(checked(value.EncodeXML())))
        if !bytes.Equal(canonical, checked(value.Encode(BINARY_NONE))) || !bytes.Equal(canonical, checked(recovered.Encode(BINARY_NONE))) { panic("Recovery changed values") }
        verified++
      }
    }
    if err := os.WriteFile(filepath.Join(os.Args[1],strconv.Itoa(index+1)+".wire"), canonical,0600); err != nil { panic(err) }
    if err := json.NewEncoder(os.Stdout).Encode(map[string]any{"fixture":index+1,"verified_interruptions":verified,"recovered":true}); err != nil { panic(err) }
  }
}
