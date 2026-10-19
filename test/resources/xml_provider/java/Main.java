import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.Executors;

/** Qualify immutable JAXP schemas through the generated owning XML codec. */
public final class Main {
  private static final String XML = "<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>";
  private static final String XSD_START = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>";

  /** Require the provider or generated codec to reject the operation before returning. */
  private static void rejected(Runnable action) {
    try { action.run(); } catch (IllegalArgumentException expected) { return; }
    throw new AssertionError("Provider accepted invalid input");
  }

  /** Prepare trusted bytes and test complete-message validation with independent contexts. */
  public static void main(String[] args) throws Exception {
    var schema = new Schema.XmlSchema(Files.readAllBytes(Path.of(args[0])));
    var limits = Schema.Limits.defaults();
    byte[] bytes = XML.getBytes(StandardCharsets.UTF_8);
    var value = Schema.XmlProviderFixture.Order.decodeXml(bytes, limits, schema);
    if (value.id != 7 || !value.customer.equals("R&D") || value.values[0] != 3) throw new AssertionError("Generated values");
    schema.validate(value.encodeXml(limits, schema), limits);
    for (String bad : new String[]{"<wrong xmlns='urn:serializer:xsd'/>",
        XML.replace("urn:serializer:xsd", "urn:wrong"), XML + "<extra/>",
        XML.replace("<value>3</value>", ""), "<!DOCTYPE order SYSTEM 'file:///must-not-open'>" + XML}) {
      rejected(() -> schema.validate(bad.getBytes(StandardCharsets.UTF_8), limits));
    }
    var mismatch = new Schema.XmlSchema(("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:serializer:xsd'><xs:element name='order' type='xs:string'/></xs:schema>").getBytes(StandardCharsets.UTF_8));
    rejected(() -> value.encodeXml(limits, mismatch));
    rejected(() -> Schema.XmlProviderFixture.Order.decodeXml(bytes, limits, mismatch));
    for (String directive : new String[]{"include", "import", "redefine", "override"}) {
      for (String location : new String[]{"file:///must-not-open.xsd", "https://invalid.example/must-not-open.xsd"}) {
        String source = XSD_START + "<xs:" + directive + " schemaLocation='" + location + "'/></xs:schema>";
        rejected(() -> new Schema.XmlSchema(source.getBytes(StandardCharsets.UTF_8)));
      }
    }
    String hinted = XML.replace(" id=", " xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:schemaLocation='urn:serializer:xsd https://invalid.example/must-not-open.xsd' id=");
    schema.validate(hinted.getBytes(StandardCharsets.UTF_8), limits);
    for (var bounded : new Schema.Limits[]{
        new Schema.Limits(bytes.length - 1, limits.maxStringBytes, limits.maxElements, limits.maxDepth),
        new Schema.Limits(limits.maxBytes, 1, limits.maxElements, limits.maxDepth),
        new Schema.Limits(limits.maxBytes, limits.maxStringBytes, 1, limits.maxDepth),
        new Schema.Limits(limits.maxBytes, limits.maxStringBytes, limits.maxElements, 1)}) {
      rejected(() -> schema.validate(bytes, bounded));
    }
    var workers = Executors.newFixedThreadPool(8);
    try {
      var tasks = new java.util.ArrayList<java.util.concurrent.Future<?>>();
      for (int worker = 0; worker < 8; ++worker) {
        tasks.add(workers.submit(() -> {
          for (int index = 0; index < 16; ++index) {
            schema.validate(bytes, limits);
            rejected(() -> schema.validate("<wrong/>".getBytes(StandardCharsets.UTF_8), limits));
          }
        }));
      }
      for (var task : tasks) task.get();
    } finally { workers.shutdownNow(); }
    System.out.println("Java prepared XSD provider checks passed");
  }
}
