// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Optional;

/** Qualify actual generated typed records against independent wire bytes. */
final class ManagedTokenJavaTest {
  private static int checks;
  private static final String[] NAMES = {"document_high", "document_low", "class_id", "object_id"};

  /** Keep checks active regardless of JVM assertion configuration. */
  private static void check(boolean condition) {
    ++checks;
    if (!condition) throw new AssertionError("Managed Java token check " + checks);
  }

  /** Require malformed input to fail transactionally. */
  private static void reject(Runnable action) {
    try { action.run(); }
    catch (IllegalArgumentException expected) { ++checks; return; }
    throw new AssertionError("Expected managed token rejection");
  }

  /** Write canonical unsigned base-128 bytes without generated runtime helpers. */
  private static void varint(ByteArrayOutputStream out, long value) {
    while (Long.compareUnsigned(value, 128) >= 0) {
      out.write((int) value & 127 | 128);
      value >>>= 7;
    }
    out.write((int) value);
  }

  /** Write the envelope independently of generated metadata, key and integer encoders. */
  private static byte[] fixture(Model.Protocol protocol, long[] values, int width,
      boolean compact, int[] fields) {
    var out = new ByteArrayOutputStream();
    if (protocol == Model.Protocol.JSON) {
      var text = new StringBuilder("{\"target\":{");
      for (int index = 0; index < fields.length; ++index) {
        int field = fields[index];
        if (index != 0) text.append(',');
        text.append('"').append(NAMES[field - 1]).append("\":")
            .append(Long.toUnsignedString(values[field - 1]));
      }
      return text.append("}}").toString().getBytes(StandardCharsets.UTF_8);
    }
    if (protocol == Model.Protocol.BINARY_INTEGER) out.write(7);
    else if (protocol == Model.Protocol.BINARY_STRING) {
      out.write(6); out.writeBytes("target".getBytes(StandardCharsets.UTF_8));
    }
    for (int field : fields) {
      if (protocol == Model.Protocol.BINARY_INTEGER) out.write(field);
      else if (protocol == Model.Protocol.BINARY_STRING) {
        byte[] name = NAMES[field - 1].getBytes(StandardCharsets.UTF_8);
        out.write(name.length); out.writeBytes(name);
      }
      long value = values[field - 1];
      if (field == 4 && compact) varint(out, value);
      else for (int byteIndex = 0; byteIndex < (field == 4 ? width : Long.BYTES); ++byteIndex) {
        out.write((int) (value >>> (Byte.SIZE * byteIndex)));
      }
    }
    if (protocol != Model.Protocol.BINARY_NONE) { out.write(0); out.write(0); }
    return out.toByteArray();
  }

  /** Exercise unsigned maxima, wrappers, phantom self edges, failures and exchange corpus files. */
  public static void main(String[] args) throws Exception {
    int width = Integer.parseInt(args[0]);
    boolean compact = Boolean.parseBoolean(args[1]);
    Path folder = Path.of(args[2]);
    check(Model.TokenWire.Item.MANAGED_CLASS_ID == 101);
    int corpus = 0;
    for (var protocol : Model.Protocol.values()) {
      for (long id : new long[] {1, 127, 128, width == Integer.BYTES ? 0xffffffffL : -1L}) {
        long[] values = {-1L, 2, 101, id};
        var token = new Model.ManagedReference<Model.TokenWire.Item>(-1L, 2, 101, id);
        var envelope = new Model.TokenWire.Envelope();
        envelope.target = token;
        byte[] expected = fixture(protocol, values, width, compact, new int[] {1, 2, 3, 4});
        check(Arrays.equals(envelope.encode(protocol), expected));
        var decoded = Model.TokenWire.Envelope.decode(expected, protocol);
        check(decoded.target.documentHigh == -1L && decoded.target.objectId == id);
        check(Arrays.equals(decoded.encode(protocol), expected));
        Files.write(folder.resolve("corpus-" + protocol.ordinal() + "-" + corpus++ + ".bin"), expected);
        for (int end = 0; end < expected.length; ++end) {
          byte[] truncated = Arrays.copyOf(expected, end);
          reject(() -> Model.TokenWire.Envelope.decode(truncated, protocol));
        }
        byte[] trailing = Arrays.copyOf(expected, expected.length + 1);
        reject(() -> Model.TokenWire.Envelope.decode(trailing, protocol));
        var wrapped = new Model.TokenWire.Containers();
        wrapped.selected = Optional.of(token);
        wrapped.links = tokenArray(token);
        wrapped.lookup.put(7, token);
        var restored = Model.TokenWire.Containers.decode(wrapped.encode(protocol), protocol);
        check(restored.selected.orElseThrow().objectId == id);
        check(restored.links[1].classId == 101 && restored.lookup.get(7).documentHigh == -1L);
        for (long[] invalid : new long[][] {{0, 0, 101, 1}, {1, 0, 102, 1}, {1, 0, 101, 0}}) {
          byte[] bytes = fixture(protocol, invalid, width, compact, new int[] {1, 2, 3, 4});
          reject(() -> Model.TokenWire.Envelope.decode(bytes, protocol));
          envelope.target = new Model.ManagedReference<>(invalid[0], invalid[1], invalid[2], invalid[3]);
          reject(() -> envelope.encode(protocol));
        }
        if (width == Integer.BYTES) {
          for (long invalidId : new long[] {-1L, 1L << Integer.SIZE}) {
            envelope.target = new Model.ManagedReference<>(1, 2, 101, invalidId);
            reject(() -> envelope.encode(protocol));
          }
        }
        if (protocol != Model.Protocol.BINARY_NONE) {
          for (int[] fields : new int[][] {{1, 2, 3}, {1, 2, 3, 4, 1}, {4, 3, 2, 1, 4}}) {
            byte[] malformed = fixture(protocol, values, width, compact, fields);
            reject(() -> Model.TokenWire.Envelope.decode(malformed, protocol));
          }
        }
      }
      var item = new Model.TokenWire.Item();
      item.persistentId = 42;
      item.next = Optional.of(new Model.ManagedReference<>(1, 2, 101, 42));
      var restored = Model.TokenWire.Item.decode(item.encode(protocol), protocol);
      check(restored.persistentId == 42 && restored.next.orElseThrow().objectId == 42);
    }
    for (String invalid : new String[] {
        "{\"target\":{\"document_high\":1,\"document_low\":2,\"class_id\":101,\"object_id\":3,\"extra\":0}}",
        "{\"target\":{\"document_high\":1,\"document_low\":2,\"class_id\":101,\"object_id\":-1}}"}) {
      byte[] bytes = invalid.getBytes(StandardCharsets.UTF_8);
      reject(() -> Model.TokenWire.Envelope.decode(bytes, Model.Protocol.JSON));
    }
    System.out.println("Generated Java managed token checks: " + checks);
  }

  /** Construct a typed generic array at the only unchecked boundary in this independent consumer. */
  @SuppressWarnings("unchecked")
  private static Model.ManagedReference<Model.TokenWire.Item>[] tokenArray(
      Model.ManagedReference<Model.TokenWire.Item> token) {
    var result = (Model.ManagedReference<Model.TokenWire.Item>[]) new Model.ManagedReference<?>[2];
    result[0] = token; result[1] = token;
    return result;
  }
}
