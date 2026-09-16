import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import serializer.example.AccountSchema;

/** Verify wire compatibility, malformed input, numeric boundaries, and decode limits. */
public final class CodecTest {
  /** Require a condition without relying on the JVM assertion switch. */
  private static void check(boolean condition) {
    if (!condition) { throw new AssertionError("Codec check failed"); }
  }

  /** Require malformed input or an exhausted limit to fail predictably. */
  private static void rejects(Runnable operation) {
    try { operation.run(); }
    catch (IllegalArgumentException expected) { return; }
    throw new AssertionError("Expected IllegalArgumentException");
  }

  /** Decode an exact JSON account with the normal generated entry point. */
  private static AccountSchema.Demo.Account json(String value) {
    return AccountSchema.Demo.Account.decode(value.getBytes(StandardCharsets.UTF_8), AccountSchema.Protocol.JSON);
  }

  /** Exercise portable primitive representations through every codec. */
  private static void primitives() {
    TestSchema.Coverage.Primitives value = new TestSchema.Coverage.Primitives();
    value.character = '\n';
    value.i8 = Byte.MIN_VALUE;
    value.u8 = (byte) 255;
    value.i16 = Short.MIN_VALUE;
    value.u16 = (short) 65535;
    value.i32 = Integer.MIN_VALUE;
    value.u32 = -1;
    value.i64 = Long.MIN_VALUE;
    value.u64 = -1L;
    value.f32 = -0.0f;
    value.f64 = Double.MAX_VALUE;
    value.flag = true;
    value.text = "\u0000\n\\\"\uD83D\uDE80";
    value.bytes = new byte[] {0, -1, Byte.MIN_VALUE};
    value.longs = new long[] {0, -1, Long.MIN_VALUE};
    value.flags = new boolean[] {true, false};
    value.emptyValues = new TestSchema.Coverage.Empty[] {new TestSchema.Coverage.Empty()};
    value.choices.put(-1L, TestSchema.Coverage.Choice.SECOND_VALUE);
    value.choices.put(0L, TestSchema.Coverage.Choice.FIRST_VALUE);
    value.names.put("\uD800\uDC00", "astral");
    value.names.put("\uE000", "bmp");
    value.variantIndex = 1;
    value.variantSelection = TestSchema.Coverage.Choice.SECOND_VALUE;
    for (TestSchema.Protocol protocol : TestSchema.Protocol.values()) {
      byte[] bytes = value.encode(protocol);
      TestSchema.Coverage.Primitives copy = TestSchema.Coverage.Primitives.decode(bytes, protocol);
      check(Arrays.equals(bytes, copy.encode(protocol)));
      check(copy.i8 == value.i8 && copy.u8 == value.u8 && copy.i16 == value.i16
          && copy.u16 == value.u16 && copy.i32 == value.i32 && copy.u32 == value.u32
          && copy.i64 == value.i64 && copy.u64 == value.u64 && copy.character == '\n'
          && Float.floatToRawIntBits(copy.f32) == Float.floatToRawIntBits(value.f32)
          && copy.f64 == value.f64 && copy.flag && copy.text.equals(value.text)
          && copy.choices.equals(value.choices) && copy.names.equals(value.names)
          && copy.variantSelection == value.variantSelection);
      for (int length = 0; length < bytes.length; ++length) {
        final byte[] truncated = Arrays.copyOf(bytes, length);
        rejects(() -> TestSchema.Coverage.Primitives.decode(truncated, protocol));
      }
      rejects(() -> TestSchema.Coverage.Primitives.decode(bytes, protocol,
          new TestSchema.Limits(bytes.length - 1, bytes.length, 100, 64)));
      rejects(() -> TestSchema.Coverage.Primitives.decode(bytes, protocol,
          new TestSchema.Limits(bytes.length, bytes.length, 0, 64)));
      rejects(() -> TestSchema.Coverage.Primitives.decode(bytes, protocol,
          new TestSchema.Limits(bytes.length, bytes.length, 100, 0)));
    }
    value.f32 = Float.intBitsToFloat(0x7fc01234);
    for (TestSchema.Protocol protocol : new TestSchema.Protocol[] {
        TestSchema.Protocol.BINARY_NONE, TestSchema.Protocol.BINARY_INTEGER,
        TestSchema.Protocol.BINARY_STRING}) {
      check(Float.floatToRawIntBits(TestSchema.Coverage.Primitives.decode(value.encode(protocol), protocol).f32)
          == 0x7fc01234);
    }
    rejects(() -> value.encode(TestSchema.Protocol.JSON));
    check(new TestSchema.Coverage.Defaults().maximum == -1L);
    check(new TestSchema.Coverage.Defaults().minimum == Long.MIN_VALUE);
    check(new TestSchema.Coverage.Defaults().newline == '\n');
    check(new TestSchema.Coverage.Defaults().escaped.equals("a\\b\n\"c"));
  }

  /** Check canonical compact length boundaries and rejection before excessive allocation. */
  private static void compact() {
    for (int length : new int[] {0, 63, 64, 16383, 16384, 4194303, 4194304}) {
      TestSchema.Coverage.CompactValues value = new TestSchema.Coverage.CompactValues();
      value.text = "x".repeat(length);
      byte[] bytes = value.encode(TestSchema.Protocol.BINARY_NONE);
      int extra = length <= 63 ? 0 : length <= 16383 ? 1 : length <= 4194303 ? 2 : 3;
      check(Byte.toUnsignedInt(bytes[0]) >>> 6 == extra);
      check(bytes.length == length + extra + 2);
      check(TestSchema.Coverage.CompactValues.decode(bytes, TestSchema.Protocol.BINARY_NONE).text.equals(value.text));
    }
    check(TestSchema.Coverage.CompactValues.decode(new byte[] {64, 0, 1},
        TestSchema.Protocol.BINARY_NONE).flag);
    rejects(() -> TestSchema.Coverage.CompactValues.decode(new byte[] {0, 2}, TestSchema.Protocol.BINARY_NONE));
    rejects(() -> TestSchema.Coverage.CompactValues.decode(new byte[] {(byte) 255, -1, -1, -1}, TestSchema.Protocol.BINARY_NONE));
    rejects(() -> TestSchema.Coverage.CompactValues.decode(new byte[] {1, (byte) 0xff, 0}, TestSchema.Protocol.BINARY_NONE));
  }

  /** Check strict JSON grammar, defaults, duplicate fields, and Unicode handling. */
  private static void malformed() {
    check(json("{}").displayName.equals("Ada"));
    check(json("{\"name\":\"a\",\"name\":\"b\"}").displayName.equals("b"));
    check(json("{\"base\":{\"record_id\":42},\"base\":{}}").base0.recordId == 42);
    check(json("{\"name\":\"\\uD83D\\uDE80\"}").displayName.equals("\uD83D\uDE80"));
    check(json("{\"counters\":[{\"value\":1,\"key\":\"a\"},{\"key\":\"a\",\"value\":2}]}").counters.get("a") == 2);
    for (String invalid : new String[] {
        "{}x", "{\"unknown\":1}", "{\"name\":null}", "{\"enabled\":2}", "{\"small\":128}",
        "{\"small\":-129}", "{\"medium\":-1}", "{\"medium\":65536}", "{\"account_id\":18446744073709551616}",
        "{\"account_id\":-1}", "{\"account_id\":01}", "{\"account_id\":+1}", "{\"account_id\":1.0}",
        "{\"balance\":NaN}", "{\"balance\":1e999}", "{\"balance\":.5}", "{\"balance\":1.}",
        "{\"balance\":1e-999}", "{\"payload:ratio\":1e-99}",
        "{\"name\":\"\\uD800\"}", "{\"name\":\"\\uDC00\"}", "{\"name\":\"\\uZZZZ\"}",
        "{\"name\":\"\\x\"}", "{\"name\":\"\n\"}", "{\"name\":\"a\",}", "{\"scores\":[1,]}",
        "{\"state\":\"other\"}", "{\"counters\":[{\"key\":\"a\"}]}", "{\"payload:other\":1}"}) {
      rejects(() -> json(invalid));
    }
    AccountSchema.Demo.Account value = new AccountSchema.Demo.Account();
    value.displayName = "\uD800";
    rejects(() -> value.encode(AccountSchema.Protocol.BINARY_NONE));
    value.displayName = "valid";
    value.payloadIndex = -1;
    rejects(() -> value.encode(AccountSchema.Protocol.BINARY_INTEGER));
  }

  /** Read C++ fixtures, check values, and emit messages for C++ to validate. */
  private static void interoperability(Path directory) throws Exception {
    for (AccountSchema.Protocol protocol : AccountSchema.Protocol.values()) {
      byte[] bytes = Files.readAllBytes(directory.resolve("cpp_" + protocol + ".bin"));
      AccountSchema.Demo.Account value = AccountSchema.Demo.Account.decode(bytes, protocol);
      check(value.base0.recordId == 42 && value.accountId == -1L && value.small == -128
          && value.displayName.equals("Ada \"Lovelace\" \uD83D\uDE80")
          && value.payloadIndex == 1 && value.payloadRatio == 1.5f
          && value.otherIndex == 0 && value.otherStatus == AccountSchema.Demo.AccountState.ACTIVE
          && Arrays.equals(value.scores, new int[] {-1, 0, 100})
          && value.counters.get("visits") == 7 && value.history.length == 2);
      byte[] encoded = value.encode(protocol);
      if (protocol != AccountSchema.Protocol.JSON) { check(Arrays.equals(bytes, encoded)); }
      Files.write(directory.resolve("java_" + protocol + ".bin"), encoded);
    }
  }

  /** Run deterministic codec qualification without third-party Java dependencies. */
  public static void main(String[] args) throws Exception {
    primitives();
    compact();
    malformed();
    if (args.length != 0) { interoperability(Path.of(args[0])); }
    System.out.println("Java codec and interoperability checks passed");
  }
}
