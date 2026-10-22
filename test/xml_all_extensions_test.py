# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify all-group members followed by bounded foreign children and attributes."""
import xml_ordered_portable_test as ordered


def configure():
    """Reuse actual SDK compilation with a schema-bound unordered owning model."""
    ordered.MODEL = '''serializer version 1.21.0;
namespace ordered {
  using schema "all.xsd";
  class root { public string first; public optional<string> second; }
  class fixed { public string first; public optional<string> second; }
}
'''
    permission = '''extension.suffix.kind=element
extension.suffix.namespaces=urn:foreign
extension.suffix.policy=preserve
extension.suffix.minimum=1
extension.suffix.maximum=1
extension.attributes.kind=attribute
extension.attributes.namespaces=urn:foreign
extension.attributes.policy=preserve
extension.attributes.maximum=1
'''
    ordered.CONFIG = '[xml]\nenabled=true\n' + ''.join(
        f'[xml.class.ordered::{owner}]\n' + permission for owner in ('root', 'fixed'))
    xsd = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">' + ''.join(
        f'''<xs:element name="{owner}"><xs:complexType><xs:all>
<xs:element name="first" type="xs:string"/>
<xs:element name="second" type="xs:string" minOccurs="0"/>
</xs:all></xs:complexType></xs:element>''' for owner in ('root', 'fixed')) + '</xs:schema>'
    original_execute = ordered.execute

    def execute(args):
        """Supply the XSD before invoking the real schema compiler and SDK runners."""
        (args.build / 'all.xsd').write_text(xsd, encoding='utf-8')
        original_execute(args)

    ordered.execute = execute

    def cases():
        """Keep native permutation freedom separate from strict foreign suffix placement."""
        result = []
        for owner in ('root', 'fixed'):
            def add(name, children, valid=True, attributes='f:note="opaque"'):
                """Construct small fixtures with an independently known content decision."""
                result.append((owner + '-' + name,
                    f'<{owner} xmlns:f="urn:foreign" {attributes}>{children}</{owner}>', 16, valid))
            suffix = '<f:suffix f:attr="opaque">unbound:opaque<f:nested/>tail</f:suffix>'
            add('ordered', '<first>A&amp;B</first><second>C</second>' + suffix)
            add('permuted', '<second>C</second><first>A&amp;B</first>' + suffix)
            add('optional-absent', '<first>A</first>' + suffix)
            add('required-missing', '<second>C</second>' + suffix, False)
            add('duplicate', '<first>A</first><first>B</first>' + suffix, False)
            add('missing-suffix', '<first>A</first>', False)
            add('excess-suffix', '<first>A</first>' + suffix + suffix, False)
            add('early-suffix', suffix + '<first>A</first>', False)
            add('middle-suffix', '<first>A</first>' + suffix + '<second>B</second>', False)
            add('late-text', '<first>A</first>' + suffix + 'late', False)
            add('unknown-child', '<first>A</first><unknown/>' + suffix, False)
            add('unknown-attribute', '<first>A</first>' + suffix, False, 'core="unknown"')
            add('wrong-foreign-uri', '<first>A</first><x:suffix xmlns:x="urn:wrong"/>', False)
        return result

    ordered.cases = cases

    def observe(root):
        """Compare all known values canonically while preserving opaque subtree identity."""
        children = list(root)
        assert children and children[-1].tag == '{urn:foreign}suffix'
        assert all(child.tag in ('first', 'second') for child in children[:-1])
        def node(value):
            """Ignore namespace aliases while retaining expanded names, attributes and text."""
            return (value.tag, dict(value.attrib), value.text or '',
                    [(node(child), child.tail or '') for child in value])
        return root.tag, dict(root.attrib), sorted((child.tag, child.text or '')
            for child in children[:-1]), node(children[-1])

    ordered.observe = observe
    original_consumer = ordered.consumer

    def consumer(language, grouped=False):
        """Use native document companions when foreign state is deliberately separate."""
        if language != 'rust':
            return original_consumer(language, grouped)
        bodies = []
        for kind in ('Fixed', 'Root'):
            bodies.append(f'''let options=XmlExtensionOptions::default();
let document=Ordered{kind}::decode_xml_document(&input,XmlLimits::default(),None,&options)?;
assert!(valid,"Invalid all input admitted: {{path}}");
let output=document.value.encode_xml_with_extensions(&document.extensions,XmlLimits::default(),None,&options)?;
let copy=Ordered{kind}::decode_xml_document(&output,XmlLimits::default(),None,&options)?;
for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{{assert_eq!(document.value.encode(protocol)?,copy.value.encode(protocol)?);}}
Ok(output)''')
        return ordered.RUST.replace('BODY', 'let _=limits; if Path::new(path).file_name().unwrap().to_str().unwrap().starts_with("fixed"){'
            + bodies[0] + '}else{' + bodies[1] + '}')

    ordered.consumer = consumer
    ordered.C_CHECK = '''/* Preserve opaque companions while comparing all ordinary native codecs. */
static srl_status check_KIND(const srl_buffer* input,const srl_xml_mixed_limits* limits,srl_buffer* output,bool valid){
  (void)limits;ordered_KIND_xml_document value={0},copy={0};
  require(ordered_KIND_xml_document_init(&value)==srl_ok&&ordered_KIND_xml_document_init(&copy)==srl_ok);
  const ordered_KIND previous=value.value;const srl_xml_extension_state* state=value.extensions;
  srl_status status=ordered_KIND_decode_xml_document(&value,input->data,input->size,NULL,NULL);
  if(status!=srl_ok){require(memcmp(&value.value,&previous,sizeof(previous))==0&&value.extensions==state);goto done;}
  require(valid);status=ordered_KIND_encode_xml_document(&value,output,NULL,NULL);if(status!=srl_ok){goto done;}
  require(ordered_KIND_decode_xml_document(&copy,output->data,output->size,NULL,NULL)==srl_ok);
  const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
  for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){
    srl_buffer left={0},right={0};require(ordered_KIND_encode(&value.value,protocols[index],&left)==srl_ok&&ordered_KIND_encode(&copy.value,protocols[index],&right)==srl_ok);
    require(left.size==right.size&&memcmp(left.data,right.data,left.size)==0);srl_buffer_free(&left);srl_buffer_free(&right);
  }
done:ordered_KIND_xml_document_free(&value);ordered_KIND_xml_document_free(&copy);return status;
}
'''


if __name__ == '__main__':
    configure()
    ordered.main()
