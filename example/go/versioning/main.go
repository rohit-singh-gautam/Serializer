package main

import (
	"bytes"
	"fmt"
	"os"

	"serializer.example/go/versioning/internal/model"
)

// run edits one owning model and checks complete values using every wire protocol.
func run() error {
	{
		typed := model.NewUint16Version()
		encoded, err := typed.Encode(model.BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(encoded, []byte{44, 1}) {
			return fmt.Errorf("Revision width")
		}
		for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil {
				return err
			}
			copy, err := model.DecodeUint16Version(payload, protocol)
			if err != nil {
				return err
			}
			decoded, err := copy.Encode(model.BINARY_NONE)
			if err != nil {
				return err
			}
			if !bytes.Equal(encoded, decoded) {
				return fmt.Errorf("Revision mismatch")
			}
		}
	}
	{
		typed := model.NewUint32Version()
		encoded, err := typed.Encode(model.BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(encoded, []byte{112, 17, 1, 0}) {
			return fmt.Errorf("Revision width")
		}
		for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil {
				return err
			}
			copy, err := model.DecodeUint32Version(payload, protocol)
			if err != nil {
				return err
			}
			decoded, err := copy.Encode(model.BINARY_NONE)
			if err != nil {
				return err
			}
			if !bytes.Equal(encoded, decoded) {
				return fmt.Errorf("Revision mismatch")
			}
		}
	}
	{
		typed := model.NewUint64Version()
		encoded, err := typed.Encode(model.BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(encoded, []byte{255, 255, 255, 255, 255, 255, 255, 255}) {
			return fmt.Errorf("Revision width")
		}
		for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil {
				return err
			}
			copy, err := model.DecodeUint64Version(payload, protocol)
			if err != nil {
				return err
			}
			decoded, err := copy.Encode(model.BINARY_NONE)
			if err != nil {
				return err
			}
			if !bytes.Equal(encoded, decoded) {
				return fmt.Errorf("Revision mismatch")
			}
		}
	}
	{
		typed := model.NewFloatVersion()
		encoded, err := typed.Encode(model.BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(encoded, []byte{205, 204, 204, 61}) {
			return fmt.Errorf("Revision width")
		}
		for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil {
				return err
			}
			copy, err := model.DecodeFloatVersion(payload, protocol)
			if err != nil {
				return err
			}
			decoded, err := copy.Encode(model.BINARY_NONE)
			if err != nil {
				return err
			}
			if !bytes.Equal(encoded, decoded) {
				return fmt.Errorf("Revision mismatch")
			}
		}
	}
	{
		typed := model.NewDoubleVersion()
		encoded, err := typed.Encode(model.BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(encoded, []byte{0, 0, 0, 0, 0, 0, 4, 64}) {
			return fmt.Errorf("Revision width")
		}
		for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil {
				return err
			}
			copy, err := model.DecodeDoubleVersion(payload, protocol)
			if err != nil {
				return err
			}
			decoded, err := copy.Encode(model.BINARY_NONE)
			if err != nil {
				return err
			}
			if !bytes.Equal(encoded, decoded) {
				return fmt.Errorf("Revision mismatch")
			}
		}
	}
	{
		typed := model.NewDotted2Version()
		encoded, err := typed.Encode(model.BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(encoded, []byte{1, 0, 10, 0}) {
			return fmt.Errorf("Revision width")
		}
		for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil {
				return err
			}
			copy, err := model.DecodeDotted2Version(payload, protocol)
			if err != nil {
				return err
			}
			decoded, err := copy.Encode(model.BINARY_NONE)
			if err != nil {
				return err
			}
			if !bytes.Equal(encoded, decoded) {
				return fmt.Errorf("Revision mismatch")
			}
		}
	}
	{
		typed := model.NewDotted3Version()
		encoded, err := typed.Encode(model.BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(encoded, []byte{1, 0, 10, 0, 0, 0}) {
			return fmt.Errorf("Revision width")
		}
		for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil {
				return err
			}
			copy, err := model.DecodeDotted3Version(payload, protocol)
			if err != nil {
				return err
			}
			decoded, err := copy.Encode(model.BINARY_NONE)
			if err != nil {
				return err
			}
			if !bytes.Equal(encoded, decoded) {
				return fmt.Errorf("Revision mismatch")
			}
		}
	}
	{
		typed := model.NewDotted4Version()
		encoded, err := typed.Encode(model.BINARY_NONE)
		if err != nil {
			return err
		}
		if !bytes.Equal(encoded, []byte{1, 0, 10, 0, 0, 0, 4, 0}) {
			return fmt.Errorf("Revision width")
		}
		for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
			payload, err := typed.Encode(protocol)
			if err != nil {
				return err
			}
			copy, err := model.DecodeDotted4Version(payload, protocol)
			if err != nil {
				return err
			}
			decoded, err := copy.Encode(model.BINARY_NONE)
			if err != nil {
				return err
			}
			if !bytes.Equal(encoded, decoded) {
				return fmt.Errorf("Revision mismatch")
			}
		}
	}
	input, err := os.ReadFile(os.Args[1])
	if err != nil {
		return err
	}
	limits := model.DefaultLimits()
	limits.ReadPolicy = model.COMPATIBLE
	value, err := model.DecodeExampleModel(input, model.JSON, limits)
	if err != nil {
		return err
	}
	oldBytes, err := value.Encode(model.BINARY_NONE)
	if err != nil {
		return err
	}
	old, err := model.DecodeExampleModel(oldBytes, model.BINARY_NONE, limits)
	if err != nil {
		return err
	}
	if old.Version != 8 || old.OldName != "Ada" {
		return fmt.Errorf("Historical mismatch")
	}
	value.Name = value.OldName
	value.Version = 10
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
	fmt.Println("Version migration and four protocols passed")
}
