R"serializer(
  /// <summary>Native wire protocols with little-endian binary scalars.</summary>
  public enum Protocol { JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING }
  /// <summary>Immutable per-message limits; collection entries accumulate.</summary>
  public sealed class Limits {
    public int MaxBytes { get; }
    public int MaxStringBytes { get; }
    public int MaxElements { get; }
    public int MaxDepth { get; }
    // Validate policy before a decoding session starts.
    public Limits(int maxBytes = 64 * 1024 * 1024, int maxStringBytes = 16 * 1024 * 1024, int maxElements = 1000000, int maxDepth = 64) {
      if (maxBytes < 0 || maxStringBytes < 0 || maxElements < 0 || maxDepth < 0) throw new System.ArgumentOutOfRangeException(nameof(maxBytes));
      MaxBytes = maxBytes; MaxStringBytes = maxStringBytes; MaxElements = maxElements; MaxDepth = maxDepth;
    }
  }
  private static readonly System.Text.UTF8Encoding SrlUtf8 = new System.Text.UTF8Encoding(false, true);
  // Compare strings by UTF-8 bytes and other supported keys by their native values.
  private static int SrlCompare<T>(T first, T second) {
    if (first is string a && second is string b) {
      var x = SrlUtf8.GetBytes(a); var y = SrlUtf8.GetBytes(b);
      for (int i = 0; i < System.Math.Min(x.Length, y.Length); ++i) if (x[i] != y[i]) return x[i].CompareTo(y[i]);
      return x.Length.CompareTo(y.Length);
    }
    return System.Collections.Generic.Comparer<T>.Default.Compare(first, second);
  }
  /// <summary>Per-call byte writer; no reflection or native dependency.</summary>
  internal sealed class SrlWriter {
    internal readonly Protocol protocol;
    private readonly System.Buffers.ArrayBufferWriter<byte> buffer = new System.Buffers.ArrayBufferWriter<byte>(256);
    private int depth;
    // Validate protocol selection before any encoding.
    internal SrlWriter(Protocol protocol) {
      if (protocol < Protocol.JSON || protocol > Protocol.BINARY_STRING) throw new System.ArgumentOutOfRangeException(nameof(protocol));
      this.protocol = protocol;
    }
    // Return independent output storage.
    internal byte[] bytes() { return buffer.WrittenSpan.ToArray(); }
    // Test the active protocol.
    internal bool json() { return protocol == Protocol.JSON; }
    // Append punctuation or numeric text.
    internal void raw(string text) {
      int count = SrlUtf8.GetByteCount(text);
      int written = SrlUtf8.GetBytes(text, buffer.GetSpan(count)); buffer.Advance(written);
    }
    // Copy a validated sequence into reserved output capacity.
    private void put(System.ReadOnlySpan<byte> bytes) { bytes.CopyTo(buffer.GetSpan(bytes.Length)); buffer.Advance(bytes.Length); }
    // Append one framing byte without a temporary allocation.
    private void putByte(byte value) { buffer.GetSpan(1)[0] = value; buffer.Advance(1); }
    // Throw a stable validation error.
    internal void fail(string message) { throw new System.ArgumentException(message); }
    // Write the established compact integer encoding.
    internal void compact(int value) {
      if (value < 0 || value > 0x3fffffff) throw new System.ArgumentOutOfRangeException(nameof(value));
      int extra = value <= 0x3f ? 0 : value <= 0x3fff ? 1 : value <= 0x3fffff ? 2 : 3;
      putByte((byte)((extra << 6) | (value >> (extra * 8))));
      for (int shift = (extra - 1) * 8; shift >= 0; shift -= 8) putByte((byte)(value >> shift));
    }
    // Write unsigned bit patterns, interpreting signed JSON values at their width.
    internal void integer(ulong value, int width, bool unsigned) {
      if (json()) {
        int shift = 64 - width * 8;
        raw(unsigned ? value.ToString(System.Globalization.CultureInfo.InvariantCulture) :
          (unchecked((long)(value << shift)) >> shift).ToString(System.Globalization.CultureInfo.InvariantCulture));
      } else {
        var span = buffer.GetSpan(width);
        switch (width) {
          case 1: span[0] = (byte)value; break;
          case 2: System.Buffers.Binary.BinaryPrimitives.WriteUInt16LittleEndian(span, (ushort)value); break;
          case 4: System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(span, (uint)value); break;
          default: System.Buffers.Binary.BinaryPrimitives.WriteUInt64LittleEndian(span, value); break;
        }
        buffer.Advance(width);
      }
    }
    // Preserve IEEE binary representation and reject nonfinite JSON numbers.
    internal void floating(double value, bool single) {
      if (single) value = (float)value;
      if (json()) {
        if (!double.IsFinite(value)) fail("Nonfinite JSON number");
        raw(single ? ((float)value).ToString("R", System.Globalization.CultureInfo.InvariantCulture) : value.ToString("R", System.Globalization.CultureInfo.InvariantCulture));
      } else if (single) integer(unchecked((uint)System.BitConverter.SingleToInt32Bits((float)value)), 4, true);
      else integer(unchecked((ulong)System.BitConverter.DoubleToInt64Bits(value)), 8, true);
    }
    // Avoid widening single-precision binary NaN payloads.
    internal void float32(float value) { if (json()) floating(value, true); else integer(unchecked((uint)System.BitConverter.SingleToInt32Bits(value)), 4, true); }
    // Write canonical boolean values.
    internal void boolean(bool value) { if (json()) raw(value ? "true" : "false"); else putByte(value ? (byte)1 : (byte)0); }
    // Schema characters are bytes; JSON characters must be ASCII.
    internal void character(byte value) { if (json()) { if (value > 127) fail("JSON char must be ASCII"); text(((char)value).ToString()); } else putByte(value); }
    // Reject malformed UTF-16 before emitting string payloads.
    internal void text(string value) {
      int count = SrlUtf8.GetByteCount(value);
      if (json()) put(System.Text.Json.JsonSerializer.SerializeToUtf8Bytes(value));
      else { compact(count); raw(value); }
    }
    // Bound recursive object traversal, including user-created cycles.
    internal void beginObject() { if (++depth > 64) fail("Encode depth limit"); if (json()) raw("{"); }
    // Write the unchanged schema field identity.
    internal void field(int id, SrlField key, bool first) {
      if (json()) { if (!first) raw(","); put(key.Json); }
      else if (protocol == Protocol.BINARY_INTEGER) compact(id);
      else if (protocol == Protocol.BINARY_STRING) put(key.Binary);
    }
    // Emit the protocol-specific object terminator.
    internal void endObject() { if (json()) raw("}"); else if (protocol != Protocol.BINARY_NONE) compact(0); --depth; }
    // Begin an array or map entry sequence.
    internal void beginArray(int count) { if (json()) raw("["); else compact(count); }
    // Separate JSON elements only.
    internal void element(int index) { if (json() && index != 0) raw(","); }
    // Close a JSON collection.
    internal void endArray() { if (json()) raw("]"); }
  }
  // Immutable, pre-encoded keys avoid per-field UTF-8 work and temporary buffers.
  internal sealed class SrlField {
    internal readonly byte[] Binary, Json;
    // Compute both keyed representations once at generated-type initialization.
    internal SrlField(string name) {
      var binary = new SrlWriter(Protocol.BINARY_STRING); binary.text(name); Binary = binary.bytes();
      Json = SrlUtf8.GetBytes(System.Text.Json.JsonSerializer.Serialize(name) + ":");
    }
  }
  /// <summary>Bounded cursor shared by nested readers; failures expose no partial result.</summary>
  internal sealed class SrlReader {
    private static readonly System.Text.RegularExpressions.Regex IntegerPattern = new System.Text.RegularExpressions.Regex(@"\A-?(0|[1-9][0-9]*)\z", System.Text.RegularExpressions.RegexOptions.CultureInvariant);
    private static readonly System.Text.RegularExpressions.Regex NumberPattern = new System.Text.RegularExpressions.Regex(@"\A-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?\z", System.Text.RegularExpressions.RegexOptions.CultureInvariant);
    internal readonly Protocol protocol;
    private readonly System.ReadOnlyMemory<byte> data;
    private readonly Limits limits;
    private int offset, depth, elements;
    // Retain the input only for this synchronous decode call.
    internal SrlReader(System.ReadOnlyMemory<byte> data, Protocol protocol, Limits limits) {
      if (protocol < Protocol.JSON || protocol > Protocol.BINARY_STRING) throw new System.ArgumentOutOfRangeException(nameof(protocol));
      this.data = data; this.protocol = protocol; this.limits = limits;
      if (data.Length > limits.MaxBytes) fail("Message byte limit");
    }
    // Report the byte offset without embedding source contents.
    internal void fail(string message) { throw new System.FormatException(message + " at byte " + offset); }
    // Test the active protocol.
    internal bool json() { return protocol == Protocol.JSON; }
    // Validate lengths before taking spans or indexing.
    private void require(int count) { if (count < 0 || count > data.Length - offset) fail("Truncated input"); }
    // Skip only JSON whitespace bytes.
    private void whitespace() { while (offset < data.Length && (data.Span[offset] == 32 || data.Span[offset] == 9 || data.Span[offset] == 10 || data.Span[offset] == 13)) ++offset; }
    // Require one punctuation token.
    internal void expect(char value) { whitespace(); require(1); if (data.Span[offset++] != value) fail("Unexpected delimiter"); }
    // Decode an intact compact prefix without premature cursor movement.
    internal int compact() {
      require(1); int first = data.Span[offset], extra = first >> 6; require(extra + 1); int value = first & 63;
      for (int i = 1; i <= extra; ++i) value = (value << 8) | data.Span[offset + i];
      offset += extra + 1; return value;
    }
    // Preserve numeric digits until a typed parse is requested.
    private string token() {
      whitespace(); int start = offset;
      while (offset < data.Length && ",}] \t\r\n".IndexOf((char)data.Span[offset]) < 0) ++offset;
      if (start == offset) fail("Missing token"); return SrlUtf8.GetString(data.Span.Slice(start, offset - start));
    }
    // Read the declared integer width and enforce JSON signedness and range.
    internal ulong integer(int width, bool unsigned) {
      if (!json()) {
        require(width); var span = data.Span.Slice(offset, width); offset += width;
        return width switch {
          1 => span[0],
          2 => System.Buffers.Binary.BinaryPrimitives.ReadUInt16LittleEndian(span),
          4 => System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(span),
          _ => System.Buffers.Binary.BinaryPrimitives.ReadUInt64LittleEndian(span)
        };
      }
      string text = token(); if (!IntegerPattern.IsMatch(text)) fail("Invalid integer");
      if (unsigned) {
        if (text.StartsWith("-", System.StringComparison.Ordinal) || !ulong.TryParse(text, System.Globalization.NumberStyles.None, System.Globalization.CultureInfo.InvariantCulture, out _)) fail("Integer out of range");
        ulong value = ulong.Parse(text, System.Globalization.CultureInfo.InvariantCulture);
        if (width < sizeof(ulong) && value >= (1UL << (width * 8))) fail("Integer out of range"); return value;
      }
      if (!long.TryParse(text, System.Globalization.NumberStyles.AllowLeadingSign, System.Globalization.CultureInfo.InvariantCulture, out long signed)) fail("Integer out of range");
      if (width < sizeof(long) && (signed < -(1L << (width * 8 - 1)) || signed >= (1L << (width * 8 - 1)))) fail("Integer out of range");
      return unchecked((ulong)signed);
    }
    // Reject invalid numeric grammar, overflow, and underflow to zero.
    internal double floating(bool single) {
      if (!json()) return single ? System.BitConverter.Int32BitsToSingle(unchecked((int)integer(4, true))) : System.BitConverter.Int64BitsToDouble(unchecked((long)integer(8, true)));
      string text = token(); if (!NumberPattern.IsMatch(text)) fail("Invalid number");
      if (!double.TryParse(text, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out double value)) fail("Floating-point range");
      if (single) value = (float)value;
      if (!double.IsFinite(value)) fail("Floating-point range");
      if (value == 0) { foreach (char c in text.Split('e', 'E')[0]) if (c >= '1' && c <= '9') fail("Floating-point underflow"); }
      return value;
    }
    // Retain binary single-precision payload bits.
    internal float float32() { return json() ? (float)floating(true) : System.BitConverter.Int32BitsToSingle(unchecked((int)integer(4, true))); }
    // Accept canonical booleans only.
    internal bool boolean() {
      if (json()) { string text = token(); if (text == "true") return true; if (text == "false") return false; }
      else { ulong value = integer(1, true); if (value <= 1) return value == 1; }
      fail("Invalid boolean"); return false;
    }
    // Read one byte or one ASCII JSON character.
    internal byte character() {
      if (!json()) return (byte)integer(1, true); string value = text();
      if (value.Length != 1 || value[0] > 127) fail("Invalid char"); return (byte)value[0];
    }
    // Decode strict UTF-8 and reject malformed escaped surrogate sequences.
    internal string text() {
      if (!json()) {
        int count = compact(); if (count > limits.MaxStringBytes) fail("String limit"); require(count);
        string value = SrlUtf8.GetString(data.Span.Slice(offset, count)); offset += count; return value;
      }
      expect('"'); int start = offset; bool escaped = false;
      while (true) {
        require(1); byte c = data.Span[offset++];
        if (offset - start - 1 > limits.MaxStringBytes) fail("String limit");
        if (!escaped && c == '"') break;
        if (!escaped && c == '\\') escaped = true; else escaped = false;
      }
      string raw = SrlUtf8.GetString(data.Span.Slice(start - 1, offset - start + 1));
      string result = System.Text.Json.JsonSerializer.Deserialize<string>(raw) ?? throw new System.FormatException("Invalid string");
      if (SrlUtf8.GetByteCount(result) > limits.MaxStringBytes) fail("String limit"); return result;
    }
    // Check nesting before entering an object.
    internal void beginObject() { if (++depth > limits.MaxDepth) fail("Depth limit"); if (json()) expect('{'); }
    // Read JSON framing while retaining duplicate-field order.
    internal bool nextObject(bool first) { whitespace(); require(1); if (data.Span[offset] == '}') { ++offset; return false; } if (!first) expect(','); return true; }
    // Read a decoded field name.
    internal string key() { string value = text(); if (json()) expect(':'); return value; }
    // Leave an entered object.
    internal void endObject() { --depth; }
    // Validate counts before allocating collections.
    internal int beginArray() {
      if (json()) { expect('['); return -1; } int count = compact(); if (count > limits.MaxElements - elements) fail("Element limit"); return count;
    }
    // Validate binary scalar payload extents before reserving collection capacity.
    internal int arrayCapacity(int count, int width) {
      if (json()) return 0;
      if (width != 0 && count > (data.Length - offset) / width) fail("Truncated array");
      return count;
    }
    // Charge every collection slot, including zero-byte object values.
    internal bool nextElement(int index, int count) {
      if (json()) { whitespace(); require(1); if (data.Span[offset] == ']') { ++offset; return false; } if (index != 0) expect(','); }
      else if (index == count) return false;
      if (elements >= limits.MaxElements) fail("Element limit"); ++elements; return true;
    }
    // Require exact message consumption.
    internal void finish() { if (json()) whitespace(); if (offset != data.Length) fail("Trailing input"); }
  }
  // Encode only declared enum values, with the native context-dependent representation.
  private static void SrlWriteEnum(SrlWriter output, int value, string[] names, bool collection) {
    if (value < 0 || value >= names.Length) output.fail("Unknown enum");
    if (output.json() || (!collection && output.protocol == Protocol.BINARY_STRING)) output.text(names[value]); else output.compact(value);
  }
  // Decode a declared enum name or ordinal.
  private static int SrlReadEnum(SrlReader input, string[] names, bool collection) {
    int value = input.json() || (!collection && input.protocol == Protocol.BINARY_STRING) ? System.Array.IndexOf(names, input.text()) : input.compact();
    if (value < 0 || value >= names.Length) input.fail("Unknown enum"); return value;
  }
)serializer"
