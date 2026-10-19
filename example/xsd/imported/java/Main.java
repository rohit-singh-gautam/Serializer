import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** Use generated XML and binary codecs with the same owning Java model. */
public final class Main {
  /** Require malformed or over-budget input to return no owning candidate. */
  private static void rejected(byte[] bytes, Schema.Limits limits) {
    try {
      Schema.XsdExample.XsdOrder196281168.decodeXml(bytes, limits, null);
    } catch (RuntimeException expected) {
      return;
    }
    throw new AssertionError("Invalid XML was accepted");
  }

  /** Check the shared XML contract, exact generated roundtrip, and binary side channels. */
  public static void main(String[] args) throws Exception {
    if (args.length != 2) throw new IllegalArgumentException("Expected INPUT OUTPUT");
    var input = Path.of(args[0]);
    if (Files.size(input) > 1024 * 1024) throw new IllegalArgumentException("Example input exceeds one MiB");
    byte[] bytes = Files.readAllBytes(input);
    var value = Schema.XsdExample.XsdOrder196281168.decodeXml(bytes);
    if (value.xsdId != 7 || !value.xsdCustomer.equals("R&D") || value.xsdCount != 3 ||
        !Arrays.equals(value.xsdTag, new String[]{"A", "B"})) throw new AssertionError("Fixture mismatch");
    byte[] canonical = value.encode(Schema.Protocol.BINARY_NONE);
    byte[] encoded = value.encodeXml();
    if (!Arrays.equals(Schema.XsdExample.XsdOrder196281168.decodeXml(encoded).encode(Schema.Protocol.BINARY_NONE), canonical)) throw new AssertionError("XML roundtrip");
    if (!Arrays.equals(Schema.XsdExample.XsdOrder196281168.decode(canonical, Schema.Protocol.BINARY_NONE).encodeXml(), encoded)) throw new AssertionError("Binary side channel");
    String text = "<order xmlns=\"urn:serializer:xsd-example\" id=\"7\"><customer>R&amp;D</customer><count>3</count><tag>A</tag><tag>B</tag></order>";
    String[] invalid = {
        text.replace("urn:serializer:xsd-example", "urn:wrong"),
        text.replace("<order ", "<wrong ").replace("</order>", "</wrong>"),
        text.replace("<count>3</count>", "<count>2147483648</count>"),
        text.replace("<count>3</count>", "<count>+-1</count>"),
        "<!DOCTYPE order [<!ENTITY attack 'bad'>]><order>&attack;</order>",
        new String(encoded, StandardCharsets.UTF_8) + "<extra/>"
    };
    for (String document : invalid) rejected(document.getBytes(StandardCharsets.UTF_8), Schema.Limits.defaults());
    rejected(bytes, new Schema.Limits(1, 1024, 100, 8));
    Files.write(Path.of(args[1]), encoded);
    System.out.println("Generated Java XML and binary side-channel checks passed");
  }
}
