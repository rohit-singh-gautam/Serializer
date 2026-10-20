// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compresscs(    // Immutable public reflection handles; every stream and compression state is operation-owned.
    private sealed class ZstdApi {
      internal readonly int MinimumLevel, MaximumLevel;
      internal readonly global::System.Reflection.ConstructorInfo Encoder, Decoder;
      internal readonly global::System.Reflection.MethodInfo Pledge, EncodeParameter, DecodeParameter;
      private readonly global::System.Type error;
      private readonly global::System.Reflection.PropertyInfo errorCode;
      internal readonly object Checksum, Window;
      // Resolve the bounded stream API and advertised numeric limits without native assembly dependencies.
      internal ZstdApi(global::System.Reflection.Assembly assembly) {
        var compressor = OptionalType(assembly, "ZstdSharp.Compressor");
        var encoder = OptionalType(assembly, "ZstdSharp.CompressionStream");
        var decoder = OptionalType(assembly, "ZstdSharp.DecompressionStream");
        if (!typeof(global::System.IO.Stream).IsAssignableFrom(encoder) || !typeof(global::System.IO.Stream).IsAssignableFrom(decoder))
          throw new global::System.TypeLoadException();
        Encoder = encoder.GetConstructor(new[]{typeof(global::System.IO.Stream),typeof(int),typeof(int),typeof(bool)}) ?? throw new global::System.MissingMethodException();
        Decoder = decoder.GetConstructor(new[]{typeof(global::System.IO.Stream),typeof(int),typeof(bool),typeof(bool)}) ?? throw new global::System.MissingMethodException();
        var cparam = OptionalType(assembly, "ZstdSharp.Unsafe.ZSTD_cParameter");
        var dparam = OptionalType(assembly, "ZstdSharp.Unsafe.ZSTD_dParameter");
        Checksum = OptionalEnum(cparam, "ZSTD_c_checksumFlag");
        Window = OptionalEnum(dparam, "ZSTD_d_windowLogMax");
        Pledge = OptionalMethod(encoder, "SetPledgedSrcSize", typeof(ulong));
        EncodeParameter = OptionalMethod(encoder, "SetParameter", cparam, typeof(int));
        DecodeParameter = OptionalMethod(decoder, "SetParameter", dparam, typeof(int));
        MinimumLevel = (int)OptionalInvoke(OptionalMethod(compressor, "get_MinCompressionLevel"), null)!;
        MaximumLevel = (int)OptionalInvoke(OptionalMethod(compressor, "get_MaxCompressionLevel"), null)!;
        if (MinimumLevel > MaximumLevel) throw new global::System.MissingMemberException();
        error = OptionalType(assembly, "ZstdSharp.ZstdException");
        errorCode = error.GetProperty("Code") ?? throw new global::System.MissingMemberException();
      }
      // Accept the full provider-advertised interval, including negative fast compression levels.
      internal void ValidateLevel(int value) {
        if (value < MinimumLevel || value > MaximumLevel) throw new Error(Code.InvalidOptions, "Invalid Zstandard compression level");
      }
      // Map only expected frame/window diagnostics; unrelated provider failures retain their types.
      internal global::System.Exception DecodeFailure(global::System.Exception observed) {
        if (observed is global::System.IO.EndOfStreamException) return new Error(Code.InvalidData, "Truncated Zstandard frame");
        if (!error.IsInstanceOfType(observed)) return observed;
        string code = errorCode.GetValue(observed)?.ToString() ?? "";
        if (code == "ZSTD_error_frameParameter_windowTooLarge") return new Error(Code.ResourceLimit, "Compression history limit exceeded");
        if (code == "ZSTD_error_prefix_unknown" || code == "ZSTD_error_version_unsupported" ||
            code == "ZSTD_error_frameParameter_unsupported" || code == "ZSTD_error_corruption_detected" ||
            code == "ZSTD_error_literals_headerWrong" ||
            code == "ZSTD_error_checksum_wrong" || code == "ZSTD_error_dictionary_corrupted" ||
            code == "ZSTD_error_dictionary_wrong" || code == "ZSTD_error_srcSize_wrong" ||
            code == "ZSTD_error_tableLog_tooLarge" || code == "ZSTD_error_maxSymbolValue_tooLarge" ||
            code == "ZSTD_error_maxSymbolValue_tooSmall") return new Error(Code.InvalidData, "Invalid Zstandard frame");
        return observed;
      }
    }
    // Check exactly one standard frame and conservative advertised history before expanded allocation.
    private static void ZstdFrame(byte[] input, DecodeOptions policy, OptionalCheck check) {
      if (OptionalLittle(input, 0, 4) != 0xfd2fb528UL) throw new Error(Code.InvalidData, "Expected one standard Zstandard frame");
      Require(input, 4, 1); int descriptor = input[4];
      if ((descriptor & 8) != 0) throw new Error(Code.InvalidData, "Reserved Zstandard descriptor");
      bool single = (descriptor & 32) != 0; int offset = 5; ulong history = 0;
      if (!single) {
        Require(input, offset, 1); int window = input[offset++];
        ulong basis = 1UL << (10 + (window >> 3)); history = basis + (basis >> 3) * (ulong)(window & 7);
      }
      int dictionaryBytes = (descriptor & 3) == 3 ? 4 : descriptor & 3;
      if (OptionalLittle(input, offset, dictionaryBytes) != 0) throw new Error(Code.InvalidData, "Zstandard dictionaries are unsupported");
      offset += dictionaryBytes; int flag = descriptor >> 6;
      int sizeBytes = flag == 0 ? single ? 1 : 0 : 1 << flag;
      ulong contentSize = sizeBytes == 0 ? ulong.MaxValue : OptionalLittle(input, offset, sizeBytes);
      if (sizeBytes == 2) contentSize += 256;
      if (sizeBytes != 0 && contentSize > (ulong)policy.MaxOutputBytes) throw new Error(Code.ResourceLimit, "Expanded output limit exceeded");
      offset += sizeBytes; if (single) history = contentSize;
      ulong effective = 1UL << global::System.Numerics.BitOperations.Log2((uint)policy.MaxWindowBytes);
      if (history > effective) throw new Error(Code.ResourceLimit, "Compression history limit exceeded");
      ulong maximumBlock = global::System.Math.Min(128UL * 1024, history); bool last;
      do {
        check.Poll(); int block = (int)OptionalLittle(input, offset, 3); offset += 3;
        last = (block & 1) != 0; int type = (block >> 1) & 3, count = block >> 3;
        if (type == 3 || (ulong)count > maximumBlock) throw new Error(Code.InvalidData, "Invalid Zstandard block");
        int stored = type == 1 ? 1 : count; Require(input, offset, stored); offset += stored;
      } while (!last);
      if ((descriptor & 4) != 0) { Require(input, offset, 4); offset += 4; }
      if (offset != input.Length) throw new Error(Code.InvalidData, "Trailing Zstandard input");
      check.Poll();
    }
    // Encode one checksummed pledged-size frame, bounding input and provider output batches separately.
    private static byte[] ZstdCompress(byte[] input, EncodeOptions policy) {
      var api = (ZstdApi)policy.Provider!.Api; var check = new OptionalCheck(policy.CheckCancelled);
      using var output = new BoundedOutput(policy.MaxOutputBytes, check.Poll);
      global::System.IO.Stream? provider = null;
      try {
        provider = (global::System.IO.Stream)OptionalConstruct(api.Encoder, output, policy.ProviderLevel, ChunkBytes, true);
        OptionalInvoke(api.Pledge, provider, (ulong)input.Length); OptionalInvoke(api.EncodeParameter, provider, api.Checksum, 1);
        for (int offset = 0; offset < input.Length;) {
          int count = global::System.Math.Min(ChunkBytes, input.Length - offset);
          check.Poll(); provider.Write(input, offset, count); check.Poll(); offset += count;
        }
        provider.Dispose(); provider = null; check.Poll(); return output.Bytes();
      } catch {
        output.Abort(); check.Rethrow(); throw;
      } finally { OptionalDispose(provider); }
    }
    // Use the independent extent validator and provider-native window limit before bounded expansion.
    private static byte[] ZstdDecompress(byte[] input, DecodeOptions policy) {
      var api = (ZstdApi)policy.Provider!.Api; var check = new OptionalCheck(policy.CheckCancelled);
      ZstdFrame(input, policy, check);
      using var source = new ProviderInput(input, 0, input.Length, check.Poll);
      using var output = new BoundedOutput(policy.MaxOutputBytes, check.Poll);
      global::System.IO.Stream? provider = null;
      try {
        provider = (global::System.IO.Stream)OptionalConstruct(api.Decoder, source, ChunkBytes, true, true);
        OptionalInvoke(api.DecodeParameter, provider, api.Window, global::System.Numerics.BitOperations.Log2((uint)policy.MaxWindowBytes));
        byte[] batch = new byte[ChunkBytes];
        while (true) {
          int capacity = (int)global::System.Math.Min(ChunkBytes, (long)policy.MaxOutputBytes - output.Length + 1);
          check.Poll(); int count = provider.Read(batch, 0, capacity); check.Poll();
          if (count == 0) break;
          if (count < 0 || count > capacity) throw new global::System.IO.IOException("Invalid optional provider read count");
          output.Write(batch, 0, count);
        }
        provider.Dispose(); provider = null; check.Poll(); return output.Bytes();
      } catch (global::System.Exception failure) {
        output.Abort(); check.Rethrow();
        var observed = api.DecodeFailure(failure);
        if (!global::System.Object.ReferenceEquals(observed, failure)) throw observed;
        throw;
      } finally { OptionalDispose(provider); }
    }
)compresscs"
