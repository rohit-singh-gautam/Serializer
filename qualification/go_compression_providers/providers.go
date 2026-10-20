// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
	"bytes"
	"errors"
	"io"

	"github.com/klauspost/compress/zstd"
	"github.com/pierrec/lz4/v4"
)

// ZstdProvider supplies the pinned pure-Go provider without changing generated imports or global state.
// Native Zstandard levels 1..22 select the provider's four documented speed tiers, not native exact levels.
type ZstdProvider struct{}

// CompressionFormat binds this adapter to one explicit standard frame.
func (ZstdProvider) CompressionFormat() SerializerCompressionFormat { return SerializerCompressionZstd }

// ValidateEncode admits the adapter's documented options before native callbacks or borrowed I/O.
func (ZstdProvider) ValidateEncode(level int, checksum bool) error {
	if level < 1 || level > 22 {
		return &SerializerCompressionError{Code: SerializerCompressionInvalidOptions, Message: "Zstandard level must be between one and twenty-two"}
	}
	return nil
}

// ValidateDecode enforces the maintained provider's minimum history policy without reading input.
func (ZstdProvider) ValidateDecode(maxWindowBytes int64) error {
	if maxWindowBytes < 1024 {
		return &SerializerCompressionError{Code: SerializerCompressionResourceLimit, Message: "Zstandard requires a history budget of at least 1024 bytes"}
	}
	return nil
}

// Compress emits one checksum-selected frame using a single worker and bounded source batches.
func (ZstdProvider) Compress(input []byte, level int, checksum bool, output io.Writer, check func() error) error {
	if err := (ZstdProvider{}).ValidateEncode(level, checksum); err != nil {
		return err
	}
	encoder, err := zstd.NewWriter(output, zstd.WithEncoderConcurrency(1), zstd.WithLowerEncoderMem(true),
		zstd.WithEncoderCRC(checksum), zstd.WithEncoderLevel(zstd.EncoderLevelFromZstd(level)))
	if err != nil {
		return err
	}
	if err = providerWrite(encoder, input, check); err != nil {
		return err
	}
	if err = encoder.Close(); err != nil {
		return err
	}
	return providerPoll(check)
}

// Decompress disables eager whole-buffer decoding and bounds provider history with one synchronous worker.
// The provider's bounded internal tables/batches add heap overhead beyond the advertised frame window.
func (ZstdProvider) Decompress(input []byte, maxWindowBytes int64, output io.Writer, check func() error) error {
	if err := (ZstdProvider{}).ValidateDecode(maxWindowBytes); err != nil {
		return err
	}
	window := uint64(1)
	for window <= uint64(maxWindowBytes)/2 {
		window *= 2
	}
	const maximumFormatWindow = (1 << 41) + 7*(1<<38)
	if window > maximumFormatWindow {
		window = maximumFormatWindow
	}
	decoder, err := zstd.NewReader(bytes.NewReader(input), zstd.WithDecoderConcurrency(1),
		zstd.WithDecoderLowmem(true), zstd.WithDecoderMaxMemory(window),
		zstd.WithDecoderMaxWindow(window), zstd.WithDecodeBuffersBelow(0))
	if err != nil {
		return optionalDataFailure(err)
	}
	defer decoder.Close()
	return providerRead(output, decoder, check)
}

// Lz4Provider supplies standard independent output and accepts independently checked linked input frames.
type Lz4Provider struct{}

// CompressionFormat binds this adapter to the standard LZ4 frame format.
func (Lz4Provider) CompressionFormat() SerializerCompressionFormat { return SerializerCompressionLz4 }

// ValidateEncode admits only the explicit fast/high-compression levels supported by this adapter.
func (Lz4Provider) ValidateEncode(level int, checksum bool) error {
	if level < 0 || level > 9 {
		return &SerializerCompressionError{Code: SerializerCompressionInvalidOptions, Message: "LZ4 level must be between zero and nine"}
	}
	return nil
}

// ValidateDecode enforces the smallest standard LZ4 block allocation before input or native decoding.
func (Lz4Provider) ValidateDecode(maxWindowBytes int64) error {
	if maxWindowBytes < 64*1024 {
		return &SerializerCompressionError{Code: SerializerCompressionResourceLimit, Message: "LZ4 requires a block budget of at least 64 KiB"}
	}
	return nil
}

// Compress emits 64 KiB independent blocks with explicit block/content checksum policy and one worker.
func (Lz4Provider) Compress(input []byte, level int, checksum bool, output io.Writer, check func() error) error {
	if err := (Lz4Provider{}).ValidateEncode(level, checksum); err != nil {
		return err
	}
	levels := [...]lz4.CompressionLevel{lz4.Fast, lz4.Level1, lz4.Level2, lz4.Level3, lz4.Level4,
		lz4.Level5, lz4.Level6, lz4.Level7, lz4.Level8, lz4.Level9}
	encoder := lz4.NewWriter(output)
	if err := encoder.Apply(lz4.BlockSizeOption(lz4.Block64Kb), lz4.ConcurrencyOption(1),
		lz4.CompressionLevelOption(levels[level]), lz4.ChecksumOption(checksum),
		lz4.BlockChecksumOption(checksum), lz4.SizeOption(uint64(len(input)))); err != nil {
		return err
	}
	if err := providerWrite(encoder, input, check); err != nil {
		return err
	}
	if err := encoder.Close(); err != nil {
		return err
	}
	return providerPoll(check)
}

// Decompress relies on the generated frame admission before allocating bounded standard block/history buffers.
// Single-worker Read preserves linked-block dictionaries and validates provider checksums and known size.
func (Lz4Provider) Decompress(input []byte, maxWindowBytes int64, output io.Writer, check func() error) error {
	if err := (Lz4Provider{}).ValidateDecode(maxWindowBytes); err != nil {
		return err
	}
	decoder := lz4.NewReader(bytes.NewReader(input))
	if err := decoder.Apply(lz4.ConcurrencyOption(1)); err != nil {
		return err
	}
	defer decoder.Reset(nil)
	return providerRead(output, decoder, check)
}

// providerPoll preserves the caller's exact cancellation/deadline error.
func providerPoll(check func() error) error {
	if check != nil {
		return check()
	}
	return nil
}

// providerWrite feeds bounded batches and retains sink/cancellation failures without a finishing write.
func providerWrite(output io.Writer, input []byte, check func() error) error {
	const batchBytes = 64 * 1024
	for len(input) != 0 {
		if err := providerPoll(check); err != nil {
			return err
		}
		count := min(len(input), batchBytes)
		written, err := output.Write(input[:count])
		if err != nil {
			return err
		}
		if written != count {
			return io.ErrShortWrite
		}
		input = input[count:]
	}
	return providerPoll(check)
}

// providerRead bounds each provider expansion batch and stops immediately on sink, input or cancellation failure.
func providerRead(output io.Writer, input io.Reader, check func() error) error {
	const batchBytes = 64 * 1024
	const maximumEmptyReads = 100
	buffer := make([]byte, batchBytes)
	empty := 0
	for {
		if err := providerPoll(check); err != nil {
			return err
		}
		count, failure := input.Read(buffer)
		if count < 0 || count > len(buffer) {
			return &SerializerCompressionError{Code: SerializerCompressionInvalidData, Message: "Invalid optional decoder progress"}
		}
		if count != 0 {
			empty = 0
			written, err := output.Write(buffer[:count])
			if err != nil {
				return err
			}
			if written != count {
				return io.ErrShortWrite
			}
		} else if failure == nil {
			empty++
			if empty >= maximumEmptyReads {
				return &SerializerCompressionError{Code: SerializerCompressionInvalidData, Message: "Optional decoder made no progress"}
			}
		}
		if failure == io.EOF {
			return providerPoll(check)
		}
		if failure != nil {
			return optionalDataFailure(failure)
		}
	}
}

// optionalDataFailure classifies only private provider decoding; application sink/cancellation errors bypass it.
func optionalDataFailure(err error) error {
	code := SerializerCompressionInvalidData
	if errors.Is(err, zstd.ErrWindowSizeExceeded) || errors.Is(err, zstd.ErrDecoderSizeExceeded) {
		code = SerializerCompressionResourceLimit
	}
	return &SerializerCompressionError{Code: code, Message: "Optional compression provider rejected the frame"}
}
