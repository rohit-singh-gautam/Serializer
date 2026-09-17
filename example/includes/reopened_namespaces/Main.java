/** Run the reopened namespaces schema example using generated Java types. */
public final class Main {
  /** Check included declarations through JSON and all three binary protocols. */
  public static void main(String[] args) {
    IncludeSchema.Demo.Request source = new IncludeSchema.Demo.Request();
    source.owner.id = 42;
    source.auditRecord.label = "reopened namespace";
    for (IncludeSchema.Protocol protocol : IncludeSchema.Protocol.values()) {
      byte[] bytes = source.encode(protocol);
      IncludeSchema.Demo.Request decoded = IncludeSchema.Demo.Request.decode(bytes, protocol);
      if (decoded.owner.id != source.owner.id
          || !decoded.auditRecord.label.equals(source.auditRecord.label)) {
        throw new AssertionError("Round trip failed: " + protocol);
      }
      System.out.println(protocol + ": " + bytes.length + " bytes");
    }
  }
}
