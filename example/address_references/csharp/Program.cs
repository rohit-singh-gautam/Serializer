using System;
using System.Text;
using static SerializerGenerated.Schema;

internal static class Program {
  // Write a target projection through a shared reference, then reload its address.
  private static void Main() {
    var document = new Owner();
    document.B.Text = "initial";
    document.B.Count = 1;
    var observer = new Consumer();
    observer.D.Bind(document, "document-1", 7, 1);

    // The shared reference retains document; it observes the current B field.
    document.B.Text = "updated";
    document.B.Count = 2;
    var output = observer.D.WithValue(live =>
      live.WithToWire(projected => projected.Encode(Protocol.JSON)));
    var value = ProjectedValue.Decode(output, Protocol.JSON);
    if (value.Label != "updated" || value.Count != 2) {
      throw new Exception("The projection must read the current owner field");
    }

    // Load owners before resolving reference addresses from a separate payload.
    var loadedDocument = Owner.Decode(document.Encode(Protocol.JSON), Protocol.JSON);
    var loadedObserver = Consumer.Decode(observer.Encode(Protocol.JSON), Protocol.JSON);
    if (loadedObserver.D.IsBound()) {
      throw new Exception("A decoded address must start unbound");
    }
    if (!loadedObserver.D.Resolve(loadedDocument, "document-1", 7, 1)) {
      throw new Exception("The loaded address must resolve to its matching owner");
    }
    loadedObserver.D.WithValue(live => {
      if (live.Text != "updated") {
        throw new Exception("Resolution must expose the loaded owner field");
      }
      return 0;
    });

    Console.WriteLine(Encoding.UTF8.GetString(output));
  }
}
