using System;
using System.Text;
using System.Collections.Generic;
using static SerializerGenerated.Schema;

internal static class CodecTest {
  // Require a rejected input rather than a partially decoded result.
  private static void Reject(Action action) {
    bool rejected = false;
    try { action(); } catch (FormatException) { rejected = true; }
    catch (ArgumentException) { rejected = true; }
    catch (System.Text.Json.JsonException) { rejected = true; }
    catch (InvalidOperationException) { rejected = true; }
    if (!rejected) throw new Exception("Malformed input accepted");
  }
  // Convert only test text; production codecs accept raw bytes.
  private static byte[] Utf8(string value) { return Encoding.UTF8.GetBytes(value); }
  // Assert values without a test-framework dependency.
  private static void Check(bool value) { if (!value) throw new Exception("Value mismatch"); }

  // Exercise all generated runtime branches with malformed input and boundary values.
  private static void Main() {
    foreach (Protocol protocol in Enum.GetValues<Protocol>()) {
      var value = new InteropMessage(); var bytes = value.Encode(protocol);
      Check(InteropMessage.Decode(bytes, protocol).Encode(protocol).AsSpan().SequenceEqual(bytes));
      for (int end = 0; end < bytes.Length; ++end) { int count = end; Reject(() => InteropMessage.Decode(bytes.AsMemory(0, count), protocol)); }
      var extra = new byte[bytes.Length + 1]; bytes.CopyTo(extra, 0); Reject(() => InteropMessage.Decode(extra, protocol));
      Reject(() => InteropMessage.Decode(bytes, protocol, new Limits(maxBytes: bytes.Length - 1)));
      Reject(() => InteropMessage.Decode(bytes, protocol, new Limits(maxDepth: 0)));
    }
    Reject(() => CheckBoolValue.Decode(new byte[] {2}, Protocol.BINARY_NONE));
    Reject(() => CheckBoolValue.Decode(Utf8("{\"value\":null}"), Protocol.JSON));
    foreach (var text in new[] {"{\"value\":01}", "{\"value\":-1}", "{\"value\":18446744073709551616}", "{\"value\":1e3}", "{\"value\":1,}", "{\"unknown\":1}"}) Reject(() => CheckIntegerValue.Decode(Utf8(text), Protocol.JSON));
    foreach (var text in new[] {"{\"value\":NaN}", "{\"value\":1e100}", "{\"value\":1e-100}", "{\"value\":+1}"}) Reject(() => CheckFloatValue.Decode(Utf8(text), Protocol.JSON));
    foreach (var text in new[] {"{\"value\":\"\\ud800\"}", "{\"value\":\"\\udc00\"}", "{\"value\":\"\\ud800x\"}", "{\"value\":\"x\n\"}"}) Reject(() => CheckTextValue.Decode(Utf8(text), Protocol.JSON));
    Reject(() => CheckTextValue.Decode(new byte[] {2, 0xc0, 0xaf}, Protocol.BINARY_NONE));
    Reject(() => CheckTextValue.Decode(Utf8("{\"value\":\"abcdef\"}"), Protocol.JSON, new Limits(maxStringBytes: 3)));
    Reject(() => CheckTextValue.Decode(Utf8("{\"value\":\"\\u0061\"}"), Protocol.JSON, new Limits(maxStringBytes: 3)));
    Reject(() => CheckBytesValue.Decode(new byte[] {255, 255, 255, 255}, Protocol.BINARY_NONE));
    Reject(() => CheckBytesValue.Decode(new byte[] {3, 1, 2}, Protocol.BINARY_NONE));
    Reject(() => CheckBytesValue.Decode(Utf8("{\"values\":[1,2,3]}"), Protocol.JSON, new Limits(maxElements: 2)));
    Reject(() => CheckBytesValue.Decode(Utf8("{\"values\":[1,]}"), Protocol.JSON));
    foreach (var text in new[] {"{\"payload:missing\":0}", "{\"states\":[\"missing\"]}", "{\"counts\":[{\"key\":\"a\"}]}", "{\"counts\":[{\"key\":\"a\",\"value\":1,\"extra\":0}]}"}) Reject(() => InteropMessage.Decode(Utf8(text), Protocol.JSON));
    Check(CheckIntegerValue.Decode(Utf8("{\"value\":18446744073709551615}"), Protocol.JSON).Value == ulong.MaxValue);
    Check(CheckTextValue.Decode(new byte[] {0x40, 0}, Protocol.BINARY_NONE).Value == "");
    Check(CheckTextValue.Decode(Utf8("{\"value\":\"\\ud83d\\ude80\"}"), Protocol.JSON).Value == "🚀");
    Check(CheckTextValue.Decode(new byte[] {3, 0xef, 0xbb, 0xbf}, Protocol.BINARY_NONE).Value == "\ufeff");
    var merged = InteropMessage.Decode(Utf8("{\"nested\":{\"code\":99},\"nested\":{\"note\":\"merged\"},\"numbers\":[1,2],\"numbers\":[3],\"counts\":[{\"value\":1,\"key\":\"a\"},{\"key\":\"a\",\"value\":2}]}"), Protocol.JSON);
    Check(merged.Nested.Code == 99 && merged.Nested.Note == "merged" && merged.Numbers.Count == 1 && merged.Numbers[0] == 3 && merged.Counts["a"] == 2);
    Check(InteropMessage.Decode(Utf8("{} \r\n\t"), Protocol.JSON).BigUnsigned == ulong.MaxValue);
    Check(new CheckDefaultValue().Escaped == "a\\b\n\"c");
    var cycle = new CheckRecursive(); cycle.Children.Add(cycle); Reject(() => cycle.Encode(Protocol.BINARY_NONE));
    var nested = new CheckRecursive(); nested.Children.Add(new CheckRecursive());
    Reject(() => CheckRecursive.Decode(nested.Encode(Protocol.BINARY_NONE), Protocol.BINARY_NONE, new Limits(maxDepth: 1)));
    var malformed = new CheckTextValue {Value = "\ud800"}; Reject(() => malformed.Encode(Protocol.JSON));
    Reject(() => CheckEmpty.Decode(Array.Empty<byte>(), (Protocol)99)); Reject(() => new Limits(-1));
    Console.WriteLine("C# codec boundary and rejection tests passed");
  }
}
