package main

import (
	"bytes"
	"math"
	"testing"
)

// reject requires an error and no partially decoded object.
func reject(t *testing.T, kind string, input []byte, protocol Protocol, limits Limits) {
	t.Helper()
	var err error
	switch kind {
	case "message":
		value, failure := DecodeInteropMessage(input, protocol, limits)
		err = failure
		if err != nil && value != nil {
			t.Fatal("Partial message escaped")
		}
	case "bool":
		_, err = DecodeCheckBoolValue(input, protocol, limits)
	case "integer":
		_, err = DecodeCheckIntegerValue(input, protocol, limits)
	case "float":
		_, err = DecodeCheckFloatValue(input, protocol, limits)
	case "text":
		_, err = DecodeCheckTextValue(input, protocol, limits)
	case "bytes":
		_, err = DecodeCheckBytesValue(input, protocol, limits)
	case "recursive":
		_, err = DecodeCheckRecursive(input, protocol, limits)
	default:
		t.Fatal("Unknown test kind")
	}
	if err == nil {
		t.Fatalf("Accepted malformed %s input (%d)", kind, protocol)
	}
}

// TestBoundaries exercises real generated codecs across every protocol and truncated prefix.
func TestBoundaries(t *testing.T) {
	for protocol := JSON; protocol <= BINARY_STRING; protocol++ {
		expected := NewInteropMessage()
		encoded, err := expected.Encode(protocol)
		if err != nil {
			t.Fatal(err)
		}
		actual, err := DecodeInteropMessage(encoded, protocol)
		if err != nil {
			t.Fatal(err)
		}
		output, err := actual.Encode(protocol)
		if err != nil || !bytes.Equal(output, encoded) {
			t.Fatal("Round trip mismatch", err)
		}
		for end := 0; end < len(encoded); end++ {
			reject(t, "message", encoded[:end], protocol, DefaultLimits())
		}
		reject(t, "message", append(encoded, 0), protocol, DefaultLimits())
		limits := DefaultLimits()
		limits.MaxBytes = len(encoded) - 1
		reject(t, "message", encoded, protocol, limits)
		limits = DefaultLimits()
		limits.MaxDepth = 0
		reject(t, "message", encoded, protocol, limits)
	}
	reject(t, "bool", []byte{2}, BINARY_NONE, DefaultLimits())
	reject(t, "bool", []byte(`{"value":null}`), JSON, DefaultLimits())
	for _, text := range []string{`{"value":01}`, `{"value":-1}`, `{"value":18446744073709551616}`, `{"value":1e3}`, `{"value":1,}`, `{"unknown":1}`} {
		reject(t, "integer", []byte(text), JSON, DefaultLimits())
	}
	for _, text := range []string{`{"value":NaN}`, `{"value":1e100}`, `{"value":1e-100}`, `{"value":+1}`} {
		reject(t, "float", []byte(text), JSON, DefaultLimits())
	}
	for _, text := range []string{`{"value":"\ud800"}`, `{"value":"\udc00"}`, `{"value":"\ud800x"}`, "{\"value\":\"x\n\"}"} {
		reject(t, "text", []byte(text), JSON, DefaultLimits())
	}
	reject(t, "text", []byte{2, 0xc0, 0xaf}, BINARY_NONE, DefaultLimits())
	limits := Limits{100, 3, 100, 64}
	reject(t, "text", []byte(`{"value":"abcdef"}`), JSON, limits)
	reject(t, "text", []byte(`{"value":"\u0061"}`), JSON, limits)
	reject(t, "bytes", []byte{255, 255, 255, 255}, BINARY_NONE, DefaultLimits())
	reject(t, "bytes", []byte{3, 1, 2}, BINARY_NONE, DefaultLimits())
	reject(t, "bytes", []byte(`{"values":[1,2,3]}`), JSON, Limits{100, 100, 2, 64})
	reject(t, "bytes", []byte(`{"values":[1,]}`), JSON, DefaultLimits())
	for _, text := range []string{`{"payload:missing":0}`, `{"states":["missing"]}`, `{"counts":[{"key":"a"}]}`, `{"counts":[{"key":"a","value":1,"extra":0}]}`} {
		reject(t, "message", []byte(text), JSON, DefaultLimits())
	}
}

// TestDefaultsAndMerging covers exact integers, Unicode, replacement collections, and duplicate objects.
func TestDefaultsAndMerging(t *testing.T) {
	integer, err := DecodeCheckIntegerValue([]byte(`{"value":18446744073709551615}`), JSON)
	if err != nil || integer.Value != math.MaxUint64 {
		t.Fatal("Lost uint64 precision", err)
	}
	empty, err := DecodeCheckTextValue([]byte{0x40, 0}, BINARY_NONE)
	if err != nil || empty.Value != "" {
		t.Fatal("Nonminimal compact compatibility", err)
	}
	text, err := DecodeCheckTextValue([]byte(`{"value":"\ud83d\ude80"}`), JSON)
	if err != nil || text.Value != "🚀" {
		t.Fatal("Surrogate pair", err)
	}
	text, err = DecodeCheckTextValue([]byte{3, 0xef, 0xbb, 0xbf}, BINARY_NONE)
	if err != nil || text.Value != "\ufeff" {
		t.Fatal("BOM preservation", err)
	}
	merged, err := DecodeInteropMessage([]byte(`{"nested":{"code":99},"nested":{"note":"merged"},"numbers":[1,2],"numbers":[3],"counts":[{"value":1,"key":"a"},{"key":"a","value":2}]}`), JSON)
	if err != nil || merged.Nested.Code != 99 || merged.Nested.Note != "merged" || len(merged.Numbers) != 1 || merged.Numbers[0] != 3 || merged.Counts["a"] != 2 {
		t.Fatal("Merge/replacement semantics", err)
	}
	defaults, err := DecodeInteropMessage([]byte("{} \r\n\t"), JSON)
	if err != nil || defaults.BigUnsigned != math.MaxUint64 {
		t.Fatal("Schema defaults", err)
	}
	if NewCheckDefaultValue().Escaped != "a\\b\n\"c" {
		t.Fatal("Escaped default")
	}
	cycle := NewCheckRecursive()
	cycle.Children = append(cycle.Children, cycle)
	if _, err := cycle.Encode(BINARY_NONE); err == nil {
		t.Fatal("Unbounded output cycle")
	}
	nested := NewCheckRecursive()
	nested.Children = append(nested.Children, NewCheckRecursive())
	encoded, _ := nested.Encode(BINARY_NONE)
	reject(t, "recursive", encoded, BINARY_NONE, Limits{100, 100, 100, 1})
	malformed := NewCheckTextValue()
	malformed.Value = string([]byte{0xff})
	if _, err := malformed.Encode(JSON); err == nil {
		t.Fatal("Invalid UTF-8 output")
	}
	reject(t, "message", nil, Protocol(99), DefaultLimits())
	reject(t, "message", nil, BINARY_NONE, Limits{-1, 1, 1, 1})
	var missing *InteropMessage
	if _, err := missing.Encode(JSON); err == nil {
		t.Fatal("Nil object encoded")
	}
}

// BenchmarkNativeCodec records allocations for an ordinary generated owning decode.
func BenchmarkNativeCodec(b *testing.B) {
	value := NewInteropMessage()
	value.Numbers = make([]int32, 1024)
	bytes, err := value.Encode(BINARY_NONE)
	if err != nil {
		b.Fatal(err)
	}
	b.ReportAllocs()
	b.SetBytes(int64(len(bytes)))
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if _, err := DecodeInteropMessage(bytes, BINARY_NONE); err != nil {
			b.Fatal(err)
		}
	}
}
