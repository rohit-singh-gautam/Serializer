// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compresscs(    private const int Lz4HistoryBytes = 64 * 1024;
    private const int Lz4OutputBlockBytes = 64 * 1024;
    private const uint XxPrime1 = 2654435761U, XxPrime2 = 2246822519U,
        XxPrime3 = 3266489917U, XxPrime4 = 668265263U, XxPrime5 = 374761393U;
    // Public safe byte-array overloads support a separate retained dictionary directly.
    private sealed class Lz4Api {
      internal readonly global::System.Reflection.MethodInfo Bound, Encode, Decode;
      internal readonly global::System.Type Level;
      private readonly global::System.Collections.Generic.HashSet<int> levels = new global::System.Collections.Generic.HashSet<int>();
      internal readonly int MinimumLevel, MaximumLevel;
      // Resolve exact array APIs without referencing the provider's unsafe source or frame-stream package.
      internal Lz4Api(global::System.Reflection.Assembly assembly) {
        var codec = OptionalType(assembly, "K4os.Compression.LZ4.LZ4Codec");
        Level = OptionalType(assembly, "K4os.Compression.LZ4.LZ4Level");
        if (!Level.IsEnum || global::System.Enum.GetUnderlyingType(Level) != typeof(int)) throw new global::System.TypeLoadException();
        Bound = OptionalMethod(codec, "MaximumOutputSize", typeof(int));
        Encode = OptionalMethod(codec, "Encode", typeof(byte[]), typeof(int), typeof(int), typeof(byte[]), typeof(int), typeof(int), Level);
        Decode = OptionalMethod(codec, "Decode", typeof(byte[]), typeof(int), typeof(int), typeof(byte[]), typeof(int), typeof(int), typeof(byte[]), typeof(int), typeof(int));
        MinimumLevel = int.MaxValue; MaximumLevel = int.MinValue;
        foreach (object value in global::System.Enum.GetValues(Level)) {
          int number = global::System.Convert.ToInt32(value); levels.Add(number);
          MinimumLevel = global::System.Math.Min(MinimumLevel, number); MaximumLevel = global::System.Math.Max(MaximumLevel, number);
        }
        if (!levels.Contains(0)) throw new global::System.MissingMemberException();
      }
      // Preserve the provider's defined numeric levels rather than inventing DEFLATE-equivalent tuning.
      internal void ValidateLevel(int value) {
        if (!levels.Contains(value)) throw new Error(Code.InvalidOptions, "Invalid LZ4 compression level");
      }
    }
    // Rotate one standard 32-bit XXHash word with intentional unsigned wraparound.
    private static uint XxRotate(uint value, int count) { return (value << count) | (value >> (32 - count)); }
    // Apply the standard XXHash32 lane round, independent of provider-specific frame readers.
    private static uint XxRound(uint value, uint input) { unchecked { return XxRotate(value + input * XxPrime2, 13) * XxPrime1; } }
    // Check descriptor/block/content bytes using standard seed-zero XXHash32 and bounded policy polling.
    private static uint XxHash(byte[] input, int offset, int count, OptionalCheck check) {
      Require(input, offset, count);
      unchecked {
        int start = offset, end = offset + count; uint hash;
        if (count >= 16) {
          uint first = XxPrime1 + XxPrime2, second = XxPrime2, third = 0, fourth = 0U - XxPrime1;
          do {
            if (((offset - start) % ChunkBytes) == 0) check.Poll();
            first = XxRound(first, (uint)OptionalLittle(input, offset, 4)); offset += 4;
            second = XxRound(second, (uint)OptionalLittle(input, offset, 4)); offset += 4;
            third = XxRound(third, (uint)OptionalLittle(input, offset, 4)); offset += 4;
            fourth = XxRound(fourth, (uint)OptionalLittle(input, offset, 4)); offset += 4;
          } while (end - offset >= 16);
          hash = XxRotate(first, 1) + XxRotate(second, 7) + XxRotate(third, 12) + XxRotate(fourth, 18);
        } else hash = XxPrime5;
        hash += (uint)count;
        while (end - offset >= 4) { hash = XxRotate(hash + (uint)OptionalLittle(input, offset, 4) * XxPrime3, 17) * XxPrime4; offset += 4; }
        while (offset < end) { hash = XxRotate(hash + input[offset++] * XxPrime5, 11) * XxPrime1; }
        hash ^= hash >> 15; hash *= XxPrime2; hash ^= hash >> 13; hash *= XxPrime3; hash ^= hash >> 16;
        check.Poll(); return hash;
      }
    }
    // Emit one ordinary checksummed independent-block frame with bounded safe block calls.
    private static byte[] Lz4Compress(byte[] input, EncodeOptions policy) {
      var api = (Lz4Api)policy.Provider!.Api; var check = new OptionalCheck(policy.CheckCancelled);
      using var output = new BoundedOutput(policy.MaxOutputBytes, check.Poll);
      int capacity = (int)OptionalInvoke(api.Bound, null, Lz4OutputBlockBytes)!;
      if (capacity < Lz4OutputBlockBytes || capacity > 2 * Lz4OutputBlockBytes) throw new global::System.IO.IOException("Invalid LZ4 provider bound");
      byte[] packed = new byte[capacity], header = new byte[]{4,34,77,24,100,64,0};
      header[6] = (byte)((XxHash(header, 4, 2, check) >> 8) & byte.MaxValue); output.Write(header, 0, header.Length);
      for (int offset = 0; offset < input.Length;) {
        int count = global::System.Math.Min(Lz4OutputBlockBytes, input.Length - offset);
        check.Poll(); int produced = (int)OptionalInvoke(api.Encode, null, input, offset, count, packed, 0, packed.Length,
            global::System.Enum.ToObject(api.Level, policy.ProviderLevel))!; check.Poll();
        if (produced <= 0 || produced > packed.Length) throw new global::System.IO.IOException("Invalid LZ4 compression result");
        if (produced < count) { OptionalWrite(output, (uint)produced); output.Write(packed, 0, produced); }
        else { OptionalWrite(output, (uint)count | 0x80000000U); output.Write(input, offset, count); }
        offset += count;
      }
      OptionalWrite(output, 0); OptionalWrite(output, XxHash(input, 0, input.Length, check));
      check.Poll(); return output.Bytes();
    }
    // Read extension bytes without overflow or materializing unvalidated regenerated output.
    private static int Lz4Length(byte[] input, ref int cursor, int initial, int end, int maximum, OptionalCheck check) {
      int value = initial;
      if (initial == 15) {
        int extension;
        do {
          if (cursor >= end) throw new Error(Code.InvalidData, "Truncated LZ4 length");
          if (cursor % ChunkBytes == 0) check.Poll(); extension = input[cursor++];
          if (extension > maximum - value) throw new Error(Code.InvalidData, "Invalid LZ4 length");
          value += extension;
        } while (extension == 255);
      }
      return value;
    }
    // Count every regenerated byte and verify dictionary/history availability before expanded allocation.
    private static int Lz4BlockSize(byte[] input, int offset, int count, int history, int maximum, OptionalCheck check) {
      int cursor = offset, end = offset + count, produced = 0, nextPoll = offset;
      while (cursor < end) {
        if (cursor >= nextPoll) { check.Poll(); nextPoll = (int)global::System.Math.Min((long)cursor + ChunkBytes, end); }
        int token = input[cursor++], literals = Lz4Length(input, ref cursor, token >> 4, end, maximum, check);
        if (literals > end - cursor || literals > maximum - produced) throw new Error(Code.InvalidData, "Invalid LZ4 literals");
        cursor += literals; produced += literals; if (cursor == end) return produced;
        if (end - cursor < 2) throw new Error(Code.InvalidData, "Truncated LZ4 match");
        int distance = (int)OptionalLittle(input, cursor, 2); cursor += 2;
        if (distance == 0 || distance > produced + history) throw new Error(Code.InvalidData, "Invalid LZ4 match distance");
        int match = Lz4Length(input, ref cursor, token & 15, end, maximum, check);
        if (maximum - produced < 4 || match > maximum - produced - 4) throw new Error(Code.InvalidData, "Invalid LZ4 match length");
        produced += match + 4;
      }
      throw new Error(Code.InvalidData, "Missing LZ4 final literals");
    }
    // Retain only the final 64 KiB, independently of total frame length and previous block shape.
    private static byte[] Lz4History(byte[] history, byte[] block, int offset, int count) {
      int size = global::System.Math.Min(Lz4HistoryBytes, history.Length + count), fromBlock = global::System.Math.Min(size, count);
      byte[] result = new byte[size];
      if (size > fromBlock) global::System.Array.Copy(history, history.Length - (size - fromBlock), result, 0, size - fromBlock);
      global::System.Array.Copy(block, offset + count - fromBlock, result, size - fromBlock, fromBlock); return result;
    }
    // Validate standard framing/checksums, exact EOF and linked history with the explicit dictionary API.
    private static byte[] Lz4Decompress(byte[] input, DecodeOptions policy) {
      var api = (Lz4Api)policy.Provider!.Api; var check = new OptionalCheck(policy.CheckCancelled);
      if (OptionalLittle(input, 0, 4) != 0x184d2204UL) throw new Error(Code.InvalidData, "Expected one standard LZ4 frame");
      Require(input, 4, 2); int flags = input[4], descriptor = input[5];
      if ((flags >> 6) != 1 || (flags & 2) != 0 || (descriptor & 143) != 0) throw new Error(Code.InvalidData, "Invalid LZ4 descriptor");
      if ((flags & 1) != 0) throw new Error(Code.InvalidData, "LZ4 dictionaries are unsupported");
      int blockId = (descriptor >> 4) & 7;
      if (blockId < 4 || blockId > 7) throw new Error(Code.InvalidData, "Invalid LZ4 block maximum");
      int maximum = 1 << (8 + 2 * blockId), offset = 6; ulong expected = ulong.MaxValue;
      if ((flags & 8) != 0) {
        expected = OptionalLittle(input, offset, 8); offset += 8;
        if (expected > (ulong)policy.MaxOutputBytes) throw new Error(Code.ResourceLimit, "Expanded output limit exceeded");
      }
      Require(input, offset, 1);
      if (input[offset] != (byte)((XxHash(input, 4, offset - 4, check) >> 8) & byte.MaxValue)) throw new Error(Code.InvalidData, "Invalid LZ4 descriptor checksum");
      ++offset; if (maximum > policy.MaxWindowBytes) throw new Error(Code.ResourceLimit, "Compression history limit exceeded");
      using var output = new BoundedOutput(policy.MaxOutputBytes, check.Poll); byte[] history = global::System.Array.Empty<byte>();
      while (true) {
        check.Poll(); uint block = (uint)OptionalLittle(input, offset, 4); offset += 4; if (block == 0) break;
        bool raw = (block & 0x80000000U) != 0; int count = (int)(block & 0x7fffffffU);
        if (count > maximum) throw new Error(Code.InvalidData, "Invalid LZ4 block size"); Require(input, offset, count);
        if ((flags & 16) != 0 && (uint)OptionalLittle(input, offset + count, 4) != XxHash(input, offset, count, check))
          throw new Error(Code.InvalidData, "Invalid LZ4 block checksum");
        byte[] plain = input; int plainOffset = offset, plainCount = count;
        if (!raw) {
          plainCount = Lz4BlockSize(input, offset, count, history.Length, maximum, check);
          if (plainCount > policy.MaxOutputBytes - output.Length) throw new Error(Code.ResourceLimit, "Expanded output limit exceeded");
          plain = new byte[plainCount]; plainOffset = 0; check.Poll();
          int decoded = (int)OptionalInvoke(api.Decode, null, input, offset, count, plain, 0, plain.Length, history, 0, history.Length)!; check.Poll();
          if (decoded != plainCount) throw new Error(Code.InvalidData, "Invalid LZ4 regenerated block");
        }
        check.Poll(); output.Write(plain, plainOffset, plainCount);
        if ((flags & 32) == 0) history = Lz4History(history, plain, plainOffset, plainCount);
        offset += count + ((flags & 16) != 0 ? 4 : 0);
      }
      byte[] result = output.Bytes();
      if ((flags & 4) != 0) {
        uint checksum = (uint)OptionalLittle(input, offset, 4); offset += 4;
        if (checksum != XxHash(result, 0, result.Length, check)) throw new Error(Code.InvalidData, "Invalid LZ4 content checksum");
      }
      if (offset != input.Length) throw new Error(Code.InvalidData, "Trailing LZ4 input");
      if (expected != ulong.MaxValue && expected != (ulong)result.Length) throw new Error(Code.InvalidData, "Invalid LZ4 content size");
      check.Poll(); return result;
    }
)compresscs"
