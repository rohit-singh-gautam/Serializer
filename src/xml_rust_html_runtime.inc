// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rusthtml(
/// Independent byte/event/stack bounds for the explicitly approved ordinary HTML fragment subset.
#[derive(Clone, Copy, Debug)]
pub struct HtmlLimits {
    pub max_input_bytes:usize, pub max_output_bytes:usize, pub max_events:usize,
    pub max_depth:usize, pub max_attributes_per_element:usize, pub max_text_bytes:usize,
}
impl Default for HtmlLimits {
    /// Apply the same conservative semantic bounds as every approved fragment adapter.
    fn default()->Self { Self {max_input_bytes:8388608,max_output_bytes:16777216,max_events:200000,
        max_depth:128,max_attributes_per_element:256,max_text_bytes:8388608} }
}
/// An owning lowercase approved attribute; hidden is represented as Boolean presence.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct HtmlAttribute { pub name:String, pub value:String, pub boolean:bool }
/// Semantic events contain ordinary HTML names and text, independently of XML namespaces and xsi metadata.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum HtmlEvent {
    StartFragment, EndFragment, StartElement {name:String,attributes:Vec<HtmlAttribute>},
    EndElement {name:String}, Text(String),
}
/// Explicitly balanced approved HTML; general browser recovery and URL sanitization are outside this contract.
pub struct Html;
impl Html {
    /// Describe the exact supported subset without claiming general HTML5 parsing or sanitization.
    pub fn supported_subset()->&'static str {
        "UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; br/hr/img void tags; ASCII case-insensitive names; quoted/unquoted and hidden Boolean attributes; amp/lt/gt/quot/apos/nbsp and numeric references; comments omitted; no doctype, implied closes, raw text, foreign content or browser recovery"
    }
    /// Resolve only approved lowercase spellings to immutable metadata, never arbitrary runtime tag vocabularies.
    fn tag(value:&str)->Option<&'static str> {
        match value {"div"=>Some("div"),"p"=>Some("p"),"span"=>Some("span"),"b"=>Some("b"),"i"=>Some("i"),
            "strong"=>Some("strong"),"em"=>Some("em"),"a"=>Some("a"),"ul"=>Some("ul"),"ol"=>Some("ol"),
            "li"=>Some("li"),"section"=>Some("section"),"article"=>Some("article"),"h1"=>Some("h1"),
            "h2"=>Some("h2"),"h3"=>Some("h3"),"h4"=>Some("h4"),"h5"=>Some("h5"),"h6"=>Some("h6"),
            "br"=>Some("br"),"hr"=>Some("hr"),"img"=>Some("img"),"small"=>Some("small"),"s"=>Some("s"),
            "u"=>Some("u"),"code"=>Some("code"),"blockquote"=>Some("blockquote"),"nav"=>Some("nav"),"sub"=>Some("sub"),"sup"=>Some("sup"),_=>None}
    }
    /// Identify only the approved void elements; they still have paired semantic start/end events.
    fn void(value:&str)->bool { matches!(value,"br"|"hr"|"img") }
    /// Identify phrasing elements whose accepted children cannot require browser rearrangement.
    fn phrasing(value:&str)->bool { matches!(value,"span"|"b"|"i"|"strong"|"em"|"a"|"br"|"img"|"small"|"s"|"u"|"code"|"sub"|"sup") }
    /// Recognize the fixed HTML ASCII whitespace set, including form feed.
    fn space(value:u8)->bool { matches!(value,b' '|b'\t'|b'\r'|b'\n'|12) }
    /// Reject unsupported control scalars without applying XML-specific character rules.
    fn text(value:&str)->Result<(),Error> {
        if value.chars().any(|item| {let code=item as u32;code==0 || (127..=159).contains(&code) || code<32 && !matches!(code,9|10|12|13)}) {
            return Err(Error("Unsupported HTML control character"));
        } Ok(())
    }
    /// Refuse parent/child relationships that independent HTML parsers would implicitly close or rearrange.
    fn child(open:&[&str],child:&str)->Result<(),Error> {
        if Self::tag(child).is_none() || child=="li" && !matches!(open.last(),Some(&"ul"|&"ol")) {
            return Err(Error("HTML tag or list relationship outside approved subset"));
        }
        if let Some(parent)=open.last() {
            if Self::void(parent) || matches!(*parent,"ul"|"ol") && child!="li" ||
                (Self::phrasing(parent) || matches!(*parent,"p"|"h1"|"h2"|"h3"|"h4"|"h5"|"h6")) && !Self::phrasing(child) {
                return Err(Error("HTML child requires unsupported browser recovery"));
            }
        }
        if child=="a" && open.contains(&"a") {return Err(Error("Nested HTML anchor outside approved subset"));} Ok(())
    }
    /// Keep accepted attributes explicit; neither arbitrary event handlers nor foreign namespace attributes are admitted.
    fn attribute(tag:&str,key:&str)->bool {
        matches!(key,"id"|"class"|"title"|"lang"|"dir"|"hidden") ||
            tag=="a" && matches!(key,"href"|"rel") || tag=="img" && matches!(key,"src"|"alt"|"width"|"height")
    }
    /// Check output byte admission before reserving or appending to the unpublished candidate buffer.
    fn append(output:&mut String,value:&str,maximum:usize)->Result<(),Error> {
        if value.len()>maximum.saturating_sub(output.len()) {return Err(Error("HTML output byte limit"));}
        output.try_reserve(value.len()).map_err(|_|Error("HTML allocation failure"))?;output.push_str(value);Ok(())
    }
    /// Escape semantic text while retaining referenced CR and ordinary HTML form-feed scalars exactly.
    fn escape(output:&mut String,value:&str,attribute:bool,maximum:usize)->Result<(),Error> {
        Self::text(value)?;
        for item in value.chars() {
            let mut encoded=[0u8;4];let spelling=match item {'&'=>"&amp;",'<'=>"&lt;",'>'=>"&gt;",'\r'=>"&#13;",'"' if attribute=>"&quot;",_=>item.encode_utf8(&mut encoded)};
            Self::append(output,spelling,maximum)?;
        } Ok(())
    }
    /// Validate a complete owning semantic trace before returning deterministic ordinary HTML UTF-8.
    pub fn write_fragment(events:&[HtmlEvent],limits:HtmlLimits)->Result<Vec<u8>,Error> {
        if events.len()<2 || events.len()>limits.max_events || events.first()!=Some(&HtmlEvent::StartFragment) || events.last()!=Some(&HtmlEvent::EndFragment) {
            return Err(Error("HTML requires bounded explicit fragment boundaries"));
        }
        let mut output=String::new();let mut open:Vec<&str>=Vec::new();let mut text_bytes=0usize;
        for (index,event) in events.iter().enumerate() {
            match event {
                HtmlEvent::StartFragment|HtmlEvent::EndFragment=>{
                    if index!=0 && index!=events.len()-1 || !open.is_empty() {return Err(Error("Malformed HTML fragment boundary"));}
                }
                HtmlEvent::StartElement{name,attributes}=>{
                    Self::child(&open,name)?;
                    if open.len()>=limits.max_depth || attributes.len()>limits.max_attributes_per_element {return Err(Error("HTML element depth/attribute limit"));}
                    Self::append(&mut output,"<",limits.max_output_bytes)?;Self::append(&mut output,name,limits.max_output_bytes)?;
                    for (position,attribute) in attributes.iter().enumerate() {
                        let key=attribute.name.as_str();
                        if !Self::attribute(name,key) || attributes[..position].iter().any(|item|item.name==key) ||
                            attribute.boolean!=(key=="hidden") || attribute.boolean && !attribute.value.is_empty() {return Err(Error("Unsupported or duplicate HTML attribute"));}
                        Self::text(&attribute.value)?;Self::append(&mut output," ",limits.max_output_bytes)?;Self::append(&mut output,key,limits.max_output_bytes)?;
                        if !attribute.boolean {Self::append(&mut output,"=\"",limits.max_output_bytes)?;Self::escape(&mut output,&attribute.value,true,limits.max_output_bytes)?;Self::append(&mut output,"\"",limits.max_output_bytes)?;}
                    }
                    Self::append(&mut output,">",limits.max_output_bytes)?;
                    open.try_reserve(1).map_err(|_|Error("HTML allocation failure"))?;open.push(name);
                }
                HtmlEvent::EndElement{name}=>{
                    if open.last()!=Some(&name.as_str()) {return Err(Error("Unbalanced HTML close"));}
                    if !Self::void(name) {Self::append(&mut output,"</",limits.max_output_bytes)?;Self::append(&mut output,name,limits.max_output_bytes)?;Self::append(&mut output,">",limits.max_output_bytes)?;}
                    open.pop();
                }
                HtmlEvent::Text(value)=>{
                    if open.last().is_some_and(|name|Self::void(name)) || value.len()>limits.max_text_bytes.saturating_sub(text_bytes) ||
                        matches!(open.last(),Some(&"ul"|&"ol")) && !value.bytes().all(Self::space) {return Err(Error("HTML text relationship or byte limit"));}
                    text_bytes+=value.len();Self::escape(&mut output,value,false,limits.max_output_bytes)?;
                }
            }
        }
        if !open.is_empty() {return Err(Error("HTML fragment has unclosed elements"));} Ok(output.into_bytes())
    }
)rusthtml"
R"rusthtml(
    /// Decode only approved terminated references and normalize source CR/CRLF before semantic publication.
    fn decode(source:&str)->Result<String,Error> {
        let bytes=source.as_bytes();let mut output=String::new();let mut position=0;
        output.try_reserve(bytes.len()).map_err(|_|Error("HTML allocation failure"))?;
        while position<bytes.len() {
            if bytes[position]==b'\r' {output.push('\n');position+=1;if bytes.get(position)==Some(&b'\n') {position+=1;}continue;}
            if bytes[position]!=b'&' {let item=source[position..].chars().next().ok_or(Error("Invalid HTML UTF8"))?;output.push(item);position+=item.len_utf8();continue;}
            let length=bytes[position+1..].iter().position(|byte|*byte==b';').ok_or(Error("HTML references require terminated spelling"))?;
            if length+1>12 {return Err(Error("HTML reference outside approved spelling"));}
            let entity=&source[position+1..position+1+length];
            let item=match entity {"amp"=>'&',"lt"=>'<',"gt"=>'>',"quot"=>'"',"apos"=>'\'',"nbsp"=>'\u{a0}',_=>{
                let raw=entity.strip_prefix('#').ok_or(Error("HTML entity outside approved subset"))?;
                let (digits,radix)=if raw.starts_with(['x','X']) {(&raw[1..],16)} else {(raw,10)};
                if digits.is_empty() || !digits.bytes().all(|byte|if radix==16 {byte.is_ascii_hexdigit()} else {byte.is_ascii_digit()}) {return Err(Error("Malformed HTML numeric reference"));}
                char::from_u32(u32::from_str_radix(digits,radix).map_err(|_|Error("Malformed HTML numeric reference"))?).ok_or(Error("Invalid HTML numeric scalar"))?
            }};
            output.push(item);position+=length+2;
        }
        Self::text(&output)?;Ok(output)
    }
    /// Advance only through the fixed HTML whitespace set without allocating source suffixes.
    fn skip(bytes:&[u8],mut position:usize)->usize {while bytes.get(position).is_some_and(|byte|Self::space(*byte)) {position+=1;}position}
    /// Read a bounded ASCII syntax name; approved names cannot exceed the longest fixed metadata spelling.
    fn name(bytes:&[u8],position:&mut usize)->Result<String,Error> {
        let start=*position;
        while bytes.get(*position).is_some_and(|byte|byte.is_ascii_alphanumeric() || *byte==b'-') {*position+=1;}
        if *position==start || *position-start>"blockquote".len() {return Err(Error("Unsupported HTML tag/attribute syntax"));}
        Ok(bytes[start..*position].iter().map(|byte|char::from(byte.to_ascii_lowercase())).collect())
    }
    /// Check event and semantic text admission before retaining each newly decoded owning value.
    fn event(events:&mut Vec<HtmlEvent>,event:HtmlEvent,text_bytes:&mut usize,limits:HtmlLimits)->Result<(),Error> {
        if events.len()>=limits.max_events {return Err(Error("HTML event count limit"));}
        if let HtmlEvent::Text(value)=&event {
            if value.len()>limits.max_text_bytes.saturating_sub(*text_bytes) {return Err(Error("HTML text byte limit"));}*text_bytes+=value.len();
        }
        events.try_reserve(1).map_err(|_|Error("HTML allocation failure"))?;events.push(event);Ok(())
    }
    /// Parse fatal UTF8 with explicit nesting; no event trace is published before complete validated EOF.
    pub fn parse_fragment(input:&[u8],limits:HtmlLimits)->Result<Vec<HtmlEvent>,Error> {
        if input.len()>limits.max_input_bytes {return Err(Error("HTML input byte limit"));}
        let source=std::str::from_utf8(input).map_err(|_|Error("Invalid HTML UTF8"))?;Self::text(source)?;
        let mut events=Vec::new();let mut open:Vec<&str>=Vec::new();let mut position=0usize;let mut text_bytes=0usize;
        Self::event(&mut events,HtmlEvent::StartFragment,&mut text_bytes,limits)?;
        while position<input.len() {
            if input[position]!=b'<' {
                let end=input[position..].iter().position(|byte|*byte==b'<').map_or(input.len(),|count|position+count);
                Self::event(&mut events,HtmlEvent::Text(Self::decode(&source[position..end])?),&mut text_bytes,limits)?;position=end;continue;
            }
            if input[position..].starts_with(b"<!--") {
                let end=input[position+4..].windows(3).position(|part|part==b"-->").map(|count|position+4+count).ok_or(Error("Malformed HTML comment"))?;
                if input[position+4..end].windows(2).any(|part|part==b"--") {return Err(Error("Malformed HTML comment"));}
                position=end+3;continue;
            }
            position+=1;let closing=input.get(position)==Some(&b'/');if closing {position+=1;}
            let spelling=Self::name(input,&mut position)?;let tag=Self::tag(&spelling).ok_or(Error("HTML tag outside approved subset"))?;
            if closing {
                position=Self::skip(input,position);
                if input.get(position)!=Some(&b'>') || Self::void(tag) || open.last()!=Some(&tag) {return Err(Error("HTML close requires matching explicit nonvoid start"));}
                position+=1;open.pop();Self::event(&mut events,HtmlEvent::EndElement{name:tag.into()},&mut text_bytes,limits)?;continue;
            }
            Self::child(&open,tag)?;if open.len()>=limits.max_depth {return Err(Error("HTML nesting limit"));}
            let mut attributes:Vec<HtmlAttribute>=Vec::new();let mut self_closing=false;
            loop {
                let separator=position;position=Self::skip(input,position);
                let byte=*input.get(position).ok_or(Error("Truncated HTML start tag"))?;
                if byte==b'>' {position+=1;break;}
                if input[position..].starts_with(b"/>") {self_closing=true;position+=2;break;}
                if separator==position || attributes.len()>=limits.max_attributes_per_element {return Err(Error("HTML attribute separation/cardinality"));}
                let key=Self::name(input,&mut position)?;let key_end=position;position=Self::skip(input,position);
                let assigned=input.get(position)==Some(&b'=');let mut value=String::new();
                if assigned {
                    position=Self::skip(input,position+1);let first=*input.get(position).ok_or(Error("Truncated HTML attribute"))?;
                    let quote=if matches!(first,b'\''|b'"') {position+=1;Some(first)} else {None};let start=position;
                    if let Some(quote)=quote {
                        position+=input[position..].iter().position(|byte|*byte==quote).ok_or(Error("Unterminated HTML attribute"))?;
                    } else {
                        while input.get(position).is_some_and(|byte|!Self::space(*byte) && *byte!=b'>') {
                            if matches!(input[position],b'"'|b'\''|b'<'|b'='|b'`') {return Err(Error("Invalid unquoted HTML attribute"));}position+=1;
                        }
                        if position==start {return Err(Error("Empty unquoted HTML attribute"));}
                    }
                    value=Self::decode(&source[start..position])?;if quote.is_some() {position+=1;}
                } else {position=key_end;}
                if key=="hidden" {
                    if assigned && !value.is_empty() && !value.eq_ignore_ascii_case("hidden") {return Err(Error("HTML hidden requires Boolean spelling"));}value.clear();
                } else if !assigned {return Err(Error("Only approved Boolean attributes may omit values"));}
                if !Self::attribute(tag,&key) || attributes.iter().any(|item|item.name==key) {return Err(Error("Unsupported or duplicate HTML attribute"));}
                attributes.try_reserve(1).map_err(|_|Error("HTML allocation failure"))?;attributes.push(HtmlAttribute{name:key.clone(),value,boolean:key=="hidden"});
            }
            if self_closing && !Self::void(tag) {return Err(Error("Self-closing nonvoid HTML requires browser recovery"));}
            Self::event(&mut events,HtmlEvent::StartElement{name:tag.into(),attributes},&mut text_bytes,limits)?;
            if Self::void(tag) {Self::event(&mut events,HtmlEvent::EndElement{name:tag.into()},&mut text_bytes,limits)?;}
            else {open.try_reserve(1).map_err(|_|Error("HTML allocation failure"))?;open.push(tag);}
        }
        if !open.is_empty() {return Err(Error("HTML fragment has unclosed elements"));}
        Self::event(&mut events,HtmlEvent::EndFragment,&mut text_bytes,limits)?;Self::write_fragment(&events,limits)?;Ok(events)
    }
}
)rusthtml"
