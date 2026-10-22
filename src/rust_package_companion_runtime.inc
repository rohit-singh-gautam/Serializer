// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustpkgxml(
    /// Keep XML-only state beside the package, leaving native model and ModelPart layouts intact.
    #[derive(Default)]
    pub struct XmlCompanions {roles:BTreeMap<&'static str,BTreeMap<String,super::XmlExtensionState>>}
    impl XmlCompanions {
        /// Borrow an original physical occurrence; singleton roles use an empty occurrence key.
        pub fn get(&self,role:&str,part:&str)->Option<&super::XmlExtensionState>{self.roles.get(role).and_then(|parts|parts.get(part).or_else(||parts.iter().find(|(name,_)|name.eq_ignore_ascii_case(part)).map(|(_,state)|state)))}
        /// Bind caller-authored presentation outside a running operation, without copying the model.
        pub fn insert(&mut self,role:&'static str,part:String,state:super::XmlExtensionState)->Option<super::XmlExtensionState>{self.roles.entry(role).or_default().insert(part,state)}
        /// Follow an explicit destination move back to the stable original part identity.
        pub(super) fn selected(&self,role:&str,part:&str,singleton:bool,renames:&[PartRename])->Option<&super::XmlExtensionState>{
            if singleton{return self.get(role,"");}
            let original=renames.iter().find(|rename|rename.role==role&&rename.destination.eq_ignore_ascii_case(part)).map_or(part,|rename|rename.original.as_str());self.get(role,original)
        }
    }
    impl XmlOperation<'_,'_> {
        /// Borrow an immutable native payload and its original XML scopes through one synchronous ledger.
        pub fn encode_document<T:native_operation::DocumentCodec>(&mut self,value:&T,extensions:Option<&super::XmlExtensionState>)->Result<String>{
            let limits=self.budget.xml_limits();let result=<T as native_operation::DocumentCodec>::encode_document_with_operation(value,extensions,native_operation::Options{limits,..Default::default()},self);self.native_result(result)
        }
        /// Construct model and presentation privately; neither escapes when native or package admission fails.
        pub fn decode_document<T:native_operation::DocumentCodec>(&mut self,text:&str)->Result<super::XmlDocument<T>>{
            let limits=self.budget.xml_limits();let expected=self.role.expected_type;let result=<T as native_operation::DocumentCodec>::decode_document_with_operation(text,native_operation::Options{limits,..Default::default()},expected,self);self.native_result(result)
        }
        /// Capture presentation for an unchanged caller-selected decoder without constructing a payload copy.
        pub fn prepare_document<T:native_operation::DocumentCodec>(&mut self,text:&str)->Result<super::XmlExtensionState>{
            let limits=self.budget.xml_limits();let result=<T as native_operation::DocumentCodec>::prepare_document_with_operation(text,native_operation::Options{limits,..Default::default()},self);self.native_result(result)
        }
        /// Admit owning sidecar association storage and preserved logical bytes before retaining it.
        pub(super) fn retain_document(&mut self,companions:&mut XmlCompanions,singleton:bool,state:super::XmlExtensionState)->Result<()>{
            let part=if singleton{""}else{self.part};let role=self.role.field;
            self.budget.consume(Resource::WorkUnits,companions.roles.len().saturating_add(part.len()).saturating_add(1),self.part,role,self.link)?;
            if !companions.roles.contains_key(role){self.budget.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<(&str,BTreeMap<String,super::XmlExtensionState>)>(),self.part,role,self.link)?;}
            let additional=std::mem::size_of::<(String,super::XmlExtensionState)>().checked_add(part.len()).ok_or_else(||fail(Code::ResourceLimit,"XML companion association limit",self.part,role,self.link))?;
            self.budget.consume(Resource::DecoderAllocationBytes,additional,self.part,role,self.link)?;
            let mut name=String::new();name.try_reserve_exact(part.len()).map_err(native_error)?;name.push_str(part);companions.insert(role,name,state);Ok(())
        }
    }
)rustpkgxml"
