import java.nio.charset.StandardCharsets;

/** Bind an owner field, write a scoped projection, then resolve a loaded address. */
public final class Main {
  /** Keep projection use inside the reference callback; only encoded bytes escape. */
  public static void main(String[] args) {
    var document = new Schema.Owner();
    document.b.text = "initial";
    document.b.count = 1;
    var observer = new Schema.Consumer();
    observer.d.bind(document, "document-1", 7, 1);

    // Shared binding retains document, and reads follow its current b field.
    document.b.text = "updated";
    document.b.count = 2;
    var output = observer.d.withValue(live ->
        live.withToWire(projected -> projected.encode(Schema.Protocol.JSON)));
    var value = Schema.ProjectedValue.decode(output, Schema.Protocol.JSON);
    if (!value.label.equals("updated") || value.count != 2) {
      throw new AssertionError("The projection must read the current owner field");
    }

    // A saved address contains identity, not a Java object reference.
    var loadedDocument = Schema.Owner.decode(document.encode(Schema.Protocol.JSON), Schema.Protocol.JSON);
    var loadedObserver = Schema.Consumer.decode(observer.encode(Schema.Protocol.JSON), Schema.Protocol.JSON);
    if (loadedObserver.d.isBound()) {
      throw new AssertionError("A decoded address must start unbound");
    }
    if (!loadedObserver.d.resolve(loadedDocument, "document-1", 7, 1)) {
      throw new AssertionError("The loaded address must resolve to its matching owner");
    }
    loadedObserver.d.withValue(live -> {
      if (!live.text.equals("updated")) {
        throw new AssertionError("Resolution must expose the loaded owner field");
      }
      return null;
    });

    System.out.println(new String(output, StandardCharsets.UTF_8));
  }
}
