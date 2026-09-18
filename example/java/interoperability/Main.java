import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

/** Five-language producer and consumer using the same generated schema. */
public final class Main {

  /** Construct the shared fixture independently, including unsigned bit patterns. */
  private static Schema.Interop.Message fixture(int variant) {
    var value = new Schema.Interop.Message();
    value.text = "Ada \"Lovelace\" 🚀\n" + "x".repeat(64);
    value.numbers = new int[] {Integer.MIN_VALUE, -1, 0, Integer.MAX_VALUE};
    value.decimals = new float[] {-0.0f, 1.5f, -2.25f};
    value.flags = new boolean[] {false, true}; value.labels = new String[] {"", "é", "🚀"};
    var child = new Schema.Interop.Detail(); child.code = -9; child.note = "child";
    value.children = new Schema.Interop.Detail[] {new Schema.Interop.Detail(), child};
    value.states = new Schema.Interop.State[] {Schema.Interop.State.PAUSED, Schema.Interop.State.READY};
    value.counts.put("🚀", -1L); value.counts.put("é", 7L); value.counts.put("a", 0L);
    value.indexed.put(-1L, child); value.indexed.put(0L, new Schema.Interop.Detail());
    value.toggles.put(true, "yes"); value.toggles.put(false, "no");
    value.enums.put(Schema.Interop.State.PAUSED, -2); value.enums.put(Schema.Interop.State.READY, 1);
    value.payloadIndex = variant; value.payloadNumber = -1234567890123456789L;
    value.payloadRatio = -3.5f; value.payloadState = Schema.Interop.State.PAUSED;
    return value;
  }

  /** Check full values using canonical positional encoding and compare all binary bytes. */
  private static void exchange(Path directory, String mode) throws Exception {
    Files.createDirectories(directory);
    for (int variant = 0; variant < 3; ++variant) {
      var expected = fixture(variant); var canonical = expected.encode(Schema.Protocol.BINARY_NONE);
      for (var protocol : Schema.Protocol.values()) {
        var encoded = expected.encode(protocol);
        if (mode.equals("emit")) Files.write(directory.resolve("java_" + protocol + "_" + variant + ".bin"), encoded);
        else for (var language : Files.readAllLines(directory.resolve("producers.txt"))) {
          var input = Files.readAllBytes(directory.resolve(language + "_" + protocol + "_" + variant + ".bin"));
          var actual = Schema.Interop.Message.decode(input, protocol);
          if (!Arrays.equals(actual.encode(Schema.Protocol.BINARY_NONE), canonical)) throw new AssertionError(language + " -> java " + protocol + "/" + variant + " value mismatch");
          if (protocol != Schema.Protocol.JSON && !Arrays.equals(input, encoded)) throw new AssertionError("Canonical binary mismatch");
        }
      }
    }
  }

  /** Run this language's side of the shared interoperability example. */
  public static void main(String[] args) throws Exception {
    if (args.length != 2 || (!args[1].equals("emit") && !args[1].equals("verify"))) throw new IllegalArgumentException("Usage: Main <directory> emit|verify");
    exchange(Path.of(args[0]), args[1]); System.out.println("java " + args[1] + " passed");
  }
}
