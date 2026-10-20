// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// SerializerCompressionProvider is a trusted, caller-owned optional Zstandard or LZ4 implementation.
// Validation must admit availability/options without input, output or native callbacks. Methods must
// stop on sink errors and poll cancellation between bounded batches; providers own and release their
// private codec resources. A provider must remain immutable throughout an operation. This contract
// bounds frame history and logical bytes, not the provider's total heap or an uninterrupted codec call.
type SerializerCompressionProvider interface {
  CompressionFormat() SerializerCompressionFormat
  ValidateEncode(level int, checksum bool) error
  ValidateDecode(maxWindowBytes int64) error
  Compress(input []byte, level int, checksum bool, output srlCompressionIO.Writer, check func() error) error
  Decompress(input []byte, maxWindowBytes int64, output srlCompressionIO.Writer, check func() error) error
}

// SerializerCompressionAvailableWithProvider reports explicit per-operation optional admission.
// It never installs global state; the provider's validation methods perform option-specific admission.
func SerializerCompressionAvailableWithProvider(format SerializerCompressionFormat, provider SerializerCompressionProvider) bool {
  if SerializerCompressionAvailable(format) { return true }
  return (format == SerializerCompressionZstd || format == SerializerCompressionLz4) &&
    provider != nil && provider.CompressionFormat() == format
}

// srlCompressionFrame records independently checked standard framing without retaining input slices.
type srlCompressionFrame struct {
  knownSize bool
  size uint64
  checksum bool
  digest uint32
}

// srlCompressionLittle reads a bounded unsigned frame field without architecture-dependent conversions.
func srlCompressionLittle(data []byte, offset, count int) (uint64, error) {
  if offset < 0 || count < 0 || count > 8 || offset > len(data) || count > len(data)-offset {
    return 0, srlCompressionFailure(SerializerCompressionInvalidData, "Truncated compression frame")
  }
  var value uint64
  for index := 0; index < count; index++ { value |= uint64(data[offset+index]) << uint(8*index) }
  return value, nil
}

// srlCompressionZstdFrame admits one RFC 8878 frame before a provider can allocate expansion history.
func srlCompressionZstdFrame(data []byte, options SerializerCompressionDecodeOptions) (srlCompressionFrame, error) {
  var frame srlCompressionFrame
  magic, err := srlCompressionLittle(data, 0, 4)
  if err != nil { return frame, err }
  if magic != 0xfd2fb528 { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Expected one standard Zstandard frame") }
  descriptor, err := srlCompressionLittle(data, 4, 1)
  if err != nil { return frame, err }
  if descriptor & 8 != 0 { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Reserved Zstandard descriptor") }
  single := descriptor & 32 != 0
  offset := 5
  var window uint64
  if !single {
    encoded, failure := srlCompressionLittle(data, offset, 1)
    if failure != nil { return frame, failure }; offset++
    base := uint64(1) << uint(10 + (encoded >> 3))
    window = base + (base >> 3) * (encoded & 7)
  }
  dictionaryBytes := int(descriptor & 3)
  if dictionaryBytes == 3 { dictionaryBytes = 4 }
  dictionary, err := srlCompressionLittle(data, offset, dictionaryBytes)
  if err != nil { return frame, err }; offset += dictionaryBytes
  if dictionary != 0 { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Zstandard dictionaries are unsupported") }
  sizeFlag := descriptor >> 6
  sizeBytes := 0
  if sizeFlag != 0 { sizeBytes = 1 << uint(sizeFlag) } else if single { sizeBytes = 1 }
  frame.knownSize = sizeBytes != 0
  frame.size, err = srlCompressionLittle(data, offset, sizeBytes)
  if err != nil { return frame, err }; offset += sizeBytes
  if sizeBytes == 2 { frame.size += 256 }
  if frame.knownSize && frame.size > uint64(options.MaxOutputBytes) {
    return frame, srlCompressionFailure(SerializerCompressionResourceLimit, "Zstandard content exceeds the output budget")
  }
  if single { window = frame.size }
  // Match the native reference profile's conservative power-of-two provider window admission.
  windowLimit := uint64(0)
  if options.MaxWindowBytes > 0 {
    windowLimit = 1
    for windowLimit <= uint64(options.MaxWindowBytes)/2 { windowLimit *= 2 }
  }
  if window > windowLimit {
    return frame, srlCompressionFailure(SerializerCompressionResourceLimit, "Zstandard history exceeds the window budget")
  }
  maximumBlock := uint64(128 * 1024)
  if window < maximumBlock { maximumBlock = window }
  for {
    if err = srlCompressionPoll(options.CheckCancelled); err != nil { return frame, err }
    block, failure := srlCompressionLittle(data, offset, 3)
    if failure != nil { return frame, failure }; offset += 3
    kind, count := (block >> 1) & 3, block >> 3
    if kind == 3 || count > maximumBlock {
      return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid Zstandard block")
    }
    stored := count
    if kind == 1 { stored = 1 }
    if stored > uint64(len(data)-offset) {
      return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Truncated Zstandard block")
    }
    offset += int(stored)
    if block & 1 != 0 { break }
  }
  frame.checksum = descriptor & 4 != 0
  if frame.checksum {
    digest, failure := srlCompressionLittle(data, offset, 4)
    if failure != nil { return frame, failure }; offset += 4
    frame.digest = uint32(digest)
  }
  if offset != len(data) { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Trailing or concatenated Zstandard input") }
  return frame, srlCompressionPoll(options.CheckCancelled)
}

// srlCompressionLz4Frame admits one standard frame, including descriptor and compressed-block checksums.
// Linked blocks retain their specified 64 KiB history inside the selected provider, within the block budget.
func srlCompressionLz4Frame(data []byte, options SerializerCompressionDecodeOptions) (srlCompressionFrame, error) {
  var frame srlCompressionFrame
  magic, err := srlCompressionLittle(data, 0, 4)
  if err != nil { return frame, err }
  if magic != 0x184d2204 { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Expected one standard LZ4 frame") }
  descriptor, err := srlCompressionLittle(data, 4, 2)
  if err != nil { return frame, err }
  flags, blockDescriptor := byte(descriptor), byte(descriptor >> 8)
  blockID := (blockDescriptor >> 4) & 7
  if flags >> 6 != 1 || flags & 2 != 0 || blockDescriptor & 0x8f != 0 || blockID < 4 || blockID > 7 {
    return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid LZ4 frame descriptor")
  }
  if flags & 1 != 0 { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "LZ4 dictionaries are unsupported") }
  maximumBlock := uint64(1) << uint(8 + 2*blockID)
  offset := 6
  frame.knownSize = flags & 8 != 0
  if frame.knownSize {
    frame.size, err = srlCompressionLittle(data, offset, 8)
    if err != nil { return frame, err }; offset += 8
  }
  header, err := srlCompressionLittle(data, offset, 1)
  if err != nil { return frame, err }
  digest, err := srlCompressionXXH32(data[4:offset], options.CheckCancelled)
  if err != nil { return frame, err }
  if byte(header) != byte(digest >> 8) { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid LZ4 descriptor checksum") }
  offset++
  if maximumBlock > uint64(options.MaxWindowBytes) {
    return frame, srlCompressionFailure(SerializerCompressionResourceLimit, "LZ4 block history exceeds the window budget")
  }
  if frame.knownSize && frame.size > uint64(options.MaxOutputBytes) {
    return frame, srlCompressionFailure(SerializerCompressionResourceLimit, "LZ4 content exceeds the output budget")
  }
  for {
    if err = srlCompressionPoll(options.CheckCancelled); err != nil { return frame, err }
    block, failure := srlCompressionLittle(data, offset, 4)
    if failure != nil { return frame, failure }; offset += 4
    if block == 0 { break }
    count := block & 0x7fffffff
    if count > maximumBlock {
      return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid LZ4 block length")
    }
    if count > uint64(len(data)-offset) { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Truncated LZ4 block") }
    if flags & 16 != 0 {
      expected, failure := srlCompressionLittle(data, offset+int(count), 4)
      if failure != nil { return frame, failure }
      actual, failure := srlCompressionXXH32(data[offset:offset+int(count)], options.CheckCancelled)
      if failure != nil { return frame, failure }
      if uint32(expected) != actual { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid LZ4 block checksum") }
    }
    offset += int(count)
    if flags & 16 != 0 { offset += 4 }
  }
  frame.checksum = flags & 4 != 0
  if frame.checksum {
    expected, failure := srlCompressionLittle(data, offset, 4)
    if failure != nil { return frame, failure }; offset += 4
    frame.digest = uint32(expected)
  }
  if offset != len(data) { return frame, srlCompressionFailure(SerializerCompressionInvalidData, "Trailing or concatenated LZ4 input") }
  return frame, srlCompressionPoll(options.CheckCancelled)
}

// srlCompressionOptionalFrame keeps standard framing and admission independent of permissive providers.
func srlCompressionOptionalFrame(data []byte, options SerializerCompressionDecodeOptions) (srlCompressionFrame, error) {
  if options.Format == SerializerCompressionZstd { return srlCompressionZstdFrame(data, options) }
  return srlCompressionLz4Frame(data, options)
}

// srlCompressionFrameResult independently checks known size and standard content integrity before publication.
func srlCompressionFrameResult(data []byte, format SerializerCompressionFormat, frame srlCompressionFrame, check func() error) error {
  if frame.knownSize && frame.size != uint64(len(data)) {
    return srlCompressionFailure(SerializerCompressionInvalidData, "Compression content size mismatch")
  }
  if frame.checksum {
    var actual uint32
    var err error
    if format == SerializerCompressionZstd { actual, err = srlCompressionXXH64Low(data, check) } else {
      actual, err = srlCompressionXXH32(data, check)
    }
    if err != nil { return err }
    if actual != frame.digest { return srlCompressionFailure(SerializerCompressionInvalidData, "Compression content checksum mismatch") }
  }
  return srlCompressionPoll(check)
}

// srlCompressionProviderCompress fences a trusted provider's output and checks its complete returned frame.
func srlCompressionProviderCompress(data []byte, options SerializerCompressionEncodeOptions, output *srlCompressionBuffer) ([]byte, error) {
  if err := options.Provider.Compress(data, options.Level, options.Checksum, output, options.CheckCancelled); err != nil { return nil, err }
  if output.err != nil { return nil, output.err }
  policy := SerializerCompressionDefaultDecodeOptions(options.Format)
  policy.MaxOutputBytes = int64(len(data))
  policy.MaxWindowBytes = int64(^uint64(0) >> 1)
  policy.CheckCancelled = options.CheckCancelled
  frame, err := srlCompressionOptionalFrame(output.bytes, policy)
  if err != nil { return nil, err }
  if frame.checksum != options.Checksum {
    return nil, srlCompressionFailure(SerializerCompressionInvalidData, "Compression provider changed checksum policy")
  }
  if err = srlCompressionFrameResult(data, options.Format, frame, options.CheckCancelled); err != nil { return nil, err }
  return output.bytes, nil
}

// srlCompressionProviderDecompress admits framing/history before calling the trusted bounded provider.
func srlCompressionProviderDecompress(data []byte, options SerializerCompressionDecodeOptions, output *srlCompressionBuffer) ([]byte, error) {
  frame, err := srlCompressionOptionalFrame(data, options)
  if err != nil { return nil, err }
  if err = options.Provider.Decompress(data, options.MaxWindowBytes, output, options.CheckCancelled); err != nil { return nil, err }
  if output.err != nil { return nil, output.err }
  if err = srlCompressionFrameResult(output.bytes, options.Format, frame, options.CheckCancelled); err != nil { return nil, err }
  return output.bytes, nil
}
)serializer"
R"serializer(
// srlCompressionRotate32 performs the fixed-width rotation used by standard XXH32.
func srlCompressionRotate32(value uint32, count uint) uint32 { return value << count | value >> (32-count) }
// srlCompressionRotate64 performs the fixed-width rotation used by standard XXH64.
func srlCompressionRotate64(value uint64, count uint) uint64 { return value << count | value >> (64-count) }

// srlCompressionXXH32 computes seed-zero standard frame checksums with bounded cancellation polling.
func srlCompressionXXH32(data []byte, check func() error) (uint32, error) {
  const prime1 uint32 = 2654435761
  const prime2 uint32 = 2246822519
  const prime3 uint32 = 3266489917
  const prime4 uint32 = 668265263
  const prime5 uint32 = 374761393
  // Each lane consumes one little-endian word with defined modular arithmetic.
  round := func(lane, word uint32) uint32 { return srlCompressionRotate32(lane + word*prime2, 13) * prime1 }
  offset := 0
  var hash uint32
  if len(data) >= 16 {
    lane1, lane2, lane3, lane4 := prime1, prime2, uint32(0), uint32(0)
    lane1 += prime2; lane4 -= prime1
    for len(data)-offset >= 16 {
      if offset % srlCompressionChunkBytes == 0 { if err := srlCompressionPoll(check); err != nil { return 0, err } }
      lane1 = round(lane1, binary.LittleEndian.Uint32(data[offset:offset+4]))
      lane2 = round(lane2, binary.LittleEndian.Uint32(data[offset+4:offset+8]))
      lane3 = round(lane3, binary.LittleEndian.Uint32(data[offset+8:offset+12]))
      lane4 = round(lane4, binary.LittleEndian.Uint32(data[offset+12:offset+16]))
      offset += 16
    }
    hash = srlCompressionRotate32(lane1, 1) + srlCompressionRotate32(lane2, 7) +
      srlCompressionRotate32(lane3, 12) + srlCompressionRotate32(lane4, 18)
  } else { hash = prime5 }
  hash += uint32(len(data))
  for len(data)-offset >= 4 {
    hash = srlCompressionRotate32(hash + binary.LittleEndian.Uint32(data[offset:offset+4])*prime3, 17) * prime4
    offset += 4
  }
  for offset < len(data) { hash = srlCompressionRotate32(hash + uint32(data[offset])*prime5, 11) * prime1; offset++ }
  hash ^= hash >> 15; hash *= prime2; hash ^= hash >> 13; hash *= prime3; hash ^= hash >> 16
  return hash, srlCompressionPoll(check)
}

// srlCompressionXXH64Low computes the low 32 bits of seed-zero XXH64 required by Zstandard.
func srlCompressionXXH64Low(data []byte, check func() error) (uint32, error) {
  const prime1 uint64 = 11400714785074694791
  const prime2 uint64 = 14029467366897019727
  const prime3 uint64 = 1609587929392839161
  const prime4 uint64 = 9650029242287828579
  const prime5 uint64 = 2870177450012600261
  // Four independent lanes and final merges implement the standard fixed-width checksum.
  round := func(lane, word uint64) uint64 { return srlCompressionRotate64(lane + word*prime2, 31) * prime1 }
  offset := 0
  var hash uint64
  if len(data) >= 32 {
    lanes := [4]uint64{prime1, prime2, 0, 0}
    lanes[0] += prime2; lanes[3] -= prime1
    for len(data)-offset >= 32 {
      if offset % srlCompressionChunkBytes == 0 { if err := srlCompressionPoll(check); err != nil { return 0, err } }
      for index := range lanes { lanes[index] = round(lanes[index], binary.LittleEndian.Uint64(data[offset+8*index:offset+8*index+8])) }
      offset += 32
    }
    hash = srlCompressionRotate64(lanes[0], 1) + srlCompressionRotate64(lanes[1], 7) +
      srlCompressionRotate64(lanes[2], 12) + srlCompressionRotate64(lanes[3], 18)
    for _, lane := range lanes { hash ^= round(0, lane); hash = hash*prime1 + prime4 }
  } else { hash = prime5 }
  hash += uint64(len(data))
  for len(data)-offset >= 8 {
    hash ^= round(0, binary.LittleEndian.Uint64(data[offset:offset+8]))
    hash = srlCompressionRotate64(hash, 27)*prime1 + prime4
    offset += 8
  }
  if len(data)-offset >= 4 {
    hash ^= uint64(binary.LittleEndian.Uint32(data[offset:offset+4]))*prime1
    hash = srlCompressionRotate64(hash, 23)*prime2 + prime3; offset += 4
  }
  for offset < len(data) {
    hash ^= uint64(data[offset])*prime5
    hash = srlCompressionRotate64(hash, 11)*prime1; offset++
  }
  hash ^= hash >> 33; hash *= prime2; hash ^= hash >> 29; hash *= prime3; hash ^= hash >> 32
  return uint32(hash), srlCompressionPoll(check)
}
)serializer"
