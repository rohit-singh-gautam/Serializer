# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Require private wildcard identity validators to inherit the outer identity policy."""
import xml_ordered_portable_test as ordered
import xml_wildcard_validation_test as validation


def configure():
    """Keep independent XSD decisions separate from selected native resource admission."""
    validation.FOREIGN_XSD = f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
      xmlns:f="{validation.FOREIGN}" targetNamespace="{validation.FOREIGN}" elementFormDefault="qualified">
      <xs:element name="identified"><xs:complexType><xs:sequence>
        <xs:element name="entry" minOccurs="1" maxOccurs="unbounded"><xs:complexType>
          <xs:attribute name="id" type="xs:int" use="required"/>
        </xs:complexType></xs:element>
      </xs:sequence></xs:complexType><xs:key name="entryKey">
        <xs:selector xpath="f:entry"/><xs:field xpath="@id"/>
      </xs:key></xs:element>
      <xs:element name="nested"><xs:complexType><xs:sequence>
        <xs:any namespace="{validation.FOREIGN}" processContents="strict" minOccurs="1" maxOccurs="1"/>
      </xs:sequence></xs:complexType></xs:element></xs:schema>'''
    validation.configure()
    ordered.PYTHON = validation.PYTHON_CONSUMER
    ordered.JAVASCRIPT = validation.JAVASCRIPT_CONSUMER
    ordered.KOTLIN = validation.KOTLIN_CONSUMER
    ordered.SWIFT = validation.SWIFT_CONSUMER
    ordered.PYTHON = ordered.PYTHON.replace('    path.with_name', '''    zero = Limits(xml_identity=XmlIdentityLimits(max_tuples=0))
    for operation in (lambda: kind.decode_xml(path.read_bytes(), zero), lambda: value.encode_xml(zero)):
        try:
            operation()
        except ValueError:
            pass
        else:
            raise AssertionError('Private declaration reset identity policy')
    path.with_name''')
    ordered.JAVASCRIPT = ordered.JAVASCRIPT.replace('  writeFileSync', '''  const zero=new M.XmlLimits({identity:new M.XmlIdentityLimits({maxTuples:0})});
  for(const operation of [()=>Kind.decodeXml(readFileSync(path,'utf8'),zero),()=>value.encodeXml(zero)]){
    let rejected=false;try{operation();}catch{rejected=true;}assert.deepEqual(rejected,true,'Private declaration reset identity policy');
  }
  writeFileSync''')
    ordered.SWIFT = ordered.SWIFT.replace('  try require(valid);try Data(output)', '''  try require(valid)
  let value=try OrderedRoot.decodeXml(input)
  for operation in [{_ = try OrderedRoot.decodeXml(input,identityLimits:XmlIdentityLimits(maxTuples:0))},
                    {_ = try value.encodeXml(identityLimits:XmlIdentityLimits(maxTuples:0))}] {
    var rejected=false;do{try operation()}catch{rejected=true};try require(rejected)
  }
  try Data(output)''')
    prefix, _ = ordered.MODEL.rsplit('}', 1)
    ordered.MODEL = prefix + '''package archive { part main : root { name("/main.xml"); content_type("application/vnd.serializer.identity"); relationship("urn:serializer:identity:main"); policy(defaults); } }\n}\n'''
    ordered.C_CHECK = ordered.C_CHECK.replace('done:ordered_KIND_xml_document_free', '''if(!strcmp("KIND","root")&&status==srl_ok){
  const srl_xml_package_document_codec* codec=srl_xml_package_document_ordered_root();srl_limits bounds=srl_default_limits();
  srl_xml_identity_limits identity=srl_xml_default_identity_limits();srl_xml_operation_policies policies={0};policies.identity=&identity;
  const srl_xml_package_contract contract={false,NULL,0};srl_xml_operation operation={0};operation.policies=&policies;operation.package_contract=&contract;
  for(size_t selected=0;selected<3;++selected){identity.max_tuples=selected==1?0:srl_xml_default_identity_limits().max_tuples;
    for(size_t direction=0;direction<3;++direction){ordered_root candidate={0};srl_xml_extension_state* retained=NULL;srl_buffer encoded={0};
      operation.failure=srl_xml_operation_none;operation.caller_status=0;
      srl_status result=direction==0?codec->decode(&candidate,&retained,input->data,input->size,&bounds,NULL,&operation):direction==1?codec->encode(&value.value,value.extensions,&encoded,&bounds,NULL,&operation):codec->prepare(&retained,input->data,input->size,&bounds,NULL,&operation);
      require((result==srl_ok)==(selected!=1));require(operation.policies==&policies&&operation.package_contract==&contract);
      if(selected==1){require(result==srl_limit&&operation.failure==srl_xml_operation_resource_limit);require(!encoded.data&&!retained&&!candidate.first.data&&!candidate.second.data);}
      ordered_root_free(&candidate);srl_xml_extension_state_free(retained);srl_buffer_free(&encoded);
    }
  }
}
done:ordered_KIND_xml_document_free''')
    base_consumer = ordered.consumer

    def consumer(language, grouped=False):
        """Inject policy checks into the valid owning branch without changing validity catches."""
        source = base_consumer(language, grouped)
        if language == 'java':
            source = source.replace('var copy=Schema.Ordered.Root.decodeXml(output);', '''var zero=new Schema.XmlIdentityLimits(128,16,64,100000,0,8388608,1000000);
for(Runnable operation:java.util.List.<Runnable>of(
  ()->Schema.Ordered.Root.decodeXml(output,Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),zero),
  ()->value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),zero))){
  boolean rejected=false;try{operation.run();}catch(IllegalArgumentException expected){rejected=true;}
  if(!rejected)throw new AssertionError("Private declaration reset identity policy");
}
var copy=Schema.Ordered.Root.decodeXml(output);''')
        elif language == 'csharp':
            source = source.replace('var copy=OrderedRoot.DecodeXml(output);', '''var zero=new XmlIdentityLimits(128,16,64,100000,0,8388608,1000000);
foreach(Action operation in new Action[]{()=>OrderedRoot.DecodeXml(File.ReadAllBytes(path),new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),zero),()=>value.EncodeXml(new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),zero)}){
  bool rejected=false;try{operation();}catch(Exception){rejected=true;}if(!rejected)throw new Exception("Private declaration reset identity policy");
}
var copy=OrderedRoot.DecodeXml(output);''')
        elif language == 'go':
            source = source.replace('copy,err:=DecodeOrderedRootXML(output);if err!=nil{return nil,err}', '''copy,err:=DecodeOrderedRootXML(output);if err!=nil{return nil,err}
zero:=DefaultXmlIdentityLimits();zero.MaxTuples=0
if _,err:=DecodeOrderedRootXMLWithIdentityLimits(input,zero);err==nil{panic("Private declaration reset decode identity policy")}
if _,err:=value.EncodeXMLWithIdentityLimits(zero);err==nil{panic("Private declaration reset encode identity policy")}
if _,err:=value.EncodeXML();err!=nil{return nil,err}''')
        elif language == 'rust':
            source += '''
/// Exercise public operation policies without changing old native codec signatures.
#[derive(Debug)]
struct IdentityPolicyFailure;
/// Keep selected policy failure distinguishable from ordinary declaration invalidity.
impl std::fmt::Display for IdentityPolicyFailure{fn fmt(&self,output:&mut std::fmt::Formatter<'_>)->std::fmt::Result{output.write_str("Selected identity policy")}}
impl std::error::Error for IdentityPolicyFailure{}
struct IdentityAdmission;
impl serializer_xml_operation::Operation for IdentityAdmission{
 /// Admit shared resources so this probe isolates the independent identity selector.
 fn consume(&mut self,_:serializer_xml_operation::Resource,_:usize,_:&str)->serializer_xml_operation::Result<()>{Ok(())}
 /// Keep cancellation independent of the selected identity policy.
 fn poll(&mut self,_:&str)->serializer_xml_operation::Result<()>{Ok(())}
 /// Return a concrete application failure for the identity quota boundary.
 fn limit(&mut self,_:&'static str,_:&str)->serializer_xml_operation::Failure{std::sync::Arc::new(IdentityPolicyFailure)}
}
'''
            source = source.replace('let copy=OrderedRoot::decode_xml_document(&output,XmlLimits::default(),None,&options)?;', '''let copy=OrderedRoot::decode_xml_document(&output,XmlLimits::default(),None,&options)?;
use serializer_xml_operation::{DocumentCodec,Options};
for selected in [false,true,false]{
 let mut options=Options::default();if selected{options.identity_limits.max_tuples=0;}
 let result=<OrderedRoot as DocumentCodec>::decode_document_with_operation(&input,options,None,&mut IdentityAdmission);assert_eq!(result.is_err(),selected,"Private declaration reset decode identity policy");if let Err(error)=result{assert!(error.downcast_ref::<IdentityPolicyFailure>().is_some());}
 let mut options=Options::default();if selected{options.identity_limits.max_tuples=0;}
 let result=document.value.encode_document_with_operation(Some(&document.extensions),options,&mut IdentityAdmission);assert_eq!(result.is_err(),selected,"Private declaration reset encode identity policy");if let Err(error)=result{assert!(error.downcast_ref::<IdentityPolicyFailure>().is_some());}
 let mut options=Options::default();if selected{options.identity_limits.max_tuples=0;}
 let result=<OrderedRoot as DocumentCodec>::prepare_document_with_operation(&input,options,&mut IdentityAdmission);assert_eq!(result.is_err(),selected,"Private declaration reset prepare identity policy");if let Err(error)=result{assert!(error.downcast_ref::<IdentityPolicyFailure>().is_some());}
}''')
        elif language == 'kotlin':
            source = source.replace('val copy=OrderedRoot.decodeXml(output)', '''val zero=XmlIdentityLimits(maxTuples=0)
for(operation in listOf<()->Unit>({OrderedRoot.decodeXml(Files.readAllBytes(path),identityLimits=zero);Unit},{value.encodeXml(identityLimits=zero);Unit})){
  var rejected=false;try{operation()}catch(expected:IllegalArgumentException){rejected=true};check(rejected){"Private declaration reset identity policy"}
}
val copy=OrderedRoot.decodeXml(output)''')
        return source

    ordered.consumer = consumer

    def cases():
        """Only identity validity differs; namespace, field and wildcard structure stay identical."""
        prefix = f'<c:root xmlns:c="{validation.CORE}" xmlns:f="{validation.FOREIGN}"><c:first>A</c:first><c:second>B</c:second><f:identified>'
        suffix = '</f:identified></c:root>'
        result = [('root-key-valid', prefix + '<f:entry id="1"/><f:entry id="2"/>' + suffix, 64, True),
                ('root-key-duplicate', prefix + '<f:entry id="1"/><f:entry id="1"/>' + suffix, 64, False)]
        return result + [(name.replace('root-key', 'root-nested-key'), text.replace('<f:identified>', '<f:nested><f:identified>').replace('</f:identified>', '</f:identified></f:nested>'), maximum, valid) for name, text, maximum, valid in result]

    ordered.cases = cases


if __name__ == '__main__':
    configure()
    ordered.main()
