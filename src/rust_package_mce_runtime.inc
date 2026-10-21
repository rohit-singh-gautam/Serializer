// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustpackage(
    const MC:&str="http://schemas.openxmlformats.org/markup-compatibility/2006";
    /// Retain borrowed expanded identities while projecting only selected Relationships markup.
    struct CompatibilityNode<'a,'input:'a>{node:roxmltree::Node<'a,'input>,attributes:Vec<roxmltree::Attribute<'a,'input>>,children:Vec<Self>,text:bool}
    enum CompatibilityRule<'a>{Ignore(&'a str),Process(&'a str,&'a str)}
    /// Split exactly XML whitespace without allocating a second token inventory.
    fn compatibility_tokens(text:&str)->impl Iterator<Item=&str>{text.split(|value|matches!(value,' '| '\t'|'\r'|'\n')).filter(|value|!value.is_empty())}
    /// Validate XML fifth-edition NCNames, including non-ASCII prefix spelling.
    fn compatibility_name(text:&str)->bool{!text.is_empty()&&text.chars().enumerate().all(|(index,value)|super::srl_xml_scalar_name_character(value as u32,index==0,false))}
    /// Resolve each capability in its original scope and account the namespace search depth.
    fn compatibility_prefix<'a,'input:'a>(node:roxmltree::Node<'a,'input>,token:&str,part:&str,budget:&mut Budget<'_>)->Result<&'a str>{
        budget.consume(Resource::WorkUnits,token.len()+node.ancestors().count(),part,"compatibility","")?;
        node.lookup_namespace_uri(Some(token)).filter(|uri|compatibility_name(token)&&!uri.is_empty()&&*uri!=MC).ok_or_else(||fail(Code::InvalidMetadata,"Invalid compatibility namespace prefix",part,"compatibility",""))
    }
    /// Search inherited URI identities without copying accumulated scopes at each depth.
    fn compatibility_ignored(rules:&[CompatibilityRule<'_>],uri:&str,part:&str,budget:&mut Budget<'_>)->Result<bool>{
        for rule in rules.iter().rev(){budget.consume(Resource::WorkUnits,1,part,"compatibility","")?;if matches!(rule,CompatibilityRule::Ignore(value)if *value==uri){return Ok(true);}}Ok(false)
    }
    /// Match ProcessContent expanded names or namespace wildcards after prefix resolution.
    fn compatibility_process(rules:&[CompatibilityRule<'_>],uri:&str,local:&str,part:&str,budget:&mut Budget<'_>)->Result<bool>{
        for rule in rules.iter().rev(){budget.consume(Resource::WorkUnits,1,part,"compatibility","")?;if matches!(rule,CompatibilityRule::Process(space,name)if *space==uri&&(*name=="*"||*name==local)){return Ok(true);}}Ok(false)
    }
    /// Validate declarations before selection; ignored content cannot introduce malformed directives.
    fn compatibility_declarations<'a,'input:'a>(node:roxmltree::Node<'a,'input>,rules:&mut Vec<CompatibilityRule<'a>>,part:&str,budget:&mut Budget<'_>)->Result<()>{
        if let Some(value)=node.attribute((MC,"Ignorable")){for token in compatibility_tokens(value){let uri=compatibility_prefix(node,token,part,budget)?;budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<CompatibilityRule<'_>>(),part,"compatibility","")?;rules.push(CompatibilityRule::Ignore(uri));}}
        for attribute in node.attributes().filter(|attribute|attribute.namespace()==Some(MC)){
            budget.consume(Resource::WorkUnits,attribute.value().len()+1,part,"compatibility","")?;
            match attribute.name(){
                "Ignorable"=>(),
                "MustUnderstand"=>{for token in compatibility_tokens(attribute.value()){compatibility_prefix(node,token,part,budget)?;}},
                "ProcessContent"=>{for token in compatibility_tokens(attribute.value()){
                    let(prefix,local)=token.split_once(':').filter(|(_,local)|*local=="*"||compatibility_name(local)).ok_or_else(||fail(Code::InvalidMetadata,"Invalid ProcessContent name",part,"compatibility",""))?;
                    let uri=compatibility_prefix(node,prefix,part,budget)?;
                    if !compatibility_ignored(rules,uri,part,budget)?{return Err(fail(Code::InvalidMetadata,"ProcessContent namespace is not ignorable",part,"compatibility",""));}
                    budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<CompatibilityRule<'_>>(),part,"compatibility","")?;rules.push(CompatibilityRule::Process(uri,local));
                }},
                _=>return Err(fail(Code::InvalidMetadata,"Unknown compatibility attribute",part,"compatibility",""))
            }
        }Ok(())
    }
    /// Enforce required capabilities only for retained or selected content.
    fn compatibility_must(node:roxmltree::Node<'_,'_>,part:&str,budget:&mut Budget<'_>)->Result<()>{
        if let Some(value)=node.attribute((MC,"MustUnderstand")){for token in compatibility_tokens(value){if compatibility_prefix(node,token,part,budget)?!=RELS{return Err(fail(Code::InvalidMetadata,"Unsupported required compatibility namespace",part,"compatibility",""));}}}Ok(())
    }
    /// Check all wrapper attributes, including wrappers whose descendant payload is not selected.
    fn compatibility_wrapper(node:roxmltree::Node<'_,'_>,rules:&[CompatibilityRule<'_>],choice:bool,part:&str,budget:&mut Budget<'_>)->Result<bool>{
        for attribute in node.attributes(){
            if let Some(uri)=attribute.namespace(){if uri!=MC&&(uri=="http://www.w3.org/XML/1998/namespace"||!compatibility_ignored(rules,uri,part,budget)?){return Err(fail(Code::InvalidMetadata,"Unexpected qualified compatibility wrapper attribute",part,"compatibility",""));}}
            else if !choice||attribute.name()!="Requires"{return Err(fail(Code::InvalidMetadata,"Unexpected compatibility wrapper attribute",part,"compatibility",""));}
        }
        if !choice{return Ok(false);}
        let value=node.attribute("Requires").filter(|value|compatibility_tokens(value).next().is_some()).ok_or_else(||fail(Code::InvalidMetadata,"Choice requires namespace prefixes",part,"compatibility",""))?;
        let mut supported=true;for token in compatibility_tokens(value){if compatibility_prefix(node,token,part,budget)?!=RELS{supported=false;}}Ok(supported)
    }
)rustpackage"
R"rustpackage(
    /// Project a selected subtree; text is retained as a grammar violation, never silently discarded.
    fn compatibility_children<'a,'input:'a>(node:roxmltree::Node<'a,'input>,rules:&mut Vec<CompatibilityRule<'a>>,part:&str,budget:&mut Budget<'_>)->Result<(Vec<CompatibilityNode<'a,'input>>,bool)>{
        let mut output=Vec::new();let mut text=node.children().any(|child|child.is_text()&&child.text().is_some_and(|value|compatibility_tokens(value).next().is_some()));
        for child in node.children().filter(|child|child.is_element()){
            let(children,extra_text)=compatibility_visit(child,rules,false,part,budget)?;text|=extra_text;
            budget.consume(Resource::DecoderAllocationBytes,children.len()*std::mem::size_of::<CompatibilityNode<'_,'_>>(),part,"compatibility","")?;output.extend(children);
        }Ok((output,text))
    }
    /// Apply fifth-edition Relationships preprocessing with scoped rules and first-understood Choice.
    fn compatibility_visit<'a,'input:'a>(node:roxmltree::Node<'a,'input>,rules:&mut Vec<CompatibilityRule<'a>>,selected:bool,part:&str,budget:&mut Budget<'_>)->Result<(Vec<CompatibilityNode<'a,'input>>,bool)>{
        budget.consume(Resource::WorkUnits,1,part,"compatibility","")?;let original=rules.len();compatibility_declarations(node,rules,part,budget)?;
        let uri=node.tag_name().namespace().unwrap_or("");let local=node.tag_name().name();
        let result=if uri!=RELS&&compatibility_ignored(rules,uri,part,budget)?{
            if !compatibility_process(rules,uri,local,part,budget)?{Ok((Vec::new(),false))}else{
                if node.attributes().any(|attribute|attribute.namespace()==Some("http://www.w3.org/XML/1998/namespace")&&matches!(attribute.name(),"base"|"lang"|"space")){return Err(fail(Code::InvalidMetadata,"XML context on unwrapped compatibility element",part,"compatibility",""));}
                compatibility_must(node,part,budget)?;compatibility_children(node,rules,part,budget)
            }
        }else if uri==MC{
            if local=="AlternateContent"{
                compatibility_wrapper(node,rules,false,part,budget)?;
                if node.children().any(|child|child.is_text()&&child.text().is_some_and(|value|compatibility_tokens(value).next().is_some())){return Err(fail(Code::InvalidMetadata,"Text in AlternateContent",part,"compatibility",""));}
                let mut choices=0;let mut fallback=false;let mut branch=None;
                for child in node.children().filter(|child|child.is_element()){
                    let boundary=rules.len();compatibility_declarations(child,rules,part,budget)?;
                    let child_uri=child.tag_name().namespace().unwrap_or("");let child_local=child.tag_name().name();
                    if child_uri!=RELS&&compatibility_ignored(rules,child_uri,part,budget)?&&!compatibility_process(rules,child_uri,child_local,part,budget)?{rules.truncate(boundary);continue;}
                    if child_uri!=MC{return Err(fail(Code::InvalidMetadata,"Unexpected AlternateContent child",part,"compatibility",""));}
                    if child_local=="Choice"&&!fallback{choices+=1;let understood=compatibility_wrapper(child,rules,true,part,budget)?;if branch.is_none()&&understood{branch=Some(child);}}
                    else if child_local=="Fallback"&&choices>0&&!fallback{fallback=true;compatibility_wrapper(child,rules,false,part,budget)?;if branch.is_none(){branch=Some(child);}}
                    else{return Err(fail(Code::InvalidMetadata,"Invalid Choice/Fallback order",part,"compatibility",""));}
                    rules.truncate(boundary);
                }
                if choices==0{return Err(fail(Code::InvalidMetadata,"AlternateContent has no Choice",part,"compatibility",""));}compatibility_must(node,part,budget)?;
                if let Some(branch)=branch{compatibility_visit(branch,rules,true,part,budget)}else{Ok((Vec::new(),false))}
            }else{
                if !selected||!matches!(local,"Choice"|"Fallback"){return Err(fail(Code::InvalidMetadata,"Unexpected compatibility element",part,"compatibility",""));}
                compatibility_wrapper(node,rules,local=="Choice",part,budget)?;compatibility_must(node,part,budget)?;compatibility_children(node,rules,part,budget)
            }
        }else{
            compatibility_must(node,part,budget)?;let(children,text)=compatibility_children(node,rules,part,budget)?;let mut attributes=Vec::new();
            for attribute in node.attributes(){let uri=attribute.namespace().unwrap_or("");if uri==MC||uri!=RELS&&compatibility_ignored(rules,uri,part,budget)?{continue;}budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<roxmltree::Attribute<'_,'_>>(),part,"compatibility","")?;attributes.push(attribute);}
            budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<CompatibilityNode<'_,'_>>(),part,"compatibility","")?;
            Ok((vec![CompatibilityNode{node,attributes,children,text}],false))
        };rules.truncate(original);result
    }
    /// Decode only the one projected standard root, preserving unknown-core rejection after preprocessing.
    fn compatibility_relationships(document:&roxmltree::Document<'_>,part:&str,source:&str,budget:&mut Budget<'_>)->Result<Vec<Relationship>>{
        let(mut roots,text)=compatibility_visit(document.root_element(),&mut Vec::new(),false,part,budget)?;
        if text||roots.len()!=1{return Err(fail(Code::InvalidMetadata,"Compatibility output requires one root",part,"compatibility",""));}
        let root=roots.pop().unwrap();if root.node.tag_name().namespace()!=Some(RELS)||root.node.tag_name().name()!="Relationships"||!root.attributes.is_empty()||root.text{return Err(fail(Code::InvalidMetadata,"Invalid relationships root",part,"",""));}
        let mut output=Vec::new();for child in root.children{
            if child.node.tag_name().namespace()!=Some(RELS)||child.node.tag_name().name()!="Relationship"||!child.children.is_empty()||child.text||child.attributes.iter().any(|attribute|attribute.namespace().is_some()||!matches!(attribute.name(),"Id"|"Type"|"Target"|"TargetMode")){return Err(fail(Code::InvalidMetadata,"Unknown relationship declaration",part,"",""));}
            let mode=child.node.attribute("TargetMode").unwrap_or("Internal");if !matches!(mode,"Internal"|"External"){return Err(fail(Code::InvalidRelationship,"Invalid TargetMode",part,"",""));}
            budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<Relationship>(),part,"compatibility","")?;
            output.push(Relationship{source:source.into(),id:uri_attribute(child.node,"Id",part,budget)?,kind:uri_attribute(child.node,"Type",part,budget)?,target:uri_attribute(child.node,"Target",part,budget)?,external:mode=="External"});
        }Ok(output)
    }
)rustpackage"
