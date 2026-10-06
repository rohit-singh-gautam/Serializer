package main

import (
	"bytes"
	"fmt"
	"os"
)

// run edits one owning model and checks complete values using every wire protocol.
func run() error {
	{
		typed := NewUint16Version()
		encoded, err := typed.Encode(BINARY_NONE)
		if err != nil { return err }
		if !bytes.Equal(encoded, []byte{44, 1}) { return fmt.Errorf("Revision width") }
		for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil { return err }
			copy, err := DecodeUint16Version(payload, protocol)
			if err != nil { return err }
			decoded, err := copy.Encode(BINARY_NONE)
			if err != nil { return err }
			if !bytes.Equal(encoded, decoded) { return fmt.Errorf("Revision mismatch") }
		}
	}
	{
		typed := NewUint32Version()
		encoded, err := typed.Encode(BINARY_NONE)
		if err != nil { return err }
		if !bytes.Equal(encoded, []byte{112, 17, 1, 0}) { return fmt.Errorf("Revision width") }
		for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil { return err }
			copy, err := DecodeUint32Version(payload, protocol)
			if err != nil { return err }
			decoded, err := copy.Encode(BINARY_NONE)
			if err != nil { return err }
			if !bytes.Equal(encoded, decoded) { return fmt.Errorf("Revision mismatch") }
		}
	}
	{
		typed := NewUint64Version()
		encoded, err := typed.Encode(BINARY_NONE)
		if err != nil { return err }
		if !bytes.Equal(encoded, []byte{255, 255, 255, 255, 255, 255, 255, 255}) { return fmt.Errorf("Revision width") }
		for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil { return err }
			copy, err := DecodeUint64Version(payload, protocol)
			if err != nil { return err }
			decoded, err := copy.Encode(BINARY_NONE)
			if err != nil { return err }
			if !bytes.Equal(encoded, decoded) { return fmt.Errorf("Revision mismatch") }
		}
	}
	{
		typed := NewFloatVersion()
		encoded, err := typed.Encode(BINARY_NONE)
		if err != nil { return err }
		if !bytes.Equal(encoded, []byte{205, 204, 204, 61}) { return fmt.Errorf("Revision width") }
		for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil { return err }
			copy, err := DecodeFloatVersion(payload, protocol)
			if err != nil { return err }
			decoded, err := copy.Encode(BINARY_NONE)
			if err != nil { return err }
			if !bytes.Equal(encoded, decoded) { return fmt.Errorf("Revision mismatch") }
		}
	}
	{
		typed := NewDoubleVersion()
		encoded, err := typed.Encode(BINARY_NONE)
		if err != nil { return err }
		if !bytes.Equal(encoded, []byte{0, 0, 0, 0, 0, 0, 4, 64}) { return fmt.Errorf("Revision width") }
		for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil { return err }
			copy, err := DecodeDoubleVersion(payload, protocol)
			if err != nil { return err }
			decoded, err := copy.Encode(BINARY_NONE)
			if err != nil { return err }
			if !bytes.Equal(encoded, decoded) { return fmt.Errorf("Revision mismatch") }
		}
	}
	{
		typed := NewDotted2Version()
		encoded, err := typed.Encode(BINARY_NONE)
		if err != nil { return err }
		if !bytes.Equal(encoded, []byte{1, 0, 10, 0}) { return fmt.Errorf("Revision width") }
		for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil { return err }
			copy, err := DecodeDotted2Version(payload, protocol)
			if err != nil { return err }
			decoded, err := copy.Encode(BINARY_NONE)
			if err != nil { return err }
			if !bytes.Equal(encoded, decoded) { return fmt.Errorf("Revision mismatch") }
		}
	}
	{
		typed := NewDotted3Version()
		encoded, err := typed.Encode(BINARY_NONE)
		if err != nil { return err }
		if !bytes.Equal(encoded, []byte{1, 0, 10, 0, 0, 0}) { return fmt.Errorf("Revision width") }
		for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil { return err }
			copy, err := DecodeDotted3Version(payload, protocol)
			if err != nil { return err }
			decoded, err := copy.Encode(BINARY_NONE)
			if err != nil { return err }
			if !bytes.Equal(encoded, decoded) { return fmt.Errorf("Revision mismatch") }
		}
	}
	{
		typed := NewDotted4Version()
		encoded, err := typed.Encode(BINARY_NONE)
		if err != nil { return err }
		if !bytes.Equal(encoded, []byte{1, 0, 10, 0, 0, 0, 4, 0}) { return fmt.Errorf("Revision width") }
		for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil { return err }
			copy, err := DecodeDotted4Version(payload, protocol)
			if err != nil { return err }
			decoded, err := copy.Encode(BINARY_NONE)
			if err != nil { return err }
			if !bytes.Equal(encoded, decoded) { return fmt.Errorf("Revision mismatch") }
		}
	}
	input, err := os.ReadFile(os.Args[1])
	if err != nil {
		return err
	}
	limits := DefaultLimits()
	limits.ReadPolicy = COMPATIBLE
	value, err := DecodeExampleModel(input, JSON, limits)
	if err != nil {
		return err
	}
	oldBytes, err := value.Encode(BINARY_NONE)
	if err != nil { return err }
	old, err := DecodeExampleModel(oldBytes, BINARY_NONE, limits)
	if err != nil { return err }
	if old.Version != 8 || old.OldName != "Ada" { return fmt.Errorf("Historical mismatch") }
	value.Name = value.OldName
	value.Version = 10
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
	fmt.Println("Version migration and four protocols passed")
}
