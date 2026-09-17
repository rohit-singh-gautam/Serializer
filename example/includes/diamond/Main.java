/** Run the diamond schema example using generated Java types. */
public final class Main {
  /** Check included declarations through JSON and all three binary protocols. */
  public static void main(String[] args) {
    IncludeSchema.Demo.Request source = new IncludeSchema.Demo.Request();
    source.first.owner.id = 42;
    source.second.base0.id = 73;
    source.second.label = "diamond include";
    for (IncludeSchema.Protocol protocol : IncludeSchema.Protocol.values()) {
      byte[] bytes = source.encode(protocol);
      IncludeSchema.Demo.Request decoded = IncludeSchema.Demo.Request.decode(bytes, protocol);
      if (decoded.first.owner.id != source.first.owner.id || decoded.second.base0.id != source.second.base0.id
          || !decoded.second.label.equals(source.second.label)) {
        throw new AssertionError("Round trip failed: " + protocol);
      }
      System.out.println(protocol + ": " + bytes.length + " bytes");
    }
  }
}
