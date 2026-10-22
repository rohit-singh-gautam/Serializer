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
  qnames []srlxml.Name
  qnameLists [][]srlxml.Name
  finite bool
  patterns [][]srlXmlPattern
  members []srlXmlRule
  listItem *srlXmlRule
  fixed string
  hasFixed bool
  defaultText string
  defaultScope map[string]string
  hasDefault bool
  fixedNames []srlxml.Name
  hasFixedNames bool
  defaultNames []srlxml.Name
  defaultAttribute bool
  defaultContent bool
  keepNamespaceLexical bool
  unionValues []srlXmlUnionValue
  unionFixed *srlXmlUnionValue
}
// Store the selected primitive and schema-expanded names without retaining provider namespace maps.
type srlXmlUnionValue struct {kind,listKind,text string;names []srlxml.Name;items []srlXmlUnionValue}
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
  work:=srlXmlPatternWork{remaining:srlXmlPatternMaxWorkUnits};if len(budgets)!=0{work.budget=budgets[0]};checked,err:=r.checkWith(text,maximumItems,&work);if len(r.members)!=0{return text,err};return checked,err
}
// Preserve branch independence while sharing resource admission through recursive unions.
func (r *srlXmlRule) checkWith(text string,maximumItems int,work *srlXmlPatternWork)(string,error){
  if err:=work.spend(len(text));err!=nil{return "",err};if !utf8.ValidString(text){return "",fmt.Errorf("Invalid XML UTF-8")}
  if len(r.members)==0{text=srlXmlWhitespace(text,r.whitespace);if err:=r.lexical(text);err!=nil{return "",err}}
  selected:=r
  if len(r.members)!=0{matched:=false;for index:=range r.members{member,err:=r.members[index].checkWith(text,maximumItems,work);if err==nil{text=member;selected=work.selected;matched=true;break};if _,resource:=err.(srlXmlLimitError);resource{return "",err}};if !matched{return "",fmt.Errorf("XML union has no matching member")}}
  count:=utf8.RuneCountInString(text)
  if r.listKind!=""{items,err:=srlXmlTokens(text,maximumItems);if err!=nil{return "",err};count=len(items);item:=srlXmlRule{kind:r.listKind};for _,value:=range items{if r.listItem!=nil{if _,err=r.listItem.checkWith(value,maximumItems,work);err!=nil{return "",err}}else{if err=work.spend(len(value));err!=nil{return "",err};if err=item.lexical(value);err!=nil{return "",err}}}}
  if r.length>=0&&count!=r.length||r.minimum>=0&&count<r.minimum||r.maximum>=0&&count>r.maximum{return "",fmt.Errorf("XML length facet failed")}
  for index,bound:=range r.bounds{if bound==""{continue};order,comparable,err:=r.compare(text,bound);if err!=nil{return "",err};if !comparable||index==0&&order<0||index==1&&order>0||index==2&&order<=0||index==3&&order>=0{return "",fmt.Errorf("XML numeric facet failed")}}
  if len(r.enumeration)!=0&&len(r.qnames)==0&&len(r.qnameLists)==0{matched:=false;for index,allowed:=range r.enumeration{var equal bool;var err error;if len(r.unionValues)!=0{equal,err=r.constantEqual(text,selected,r.unionValues[index],maximumItems,work)}else if len(r.members)!=0{equal,err=r.unionEqual(text,selected,allowed,maximumItems,work)}else{var order int;var comparable bool;order,comparable,err=r.compare(text,srlXmlWhitespace(allowed,r.whitespace));equal=comparable&&order==0};if err!=nil{return "",err};if equal{matched=true;break}};if !matched{return "",fmt.Errorf("XML enumeration facet failed")}}
  if r.hasFixed&&!r.hasFixedNames{var equal bool;var err error;if r.unionFixed!=nil{equal,err=r.constantEqual(text,selected,*r.unionFixed,maximumItems,work)}else if len(r.members)!=0{equal,err=r.unionEqual(text,selected,r.fixed,maximumItems,work)}else{var order int;var comparable bool;order,comparable,err=r.compare(text,srlXmlWhitespace(r.fixed,r.whitespace));equal=comparable&&order==0};if err!=nil{return "",err};if !equal{return "",fmt.Errorf("XML fixed value differs")}}
  if work.namespace!=nil&&(r.kind=="QName"||r.listKind=="QName"){if err:=r.checkNames(text,maximumItems,work);err!=nil{return "",err}}
  if r.finite{value,err:=srlXmlDouble(text,srlXmlKindBits(r.kind));if err!=nil{return "",err};if math.IsNaN(value)||math.IsInf(value,0){return "",fmt.Errorf("XML finite facet failed")}}
  for _,group:=range r.patterns{matched:=false;for index:=range group{accept,err:=group[index].matches(text,work);if err!=nil{return "",err};if accept{matched=true;break}};if !matched{return "",fmt.Errorf("XML portable pattern facet failed")}}
  work.selected=selected;return text,nil
}
// Select the first complete union member while all alternatives share one bounded evaluator.
func (r *srlXmlRule) selectNamespace(text string,lookup func(string)(string,bool),maximum int,budget *srlXmlBudget)(string,*srlXmlRule,error){
  work:=srlXmlPatternWork{remaining:srlXmlPatternMaxWorkUnits,budget:budget,namespace:lookup}
  checked,err:=r.checkWith(text,maximum,&work);if len(r.members)!=0{checked=text};return checked,work.selected,err
}
// Compare primitive value families independently of authored union member names.
func (r *srlXmlRule) valueFamily()string{if r.listKind!=""{item:=srlXmlRule{kind:r.listKind};return "list:"+item.valueFamily()};if srlXmlKindBits(r.kind)!=0&&r.kind!="float"&&r.kind!="double"||srlXmlIntegerKind(r.kind)||r.kind=="decimal"{return "decimal"};switch r.kind{case "boolean","float","double","anyURI","QName":return r.kind};return "string"}
// Compare normalized identities without truncating arbitrary-precision decimal values.
func (r *srlXmlRule) identityEqual(first string,other *srlXmlRule,second string,maximum int,work *srlXmlPatternWork)(bool,error){
  if err:=work.spend(len(first)+len(second)+1);err!=nil{return false,err};if r.valueFamily()!=other.valueFamily(){return false,nil}
  if r.listKind!=""{left,err:=srlXmlTokens(first,maximum);if err!=nil{return false,err};right,err:=srlXmlTokens(second,maximum);if err!=nil{return false,err};if len(left)!=len(right){return false,nil};item,otherItem:=srlXmlRule{kind:r.listKind},srlXmlRule{kind:other.listKind};for index:=range left{equal,err:=item.identityEqual(left[index],&otherItem,right[index],maximum,work);if err!=nil||!equal{return false,err}};return true,nil}
  if r.valueFamily()=="decimal"{return srlXmlDecimalCompare(first,second)==0,nil};order,comparable,err:=r.compare(first,second);return comparable&&order==0,err
}
// Resolve every schema literal through the same first-successful-member rule as the instance.
func (r *srlXmlRule) unionEqual(text string,selected *srlXmlRule,allowed string,maximum int,work *srlXmlPatternWork)(bool,error){
  for index:=range r.members{expected,err:=r.members[index].checkWith(allowed,maximum,work);if err!=nil{if _,resource:=err.(srlXmlLimitError);resource{return false,err};continue};return selected.identityEqual(text,work.selected,expected,maximum,work)};return false,nil
}
// Compare the selected primitive family before resolving occurrence-local QName/list names.
func (r *srlXmlRule) constantEqual(text string,selected *srlXmlRule,expected srlXmlUnionValue,maximum int,work *srlXmlPatternWork)(bool,error){
  other:=srlXmlRule{kind:expected.kind,listKind:expected.listKind};if (selected.listKind=="")!=(expected.listKind==""){return false,nil}
  if selected.listKind!=""&&(len(expected.items)!=0||expected.text==""){values,err:=srlXmlTokens(text,maximum);if err!=nil{return false,err};if len(values)!=len(expected.items){return false,nil};builtin:=srlXmlRule{kind:selected.listKind,whitespace:"collapse",length:-1,minimum:-1,maximum:-1};item:=selected.listItem;if item==nil{item=&builtin};for index,token:=range values{value,err:=item.checkWith(token,maximum,work);if err!=nil{return false,err};equal,err:=r.constantEqual(value,work.selected,expected.items[index],maximum,work);if err!=nil||!equal{return false,err}};return true,nil}
  if selected.valueFamily()!=other.valueFamily(){return false,nil}
  if selected.kind!="QName"&&selected.listKind!="QName"{return selected.identityEqual(text,&other,expected.text,maximum,work)}
  if work.namespace==nil{return false,fmt.Errorf("Missing XML union namespace scope")};mode:=0;if selected.listKind=="QName"{mode=1};names,err:=srlXmlNamespaceNames(text,mode,maximum,work.namespace);if err!=nil{return false,err};if len(names)!=len(expected.names){return false,nil})serializer"
R"serializer(
  for index,name:=range names{if err=work.spend(len(name.Space)+len(name.Local)+1);err!=nil{return false,err};if name!=expected.names[index]{return false,nil}};return true,nil
}
// Bind an explicit private scope for ordinary caller values and return normalized lexical text.
func (r *srlXmlRule) checkScoped(text string,scope map[string]string,maximum int,budget *srlXmlBudget)(string,error){
  lookup:=func(prefix string)(string,bool){if prefix=="xml"{return "http://www.w3.org/XML/1998/namespace",true};uri,found:=scope[prefix];return uri,found||prefix==""}
  checked,_,err:=r.selectNamespace(text,lookup,maximum,budget);return checked,err
}
// Compare schema-owned expanded QName identities independently of source presentation aliases.
func (r *srlXmlRule) checkNames(checked string,maximum int,work *srlXmlPatternWork)error{
  budget:=work.budget
  // One encoding/xml.Name holds two native string descriptors, each with pointer and length words.
  const stringsPerName=2;const wordsPerString=2;const bitsPerByte=8
  const nameBytes=stringsPerName*wordsPerString*(strconv.IntSize/bitsPerByte)
  count,inside:=0,false;for _,value:=range checked{space:=srlXmlSpace(value);if !space&&!inside{count++};inside=!space}
  if count>int(^uint(0)>>1)/nameBytes{return srlXmlLimitError("XML QName identity allocation overflow")}
  if budget!=nil{if err:=budget.charge("allocation",int64(count*nameBytes));err!=nil{return err}}
  mode:=0;if r.listKind=="QName"{mode=1}
  names,err:=srlXmlNamespaceNames(checked,mode,maximum,work.namespace);if err!=nil{return err}
  equal:=func(expected []srlxml.Name)(bool,error){
    if budget!=nil{if err:=budget.charge("work",1);err!=nil{return false,err}}
    if len(names)!=len(expected){return false,nil}
    for index,name:=range names{if budget!=nil{if err:=budget.charge("work",int64(len(name.Space)+len(name.Local)+1));err!=nil{return false,err}};if name!=expected[index]{return false,nil}}
    return true,nil
  }
  if r.hasFixedNames{matched,failure:=equal(r.fixedNames);if failure!=nil{return failure};if !matched{return fmt.Errorf("XML fixed QName value differs")}}
  if len(r.qnames)==0&&len(r.qnameLists)==0{return nil}
  if mode==0{for _,allowed:=range r.qnames{matched,failure:=equal([]srlxml.Name{allowed});if failure!=nil{return failure};if matched{return nil}}}else{
    for _,allowed:=range r.qnameLists{matched,failure:=equal(allowed);if failure!=nil{return failure};if matched{return nil}}
  }
  return fmt.Errorf("XML QName enumeration facet failed")
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
