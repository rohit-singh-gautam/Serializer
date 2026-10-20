// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// SerializerCompressionFormat selects one complete standard frame without auto-detection.
type SerializerCompressionFormat uint8
const (
  SerializerCompressionNone SerializerCompressionFormat = iota
  SerializerCompressionZstd
  SerializerCompressionLz4
  SerializerCompressionGzip
  SerializerCompressionZlib
  SerializerCompressionDeflate
)

// SerializerCompressionCode classifies framing, provider, policy and logical byte-limit failures.
type SerializerCompressionCode string
const (
  SerializerCompressionUnavailable SerializerCompressionCode = "unavailable"
  SerializerCompressionInvalidOptions SerializerCompressionCode = "invalid_options"
  SerializerCompressionInvalidData SerializerCompressionCode = "invalid_data"
  SerializerCompressionResourceLimit SerializerCompressionCode = "resource_limit"
)

// SerializerCompressionError contains no payload excerpts; callback and I/O errors retain their types.
type SerializerCompressionError struct { Code SerializerCompressionCode; Message string }
// Error describes one stable compression failure.
func (failure *SerializerCompressionError) Error() string { return string(failure.Code) + ": " + failure.Message }

const (
  srlCompressionDefaultBytes int64 = 64 * 1024 * 1024
  srlCompressionDefaultWindowBytes int64 = 8 * 1024 * 1024
  srlCompressionDeflateWindowBytes int64 = 32 * 1024
  srlCompressionChunkBytes = 64 * 1024
  srlCompressionMaxEmptyReads = 100
  srlCompressionBinaryNoneOnly = __srl_compression_binary_none_only__
)

// SerializerCompressionEncodeOptions bounds staged input and output; CheckCancelled returns an application error.
// Copy options before concurrent use; callbacks and wrappers add no synchronization.
type SerializerCompressionEncodeOptions struct {
  Format SerializerCompressionFormat
  Level int
  Checksum bool
  Provider SerializerCompressionProvider
  MaxInputBytes, MaxOutputBytes int64
  CheckCancelled func() error
}
// SerializerCompressionDecodeOptions bounds compressed bytes, expanded bytes and provider history separately.
// Limits is copied by value and used by fresh native decoding; these are logical bounds, not total heap limits.
type SerializerCompressionDecodeOptions struct {
  Format SerializerCompressionFormat
  Provider SerializerCompressionProvider
  MaxInputBytes, MaxOutputBytes, MaxWindowBytes int64
  Limits Limits
  CheckCancelled func() error
}

// SerializerCompressionDefaultEncodeOptions returns independent standard defaults; identity has no level.
func SerializerCompressionDefaultEncodeOptions(format SerializerCompressionFormat) SerializerCompressionEncodeOptions {
  level := 6
  if format == SerializerCompressionNone || format == SerializerCompressionLz4 { level = 0 }
  if format == SerializerCompressionZstd { level = 3 }
  return SerializerCompressionEncodeOptions{Format: format, Level: level,
    Checksum: format == SerializerCompressionZstd || format == SerializerCompressionLz4,
    MaxInputBytes: srlCompressionDefaultBytes, MaxOutputBytes: srlCompressionDefaultBytes}
}
// SerializerCompressionDefaultDecodeOptions returns independent compression and native decode policies.
func SerializerCompressionDefaultDecodeOptions(format SerializerCompressionFormat) SerializerCompressionDecodeOptions {
  return SerializerCompressionDecodeOptions{Format: format, MaxInputBytes: srlCompressionDefaultBytes,
    MaxOutputBytes: srlCompressionDefaultBytes, MaxWindowBytes: srlCompressionDefaultWindowBytes,
    Limits: DefaultLimits()}
}
// SerializerCompressionAvailable reports this standard-library provider profile without loading external modules.
func SerializerCompressionAvailable(format SerializerCompressionFormat) bool {
  return format == SerializerCompressionNone || format == SerializerCompressionGzip ||
    format == SerializerCompressionZlib || format == SerializerCompressionDeflate
}
// srlCompressionFailure creates a payload-free classified failure.
func srlCompressionFailure(code SerializerCompressionCode, message string) error {
  return &SerializerCompressionError{Code: code, Message: message}
}
// srlCompressionPoll preserves the original cancellation or deadline error.
func srlCompressionPoll(check func() error) error { if check != nil { return check() }; return nil }
// srlCompressionFormat checks known format values and provider absence before native callbacks or I/O.
func srlCompressionFormat(format SerializerCompressionFormat, provider SerializerCompressionProvider) error {
  if format > SerializerCompressionDeflate { return srlCompressionFailure(SerializerCompressionInvalidOptions, "Unknown compression format") }
  if !SerializerCompressionAvailableWithProvider(format, provider) { return srlCompressionFailure(SerializerCompressionUnavailable, "Compression provider is unavailable") }
  if SerializerCompressionAvailable(format) && provider != nil {
    return srlCompressionFailure(SerializerCompressionInvalidOptions, "An external provider requires Zstandard or LZ4")
  }
  return nil
}
// srlCompressionEncodePolicy validates every format-specific option before accepting source bytes.
func srlCompressionEncodePolicy(options SerializerCompressionEncodeOptions) error {
  if err := srlCompressionFormat(options.Format, options.Provider); err != nil { return err }
  if options.MaxInputBytes < 0 || options.MaxOutputBytes < 0 {
    return srlCompressionFailure(SerializerCompressionInvalidOptions, "Negative compression byte limit")
  }
  if options.Format == SerializerCompressionZstd || options.Format == SerializerCompressionLz4 {
    if err := options.Provider.ValidateEncode(options.Level, options.Checksum); err != nil { return err }
  } else if options.Checksum {
    return srlCompressionFailure(SerializerCompressionInvalidOptions, "The checksum option requires Zstandard or LZ4")
  } else if options.Format == SerializerCompressionNone {
    if options.Level != 0 { return srlCompressionFailure(SerializerCompressionInvalidOptions, "Identity compression has no level") }
  } else if options.Level < 0 || options.Level > 9 {
    return srlCompressionFailure(SerializerCompressionInvalidOptions, "DEFLATE level must be between zero and nine")
  }
  return srlCompressionPoll(options.CheckCancelled)
}
// srlCompressionDecodePolicy validates compression and native policies before any source I/O.
func srlCompressionDecodePolicy(options SerializerCompressionDecodeOptions) error {
  if err := srlCompressionFormat(options.Format, options.Provider); err != nil { return err }
  if options.MaxInputBytes < 0 || options.MaxOutputBytes < 0 || options.MaxWindowBytes < 0 ||
    options.Limits.MaxBytes < 0 || options.Limits.MaxStringBytes < 0 ||
    options.Limits.MaxElements < 0 || options.Limits.MaxDepth < 0 || options.Limits.ReadPolicy > FLEXIBLE {
    return srlCompressionFailure(SerializerCompressionInvalidOptions, "Invalid compression or native decode limits")
  }
  if options.Format == SerializerCompressionZstd || options.Format == SerializerCompressionLz4 {
    if options.MaxWindowBytes == 0 { return srlCompressionFailure(SerializerCompressionResourceLimit, "Compression history budget is zero") }
    if err := options.Provider.ValidateDecode(options.MaxWindowBytes); err != nil { return err }
  } else if options.Format != SerializerCompressionNone && options.MaxWindowBytes < srlCompressionDeflateWindowBytes {
    return srlCompressionFailure(SerializerCompressionResourceLimit, "DEFLATE history exceeds the window budget")
  }
  return srlCompressionPoll(options.CheckCancelled)
}
// srlCompressionProtocol rejects disabled or unknown native formats before decoding or serialization.
func srlCompressionProtocol(protocol Protocol) error {
  if protocol > BINARY_STRING || (srlCompressionBinaryNoneOnly && protocol != BINARY_NONE) {
    return srlCompressionFailure(SerializerCompressionInvalidOptions, "Native protocol is disabled or unknown")
  }
  return nil
}

// srlCompressionBuffer owns bounded output and retains the first callback or byte-limit failure.
type srlCompressionBuffer struct {
  bytes []byte
  maximum int64
  check func() error
  err error
  checksum bool
  crc uint32
}
// Write accepts complete chunks without publishing a partial owned result.
func (output *srlCompressionBuffer) Write(data []byte) (int, error) {
  if output.err != nil { return 0, output.err }
  if output.err = srlCompressionPoll(output.check); output.err != nil { return 0, output.err }
  if int64(len(data)) > output.maximum - int64(len(output.bytes)) {
    output.err = srlCompressionFailure(SerializerCompressionResourceLimit, "Compression output exceeds its byte budget")
    return 0, output.err
  }
  output.bytes = append(output.bytes, data...)
  if output.checksum { output.crc = srlCompressionCRC32.Update(output.crc, srlCompressionCRC32.IEEETable, data) }
  return len(data), nil
}
// srlCompressionCopy stages bounded chunks so identity framing retains the same cancellation boundaries.
func srlCompressionCopy(output *srlCompressionBuffer, data []byte) error {
  for len(data) != 0 {
    count := len(data); if count > srlCompressionChunkBytes { count = srlCompressionChunkBytes }
    if _, err := output.Write(data[:count]); err != nil { return err }
    data = data[count:]
  }
  return srlCompressionPoll(output.check)
}

// SerializerCompressionCompress returns owned standard-frame bytes; failure publishes no output.
// Existing native Encode calls remain direct; whole-message compression stages serialized and compressed bytes.
func SerializerCompressionCompress(data []byte, options SerializerCompressionEncodeOptions) ([]byte, error) {
  if err := srlCompressionEncodePolicy(options); err != nil { return nil, err }
  if int64(len(data)) > options.MaxInputBytes {
    return nil, srlCompressionFailure(SerializerCompressionResourceLimit, "Compression input exceeds its byte budget")
  }
  output := &srlCompressionBuffer{maximum: options.MaxOutputBytes, check: options.CheckCancelled}
  if options.Format == SerializerCompressionNone {
    if err := srlCompressionCopy(output, data); err != nil { return nil, err }
    return output.bytes, nil
  }
  if options.Format == SerializerCompressionZstd || options.Format == SerializerCompressionLz4 {
    return srlCompressionProviderCompress(data, options, output)
  }
  var encoder srlCompressionIO.WriteCloser
  var err error
  switch options.Format {
  case SerializerCompressionGzip: encoder, err = srlCompressionGzip.NewWriterLevel(output, options.Level)
  case SerializerCompressionZlib: encoder, err = srlCompressionZlib.NewWriterLevel(output, options.Level)
  case SerializerCompressionDeflate: encoder, err = srlCompressionFlate.NewWriter(output, options.Level)
  }
  if err != nil { return nil, srlCompressionFailure(SerializerCompressionInvalidOptions, "Invalid compression options") }
  for len(data) != 0 {
    if err = srlCompressionPoll(options.CheckCancelled); err != nil { return nil, err }
    count := len(data); if count > srlCompressionChunkBytes { count = srlCompressionChunkBytes }
    written, failure := encoder.Write(data[:count])
    if failure != nil { return nil, failure }
    if written != count { return nil, srlCompressionIO.ErrShortWrite }
    data = data[count:]
  }
  if err = srlCompressionPoll(options.CheckCancelled); err != nil { return nil, err }
  if err = encoder.Close(); err != nil { return nil, err }
  if err = srlCompressionPoll(options.CheckCancelled); err != nil { return nil, err }
  return output.bytes, nil
}

)serializer"
R"serializer(
const (
  srlCompressionDeflateMaxBits = 15
  srlCompressionDeflateSymbols = 288
  srlCompressionDeflateDistanceSymbols = 32
  srlCompressionDeflateCheckInterval = 1024
)

// srlCompressionDeflateBits borrows exact raw frame bytes and retains at most one partial byte.
type srlCompressionDeflateBits struct {
  data []byte
  position int
  value uint32
  count uint
  work int
  check func() error
  err error
}
// invalid retains an earlier cancellation error instead of replacing it with a framing diagnostic.
func (bits *srlCompressionDeflateBits) invalid(message string) error {
  if bits.err == nil { bits.err = srlCompressionFailure(SerializerCompressionInvalidData, message) }; return bits.err
}
// poll provides bounded cooperative checks without allocating input-dependent scratch.
func (bits *srlCompressionDeflateBits) poll() error {
  if bits.err == nil { bits.err = srlCompressionPoll(bits.check) }; return bits.err
}
// read consumes at most sixteen little-endian bits without reading past their exact byte extent.
func (bits *srlCompressionDeflateBits) read(size uint) int {
  if bits.err != nil { return 0 }
  bits.work++; if bits.work == srlCompressionDeflateCheckInterval { bits.work = 0; if bits.poll() != nil { return 0 } }
  for bits.count < size {
    if bits.position == len(bits.data) { bits.invalid("Truncated DEFLATE bits"); return 0 }
    bits.value |= uint32(bits.data[bits.position]) << bits.count; bits.position++; bits.count += 8
  }
  result := int(bits.value & ((uint32(1) << size) - 1)); bits.value >>= size; bits.count -= size; return result
}
// align discards only the unused padding bits before one stored-block length.
func (bits *srlCompressionDeflateBits) align() { bits.value = 0; bits.count = 0 }

// srlCompressionDeflateHuffman uses fixed canonical tables independent of advertised alphabet counts.
type srlCompressionDeflateHuffman struct {
  counts, first, offsets [srlCompressionDeflateMaxBits+1]int
  symbols [srlCompressionDeflateSymbols]uint16
  maximum int
}
// build rejects over/incomplete trees except the standard single-symbol and unused-distance cases.
func (table *srlCompressionDeflateHuffman) build(lengths []uint8, codeLengths, allowEmpty bool, bits *srlCompressionDeflateBits) bool {
  if bits.err != nil { return false }; *table = srlCompressionDeflateHuffman{}; used := 0
  for _, size := range lengths {
    if size > srlCompressionDeflateMaxBits { bits.invalid("Invalid DEFLATE code length"); return false }
    if size != 0 { table.counts[size]++; used++; if int(size) > table.maximum { table.maximum = int(size) } }
  }
  if used == 0 { if allowEmpty { return true }; bits.invalid("Empty DEFLATE Huffman tree"); return false }
  left := 1
  for size := 1; size <= srlCompressionDeflateMaxBits; size++ {
    left = (left << 1) - table.counts[size]
    if left < 0 { bits.invalid("Oversubscribed DEFLATE tree"); return false }
  }
  if left != 0 && (codeLengths || used != 1 || table.maximum != 1) { bits.invalid("Incomplete DEFLATE tree"); return false }
  code, offset := 0, 0
  for size := 1; size <= srlCompressionDeflateMaxBits; size++ {
    code = (code + table.counts[size-1]) << 1; table.first[size] = code
    table.offsets[size] = offset; offset += table.counts[size]
  }
  next := table.offsets
  for symbol, size := range lengths { if size != 0 { table.symbols[next[size]] = uint16(symbol); next[size]++ } }
  return true
}
// decode reads canonical Huffman bits in their transmitted most-significant-code-bit order.
func (table *srlCompressionDeflateHuffman) decode(bits *srlCompressionDeflateBits) int {
  code := 0
  for size := 1; size <= table.maximum; size++ {
    code = (code << 1) | bits.read(1); if bits.err != nil { return 0 }
    index := code - table.first[size]
    if index >= 0 && index < table.counts[size] { return int(table.symbols[table.offsets[size]+index]) }
  }
  bits.invalid("Invalid DEFLATE Huffman code"); return 0
}
// fixed constructs one standard fixed alphabet in bounded local storage.
func (table *srlCompressionDeflateHuffman) fixed(distance bool, bits *srlCompressionDeflateBits) bool {
  var lengths [srlCompressionDeflateSymbols]uint8
  count := len(lengths); if distance { count = srlCompressionDeflateDistanceSymbols }
  for index := 0; index < count; index++ {
    if distance { lengths[index] = 5 } else if index < 144 { lengths[index] = 8 } else if index < 256 { lengths[index] = 9 } else if index < 280 { lengths[index] = 7 } else { lengths[index] = 8 }
  }
  return table.build(lengths[:count], false, false, bits)
}

// srlCompressionCheckDeflateWindow validates/counts raw grammar without expanding or allocating output.
// Only small-window RFC1950 frames use this preflight; the provider still checks the Adler checksum.
func srlCompressionCheckDeflateWindow(data []byte, window int, maximum int64, check func() error) error {
  lengthBase := [...]int{3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258}
  lengthExtra := [...]uint{0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0}
  distanceBase := [...]int{1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577}
  distanceExtra := [...]uint{0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13}
  codeOrder := [...]int{16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15}
  bits := srlCompressionDeflateBits{data: data, check: check}; if bits.poll() != nil { return bits.err }
  var fixedLiteral, fixedDistance srlCompressionDeflateHuffman
  if !fixedLiteral.fixed(false, &bits) || !fixedDistance.fixed(true, &bits) { return bits.err }
  var output int64
  for {
    if bits.poll() != nil { return bits.err }; final, kind := bits.read(1), bits.read(2)
    if bits.err != nil { return bits.err }
    if kind == 0 {
      bits.align(); count, complement := bits.read(16), bits.read(16)
      if bits.err != nil { return bits.err }
      if count ^ 65535 != complement || count > len(data)-bits.position { return bits.invalid("Invalid stored DEFLATE extent") }
      if int64(count) > maximum-output { return srlCompressionFailure(SerializerCompressionResourceLimit, "Expanded output exceeds limit") }
      bits.position += count; output += int64(count)
    } else {
      if kind == 3 { return bits.invalid("Reserved DEFLATE block type") }
      literal, distance := fixedLiteral, fixedDistance
      if kind == 2 {
        literalCount, distanceCount, codeCount := bits.read(5)+257, bits.read(5)+1, bits.read(4)+4
        if bits.err != nil { return bits.err }
        if literalCount > 286 { return bits.invalid("Invalid DEFLATE literal alphabet") }
        var codeLengths [19]uint8
        for index := 0; index < codeCount; index++ { codeLengths[codeOrder[index]] = uint8(bits.read(3)) }
        var codes srlCompressionDeflateHuffman
        if !codes.build(codeLengths[:], true, false, &bits) { return bits.err }
        var lengths [srlCompressionDeflateSymbols+srlCompressionDeflateDistanceSymbols]uint8
        total, position := literalCount+distanceCount, 0
        for position < total {
          symbol := codes.decode(&bits); if bits.err != nil { return bits.err }
          if symbol < 16 { lengths[position] = uint8(symbol); position++; continue }
          width, base := uint(7), 11
          if symbol == 16 { width, base = 2, 3 } else if symbol == 17 { width, base = 3, 3 }
          count := bits.read(width)+base; if bits.err != nil { return bits.err }
          if symbol == 16 && position == 0 || count > total-position { return bits.invalid("Invalid DEFLATE length repeat") }
          var value uint8; if symbol == 16 { value = lengths[position-1] }
          for end := position+count; position < end; position++ { lengths[position] = value }
        }
        if lengths[256] == 0 { return bits.invalid("Missing DEFLATE end code") }
        if !literal.build(lengths[:literalCount], false, false, &bits) || !distance.build(lengths[literalCount:total], false, true, &bits) { return bits.err }
      }
      for {
        symbol := literal.decode(&bits); if bits.err != nil { return bits.err }; if symbol == 256 { break }
        if symbol < 256 {
          if output == maximum { return srlCompressionFailure(SerializerCompressionResourceLimit, "Expanded output exceeds limit") }; output++; continue
        }
        lengthCode := symbol-257
        if lengthCode >= len(lengthBase) { return bits.invalid("Reserved DEFLATE length") }
        count := lengthBase[lengthCode]+bits.read(lengthExtra[lengthCode]); distanceCode := distance.decode(&bits)
        if bits.err != nil { return bits.err }
        if distanceCode >= len(distanceBase) { return bits.invalid("Reserved DEFLATE distance") }
        back := distanceBase[distanceCode]+bits.read(distanceExtra[distanceCode]); if bits.err != nil { return bits.err }
        if int64(back) > output || back > window { return bits.invalid("DEFLATE distance exceeds advertised history") }
        if int64(count) > maximum-output { return srlCompressionFailure(SerializerCompressionResourceLimit, "Expanded output exceeds limit") }; output += int64(count)
      }
    }
    if final != 0 { break }
  }
  if bits.position != len(data) { return bits.invalid("Trailing DEFLATE frame data") }; return bits.poll()
}
// srlCompressionSmallZlibWindow gates additional work to valid dictionary-free headers below32KiB.
// Normal RFC1950 32KiB, gzip and raw DEFLATE retain their existing standard-library hot paths.
func srlCompressionSmallZlibWindow(data []byte, options SerializerCompressionDecodeOptions) error {
  if len(data) < 2 || data[0] & 15 != 8 || data[0] >> 4 >= 7 || data[1] & 32 != 0 ||
    (uint16(data[0]) << 8 | uint16(data[1])) % 31 != 0 { return nil }
  const headerBytes, trailerBytes = 2, 4
  if len(data) < headerBytes+trailerBytes { return srlCompressionFailure(SerializerCompressionInvalidData, "Truncated zlib frame") }
  return srlCompressionCheckDeflateWindow(data[headerBytes:len(data)-trailerBytes], 1 << (uint(data[0] >> 4)+8), options.MaxOutputBytes, options.CheckCancelled)
}

)serializer"
R"serializer(
// srlCompressionGzipHeader validates bounded RFC 1952 fields without the provider's optional-string length cap.
// The returned extent starts raw DEFLATE data; the caller separately verifies one trailer and exact input.
func srlCompressionGzipHeader(data []byte, check func() error) (int, error) {
  const headerBytes = 10
  const flagsHeaderCRC byte = 2
  const flagsExtra byte = 4
  const flagsName byte = 8
  const flagsComment byte = 16
  const flagsReserved byte = 0xe0
  // Return a fixed diagnostic without retaining optional header contents.
  invalid := func() (int, error) { return 0, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid gzip header") }
  if len(data) < headerBytes || data[0] != 0x1f || data[1] != 0x8b || data[2] != 8 || data[3] & flagsReserved != 0 { return invalid() }
  flags := data[3]
  offset := headerBytes
  if flags & flagsExtra != 0 {
    if len(data) - offset < 2 { return invalid() }
    length := int(binary.LittleEndian.Uint16(data[offset:offset+2])); offset += 2
    if length > len(data) - offset { return invalid() }; offset += length
  }
  for _, flag := range []byte{flagsName, flagsComment} {
    if flags & flag == 0 { continue }
    terminated := false
    for offset < len(data) {
      if err := srlCompressionPoll(check); err != nil { return 0, err }
      count := len(data) - offset; if count > srlCompressionChunkBytes { count = srlCompressionChunkBytes }
      end := srlCompressionBytes.IndexByte(data[offset:offset+count], 0)
      if end >= 0 { offset += end + 1; terminated = true; break }
      offset += count
    }
    if !terminated { return invalid() }
  }
  if flags & flagsHeaderCRC != 0 {
    if len(data) - offset < 2 { return invalid() }
    var checksum uint32
    for position := 0; position < offset; {
      if err := srlCompressionPoll(check); err != nil { return 0, err }
      count := offset - position; if count > srlCompressionChunkBytes { count = srlCompressionChunkBytes }
      checksum = srlCompressionCRC32.Update(checksum, srlCompressionCRC32.IEEETable, data[position:position+count]); position += count
    }
    if uint16(checksum) != binary.LittleEndian.Uint16(data[offset:offset+2]) { return invalid() }; offset += 2
  }
  if err := srlCompressionPoll(check); err != nil { return 0, err }
  return offset, nil
}

// SerializerCompressionDecompress validates exactly one frame before returning owned plaintext.
// Advertised sizes never drive allocation; provider history and logical output are bounded independently.
func SerializerCompressionDecompress(data []byte, options SerializerCompressionDecodeOptions) ([]byte, error) {
  if err := srlCompressionDecodePolicy(options); err != nil { return nil, err }
  if int64(len(data)) > options.MaxInputBytes {
    return nil, srlCompressionFailure(SerializerCompressionResourceLimit, "Compressed input exceeds its byte budget")
  }
  output := &srlCompressionBuffer{maximum: options.MaxOutputBytes, check: options.CheckCancelled,
    checksum: options.Format == SerializerCompressionGzip}
  if options.Format == SerializerCompressionNone {
    if err := srlCompressionCopy(output, data); err != nil { return nil, err }
    return output.bytes, nil
  }
  if options.Format == SerializerCompressionZstd || options.Format == SerializerCompressionLz4 {
    return srlCompressionProviderDecompress(data, options, output)
  }
  framed := data
  var trailer []byte
  if options.Format == SerializerCompressionGzip {
    const trailerBytes = 8
    offset, err := srlCompressionGzipHeader(data, options.CheckCancelled)
    if err != nil { return nil, err }
    if len(data) - offset < trailerBytes {
      return nil, srlCompressionFailure(SerializerCompressionInvalidData, "Truncated gzip member")
    }
    trailer = data[len(data)-trailerBytes:]
    framed = data[offset:len(data)-trailerBytes]
  }
  if options.Format == SerializerCompressionZlib {
    if err := srlCompressionSmallZlibWindow(data, options); err != nil { return nil, err }
  }
  source := srlCompressionBytes.NewReader(framed)
  var decoder srlCompressionIO.ReadCloser
  var err error
  if options.Format == SerializerCompressionZlib { decoder, err = srlCompressionZlib.NewReader(source) } else {
    decoder = srlCompressionFlate.NewReader(source)
  }
  if err != nil { return nil, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid compressed stream") }
  // Provider cleanup owns only private memory and never closes a borrowed application source.
  defer decoder.Close()
  chunk := make([]byte, srlCompressionChunkBytes)
  emptyReads := 0
  for {
    if err = srlCompressionPoll(options.CheckCancelled); err != nil { return nil, err }
    count, failure := decoder.Read(chunk)
    if count < 0 || count > len(chunk) { return nil, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid decoder progress") }
    if count != 0 {
      emptyReads = 0
      if _, err = output.Write(chunk[:count]); err != nil { return nil, err }
    } else if failure == nil {
      emptyReads++
      if emptyReads >= srlCompressionMaxEmptyReads { return nil, srlCompressionFailure(SerializerCompressionInvalidData, "Decoder made no progress") }
    }
    if failure == srlCompressionIO.EOF { break }
    if failure != nil { return nil, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid or truncated compressed stream") }
  }
  if source.Len() != 0 { return nil, srlCompressionFailure(SerializerCompressionInvalidData, "Trailing or concatenated compressed input") }
  // RFC 1952 stores the expanded length modulo 2^32 independently of the logical byte budget.
  if trailer != nil && (binary.LittleEndian.Uint32(trailer[:4]) != output.crc ||
      binary.LittleEndian.Uint32(trailer[4:]) != uint32(len(output.bytes))) {
    return nil, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid gzip checksum or length")
  }
  if err = decoder.Close(); err != nil { return nil, srlCompressionFailure(SerializerCompressionInvalidData, "Invalid compressed stream finish") }
  if err = srlCompressionPoll(options.CheckCancelled); err != nil { return nil, err }
  return output.bytes, nil
}

// SerializerCompressionEncode calls the existing native encoder after validating all compression options.
// Serialization itself retains its existing allocation contract; the result is bounded before compression.
func SerializerCompressionEncode(value interface{ Encode(Protocol) ([]byte, error) }, protocol Protocol,
    options SerializerCompressionEncodeOptions) ([]byte, error) {
  if err := srlCompressionEncodePolicy(options); err != nil { return nil, err }
  if err := srlCompressionProtocol(protocol); err != nil { return nil, err }
  if value == nil { return nil, srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing native encoder") }
  data, err := value.Encode(protocol)
  if err != nil { return nil, err }
  if err = srlCompressionPoll(options.CheckCancelled); err != nil { return nil, err }
  return SerializerCompressionCompress(data, options)
}
// SerializerCompressionDecode invokes a fresh native decoder and publishes its result only after final cancellation.
// Pass this module's Decode<Type> function; native errors retain their original identity and limits are copied.
func SerializerCompressionDecode[T any](data []byte, protocol Protocol,
    decode func([]byte, Protocol, ...Limits) (T, error), options SerializerCompressionDecodeOptions) (T, error) {
  var zero T
  if err := srlCompressionDecodePolicy(options); err != nil { return zero, err }
  if err := srlCompressionProtocol(protocol); err != nil { return zero, err }
  if decode == nil { return zero, srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing native decoder") }
  if int64(options.Limits.MaxBytes) < options.MaxOutputBytes { options.MaxOutputBytes = int64(options.Limits.MaxBytes) }
  plain, err := SerializerCompressionDecompress(data, options)
  if err != nil { return zero, err }
  result, err := decode(plain, protocol, options.Limits)
  if err != nil { return zero, err }
  if err = srlCompressionPoll(options.CheckCancelled); err != nil { return zero, err }
  return result, nil
}

)serializer"
R"serializer(
// SerializerCompressionOutput borrows a sink and stages one bounded message until explicit Finish.
// A failed wrapper retains its first error; external writes can leave a prefix and the sink is never closed.
type SerializerCompressionOutput struct {
  sink srlCompressionIO.Writer
  options SerializerCompressionEncodeOptions
  bytes []byte
  err error
  finished bool
  active bool
}
// SerializerCompressionNewOutput validates policy/provider availability before any native callback or sink I/O.
func SerializerCompressionNewOutput(sink srlCompressionIO.Writer, options SerializerCompressionEncodeOptions) (*SerializerCompressionOutput, error) {
  if err := srlCompressionEncodePolicy(options); err != nil { return nil, err }
  if sink == nil { return nil, srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing compression sink") }
  return &SerializerCompressionOutput{sink: sink, options: options}, nil
}
// fail fences later operations with the original failure.
func (output *SerializerCompressionOutput) fail(err error) error {
  if output.err == nil { output.err = err }; return output.err
}
// begin rejects callback reentry and fences both the nested and active outer operation.
func (output *SerializerCompressionOutput) begin() error {
  if output.err != nil { return output.err }
  if output.active { return output.fail(srlCompressionFailure(SerializerCompressionInvalidOptions, "Reentrant compression output operation")) }
  output.active = true
  return nil
}
// poll observes failure installed by a nested callback even when that callback returns no error.
func (output *SerializerCompressionOutput) poll() error {
  if output.err != nil { return output.err }
  if err := srlCompressionPoll(output.options.CheckCancelled); err != nil { return output.fail(err) }
  return output.err
}
// Write copies accepted plaintext; no compressed bytes reach the sink before Finish.
func (output *SerializerCompressionOutput) Write(data []byte) (int, error) {
  if err := output.begin(); err != nil { return 0, err }
  defer func() { output.active = false }()
  return output.write(data)
}
// write appends under an already admitted operation, including a native Encode operation.
func (output *SerializerCompressionOutput) write(data []byte) (int, error) {
  if output.err != nil { return 0, output.err }
  if output.finished { return 0, output.fail(srlCompressionIO.ErrClosedPipe) }
  if output.sink == nil { return 0, output.fail(srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing compression sink")) }
  if err := output.poll(); err != nil { return 0, err }
  if int64(len(data)) > output.options.MaxInputBytes - int64(len(output.bytes)) {
    return 0, output.fail(srlCompressionFailure(SerializerCompressionResourceLimit, "Compression input exceeds its byte budget"))
  }
  accepted := len(data)
  for len(data) != 0 {
    if err := output.poll(); err != nil { return 0, err }
    count := len(data); if count > srlCompressionChunkBytes { count = srlCompressionChunkBytes }
    output.bytes = append(output.bytes, data[:count]...); data = data[count:]
  }
  if err := output.poll(); err != nil { return 0, err }
  return accepted, nil
}
// Encode stages bytes from an existing native model without changing its direct Encode behavior.
func (output *SerializerCompressionOutput) Encode(value interface{ Encode(Protocol) ([]byte, error) }, protocol Protocol) error {
  if err := output.begin(); err != nil { return err }
  defer func() { output.active = false }()
  if output.finished { return output.fail(srlCompressionIO.ErrClosedPipe) }
  if err := srlCompressionProtocol(protocol); err != nil { return output.fail(err) }
  if value == nil { return output.fail(srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing native encoder")) }
  if err := output.poll(); err != nil { return err }
  data, err := value.Encode(protocol)
  if err != nil { return output.fail(err) }
  if err = output.poll(); err != nil { return err }
  _, err = output.write(data); return err
}
// Finish validates the whole frame, then writes bounded chunks and checks cancellation after the final sink call.
// Success is idempotent; short writes obey io.Writer's ErrShortWrite contract and fence the wrapper.
func (output *SerializerCompressionOutput) Finish() error {
  if err := output.begin(); err != nil { return err }
  defer func() { output.active = false }()
  if output.finished { return nil }
  if output.sink == nil { return output.fail(srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing compression sink")) }
  options := output.options
  options.CheckCancelled = output.poll
  packed, err := SerializerCompressionCompress(output.bytes, options)
  if err != nil { return output.fail(err) }
  for len(packed) != 0 {
    if err = output.poll(); err != nil { return err }
    count := len(packed); if count > srlCompressionChunkBytes { count = srlCompressionChunkBytes }
    written, failure := output.sink.Write(packed[:count])
    if output.err != nil { return output.err }
    if written < 0 || written > count { return output.fail(fmt.Errorf("Invalid compression sink write count")) }
    if failure != nil { return output.fail(failure) }
    if written != count { return output.fail(srlCompressionIO.ErrShortWrite) }
    packed = packed[count:]
  }
  if err = output.poll(); err != nil { return err }
  output.finished = true
  output.bytes = nil
  return nil
}

// SerializerCompressionInput borrows an EOF-delimited source and privately validates one complete frame.
// Read exposes bounded plaintext batches; fresh native decoding remains transactional through DecodeFrom.
type SerializerCompressionInput struct {
  source srlCompressionIO.Reader
  options SerializerCompressionDecodeOptions
  bytes []byte
  position int
  loaded bool
  err error
  active bool
}
// SerializerCompressionNewInput validates options and provider availability before reading the borrowed source.
func SerializerCompressionNewInput(source srlCompressionIO.Reader, options SerializerCompressionDecodeOptions) (*SerializerCompressionInput, error) {
  if err := srlCompressionDecodePolicy(options); err != nil { return nil, err }
  if source == nil { return nil, srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing compression source") }
  return &SerializerCompressionInput{source: source, options: options}, nil
}
// fail retains the first source, framing, policy or native callback failure.
func (input *SerializerCompressionInput) fail(err error) error {
  if input.err == nil { input.err = err }; return input.err
}
// begin fences nested public operations without permitting another source read or native publication.
func (input *SerializerCompressionInput) begin() error {
  if input.err != nil { return input.err }
  if input.active { return input.fail(srlCompressionFailure(SerializerCompressionInvalidOptions, "Reentrant compression input operation")) }
  input.active = true
  return nil
}
// poll retains a nested callback failure even when the outer cancellation hook ignores it.
func (input *SerializerCompressionInput) poll() error {
  if input.err != nil { return input.err }
  if err := srlCompressionPoll(input.options.CheckCancelled); err != nil { return input.fail(err) }
  return input.err
}
// load reads bounded compressed bytes through EOF and rejects zero-progress sources without looping forever.
func (input *SerializerCompressionInput) load() error {
  if input.err != nil { return input.err }
  if input.source == nil { return input.fail(srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing compression source")) }
  if err := input.poll(); err != nil { return err }
  if input.loaded { return nil }
  packed := make([]byte, 0)
  chunk := make([]byte, srlCompressionChunkBytes)
  emptyReads := 0
  for {
    if err := input.poll(); err != nil { return err }
    remaining := input.options.MaxInputBytes - int64(len(packed))
    count := len(chunk)
    // One bounded lookahead byte distinguishes an exact budget from an oversized source.
    if remaining < int64(count) { count = int(remaining) + 1 }
    read, failure := input.source.Read(chunk[:count])
    if read < 0 || read > count { return input.fail(fmt.Errorf("Invalid compression source read count")) }
    if failure != nil && failure != srlCompressionIO.EOF { return input.fail(failure) }
    if err := input.poll(); err != nil { return err }
    if int64(read) > remaining { return input.fail(srlCompressionFailure(SerializerCompressionResourceLimit, "Compressed input exceeds its byte budget")) }
    packed = append(packed, chunk[:read]...)
    if failure == srlCompressionIO.EOF { break }
    if read == 0 {
      emptyReads++
      if emptyReads >= srlCompressionMaxEmptyReads { return input.fail(srlCompressionIO.ErrNoProgress) }
    } else { emptyReads = 0 }
  }
  options := input.options
  options.CheckCancelled = input.poll
  plain, err := SerializerCompressionDecompress(packed, options)
  if err != nil { return input.fail(err) }
  if err = input.poll(); err != nil { return err }
  input.bytes = plain
  input.loaded = true
  return nil
}
// Read returns at most one bounded plaintext batch and polls cancellation on later reads and at EOF.
func (input *SerializerCompressionInput) Read(destination []byte) (int, error) {
  if err := input.begin(); err != nil { return 0, err }
  defer func() { input.active = false }()
  if err := input.poll(); err != nil { return 0, err }
  if len(destination) == 0 { return 0, nil }
  if err := input.load(); err != nil { return 0, err }
  if input.position == len(input.bytes) { return 0, srlCompressionIO.EOF }
  count := len(destination)
  if count > srlCompressionChunkBytes { count = srlCompressionChunkBytes }
  if count > len(input.bytes) - input.position { count = len(input.bytes) - input.position }
  copy(destination[:count], input.bytes[input.position:input.position+count]); input.position += count
  if err := input.poll(); err != nil { return count, err }
  return count, nil
}
// Finish requires all validated plaintext to be consumed and observes late cancellation without closing the source.
func (input *SerializerCompressionInput) Finish() error {
  if err := input.begin(); err != nil { return err }
  defer func() { input.active = false }()
  return input.finish()
}
// finish verifies consumption inside an already admitted public Finish or typed DecodeFrom operation.
func (input *SerializerCompressionInput) finish() error {
  if err := input.load(); err != nil { return err }
  if input.position != len(input.bytes) { return input.fail(srlCompressionFailure(SerializerCompressionInvalidData, "Unread decompressed input")) }
  if err := input.poll(); err != nil { return err }
  return nil
}
// SerializerCompressionDecodeFrom validates native selection before source I/O and returns a fresh candidate only.
// Pass this module's Decode<Type> function; callbacks must preserve that fresh-value contract.
func SerializerCompressionDecodeFrom[T any](input *SerializerCompressionInput, protocol Protocol,
    decode func([]byte, Protocol, ...Limits) (T, error)) (T, error) {
  var zero T
  if input == nil { return zero, srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing compression input") }
  if err := input.begin(); err != nil { return zero, err }
  defer func() { input.active = false }()
  if err := srlCompressionProtocol(protocol); err != nil { return zero, input.fail(err) }
  if decode == nil { return zero, input.fail(srlCompressionFailure(SerializerCompressionInvalidOptions, "Missing native decoder")) }
  if int64(input.options.Limits.MaxBytes) < input.options.MaxOutputBytes {
    input.options.MaxOutputBytes = int64(input.options.Limits.MaxBytes)
  }
  if err := input.load(); err != nil { return zero, err }
  data := input.bytes[input.position:]
  input.position = len(input.bytes)
  result, err := decode(data, protocol, input.options.Limits)
  if err != nil { return zero, input.fail(err) }
  if err = input.poll(); err != nil { return zero, err }
  if err = input.finish(); err != nil { return zero, err }
  return result, nil
}
)serializer"
