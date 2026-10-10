package main

import (
	"bytes"
	"fmt"
	"os"
)

// run edits one owning model and checks complete values using every wire protocol.
func run() error {
	defaults := NewExampleModel()
	if len(defaults.Pages) != 2 || len(defaults.Columns) != 3 || len(defaults.Headings) != 2 {
		return fmt.Errorf("Incorrect fixed defaults")
	}
	defaults.Pages[0].Title = "First"
	if defaults.Pages[1].Title != "" { return fmt.Errorf("Aliased fixed elements") }
	input, err := os.ReadFile(os.Args[1])
	if err != nil {
		return err
	}
	value, err := DecodeExampleModel(input, JSON)
	if err != nil {
		return err
	}
	value.LookupCache = 77
	value.Revision++
	canonical, err := value.Encode(BINARY_NONE)
	if err != nil {
		return err
	}
	for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
		encoded, err := value.Encode(protocol)
		if err != nil {
			return err
		}
		copy, err := DecodeExampleModel(encoded, protocol)
		if err != nil {
			return err
		}
		if copy.LookupCache != 0 { return fmt.Errorf("Transient cache persisted") }
		decoded, err := copy.Encode(BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(decoded, canonical) {
			return fmt.Errorf("Value mismatch")
		}
	}
	output, err := value.Encode(JSON)
	if err != nil {
		return err
	}
	return os.WriteFile(os.Args[2], output, 0644)
}

// main reports I/O or codec failures as a nonzero exit status.
func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Println("Four protocols passed")
}
