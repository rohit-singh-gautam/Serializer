// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
	"bytes"
	"errors"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

var optionalProviders = map[string]SerializerCompressionProvider{"ZSTD": ZstdProvider{}, "LZ4": Lz4Provider{}}
var optionalCancellation = errors.New("original optional-provider cancellation")

// TestOptionalWrapperReentry applies the same callback boundary corpus to both real optional providers.
func TestOptionalWrapperReentry(t *testing.T) {
	for name, provider := range optionalProviders {
		t.Run(name, func(t *testing.T) {
			encoding := SerializerCompressionDefaultEncodeOptions(provider.CompressionFormat())
			encoding.Provider = provider
			decoding := SerializerCompressionDefaultDecodeOptions(provider.CompressionFormat())
			decoding.Provider = provider
			checkWrapperReentry(t, encoding, decoding)
		})
	}
}

// optionalPath selects only the qualification runner's retained fixture directory.
func optionalPath(name string) string {
	return filepath.Join(os.Getenv("SRL_COMPRESSION_FIXTURES"), name)
}

// optionalCode requires the precise public category and prevents partial owned results from escaping.
func optionalCode(t *testing.T, expected SerializerCompressionCode, value []byte, err error) {
	t.Helper()
	var failure *SerializerCompressionError
	if value != nil || !errors.As(err, &failure) || failure.Code != expected {
		t.Fatalf("Expected %s without output; got %d bytes and %v", expected, len(value), err)
	}
}

// optionalRead requires every corpus file, including empty expected plaintext, to exist.
func optionalRead(t *testing.T, name string) []byte {
	t.Helper()
	value, err := os.ReadFile(optionalPath(name))
	if err != nil {
		t.Fatal(err)
	}
	return value
}

// TestIndependentOptionalFrames replays the same independently produced standard-frame decisions as Java.
func TestIndependentOptionalFrames(t *testing.T) {
	for name, provider := range optionalProviders {
		for _, line := range strings.Split(strings.TrimSpace(string(optionalRead(t, name+".tsv"))), "\n") {
			fields := strings.Split(strings.TrimSuffix(line, "\r"), "\t")
			if len(fields) != 6 {
				t.Fatal("Malformed shared optional corpus row")
			}
			t.Run(name+"/"+fields[0], func(t *testing.T) {
				options := SerializerCompressionDefaultDecodeOptions(provider.CompressionFormat())
				options.Provider = provider
				values := []*int64{&options.MaxInputBytes, &options.MaxOutputBytes, &options.MaxWindowBytes}
				for index, value := range values {
					parsed, err := strconv.ParseInt(fields[index+3], 10, 64)
					if err != nil {
						t.Fatal(err)
					}
					*value = parsed
				}
				actual, err := SerializerCompressionDecompress(optionalRead(t, fields[0]), options)
				if fields[2] != "ok" {
					optionalCode(t, SerializerCompressionCode(strings.ToLower(fields[2])), actual, err)
				} else if err != nil || !bytes.Equal(actual, optionalRead(t, fields[1])) {
					t.Fatalf("Independent plaintext differs: %d bytes, %v", len(actual), err)
				}
			})
		}
	}
}

// TestOptionalOwningAndNativeBytes checks every native protocol with checksum-on/off and retained external output.
func TestOptionalOwningAndNativeBytes(t *testing.T) {
	value := NewAccountRecord()
	value.AccountId, value.Label = 17, "shared λ 🙂"
	for name, provider := range optionalProviders {
		for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
			native, err := value.Encode(protocol)
			if err != nil {
				t.Fatal(err)
			}
			for _, checked := range []bool{false, true} {
				encoding := SerializerCompressionDefaultEncodeOptions(provider.CompressionFormat())
				encoding.Provider, encoding.Checksum = provider, checked
				packed, err := SerializerCompressionEncode(value, protocol, encoding)
				if err != nil {
					t.Fatal(err)
				}
				decoding := SerializerCompressionDefaultDecodeOptions(provider.CompressionFormat())
				decoding.Provider = provider
				copy, err := SerializerCompressionDecode(packed, protocol, DecodeAccountRecord, decoding)
				if err != nil || copy == nil || copy.AccountId != value.AccountId || copy.Label != value.Label {
					t.Fatalf("Native object differs: %v", err)
				}
				plain, err := SerializerCompressionDecompress(packed, decoding)
				if err != nil || !bytes.Equal(plain, native) {
					t.Fatalf("Native bytes changed: %v", err)
				}
				stem := "go-" + name + "-" + strconv.Itoa(int(protocol)) + "-" + strconv.FormatBool(checked)
				if err = os.WriteFile(optionalPath(stem+".bin"), packed, 0600); err != nil {
					t.Fatal(err)
				}
				if err = os.WriteFile(optionalPath(stem+".plain"), native, 0600); err != nil {
					t.Fatal(err)
				}
				suffix, err := SerializerCompressionCompress(append(append([]byte{}, native...), 0), encoding)
				if err != nil {
					t.Fatal(err)
				}
				rejected, failure := SerializerCompressionDecode(suffix, protocol, DecodeAccountRecord, decoding)
				if rejected != nil || failure == nil {
					t.Fatal("Native suffix published a candidate")
				}
			}
		}
	}
}

// TestOptionalPolicyBounds checks exact/tight logical bytes and provider levels without unchecked allocation.
func TestOptionalPolicyBounds(t *testing.T) {
	plain := optionalRead(t, "plain.bin")
	for name, provider := range optionalProviders {
		t.Run(name, func(t *testing.T) {
			encoding := SerializerCompressionDefaultEncodeOptions(provider.CompressionFormat())
			encoding.Provider = provider
			packed, err := SerializerCompressionCompress(plain, encoding)
			if err != nil {
				t.Fatal(err)
			}
			encoding.MaxInputBytes, encoding.MaxOutputBytes = int64(len(plain)), int64(len(packed))
			exact, err := SerializerCompressionCompress(plain, encoding)
			if err != nil || !bytes.Equal(exact, packed) {
				t.Fatalf("Exact output budget failed: %v", err)
			}
			for _, input := range []bool{true, false} {
				tight := encoding
				if input {
					tight.MaxInputBytes--
				} else {
					tight.MaxOutputBytes--
				}
				result, failure := SerializerCompressionCompress(plain, tight)
				optionalCode(t, SerializerCompressionResourceLimit, result, failure)
			}
			decoding := SerializerCompressionDefaultDecodeOptions(provider.CompressionFormat())
			decoding.Provider, decoding.MaxInputBytes, decoding.MaxOutputBytes = provider, int64(len(packed)), int64(len(plain))
			result, err := SerializerCompressionDecompress(packed, decoding)
			if err != nil || !bytes.Equal(result, plain) {
				t.Fatalf("Exact expansion budget failed: %v", err)
			}
			for _, maximum := range []int64{0, 1, int64(len(plain) - 1)} {
				tight := decoding
				tight.MaxOutputBytes = maximum
				result, failure := SerializerCompressionDecompress(packed, tight)
				optionalCode(t, SerializerCompressionResourceLimit, result, failure)
			}
			decoding.MaxInputBytes--
			result, failure := SerializerCompressionDecompress(packed, decoding)
			optionalCode(t, SerializerCompressionResourceLimit, result, failure)
			for _, level := range []int{-1, 23} {
				invalid := encoding
				invalid.Level = level
				result, failure := SerializerCompressionCompress(plain, invalid)
				optionalCode(t, SerializerCompressionInvalidOptions, result, failure)
			}
			for _, limit := range []int64{-1, 0, 1} {
				invalid := SerializerCompressionDefaultDecodeOptions(provider.CompressionFormat())
				invalid.Provider, invalid.MaxWindowBytes = provider, limit
				result, failure := SerializerCompressionDecompress(packed, invalid)
				code := SerializerCompressionResourceLimit
				if limit < 0 {
					code = SerializerCompressionInvalidOptions
				}
				optionalCode(t, code, result, failure)
			}
		})
	}
}

// admissionProvider replaces only validation so a deployment failure cannot enter application work.
type admissionProvider struct{ SerializerCompressionProvider }

// ValidateEncode reports a selected but unavailable provider before native callbacks or stream construction.
func (admissionProvider) ValidateEncode(level int, checksum bool) error {
	return &SerializerCompressionError{Code: SerializerCompressionUnavailable, Message: "Explicit provider unavailable"}
}

// ValidateDecode reports unavailable deployment without touching compressed source bytes.
func (admissionProvider) ValidateDecode(maxWindowBytes int64) error {
	return &SerializerCompressionError{Code: SerializerCompressionUnavailable, Message: "Explicit provider unavailable"}
}

// effectStream detects any forbidden unavailable-provider I/O.
type effectStream struct{ effects *int }

// Read records actual source access.
func (source effectStream) Read(value []byte) (int, error) { *source.effects++; return 0, io.EOF }

// Write records actual sink publication.
func (sink effectStream) Write(value []byte) (int, error) { *sink.effects++; return len(value), nil }

// Encode records actual native application work.
func (value effectStream) Encode(protocol Protocol) ([]byte, error) {
	*value.effects++
	return nil, nil
}

// TestOptionalAdmission prevents missing/unavailable providers and invalid policies from entering I/O.
func TestOptionalAdmission(t *testing.T) {
	for _, provider := range optionalProviders {
		effects := 0
		stream := effectStream{&effects}
		format := provider.CompressionFormat()
		if SerializerCompressionAvailable(format) || !SerializerCompressionAvailableWithProvider(format, provider) {
			t.Fatal("Per-operation capability differs from the dependency-free default")
		}
		for _, selected := range []SerializerCompressionProvider{nil, admissionProvider{provider}} {
			encoding := SerializerCompressionDefaultEncodeOptions(format)
			encoding.Provider, encoding.CheckCancelled = selected, func() error { effects++; return nil }
			result, err := SerializerCompressionEncode(stream, JSON, encoding)
			optionalCode(t, SerializerCompressionUnavailable, result, err)
			if _, err = SerializerCompressionNewOutput(stream, encoding); err == nil {
				t.Fatal("Unavailable output constructed")
			}
			decoding := SerializerCompressionDefaultDecodeOptions(format)
			decoding.Provider, decoding.CheckCancelled = selected, func() error { effects++; return nil }
			if _, err = SerializerCompressionNewInput(stream, decoding); err == nil {
				t.Fatal("Unavailable input constructed")
			}
		}
		if effects != 0 {
			t.Fatalf("Unavailable admission produced %d effects", effects)
		}
		encoding := SerializerCompressionDefaultEncodeOptions(SerializerCompressionGzip)
		encoding.Provider = provider
		result, err := SerializerCompressionEncode(stream, JSON, encoding)
		optionalCode(t, SerializerCompressionInvalidOptions, result, err)
		encoding.Provider, encoding.Checksum = nil, true
		result, err = SerializerCompressionEncode(stream, JSON, encoding)
		optionalCode(t, SerializerCompressionInvalidOptions, result, err)
		if effects != 0 {
			t.Fatal("Invalid provider options entered native encoding")
		}
	}
}

// alteredProvider deliberately violates one trusted provider output contract to test the publication boundary.
type alteredProvider struct {
	SerializerCompressionProvider
	encode func([]byte, int, bool, io.Writer, func() error) error
	decode func([]byte, int64, io.Writer, func() error) error
}

// Compress selects the test contract violation or the actual maintained implementation.
func (provider alteredProvider) Compress(input []byte, level int, checksum bool, output io.Writer, check func() error) error {
	if provider.encode != nil {
		return provider.encode(input, level, checksum, output, check)
	}
	return provider.SerializerCompressionProvider.Compress(input, level, checksum, output, check)
}

// Decompress selects the test contract violation or the actual maintained implementation.
func (provider alteredProvider) Decompress(input []byte, window int64, output io.Writer, check func() error) error {
	if provider.decode != nil {
		return provider.decode(input, window, output, check)
	}
	return provider.SerializerCompressionProvider.Decompress(input, window, output, check)
}

// TestOptionalProviderPublication checks private output bounds, ignored sink failures and final integrity.
func TestOptionalProviderPublication(t *testing.T) {
	for _, provider := range optionalProviders {
		encoding := SerializerCompressionDefaultEncodeOptions(provider.CompressionFormat())
		encoding.Provider = provider
		packed, err := SerializerCompressionCompress([]byte("complete message"), encoding)
		if err != nil {
			t.Fatal(err)
		}
		for _, mutation := range []string{"trailing", "checksum", "swallowed-limit"} {
			bad := alteredProvider{SerializerCompressionProvider: provider}
			bad.encode = func(input []byte, level int, checksum bool, output io.Writer, check func() error) error {
				switch mutation {
				case "trailing":
					_, _ = output.Write(append(append([]byte{}, packed...), 0))
				case "checksum":
					return provider.Compress(input, level, !checksum, output, check)
				default:
					_, _ = output.Write(make([]byte, 32))
				}
				return nil
			}
			options := encoding
			options.Provider = bad
			expected := SerializerCompressionInvalidData
			if mutation == "swallowed-limit" {
				options.MaxOutputBytes, expected = 1, SerializerCompressionResourceLimit
			}
			result, failure := SerializerCompressionCompress([]byte("complete message"), options)
			optionalCode(t, expected, result, failure)
		}
		decoding := SerializerCompressionDefaultDecodeOptions(provider.CompressionFormat())
		decoding.Provider = alteredProvider{SerializerCompressionProvider: provider,
			decode: func(input []byte, window int64, output io.Writer, check func() error) error {
				_, err := output.Write([]byte("corrupt message!"))
				return err
			}}
		result, failure := SerializerCompressionDecompress(packed, decoding)
		optionalCode(t, SerializerCompressionInvalidData, result, failure)
	}
}

// cancelSink arms cancellation only after an externally visible final write.
type cancelSink struct {
	bytes.Buffer
	armed *bool
}

// Write records a complete sink write before changing the application's cancellation state.
func (sink *cancelSink) Write(data []byte) (int, error) {
	count, err := sink.Buffer.Write(data)
	*sink.armed = true
	return count, err
}

// TestOptionalCancellationAndLifecycle checks active and post-callback cancellation plus sticky borrowed streams.
func TestOptionalCancellationAndLifecycle(t *testing.T) {
	plain := optionalRead(t, "plain.bin")
	for _, provider := range optionalProviders {
		format := provider.CompressionFormat()
		encoding := SerializerCompressionDefaultEncodeOptions(format)
		encoding.Provider = provider
		packed, err := SerializerCompressionCompress(plain, encoding)
		if err != nil {
			t.Fatal(err)
		}
		for _, decompress := range []bool{false, true} {
			polls := 0
			check := func() error {
				polls++
				if polls >= 5 {
					return optionalCancellation
				}
				return nil
			}
			var result []byte
			var failure error
			if decompress {
				options := SerializerCompressionDefaultDecodeOptions(format)
				options.Provider, options.CheckCancelled = provider, check
				result, failure = SerializerCompressionDecompress(packed, options)
			} else {
				options := encoding
				options.CheckCancelled = check
				result, failure = SerializerCompressionCompress(plain, options)
			}
			if result != nil || failure != optionalCancellation {
				t.Fatalf("Active cancellation changed: %v", failure)
			}
		}
		armed := false
		check := func() error {
			if armed {
				return optionalCancellation
			}
			return nil
		}
		decoding := SerializerCompressionDefaultDecodeOptions(format)
		decoding.Provider, decoding.CheckCancelled = provider, check
		for _, finishing := range []bool{false, true} {
			input, err := SerializerCompressionNewInput(bytes.NewReader(packed), decoding)
			if err != nil {
				t.Fatal(err)
			}
			if finishing {
				if value, err := io.ReadAll(input); err != nil || !bytes.Equal(value, plain) {
					t.Fatalf("Borrowed input failed: %v", err)
				}
			}
			armed = true
			if finishing {
				err = input.Finish()
			} else {
				_, err = input.Read(make([]byte, 1))
			}
			if err != optionalCancellation {
				t.Fatal("Late input cancellation was ignored")
			}
			if _, err = input.Read(make([]byte, 1)); err != optionalCancellation {
				t.Fatal("Failed input resumed")
			}
			armed = false
		}
		sink := &cancelSink{armed: &armed}
		outputOptions := encoding
		outputOptions.CheckCancelled = check
		output, err := SerializerCompressionNewOutput(sink, outputOptions)
		if err != nil {
			t.Fatal(err)
		}
		if _, err = output.Write(plain); err != nil || sink.Len() != 0 {
			t.Fatal("Output published before Finish")
		}
		if err = output.Finish(); err != optionalCancellation || sink.Len() == 0 {
			t.Fatal("Post-publication cancellation was ignored")
		}
		if err = output.Finish(); err != optionalCancellation {
			t.Fatal("Failed output finish resumed")
		}
		armed = false
		candidate, err := SerializerCompressionDecode(packed, JSON,
			func(data []byte, protocol Protocol, limits ...Limits) (*AccountRecord, error) {
				armed = true
				return NewAccountRecord(), nil
			}, decoding)
		if candidate != nil || err != optionalCancellation {
			t.Fatal("Post-native-callback cancellation published a candidate")
		}
	}
}

// shortOptionalSink detects both an invalid short sink write and any forbidden retry.
type shortOptionalSink struct{ calls int }

// Write deliberately violates the complete-write contract without returning an application error.
func (sink *shortOptionalSink) Write(data []byte) (int, error) {
	sink.calls++
	return len(data) / 2, nil
}

// failingOptionalSource retains one application-owned source failure and counts subsequent access.
type failingOptionalSource struct {
	calls   int
	failure error
}

// Read returns the original external failure rather than a codec framing error.
func (source *failingOptionalSource) Read(data []byte) (int, error) {
	source.calls++
	return 0, source.failure
}

// TestOptionalExternalFailures preserves external I/O errors and prevents failed wrappers from resuming I/O.
func TestOptionalExternalFailures(t *testing.T) {
	for _, provider := range optionalProviders {
		encoding := SerializerCompressionDefaultEncodeOptions(provider.CompressionFormat())
		encoding.Provider = provider
		sink := &shortOptionalSink{}
		output, err := SerializerCompressionNewOutput(sink, encoding)
		if err != nil {
			t.Fatal(err)
		}
		if _, err = output.Write([]byte("bounded message")); err != nil {
			t.Fatal(err)
		}
		if err = output.Finish(); err != io.ErrShortWrite {
			t.Fatalf("Short-write failure changed: %v", err)
		}
		if err = output.Finish(); err != io.ErrShortWrite || sink.calls != 1 {
			t.Fatal("Short sink write was retried")
		}
		source := &failingOptionalSource{failure: errors.New("original source failure")}
		decoding := SerializerCompressionDefaultDecodeOptions(provider.CompressionFormat())
		decoding.Provider = provider
		input, err := SerializerCompressionNewInput(source, decoding)
		if err != nil {
			t.Fatal(err)
		}
		if _, err = input.Read(make([]byte, 1)); err != source.failure {
			t.Fatal("Application source error was replaced")
		}
		if err = input.Finish(); err != source.failure || source.calls != 1 {
			t.Fatal("Failed source resumed")
		}
	}
}
