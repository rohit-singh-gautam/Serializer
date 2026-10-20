// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlmodel(
/** Bound each owning digest without changing existing document or decoder policies. */
public static final class XmlDigestLimits {
  public final int maxBytes;
  /** Use the shared default byte admission independently for every digest. */
  public XmlDigestLimits() { this(srlXmlDefaultDigestBytes); }
  /** Reject negative policy before allocating a candidate or output buffer. */
  public XmlDigestLimits(int maxBytes) { if(maxBytes<0)throw new IllegalArgumentException("Negative XML digest limit");this.maxBytes=maxBytes; }
}
/** Validate canonical native revision grammar before conversion and historical layout selection. */
private static String srlXmlRevisionLexical(String raw,String kind) {
  if(kind.startsWith("version")){srlVersionParts(raw,kind.charAt(kind.length()-1)-'0');return raw;}
  String text=srlXmlTrim(raw);
  if(!text.matches("(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?"))throw new IllegalArgumentException("Noncanonical XML revision");
  if(kind.startsWith("uint")){if(!text.matches("[0-9]+"))throw new IllegalArgumentException("Noncanonical XML integer revision");}
  else {double value=srlXmlFloat(text,kind.equals("float"));int end=text.indexOf('e');if(end<0)end=text.indexOf('E');String mantissa=end<0?text:text.substring(0,end);if(!Double.isFinite(value)||value==0&&mantissa.matches(".*[1-9].*"))throw new IllegalArgumentException("XML revision overflow or underflow");}
  return text;
}
/** Convert only strict ASCII hexadecimal digits. */
private static int srlXmlHexDigit(char value) {return value>='0'&&value<='9'?value-'0':value>='a'&&value<='f'?value-'a'+10:value>='A'&&value<='F'?value-'A'+10:-1;}
/** Validate pairs, extent and admission before allocating private owning digest bytes. */
private static byte[] srlXmlDigestRead(String text,int extent,XmlDigestLimits limits) {
  int size=text.length()/2;if((text.length()&1)!=0||extent!=0&&size!=extent)throw new IllegalArgumentException("XML digest extent or hexadecimal pairs");
  if(size>limits.maxBytes)throw SrlXmlRule.quota("XML digest byte limit");
  for(int index=0;index<text.length();++index)if(srlXmlHexDigit(text.charAt(index))<0)throw new IllegalArgumentException("Invalid XML digest hexadecimal");
  byte[] result=new byte[size];for(int index=0;index<size;++index)result[index]=(byte)((srlXmlHexDigit(text.charAt(index*2))<<4)|srlXmlHexDigit(text.charAt(index*2+1)));return result;
}
/** Project exact native bytes as lowercase hexadecimal without hashing or model mutation. */
private static String srlXmlDigestText(byte[] value,int extent,XmlDigestLimits limits) {
  if(value==null||extent!=0&&value.length!=extent)throw new IllegalArgumentException("XML digest extent");
  if(value.length>limits.maxBytes||value.length>Integer.MAX_VALUE/2)throw SrlXmlRule.quota("XML digest byte limit");
  char[] output=new char[value.length*2];String digits="0123456789abcdef";
  for(int index=0;index<value.length;++index){int item=Byte.toUnsignedInt(value[index]);output[index*2]=digits.charAt(item>>>4);output[index*2+1]=digits.charAt(item&15);}return new String(output);
}
)xmlmodel"
