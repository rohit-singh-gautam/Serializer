package main

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"testing"

	model "serializer-interoperability/internal/model"
)

// TestSelfExchange checks all protocols and union arms against the frozen independent wire fixture.
func TestSelfExchange(t *testing.T) {
	directory := t.TempDir()
	if err := os.WriteFile(filepath.Join(directory, "producers.txt"), []byte("go\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := exchange(directory, "emit"); err != nil {
		t.Fatal(err)
	}
	if err := exchange(directory, "verify"); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(filepath.Join("..", "..", "..", "test", "positional_binary_fixture.json"))
	if err != nil {
		t.Fatal(err)
	}
	var frozen struct {
		PrefixHex  string   `json:"prefix_hex"`
		VariantHex []string `json:"variant_hex"`
	}
	if err := json.Unmarshal(data, &frozen); err != nil {
		t.Fatal(err)
	}
	for variant, suffix := range frozen.VariantHex {
		expected, err := hex.DecodeString(frozen.PrefixHex + suffix)
		if err != nil {
			t.Fatal(err)
		}
		actual, err := os.ReadFile(filepath.Join(directory, fmt.Sprintf("go_BINARY_NONE_%d.bin", variant)))
		if err != nil {
			t.Fatal(err)
		}
		if !bytes.Equal(actual, expected) {
			t.Fatalf("variant %d differs from frozen positional bytes", variant)
		}
	}
}

// TestMalformedMessages requires exact framing for an independently constructed exchange message.
func TestMalformedMessages(t *testing.T) {
	for index, name := range protocols {
		protocol := model.Protocol(index)
		encoded, err := fixture(0).Encode(protocol)
		if err != nil {
			t.Fatal(err)
		}
		if len(encoded) == 0 {
			t.Fatal("encoded fixture is empty")
		}
		for _, malformed := range []struct {
			name string
			data []byte
		}{
			{"truncated", encoded[:len(encoded)-1]},
			{"trailing", append(bytes.Clone(encoded), 0)},
		} {
			t.Run(name+"/"+malformed.name, func(t *testing.T) {
				decoded, err := model.DecodeInteropMessage(malformed.data, protocol)
				if err == nil || decoded != nil {
					t.Fatal("invalid exchange message returned a decoded model")
				}
			})
		}
	}
}
