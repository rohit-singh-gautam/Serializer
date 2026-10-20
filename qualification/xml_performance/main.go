// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
	"bytes"
	"encoding/json"
	"os"
	"runtime"
	"strconv"
	"time"
)

var retained any

// Require successful complete-message results before retaining a timed result.
func checked[T any](value T, err error) T {
	if err != nil {
		panic(err)
	}
	return value
}

// Measure one generated owning operation; preparation and verification are untimed.
func main() {
	input := checked(os.ReadFile(os.Args[1]))
	mode := os.Args[2]
	repetitions := checked(strconv.Atoi(os.Args[4]))
	// BENCHMARK_SETUP
	canonical := checked(value.Encode(BINARY_NONE))
	var operation func() any
	switch mode {
	// BENCHMARK_XML_OPERATIONS
	case "native-encode":
		operation = func() any { return checked(value.Encode(BINARY_NONE)) }
	case "native-decode":
		operation = func() any { return checked(DecodeXmlExampleOrder(canonical, BINARY_NONE)) }
	default:
		panic("Unknown benchmark operation")
	}
	// Warm the same batch three times before five individually retained samples.
	for warmup := 0; warmup < 3; warmup++ {
		for iteration := 0; iteration < repetitions; iteration++ {
			retained = operation()
		}
	}
	samples := make([]int64, 5)
	for sample := range samples {
		start := time.Now()
		for iteration := 0; iteration < repetitions; iteration++ {
			retained = operation()
		}
		samples[sample] = time.Since(start).Nanoseconds()
	}
	runtime.GC()
	var before, after runtime.MemStats
	runtime.ReadMemStats(&before)
	retained = operation()
	runtime.ReadMemStats(&after)
	result, owning := retained.(*XmlExampleOrder)
	if !owning {
		encoded := retained.([]byte)
		// BENCHMARK_RESULT
	}
	if !bytes.Equal(checked(result.Encode(BINARY_NONE)), canonical) {
		panic("Measured result changed native values")
	}
	if err := os.WriteFile(os.Args[3], canonical, 0600); err != nil {
		panic(err)
	}
	// BENCHMARK_XML_OUTPUT
	if err := json.NewEncoder(os.Stdout).Encode(map[string]any{
		"wall_ns": samples, "repetitions": repetitions,
		"allocation_bytes": after.TotalAlloc - before.TotalAlloc,
		"allocation_method": "runtime.MemStats.TotalAlloc delta for one separate call; process-wide",
		"sdk": runtime.Version(),
	}); err != nil {
		panic(err)
	}
}
