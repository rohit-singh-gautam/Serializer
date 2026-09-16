R"java(
/** Wire protocols; fixed-width binary values use little-endian byte order. */
public enum Protocol { JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING }

private static final int DEFAULT_MAX_DEPTH = 64;

/** Per-message decoding limits; collection entries are charged cumulatively. */
public static final class Limits {
  public final int maxBytes;
  public final int maxStringBytes;
  public final int maxElements;
  public final int maxDepth;

  /** Construct a nonnegative resource policy. */
  public Limits(int maxBytes, int maxStringBytes, int maxElements, int maxDepth) {
    if (maxBytes < 0 || maxStringBytes < 0 || maxElements < 0 || maxDepth < 0) {
      throw new java.lang.IllegalArgumentException("Negative decode limit");
    }
    this.maxBytes = maxBytes;
    this.maxStringBytes = maxStringBytes;
    this.maxElements = maxElements;
    this.maxDepth = maxDepth;
  }

  /** Return the default bounded policy. */
  public static Limits defaults() {
    return new Limits(64 * 1024 * 1024, 16 * 1024 * 1024, 1000000, DEFAULT_MAX_DEPTH);
  }
}

/** Reject malformed UTF-16 rather than silently replacing unpaired surrogates. */
private static byte[] utf8(java.lang.String value) {
  try {
    java.nio.ByteBuffer bytes = java.nio.charset.StandardCharsets.UTF_8.newEncoder()
        .onMalformedInput(java.nio.charset.CodingErrorAction.REPORT)
        .encode(java.nio.CharBuffer.wrap(java.util.Objects.requireNonNull(value)));
    byte[] result = new byte[bytes.remaining()];
    bytes.get(result);
    return result;
  } catch (java.nio.charset.CharacterCodingException error) {
    throw new java.lang.IllegalArgumentException("Invalid UTF-16 string", error);
  }
}

/** Decode UTF-8 strictly, preserving the Java string contract. */
private static java.lang.String utf8(byte[] bytes, int offset, int length) {
  try {
    return java.nio.charset.StandardCharsets.UTF_8.newDecoder()
        .onMalformedInput(java.nio.charset.CodingErrorAction.REPORT)
        .decode(java.nio.ByteBuffer.wrap(bytes, offset, length)).toString();
  } catch (java.nio.charset.CharacterCodingException error) {
    throw new java.lang.IllegalArgumentException("Invalid UTF-8 string", error);
  }
}

/** Compare UTF-8 byte sequences in the same order as C++ string map keys. */
private static int compareText(java.lang.String first, java.lang.String second) {
  return java.util.Arrays.compareUnsigned(utf8(first), utf8(second));
}

/** Per-call writer; generated codecs access fields directly without reflection. */
private static final class Writer {
  private static final int COMPACT_MAX = 0x3fffffff;
  final Protocol protocol;
  final java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
  int depth;

  /** Create an isolated encoding session. */
  Writer(Protocol protocol) {
    this.protocol = java.util.Objects.requireNonNull(protocol);
  }

  /** Report whether this session uses JSON. */
  boolean json() { return protocol == Protocol.JSON; }

  /** Write ASCII punctuation or numeric text. */
  void raw(java.lang.String value) {
    bytes.writeBytes(value.getBytes(java.nio.charset.StandardCharsets.US_ASCII));
  }

  /** Encode the established two-bit length tag, independent of scalar endianness. */
  void compact(int value) {
    if (value < 0 || value > COMPACT_MAX) {
      throw new java.lang.IllegalArgumentException("Compact integer out of range");
    }
    int extra = value <= 0x3f ? 0 : value <= 0x3fff ? 1 : value <= 0x3fffff ? 2 : 3;
    bytes.write((extra << 6) | (value >>> (extra * 8)));
    for (int shift = (extra - 1) * 8; shift >= 0; shift -= 8) {
      bytes.write(value >>> shift);
    }
  }

  /** Write an integer, retaining unsigned bit patterns where requested. */
  void integer(long value, int width, boolean unsigned) {
    if (json()) {
      if (unsigned && width < java.lang.Long.BYTES) {
        value &= (1L << (width * 8)) - 1;
      }
      raw(unsigned ? java.lang.Long.toUnsignedString(value) : java.lang.Long.toString(value));
    } else {
      for (int index = 0; index < width; ++index) {
        bytes.write((int) (value >>> (index * 8)));
      }
    }
  }

  /** Preserve floating-point binary bits and reject nonfinite JSON values. */
  void floating(double value, boolean single) {
    if (json()) {
      if (!java.lang.Double.isFinite(value)) {
        throw new java.lang.IllegalArgumentException("Nonfinite JSON number");
      }
      raw(single ? java.lang.Float.toString((float) value) : java.lang.Double.toString(value));
    } else {
      integer(single ? java.lang.Float.floatToRawIntBits((float) value)
          : java.lang.Double.doubleToRawLongBits(value), single ? java.lang.Float.BYTES : java.lang.Double.BYTES, false);
    }
  }

  /** Avoid float-to-double conversion so binary NaN payload bits are preserved. */
  void float32(float value) {
    if (json()) { floating(value, true); }
    else { integer(java.lang.Float.floatToRawIntBits(value), java.lang.Float.BYTES, false); }
  }

  /** Write the canonical boolean representation. */
  void bool(boolean value) {
    if (json()) { raw(value ? "true" : "false"); }
    else { bytes.write(value ? 1 : 0); }
  }

  /** Write a one-byte schema character. */
  void character(byte value) {
    if (json()) {
      if (value < 0) { throw new java.lang.IllegalArgumentException("JSON char must be ASCII"); }
      text(java.lang.Character.toString((char) value));
    } else { bytes.write(value); }
  }

  /** Encode UTF-8 with compact binary length or JSON escaping. */
  void text(java.lang.String value) {
    byte[] encoded = utf8(value);
    if (!json()) {
      compact(encoded.length);
      bytes.writeBytes(encoded);
      return;
    }
    bytes.write('"');
    final java.lang.String hex = "0123456789abcdef";
    for (byte element : encoded) {
      int current = java.lang.Byte.toUnsignedInt(element);
      if (current == '"' || current == '\\') {
        bytes.write('\\');
        bytes.write(current);
      } else if (current < 0x20) {
        raw("\\u00");
        bytes.write(hex.charAt(current >>> 4));
        bytes.write(hex.charAt(current & 15));
      } else { bytes.write(current); }
    }
    bytes.write('"');
  }

  /** Bound recursive output and begin an object. */
  void beginObject() {
    if (++depth > DEFAULT_MAX_DEPTH) {
      throw new java.lang.IllegalArgumentException("Encode depth limit exceeded");
    }
    if (json()) { bytes.write('{'); }
  }

  /** Write a field key in the selected protocol. */
  void field(int id, java.lang.String name, boolean first) {
    if (json()) {
      if (!first) { bytes.write(','); }
      text(name);
      bytes.write(':');
    } else if (protocol == Protocol.BINARY_INTEGER) { compact(id); }
    else if (protocol == Protocol.BINARY_STRING) { text(name); }
  }

  /** Finish an object with its protocol-specific terminator. */
  void endObject() {
    if (json()) { bytes.write('}'); }
    else if (protocol != Protocol.BINARY_NONE) { compact(0); }
    --depth;
  }

  /** Begin a collection with its element count where binary requires one. */
  void beginArray(int count) {
    if (json()) { bytes.write('['); }
    else { compact(count); }
  }

  /** Separate JSON elements without adding binary framing. */
  void element(int index) { if (json() && index != 0) { bytes.write(','); } }

  /** Finish a collection. */
  void endArray() { if (json()) { bytes.write(']'); } }
}

/** Bounded cursor shared by all nested generated readers in one message. */
private static final class Reader {
  private static final java.util.regex.Pattern INTEGER =
      java.util.regex.Pattern.compile("-?(0|[1-9][0-9]*)");
  private static final java.util.regex.Pattern NUMBER =
      java.util.regex.Pattern.compile("-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?");
  final byte[] bytes;
  final Protocol protocol;
  final Limits limits;
  int offset;
  int depth;
  int elements;

  /** Validate the input policy before decoding or allocation. */
  Reader(byte[] bytes, Protocol protocol, Limits limits) {
    this.bytes = java.util.Objects.requireNonNull(bytes);
    this.protocol = java.util.Objects.requireNonNull(protocol);
    this.limits = java.util.Objects.requireNonNull(limits);
    if (bytes.length > limits.maxBytes) { throw error("Message byte limit exceeded"); }
  }

  /** Attach a stable byte offset to malformed-input diagnostics. */
  java.lang.IllegalArgumentException error(java.lang.String message) {
    return new java.lang.IllegalArgumentException(message + " at byte " + offset);
  }

  /** Report whether the selected protocol is JSON. */
  boolean json() { return protocol == Protocol.JSON; }

  /** Require a bounded slice without overflow. */
  void require(int length) {
    if (length < 0 || length > bytes.length - offset) { throw error("Truncated input"); }
  }

  /** Consume only the four JSON whitespace characters. */
  void whitespace() {
    while (offset < bytes.length) {
      int value = bytes[offset];
      if (value != ' ' && value != '\n' && value != '\r' && value != '\t') { break; }
      ++offset;
    }
  }

  /** Require one JSON delimiter. */
  void expect(int value) {
    whitespace();
    require(1);
    if (bytes[offset++] != value) { throw error("Unexpected JSON delimiter"); }
  }

  /** Read a compact integer without advancing on a truncated prefix. */
  int compact() {
    require(1);
    int first = java.lang.Byte.toUnsignedInt(bytes[offset]);
    int extra = first >>> 6;
    require(extra + 1);
    int value = first & 0x3f;
    for (int index = 1; index <= extra; ++index) {
      value = (value << 8) | java.lang.Byte.toUnsignedInt(bytes[offset + index]);
    }
    offset += extra + 1;
    return value;
  }

  /** Read a JSON token without accepting punctuation as part of it. */
  java.lang.String token() {
    whitespace();
    int start = offset;
    while (offset < bytes.length) {
      int value = bytes[offset];
      if (value == ',' || value == '}' || value == ']' || value == ' '
          || value == '\r' || value == '\n' || value == '\t') { break; }
      ++offset;
    }
    if (start == offset) { throw error("Missing JSON token"); }
    return new java.lang.String(bytes, start, offset - start, java.nio.charset.StandardCharsets.US_ASCII);
  }

  /** Read exactly a declared integer width, validating JSON signedness and range. */
  long integer(int width, boolean unsigned) {
    if (!json()) {
      require(width);
      long result = 0;
      for (int index = 0; index < width; ++index) {
        result |= (long) java.lang.Byte.toUnsignedInt(bytes[offset++]) << (index * 8);
      }
      return result;
    }
    java.lang.String value = token();
    if (!INTEGER.matcher(value).matches()) { throw error("Invalid JSON integer"); }
    try {
      long result = unsigned ? java.lang.Long.parseUnsignedLong(value) : java.lang.Long.parseLong(value);
      if (width < java.lang.Long.BYTES) {
        int bits = width * 8;
        long minimum = unsigned ? 0 : -(1L << (bits - 1));
        long maximum = unsigned ? (1L << bits) - 1 : (1L << (bits - 1)) - 1;
        if (result < minimum || result > maximum) { throw error("java.lang.Integer out of range"); }
      }
      return result;
    } catch (java.lang.NumberFormatException error) { throw error("java.lang.Integer out of range"); }
  }

  /** Decode floating-point bits or a finite, grammatical JSON number. */
  double floating(boolean single) {
    if (!json()) {
      return single ? java.lang.Float.intBitsToFloat((int) integer(java.lang.Float.BYTES, false))
          : java.lang.Double.longBitsToDouble(integer(java.lang.Double.BYTES, false));
    }
    java.lang.String value = token();
    if (!NUMBER.matcher(value).matches()) {
      throw error("Invalid JSON number");
    }
    double result = single ? java.lang.Float.parseFloat(value) : java.lang.Double.parseDouble(value);
    if (!java.lang.Double.isFinite(result)) { throw error("Floating-point value out of range"); }
    if (result == 0) {
      java.lang.String mantissa = value.split("[eE]", 2)[0];
      if (mantissa.matches(".*[1-9].*")) { throw error("Floating-point underflow"); }
    }
    return result;
  }

  /** Retain binary float bits without promotion through double. */
  float float32() {
    return json() ? (float) floating(true) : java.lang.Float.intBitsToFloat((int) integer(java.lang.Float.BYTES, false));
  }

  /** Reject noncanonical binary booleans and nonboolean JSON tokens. */
  boolean bool() {
    if (json()) {
      java.lang.String value = token();
      if (value.equals("true")) { return true; }
      if (value.equals("false")) { return false; }
    } else {
      require(1);
      int value = bytes[offset++];
      if (value == 0 || value == 1) { return value == 1; }
    }
    throw error("Invalid boolean");
  }

  /** Read one byte, restricting JSON characters to single-byte UTF-8. */
  byte character() {
    if (!json()) { return (byte) integer(1, false); }
    java.lang.String value = text();
    if (value.length() != 1 || value.charAt(0) > 0x7f) {
      throw error("JSON char must contain one ASCII byte");
    }
    return (byte) value.charAt(0);
  }

  /** Decode four hexadecimal digits of a JSON Unicode escape. */
  char unicode() {
    require(4);
    int value = 0;
    for (int index = 0; index < 4; ++index) {
      int digit = java.lang.Character.digit((char) bytes[offset++], 16);
      if (digit < 0) { throw error("Invalid Unicode escape"); }
      value = (value << 4) | digit;
    }
    return (char) value;
  }

  /** Decode a bounded string, rejecting malformed UTF-8 and unpaired surrogates. */
  java.lang.String text() {
    if (!json()) {
      int length = compact();
      if (length > limits.maxStringBytes) { throw error("java.lang.String limit exceeded"); }
      require(length);
      java.lang.String value = utf8(bytes, offset, length);
      offset += length;
      return value;
    }
    expect('"');
    java.lang.StringBuilder value = new java.lang.StringBuilder();
    int start = offset;
    int segment = offset;
    while (true) {
      require(1);
      int current = java.lang.Byte.toUnsignedInt(bytes[offset++]);
      if (offset - start - 1 > limits.maxStringBytes) { throw error("java.lang.String limit exceeded"); }
      if (current == '"' || current == '\\') {
        value.append(utf8(bytes, segment, offset - segment - 1));
        if (current == '"') {
          java.lang.String result = value.toString();
          if (utf8(result).length > limits.maxStringBytes) { throw error("java.lang.String limit exceeded"); }
          return result;
        }
        require(1);
        int escaped = bytes[offset++];
        switch (escaped) {
          case '"': value.append('"'); break;
          case '\\': value.append('\\'); break;
          case '/': value.append('/'); break;
          case 'b': value.append('\b'); break;
          case 'f': value.append('\f'); break;
          case 'n': value.append('\n'); break;
          case 'r': value.append('\r'); break;
          case 't': value.append('\t'); break;
          case 'u': value.append(unicode()); break;
          default: throw error("Invalid JSON escape");
        }
        segment = offset;
      } else if (current < 0x20) { throw error("Unescaped JSON control character"); }
    }
  }

  /** Bound object nesting before descending. */
  void beginObject() {
    if (++depth > limits.maxDepth) { throw error("Depth limit exceeded"); }
    if (json()) { expect('{'); }
  }

  /** Determine whether another JSON object field follows, rejecting trailing commas. */
  boolean nextObject(boolean first) {
    whitespace();
    require(1);
    if (bytes[offset] == '}') { ++offset; return false; }
    if (!first) { expect(','); }
    return true;
  }

  /** Read a field name and its JSON separator if applicable. */
  java.lang.String key() {
    java.lang.String value = text();
    if (json()) { expect(':'); }
    return value;
  }

  /** Finish a generated object after its terminator has been consumed. */
  void endObject() { --depth; }

  /** Read a collection count without preallocating untrusted capacity. */
  int beginArray() {
    if (json()) { expect('['); return -1; }
    int count = compact();
    if (count > limits.maxElements - elements) { throw error("Element limit exceeded"); }
    return count;
  }

  /** Check fixed-width array bytes before allocating primitive storage. */
  void requireArray(int count, int width) {
    if (width != 0 && count > (bytes.length - offset) / width) {
      throw error("Truncated array");
    }
  }

  /** Consume separators and charge one cumulative collection entry. */
  boolean nextElement(int index, int count) {
    if (json()) {
      whitespace();
      require(1);
      if (bytes[offset] == ']') { ++offset; return false; }
      if (index != 0) { expect(','); }
    } else if (index == count) { return false; }
    if (elements == limits.maxElements) { throw error("Element limit exceeded"); }
    ++elements;
    return true;
  }

  /** Require one exact message; JSON permits trailing whitespace only. */
  void finish() {
    if (json()) { whitespace(); }
    if (offset != bytes.length) { throw error("Trailing input"); }
  }
}
)java"
