// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
// Retain complete immutable lexical rules separately for every union branch.
type srlXmlRule struct {
  kind, whitespace, listKind string
  bounds [4]string
  length, minimum, maximum int
  enumeration []string
  finite bool
  patterns [][]srlXmlPattern
  members []srlXmlRule
  fixed string
  hasFixed bool
}
// Keep resource admission failures distinct from an ordinary rejected union alternative.
type srlXmlLimitError string
// Preserve the established public error diagnostics without wrapping operation-local state.
func (e srlXmlLimitError) Error()string{return string(e)}
// Recognize only XML whitespace when splitting an XSD lexical list.
func srlXmlSpace(value rune)bool{return value==' '||value=='\t'||value=='\r'||value=='\n'}
// Admit all list items before allocating token strings or native collection storage.
func srlXmlTokens(text string,maximum int)([]string,error){
  count:=0;item:=false
  for _,value:=range text{space:=srlXmlSpace(value);if !space&&!item{if count>=maximum{return nil,srlXmlLimitError("XML list item limit exceeded")};count++};item=!space}
  return strings.FieldsFunc(text,srlXmlSpace),nil
}
// Derive builtin scalar widths independently of native string storage.
func srlXmlKindBits(kind string)int{switch kind{case "byte","unsignedByte":return 8;case "short","unsignedShort":return 16;case "int","unsignedInt","float":return 32;case "long","unsignedLong","double":return 64};return 0}
// Identify the arbitrary-precision integer family without relying on host widths.
func srlXmlIntegerKind(kind string)bool{return kind=="integer"||kind=="positiveInteger"||kind=="negativeInteger"||kind=="nonPositiveInteger"||kind=="nonNegativeInteger"}
var srlXmlIntegerLexical=regexp.MustCompile(`^[+-]?[0-9]+$`)
var srlXmlDecimalLexical=regexp.MustCompile(`^[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)$`)
var srlXmlLanguageLexical=regexp.MustCompile(`^[A-Za-z]{1,8}(?:-[A-Za-z0-9]{1,8})*$`)
// Compare arbitrarily large decimal values using digits and scale, without numeric rounding.
func srlXmlDecimalCompare(left,right string)int{
  type parts struct{negative bool;whole,fraction string}
  parse:=func(text string)parts{
    result:=parts{negative:strings.HasPrefix(text,"-")};if strings.HasPrefix(text,"+")||result.negative{text=text[1:]}
    whole,fraction,found:=strings.Cut(text,".");if !found{fraction=""};result.whole=strings.TrimLeft(whole,"0");result.fraction=strings.TrimRight(fraction,"0")
    result.negative=result.negative&&(result.whole!=""||result.fraction!="");return result
  }
  a,b:=parse(left),parse(right);if a.negative!=b.negative{if a.negative{return -1};return 1};order:=0
  if len(a.whole)<len(b.whole){order=-1}else if len(a.whole)>len(b.whole){order=1}else{order=strings.Compare(a.whole,b.whole)}
  if order==0{for index:=0;index<len(a.fraction)||index<len(b.fraction);index++{first,second:=byte('0'),byte('0');if index<len(a.fraction){first=a.fraction[index]};if index<len(b.fraction){second=b.fraction[index]};if first<second{order=-1;break};if first>second{order=1;break}}}
  if a.negative{return -order};return order
}
// Apply XML1.0 NameStartChar and NameChar ranges to complete Unicode scalars.
func srlXmlNameCharacter(value rune,first,colon bool)bool{
  start:=value=='_'||colon&&value==':'||value>='A'&&value<='Z'||value>='a'&&value<='z'||value>=0xc0&&value<=0xd6||value>=0xd8&&value<=0xf6||value>=0xf8&&value<=0x2ff||value>=0x370&&value<=0x37d||value>=0x37f&&value<=0x1fff||value>=0x200c&&value<=0x200d||value>=0x2070&&value<=0x218f||value>=0x2c00&&value<=0x2fef||value>=0x3001&&value<=0xd7ff||value>=0xf900&&value<=0xfdcf||value>=0xfdf0&&value<=0xfffd||value>=0x10000&&value<=0xeffff
  return start||!first&&(value=='-'||value=='.'||value>='0'&&value<='9'||value==0xb7||value>=0x300&&value<=0x36f||value>=0x203f&&value<=0x2040)
}
// Reject builtin lexical/value-space errors before bound or enumeration comparison.
func (r *srlXmlRule) lexical(text string)error{
  bits:=srlXmlKindBits(r.kind)
  if strings.HasPrefix(r.kind,"unsigned"){_,err:=srlXmlUint(text,bits);return err}
  switch r.kind{
  case "byte","short","int","long":_,err:=srlXmlInt(text,bits);return err
  case "boolean":_,err:=srlXmlBool(text);return err
  case "float","double":_,err:=srlXmlDouble(text,bits);return err
  case "decimal":if !srlXmlDecimalLexical.MatchString(text){return fmt.Errorf("Invalid XML decimal")}
  case "language":if !srlXmlLanguageLexical.MatchString(text){return fmt.Errorf("Invalid XML language")}
  case "Name","NCName","NMTOKEN","ID","IDREF","ENTITY":
    if text==""{return fmt.Errorf("Empty XML name token")};for offset,value:=range text{if !srlXmlNameCharacter(value,offset==0&&r.kind!="NMTOKEN",r.kind=="Name"||r.kind=="NMTOKEN"){return fmt.Errorf("Invalid XML name token")}}
  default:
    if srlXmlIntegerKind(r.kind){if !srlXmlIntegerLexical.MatchString(text){return fmt.Errorf("Invalid XML integer")};sign:=srlXmlDecimalCompare(text,"0");if r.kind=="positiveInteger"&&sign<=0||r.kind=="negativeInteger"&&sign>=0||r.kind=="nonPositiveInteger"&&sign>0||r.kind=="nonNegativeInteger"&&sign<0{return fmt.Errorf("XML integer sign failed")}}
  };return nil
}
// Compare complete list item value spaces and exact scalar identities.
func (r *srlXmlRule) compare(left,right string)(int,bool,error){
  if r.listKind!=""{
    first,err:=srlXmlTokens(left,len(left));if err!=nil{return 0,false,err};second,err:=srlXmlTokens(right,len(right));if err!=nil{return 0,false,err};if len(first)!=len(second){return 1,true,nil}
    item:=srlXmlRule{kind:r.listKind};for index:=range first{order,comparable,err:=item.compare(first[index],second[index]);if err!=nil{return 0,false,err};if !comparable||order!=0{return 1,true,nil}};return 0,true,nil
  }
  if r.kind=="decimal"||srlXmlIntegerKind(r.kind){return srlXmlDecimalCompare(left,right),true,nil}
  return srlXmlCompare(left,right,r.kind,srlXmlKindBits(r.kind))
}
// Bind one operation-local native collection allowance and cumulative pattern work budget.
func (r *srlXmlRule) check(text string,maximumItems int,budgets ...*srlXmlBudget)(string,error){
  work:=srlXmlPatternWork{remaining:srlXmlPatternMaxWorkUnits};if len(budgets)!=0{work.budget=budgets[0]};return r.checkWith(text,maximumItems,&work)
}
// Preserve branch independence while sharing resource admission through recursive unions.
func (r *srlXmlRule) checkWith(text string,maximumItems int,work *srlXmlPatternWork)(string,error){
  if err:=work.spend(len(text));err!=nil{return "",err};if !utf8.ValidString(text){return "",fmt.Errorf("Invalid XML UTF-8")}
  text=srlXmlWhitespace(text,r.whitespace);if err:=r.lexical(text);err!=nil{return "",err}
  if len(r.members)!=0{matched:=false;for index:=range r.members{_,err:=r.members[index].checkWith(text,maximumItems,work);if err==nil{matched=true;break};if _,resource:=err.(srlXmlLimitError);resource{return "",err}};if !matched{return "",fmt.Errorf("XML union has no matching member")}}
  count:=utf8.RuneCountInString(text)
  if r.listKind!=""{items,err:=srlXmlTokens(text,maximumItems);if err!=nil{return "",err};count=len(items);item:=srlXmlRule{kind:r.listKind};for _,value:=range items{if err=work.spend(len(value));err!=nil{return "",err};if err=item.lexical(value);err!=nil{return "",err}}}
  if r.length>=0&&count!=r.length||r.minimum>=0&&count<r.minimum||r.maximum>=0&&count>r.maximum{return "",fmt.Errorf("XML length facet failed")}
  for index,bound:=range r.bounds{if bound==""{continue};order,comparable,err:=r.compare(text,bound);if err!=nil{return "",err};if !comparable||index==0&&order<0||index==1&&order>0||index==2&&order<=0||index==3&&order>=0{return "",fmt.Errorf("XML numeric facet failed")}}
  if len(r.enumeration)!=0{matched:=false;for _,allowed:=range r.enumeration{order,comparable,err:=r.compare(text,srlXmlWhitespace(allowed,r.whitespace));if err!=nil{return "",err};if comparable&&order==0{matched=true;break}};if !matched{return "",fmt.Errorf("XML enumeration facet failed")}}
  if r.hasFixed{order,comparable,err:=r.compare(text,srlXmlWhitespace(r.fixed,r.whitespace));if err!=nil{return "",err};if !comparable||order!=0{return "",fmt.Errorf("XML fixed value differs")}}
  if r.finite{value,err:=srlXmlDouble(text,srlXmlKindBits(r.kind));if err!=nil{return "",err};if math.IsNaN(value)||math.IsInf(value,0){return "",fmt.Errorf("XML finite facet failed")}}
  for _,group:=range r.patterns{matched:=false;for index:=range group{accept,err:=group[index].matches(text,work);if err!=nil{return "",err};if accept{matched=true;break}};if !matched{return "",fmt.Errorf("XML portable pattern facet failed")}}
  return text,nil
}
// Preserve a present normalized empty token without activating a nonempty element default.
func (r *srlXmlRule) output(text,fallback string,hasDefault bool,maximumItems int,budgets ...*srlXmlBudget)(string,error){
  text,err:=r.check(text,maximumItems,budgets...);if err!=nil{return "",err}
  if hasDefault&&text==""&&srlXmlWhitespace(fallback,r.whitespace)!=""{if r.whitespace=="collapse"{return " ",nil};return "",fmt.Errorf("Empty XML value would activate a different XSD default")};return text,nil
}
// Bound native joined list text before retaining another token or separator in its builder.
func (o *srlXmlOutput) listItem(text string,joined *strings.Builder,used int)(int,error){
  remaining:=o.budget.limits.MaxTextBytes-o.budget.text;separator:=0;if joined.Len()!=0{separator=1}
  if separator>remaining-used||len(text)>remaining-used-separator{return used,srlXmlLimitError("XML list text limit exceeded")}
  if separator!=0{joined.WriteByte(' ')};joined.WriteString(text);return used+separator+len(text),nil
}
)serializer"
