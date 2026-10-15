package main

import (
	"bytes"
	"fmt"
	"os"

	"serializer.example/go/basic/internal/model"
)

// run reads one owning model and checks complete values using every wire protocol.
func run() error {
	input, err := os.ReadFile(os.Args[1])
	if err != nil {
		return err
	}
	value, err := model.DecodeExampleModel(input, model.JSON)
	if err != nil {
		return err
	}
	canonical, err := value.Encode(model.BINARY_NONE)
	if err != nil {
		return err
	}
	for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
		encoded, err := value.Encode(protocol)
		if err != nil {
			return err
		}
		copy, err := model.DecodeExampleModel(encoded, protocol)
		if err != nil {
			return err
		}
		decoded, err := copy.Encode(model.BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(decoded, canonical) {
			return fmt.Errorf("Value mismatch")
		}
	}
	output, err := value.Encode(model.JSON)
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
