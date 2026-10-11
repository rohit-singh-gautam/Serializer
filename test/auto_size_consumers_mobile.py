# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise computed record spans in the Java, Swift and Kotlin SDKs."""


def consumer(language, retain=False):
    """Check nested spans, source immutability and structural validation in both policies."""
    sources = {
        "java": r'''import java.util.Arrays;
public final class Main {
  private static void check(boolean value){if(!value)throw new AssertionError("auto_size");}
  private static void bad(Runnable action){boolean rejected=false;try{action.run();}catch(IllegalArgumentException ex){rejected=true;}check(rejected);}
  public static void main(String[] args){
    for(var protocol:Schema.Protocol.values()){
      var value=new Schema.SizedEnvelope();value.encodedSize=444;value.payload.encodedSize=333;value.payload.text="crate";value.payload.samples=new int[]{1,2,3};
      var item=new Schema.SizedPayload();item.text="\u03c0";value.items=new Schema.SizedPayload[]{item};
      byte[] bytes=value.encode(protocol);check(value.encodedSize==444 && value.payload.encodedSize==333 && item.encodedSize==0);
      var decoded=Schema.SizedEnvelope.decode(bytes,protocol);check(decoded.tag==7 && decoded.payload.text.equals("crate") && decoded.items[0].text.equals("\u03c0"));
      if(protocol==Schema.Protocol.JSON){check(decoded.encodedSize==0 && decoded.payload.encodedSize==0);check(!new String(bytes,java.nio.charset.StandardCharsets.UTF_8).contains("encoded_size"));}
      else{check(decoded.encodedSize==bytes.length);check(Integer.toUnsignedLong(decoded.payload.encodedSize)==value.payload.encode(protocol).length);check(Integer.toUnsignedLong(decoded.items[0].encodedSize)==item.encode(protocol).length);}
      bad(()->Schema.SizedEnvelope.decode(Arrays.copyOf(bytes,bytes.length-1),protocol));
      byte[] trailing=Arrays.copyOf(bytes,bytes.length+1);bad(()->Schema.SizedEnvelope.decode(trailing,protocol));
      var split=new Schema.SplitSized();split.positionalSize=11;split.integerSize=22;split.stringSize=33;split.text="bin";byte[] splitBytes=split.encode(protocol);
      check(split.positionalSize==11 && split.integerSize==22 && split.stringSize==33);
      var splitDecoded=Schema.SplitSized.decode(splitBytes,protocol);
      check(splitDecoded.positionalSize==(protocol==Schema.Protocol.BINARY_NONE?splitBytes.length:0));check(splitDecoded.integerSize==(protocol==Schema.Protocol.BINARY_INTEGER?splitBytes.length:0));check(splitDecoded.stringSize==(protocol==Schema.Protocol.BINARY_STRING?splitBytes.length:0));
      var wire=new Schema.WireSized();wire.text="wire";byte[] wireBytes=wire.encode(protocol);check(Schema.WireSized.decode(wireBytes,protocol).text.equals("wire"));check(Arrays.stream(Schema.WireSized.class.getDeclaredFields()).noneMatch(f->f.getName().equals("encodedSize")));
      var scoped=new Schema.PayloadSized();scoped.encodedSize=777;scoped.text="body";byte[] scopedBytes=scoped.encode(protocol);check(scoped.encodedSize==777);var scopedDecoded=Schema.PayloadSized.decode(scopedBytes,protocol);
      if(protocol!=Schema.Protocol.JSON){int prefix=protocol==Schema.Protocol.BINARY_NONE?0:protocol==Schema.Protocol.BINARY_INTEGER?1:13;check(scopedDecoded.encodedSize==scopedBytes.length-prefix-4);long wireSize=0;for(int i=0;i<4;i++)wireSize|=(long)Byte.toUnsignedInt(wireBytes[prefix+i])<<(i*8);check(wireSize==wireBytes.length);
        if(protocol!=Schema.Protocol.BINARY_NONE){byte[] expanded=new byte[scopedBytes.length+1];expanded[0]=0x40;System.arraycopy(scopedBytes,0,expanded,1,scopedBytes.length);check(Schema.PayloadSized.decode(expanded,protocol).encodedSize==scopedDecoded.encodedSize);}
      }else{check(scopedDecoded.encodedSize==0);}
      var magic=new Schema.SizedMagic();magic.encodedSize=99;magic.text="parcel";byte[] magicBytes=magic.encode(protocol);check(magic.encodedSize==99);var magicDecoded=Schema.SizedMagic.decode(magicBytes,protocol);check(magicDecoded.encodedSize==(protocol==Schema.Protocol.JSON?0:magicBytes.length));
      if(protocol!=Schema.Protocol.JSON){magicBytes[0]^=1;bad(()->Schema.SizedMagic.decode(magicBytes,protocol));
        byte[] payload=value.payload.encode(protocol);int position=protocol==Schema.Protocol.BINARY_NONE?0:protocol==Schema.Protocol.BINARY_INTEGER?1:13;
        byte[] mismatch=payload.clone();Arrays.fill(mismatch,position,position+4,(byte)255);
        MISMATCH
        if(protocol!=Schema.Protocol.BINARY_NONE){int fieldLength=position+4;byte[] missing=Arrays.copyOfRange(payload,fieldLength,payload.length);bad(()->Schema.SizedPayload.decode(missing,protocol));
          byte[] duplicate=new byte[payload.length+fieldLength];System.arraycopy(payload,0,duplicate,0,fieldLength);System.arraycopy(payload,0,duplicate,fieldLength,payload.length);bad(()->Schema.SizedPayload.decode(duplicate,protocol));}
      }
    }
  }
}
''',
        "swift": r'''import Foundation
func bad(_ action: () throws -> Void){var rejected=false;do{try action()}catch{rejected=true};precondition(rejected)}
for protocolValue in [Protocol.JSON,.BINARY_NONE,.BINARY_INTEGER,.BINARY_STRING]{
  var value=SizedEnvelope();value.encodedSize=444;value.payload.encodedSize=333;value.payload.text="crate";value.payload.samples=[1,2,3];var item=SizedPayload();item.text="\u{03c0}";value.items=[item]
  let bytes=try value.encode(protocolValue);precondition(value.encodedSize==444 && value.payload.encodedSize==333 && item.encodedSize==0)
  let decoded=try SizedEnvelope.decode(bytes,protocolValue);precondition(decoded.tag==7 && decoded.payload.text=="crate" && decoded.items[0].text=="\u{03c0}")
  if protocolValue == .JSON{precondition(decoded.encodedSize==0 && decoded.payload.encodedSize==0);precondition(!String(decoding:bytes,as:UTF8.self).contains("encoded_size"))}
  else{precondition(decoded.encodedSize==UInt64(bytes.count));let nested=try value.payload.encode(protocolValue);let child=try item.encode(protocolValue);precondition(decoded.payload.encodedSize==UInt32(nested.count) && decoded.items[0].encodedSize==UInt32(child.count))}
  bad{_ = try SizedEnvelope.decode(Array(bytes.dropLast()),protocolValue)};bad{_ = try SizedEnvelope.decode(bytes+[0],protocolValue)}
  var split=SplitSized();split.positionalSize=11;split.integerSize=22;split.stringSize=33;split.text="bin";let splitBytes=try split.encode(protocolValue);precondition(split.positionalSize==11 && split.integerSize==22 && split.stringSize==33)
  let splitDecoded=try SplitSized.decode(splitBytes,protocolValue);precondition(splitDecoded.positionalSize==(protocolValue == .BINARY_NONE ? UInt32(splitBytes.count):0));precondition(splitDecoded.integerSize==(protocolValue == .BINARY_INTEGER ? UInt32(splitBytes.count):0));precondition(splitDecoded.stringSize==(protocolValue == .BINARY_STRING ? UInt64(splitBytes.count):0))
  var wire=WireSized();wire.text="wire";let wireBytes=try wire.encode(protocolValue);let wireDecoded=try WireSized.decode(wireBytes,protocolValue);precondition(wireDecoded.text=="wire");precondition(!Mirror(reflecting:wire).children.contains{$0.label=="encodedSize"})
  var scoped=PayloadSized();scoped.encodedSize=777;scoped.text="body";let scopedBytes=try scoped.encode(protocolValue);precondition(scoped.encodedSize==777);let scopedDecoded=try PayloadSized.decode(scopedBytes,protocolValue)
  if protocolValue != .JSON{let prefix=protocolValue == .BINARY_NONE ? 0:protocolValue == .BINARY_INTEGER ? 1:13;precondition(scopedDecoded.encodedSize==UInt32(scopedBytes.count-prefix-4));var wireSize:UInt64=0;for i in 0..<4{wireSize |= UInt64(wireBytes[prefix+i]) << (i*8)};precondition(wireSize==UInt64(wireBytes.count))
    if protocolValue != .BINARY_NONE{let expanded=try PayloadSized.decode([0x40]+scopedBytes,protocolValue);precondition(expanded.encodedSize==scopedDecoded.encodedSize)}
  }else{precondition(scopedDecoded.encodedSize==0)}
  var magic=SizedMagic();magic.encodedSize=99;magic.text="parcel";var magicBytes=try magic.encode(protocolValue);precondition(magic.encodedSize==99);let magicDecoded=try SizedMagic.decode(magicBytes,protocolValue);precondition(magicDecoded.encodedSize==(protocolValue == .JSON ? 0:UInt32(magicBytes.count)))
  if protocolValue != .JSON{magicBytes[0] ^= 1;bad{_ = try SizedMagic.decode(magicBytes,protocolValue)};let payload=try value.payload.encode(protocolValue);let position=protocolValue == .BINARY_NONE ? 0:protocolValue == .BINARY_INTEGER ? 1:13
    var mismatch=payload;for index in position..<position+4{mismatch[index]=255}
    MISMATCH
    if protocolValue != .BINARY_NONE{let fieldLength=position+4;bad{_ = try SizedPayload.decode(Array(payload.dropFirst(fieldLength)),protocolValue)};let duplicate=Array(payload.prefix(fieldLength))+payload;bad{_ = try SizedPayload.decode(duplicate,protocolValue)}}
  }
}
''',
        "kotlin": r'''@file:OptIn(kotlin.ExperimentalUnsignedTypes::class)
fun bad(action: () -> Unit){var rejected=false;try{action()}catch(ex:SerializerException){rejected=true};check(rejected)}
fun main(){
  for(protocol in Protocol.entries){
    val value=SizedEnvelope();value.encodedSize=444uL;value.payload.encodedSize=333u;value.payload.text="crate";value.payload.samples=uintArrayOf(1u,2u,3u);val item=SizedPayload();item.text="\u03c0";value.items=mutableListOf(item)
    val bytes=value.encode(protocol);check(value.encodedSize==444uL && value.payload.encodedSize==333u && item.encodedSize==0u)
    val decoded=SizedEnvelope.decode(bytes,protocol);check(decoded.tag==7u && decoded.payload.text=="crate" && decoded.items[0].text=="\u03c0")
    if(protocol==Protocol.JSON){check(decoded.encodedSize==0uL && decoded.payload.encodedSize==0u);check(!bytes.toString(Charsets.UTF_8).contains("encoded_size"))}
    else{check(decoded.encodedSize==bytes.size.toULong());check(decoded.payload.encodedSize==value.payload.encode(protocol).size.toUInt());check(decoded.items[0].encodedSize==item.encode(protocol).size.toUInt())}
    bad{SizedEnvelope.decode(bytes.copyOf(bytes.size-1),protocol)};bad{SizedEnvelope.decode(bytes+byteArrayOf(0),protocol)}
    val split=SplitSized();split.positionalSize=11u;split.integerSize=22u;split.stringSize=33uL;split.text="bin";val splitBytes=split.encode(protocol);check(split.positionalSize==11u && split.integerSize==22u && split.stringSize==33uL)
    val splitDecoded=SplitSized.decode(splitBytes,protocol);check(splitDecoded.positionalSize==(if(protocol==Protocol.BINARY_NONE) splitBytes.size.toUInt() else 0u));check(splitDecoded.integerSize==(if(protocol==Protocol.BINARY_INTEGER) splitBytes.size.toUInt() else 0u));check(splitDecoded.stringSize==(if(protocol==Protocol.BINARY_STRING) splitBytes.size.toULong() else 0uL))
    val wire=WireSized();wire.text="wire";val wireBytes=wire.encode(protocol);check(WireSized.decode(wireBytes,protocol).text=="wire");check(WireSized::class.java.declaredFields.none{it.name=="encodedSize"})
    val scoped=PayloadSized();scoped.encodedSize=777u;scoped.text="body";val scopedBytes=scoped.encode(protocol);check(scoped.encodedSize==777u);val scopedDecoded=PayloadSized.decode(scopedBytes,protocol)
    if(protocol!=Protocol.JSON){val prefix=if(protocol==Protocol.BINARY_NONE) 0 else if(protocol==Protocol.BINARY_INTEGER) 1 else 13;check(scopedDecoded.encodedSize==(scopedBytes.size-prefix-4).toUInt());var wireSize=0uL;for(i in 0 until 4)wireSize=wireSize or ((wireBytes[prefix+i].toInt() and 255).toULong() shl (i*8));check(wireSize==wireBytes.size.toULong())
      if(protocol!=Protocol.BINARY_NONE){check(PayloadSized.decode(byteArrayOf(0x40)+scopedBytes,protocol).encodedSize==scopedDecoded.encodedSize)}
    }else{check(scopedDecoded.encodedSize==0u)}
    val magic=SizedMagic();magic.encodedSize=99u;magic.text="parcel";val magicBytes=magic.encode(protocol);check(magic.encodedSize==99u);val magicDecoded=SizedMagic.decode(magicBytes,protocol);check(magicDecoded.encodedSize==(if(protocol==Protocol.JSON) 0u else magicBytes.size.toUInt()))
    if(protocol!=Protocol.JSON){magicBytes[0]=(magicBytes[0].toInt() xor 1).toByte();bad{SizedMagic.decode(magicBytes,protocol)};val payload=value.payload.encode(protocol);val position=if(protocol==Protocol.BINARY_NONE) 0 else if(protocol==Protocol.BINARY_INTEGER) 1 else 13
      val mismatch=payload.copyOf();for(index in position until position+4)mismatch[index]=(-1).toByte()
      MISMATCH
      if(protocol!=Protocol.BINARY_NONE){val fieldLength=position+4;bad{SizedPayload.decode(payload.copyOfRange(fieldLength,payload.size),protocol)};val duplicate=payload.copyOfRange(0,fieldLength)+payload;bad{SizedPayload.decode(duplicate,protocol)}}
    }
  }
}
''',
    }
    mismatch = {
        "java": "check(Integer.toUnsignedLong(Schema.SizedPayload.decode(mismatch,protocol).encodedSize)==0xffffffffL);" if retain else "bad(()->Schema.SizedPayload.decode(mismatch,protocol));",
        "swift": "let retained=try SizedPayload.decode(mismatch,protocolValue);precondition(retained.encodedSize==UInt32.max)" if retain else "bad{_ = try SizedPayload.decode(mismatch,protocolValue)}",
        "kotlin": "check(SizedPayload.decode(mismatch,protocol).encodedSize==UInt.MAX_VALUE)" if retain else "bad{SizedPayload.decode(mismatch,protocol)}",
    }
    return sources[language].replace("MISMATCH", mismatch[language])
