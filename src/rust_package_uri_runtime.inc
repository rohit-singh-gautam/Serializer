// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustpackage(
    /// Admit inert OPC ZIP items without changing ordinary ZIP path-safety defaults.
    fn zip_item_name(value:&str,opc:bool)->Result<()>{
        if !opc{return zip_name(value);}let segments=value.strip_suffix('/').unwrap_or(value);
        if value.is_empty()||value.len()>MAX_NAME_BYTES||value.starts_with('/')||value.contains(['\\','\0'])||segments.split('/').any(|part|part.is_empty()||part=="."||part==".."){return Err(fail(Code::InvalidName,"Invalid OPC physical ZIP name",value,"",""));}Ok(())
    }
    /// Recognize RFC 3986 unreserved ASCII bytes independently of locale.
    fn uri_unreserved(value:u8)->bool{value.is_ascii_alphanumeric()||b"-._~".contains(&value)}
    /// Check canonical ASCII OPC names without decoding encoded reserved bytes or filesystem paths.
    pub fn part_name(value:&str)->Result<String>{
        let invalid=||fail(Code::InvalidName,"Invalid OPC part URI",value,"","");
        if !value.starts_with('/')||value.len()<2||value.ends_with('/')||value.split('/').skip(1).any(|part|part.is_empty()||part.ends_with('.')){return Err(invalid());}
        zip_item_name(&value[1..],true)?;let data=value.as_bytes();let mut position=1;
        while position<data.len(){let byte=data[position];if byte==b'%'{
            if data.len()-position<3||!data[position+1..position+3].iter().all(u8::is_ascii_hexdigit){return Err(invalid());}
            let code=u8::from_str_radix(&value[position+1..position+3],16).map_err(|_|invalid())?;
            if uri_unreserved(code)||code==b'/'||code==b'\\'||code>=128{return Err(invalid());}position+=3;
        }else{if !uri_unreserved(byte)&&!b"/!$&'()*+,;=:@".contains(&byte){return Err(invalid());}position+=1;}}
        Ok(value.to_ascii_lowercase())
    }
    /// Distinguish unsupported Unicode candidates from ASCII physical entries that map to no part.
    fn mapped_part(value:&str)->Result<Option<String>>{
        let data=value.as_bytes();for (index,byte) in data.iter().enumerate(){
            if *byte>=128||*byte==b'%'&&data.len()-index>=3&&data[index+1].is_ascii_hexdigit()&&data[index+2].is_ascii_hexdigit()&&data[index+1].to_ascii_lowercase()>=b'8'{return Err(fail(Code::InvalidName,"Non-ASCII OPC logical naming is unsupported",value,"",""));}
        }Ok(part_name(value).ok())
    }
    /// Validate RFC 3987 iunreserved and query-only private Unicode scalar ranges.
    fn iri_character(value:u32,query:bool)->bool{
        (0xa0..=0xd7ff).contains(&value)||(0xf900..=0xfdcf).contains(&value)||(0xfdf0..=0xffef).contains(&value)||((0x10000..=0xdfffd).contains(&value)&&(value&0xffff)<=0xfffd)||(0xe1000..=0xefffd).contains(&value)||query&&((0xe000..=0xf8ff).contains(&value)||(0xf0000..=0xffffd).contains(&value)||(0x100000..=0x10fffd).contains(&value))
    }
    /// Check exact component delimiters and percent triplets without normalizing authored spelling.
    fn uri_component(value:&str,extra:&[u8],query:bool)->Result<()>{
        let invalid=||fail(Code::InvalidRelationship,"Invalid relationship IRI component","","","");let bytes=value.as_bytes();let mut index=0;
        while index<bytes.len(){let byte=bytes[index];if byte==b'%'{if bytes.len()-index<3||!bytes[index+1..index+3].iter().all(u8::is_ascii_hexdigit){return Err(invalid());}index+=3;}
            else if byte>=128{let scalar=value[index..].chars().next().ok_or_else(invalid)?;if !iri_character(scalar as u32,query){return Err(invalid());}index+=scalar.len_utf8();}
            else{if !uri_unreserved(byte)&&!b"!$&'()*+,;=".contains(&byte)&&!extra.contains(&byte){return Err(invalid());}index+=1;}}
        Ok(())
    }
    /// Validate a dotted IPv4 tail in an IPv6 literal, including decimal bounds and leading-zero rules.
    fn ipv4_literal(value:&str)->bool{let mut count=0;for piece in value.split('.'){
        if piece.is_empty()||piece.len()>3||piece.len()>1&&piece.starts_with('0')||!piece.bytes().all(|byte|byte.is_ascii_digit())||piece.parse::<u16>().map_or(true,|number|number>255){return false;}count+=1;}count==4
    }
    /// Recognize IPv6 and IPvFuture syntax without DNS, IP address conversion or network access.
    fn ip_literal(value:&str)->bool{
        if value.starts_with(['v','V']){return value[1..].split_once('.').is_some_and(|(version,address)|!version.is_empty()&&version.bytes().all(|byte|byte.is_ascii_hexdigit())&&!address.is_empty()&&address.bytes().all(|byte|uri_unreserved(byte)||b"!$&'()*+,;=:".contains(&byte)));}
        let compression=value.find("::");if compression.is_some_and(|index|value[index+2..].contains("::")){return false;}let bytes=value.as_bytes();let mut groups=0;let mut begin=0;
        while begin<bytes.len(){if bytes[begin]==b':'{if compression.is_none_or(|index|begin!=index&&begin!=index+1){return false;}begin+=1;continue;}
            let end=value[begin..].find(':').map(|index|begin+index);let piece=&value[begin..end.unwrap_or(value.len())];
            if piece.contains('.') {if end.is_some()||!ipv4_literal(piece){return false;}groups+=2;}
            else{if piece.is_empty()||piece.len()>4||!piece.bytes().all(|byte|byte.is_ascii_hexdigit()){return false;}groups+=1;}
            match end{None=>break,Some(index)=>{begin=index+1;if begin==value.len()&&compression.map(|value|value+1)!=Some(index){return false;}}}
        }if compression.is_some(){groups<8}else{groups==8}
    }
    /// Separate user-info, IP/reg-name host and decimal port before accepting an authority.
    fn uri_authority(mut value:&str)->Result<()>{
        let invalid=||fail(Code::InvalidRelationship,"Invalid relationship IRI authority","","","");
        if let Some((user,rest))=value.split_once('@'){uri_component(user,b":",false)?;value=rest;}
        let port=if let Some(address)=value.strip_prefix('['){let (address,rest)=address.split_once(']').ok_or_else(invalid)?;if !ip_literal(address)||!rest.is_empty()&&!rest.starts_with(':'){return Err(invalid());}rest.strip_prefix(':').unwrap_or("")}
        else{let(host,port)=value.split_once(':').unwrap_or((value,""));uri_component(host,b"",false)?;port};
        if !port.bytes().all(|byte|byte.is_ascii_digit()){return Err(invalid());}Ok(())
    }
    /// Apply RFC 3986/3987 component grammar, retaining references and absolute IRI text exactly.
    fn validate_iri(mut value:&str,absolute:bool)->Result<()>{
        let invalid=||fail(Code::InvalidRelationship,"Invalid relationship IRI","","","");
        if let Some((head,fragment))=value.split_once('#'){if absolute{return Err(invalid());}uri_component(fragment,b":@/?",false)?;value=head;}
        if let Some((head,query))=value.split_once('?'){uri_component(query,b":@/?",true)?;value=head;}
        if let Some(colon)=value.find(':').filter(|colon|*colon<value.find('/').unwrap_or(value.len())){
            let scheme=&value[..colon];if scheme.is_empty()||!scheme.bytes().enumerate().all(|(index,byte)|byte.is_ascii_alphabetic()||index>0&&(byte.is_ascii_digit()||b"+-.".contains(&byte))){return Err(invalid());}value=&value[colon+1..];
        }else if absolute{return Err(invalid());}
        if let Some(rest)=value.strip_prefix("//"){let(authority,path)=rest.find('/').map_or((rest,""),|index|(&rest[..index],&rest[index..]));uri_authority(authority)?;value=path;}
        uri_component(value,b":@/",false)
    }
    /// Validate an absolute, fragment-free relationship type without opening its namespace resource.
    fn uri(value:&str)->Result<()>{validate_iri(value,true)}
    /// Accept complete external IRI references, including the empty relative reference.
    fn iri_reference(value:&str)->Result<()>{validate_iri(value,false)}
    /// Resolve internal paths using literal RFC dot segments and inert selectors within the package.
    pub fn target(source:&str,value:&str)->Result<String>{target_policy(source,value,false)}
    /// Apply RFC root clamping only inside OPC, preserving the strict public path utility.
    fn opc_target(source:&str,value:&str)->Result<String>{target_policy(source,value,true)}
    /// Share literal component parsing between strict and OPC dot-segment policies.
    fn target_policy(source:&str,value:&str,clamp:bool)->Result<String>{
        validate_iri(value,false)?;let path=value.split(['?','#']).next().unwrap_or("");
        if path.is_empty()&&source!="/"{part_name(source)?;return Ok(source.into());}
        let invalid=||fail(Code::InvalidRelationship,"Invalid internal relationship target",source,"","");
        if path.is_empty()||path.starts_with("//")||path.contains("//")||path.ends_with('/')||path.ends_with("/.")||path.ends_with("/..")||path=="."||path==".."||!path.starts_with('/')&&path.split('/').next().is_some_and(|segment|segment.contains(':')){return Err(invalid());}
        let mut segments:Vec<&str>=if path.starts_with('/') {Vec::new()}else{source.rsplit_once('/').map_or(Vec::new(),|(parent,_)|parent.split('/').filter(|part|!part.is_empty()).collect())};
        for part in path.split('/'){match part{""|"."=>(),".."=>{if segments.pop().is_none()&&!clamp{return Err(invalid());}},_=>segments.push(part)}}
        let name="/".to_owned()+&segments.join("/");part_name(&name).map_err(|_|invalid())?;Ok(name)
    }
    /// Match the selected OPC ST_Extension syntax without decoding percent escapes.
    fn valid_extension(value:&str)->bool{let bytes=value.as_bytes();let mut index=0;if bytes.is_empty(){return false;}while index<bytes.len(){let byte=bytes[index];index+=1;if byte==b'%'{if bytes.len()-index<2||!bytes[index..index+2].iter().all(u8::is_ascii_hexdigit){return false;}index+=2;}else if byte==b'.'||!uri_unreserved(byte)&&!b"!$&'()*+,:=@".contains(&byte){return false;}}true}
    /// Apply XML Schema collapse only to attributes declared anyURI or ID, preserving MIME string spaces.
    fn uri_attribute(node:roxmltree::Node<'_,'_>,name:&str,part:&str,budget:&mut Budget<'_>)->Result<String>{
        let value=attribute(node,name,part)?;budget.consume(Resource::WorkUnits,value.len(),part,name,"")?;budget.consume(Resource::DecoderAllocationBytes,value.len(),part,name,"")?;
        let mut normalized=String::with_capacity(value.len());for piece in value.split(|value|matches!(value,' '| '\t'|'\r'|'\n')).filter(|piece|!piece.is_empty()){if !normalized.is_empty(){normalized.push(' ');}normalized.push_str(piece);}Ok(normalized)
    }
)rustpackage"
