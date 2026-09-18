package main

import (
	"bytes"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"strings"
	"time"
)

var protocols = []string{"JSON", "BINARY_NONE", "BINARY_INTEGER", "BINARY_STRING"}

// fixture constructs the shared values independently of the other implementations.
func fixture(variant int) *InteropMessage {
	value := NewInteropMessage()
	value.Text = "Ada \"Lovelace\" 🚀\n"
	for i := 0; i < 64; i++ {
		value.Text += "x"
	}
	value.Numbers = []int32{math.MinInt32, -1, 0, math.MaxInt32}
	value.Decimals = []float32{float32(math.Copysign(0, -1)), 1.5, -2.25}
	value.Flags = []bool{false, true}
	value.Labels = []string{"", "é", "🚀"}
	child := NewInteropDetail()
	child.Code = -9
	child.Note = "child"
	value.Children = []*InteropDetail{NewInteropDetail(), child}
	value.States = []InteropState{InteropStatePaused, InteropStateReady}
	value.Counts = map[string]uint64{"🚀": math.MaxUint64, "é": 7, "a": 0}
	value.Indexed = map[uint64]*InteropDetail{math.MaxUint64: child, 0: NewInteropDetail()}
	value.Toggles = map[bool]string{true: "yes", false: "no"}
	value.Enums = map[InteropState]int32{InteropStatePaused: -2, InteropStateReady: 1}
	value.PayloadIndex = variant
	value.PayloadNumber = -1234567890123456789
	value.PayloadRatio = -3.5
	value.PayloadState = InteropStatePaused
	return value
}

// exchange verifies all values using an independently constructed positional encoding.
func exchange(directory, mode string) error {
	manifest, err := os.ReadFile(filepath.Join(directory, "producers.txt"))
	if err != nil {
		return err
	}
	languages := strings.Fields(string(manifest))
	if err := os.MkdirAll(directory, 0755); err != nil {
		return err
	}
	for variant := 0; variant < 3; variant++ {
		expected := fixture(variant)
		canonical, err := expected.Encode(BINARY_NONE)
		if err != nil {
			return err
		}
		for p, name := range protocols {
			protocol := Protocol(p)
			encoded, err := expected.Encode(protocol)
			if err != nil {
				return err
			}
			if mode == "emit" {
				if err := os.WriteFile(filepath.Join(directory, fmt.Sprintf("go_%s_%d.bin", name, variant)), encoded, 0644); err != nil {
					return err
				}
			} else {
				for _, language := range languages {
					input, err := os.ReadFile(filepath.Join(directory, fmt.Sprintf("%s_%s_%d.bin", language, name, variant)))
					if err != nil {
						return err
					}
					actual, err := DecodeInteropMessage(input, protocol)
					if err != nil {
						return fmt.Errorf("%s -> go %s: %w", language, name, err)
					}
					output, err := actual.Encode(BINARY_NONE)
					if err != nil {
						return err
					}
					if !bytes.Equal(output, canonical) {
						return fmt.Errorf("%s -> go %s/%d value mismatch", language, name, variant)
					}
					if protocol != JSON && !bytes.Equal(input, encoded) {
						return fmt.Errorf("Canonical binary mismatch")
					}
				}
			}
		}
	}
	return nil
}

// benchmark reports local throughput after warmup without claiming cross-runtime parity.
func benchmark() error {
	value := fixture(0)
	const iterations = 10000
	for p, name := range protocols {
		protocol := Protocol(p)
		encoded, err := value.Encode(protocol)
		if err != nil {
			return err
		}
		for i := 0; i < 1000; i++ {
			if _, err := DecodeInteropMessage(encoded, protocol); err != nil {
				return err
			}
		}
		start := time.Now()
		for i := 0; i < iterations; i++ {
			if _, err := value.Encode(protocol); err != nil {
				return err
			}
		}
		encode := time.Since(start)
		start = time.Now()
		for i := 0; i < iterations; i++ {
			if _, err := DecodeInteropMessage(encoded, protocol); err != nil {
				return err
			}
		}
		fmt.Printf("go %s: %d bytes; encode %.0f/s; decode %.0f/s\n", name, len(encoded), iterations/encode.Seconds(), iterations/time.Since(start).Seconds())
	}
	return nil
}

// main runs one side of the all-language interoperability example.
func main() {
	if len(os.Args) != 3 {
		fmt.Fprintln(os.Stderr, "Usage: interop_go <directory> emit|verify|benchmark")
		os.Exit(2)
	}
	var err error
	if os.Args[2] == "benchmark" {
		err = benchmark()
	} else if os.Args[2] == "emit" || os.Args[2] == "verify" {
		err = exchange(os.Args[1], os.Args[2])
	} else {
		err = fmt.Errorf("Unknown mode")
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Println("go", os.Args[2], "passed")
}
