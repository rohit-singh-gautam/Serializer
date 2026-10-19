// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlmodel(
// XmlDigestLimits bounds each owning digest without changing earlier positional limit structs.
type XmlDigestLimits struct { MaxBytes int }
// DefaultXmlDigestLimits selects the shared per-digest byte admission.
func DefaultXmlDigestLimits()XmlDigestLimits{return XmlDigestLimits{srlXmlDefaultDigestBytes}}
// Reject negative digest policy before any graph traversal or candidate allocation.
func srlXmlDigestPolicy(limits XmlDigestLimits)error{if limits.MaxBytes<0{return fmt.Errorf("Negative XML digest limit")};return nil}
// Validate canonical revision grammar before exact native range conversion and layout selection.
func srlXmlRevisionLexical(raw,kind string)(string,error){
  if strings.HasPrefix(kind,"version"){if _,valid:=srlVersionParts(raw,int(kind[len(kind)-1]-'0'));!valid{return "",fmt.Errorf("Invalid XML dotted revision")};return raw,nil}
  text:=strings.Trim(raw," \t\r\n");if !srlXmlRevisionNumber.MatchString(text){return "",fmt.Errorf("Noncanonical XML revision")}
  if strings.HasPrefix(kind,"uint"){for _,item:=range text{if item<'0'||item>'9'{return "",fmt.Errorf("Noncanonical XML integer revision")}}}else{
    bits:=64;if kind=="float"{bits=32};value,err:=srlXmlDouble(text,bits);if err!=nil{return "",err};mantissa:=text;if end:=strings.IndexAny(text,"eE");end>=0{mantissa=text[:end]};if math.IsInf(value,0)||math.IsNaN(value)||value==0&&strings.ContainsAny(mantissa,"123456789"){return "",fmt.Errorf("XML revision overflow or underflow")}
  };return text,nil
}
var srlXmlRevisionNumber=regexp.MustCompile(`\A(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\z`)
// Convert only strict ASCII hexadecimal digits.
func srlXmlHexDigit(value byte)int{if value>='0'&&value<='9'{return int(value-'0')};if value>='a'&&value<='f'{return int(value-'a'+10)};if value>='A'&&value<='F'{return int(value-'A'+10)};return -1}
// Admit exact extent and every lexical pair before allocating private owning digest bytes.
func srlXmlDigestRead(text string,extent int,limits XmlDigestLimits)([]byte,error){
  size:=len(text)/2;if len(text)%2!=0||extent!=0&&size!=extent{return nil,fmt.Errorf("XML digest extent or hexadecimal pairs")};if size>limits.MaxBytes{return nil,fmt.Errorf("XML digest byte limit")}
  for index:=0;index<len(text);index++{if srlXmlHexDigit(text[index])<0{return nil,fmt.Errorf("Invalid XML digest hexadecimal")}}
  result:=make([]byte,size);for index:=range result{result[index]=byte(srlXmlHexDigit(text[index*2])<<4|srlXmlHexDigit(text[index*2+1]))};return result,nil
}
// Project exact native bytes as lowercase hexadecimal without hashing or changing their ownership.
func srlXmlDigestText(value []byte,extent int,limits XmlDigestLimits)(string,error){
  if extent!=0&&len(value)!=extent{return "",fmt.Errorf("XML digest extent")};if len(value)>limits.MaxBytes||len(value)>int(^uint(0)>>1)/2{return "",fmt.Errorf("XML digest byte limit")}
  const digits="0123456789abcdef";output:=make([]byte,len(value)*2);for index,item:=range value{output[index*2]=digits[item>>4];output[index*2+1]=digits[item&15]};return string(output),nil
}
)xmlmodel"
