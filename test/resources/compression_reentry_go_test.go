// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
	"bytes"
	"errors"
	"io"
	"testing"
)

// reentrySink invokes one application callback after recording the borrowed write.
type reentrySink struct {
	calls int
	after func()
}

// Write simulates a sink that calls back into its owning wrapper before returning.
func (sink *reentrySink) Write(data []byte) (int, error) {
	sink.calls++
	if sink.after != nil {
		sink.after()
	}
	return len(data), nil
}

// reentrySource invokes an application callback after one actual borrowed read.
type reentrySource struct {
	*bytes.Reader
	calls int
	after func()
}

// Read detects any additional borrowed access caused by a nested wrapper operation.
func (source *reentrySource) Read(data []byte) (int, error) {
	source.calls++
	count, err := source.Reader.Read(data)
	if source.after != nil {
		source.after()
	}
	return count, err
}

// reentryEncoder exposes a native callback without changing the existing native model interface.
type reentryEncoder struct{ encode func() ([]byte, error) }

// Encode invokes the callback under the outer wrapper operation.
func (value reentryEncoder) Encode(protocol Protocol) ([]byte, error) { return value.encode() }

// requireReentryFailure checks the first sticky invalid-options error and rejects silent outer success.
func requireReentryFailure(t *testing.T, err error) {
	t.Helper()
	var failure *SerializerCompressionError
	if !errors.As(err, &failure) || failure.Code != SerializerCompressionInvalidOptions {
		t.Fatalf("Reentry did not fence the wrapper: %v", err)
	}
}

// checkWrapperReentry exercises cooperative hooks, borrowed callbacks and native callbacks for one provider.
func checkWrapperReentry(t *testing.T, encoding SerializerCompressionEncodeOptions, decoding SerializerCompressionDecodeOptions) {
	t.Helper()
	plain := []byte("one complete message")
	packed, err := SerializerCompressionCompress(plain, encoding)
	if err != nil {
		t.Fatal(err)
	}
	t.Run("output-hook", func(t *testing.T) {
		sink := &reentrySink{}
		var output *SerializerCompressionOutput
		armed := false
		options := encoding
		options.CheckCancelled = func() error {
			if armed {
				armed = false
				_ = output.Finish()
			}
			return nil
		}
		output, err = SerializerCompressionNewOutput(sink, options)
		if err != nil {
			t.Fatal(err)
		}
		armed = true
		_, failure := output.Write(plain)
		requireReentryFailure(t, failure)
		if again := output.Finish(); again != failure || sink.calls != 0 {
			t.Fatal("Reentrant hook reached or resumed the sink")
		}
	})
	for _, operation := range []string{"write", "finish"} {
		t.Run("output-sink-"+operation, func(t *testing.T) {
			sink := &reentrySink{}
			output, err := SerializerCompressionNewOutput(sink, encoding)
			if err != nil {
				t.Fatal(err)
			}
			if _, err = output.Write(plain); err != nil {
				t.Fatal(err)
			}
			armed := true
			sink.after = func() {
				if !armed {
					return
				}
				armed = false
				if operation == "write" {
					_, _ = output.Write([]byte("nested"))
				} else {
					_ = output.Finish()
				}
			}
			failure := output.Finish()
			requireReentryFailure(t, failure)
			if again := output.Finish(); again != failure || sink.calls != 1 {
				t.Fatal("Reentrant sink caused additional borrowed writes")
			}
		})
	}
	t.Run("output-native", func(t *testing.T) {
		sink := &reentrySink{}
		output, err := SerializerCompressionNewOutput(sink, encoding)
		if err != nil {
			t.Fatal(err)
		}
		failure := output.Encode(reentryEncoder{encode: func() ([]byte, error) {
			_ = output.Finish()
			return plain, nil
		}}, JSON)
		requireReentryFailure(t, failure)
		if sink.calls != 0 {
			t.Fatal("Reentrant native encoder reached the sink")
		}
	})
	t.Run("input-hook", func(t *testing.T) {
		source := &reentrySource{Reader: bytes.NewReader(packed)}
		var input *SerializerCompressionInput
		armed := false
		options := decoding
		options.CheckCancelled = func() error {
			if armed {
				armed = false
				_ = input.Finish()
			}
			return nil
		}
		input, err = SerializerCompressionNewInput(source, options)
		if err != nil {
			t.Fatal(err)
		}
		armed = true
		_, failure := input.Read(make([]byte, 1))
		requireReentryFailure(t, failure)
		if again := input.Finish(); again != failure || source.calls != 0 {
			t.Fatal("Reentrant input hook read the source")
		}
	})
	t.Run("input-source", func(t *testing.T) {
		source := &reentrySource{Reader: bytes.NewReader(packed)}
		input, err := SerializerCompressionNewInput(source, decoding)
		if err != nil {
			t.Fatal(err)
		}
		armed := true
		source.after = func() {
			if armed {
				armed = false
				_, _ = input.Read(make([]byte, 1))
			}
		}
		_, failure := input.Read(make([]byte, 1))
		requireReentryFailure(t, failure)
		if again := input.Finish(); again != failure || source.calls != 1 {
			t.Fatal("Reentrant source caused later borrowed reads")
		}
	})
	t.Run("input-finish-hook", func(t *testing.T) {
		var input *SerializerCompressionInput
		armed := false
		options := decoding
		options.CheckCancelled = func() error {
			if armed {
				armed = false
				_ = input.Finish()
			}
			return nil
		}
		input, err = SerializerCompressionNewInput(bytes.NewReader(packed), options)
		if err != nil {
			t.Fatal(err)
		}
		if _, err = io.ReadAll(input); err != nil {
			t.Fatal(err)
		}
		armed = true
		failure := input.Finish()
		requireReentryFailure(t, failure)
	})
	t.Run("input-native", func(t *testing.T) {
		input, err := SerializerCompressionNewInput(bytes.NewReader(packed), decoding)
		if err != nil {
			t.Fatal(err)
		}
		candidate, failure := SerializerCompressionDecodeFrom(input, JSON,
			func(data []byte, protocol Protocol, limits ...Limits) (*AccountRecord, error) {
				_ = input.Finish()
				return NewAccountRecord(), nil
			})
		if candidate != nil {
			t.Fatal("Reentrant native decoder published a candidate")
		}
		requireReentryFailure(t, failure)
	})
}

// TestWrapperReentry verifies the dependency-free default formats without permitting callback recursion.
func TestWrapperReentry(t *testing.T) {
	for _, format := range []SerializerCompressionFormat{SerializerCompressionNone,
		SerializerCompressionGzip, SerializerCompressionZlib, SerializerCompressionDeflate} {
		checkWrapperReentry(t, SerializerCompressionDefaultEncodeOptions(format),
			SerializerCompressionDefaultDecodeOptions(format))
	}
}
