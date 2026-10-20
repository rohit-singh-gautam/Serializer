// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rusttok(
const SRL_XML_NAMESPACE:&str="http://www.w3.org/XML/1998/namespace";
const SRL_XMLNS_NAMESPACE:&str="http://www.w3.org/2000/xmlns/";
/// Retain expanded names only for active elements and current attributes.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
struct SrlXmlTokenName<'a> { uri:String, local:&'a str }
/// One provider lookahead owns no completed descendants.
enum SrlXmlToken<'a> { Start(SrlXmlTokenName<'a>,Vec<(SrlXmlTokenName<'a>,String)>), End(SrlXmlTokenName<'a>), Text(String), Eof }
/// Keep the exact lexical closing name and namespace rollback position for one ancestor.
struct SrlXmlTokenAncestor<'a> { prefix:&'a str, local:&'a str, name:SrlXmlTokenName<'a>, bindings:usize }
/// A current-record attribute set is consumed once independently from child traversal.
struct SrlXmlTokenFields<'a> { values:Vec<(SrlXmlTokenName<'a>,String)> }
impl SrlXmlTokenFields<'_> {
    /// Preserve an absent attribute separately from an empty lexical string.
    fn attribute(&mut self,uri:&str,local:&str,optional:bool) -> Result<Option<String>,Error> {
        if let Some(index)=self.values.iter().position(|(name,_)| name.uri == uri && name.local == local) {
            return Ok(Some(self.values.remove(index).1));
        }
        if optional { Ok(None) } else { Err(Error("Required XML attribute missing")) }
    }
    /// Reject every unknown expanded attribute before publishing its owner.
    fn finish(self) -> Result<(),Error> { if self.values.is_empty() { Ok(()) } else { Err(Error("Unexpected XML attribute")) } }
}
/// Admit precisely the XML1.0 scalar ranges independently of tokenizer reference handling.
fn srl_xml_token_character(value:char) -> bool {
    matches!(value as u32,9|10|13|0x20..=0xd7ff|0xe000..=0xfffd|0x10000..=0x10ffff)
}
/// Normalize physical line breaks and decode only predefined/numeric references, within admission.
fn srl_xml_token_value(raw:&str,attribute:bool,literal:bool,maximum:usize) -> Result<String,Error> {
    srl_xml_token_value_with_operation(raw,attribute,literal,maximum,None)
}
/// Decode one lexical token under the selected cumulative operation before retaining its value.
fn srl_xml_token_value_with_operation(raw:&str,attribute:bool,literal:bool,maximum:usize,operation:Option<&@xml_operation@::State<'_>>) -> Result<String,Error> {
    @xml_operation@::consume(operation,@xml_operation@::Resource::WorkUnits,raw.len())?;
    let mut result=String::new(); let mut chars=raw.chars().peekable();
    while let Some(mut value)=chars.next() {
        if value == '&' && !literal {
            if chars.peek() == Some(&'#') {
                chars.next(); let hexadecimal=chars.peek() == Some(&'x');
                if hexadecimal { chars.next(); }
                let radix=if hexadecimal {16} else {10};
                let mut scalar=0u32; let mut digits=false; let mut terminated=false;
                for next in chars.by_ref() {
                    if next == ';' { terminated=true; break; }
                    let digit=next.to_digit(radix).ok_or(Error("Invalid XML character reference"))?;
                    scalar=scalar.checked_mul(radix).and_then(|value| value.checked_add(digit)).filter(|value| *value <= 0x10ffff).ok_or(Error("Invalid XML character reference"))?;
                    digits=true;
                }
                if !digits || !terminated { return Err(Error("Unterminated XML character reference")); }
                value=char::from_u32(scalar).ok_or(Error("Invalid XML character reference"))?;
            } else {
                const MAX_PREDEFINED_ENTITY_BYTES:usize=4;
                let mut reference=String::new(); let mut terminated=false;
                for next in chars.by_ref() {
                    if next == ';' { terminated=true; break; }
                    if reference.len() >= MAX_PREDEFINED_ENTITY_BYTES { return Err(Error("XML entity forbidden")); }
                    reference.push(next);
                }
                if !terminated { return Err(Error("Unterminated XML character reference")); }
                value=match reference.as_str() {
                    "amp"=>'&',"lt"=>'<',"gt"=>'>',"quot"=>'"',"apos"=>'\'',
                    _=>return Err(Error("XML entity forbidden")),
                };
            }
        } else if value == '\r' {
            if chars.peek() == Some(&'\n') { chars.next(); }
            value=if attribute {' '} else {'\n'};
        } else if attribute && (value == '\n' || value == '\t') { value=' '; }
        if !srl_xml_token_character(value) { return Err(Error("Illegal XML character")); }
        if value.len_utf8() > maximum.saturating_sub(result.len()) { return Err(@xml_operation@::quota(operation,"XML text limit")); }
        @xml_operation@::consume(operation,@xml_operation@::Resource::DecoderAllocationBytes,value.len_utf8())?;
        result.try_reserve(value.len_utf8()).map_err(|_| @xml_operation@::quota(operation,"XML text allocation"))?;
        result.push(value);
    }
    Ok(result)
}
/// Feed typed generated readers from pinned xmlparser tokens with active namespace scopes only.
struct SrlXmlInput<'a,'operation> {
    operation:Option<@xml_operation@::State<'operation>>,permitted_root_type:Option<(&'a str,&'a str)>,
    parser:xmlparser::Tokenizer<'a>, current:SrlXmlToken<'a>, limits:XmlLimits, map_limits:XmlMapLimits, mixed_limits:XmlMixedLimits, digest_limits:XmlDigestLimits,
    ancestors:Vec<SrlXmlTokenAncestor<'a>>, bindings:Vec<(&'a str,String)>,
    nodes:usize, attributes:usize, text_bytes:usize, empty_end:bool, seen_root:bool,
}
impl<'a,'operation> SrlXmlInput<'a,'operation> {
    /// Select a compiler-owned projection before policy checks or deferred scalar conversion.
    fn select(&self,field:&'static str) {if let Some(operation)=&self.operation {operation.field(field);}}
    /// Classify a native resource origin without changing the ordinary Error spelling.
    fn quota(&self,message:&'static str)->Error {@xml_operation@::quota(self.operation.as_ref(),message)}
    /// Admit retained native storage only for an explicitly selected shared operation.
    fn allocate(&self,count:usize)->Result<(),Error> {@xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,count)}

    /// Admit known default construction work before invoking an unchanged native Default implementation.
    fn default_work(&self,count:usize)->Result<(),Error> {@xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::WorkUnits,count)}
    /// Account known owned default storage before a string, fixed array or nested digest is constructed.
    fn default_storage(&self,count:usize)->Result<(),Error> {self.default_work(count)?;self.allocate(count)}
    /// Validate the UTF8/header/input bound before constructing any typed candidate.
    fn new(text:&'a str,limits:XmlLimits) -> Result<Self,Error> {Self::new_with_operation(text,limits,None)}
    /// Borrow an explicit operation through every nested primary or identity token visit.
    fn new_with_operation(text:&'a str,limits:XmlLimits,operation:Option<@xml_operation@::State<'operation>>) -> Result<Self,Error> {
        if text.len() > limits.max_bytes { return Err(@xml_operation@::quota(operation.as_ref(),"XML input byte limit")); }
        // Admit the bounded header/DTD/provider scans before visiting untrusted input bytes.
        @xml_operation@::consume(operation.as_ref(),@xml_operation@::Resource::WorkUnits,text.len().saturating_mul(3))?;
        srl_xml_header(text)?; srl_xml_reject_dtd(text)?;
        let mut result=Self {operation,permitted_root_type:None,parser:xmlparser::Tokenizer::from(text),current:SrlXmlToken::Eof,limits,map_limits:XmlMapLimits::default(),mixed_limits:XmlMixedLimits::default(),digest_limits:XmlDigestLimits::default(),
            ancestors:Vec::new(),bindings:Vec::new(),nodes:0,attributes:0,text_bytes:0,empty_end:false,seen_root:false};
        result.advance()?; Ok(result)
    }
    /// Charge decoded logical bytes before retaining them across provider tokens.
    fn charge(&mut self,value:&str) -> Result<(),Error> {
        @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::ScalarBytes,value.len())?;
        @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::WorkUnits,value.len())?;
        self.text_bytes=self.text_bytes.checked_add(value.len()).ok_or_else(||@xml_operation@::quota(self.operation.as_ref(),"XML text limit"))?;
        if self.text_bytes > self.limits.max_text_bytes { return Err(@xml_operation@::quota(self.operation.as_ref(),"XML text limit")); } Ok(())
    }
    /// Resolve the current prefix scope, leaving unqualified attributes outside the default namespace.
    fn namespace(&self,prefix:&str,attribute:bool) -> Result<String,Error> {
        if prefix == "xml" { self.allocate(SRL_XML_NAMESPACE.len())?; return Ok(SRL_XML_NAMESPACE.to_owned()); }
        if prefix == "xmlns" { return Err(Error("Reserved XML namespace prefix")); }
        if attribute && prefix.is_empty() { return Ok(String::new()); }
        if let Some((_,uri))=self.bindings.iter().rev().find(|(name,_)| *name == prefix) { self.allocate(uri.len())?; return Ok(uri.clone()); }
        if prefix.is_empty() { Ok(String::new()) } else { Err(Error("Unbound XML namespace prefix")) }
    }
    /// Admit declarations, reject reserved identities and retain one bounded start-element payload.
    fn start(&mut self,prefix:&'a str,local:&'a str) -> Result<(),Error> {
        if let Some(operation)=&self.operation {operation.begin(local)?;}
        @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::XmlNodes,1)?;
        @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,std::mem::size_of::<SrlXmlTokenAncestor<'_>>())?;
        if self.ancestors.is_empty() {
            if self.seen_root { return Err(Error("Multiple XML roots")); } self.seen_root=true;
        }
        self.nodes=self.nodes.checked_add(1).ok_or_else(||@xml_operation@::quota(self.operation.as_ref(),"XML node limit"))?;
        if self.nodes > self.limits.max_nodes || self.ancestors.len() >= self.limits.max_depth { return Err(@xml_operation@::quota(self.operation.as_ref(),"XML node/depth limit")); }
        let rollback=self.bindings.len(); let mut raw=Vec::new(); let mut names=std::collections::BTreeSet::new();
        let empty=loop {
            @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::WorkUnits,1)?;
            match self.parser.next().ok_or(Error("Unterminated XML start tag"))?.map_err(|_| Error("Malformed XML"))? {
                xmlparser::Token::Attribute {prefix,local,value,..} => {
                    if let Some(operation)=&self.operation {operation.attribute(local.as_str());}
                    @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::XmlAttributes,1)?;
                    @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,std::mem::size_of::<(SrlXmlTokenName<'_>,String)>())?;
                    self.attributes=self.attributes.checked_add(1).ok_or_else(||@xml_operation@::quota(self.operation.as_ref(),"XML attribute limit"))?;
                    if self.attributes > self.limits.max_attributes { return Err(@xml_operation@::quota(self.operation.as_ref(),"XML attribute limit")); }
                    let prefix=prefix.as_str(); let local=local.as_str();
                    self.allocate(std::mem::size_of::<(&str,&str)>())?;
                    if !names.insert((prefix,local)) { return Err(Error("Duplicate XML attribute")); }
                    let value=srl_xml_token_value_with_operation(value.as_str(),true,false,self.limits.max_text_bytes.saturating_sub(self.text_bytes),self.operation.as_ref())?;
                    self.charge(&value)?;
                    if prefix == "xmlns" || prefix.is_empty() && local == "xmlns" {
                        let declared=if prefix.is_empty() {""} else {local};
                        if declared == "xmlns" || value == SRL_XMLNS_NAMESPACE ||
                            (declared == "xml") != (value == SRL_XML_NAMESPACE) ||
                            !declared.is_empty() && value.is_empty() { return Err(Error("Invalid XML namespace binding")); }
                        self.charge(declared)?;
                        self.allocate(std::mem::size_of::<(&str,String)>())?; self.bindings.push((declared,value));
                    } else { self.allocate(std::mem::size_of::<(&str,&str,String)>())?; raw.push((prefix,local,value)); }
                }
                xmlparser::Token::ElementEnd {end:xmlparser::ElementEnd::Open,..}=>break false,
                xmlparser::Token::ElementEnd {end:xmlparser::ElementEnd::Empty,..}=>break true,
                _=>return Err(Error("Malformed XML start tag")),
            }
        };
        let name=SrlXmlTokenName {uri:self.namespace(prefix,false)?,local};
        self.charge(local)?; self.charge(&name.uri)?;
        let mut attributes=Vec::new(); let mut expanded=std::collections::BTreeSet::new();
        for (prefix,local,value) in raw {
            let name=SrlXmlTokenName {uri:self.namespace(prefix,true)?,local};
            self.allocate(std::mem::size_of::<SrlXmlTokenName<'_>>().saturating_add(name.uri.len()))?;
            if !expanded.insert(name.clone()) { return Err(Error("Duplicate expanded XML attribute")); }
            self.charge(local)?; self.charge(&name.uri)?;
            self.allocate(std::mem::size_of::<(SrlXmlTokenName<'_>,String)>())?; attributes.push((name,value));
        }
        self.allocate(name.uri.len())?;
        if let Some(operation)=&self.operation {operation.expanded(&name,&attributes,&|prefix|self.namespace(prefix,false))?;}
        self.ancestors.push(SrlXmlTokenAncestor {prefix,local,name:name.clone(),bindings:rollback});
        self.current=SrlXmlToken::Start(name,attributes); self.empty_end=empty; Ok(())
    }
)rusttok"
R"rusttok(    /// Advance one significant token, checking lexical closing names and exact complete-document EOF.
    fn advance(&mut self) -> Result<(),Error> {
        if let Some(operation)=&self.operation {operation.poll()?;}
        if self.empty_end {
            self.empty_end=false;
            self.allocate(self.ancestors.last().ok_or(Error("Unexpected XML end"))?.name.uri.len())?;
            self.current=SrlXmlToken::End(self.ancestors.last().ok_or(Error("Unexpected XML end"))?.name.clone()); return Ok(());
        }
        if matches!(self.current,SrlXmlToken::End(_)) {
            let frame=self.ancestors.pop().ok_or(Error("Unexpected XML end"))?;
            if let Some(operation)=&self.operation {operation.end();} self.bindings.truncate(frame.bindings);
        }
        loop {
            @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::WorkUnits,1)?;
            let token=match self.parser.next() {
                Some(value)=>value.map_err(|_| Error("Malformed XML"))?,
                None=> {
                    if !self.seen_root || !self.ancestors.is_empty() { return Err(Error("Incomplete XML document")); }
                    self.current=SrlXmlToken::Eof; return Ok(());
                }
            };
            match token {
                xmlparser::Token::ElementStart {prefix,local,..}=>return self.start(prefix.as_str(),local.as_str()),
                xmlparser::Token::ElementEnd {end:xmlparser::ElementEnd::Close(prefix,local),..}=> {
                    let frame=self.ancestors.last().ok_or(Error("Unexpected XML closing tag"))?;
                    if frame.prefix != prefix.as_str() || frame.local != local.as_str() { return Err(Error("Mismatched XML closing tag")); }
                    self.allocate(frame.name.uri.len())?; self.current=SrlXmlToken::End(frame.name.clone()); return Ok(());
                }
                xmlparser::Token::Text {text} | xmlparser::Token::Cdata {text,..}=> {
                    if let Some(operation)=&self.operation {operation.text();}
                    let value=srl_xml_token_value_with_operation(text.as_str(),false,matches!(token,xmlparser::Token::Cdata {..}),self.limits.max_text_bytes.saturating_sub(self.text_bytes),self.operation.as_ref())?;
                    self.charge(&value)?; self.current=SrlXmlToken::Text(value); return Ok(());
                }
                xmlparser::Token::Declaration {version,encoding,..}=> {
                    if version.as_str() != "1.0" || encoding.is_some_and(|value| !value.as_str().eq_ignore_ascii_case("UTF-8")) { return Err(Error("Only XML1.0 UTF8 supported")); }
                }
                xmlparser::Token::Comment {..}=>{},
                xmlparser::Token::ProcessingInstruction {target,..}=> {
                    if target.as_str().eq_ignore_ascii_case("xml") { return Err(Error("Reserved XML instruction target")); }
                }
                _=>return Err(Error("XML DTD/entities forbidden")),
            }
        }
    }
)rusttok"
R"rustpull(
    /// Ignore only XML formatting whitespace between generated child fields.
    fn skip_space(&mut self) -> Result<(),Error> {
        while let SrlXmlToken::Text(value)=&self.current {
            if !value.trim_matches([' ','\t','\r','\n']).is_empty() { break; } self.advance()?;
        }
        Ok(())
    }
    /// Test the one lookahead name without retaining or consuming an unrelated child.
    fn has(&mut self,uri:&str,local:&str) -> Result<bool,Error> {
        self.skip_space()?;
        Ok(matches!(&self.current,SrlXmlToken::Start(name,_) if name.uri == uri && name.local == local))
    }
    /// Distinguish absence, explicit nil and present empty payloads before consuming their element.
    fn presence(&mut self,uri:&str,local:&str,absent:bool,nil:bool) -> Result<char,Error> {
        if !self.has(uri,local)? {
            return if absent {Ok('a')} else {Err(Error("Required XML element missing"))};
        }
        let mut is_nil=false;
        if let SrlXmlToken::Start(_,attributes)=&mut self.current {
            if let Some(index)=attributes.iter().position(|(name,_)| name.uri == "http://www.w3.org/2001/XMLSchema-instance" && name.local == "nil") {
                if !nil { return Err(Error("XML nil attribute forbidden")); }
                is_nil=srl_xml_bool(&attributes.remove(index).1)?;
            }
        }
        if !is_nil { return Ok('v'); }
        self.open(uri,local)?.finish()?;
        if !self.text()?.is_empty() { return Err(Error("Nil XML element contains text")); }
        self.close(uri,local)?; Ok('n')
    }
    /// Transfer only current attributes and enter one exact generated record or wrapper.
    fn open(&mut self,uri:&str,local:&str) -> Result<SrlXmlTokenFields<'a>,Error> {
        if !self.has(uri,local)? { return Err(Error("Required XML element missing")); }
        if self.ancestors.len()==1 {
            if let Some((expected_uri,expected_local))=self.permitted_root_type.take() {
                let value=match &self.current {SrlXmlToken::Start(_,values)=>values.iter().find(|(name,_)|name.uri=="http://www.w3.org/2001/XMLSchema-instance"&&name.local=="type").map(|(_,value)|{self.allocate(value.len())?;Ok::<_,Error>(value.clone())}).transpose()?,_=>None};
                if let Some(value)=value {
                    let value=value.trim_matches([' ','\t','\r','\n']);
                    let (prefix,local)=value.split_once(':').map_or(("",value),|(prefix,local)|(prefix,local));
                    if local.is_empty()||local.contains(':')||!local.chars().enumerate().all(|(index,value)|srl_xml_scalar_name_character(value as u32,index==0,false))||!prefix.is_empty()&&!prefix.chars().enumerate().all(|(index,value)|srl_xml_scalar_name_character(value as u32,index==0,false))||self.namespace(prefix,false)?!=expected_uri||local!=expected_local {return Err(Error("Package root xsi:type mismatch"));}
                    if let SrlXmlToken::Start(_,values)=&mut self.current {values.retain(|(name,_)|name.uri!="http://www.w3.org/2001/XMLSchema-instance"||name.local!="type");}
                }
            }
        }
        let fields=match &mut self.current { SrlXmlToken::Start(_,values)=>SrlXmlTokenFields {values:std::mem::take(values)},_=>unreachable!() };
        self.advance()?; Ok(fields)
    }
    /// Join only one active scalar while preserving normalized physical and referenced whitespace.
    fn text(&mut self) -> Result<String,Error> {
        let mut result=String::new();
        while let SrlXmlToken::Text(value)=&self.current {
            @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,value.len())?;
            result.try_reserve(value.len()).map_err(|_| @xml_operation@::quota(self.operation.as_ref(),"XML scalar allocation"))?; result.push_str(value); self.advance()?;
        }
        if !matches!(self.current,SrlXmlToken::End(_)) { return Err(Error("XML scalar contains child elements")); } Ok(result)
    }
    /// Require the exact closing name and reject leftover text or unknown children.
    fn close(&mut self,uri:&str,local:&str) -> Result<(),Error> {
        self.skip_space()?;
        if !matches!(&self.current,SrlXmlToken::End(name) if name.uri == uri && name.local == local) { return Err(Error("Unexpected XML content")); }
        self.advance()
    }
    /// Decode one attribute-free scalar child without retaining any completed node.
    fn scalar(&mut self,uri:&str,local:&str) -> Result<String,Error> {
        self.open(uri,local)?.finish()?; let value=self.text()?; self.close(uri,local)?; Ok(value)
    }
    /// Publish only after the provider has consumed the entire message successfully.
    fn finish_document(&mut self) -> Result<(),Error> {
        self.skip_space()?;
        if matches!(self.current,SrlXmlToken::Eof) { Ok(()) } else { Err(Error("Unexpected trailing XML")) }
    }
}
)rustpull"
