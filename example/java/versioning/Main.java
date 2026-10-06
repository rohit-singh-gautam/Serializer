import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

/** Import JSON and use the same owning model with every native wire protocol. */
public final class Main {
  /** Edit a typed field and check every decoded field through canonical binary bytes. */
  public static void main(String[] args) throws Exception {
    {
      var typed = new Schema.Uint16Version();
      if (!Arrays.equals(typed.encode(Schema.Protocol.BINARY_NONE), new byte[] {44, 1})) throw new AssertionError("Revision width");
      for (var protocol : Schema.Protocol.values()) {
        var copy = Schema.Uint16Version.decode(typed.encode(protocol), protocol);
        if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), typed.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Revision mismatch");
      }
    }
    {
      var typed = new Schema.Uint32Version();
      if (!Arrays.equals(typed.encode(Schema.Protocol.BINARY_NONE), new byte[] {112, 17, 1, 0})) throw new AssertionError("Revision width");
      for (var protocol : Schema.Protocol.values()) {
        var copy = Schema.Uint32Version.decode(typed.encode(protocol), protocol);
        if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), typed.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Revision mismatch");
      }
    }
    {
      var typed = new Schema.Uint64Version();
      if (!Arrays.equals(typed.encode(Schema.Protocol.BINARY_NONE), new byte[] {-1, -1, -1, -1, -1, -1, -1, -1})) throw new AssertionError("Revision width");
      for (var protocol : Schema.Protocol.values()) {
        var copy = Schema.Uint64Version.decode(typed.encode(protocol), protocol);
        if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), typed.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Revision mismatch");
      }
    }
    {
      var typed = new Schema.FloatVersion();
      if (!Arrays.equals(typed.encode(Schema.Protocol.BINARY_NONE), new byte[] {-51, -52, -52, 61})) throw new AssertionError("Revision width");
      for (var protocol : Schema.Protocol.values()) {
        var copy = Schema.FloatVersion.decode(typed.encode(protocol), protocol);
        if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), typed.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Revision mismatch");
      }
    }
    {
      var typed = new Schema.DoubleVersion();
      if (!Arrays.equals(typed.encode(Schema.Protocol.BINARY_NONE), new byte[] {0, 0, 0, 0, 0, 0, 4, 64})) throw new AssertionError("Revision width");
      for (var protocol : Schema.Protocol.values()) {
        var copy = Schema.DoubleVersion.decode(typed.encode(protocol), protocol);
        if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), typed.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Revision mismatch");
      }
    }
    {
      var typed = new Schema.Dotted2Version();
      if (!Arrays.equals(typed.encode(Schema.Protocol.BINARY_NONE), new byte[] {1, 0, 10, 0})) throw new AssertionError("Revision width");
      for (var protocol : Schema.Protocol.values()) {
        var copy = Schema.Dotted2Version.decode(typed.encode(protocol), protocol);
        if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), typed.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Revision mismatch");
      }
    }
    {
      var typed = new Schema.Dotted3Version();
      if (!Arrays.equals(typed.encode(Schema.Protocol.BINARY_NONE), new byte[] {1, 0, 10, 0, 0, 0})) throw new AssertionError("Revision width");
      for (var protocol : Schema.Protocol.values()) {
        var copy = Schema.Dotted3Version.decode(typed.encode(protocol), protocol);
        if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), typed.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Revision mismatch");
      }
    }
    {
      var typed = new Schema.Dotted4Version();
      if (!Arrays.equals(typed.encode(Schema.Protocol.BINARY_NONE), new byte[] {1, 0, 10, 0, 0, 0, 4, 0})) throw new AssertionError("Revision width");
      for (var protocol : Schema.Protocol.values()) {
        var copy = Schema.Dotted4Version.decode(typed.encode(protocol), protocol);
        if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), typed.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Revision mismatch");
      }
    }
    var value = Schema.ExampleModel.decode(Files.readAllBytes(Path.of(args[0])), Schema.Protocol.JSON, new Schema.Limits(67108864, 16777216, 1000000, 64, Schema.ReadPolicy.COMPATIBLE));
    var old = Schema.ExampleModel.decode(value.encode(Schema.Protocol.BINARY_NONE), Schema.Protocol.BINARY_NONE, new Schema.Limits(67108864, 16777216, 1000000, 64, Schema.ReadPolicy.COMPATIBLE));
    if (old.version != 8 || !old.oldName.equals("Ada")) throw new AssertionError("Historical mismatch");
    value.name = value.oldName;
    value.version = 10;
    var canonical = value.encode(Schema.Protocol.BINARY_NONE);
    for (var protocol : Schema.Protocol.values()) {
      var copy = Schema.ExampleModel.decode(value.encode(protocol), protocol);
      if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), canonical)) throw new AssertionError("Value mismatch");
    }
    Files.write(Path.of(args[1]), value.encode(Schema.Protocol.JSON));
    System.out.println("Version migration and four protocols passed");
  }
}
