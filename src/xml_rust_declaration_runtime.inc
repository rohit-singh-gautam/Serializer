// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustdecl(
/// Apply processContents before either discarding or retaining an admitted wildcard value.
fn srl_xml_extension_validate(model:&SrlXmlExtensionModel,slot:usize,uri:&str,local:&str,value:Option<&str>,scope:&[(String,String)],text:&str,limits:XmlLimits,operation:Option<@xml_operation@::State<'_>>)->Result<(),Error>{
 let contract=model.contract.ok_or(Error("XML wildcard has no contract"))?;let process=contract.slots[slot].process_contents;
 if process==0{return Ok(());}let mut found=false;
 if let Some(validate)=contract.validate{
  if value.is_some(){found=validate(uri,local,value,scope,text,limits,operation)?;}
  else{let mut input=SrlXmlInput::new_with_operation(text,limits,operation.as_ref().map(@xml_operation@::State::secondary))?;input.skip_space()?;
   if let SrlXmlToken::Start(name,_)=&input.current{found=validate(&name.uri,name.local,None,&[],text,limits,operation)?;}else{return Err(Error("Missing declared XML root"));}}
 }
 if !found&&process==2{return Err(Error("Strict XML wildcard has no pinned declaration"));}Ok(())
}
/// Frame an actual global attribute without changing its explicit-empty or original namespace semantics.
fn srl_xml_declaration_attribute(root_uri:&str,root_local:&str,uri:&str,local:&str,value:&str,scope:&[(String,String)],limits:XmlLimits,operation:Option<@xml_operation@::State<'_>>)->Result<String,Error>{
 let mut serial=0usize;let root_prefix=if root_uri==SRL_XML_NAMESPACE{srl_xml_extension_owned("xml",operation.as_ref())?}else{loop{let candidate=srl_xml_extension_alias("srldecl",serial,operation.as_ref())?;serial=serial.checked_add(1).ok_or(Error("XML declaration alias limit"))?;if !scope.iter().any(|(prefix,_)|prefix==&candidate){break candidate;}}};
 let mut output=String::new();let lexical=srl_xml_extension_lexical(&root_prefix,root_local,operation.as_ref())?;
 srl_xml_extension_put(&mut output,"<",limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,&lexical,limits.max_bytes,operation.as_ref())?;
 for(prefix,namespace)in scope{if prefix=="xml"{continue;}srl_xml_extension_put(&mut output," xmlns",limits.max_bytes,operation.as_ref())?;if !prefix.is_empty(){srl_xml_extension_put(&mut output,":",limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,prefix,limits.max_bytes,operation.as_ref())?;}srl_xml_extension_put(&mut output,"=\"",limits.max_bytes,operation.as_ref())?;srl_xml_extension_escape(&mut output,namespace,true,limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,"\"",limits.max_bytes,operation.as_ref())?;}
 if root_prefix!="xml"{srl_xml_extension_put(&mut output," xmlns:",limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,&root_prefix,limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,"=\"",limits.max_bytes,operation.as_ref())?;srl_xml_extension_escape(&mut output,root_uri,true,limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,"\"",limits.max_bytes,operation.as_ref())?;}
 let prefix=if uri.is_empty(){String::new()}else if uri==root_uri{srl_xml_extension_owned(&root_prefix,operation.as_ref())?}else if uri==SRL_XML_NAMESPACE{srl_xml_extension_owned("xml",operation.as_ref())?}else if let Some((prefix,_))=scope.iter().find(|(prefix,namespace)|!prefix.is_empty()&&namespace==uri){srl_xml_extension_owned(prefix,operation.as_ref())?}else{
  let prefix=srl_xml_extension_alias("srldecl",serial,operation.as_ref())?;
  // The root alias was the first unused serial; a separate scan avoids a retained later alias.
  let mut selected=prefix;while scope.iter().any(|(prefix,_)|prefix==&selected)||selected==root_prefix{serial=serial.checked_add(1).ok_or(Error("XML declaration alias limit"))?;selected=srl_xml_extension_alias("srldecl",serial,operation.as_ref())?;}
  srl_xml_extension_put(&mut output," xmlns:",limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,&selected,limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,"=\"",limits.max_bytes,operation.as_ref())?;srl_xml_extension_escape(&mut output,uri,true,limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,"\"",limits.max_bytes,operation.as_ref())?;selected
 };
 let attribute=srl_xml_extension_lexical(&prefix,local,operation.as_ref())?;srl_xml_extension_put(&mut output," ",limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,&attribute,limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,"=\"",limits.max_bytes,operation.as_ref())?;srl_xml_extension_escape(&mut output,value,true,limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,"\"></",limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,&lexical,limits.max_bytes,operation.as_ref())?;srl_xml_extension_put(&mut output,">",limits.max_bytes,operation.as_ref())?;Ok(output)
}
/// Resolve the original occurrence's type before selecting an admitted derived declaration codec.
fn srl_xml_declaration_type(text:&str,limits:XmlLimits,operation:Option<@xml_operation@::State<'_>>)->Result<Option<(String,String)>,Error>{
 let mut input=SrlXmlInput::new_with_operation(text,limits,operation.as_ref().map(@xml_operation@::State::secondary))?;input.skip_space()?;
 let attributes=match &input.current{SrlXmlToken::Start(_,values)=>values,_=>return Err(Error("Missing declared XML root"))};
 let Some((_,value))=attributes.iter().find(|(name,_)|name.uri=="http://www.w3.org/2001/XMLSchema-instance"&&name.local=="type")else{return Ok(None)};
 let lexical=value.trim_matches([' ','\t','\r','\n']);let scope=srl_xml_extension_scope(&input)?;srl_xml_namespace_tokens(lexical,1,&scope,limits.max_nodes,&[])?;
 let(uri,local)=srl_xml_scalar_expanded(lexical,&scope);Ok(Some((srl_xml_extension_owned(uri,operation.as_ref())?,srl_xml_extension_owned(local,operation.as_ref())?)))
}
/// Validate nil and declared type before selecting the complete or attributes-only private DTO.
fn srl_xml_declaration_element(text:&str,limits:XmlLimits,nillable:bool,fixed:bool,declared_type:Option<(&str,&str)>,operation:Option<@xml_operation@::State<'_>>)->Result<(String,bool),Error>{
 let mut input=SrlXmlInput::new_with_operation(text,limits,operation.as_ref().map(@xml_operation@::State::secondary))?;input.skip_space()?;let scope=srl_xml_extension_scope(&input)?;
 let mut attributes=match std::mem::replace(&mut input.current,SrlXmlToken::Eof){SrlXmlToken::Start(name,attributes)=>{input.current=SrlXmlToken::Start(name,Vec::new());attributes},_=>return Err(Error("Missing declared XML root"))};let mut nil=false;
 for(name,value)in &attributes{if name.uri!="http://www.w3.org/2001/XMLSchema-instance"{continue;}
  if name.local=="type"{let expected=declared_type.ok_or(Error("Anonymous XML type cannot be named"))?;srl_xml_namespace_value(value,1,&scope,limits.max_nodes,&[&[expected]],None,operation.as_ref())?;}
  if name.local=="nil"{let lexical=value.trim_matches([' ','\t','\r','\n']);if !nillable||!matches!(lexical,"true"|"1"|"false"|"0"){return Err(Error("Invalid declared XML nil marker"));}nil=matches!(lexical,"true"|"1");if nil&&fixed{return Err(Error("Fixed XML element cannot be nil"));}}
 }
 attributes.retain(|(name,_)|name.uri!="http://www.w3.org/2001/XMLSchema-instance"||!matches!(name.local,"type"|"nil"));
 let mut output=String::new();let lexical=srl_xml_extension_start(&mut output,&input,&attributes,Some(&scope),limits.max_bytes)?;input.advance()?;
 loop{match &input.current{
  SrlXmlToken::Start(..)=>{if nil{return Err(Error("Nil XML element has children"));}srl_xml_extension_foreign(&mut input,Some(&mut output),limits.max_bytes,false)?;},
  SrlXmlToken::Text(value)=>{if nil&&!value.is_empty(){return Err(Error("Nil XML element has text"));}srl_xml_extension_escape(&mut output,value,false,limits.max_bytes,operation.as_ref())?;input.advance()?;},
  SrlXmlToken::End(_)=>{srl_xml_extension_end(&mut output,&lexical,limits.max_bytes,operation.as_ref())?;input.advance()?;input.finish_document()?;return Ok((output,nil));},
  SrlXmlToken::Eof=>return Err(Error("Unterminated declared XML element")),
 }}
}
)rustdecl"
