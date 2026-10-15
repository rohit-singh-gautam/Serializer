package main

import (
	"bytes"
	"encoding/json"
	"io"
	"math/big"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"testing"

	model "serializer.example/managed/multilanguage/clients/go/internal/model"
)

// TestHistoryFixture preserves independent managed metadata while each consumer increments revision once.
func TestHistoryFixture(t *testing.T) {
	fixture := readTestFile(t, filepath.Join("..", "..", "history", "fixture.json"))
	value, err := model.DecodeExampleModel(fixture, model.JSON)
	if err != nil {
		t.Fatal(err)
	}
	initial, err := value.Encode(model.BINARY_INTEGER)
	if err != nil {
		t.Fatal(err)
	}
	directory := t.TempDir()
	input := filepath.Join(directory, "initial.bin")
	if err := os.WriteFile(input, initial, 0o600); err != nil {
		t.Fatal(err)
	}
	for index, name := range []string{"first.bin", "second.bin"} {
		output := filepath.Join(directory, name)
		if err := runExample(t, input, output); err != nil {
			t.Fatal(err)
		}
		actual, err := model.DecodeExampleModel(readTestFile(t, output), model.BINARY_INTEGER)
		if err != nil {
			t.Fatal(err)
		}
		if actual.Revision != uint32(index+1) {
			t.Fatalf("revision = %d, want %d", actual.Revision, index+1)
		}
		encoded, err := actual.Encode(model.JSON)
		if err != nil {
			t.Fatal(err)
		}
		expected := jsonDocument(t, fixture).(map[string]any)
		expected["revision"] = json.Number(strconv.Itoa(index + 1))
		requireJSON(t, expected, encoded)
		input = output
	}
}

// TestMalformedHistory rejects incomplete and overlong records before publishing any output.
func TestMalformedHistory(t *testing.T) {
	fixture := readTestFile(t, filepath.Join("..", "..", "history", "fixture.json"))
	value, err := model.DecodeExampleModel(fixture, model.JSON)
	if err != nil {
		t.Fatal(err)
	}
	encoded, err := value.Encode(model.BINARY_INTEGER)
	if err != nil {
		t.Fatal(err)
	}
	if len(encoded) == 0 {
		t.Fatal("encoded history fixture is empty")
	}
	for _, malformed := range []struct {
		name string
		data []byte
	}{
		{"truncated", encoded[:len(encoded)-1]},
		{"trailing", append(bytes.Clone(encoded), 0)},
	} {
		t.Run(malformed.name, func(t *testing.T) {
			directory := t.TempDir()
			input := filepath.Join(directory, "invalid.bin")
			output := filepath.Join(directory, "result.bin")
			if err := os.WriteFile(input, malformed.data, 0o600); err != nil {
				t.Fatal(err)
			}
			if err := runExample(t, input, output); err == nil {
				t.Fatal("consumer accepted an invalid managed record")
			}
			if _, err := os.Stat(output); !os.IsNotExist(err) {
				t.Fatalf("invalid history published output: %v", err)
			}
		})
	}
}

// exactNumber keeps full-width JSON integers distinct from JSON strings.
type exactNumber string

// jsonDocument preserves integer precision while loading an independent fixture or result.
func jsonDocument(t *testing.T, data []byte) any {
	t.Helper()
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.UseNumber()
	var value any
	if err := decoder.Decode(&value); err != nil {
		t.Fatal(err)
	}
	var extra any
	if err := decoder.Decode(&extra); err != io.EOF {
		t.Fatalf("expected exactly one JSON document: %v", err)
	}
	return value
}

// normalizeNumbers compares numeric values without rounding uint64 fields through float64.
func normalizeNumbers(value any) any {
	switch value := value.(type) {
	case json.Number:
		rational, ok := new(big.Rat).SetString(string(value))
		if !ok {
			panic("Invalid JSON number")
		}
		return exactNumber(rational.RatString())
	case []any:
		for index := range value {
			value[index] = normalizeNumbers(value[index])
		}
	case map[string]any:
		for key, item := range value {
			value[key] = normalizeNumbers(item)
		}
	}
	return value
}

// requireJSON compares all fixture fields, including exact numeric values and collection order.
func requireJSON(t *testing.T, expected any, actual []byte) {
	t.Helper()
	if !reflect.DeepEqual(normalizeNumbers(expected), normalizeNumbers(jsonDocument(t, actual))) {
		t.Fatalf("decoded JSON differs from the independent fixture: %s", actual)
	}
}

// readTestFile reports fixture or consumer-output failures at the calling test.
func readTestFile(t *testing.T, path string) []byte {
	t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	return data
}

// runExample invokes the authored consumer without starting another process; callers stay sequential.
func runExample(t *testing.T, input, output string) error {
	t.Helper()
	previous := os.Args
	os.Args = []string{"example", input, output}
	defer func() { os.Args = previous }()
	return run()
}
