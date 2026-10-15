package main

import (
	"bytes"
	"encoding/json"
	"io"
	"math/big"
	"os"
	"path/filepath"
	"reflect"
	"testing"

	model "serializer.example/go/generics/internal/model"
)

const genericFixtureJSON = `{"revision":9,"payload":{"value":{"key":4294967295,"value":"generic \u00e9\u0000"},"items":[{"key":4294967295,"value":"generic \u00e9\u0000"},{"key":0,"value":""}]},"lookup":[{"key":7,"value":{"value":{"key":4294967295,"value":"generic \u00e9\u0000"},"items":[{"key":4294967295,"value":"generic \u00e9\u0000"},{"key":0,"value":""}]}}]}`

// TestFourProtocolFixture preserves nested generic copies while editing only the selected payload.
func TestFourProtocolFixture(t *testing.T) {
	directory := t.TempDir()
	input := filepath.Join(directory, "fixture.json")
	output := filepath.Join(directory, "result.json")
	if err := os.WriteFile(input, []byte(genericFixtureJSON), 0o600); err != nil {
		t.Fatal(err)
	}
	expected := jsonDocument(t, []byte(genericFixtureJSON)).(map[string]any)
	expected["revision"] = json.Number("10")
	expected["payload"].(map[string]any)["value"].(map[string]any)["key"] = json.Number("7")
	if err := runExample(t, input, output); err != nil {
		t.Fatal(err)
	}
	requireJSON(t, expected, readTestFile(t, output))
}

// TestMalformedMessages rejects truncated and trailing data for each supported protocol.
func TestMalformedMessages(t *testing.T) {
	data := []byte(genericFixtureJSON)
	value, err := model.DecodeExampleModel(data, model.JSON)
	if err != nil {
		t.Fatal(err)
	}
	for _, protocol := range []struct {
		name  string
		value model.Protocol
	}{
		{"json", model.JSON},
		{"positional", model.BINARY_NONE},
		{"integer", model.BINARY_INTEGER},
		{"string", model.BINARY_STRING},
	} {
		encoded, err := value.Encode(protocol.value)
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
			t.Run(protocol.name+"/"+malformed.name, func(t *testing.T) {
				decoded, err := model.DecodeExampleModel(malformed.data, protocol.value)
				if err == nil || decoded != nil {
					t.Fatal("invalid message returned a decoded model")
				}
			})
		}
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
