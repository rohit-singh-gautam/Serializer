"""Real portable consumers of total record sizes and structural size checks."""


def consumer(language, retain=False):
    """Return an independent consumer for the selected decoder verification policy."""
    if language in ("javascript", "typescript"):
        declarations = (
            "const check = (value: boolean): void => { if (!value) throw Error('Size mismatch'); };\n"
            "const mustFail = (action: () => unknown): void => { let failed = false; try { action(); } catch { failed = true; } check(failed); };\n"
            if language == "typescript" else
            "const check = (value) => { if (!value) throw Error('Size mismatch'); };\n"
            "const mustFail = (action) => { let failed = false; try { action(); } catch { failed = true; } check(failed); };\n"
        )
        return ("import {SizedPayload, SizedEnvelope, SplitSized, SizedMagic, WireSized, PayloadSized, Protocol, Limits} from './schema.mjs';\n"
                + declarations + "const retain = " + str(retain).lower() + ";\n" + r'''
const value = new SizedPayload(); value.encodedSize = 123; value.text = 'parcel π'; value.samples = [0, 42, 4294967295];
for (const protocol of [Protocol.BINARY_NONE, Protocol.BINARY_INTEGER, Protocol.BINARY_STRING]) {
  const bytes = value.encode(protocol), restored = SizedPayload.decode(bytes, protocol);
  check(restored.encodedSize === bytes.length && restored.text === value.text && restored.samples[2] === 4294967295 && value.encodedSize === 123);
  const headerOffset = protocol === Protocol.BINARY_NONE ? 0 : protocol === Protocol.BINARY_INTEGER ? 1 : 13;
  const wrong = bytes.slice(); wrong.fill(0, headerOffset, headerOffset + 4);
  if (retain) check(SizedPayload.decode(wrong, protocol).encodedSize === 0); else mustFail(() => SizedPayload.decode(wrong, protocol));
  mustFail(() => SizedPayload.decode(bytes.subarray(0, bytes.length - 1), protocol));
  mustFail(() => SizedPayload.decode(bytes, protocol, new Limits(bytes.length - 1)));
  if (protocol !== Protocol.BINARY_NONE) {
    const headerLength = headerOffset + 4;
    mustFail(() => SizedPayload.decode(bytes.subarray(headerLength), protocol));
    const duplicate = new Uint8Array(bytes.length + headerLength);
    duplicate.set(bytes.subarray(0, headerLength)); duplicate.set(bytes, headerLength);
    mustFail(() => SizedPayload.decode(duplicate, protocol));
  }
  const child = new SizedPayload(); child.text = 'longer parcel';
  const outer = new SizedEnvelope(); outer.encodedSize = 18446744073709551615n; outer.payload = value; outer.items = [value, child];
  const outerBytes = outer.encode(protocol), outerResult = SizedEnvelope.decode(outerBytes, protocol);
  check(outerResult.encodedSize === BigInt(outerBytes.length) && outer.encodedSize === 18446744073709551615n);
  check(outerResult.payload.encodedSize === bytes.length && outerResult.items[1].encodedSize === child.encode(protocol).length);
  const split = new SplitSized(); split.text = 'split';
  const splitBytes = split.encode(protocol), splitResult = SplitSized.decode(splitBytes, protocol);
  check(protocol === Protocol.BINARY_NONE ? splitResult.positionalSize === splitBytes.length : protocol === Protocol.BINARY_INTEGER ? splitResult.integerSize === splitBytes.length : splitResult.stringSize === BigInt(splitBytes.length));
  const magic = new SizedMagic(); magic.text = 'prefix';
  const magicBytes = magic.encode(protocol), magicResult = SizedMagic.decode(magicBytes, protocol);
  check(magicResult.encodedSize === magicBytes.length && magicBytes[0] === 83);
  const wire = new WireSized(); wire.text = 'transport';
  check(!('encodedSize' in wire));
  const wireBytes = wire.encode(protocol); check(WireSized.decode(wireBytes, protocol).text === wire.text);
  const wireWrong = wireBytes.slice(); wireWrong.fill(0, headerOffset, headerOffset + 4);
  if (retain) check(WireSized.decode(wireWrong, protocol).text === wire.text); else mustFail(() => WireSized.decode(wireWrong, protocol));
  const payload = new PayloadSized(); payload.encodedSize = 123; payload.text = 'payload';
  const payloadBytes = payload.encode(protocol), payloadResult = PayloadSized.decode(payloadBytes, protocol);
  check(payloadResult.encodedSize === payloadBytes.length - headerOffset - 4 && payload.encodedSize === 123);
  if (protocol !== Protocol.BINARY_NONE) {
    const wideKey = new Uint8Array(payloadBytes.length + 1); wideKey[0] = 0x40; wideKey.set(payloadBytes, 1);
    check(PayloadSized.decode(wideKey, protocol).encodedSize === payloadResult.encodedSize);
  }
}
const json = value.encode(Protocol.JSON); check(!new TextDecoder().decode(json).includes('encoded_size'));
check(SizedPayload.decode(json, Protocol.JSON).encodedSize === 0 && value.encodedSize === 123);
''')
    if language == "go":
        return ("package main\nimport \"fmt\"\nconst retain = " + str(retain).lower() + "\n" + r'''
// check reports a consumer assertion with no silent SDK skip.
func check(value bool) { if !value { panic("Size mismatch") } }
// main exercises the public generated APIs with actual wire lengths.
func main() {
  value := NewSizedPayload(); value.EncodedSize = 123; value.Text = "parcel π"; value.Samples = []uint32{0, 42, 4294967295}
  for _, protocol := range []Protocol{BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
    bytes, err := value.Encode(protocol); if err != nil { panic(err) }; restored, err := DecodeSizedPayload(bytes, protocol); if err != nil { panic(err) }
    check(restored.EncodedSize == uint32(len(bytes)) && restored.Text == value.Text && restored.Samples[2] == 4294967295 && value.EncodedSize == 123)
    headerOffset := 0; if protocol == BINARY_INTEGER { headerOffset = 1 } else if protocol == BINARY_STRING { headerOffset = 13 }
    wrong := append([]byte(nil), bytes...); for index := headerOffset; index < headerOffset + 4; index++ { wrong[index] = 0 }
    wrongResult, wrongErr := DecodeSizedPayload(wrong, protocol); if retain { check(wrongErr == nil && wrongResult.EncodedSize == 0) } else { check(wrongErr != nil) }
    _, err = DecodeSizedPayload(bytes[:len(bytes)-1], protocol); check(err != nil)
    limits := DefaultLimits(); limits.MaxBytes = len(bytes)-1; _, err = DecodeSizedPayload(bytes, protocol, limits); check(err != nil)
    if protocol != BINARY_NONE {
      headerLength := headerOffset + 4; _, err = DecodeSizedPayload(bytes[headerLength:], protocol); check(err != nil)
      duplicate := append([]byte(nil), bytes[:headerLength]...); duplicate = append(duplicate, bytes...); _, err = DecodeSizedPayload(duplicate, protocol); check(err != nil)
    }
    child := NewSizedPayload(); child.Text = "longer parcel"; childBytes, err := child.Encode(protocol); if err != nil { panic(err) }
    outer := NewSizedEnvelope(); outer.EncodedSize = ^uint64(0); outer.Payload = value; outer.Items = []*SizedPayload{value, child}
    outerBytes, err := outer.Encode(protocol); if err != nil { panic(err) }; outerResult, err := DecodeSizedEnvelope(outerBytes, protocol); if err != nil { panic(err) }
    check(outerResult.EncodedSize == uint64(len(outerBytes)) && outer.EncodedSize == ^uint64(0) && outerResult.Payload.EncodedSize == uint32(len(bytes)) && outerResult.Items[1].EncodedSize == uint32(len(childBytes)))
    split := NewSplitSized(); split.Text = "split"; splitBytes, err := split.Encode(protocol); if err != nil { panic(err) }; splitResult, err := DecodeSplitSized(splitBytes, protocol); if err != nil { panic(err) }
    if protocol == BINARY_NONE { check(splitResult.PositionalSize == uint32(len(splitBytes))) } else if protocol == BINARY_INTEGER { check(splitResult.IntegerSize == uint32(len(splitBytes))) } else { check(splitResult.StringSize == uint64(len(splitBytes))) }
    magic := NewSizedMagic(); magic.Text = "prefix"; magicBytes, err := magic.Encode(protocol); if err != nil { panic(err) }; magicResult, err := DecodeSizedMagic(magicBytes, protocol); if err != nil { panic(err) }; check(magicResult.EncodedSize == uint32(len(magicBytes)) && magicBytes[0] == 'S')
    wire := NewWireSized(); wire.Text = "transport"; wireBytes, err := wire.Encode(protocol); if err != nil { panic(err) }; wireResult, err := DecodeWireSized(wireBytes, protocol); if err != nil { panic(err) }; check(wireResult.Text == wire.Text)
    wireWrong := append([]byte(nil), wireBytes...); for index := headerOffset; index < headerOffset+4; index++ { wireWrong[index] = 0 }; _, err = DecodeWireSized(wireWrong, protocol); check((err == nil) == retain)
    payload := NewPayloadSized(); payload.EncodedSize = 123; payload.Text = "payload"; payloadBytes, err := payload.Encode(protocol); if err != nil { panic(err) }; payloadResult, err := DecodePayloadSized(payloadBytes, protocol); if err != nil { panic(err) }; check(payloadResult.EncodedSize == uint32(len(payloadBytes)-headerOffset-4) && payload.EncodedSize == 123)
    if protocol != BINARY_NONE { wideKey := append([]byte{0x40}, payloadBytes...); wideResult, err := DecodePayloadSized(wideKey, protocol); if err != nil { panic(err) }; check(wideResult.EncodedSize == payloadResult.EncodedSize) }
  }
  json, err := value.Encode(JSON); if err != nil { panic(err) }; restored, err := DecodeSizedPayload(json, JSON); if err != nil { panic(err) }; check(restored.EncodedSize == 0 && value.EncodedSize == 123)
  fmt.Println("Derived size examples passed")
}
''')
    if language == "csharp":
        return ("using System;\nusing static SerializerGenerated.Schema;\ninternal static class Program {\n"
                + "  private static readonly bool Retain = " + str(retain).lower() + ";\n" + r'''
  // Check one consumer assertion against the real generated assembly.
  private static void Check(bool value) { if (!value) throw new Exception("Size mismatch"); }
  // Verify malformed input fails under both decoder policies where structurally required.
  private static void MustFail(Action action) { bool failed = false; try { action(); } catch { failed = true; } Check(failed); }
  // Exercise total bytes, nested spans and source preservation through public APIs.
  private static void Main() {
    var value = new SizedPayload { EncodedSize = 123, Text = "parcel π", Samples = new(){0,42,uint.MaxValue} };
    foreach (var protocol in new[]{Protocol.BINARY_NONE, Protocol.BINARY_INTEGER, Protocol.BINARY_STRING}) {
      var bytes = value.Encode(protocol); var restored = SizedPayload.Decode(bytes, protocol);
      Check(restored.EncodedSize == bytes.Length && restored.Text == value.Text && restored.Samples[2] == uint.MaxValue && value.EncodedSize == 123);
      int headerOffset = protocol == Protocol.BINARY_NONE ? 0 : protocol == Protocol.BINARY_INTEGER ? 1 : 13;
      var wrong = (byte[])bytes.Clone(); Array.Clear(wrong, headerOffset, 4);
      if (Retain) Check(SizedPayload.Decode(wrong, protocol).EncodedSize == 0); else MustFail(() => SizedPayload.Decode(wrong, protocol));
      MustFail(() => SizedPayload.Decode(bytes.AsMemory(0, bytes.Length-1), protocol));
      MustFail(() => SizedPayload.Decode(bytes, protocol, new Limits(maxBytes: bytes.Length-1)));
      if (protocol != Protocol.BINARY_NONE) {
        int headerLength = headerOffset + 4; MustFail(() => SizedPayload.Decode(bytes.AsMemory(headerLength), protocol));
        var duplicate = new byte[bytes.Length + headerLength]; bytes.AsSpan(0, headerLength).CopyTo(duplicate); bytes.AsSpan().CopyTo(duplicate.AsSpan(headerLength));
        MustFail(() => SizedPayload.Decode(duplicate, protocol));
      }
      var child = new SizedPayload { Text = "longer parcel" };
      var outer = new SizedEnvelope { EncodedSize = ulong.MaxValue, Payload = value, Items = new(){value,child} };
      var outerBytes = outer.Encode(protocol); var outerResult = SizedEnvelope.Decode(outerBytes, protocol);
      Check(outerResult.EncodedSize == (ulong)outerBytes.Length && outer.EncodedSize == ulong.MaxValue && outerResult.Payload.EncodedSize == bytes.Length && outerResult.Items[1].EncodedSize == child.Encode(protocol).Length);
      var split = new SplitSized { Text = "split" }; var splitBytes = split.Encode(protocol); var splitResult = SplitSized.Decode(splitBytes, protocol);
      Check(protocol == Protocol.BINARY_NONE ? splitResult.PositionalSize == splitBytes.Length : protocol == Protocol.BINARY_INTEGER ? splitResult.IntegerSize == splitBytes.Length : splitResult.StringSize == (ulong)splitBytes.Length);
      var magic = new SizedMagic { Text = "prefix" }; var magicBytes = magic.Encode(protocol); var magicResult = SizedMagic.Decode(magicBytes, protocol); Check(magicResult.EncodedSize == magicBytes.Length && magicBytes[0] == (byte)'S');
      var wire = new WireSized { Text = "transport" }; Check(typeof(WireSized).GetField("EncodedSize") is null);
      var wireBytes = wire.Encode(protocol); Check(WireSized.Decode(wireBytes, protocol).Text == wire.Text);
      var wireWrong = (byte[])wireBytes.Clone(); Array.Clear(wireWrong, headerOffset, 4);
      if (Retain) Check(WireSized.Decode(wireWrong, protocol).Text == wire.Text); else MustFail(() => WireSized.Decode(wireWrong, protocol));
      var payload = new PayloadSized { EncodedSize = 123, Text = "payload" }; var payloadBytes = payload.Encode(protocol);
      Check(PayloadSized.Decode(payloadBytes, protocol).EncodedSize == payloadBytes.Length-headerOffset-4 && payload.EncodedSize == 123);
      if (protocol != Protocol.BINARY_NONE) {
        var wideKey = new byte[payloadBytes.Length+1]; wideKey[0] = 0x40; payloadBytes.CopyTo(wideKey,1);
        Check(PayloadSized.Decode(wideKey,protocol).EncodedSize == PayloadSized.Decode(payloadBytes,protocol).EncodedSize);
      }
    }
    var json = value.Encode(Protocol.JSON); Check(!System.Text.Encoding.UTF8.GetString(json).Contains("encoded_size", StringComparison.Ordinal));
    Check(SizedPayload.Decode(json, Protocol.JSON).EncodedSize == 0 && value.EncodedSize == 123);
  }
}
''')
    raise ValueError(f"Unsupported portable size consumer: {language}")
