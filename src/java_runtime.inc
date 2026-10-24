// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"java(
/** Wire protocols; fixed-width binary values use little-endian byte order. */
public enum ReadPolicy { STRICT, COMPATIBLE, FLEXIBLE }
public enum Protocol { JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING }

/** Parse the exact declared canonical component count. */
private static int[] srlVersionParts(String value, int count) {
  String[] parts = value.split("\\.", -1); if (parts.length != count) throw new IllegalArgumentException("Invalid dotted revision"); int[] result = new int[count];
  for (int i = 0; i < count; ++i) { if (!parts[i].matches("0|[1-9][0-9]{0,4}")) throw new IllegalArgumentException("Invalid dotted revision"); result[i] = Integer.parseInt(parts[i]); if (result[i] > 65535) throw new IllegalArgumentException("Invalid dotted revision"); } return result;
}
/** Compare numeric components rather than text. */
private static int srlVersionCompare(String a, String b, int count) { int[] x = srlVersionParts(a, count), y = srlVersionParts(b, count); for (int i = 0; i < count; ++i) if (x[i] != y[i]) return Integer.compare(x[i], y[i]); return 0; }

private static final int DEFAULT_MAX_DEPTH = 64;

/** Per-message decoding limits; collection entries are charged cumulatively. */
public static final class Limits {
  public final int maxBytes;
  public final int maxStringBytes;
  public final int maxElements;
  public final int maxDepth;
  public final ReadPolicy readPolicy;
  /** Optional operation-owned accounting; implementations retain their own failure identity. */
  public interface XmlOperation { void consume(String resource,long amount,String field); }
  public final XmlOperation xmlOperation;
  public String xmlExpectedRoot,xmlExpectedType;
  private boolean xmlPackageProfile,xmlRequiresType;
  private java.util.List<String> xmlVocabularies=java.util.List.of();

  /** Construct a nonnegative resource policy. */
  public Limits(int maxBytes, int maxStringBytes, int maxElements, int maxDepth) { this(maxBytes, maxStringBytes, maxElements, maxDepth, ReadPolicy.STRICT); }

  /** Construct a bounded decoding session with revision policy. */
  public Limits(int maxBytes, int maxStringBytes, int maxElements, int maxDepth, ReadPolicy readPolicy) {
    this(maxBytes,maxStringBytes,maxElements,maxDepth,readPolicy,null);
  }
  /** Attach cumulative XML admission without changing existing constructor behavior. */
  public Limits(int maxBytes,int maxStringBytes,int maxElements,int maxDepth,ReadPolicy readPolicy,XmlOperation operation) {
    this.xmlOperation=operation;
    this.readPolicy = java.util.Objects.requireNonNull(readPolicy);
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
/** Measure exact UTF8 without temporary storage and reject unpaired UTF16 surrogates. */
private static int nativeUtf8Length(String value){long size=0;for(int index=0;index<value.length();++index){char unit=value.charAt(index);if(Character.isHighSurrogate(unit)){if(index+1>=value.length()||!Character.isLowSurrogate(value.charAt(++index)))throw new IllegalArgumentException("Invalid UTF-16 string");size+=4;}else if(Character.isLowSurrogate(unit))throw new IllegalArgumentException("Invalid UTF-16 string");else size+=unit<128?1:unit<2048?2:3;if(size>Integer.MAX_VALUE)throw new IllegalArgumentException("Native string size overflow");}return (int)size;}

/** Compare UTF-8 byte sequences in the same order as C++ string map keys. */
private static int compareText(java.lang.String first, java.lang.String second) {
  return java.util.Arrays.compareUnsigned(utf8(first), utf8(second));
}

/** Per-call writer; generated codecs access fields directly without reflection. */
private static final class Writer {
  /** Keep the buffer helper inside the already reserved writer scope. */
  private static class SizeBuffer extends java.io.ByteArrayOutputStream {
    private final Limits limits;
    /** Preserve the ordinary native encoder's existing buffer behavior. */
    SizeBuffer(){super();limits=null;}
    /** Delay package-owned buffer allocation until its operation admits the actual growth. */
    SizeBuffer(Limits limits){super(0);this.limits=limits;}
    /** Establish an absolute output origin without allocating message-sized storage. */
    SizeBuffer(int start) { super(0); limits = null; count = start; }
    /** Wrap caller storage without allocating a message-sized array or mutating its contents. */
    SizeBuffer(byte[] destination, int offset) {
      this(offset);
      buf = java.util.Objects.requireNonNull(destination);
    }
    /** Admit complete output growth before mutating the private buffer. */
    private void reserve(int amount){if(limits==null)return;if(amount<0||amount>limits.maxBytes-count)throw new IllegalArgumentException("Native output byte limit");if(limits.xmlOperation!=null)limits.xmlOperation.consume("work",amount,"");int needed=count+amount;if(needed<=buf.length)return;int capacity=buf.length==0?Math.min(256,limits.maxBytes):buf.length;while(capacity<needed){if(capacity>limits.maxBytes/2){capacity=needed;break;}capacity*=2;}if(limits.xmlOperation!=null)limits.xmlOperation.consume("allocation",capacity,"");buf=java.util.Arrays.copyOf(buf,capacity);}
    /** Write one already validated byte after operation admission. */
    @Override public synchronized void write(int value){if(limits==null){super.write(value);return;}reserve(1);buf[count++]=(byte)value;}
    /** Copy a checked byte range only after its complete allocation and work admission. */
    @Override public synchronized void write(byte[] value,int offset,int length){java.util.Objects.checkFromIndexSize(offset,length,value.length);if(limits==null){super.write(value,offset,length);return;}reserve(length);java.lang.System.arraycopy(value,offset,buf,count,length);count+=length;}
    /** Keep the JDK convenience entry point on the same accounting path. */
    @Override public void writeBytes(byte[] value){write(value,0,value.length);}
    /** Admit the final exact-size independently owning result before copying the buffer. */
    @Override public synchronized byte[] toByteArray(){if(limits!=null&&limits.xmlOperation!=null)limits.xmlOperation.consume("allocation",count,"");return super.toByteArray();}
    /** Backpatch already-written fixed-width bytes without copying the surrounding message. */
    void patchInteger(int position, long value, int width) {
      if (position < 0 || width > count - position) {
        throw new java.lang.IllegalArgumentException(INVALID_SIZE_PATCH);
      }
      for (int index = 0; index < width; ++index) {
        buf[position + index] = (byte) (value >>> (index * java.lang.Byte.SIZE));
      }
    }
  }
)java"
R"java(  /** Bound output to an exclusive caller-owned window; failed writes may leave its prefix changed. */
  private static final class FixedBuffer extends SizeBuffer {
    private final int start;
    private final int end;
    /** Check the whole destination window before the first codec write. */
    FixedBuffer(byte[] destination, int offset, int length) {
      super(destination, offset);
      java.util.Objects.checkFromIndexSize(offset, length, destination.length);
      start = offset;
      end = offset + length;
    }
    /** Reject insufficient capacity before changing the next byte or byte range. */
    private void requireCapacity(int amount) {
      if (amount < 0 || amount > end - count) {
        throw new java.lang.IllegalArgumentException(OUTPUT_BUFFER_CAPACITY_EXCEEDED);
      }
    }
    /** Append one byte within the admitted destination window. */
    @Override public synchronized void write(int value) {
      requireCapacity(1);
      buf[count++] = (byte) value;
    }
    /** Copy a complete checked source range without growing or replacing caller storage. */
    @Override public synchronized void write(byte[] value, int offset, int length) {
      java.util.Objects.checkFromIndexSize(offset, length, value.length);
      requireCapacity(length);
      java.lang.System.arraycopy(value, offset, buf, count, length);
      count += length;
    }
    /** Keep generated fixed-size backpatches inside this window's already-written bytes. */
    @Override void patchInteger(int position, long value, int width) {
      if (position < start || position > count || width < 0 || width > count - position) {
        throw new java.lang.IllegalArgumentException(INVALID_SIZE_PATCH);
      }
      super.patchInteger(position, value, width);
    }
  }
  /** Bound heap/direct NIO writes without exposing the private view or changing caller cursor state. */
  private static final class NioBuffer extends SizeBuffer {
    private final java.nio.ByteBuffer window;
    private final int start;
    private final int end;
    /** Admit writability and retain an independent position/limit view of the caller's window. */
    NioBuffer(java.nio.ByteBuffer destination, int start) {
      super(start);
      if (destination.isReadOnly()) { throw new java.nio.ReadOnlyBufferException(); }
      window = destination.duplicate();
      this.start = start;
      end = window.limit();
    }
    /** Reject the next complete write before it can cross the caller's limit. */
    private void requireCapacity(int amount) {
      if (amount < 0 || amount > end - count) {
        throw new java.lang.IllegalArgumentException(OUTPUT_BUFFER_CAPACITY_EXCEEDED);
      }
    }
    /** Append one byte while advancing only the codec's private cursor. */
    @Override public synchronized void write(int value) {
      requireCapacity(1);
      window.put(count, (byte) value);
      ++count;
    }
    /** Copy a checked source range through the independent view without replacing caller storage. */
    @Override public synchronized void write(byte[] value, int offset, int length) {
      java.util.Objects.checkFromIndexSize(offset, length, value.length);
      requireCapacity(length);
      window.position(count);
      window.put(value, offset, length);
      count += length;
    }
    /** Backpatch already-written bytes in logical buffer coordinates, including sliced/direct windows. */
    @Override void patchInteger(int position, long value, int width) {
      if (position < start || position > count || width < 0 || width > count - position) {
        throw new java.lang.IllegalArgumentException(INVALID_SIZE_PATCH);
      }
      for (int index = 0; index < width; ++index) {
        window.put(position + index, (byte) (value >>> (index * java.lang.Byte.SIZE)));
      }
    }
  }
  /** Write count-free binary components or the canonical JSON text. */
  void version(String value, int count) { int[] parts = srlVersionParts(value, count); if (json()) text(value); else for (int part : parts) integer(part, 2, true); }

  private static final int COMPACT_MAX = 0x3fffffff;
  private static final java.lang.String INVALID_SIZE_PATCH = "Invalid size patch";
  private static final java.lang.String OUTPUT_BUFFER_CAPACITY_EXCEEDED = "Output buffer capacity exceeded";
  final Protocol protocol;
  final SizeBuffer bytes;
  final Limits limits;
  int depth;

  /** Create an isolated encoding session. */
  Writer(Protocol protocol) {
    this.protocol = java.util.Objects.requireNonNull(protocol);
    this.limits=null;this.bytes=new SizeBuffer();
  }
  /** Share one package-owned native operation without changing ordinary public encode APIs. */
  Writer(Protocol protocol,Limits limits){this.protocol=java.util.Objects.requireNonNull(protocol);this.limits=java.util.Objects.requireNonNull(limits);this.bytes=new SizeBuffer(limits);}
  /** Encode directly into one checked caller-owned window without changing ordinary buffer policy. */
  Writer(Protocol protocol, byte[] destination, int offset, int length) {
    this.protocol = java.util.Objects.requireNonNull(protocol);
    this.limits = null;
    this.bytes = new FixedBuffer(destination, offset, length);
  }
  /** Capture a writable NIO window with its absolute origin; public callers advance only after success. */
  Writer(Protocol protocol, java.nio.ByteBuffer destination, int start) {
    this.protocol = java.util.Objects.requireNonNull(protocol);
    this.limits = null;
    this.bytes = new NioBuffer(destination, start);
  }
  /** Select a generated count/stream sink without changing ordinary buffer construction. */
  Writer(Protocol protocol, SizeBuffer destination) {
    this.protocol = java.util.Objects.requireNonNull(protocol);
    this.limits = null;
    this.bytes = java.util.Objects.requireNonNull(destination);
  }
  /** Admit traversal and temporary storage before a generated native encoder owns it. */
  void consume(String resource,long amount){if(limits!=null&&limits.xmlOperation!=null)limits.xmlOperation.consume(resource,amount,"");}

  /** Report whether this session uses JSON. */
  boolean json() { return protocol == Protocol.JSON; }

  /** Write ASCII punctuation or numeric text. */
  void raw(java.lang.String value) {
    consume("allocation",value.length());
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

  /** Encode compact unsigned fields; lenient prefixes retain exactly the low 30 bits. */
  void compactValue(long value, int width, boolean prefix, boolean strict) {
    if (json()) { integer(value, width, true); return; }
    if (width < java.lang.Long.BYTES) { value &= (1L << (width*8))-1; }
    if (prefix) {
      if (strict && java.lang.Long.compareUnsigned(value, COMPACT_MAX) > 0) {
        throw new java.lang.IllegalArgumentException("Compact prefix integer range");
      }
      int payload = (int) (value & COMPACT_MAX);
      int extra = payload <= 0x3f ? 0 : payload <= 0x3fff ? 1 : payload <= 0x3fffff ? 2 : 3;
      bytes.write((extra << 6) | (payload >>> (extra*8)));
      for (int shift = (extra-1)*8; shift >= 0; shift -= 8) { bytes.write(payload >>> shift); }
      return;
    }
    while (java.lang.Long.compareUnsigned(value, 0x80) >= 0) {
      bytes.write((int) (value & 0x7f) | 0x80);
      value >>>= 7;
    }
    bytes.write((int) value);
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
    if(limits!=null){int size=nativeUtf8Length(value);if(size>limits.maxStringBytes)throw new IllegalArgumentException("Native string limit");consume("work",value.length());consume("allocation",3L*value.length()+size);}
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
    consume("work",1);
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
    } else if (protocol == Protocol.BINARY_INTEGER) { compact(id); })java"
R"java(
    else if (protocol == Protocol.BINARY_STRING) { text(name); }
  }

  /** Finish an object with its protocol-specific terminator. */
  void endObject() {
    if (json()) { bytes.write('}'); }
    else if (protocol != Protocol.BINARY_NONE) { compact(0); }
    --depth;
  }

  /** Write optional framing with a single binary presence byte. */
  void beginOptional(boolean present) {
    if (json()) { bytes.write('['); } else { integer(present ? 1 : 0, 1, true); }
  }
  /** Begin a collection with its element count where binary requires one. */
  void beginArray(int count) {
    consume("work",count);
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
  /** Admit decoder work and retained values while preserving package callback failure identity. */
  void consume(String resource,long amount){if(limits.xmlOperation!=null)limits.xmlOperation.consume(resource,amount,"");}
  /** Read exactly the declared dotted component count. */
  String version(int count) { String value; if (json()) value = text(); else { StringBuilder text = new StringBuilder(); for (int i = 0; i < count; ++i) { if (i != 0) text.append('.'); text.append(integer(2, true)); } value = text.toString(); } srlVersionParts(value, count); return value; }
  /** Consume bounded JSON extensions only under the flexible policy. */
  void skipValue() {
    if (!json() || limits.readPolicy != ReadPolicy.FLEXIBLE) throw error("Unknown field"); whitespace(); require(1); int c = bytes[offset];
    if (c == '"') { text(); return; }
    if (c == '{') { beginObject(); boolean first = true; java.util.Set<String> seen = new java.util.HashSet<>(); while (nextObject(first)) { first = false; if (!seen.add(key())) throw error("Duplicate field"); if (++elements > limits.maxElements) throw error("Element limit"); skipValue(); } endObject(); return; }
    if (c == '[') { if (++depth > limits.maxDepth) throw error("Depth limit"); int count = beginArray(); for (int i = 0; nextElement(i, count); ++i) skipValue(); --depth; return; }
    String value = token(); if (!value.equals("null") && !value.equals("true") && !value.equals("false") && !value.matches("-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?")) throw error("Invalid JSON value");
  }

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
    consume("work",length);
  }

)java"
R"java(  /** Consume only the four JSON whitespace characters. */
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
    consume("work",offset-start);consume("allocation",2L*(offset-start));
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

)java"
R"java(  /** Validate complete compact fields and target-width overflow before advancing. */
  long compactValue(int width, boolean prefix) {
    if (json()) { return integer(width, true); }
    int start = offset;
    if (prefix) {
      long value = compact();
      if (width < java.lang.Long.BYTES && value >= 1L << (width*8)) {
        offset = start;
        throw error("Compact integer range");
      }
      return value;
    }
    int bits = width*8;
    long value = 0;
    for (int index = 0; index < (bits+6)/7; ++index) {
      require(index+1);
      int current = java.lang.Byte.toUnsignedInt(bytes[start+index]);
      int payload = current & 0x7f;
      if (payload >= 1 << java.lang.Math.min(7, bits-index*7)) { throw error("Compact integer range"); }
      value |= (long) payload << (index*7);
      if (current < 0x80) {
        if (index != 0 && payload == 0) { throw error("Noncanonical compact integer"); }
        offset = start+index+1;
        return value;
      }
    }
    throw error("Unterminated compact integer");
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
)java"
R"java(      java.lang.String mantissa = value.split("[eE]", 2)[0];
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
      consume("allocation",4L*length);
      java.lang.String value = utf8(bytes, offset, length);
      offset += length;
      return value;
    }
    expect('"');
    consume("allocation",32);
    java.lang.StringBuilder value = new java.lang.StringBuilder();
    int start = offset;
    int segment = offset;
    while (true) {
      require(1);
      int current = java.lang.Byte.toUnsignedInt(bytes[offset++]);
      if (offset - start - 1 > limits.maxStringBytes) { throw error("java.lang.String limit exceeded"); }
      if (current == '"' || current == '\\') {
        consume("allocation",6L*(offset-segment-1)+2);
        value.append(utf8(bytes, segment, offset - segment - 1));
        if (current == '"') {
          consume("allocation",2L*value.length());
          java.lang.String result = value.toString();
          if ((limits.xmlOperation==null?utf8(result).length:nativeUtf8Length(result)) > limits.maxStringBytes) { throw error("java.lang.String limit exceeded"); }
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
    consume("work",1);consume("allocation",64);
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
    if(count>=0)consume("allocation",(long)count*(width==0?Long.BYTES:width));
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
    consume("work",1);if(json())consume("allocation",Long.BYTES);
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
R"java(
/** Preserve each optional level with bounded canonical []/[value] framing. */
private static <T> java.util.Optional<T> srlReadOptional(Reader in, java.util.function.Supplier<T> decode) {
  if (++in.depth > in.limits.maxDepth) throw in.error("Depth limit");
  boolean present;
  if (in.json()) { in.beginArray(); present = in.nextElement(0, -1); }
  else { long tag = in.integer(1, true); if (tag > 1) throw in.error("Invalid optional tag"); present = tag != 0;
    if (present && ++in.elements > in.limits.maxElements) throw in.error("Element limit"); }
  java.util.Optional<T> result = present ? java.util.Optional.of(decode.get()) : java.util.Optional.empty();
  if (present && in.json() && in.nextElement(1, -1)) throw in.error("Invalid optional length");
  --in.depth; return result;
}
/** Decode generic arrays without reserving an untrusted declared count. */
private static <T> java.util.List<T> srlReadArray(Reader in, java.util.function.Supplier<T> decode) {
  int count = in.beginArray(); java.util.List<T> result = new java.util.ArrayList<>();
  for (int index = 0; in.nextElement(index, count); ++index) result.add(decode.get());
  return result;
}
/** Decode generic maps using the native entry contract and last complete value. */
private static <K, V> java.util.NavigableMap<K, V> srlReadMap(Reader in, java.util.NavigableMap<K, V> result, java.util.function.Supplier<K> key, java.util.function.Supplier<V> value) {
  int count = in.beginArray();
  for (int index = 0; in.nextElement(index, count); ++index) {
    K k = null; V v = null; boolean hasKey = false, hasValue = false;
    if (in.json()) {
      in.beginObject(); boolean first = true;
      while (in.nextObject(first)) { first = false;
        switch (in.key()) { case "key": k = key.get(); hasKey = true; break;
          case "value": v = value.get(); hasValue = true; break; default: throw in.error("Unknown map entry field"); }
      }
      in.endObject(); if (!hasKey || !hasValue) throw in.error("Incomplete map entry");
    } else { k = key.get(); v = value.get(); }
    result.put(k, v);
  }
  return result;
}
/** Store bytes as a uint8 sequence without computing their digest. */
private static void srlWriteDigest(Writer out, byte[] value, int expected) {
  if (value == null || (expected != 0 && value.length != expected)) throw new IllegalArgumentException("Digest length mismatch");
  out.beginArray(value.length);
  for (int i = 0; i < value.length; ++i) { out.element(i); out.integer(Byte.toUnsignedInt(value[i]), 1, true); }
  out.endArray();
}
/** Decode bounded fresh storage and reject incorrect fixed digest lengths. */
private static byte[] srlReadDigest(Reader in, int expected) {
  int count = in.beginArray();
  if (expected != 0 && count != -1 && count != expected) throw in.error("Digest length mismatch");
  in.requireArray(count, 1);
  java.io.ByteArrayOutputStream value = new java.io.ByteArrayOutputStream(count < 0 ? 0 : count);
  for (int i = 0; in.nextElement(i, count); ++i) {
    if (expected != 0 && i >= expected) throw in.error("Digest length mismatch");
    value.write((int)in.integer(1, true));
  }
  if (expected != 0 && value.size() != expected) throw in.error("Digest length mismatch");
  return value.toByteArray();
}
)java"
