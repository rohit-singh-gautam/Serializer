// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compresscs(
  /// <summary>Bounded whole-message compression; streams are borrowed and synchronous.</summary>
  public static class SerializerCompression {
    public enum Format { None, Gzip, Zlib, Deflate, Zstd, Lz4 }
    public enum Code { Unavailable, InvalidOptions, InvalidData, ResourceLimit }
    /// <summary>Compression failures contain a stable category and no payload excerpts.</summary>
    public sealed class Error : global::System.Exception {
      public Code Code { get; }
      public Error(Code code, string message) : base(message) { Code = code; }
    }
    public const int DefaultMaxBytes = 64 * 1024 * 1024;
    public const int DefaultMaxWindowBytes = 8 * 1024 * 1024;
    private const int ChunkBytes = 32 * 1024;
    private const int DeflateWindowBytes = 32 * 1024;
    /// <summary>Immutable byte limits; existing BCL selectors and explicit optional-provider levels remain separate.</summary>
    public sealed class EncodeOptions {
      public Format Format { get; }
      public int MaxInputBytes { get; }
      public int MaxOutputBytes { get; }
      public global::System.IO.Compression.CompressionLevel Level { get; }
      public global::System.Action? CheckCancelled { get; }
      public Provider? Provider { get; }
      public int ProviderLevel { get; }
      public EncodeOptions(Format format = Format.None, int maxInputBytes = DefaultMaxBytes,
          int maxOutputBytes = DefaultMaxBytes,
          global::System.IO.Compression.CompressionLevel level = global::System.IO.Compression.CompressionLevel.Optimal,
          global::System.Action? checkCancelled = null) {
        Format = format; MaxInputBytes = maxInputBytes; MaxOutputBytes = maxOutputBytes;
        Level = level; CheckCancelled = checkCancelled;
      }

      /// <summary>Select one optional managed provider with its native integer compression level.</summary>
      public static EncodeOptions WithProvider(Format format, Provider provider, int maxInputBytes = DefaultMaxBytes,
          int maxOutputBytes = DefaultMaxBytes, int? level = null, global::System.Action? checkCancelled = null)
          => new EncodeOptions(format, provider, maxInputBytes, maxOutputBytes, level, checkCancelled);
      // Keep the original public constructor's overload resolution, including untyped default arguments.
      private EncodeOptions(Format format, Provider provider, int maxInputBytes,
          int maxOutputBytes, int? level, global::System.Action? checkCancelled)
          : this(format, maxInputBytes, maxOutputBytes, global::System.IO.Compression.CompressionLevel.Optimal, checkCancelled) {
        Provider = provider ?? throw new Error(Code.Unavailable, "Optional provider is absent");
        ProviderLevel = level ?? (format == Format.Zstd ? 3 : 0);
      }
      // Preserve optional-provider selection while routing wrapper polling through its activity guard.
      internal EncodeOptions WithCheck(global::System.Action check) {
        return Provider == null ? new EncodeOptions(Format, MaxInputBytes, MaxOutputBytes, Level, check) :
            new EncodeOptions(Format, Provider, MaxInputBytes, MaxOutputBytes, ProviderLevel, check);
      }
    }
    /// <summary>Immutable compressed/expanded limits; cancellation callbacks may throw unchanged.</summary>
    public sealed class DecodeOptions {
      public Format Format { get; }
      public int MaxInputBytes { get; }
      public int MaxOutputBytes { get; }
      public int MaxWindowBytes { get; }
      public global::System.Action? CheckCancelled { get; }
      public Provider? Provider { get; }
      public DecodeOptions(Format format = Format.None, int maxInputBytes = DefaultMaxBytes,
          int maxOutputBytes = DefaultMaxBytes, int maxWindowBytes = DefaultMaxWindowBytes,
          global::System.Action? checkCancelled = null) {
        Format = format; MaxInputBytes = maxInputBytes; MaxOutputBytes = maxOutputBytes;
        MaxWindowBytes = maxWindowBytes; CheckCancelled = checkCancelled;
      }

      /// <summary>Select one optional managed provider without changing compressed/expanded/history limits.</summary>
      public static DecodeOptions WithProvider(Format format, Provider provider, int maxInputBytes = DefaultMaxBytes,
          int maxOutputBytes = DefaultMaxBytes, int maxWindowBytes = DefaultMaxWindowBytes,
          global::System.Action? checkCancelled = null)
          => new DecodeOptions(format, provider, maxInputBytes, maxOutputBytes, maxWindowBytes, checkCancelled);
      // Require every private argument so no previously valid public constructor call becomes ambiguous.
      private DecodeOptions(Format format, Provider provider, int maxInputBytes,
          int maxOutputBytes, int maxWindowBytes, global::System.Action? checkCancelled)
          : this(format, maxInputBytes, maxOutputBytes, maxWindowBytes, checkCancelled) {
        Provider = provider ?? throw new Error(Code.Unavailable, "Optional provider is absent");
      }
      // Preserve provider capability identity in the private wrapper-aware policy snapshot.
      internal DecodeOptions WithCheck(global::System.Action check) {
        return Provider == null ? new DecodeOptions(Format, MaxInputBytes, MaxOutputBytes, MaxWindowBytes, check) :
            new DecodeOptions(Format, Provider, MaxInputBytes, MaxOutputBytes, MaxWindowBytes, check);
      }
    }
    // Reject unknown selectors independently of provider availability.
    private static void CheckFormat(Format format) {
      if (format < Format.None || format > Format.Lz4) throw new Error(Code.InvalidOptions, "Unknown compression format");
    }
    /// <summary>Report built-in availability without loading optional providers.</summary>
    public static bool Available(Format format) {
      CheckFormat(format);
      return format != Format.Zstd && format != Format.Lz4;
    }
    // Admit policy before application callbacks, transport calls or native provider construction.
    private static EncodeOptions Admit(EncodeOptions? options) {
      var value = options ?? new EncodeOptions();
      CheckFormat(value.Format);
      if (value.MaxInputBytes < 0 || value.MaxOutputBytes < 0 ||
          (value.Level != global::System.IO.Compression.CompressionLevel.Optimal &&
           value.Level != global::System.IO.Compression.CompressionLevel.Fastest &&
           value.Level != global::System.IO.Compression.CompressionLevel.NoCompression &&
           value.Level != global::System.IO.Compression.CompressionLevel.SmallestSize))
        throw new Error(Code.InvalidOptions, "Invalid compression options");
      value.Provider?.Admit(value.Format, value.ProviderLevel);
      if (!Available(value.Format, value.Provider)) throw new Error(Code.Unavailable, "Compression provider is unavailable");
      value.CheckCancelled?.Invoke(); return value;
    }
    // A nonnegative inadequate history budget is a resource failure, including before source I/O.
    private static DecodeOptions Admit(DecodeOptions? options) {
      var value = options ?? new DecodeOptions();
      CheckFormat(value.Format);
      if (value.MaxInputBytes < 0 || value.MaxOutputBytes < 0 || value.MaxWindowBytes < 0)
        throw new Error(Code.InvalidOptions, "Invalid decompression options");
      value.Provider?.Admit(value.Format, null);
      if (!Available(value.Format, value.Provider)) throw new Error(Code.Unavailable, "Compression provider is unavailable");
      int minimumWindow = value.Format == Format.Zstd ? 1024 : value.Format == Format.Lz4 ? 65536 : DeflateWindowBytes;
      if (value.Format != Format.None && value.MaxWindowBytes < minimumWindow)
        throw new Error(Code.ResourceLimit, value.Format == Format.Zstd || value.Format == Format.Lz4 ? "Compression history budget is insufficient" : "DEFLATE requires a 32 KiB history budget");
      value.CheckCancelled?.Invoke(); return value;
    }
    // Bound each append before MemoryStream capacity growth; Abort fences provider cleanup writes.
    private sealed class BoundedOutput : global::System.IO.Stream {
      private readonly global::System.IO.MemoryStream data = new global::System.IO.MemoryStream();
      private readonly int maximum;
      private readonly global::System.Action? check;
      private bool failed;
      internal BoundedOutput(int maximum, global::System.Action? check) { this.maximum = maximum; this.check = check; }
      internal void Abort() { failed = true; }
      internal byte[] Bytes() { return data.ToArray(); }
      public override void Write(byte[] buffer, int offset, int count) { Write(new global::System.ReadOnlySpan<byte>(buffer, offset, count)); }
      public override void Write(global::System.ReadOnlySpan<byte> value) {
        if (failed) throw new global::System.InvalidOperationException("Compression output is failed");
        try {
          check?.Invoke();
          if (value.Length > maximum - data.Length) throw new Error(Code.ResourceLimit, "Compressed output limit exceeded");
          for (int offset = 0; offset < value.Length; offset += ChunkBytes) {
            check?.Invoke(); data.Write(value.Slice(offset, global::System.Math.Min(ChunkBytes, value.Length - offset)));
          }
          check?.Invoke();
        } catch { failed = true; throw; }
      }
      protected override void Dispose(bool disposing) { if (disposing) data.Dispose(); base.Dispose(disposing); }
      public override bool CanRead => false;
      public override bool CanSeek => false;
      public override bool CanWrite => !failed;
      public override long Length => data.Length;
      public override long Position { get => data.Position; set => throw new global::System.NotSupportedException(); }
      public override void Flush() { check?.Invoke(); }
      public override int Read(byte[] buffer, int offset, int count) => throw new global::System.NotSupportedException();
      public override long Seek(long offset, global::System.IO.SeekOrigin origin) => throw new global::System.NotSupportedException();
      public override void SetLength(long value) => throw new global::System.NotSupportedException();
    }
    // Generate one standard frame with a bounded buffered raw provider and explicit wrapper checksums.
    public static byte[] Compress(byte[] input, EncodeOptions? options = null) {
      var policy = Admit(options);
      if (input == null) throw new Error(Code.InvalidOptions, "Compression input is null");
      if (input.Length > policy.MaxInputBytes) throw new Error(Code.ResourceLimit, "Serialized input limit exceeded");
      if (policy.Format == Format.None) return Copy(input, policy.MaxOutputBytes, policy.CheckCancelled);
      if (policy.Format == Format.Zstd) return ZstdCompress(input, policy);
      if (policy.Format == Format.Lz4) return Lz4Compress(input, policy);
      using var output = new BoundedOutput(policy.MaxOutputBytes, policy.CheckCancelled);
      if (policy.Format == Format.Gzip) output.Write(new byte[]{31,139,8,0,0,0,0,0,0,255});
      else if (policy.Format == Format.Zlib) {
        byte flags = policy.Level == global::System.IO.Compression.CompressionLevel.SmallestSize ? (byte)218 :
            policy.Level == global::System.IO.Compression.CompressionLevel.Optimal ? (byte)156 : (byte)1;
        output.Write(new byte[]{120, flags});
      }
      var provider = new global::System.IO.Compression.DeflateStream(output, policy.Level, true);
      try {
        // Explicit empty Write makes .NET 8 emit the complete empty raw stream at disposal.
        if (input.Length == 0) provider.Write(global::System.ReadOnlySpan<byte>.Empty);
        for (int offset = 0; offset < input.Length; offset += ChunkBytes) {
          policy.CheckCancelled?.Invoke();
          provider.Write(new global::System.ReadOnlySpan<byte>(input, offset, global::System.Math.Min(ChunkBytes, input.Length - offset)));
          policy.CheckCancelled?.Invoke();
        }
        provider.Dispose();
      } catch {
        output.Abort();
        try { provider.Dispose(); } catch { }
        throw;
      }
      if (policy.Format == Format.Gzip) {
        byte[] trailer = new byte[8];
        global::System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(trailer, Crc(input, 0, input.Length, policy.CheckCancelled));
        global::System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(new global::System.Span<byte>(trailer, 4, 4), (uint)input.Length);
        output.Write(trailer);
      } else if (policy.Format == Format.Zlib) {
        byte[] trailer = new byte[4];
        global::System.Buffers.Binary.BinaryPrimitives.WriteUInt32BigEndian(trailer, Adler(input, policy.CheckCancelled));
        output.Write(trailer);
      }
      policy.CheckCancelled?.Invoke(); return output.Bytes();
    }
    // Copy bounded chunks so identity operations retain the same cancellation and owning semantics.
    private static byte[] Copy(byte[] input, int maximum, global::System.Action? check) {
      if (input.Length > maximum) throw new Error(Code.ResourceLimit, "Output limit exceeded");
      byte[] result = new byte[input.Length];
      for (int offset = 0; offset < input.Length; offset += ChunkBytes) {
        check?.Invoke(); int size = global::System.Math.Min(ChunkBytes, input.Length - offset);
        global::System.Array.Copy(input, offset, result, offset, size);
      }
      check?.Invoke(); return result;
    }
    private static readonly uint[] CrcTable = MakeCrcTable();
    // Build the standard reflected CRC-32 table once, independently of optional packages.
    private static uint[] MakeCrcTable() {
      var result = new uint[256];
      for (uint index = 0; index < result.Length; ++index) {
        uint value = index;
        for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) != 0 ? 0xedb88320U : 0U);
        result[index] = value;
      }
      return result;
    }
    // Validate gzip data and optional header CRCs with bounded cancellation polling.
)compresscs"
R"compresscs(    private static uint Crc(byte[] data, int offset, int count, global::System.Action? check) {
      uint result = uint.MaxValue;
      for (int index = 0; index < count; ++index) {
        if ((index & (ChunkBytes - 1)) == 0) check?.Invoke();
        result = CrcTable[(result ^ data[offset + index]) & 255] ^ (result >> 8);
      }
      check?.Invoke(); return ~result;
    }
    // Reduce within Adler-32's overflow-safe batch size, retaining all unsigned checksum bits.
    private static uint Adler(byte[] data, global::System.Action? check) {
      const int safeBatchBytes = 5552;
      const uint modulus = 65521;
      uint first = 1, second = 0;
      for (int offset = 0; offset < data.Length; offset += safeBatchBytes) {
        check?.Invoke(); int end = offset + global::System.Math.Min(safeBatchBytes, data.Length - offset);
        for (int index = offset; index < end; ++index) { first += data[index]; second += first; }
        first %= modulus; second %= modulus;
      }
      check?.Invoke(); return (second << 16) | first;
    }

    // Read only required bits from staged memory; Position is the exact consumed byte extent.
    private sealed class Bits {
      private readonly byte[] data;
      private readonly global::System.Action? check;
      private uint pending;
      private int available;
      private int work;
      internal int Position;
      internal Bits(byte[] data, int position, global::System.Action? check) { this.data = data; Position = position; this.check = check; }
      internal int Read(int count) {
        if ((++work & 4095) == 0) check?.Invoke();
        while (available < count) {
          if (Position == data.Length) throw new Error(Code.InvalidData, "Truncated DEFLATE bitstream");
          pending |= (uint)data[Position++] << available; available += 8;
        }
        int result = (int)(pending & ((1U << count) - 1));
        pending >>= count; available -= count; return result;
      }
      internal void Align() { pending = 0; available = 0; }
      internal void Skip(int count) {
        if (count > data.Length - Position) throw new Error(Code.InvalidData, "Truncated DEFLATE stored block");
        Position += count;
      }
    }
    // Canonical Huffman trees need only fixed small tables; no payload bytes or history are allocated.
    private sealed class Huffman {
      private readonly int[] counts = new int[16];
      private readonly int[] first = new int[16];
      private readonly int[] offsets = new int[16];
      private readonly int[] next = new int[16];
      private readonly int[] symbols = new int[288];
      private int maximum;
      internal void Build(int[] lengths, int start, int count, bool codeLengths = false, bool allowEmpty = false) {
        global::System.Array.Clear(counts); maximum = 0; int used = 0;
        for (int index = 0; index < count; ++index) {
          int length = lengths[start + index];
          if (length < 0 || length > 15) throw new Error(Code.InvalidData, "Invalid DEFLATE code length");
          if (length != 0) { ++counts[length]; ++used; maximum = global::System.Math.Max(maximum, length); }
        }
        if (used == 0) {
          if (allowEmpty) return;
          throw new Error(Code.InvalidData, "Empty DEFLATE Huffman tree");
        }
        int left = 1;
        for (int length = 1; length <= 15; ++length) {
          left = (left << 1) - counts[length];
          if (left < 0) throw new Error(Code.InvalidData, "Oversubscribed DEFLATE Huffman tree");
        }
        if (left != 0 && (codeLengths || used != 1 || maximum != 1))
          throw new Error(Code.InvalidData, "Incomplete DEFLATE Huffman tree");
        int code = 0, offset = 0;
        for (int length = 1; length <= 15; ++length) {
          code = (code + counts[length - 1]) << 1; first[length] = code;
          offsets[length] = offset; offset += counts[length];
        }
        global::System.Array.Copy(offsets, next, offsets.Length);
        for (int symbol = 0; symbol < count; ++symbol) {
          int length = lengths[start + symbol];
          if (length != 0) symbols[next[length]++] = symbol;
        }
      }
      internal int Decode(Bits bits) {
        int code = 0;
        for (int length = 1; length <= maximum; ++length) {
          code = (code << 1) | bits.Read(1);
          int index = code - first[length];
          if ((uint)index < (uint)counts[length]) return symbols[offsets[length] + index];
        }
        throw new Error(Code.InvalidData, "Invalid DEFLATE Huffman code");
      }
    }
    private static readonly int[] LengthBase = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    private static readonly int[] LengthExtra = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    private static readonly int[] DistanceBase = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    private static readonly int[] DistanceExtra = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    private static readonly int[] CodeOrder = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    private static readonly Huffman FixedLiteral = MakeFixed(false);
    private static readonly Huffman FixedDistance = MakeFixed(true);
    // RFC 1951 fixed alphabets are immutable and shared, avoiding per-block table allocation.
    private static Huffman MakeFixed(bool distance) {
      int[] lengths = new int[distance ? 32 : 288];
      for (int symbol = 0; symbol < lengths.Length; ++symbol)
        lengths[symbol] = distance ? 5 : symbol < 144 ? 8 : symbol < 256 ? 9 : symbol < 280 ? 7 : 8;
      var result = new Huffman(); result.Build(lengths, 0, lengths.Length); return result;
    }
    // Add only actual decoded lengths and reject before overflow or expanded output allocation.
    private static void AddOutput(ref int output, int count, int maximum) {
      if (count > maximum - output) throw new Error(Code.ResourceLimit, "Expanded output limit exceeded");
      output += count;
    }
    // Validate exact extent, dynamic trees, back-reference history and actual output count in memory.
    private static (int End, int Size) DeflateExtent(byte[] data, int start, int maximum, int window, global::System.Action? check) {
      var bits = new Bits(data, start, check);
      var literal = new Huffman(); var distance = new Huffman(); var codes = new Huffman();
      int[] lengths = new int[320], codeLengths = new int[19];
      int output = 0;
      bool final;
      do {
        check?.Invoke(); final = bits.Read(1) != 0; int kind = bits.Read(2);
        if (kind == 0) {
          bits.Align(); int size = bits.Read(16), complement = bits.Read(16);
          if ((size ^ 65535) != complement) throw new Error(Code.InvalidData, "Invalid DEFLATE stored length");
          bits.Skip(size); AddOutput(ref output, size, maximum); continue;
        }
        if (kind == 3) throw new Error(Code.InvalidData, "Reserved DEFLATE block type");
        Huffman selectedLiteral = FixedLiteral, selectedDistance = FixedDistance;
        if (kind == 2) {
          int literalCount = bits.Read(5) + 257, distanceCount = bits.Read(5) + 1, codeCount = bits.Read(4) + 4;
          if (literalCount > 286) throw new Error(Code.InvalidData, "Invalid DEFLATE literal alphabet");
          global::System.Array.Clear(codeLengths);
          for (int index = 0; index < codeCount; ++index) codeLengths[CodeOrder[index]] = bits.Read(3);
          codes.Build(codeLengths, 0, codeLengths.Length, true);
          int total = literalCount + distanceCount, position = 0;
          while (position < total) {
            int symbol = codes.Decode(bits);
            if (symbol < 16) lengths[position++] = symbol;
            else {
              int count = symbol == 16 ? bits.Read(2) + 3 : symbol == 17 ? bits.Read(3) + 3 : bits.Read(7) + 11;
              if (symbol == 16 && position == 0 || count > total - position)
                throw new Error(Code.InvalidData, "Invalid DEFLATE code-length repeat");
              int value = symbol == 16 ? lengths[position - 1] : 0;
              for (int index = 0; index < count; ++index) lengths[position++] = value;
            }
          }
          if (lengths[256] == 0) throw new Error(Code.InvalidData, "DEFLATE end-of-block code is absent");
          literal.Build(lengths, 0, literalCount); distance.Build(lengths, literalCount, distanceCount, false, true);
          selectedLiteral = literal; selectedDistance = distance;
        }
        while (true) {
          int symbol = selectedLiteral.Decode(bits);
          if (symbol < 256) { AddOutput(ref output, 1, maximum); continue; }
          if (symbol == 256) break;
          int lengthCode = symbol - 257;
          if ((uint)lengthCode >= (uint)LengthBase.Length) throw new Error(Code.InvalidData, "Reserved DEFLATE length code");
          int count = LengthBase[lengthCode] + bits.Read(LengthExtra[lengthCode]);
          int distanceCode = selectedDistance.Decode(bits);
          if ((uint)distanceCode >= (uint)DistanceBase.Length) throw new Error(Code.InvalidData, "Reserved DEFLATE distance code");
          int back = DistanceBase[distanceCode] + bits.Read(DistanceExtra[distanceCode]);
          if (back > output || back > window) throw new Error(Code.InvalidData, "DEFLATE distance exceeds available history");
          AddOutput(ref output, count, maximum);
        }
      } while (!final);
      check?.Invoke(); return (bits.Position, output);
    }

    // Check each wrapper span before indexing or subtracting its length.
    private static void Require(byte[] data, int position, int count) {
      if (position < 0 || count < 0 || position > data.Length || count > data.Length - position)
        throw new Error(Code.InvalidData, "Truncated compression framing");
    }
    // Validate optional gzip fields and FHCRC without normalizing opaque header bytes.
    private static int GzipStart(byte[] data, global::System.Action? check) {
      Require(data, 0, 10);
      if (data[0] != 31 || data[1] != 139 || data[2] != 8 || (data[3] & 224) != 0)
        throw new Error(Code.InvalidData, "Invalid gzip header");
      int flags = data[3], position = 10;
      if ((flags & 4) != 0) {
        Require(data, position, 2);
        int size = data[position] | (data[position + 1] << 8); position += 2;
        Require(data, position, size); position += size;
      }
      foreach (int flag in new[]{8,16}) {
        if ((flags & flag) == 0) continue;
        while (true) {
          if ((position & (ChunkBytes - 1)) == 0) check?.Invoke();
          Require(data, position, 1); if (data[position++] == 0) break;
        }
      }
      if ((flags & 2) != 0) {
        Require(data, position, 2);
        if ((ushort)Crc(data, 0, position, check) != (data[position] | data[position + 1] << 8))
          throw new Error(Code.InvalidData, "Invalid gzip header checksum");
        position += 2;
      }
      check?.Invoke(); return position;
    }
    // Feed the BCL bounded memory batches; provider read-ahead cannot define the frame extent.
    private sealed class CallbackCheck {
      private readonly global::System.Action? callback;
      internal global::System.Exception? Failure;
      internal CallbackCheck(global::System.Action? callback) { this.callback = callback; }
      // Preserve even an InvalidDataException thrown intentionally by application cancellation.
      internal void Check() {
        try { callback?.Invoke(); } catch (global::System.Exception failure) { Failure = failure; throw; }
      }
    }
    // Feed the BCL bounded memory batches; provider read-ahead cannot define the frame extent.
    private sealed class ProviderInput : global::System.IO.Stream {
      private readonly byte[] data;
      private readonly int end;
      private readonly global::System.Action? check;
      private int position;
      internal ProviderInput(byte[] data, int start, int end, global::System.Action? check) { this.data = data; position = start; this.end = end; this.check = check; }
      public override int Read(byte[] buffer, int offset, int count) {
        check?.Invoke(); int size = global::System.Math.Min(global::System.Math.Min(count, ChunkBytes), end - position);
        global::System.Array.Copy(data, position, buffer, offset, size); position += size; check?.Invoke(); return size;
      }
      public override bool CanRead => true;
      public override bool CanSeek => false;
      public override bool CanWrite => false;
      public override long Length => throw new global::System.NotSupportedException();
      public override long Position { get => throw new global::System.NotSupportedException(); set => throw new global::System.NotSupportedException(); }
      public override void Flush() => throw new global::System.NotSupportedException();
      public override long Seek(long offset, global::System.IO.SeekOrigin origin) => throw new global::System.NotSupportedException();
      public override void SetLength(long value) => throw new global::System.NotSupportedException();
      public override void Write(byte[] buffer, int offset, int count) => throw new global::System.NotSupportedException();
    }
    /// <summary>Validate exactly one selected frame before allocating and publishing expanded bytes.</summary>
    public static byte[] Decompress(byte[] input, DecodeOptions? options = null) {
      var policy = Admit(options);
      if (input == null) throw new Error(Code.InvalidOptions, "Decompression input is null");
      if (input.Length > policy.MaxInputBytes) throw new Error(Code.ResourceLimit, "Compressed input limit exceeded");
      if (policy.Format == Format.None) return Copy(input, policy.MaxOutputBytes, policy.CheckCancelled);
      if (policy.Format == Format.Zstd) return ZstdDecompress(input, policy);
      if (policy.Format == Format.Lz4) return Lz4Decompress(input, policy);
)compresscs"
R"compresscs(      int start = 0, trailer = 0, window = DeflateWindowBytes;
      if (policy.Format == Format.Gzip) { start = GzipStart(input, policy.CheckCancelled); trailer = 8; }
      else if (policy.Format == Format.Zlib) {
        Require(input, 0, 2);
        if ((input[0] & 15) != 8 || (input[0] >> 4) > 7 || ((input[0] << 8) | input[1]) % 31 != 0 || (input[1] & 32) != 0)
          throw new Error(Code.InvalidData, "Invalid or dictionary-dependent zlib header");
        window = 1 << ((input[0] >> 4) + 8); start = 2; trailer = 4;
      }
      var extent = DeflateExtent(input, start, policy.MaxOutputBytes, window, policy.CheckCancelled);
      Require(input, extent.End, trailer);
      if (input.Length - extent.End != trailer) throw new Error(Code.InvalidData, "Trailing or concatenated compression data");
      // The extent validator derives actual expanded count from the complete grammar, never a content-size claim.
      policy.CheckCancelled?.Invoke(); byte[] output = new byte[extent.Size], probe = new byte[1];
      var cancellation = new CallbackCheck(policy.CheckCancelled);
      using var source = new ProviderInput(input, start, extent.End, cancellation.Check);
      using var provider = new global::System.IO.Compression.DeflateStream(source, global::System.IO.Compression.CompressionMode.Decompress, true);
      try {
        int position = 0;
        while (position < output.Length) {
          cancellation.Check();
          int count = provider.Read(output, position, global::System.Math.Min(ChunkBytes, output.Length - position));
          if (count == 0) throw new Error(Code.InvalidData, "Truncated expanded data");
          position += count;
        }
        if (provider.Read(probe, 0, probe.Length) != 0) throw new Error(Code.InvalidData, "Unexpected expanded data");
      } catch (global::System.IO.InvalidDataException failure) when (!global::System.Object.ReferenceEquals(failure, cancellation.Failure)) {
        throw new Error(Code.InvalidData, "Invalid DEFLATE data");
      }
      if (policy.Format == Format.Gzip) {
        uint checksum = global::System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(new global::System.ReadOnlySpan<byte>(input, extent.End, 4));
        uint size = global::System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(new global::System.ReadOnlySpan<byte>(input, extent.End + 4, 4));
        if (size != (uint)output.Length || checksum != Crc(output, 0, output.Length, policy.CheckCancelled))
          throw new Error(Code.InvalidData, "Invalid gzip trailer");
      } else if (policy.Format == Format.Zlib) {
        uint checksum = global::System.Buffers.Binary.BinaryPrimitives.ReadUInt32BigEndian(new global::System.ReadOnlySpan<byte>(input, extent.End, 4));
        if (checksum != Adler(output, policy.CheckCancelled)) throw new Error(Code.InvalidData, "Invalid zlib checksum");
      }
      policy.CheckCancelled?.Invoke(); return output;
    }
    // Read to borrowed EOF with bounded requests, including a one-byte overflow probe.
    public static byte[] Decompress(global::System.IO.Stream source, DecodeOptions? options = null) {
      var policy = Admit(options);
      if (source == null || !source.CanRead) throw new Error(Code.InvalidOptions, "Decompression source is not readable");
      using var staged = new global::System.IO.MemoryStream();
      byte[] buffer = new byte[ChunkBytes];
      while (true) {
        policy.CheckCancelled?.Invoke();
        int requested = (int)global::System.Math.Min(ChunkBytes, (long)policy.MaxInputBytes - staged.Length + 1);
        int count = source.Read(buffer, 0, requested);
        if (count < 0 || count > requested) throw new global::System.IO.IOException("Invalid borrowed source read count");
        policy.CheckCancelled?.Invoke();
        if (count == 0) break;
        if (count > policy.MaxInputBytes - staged.Length) throw new Error(Code.ResourceLimit, "Compressed input limit exceeded");
        staged.Write(buffer, 0, count);
      }
      return Decompress(staged.ToArray(), policy);
    }
    /// <summary>Admit compression before invoking a caller's complete owning encoder.</summary>
    public static byte[] Encode(global::System.Func<byte[]> encode, EncodeOptions? options = null) {
      var policy = Admit(options);
      if (encode == null) throw new Error(Code.InvalidOptions, "Encoder callback is null");
      byte[] value = encode(); policy.CheckCancelled?.Invoke(); return Compress(value, policy);
    }
    /// <summary>Return a fresh candidate only after frame checks, decoding and final cancellation.</summary>
    public static T Decode<T>(byte[] input, global::System.Func<byte[], T> decode, DecodeOptions? options = null) {
      var policy = Admit(options);
      if (decode == null) throw new Error(Code.InvalidOptions, "Decoder callback is null");
      byte[] value = Decompress(input, policy); T candidate = decode(value);
      policy.CheckCancelled?.Invoke(); return candidate;
    }

    /// <summary>Stage one message; Finish is required. Dispose neither finishes nor closes the borrowed sink.</summary>
    public sealed class OutputStream : global::System.IO.Stream {
      private readonly global::System.IO.Stream sink;
      private readonly EncodeOptions policy;
      private readonly EncodeOptions runPolicy;
      private readonly global::System.IO.MemoryStream staged = new global::System.IO.MemoryStream();
      private bool failed, finished, closed, busy;
      private global::System.Exception? failure;
      public OutputStream(global::System.IO.Stream sink, EncodeOptions? options = null) {
        policy = Admit(options);
        if (sink == null || !sink.CanWrite) throw new Error(Code.InvalidOptions, "Compression sink is not writable");
        this.sink = sink;
        runPolicy = policy.WithCheck(Poll);
      }
      // A swallowed nested-operation failure still fences its active outer operation.
      private void Begin() {
        if (failed || finished || closed || busy) {
          var cause = new global::System.InvalidOperationException("Compression output is busy, finished, closed or failed", failed ? failure : null);
          if (busy && !failed) { failed = true; failure = cause; }
          throw cause;
        }
        busy = true;
      }
      // Retain the first callback/transport failure rather than a later reentry or cleanup error.
      private void Fail(global::System.Exception cause) {
        if (!failed) { failed = true; failure = cause; }
        if (!global::System.Object.ReferenceEquals(failure, cause))
          global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure!).Throw();
      }
      // Callbacks can dispose/reenter synchronously, including when they return successfully.
      private void Active() {
        if (failed) global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure!).Throw();
        if (finished || closed) throw new global::System.InvalidOperationException("Compression output is finished or closed");
      }
      // A successful callback may still have closed or permanently fenced this wrapper.
      private void Poll() { Active(); policy.CheckCancelled?.Invoke(); Active(); }
      public override void Write(byte[] buffer, int offset, int count) { Write(new global::System.ReadOnlySpan<byte>(buffer, offset, count)); }
      public override void Write(global::System.ReadOnlySpan<byte> value) {
        Begin();
        try {
          Poll();
          if (value.Length > policy.MaxInputBytes - staged.Length) throw new Error(Code.ResourceLimit, "Serialized input limit exceeded");
          for (int offset = 0; offset < value.Length; offset += ChunkBytes) {
            Poll(); staged.Write(value.Slice(offset, global::System.Math.Min(ChunkBytes, value.Length - offset)));
          }
          Poll();
        } catch (global::System.Exception cause) { Fail(cause); throw; } finally { busy = false; }
      }
      /// <summary>Compress and synchronously publish bounded writes; a sink failure may retain a prefix.</summary>
      public void Finish() {
        Begin();
        try {
          Poll(); byte[] wire = Compress(staged.ToArray(), runPolicy); Poll();
          for (int offset = 0; offset < wire.Length; offset += ChunkBytes) {
            Poll(); sink.Write(wire, offset, global::System.Math.Min(ChunkBytes, wire.Length - offset));
            Poll();
          }
          Poll(); finished = true; staged.Dispose();
        } catch (global::System.Exception cause) { Fail(cause); throw; } finally { busy = false; }
      }
      public override void Flush() {
        Begin(); try { Poll(); } catch (global::System.Exception cause) { Fail(cause); throw; } finally { busy = false; }
      }
      protected override void Dispose(bool disposing) { closed = true; if (disposing) staged.Dispose(); base.Dispose(disposing); }
      public override bool CanRead => false;
      public override bool CanSeek => false;
      public override bool CanWrite => !failed && !finished && !closed;
      public override long Length => throw new global::System.NotSupportedException();
      public override long Position { get => throw new global::System.NotSupportedException(); set => throw new global::System.NotSupportedException(); }
      public override int Read(byte[] buffer, int offset, int count) => throw new global::System.NotSupportedException();
      public override long Seek(long offset, global::System.IO.SeekOrigin origin) => throw new global::System.NotSupportedException();
      public override void SetLength(long value) => throw new global::System.NotSupportedException();
    }
    /// <summary>Validate to borrowed source EOF before exposing bytes; Finish checks exact expanded consumption.</summary>
    public sealed class InputStream : global::System.IO.Stream {
      private readonly global::System.IO.Stream source;
      private readonly DecodeOptions policy;
      private readonly DecodeOptions runPolicy;
      private byte[]? staged;
      private int position;
      private bool failed, finished, closed, busy;
      private global::System.Exception? failure;
      public InputStream(global::System.IO.Stream source, DecodeOptions? options = null) {
        policy = Admit(options);
        if (source == null || !source.CanRead) throw new Error(Code.InvalidOptions, "Decompression source is not readable");
        this.source = source;
        runPolicy = policy.WithCheck(Poll);
      }
      // Reject nested operations before they can read the borrowed source or publish staged bytes.
      private void Begin() {
        if (failed || finished || closed || busy) {
          var cause = new global::System.InvalidOperationException("Compression input is busy, finished, closed or failed", failed ? failure : null);
          if (busy && !failed) { failed = true; failure = cause; }
          throw cause;
        }
        busy = true;
      }
      // Keep the first original exception when a nested failure is followed by a callback error.
      private void Fail(global::System.Exception cause) {
        if (!failed) { failed = true; failure = cause; }
        if (!global::System.Object.ReferenceEquals(failure, cause))
          global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure!).Throw();
      }
      // Check both sides of synchronous policy callbacks and every provider/transport boundary.
      private void Active() {
        if (failed) global::System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure!).Throw();
        if (finished || closed) throw new global::System.InvalidOperationException("Compression input is finished or closed");
      }
      // Observe callback-driven disposal/reentry before another read or candidate publication.
      private void Poll() { Active(); policy.CheckCancelled?.Invoke(); Active(); }
      // Retain the original immutable policy and observe cancellation after staging as well as during I/O.
      private void Prepare() {
        Poll();
        if (staged == null) staged = Decompress(source, runPolicy);
        Poll();
      }
      public override int Read(byte[] buffer, int offset, int count) { return Read(new global::System.Span<byte>(buffer, offset, count)); }
      public override int Read(global::System.Span<byte> destination) {
        Begin();
        try {
          Prepare(); int count = global::System.Math.Min(destination.Length, staged!.Length - position);
          int copied = 0;
          while (copied < count) {
            Poll(); int size = global::System.Math.Min(ChunkBytes, count - copied);
            new global::System.ReadOnlySpan<byte>(staged, position + copied, size).CopyTo(destination.Slice(copied, size)); copied += size;
          }
          Poll(); position += count; return count;
        } catch (global::System.Exception cause) { staged = null; Fail(cause); throw; } finally { busy = false; }
      }
      /// <summary>Require all validated output to have been consumed; failure permanently fences the wrapper.</summary>
      public void Finish() {
        Begin();
        try {
          Prepare();
          if (position != staged!.Length) throw new Error(Code.InvalidData, "Unread expanded compression data");
          Poll(); finished = true; staged = null;
        } catch (global::System.Exception cause) { staged = null; Fail(cause); throw; } finally { busy = false; }
      }
      public override void Flush() {
        Begin();
        try {
          Poll();
        } catch (global::System.Exception cause) { staged = null; Fail(cause); throw; } finally { busy = false; }
      }
      protected override void Dispose(bool disposing) { closed = true; staged = null; base.Dispose(disposing); }
      public override bool CanRead => !failed && !finished && !closed;
      public override bool CanSeek => false;
      public override bool CanWrite => false;
)compresscs"
R"compresscs(      public override long Length => throw new global::System.NotSupportedException();
      public override long Position { get => throw new global::System.NotSupportedException(); set => throw new global::System.NotSupportedException(); }
      public override long Seek(long offset, global::System.IO.SeekOrigin origin) => throw new global::System.NotSupportedException();
      public override void SetLength(long value) => throw new global::System.NotSupportedException();
      public override void Write(byte[] buffer, int offset, int count) => throw new global::System.NotSupportedException();
    }
  }
)compresscs"
