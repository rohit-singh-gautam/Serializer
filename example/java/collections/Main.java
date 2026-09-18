import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

/** Import JSON and use the same owning model with every native wire protocol. */
public final class Main {
  /** Edit a typed field and check every decoded field through canonical binary bytes. */
  public static void main(String[] args) throws Exception {
    var value = Schema.ExampleModel.decode(Files.readAllBytes(Path.of(args[0])), Schema.Protocol.JSON);
    value.revision += 1;
    var canonical = value.encode(Schema.Protocol.BINARY_NONE);
    for (var protocol : Schema.Protocol.values()) {
      var copy = Schema.ExampleModel.decode(value.encode(protocol), protocol);
      if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), canonical)) throw new AssertionError("Value mismatch");
    }
    Files.write(Path.of(args[1]), value.encode(Schema.Protocol.JSON));
    System.out.println("Four protocols passed");
  }
}
