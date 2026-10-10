import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

/** Import JSON and use the same owning model with every native wire protocol. */
public final class Main {
  /** Edit a typed field and check every decoded field through canonical binary bytes. */
  public static void main(String[] args) throws Exception {
    var defaults = new Schema.ExampleModel();
    if (defaults.pages.length != 2 || defaults.columns.length != 3 || defaults.headings.length != 2)
      throw new AssertionError("Incorrect fixed defaults");
    defaults.pages[0].title = "First";
    if (!defaults.pages[1].title.isEmpty()) throw new AssertionError("Aliased fixed elements");
    var value = Schema.ExampleModel.decode(Files.readAllBytes(Path.of(args[0])), Schema.Protocol.JSON);
    value.lookupCache = 77;
    value.revision += 1;
    var canonical = value.encode(Schema.Protocol.BINARY_NONE);
    for (var protocol : Schema.Protocol.values()) {
      var copy = Schema.ExampleModel.decode(value.encode(protocol), protocol);
      if (copy.lookupCache != 0) throw new AssertionError("Transient cache persisted");
      if (!Arrays.equals(copy.encode(Schema.Protocol.BINARY_NONE), canonical)) throw new AssertionError("Value mismatch");
    }
    Files.write(Path.of(args[1]), value.encode(Schema.Protocol.JSON));
    System.out.println("Four protocols passed");
  }
}
