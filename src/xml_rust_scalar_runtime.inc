// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustscalar(
/// One immutable compiler-owned Unicode transition; no target regex dialect is involved.
struct SrlXmlPatternState { epsilon:&'static [usize], ranges:&'static [(u32,u32)], next:usize, flags:u8 }
/// A prepared bounded Thompson program shared by all operations on one generated field.
struct SrlXmlPattern { start:usize, states:&'static [SrlXmlPatternState] }
/// Complete scalar restrictions retain independent union branches and inherited OR-pattern groups.
struct SrlXmlRule { kind:&'static str, whitespace:&'static str, bounds:[&'static str;4],
 length:Option<usize>, minimum:Option<usize>, maximum:Option<usize>, enumeration:&'static [&'static str],
 finite:bool, groups:&'static [&'static [SrlXmlPattern]], list_kind:&'static str, members:&'static [SrlXmlRule], fixed:Option<&'static str> }
/// Preserve resource failures across union alternatives and inherited pattern groups.
struct SrlXmlScalarWork { remaining:usize, max_items:usize }
impl SrlXmlScalarWork {
 /// Admit work before a scan, allocation or instruction visit.
 fn spend(&mut self,units:usize)->Result<(),Error> { if units>self.remaining {return Err(Error("XML scalar work limit"));} self.remaining-=units;Ok(()) }
}
/// Recognize only the four XML whitespace characters rather than Unicode separator categories.
fn srl_xml_scalar_space(value:char)->bool {matches!(value,' '| '\t'|'\r'|'\n')}
/// Count list items before allocating either lexical slices or native collection storage.
fn srl_xml_scalar_tokens(text:&str,maximum:usize)->Result<Vec<&str>,Error> {
 let mut count=0;let mut item=false;
 for value in text.chars(){let space=srl_xml_scalar_space(value);if !space&&!item{if count>=maximum{return Err(Error("XML list item limit"));}count+=1;}item=!space;}
 let mut result=Vec::new();result.try_reserve_exact(count).map_err(|_|Error("XML scalar allocation"))?;
 result.extend(text.split(srl_xml_scalar_space).filter(|item|!item.is_empty()));Ok(result)
}
/// Apply XML whitespace replacement/collapse with bounded allocation and no native-value mutation.
fn srl_xml_scalar_normalize(raw:&str,mode:&str)->Result<String,Error>{
 let mut result=String::new();result.try_reserve_exact(raw.len()).map_err(|_|Error("XML scalar allocation"))?;
 if mode!="replace"&&mode!="collapse"{result.push_str(raw);return Ok(result);}
 let mut pending=false;
 for value in raw.chars(){let value=if srl_xml_scalar_space(value){' '}else{value};
  if mode=="collapse"&&value==' '{pending=!result.is_empty();continue;}
  if pending{result.push(' ');pending=false;}result.push(value);
 }Ok(result)
}
/// Compare arbitrary decimal values by sign and decimal digits, without binary rounding.
fn srl_xml_decimal_compare(left:&str,right:&str)->std::cmp::Ordering{
 fn parts(raw:&str)->(bool,&str,&str){let negative=raw.starts_with('-');let raw=raw.strip_prefix(['+','-']).unwrap_or(raw);let (whole,fraction)=raw.split_once('.').unwrap_or((raw,""));let whole=whole.trim_start_matches('0');let fraction=fraction.trim_end_matches('0');(negative&&(!whole.is_empty()||!fraction.is_empty()),whole,fraction)}
 let (negative,a,af)=parts(left);let (other,b,bf)=parts(right);
 if negative!=other{return if negative{std::cmp::Ordering::Less}else{std::cmp::Ordering::Greater};}
 let mut order=a.len().cmp(&b.len()).then_with(||a.cmp(b));
 if order==std::cmp::Ordering::Equal{let a=af.as_bytes();let b=bf.as_bytes();for index in 0..a.len().max(b.len()){order=a.get(index).unwrap_or(&b'0').cmp(b.get(index).unwrap_or(&b'0'));if !order.is_eq(){break;}}}
 if negative{order.reverse()}else{order}
}
/// Require complete ASCII integer or decimal lexical grammar before exact comparison.
fn srl_xml_scalar_digits(text:&str,decimal:bool)->bool{
 let text=text.strip_prefix(['+','-']).unwrap_or(text);let mut digits=0;let mut dot=false;
 for value in text.bytes(){if value.is_ascii_digit(){digits+=1;}else if decimal&&value==b'.'&&!dot{dot=true;}else{return false;}}
 digits!=0
}
/// Apply XML 1.0 fifth-edition name ranges to Unicode scalars.
fn srl_xml_scalar_name_character(code:u32,first:bool,colon:bool)->bool{
 let start=code==0x5f||colon&&code==0x3a||matches!(code,0x41..=0x5a|0x61..=0x7a|0xc0..=0xd6|0xd8..=0xf6|0xf8..=0x2ff|0x370..=0x37d|0x37f..=0x1fff|0x200c..=0x200d|0x2070..=0x218f|0x2c00..=0x2fef|0x3001..=0xd7ff|0xf900..=0xfdcf|0xfdf0..=0xfffd|0x10000..=0xeffff);
 start||!first&&matches!(code,0x2d|0x2e|0xb7|0x30..=0x39|0x300..=0x36f|0x203f..=0x2040)
}
/// Round a validated XML float directly to binary32, avoiding a binary64 midpoint detour.
fn srl_xml_float32(text:&str)->Result<f32,Error>{
 let parsed=srl_xml_float(text)?;if !parsed.is_finite(){return Ok(parsed as f32);}
 let result=text.trim_matches(srl_xml_scalar_space).parse::<f32>().map_err(|_|Error("Invalid XML binary32"))?;
 if !result.is_finite(){return Err(Error("XML binary32 overflow"));}Ok(result)
}
impl SrlXmlRule {
 /// Identify exact signed/unsigned builtin and arbitrary-width integer value spaces.
 fn integer_kind(&self)->bool{matches!(self.kind,"byte"|"unsignedByte"|"short"|"unsignedShort"|"int"|"unsignedInt"|"long"|"unsignedLong"|"integer"|"positiveInteger"|"negativeInteger"|"nonPositiveInteger"|"nonNegativeInteger")}
 /// Check lexical grammar and builtin value bounds independently of application storage.
 fn lexical(&self,text:&str)->Result<(),Error>{
  if self.integer_kind(){
   if !srl_xml_scalar_digits(text,false){return Err(Error("Invalid XML integer"));}
   let bounds=match self.kind{"byte"=>Some(("-128","127")),"unsignedByte"=>Some(("0","255")),"short"=>Some(("-32768","32767")),"unsignedShort"=>Some(("0","65535")),"int"=>Some(("-2147483648","2147483647")),"unsignedInt"=>Some(("0","4294967295")),"long"=>Some(("-9223372036854775808","9223372036854775807")),"unsignedLong"=>Some(("0","18446744073709551615")),_=>None};
   if let Some((minimum,maximum))=bounds{if srl_xml_decimal_compare(text,minimum).is_lt()||srl_xml_decimal_compare(text,maximum).is_gt(){return Err(Error("XML integer builtin range"));}}
   let sign=srl_xml_decimal_compare(text,"0");if self.kind=="positiveInteger"&&!sign.is_gt()||self.kind=="negativeInteger"&&!sign.is_lt()||self.kind=="nonPositiveInteger"&&sign.is_gt()||self.kind=="nonNegativeInteger"&&sign.is_lt(){return Err(Error("XML integer sign"));}
  }else{match self.kind{
   "decimal"=>if !srl_xml_scalar_digits(text,true){return Err(Error("Invalid XML decimal"));},
   "boolean"=>{srl_xml_bool(text)?;},"float"=>{srl_xml_float32(text)?;},"double"=>{srl_xml_float(text)?;},
   "language"=>{for (index,part) in text.split('-').enumerate(){if part.is_empty()||part.len()>8||!part.bytes().all(|value|value.is_ascii_alphabetic()||index!=0&&value.is_ascii_digit()){return Err(Error("Invalid XML language"));}}},
   "Name"|"NCName"|"NMTOKEN"|"ID"|"IDREF"|"ENTITY"=>{if text.is_empty(){return Err(Error("Empty XML name token"));}for (index,value) in text.chars().enumerate(){if !srl_xml_scalar_name_character(value as u32,index==0&&self.kind!="NMTOKEN",self.kind=="Name"||self.kind=="NMTOKEN"){return Err(Error("Invalid XML name token"));}}},_=>{}
  }}Ok(())
 }
 /// Compare logical scalar/list values while keeping NaN unordered for numeric bounds.
 fn compare(&self,left:&str,right:&str,equal_nan:bool,maximum:usize)->Result<Option<std::cmp::Ordering>,Error>{
  use std::cmp::Ordering;
  if !self.list_kind.is_empty(){let first=srl_xml_scalar_tokens(left,maximum)?;let second=srl_xml_scalar_tokens(right,maximum)?;if first.len()!=second.len(){return Ok(Some(Ordering::Greater));}
   let item=Self::item(self.list_kind);for (first,second) in first.iter().zip(second.iter()){if item.compare(first,second,equal_nan,maximum)?!=Some(Ordering::Equal){return Ok(Some(Ordering::Greater));}}return Ok(Some(Ordering::Equal));}
  if self.integer_kind()||self.kind=="decimal"{return Ok(Some(srl_xml_decimal_compare(left,right)));}
  if self.kind=="boolean"{return Ok(Some(srl_xml_bool(left)?.cmp(&srl_xml_bool(right)?)));}
  if self.kind=="float"||self.kind=="double"{let a=if self.kind=="float"{srl_xml_float32(left)? as f64}else{srl_xml_float(left)?};let b=if self.kind=="float"{srl_xml_float32(right)? as f64}else{srl_xml_float(right)?};if a.is_nan()||b.is_nan(){return Ok(if equal_nan&&a.is_nan()&&b.is_nan(){Some(Ordering::Equal)}else{None});}return Ok(a.partial_cmp(&b));}
  Ok(Some(left.cmp(right)))
 }
 /// Construct an unrestricted builtin list-item descriptor without retaining mutable counters.
 fn item(kind:&'static str)->Self{Self{kind,whitespace:"collapse",bounds:["";4],length:None,minimum:None,maximum:None,enumeration:&[],finite:false,groups:&[],list_kind:"",members:&[],fixed:None}}
 /// Validate one normalized candidate before conversion or escaped publication.
 fn check(&self,raw:&str,maximum:usize)->Result<String,Error>{let mut work=SrlXmlScalarWork{remaining:SRL_XML_SCALAR_MAX_WORK,max_items:maximum};self.check_with(raw,&mut work)}
 /// Share finite work through union alternatives and every inherited pattern group.
 fn check_with(&self,raw:&str,work:&mut SrlXmlScalarWork)->Result<String,Error>{
  work.spend(raw.len())?;let text=srl_xml_scalar_normalize(raw,self.whitespace)?;self.lexical(&text)?;
  if !self.members.is_empty(){let mut matched=false;for member in self.members{match member.check_with(&text,work){Ok(_)=>{matched=true;break;},Err(error) if matches!(error.0,"XML scalar work limit"|"XML scalar allocation"|"XML pattern input limit"|"XML list item limit")=>return Err(error),Err(_)=>{}}}if !matched{return Err(Error("XML union has no member"));}}
  let mut count=text.chars().count();if !self.list_kind.is_empty(){let items=srl_xml_scalar_tokens(&text,work.max_items)?;count=items.len();let item=Self::item(self.list_kind);for text in items{work.spend(text.len())?;item.lexical(text)?;}}
  if self.length.is_some_and(|bound|count!=bound)||self.minimum.is_some_and(|bound|count<bound)||self.maximum.is_some_and(|bound|count>bound){return Err(Error("XML length facet"));}
  for (index,bound) in self.bounds.iter().enumerate(){if bound.is_empty(){continue;}let order=self.compare(&text,bound,false,work.max_items)?.ok_or(Error("Unordered XML bound"))?;if matches!(index,0|2)&&order.is_lt()||matches!(index,1|3)&&order.is_gt()||index>=2&&order.is_eq(){return Err(Error("XML numeric facet"));}}
  if !self.enumeration.is_empty(){let mut matched=false;for value in self.enumeration{if self.compare(&text,&srl_xml_scalar_normalize(value,self.whitespace)?,true,work.max_items)?==Some(std::cmp::Ordering::Equal){matched=true;break;}}if !matched{return Err(Error("XML enumeration facet"));}}
  if let Some(fixed)=self.fixed{if self.compare(&text,&srl_xml_scalar_normalize(fixed,self.whitespace)?,true,work.max_items)?!=Some(std::cmp::Ordering::Equal){return Err(Error("XML fixed value"));}}
  if self.finite{let value=if self.kind=="float"{srl_xml_float32(&text)? as f64}else{srl_xml_float(&text)?};if !value.is_finite(){return Err(Error("XML finite facet"));}}
  for group in self.groups{let mut matched=false;for pattern in *group{if pattern.matches(&text,work)?{matched=true;break;}}if !matched{return Err(Error("XML portable pattern facet"));}}
  Ok(text)
 }
 /// Preserve a normalized empty collapse value rather than reactivating a different element default.
 fn output(&self,raw:&str,fallback:Option<&str>,maximum:usize)->Result<String,Error>{let text=self.check(raw,maximum)?;if text.is_empty(){if let Some(fallback)=fallback{if !srl_xml_scalar_normalize(fallback,self.whitespace)?.is_empty(){if self.whitespace!="collapse"{return Err(Error("Unrepresentable empty XML default"));}return Ok(" ".to_owned());}}}Ok(text)}
}
/// Join one admitted native list item with unambiguous XML whitespace framing.
fn srl_xml_scalar_append(joined:&mut String,text:&str,maximum:usize)->Result<(),Error>{
 if text.is_empty()||text.chars().any(srl_xml_scalar_space){return Err(Error("Unrepresentable XML list item"));}
 let separator=usize::from(!joined.is_empty());if separator>maximum.saturating_sub(joined.len())||text.len()>maximum.saturating_sub(joined.len()+separator){return Err(Error("XML list text limit"));}
 joined.try_reserve(separator+text.len()).map_err(|_|Error("XML scalar allocation"))?;if separator!=0{joined.push(' ');}joined.push_str(text);Ok(())
}

)rustscalar"
R"rustscalar(
impl SrlXmlPattern {
 /// Deduplicate states on scheduling, so nullable loops fit a fixed-size pending stack.
 fn closure(&self,selected:&mut [usize],count:usize,visited:&mut [bool],stack:&mut [usize],work:&mut SrlXmlScalarWork)->Result<usize,Error>{
  work.spend(visited.len())?;visited.fill(false);let mut pending=0;let mut kept=0;
  for target in selected[..count].iter().copied(){work.spend(1)?;if !visited[target]{visited[target]=true;stack[pending]=target;pending+=1;}}
  while pending!=0{work.spend(1)?;pending-=1;let current=stack[pending];let state=&self.states[current];if state.flags&9!=0{selected[kept]=current;kept+=1;}
   for target in state.epsilon.iter().copied(){work.spend(1)?;if !visited[target]{visited[target]=true;stack[pending]=target;pending+=1;}}
  }Ok(kept)
 }
 /// Match complete Unicode sequences without backtracking, under shared input and work admission.
 fn matches(&self,text:&str,work:&mut SrlXmlScalarWork)->Result<bool,Error>{
  if text.len()>SRL_XML_SCALAR_PATTERN_BYTES{return Err(Error("XML pattern input limit"));}
  let count=self.states.len();let mut visited=Vec::new();visited.try_reserve_exact(count).map_err(|_|Error("XML scalar allocation"))?;visited.resize(count,false);
  let allocate=||->Result<Vec<usize>,Error>{let mut result=Vec::new();result.try_reserve_exact(count).map_err(|_|Error("XML scalar allocation"))?;result.resize(count,0);Ok(result)};
  let mut active=allocate()?;let mut following=allocate()?;let mut stack=allocate()?;active[0]=self.start;
  let mut count=self.closure(&mut active,1,&mut visited,&mut stack,work)?;
  for code in text.chars().map(|value|value as u32){work.spend(1)?;let mut next=0;
   for selected in active[..count].iter().copied(){work.spend(1)?;let state=&self.states[selected];if state.flags&1==0{continue;}
    let mut matched=false;if state.flags&4!=0{matched=code!=10&&code!=13;}else{for (first,last) in state.ranges{work.spend(1)?;matched|=(*first..=*last).contains(&code);}if state.flags&2!=0{matched=!matched;}}
    if matched{following[next]=state.next;next+=1;}
   }
   count=self.closure(&mut following,next,&mut visited,&mut stack,work)?;std::mem::swap(&mut active,&mut following);
  }
  Ok(active[..count].iter().any(|selected|self.states[*selected].flags&8!=0))
 }
}

)rustscalar"
