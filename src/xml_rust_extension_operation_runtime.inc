// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustextop(
/// Admit one actual companion string before obtaining independent ownership.
fn srl_xml_extension_owned(value:&str,operation:Option<&@xml_operation@::State<'_>>)->Result<String,Error>{
 @xml_operation@::consume(operation,@xml_operation@::Resource::WorkUnits,value.len().saturating_add(1))?;
 @xml_operation@::consume(operation,@xml_operation@::Resource::DecoderAllocationBytes,value.len())?;
 let mut result=String::new();result.try_reserve_exact(value.len()).map_err(|_|@xml_operation@::quota(operation,"XML extension allocation"))?;result.push_str(value);Ok(result)
}
/// Admit vector capacity growth before allocation, retaining initialized storage on failure.
fn srl_xml_extension_reserve<T>(values:&mut Vec<T>,additional:usize,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error>{
 let required=values.len().checked_add(additional).ok_or_else(||@xml_operation@::quota(operation,"XML extension allocation"))?;
 if required>values.capacity(){let capacity=required.max(values.capacity().checked_mul(2).unwrap_or(required));let bytes=capacity.checked_sub(values.capacity()).and_then(|count|count.checked_mul(std::mem::size_of::<T>())).ok_or_else(||@xml_operation@::quota(operation,"XML extension allocation"))?;
 @xml_operation@::consume(operation,@xml_operation@::Resource::DecoderAllocationBytes,bytes)?;values.try_reserve_exact(capacity-values.len()).map_err(|_|@xml_operation@::quota(operation,"XML extension allocation"))?;}Ok(())
}
/// Allocate fixed working inventories without unadmitted vec! storage.
fn srl_xml_extension_zeros<T:Default+Clone>(count:usize,operation:Option<&@xml_operation@::State<'_>>)->Result<Vec<T>,Error>{
 let mut result=Vec::new();srl_xml_extension_reserve(&mut result,count,operation)?;result.resize(count,T::default());Ok(result)
}
/// Retain map metadata under the same operation before copying namespace key/value bytes.
fn srl_xml_extension_binding(values:&mut std::collections::BTreeMap<String,String>,prefix:&str,uri:&str,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error>{
 @xml_operation@::consume(operation,@xml_operation@::Resource::WorkUnits,values.len().saturating_add(1))?;
 if !values.contains_key(prefix){@xml_operation@::consume(operation,@xml_operation@::Resource::DecoderAllocationBytes,std::mem::size_of::<(String,String)>())?;}
 let key=srl_xml_extension_owned(prefix,operation)?;let value=srl_xml_extension_owned(uri,operation)?;values.insert(key,value);Ok(())
}
/// Copy a complete saved lexical scope while charging each owned vector slot and string.
fn srl_xml_extension_scope_copy(scope:&[(String,String)],operation:Option<&@xml_operation@::State<'_>>)->Result<Vec<(String,String)>,Error>{
 let mut result=Vec::new();srl_xml_extension_reserve(&mut result,scope.len(),operation)?;
 for(prefix,uri)in scope{result.push((srl_xml_extension_owned(prefix,operation)?,srl_xml_extension_owned(uri,operation)?));}Ok(result)
}
/// Copy collision-free map/ordinal ownership paths without borrowing completed XML or payloads.
fn srl_xml_extension_path_copy(path:&[(usize,SrlXmlExtensionIndex)],operation:Option<&@xml_operation@::State<'_>>)->Result<Vec<(usize,SrlXmlExtensionIndex)>,Error>{
 let mut result=Vec::new();srl_xml_extension_reserve(&mut result,path.len(),operation)?;
 for(field,index)in path{let index=match index{SrlXmlExtensionIndex::Ordinal(index)=>SrlXmlExtensionIndex::Ordinal(*index),SrlXmlExtensionIndex::Key(value)=>SrlXmlExtensionIndex::Key(srl_xml_extension_owned(value,operation)?)};result.push((*field,index));}Ok(result)
}
/// Compose a qualified framing name without an unadmitted format! temporary.
fn srl_xml_extension_lexical(prefix:&str,local:&str,operation:Option<&@xml_operation@::State<'_>>)->Result<String,Error>{
 let count=prefix.len().checked_add(local.len()).and_then(|count|count.checked_add(usize::from(!prefix.is_empty()))).ok_or_else(||@xml_operation@::quota(operation,"XML extension name limit"))?;
 @xml_operation@::consume(operation,@xml_operation@::Resource::WorkUnits,count)?;@xml_operation@::consume(operation,@xml_operation@::Resource::DecoderAllocationBytes,count)?;
 let mut result=String::new();result.try_reserve_exact(count).map_err(|_|@xml_operation@::quota(operation,"XML extension allocation"))?;
 if !prefix.is_empty(){result.push_str(prefix);result.push(':');}result.push_str(local);Ok(result)
}
/// Join canonical scalar components after admitting their exact owned length.
fn srl_xml_extension_join(first:&str,second:&str,operation:Option<&@xml_operation@::State<'_>>)->Result<String,Error>{
 let count=first.len().checked_add(second.len()).ok_or(Error("XML extension scalar limit"))?;
 @xml_operation@::consume(operation,@xml_operation@::Resource::WorkUnits,count)?;@xml_operation@::consume(operation,@xml_operation@::Resource::DecoderAllocationBytes,count)?;
 let mut result=String::new();result.try_reserve_exact(count).map_err(|_|@xml_operation@::quota(operation,"XML extension allocation"))?;result.push_str(first);result.push_str(second);Ok(result)
}
/// Format a collision candidate in fixed stack storage before admitting its owning copy.
fn srl_xml_extension_alias(prefix:&str,mut ordinal:usize,operation:Option<&@xml_operation@::State<'_>>)->Result<String,Error>{
 const DECIMAL_CAPACITY:usize=std::mem::size_of::<usize>()*3;let mut digits=[0u8;DECIMAL_CAPACITY];let mut begin=digits.len();
 loop{begin-=1;digits[begin]=b'0'+(ordinal%10)as u8;ordinal/=10;if ordinal==0{break;}}
 let suffix=std::str::from_utf8(&digits[begin..]).map_err(|_|Error("XML extension alias"))?;srl_xml_extension_join(prefix,suffix,operation)
}
/// Validate and consume only the selected root capability declaration in its original lexical scope.
fn srl_xml_extension_package_capabilities(input:&mut SrlXmlInput<'_,'_>,package:Option<@xml_operation@::PackageContract>)->Result<(),Error>{
 let Some(contract)=package else{return Ok(())};
 let attributes=match &input.current{SrlXmlToken::Start(_,values)=>values,_=>return Err(Error("Package XML root missing"))};
 if contract.requires_type&&!attributes.iter().any(|(name,_)|name.uri=="http://www.w3.org/2001/XMLSchema-instance"&&name.local=="type"){return Err(Error("Required package XML type is missing"));}
 if let Some((_,lexical))=attributes.iter().find(|(name,_)|name.uri=="http://schemas.openxmlformats.org/markup-compatibility/2006"&&name.local=="MustUnderstand"){
  let mut count=0usize;for prefix in lexical.split([' ','\t','\r','\n']).filter(|value|!value.is_empty()){
   @xml_operation@::consume(input.operation.as_ref(),@xml_operation@::Resource::WorkUnits,prefix.len().saturating_add(1))?;
   if !prefix.chars().enumerate().all(|(index,value)|srl_xml_scalar_name_character(value as u32,index==0,false)){return Err(Error("Invalid package capability prefix"));}
   let uri=input.namespace(prefix,false)?;let mut understood=false;
   for candidate in contract.vocabularies{@xml_operation@::consume(input.operation.as_ref(),@xml_operation@::Resource::WorkUnits,uri.len().saturating_add(candidate.len()))?;if uri==*candidate{understood=true;break;}}
   if uri.is_empty()||!understood{return Err(Error("Required package vocabulary is not understood"));}count+=1;
  }
  if count==0{return Err(Error("Empty package capability declaration"));}
  if let SrlXmlToken::Start(_,values)=&mut input.current{values.retain(|(name,_)|name.uri!="http://schemas.openxmlformats.org/markup-compatibility/2006"||name.local!="MustUnderstand");}
 }Ok(())
}
)rustextop"
