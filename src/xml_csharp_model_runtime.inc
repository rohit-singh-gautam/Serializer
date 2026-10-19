// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlmodel(
  // Bound each owning digest independently without changing earlier positional policies.
  public sealed class XmlDigestLimits {
    public int MaxBytes { get; }
    // Select shared default admission for every independently owned digest value.
    public XmlDigestLimits(int maxBytes=SrlXmlDefaultDigestBytes) { if(maxBytes<0)throw new System.ArgumentOutOfRangeException(nameof(maxBytes));MaxBytes=maxBytes; }
  }
  // Validate canonical revision syntax before exact native conversion and layout selection.
  private static string SrlXmlRevisionLexical(string raw,string kind) {
    if(kind.StartsWith("version",System.StringComparison.Ordinal)){SrlVersionParts(raw,kind[kind.Length-1]-'0');return raw;}
    string text=SrlXmlTrim(raw);
    if(!System.Text.RegularExpressions.Regex.IsMatch(text,@"\A(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\z"))throw new System.FormatException("Noncanonical XML revision");
    if(kind.StartsWith("uint",System.StringComparison.Ordinal)){foreach(char item in text)if(item<'0'||item>'9')throw new System.FormatException("Noncanonical XML integer revision");}
    else {double value=SrlXmlFloat(text,kind=="float");int end=text.IndexOfAny(new[]{'e','E'});string mantissa=end<0?text:text.Substring(0,end);bool nonzero=false;foreach(char item in mantissa)nonzero|=item>='1'&&item<='9';if(!double.IsFinite(value)||value==0&&nonzero)throw new System.FormatException("XML revision overflow or underflow");}
    return text;
  }
  // Convert only ASCII hexadecimal digits; all other code units remain invalid.
  private static int SrlXmlHexDigit(char value) => value>='0'&&value<='9'?value-'0':value>='a'&&value<='f'?value-'a'+10:value>='A'&&value<='F'?value-'A'+10:-1;
  // Validate complete lexical input before retaining owning candidate bytes.
  private static byte[] SrlXmlDigestRead(string text,int extent,XmlDigestLimits limits) {
    int size=text.Length/2;if((text.Length&1)!=0||extent!=0&&size!=extent)throw new System.FormatException("XML digest extent or hexadecimal pairs");
    if(size>limits.MaxBytes)throw new System.FormatException("XML digest byte limit");
    foreach(char item in text)if(SrlXmlHexDigit(item)<0)throw new System.FormatException("Invalid XML digest hexadecimal");
    byte[] result=new byte[size];for(int index=0;index<size;++index)result[index]=(byte)((SrlXmlHexDigit(text[index*2])<<4)|SrlXmlHexDigit(text[index*2+1]));return result;
  }
  // Export exact existing bytes as lowercase hexadecimal without calculating a digest.
  private static string SrlXmlDigestText(byte[] value,int extent,XmlDigestLimits limits) {
    if(value is null||extent!=0&&value.Length!=extent)throw new System.FormatException("XML digest extent");
    if(value.Length>limits.MaxBytes||value.Length>int.MaxValue/2)throw new System.FormatException("XML digest byte limit");
    char[] output=new char[value.Length*2];const string digits="0123456789abcdef";
    for(int index=0;index<value.Length;++index){byte item=value[index];output[index*2]=digits[item>>4];output[index*2+1]=digits[item&15];}return new string(output);
  }
)xmlmodel"
