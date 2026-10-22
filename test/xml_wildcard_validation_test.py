# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compare generated strict/lax wildcard dispatch with an independent pinned XSD processor."""
import sys

import xml_all_extensions_test as all_extensions
import xml_ordered_portable_test as ordered

CORE = 'urn:serializer:wildcard:validation:core'
FOREIGN = 'urn:serializer:wildcard:validation:foreign'
XSI = 'http://www.w3.org/2001/XMLSchema-instance'
PYTHON_CONSUMER = ordered.PYTHON
JAVASCRIPT_CONSUMER = ordered.JAVASCRIPT
KOTLIN_CONSUMER = ordered.KOTLIN
SWIFT_CONSUMER = ordered.SWIFT
PYTHON_ADMISSION = '''
class AdmissionFailure(RuntimeError):
    """Keep the application's original failure distinguishable from XML lexical rejection."""

class Admission:
    """Observe one shared operation without copying counters in nested declaration decoders."""
    def __init__(self, selected=None, maximum=None, failure=None):
        self.counts, self.selected, self.maximum, self.failure = {}, selected, maximum, failure
    def consume(self, resource, amount):
        self.counts[resource] = self.counts.get(resource, 0) + amount
        if resource == self.selected and self.counts[resource] > self.maximum:
            raise self.failure
    def check(self):
        pass
    def check_buffer(self, amount):
        assert amount >= 0

def admission_probe(kind, data, value, limits):
    """Require equality admission, one-tight rejection, skip quota0 and unchanged callback identity."""
    for operation in (lambda context: kind.decode_xml(data, limits, context=context),
                      lambda context: value.encode_xml(limits, context=context)):
        baseline = Admission()
        operation(baseline)
        for resource in ('work_units', 'decoder_allocation_bytes', 'preserved_bytes'):
            count = baseline.counts.get(resource, 0)
            if not count:
                continue
            operation(Admission(resource, count, AdmissionFailure('exact budget unexpectedly rejected')))
            failure = AdmissionFailure('one-tight original admission failure')
            try:
                operation(Admission(resource, count - 1, failure))
            except AdmissionFailure as actual:
                assert actual is failure
            else:
                raise AssertionError('A nested declaration reset its shared ' + resource)
        if RETENTION_SKIP:
            assert baseline.counts.get('preserved_bytes', 0) == 0
    if RETENTION_SKIP:
        zero = Limits(xml_extensions=XmlExtensionOptions(limits=XmlExtensionLimits(max_preserved_bytes=0)))
        kind.decode_xml(data, zero)
'''
JAVASCRIPT_ADMISSION = '''
function admissionProbe(Kind=M.OrderedRoot,text='',value=new M.OrderedRoot()){
  // Count actual recursive work and storage across private decoder replay without fresh quotas.
  for(const operation of [(limits=new M.XmlLimits())=>Kind.decodeXml(text,limits),(limits=new M.XmlLimits())=>value.encodeXml(limits)]){
    const counts=new Map();operation(new M.XmlLimits({operation:(resource,amount)=>{counts.set(resource,(counts.get(resource)||0)+amount);}}));
    for(const resource of ['work','allocation','preserved']){
      const maximum=counts.get(resource)||0;if(!maximum)continue;
      let used=0;operation(new M.XmlLimits({operation:(kind,amount)=>{if(kind===resource){used+=amount;assert.deepEqual(used<=maximum,true);}}}));
      used=0;const original=new Error('Original one-tight admission failure'),failures=[];
      try{operation(new M.XmlLimits({operation:(kind,amount)=>{if(kind===resource){used+=amount;if(used>maximum-1)throw original;}}}));}catch(failure){failures.push(failure);}
      assert.deepEqual(failures[0]===original,true,'Nested declarations reset '+resource);
    }
    if(RETENTION_SKIP)assert.deepEqual(counts.get('preserved')||0,0);
  }
  if(RETENTION_SKIP)Kind.decodeXml(text,new M.XmlLimits({extensions:new M.XmlExtensionOptions({limits:new M.XmlExtensionLimits({maxPreservedBytes:0})})}));
}
'''
KOTLIN_ADMISSION = r'''
// Count private decoder replay under the same owning operation and keep caller failure identity.
fun declarationAdmission(data: ByteArray, value: OrderedRoot) {
    val options=if(RETENTION_SKIP) XmlExtensionOptions(limits=XmlExtensionLimits(maxPreservedBytes=0)) else XmlExtensionOptions()
    val native=value.encode(Protocol.BINARY_NONE)
    val operations=listOf<(XmlLimits)->Unit>(
        { limits -> OrderedRoot.decodeXml(data,limits,extensionOptions=options); Unit },
        { limits -> value.encodeXml(limits,extensionOptions=options); Unit })
    fun policy(consume: (String,Long)->Unit): XmlLimits = XmlLimits().also {
        it.srlPackageContext=SrlXmlBudget.Operation({}, { resource,amount,_ -> consume(resource,amount) })
    }
    for(operation in operations) {
        val counts=mutableMapOf<String,Long>()
        operation(policy { resource,amount -> check(amount >= 0); counts[resource]=(counts[resource] ?: 0L)+amount })
        check((counts["work"] ?: 0L)>0 && (counts["allocation"] ?: 0L)>0)
        if(RETENTION_SKIP) check((counts["preserved"] ?: 0L)==0L) else check((counts["preserved"] ?: 0L)>0)
        for(resource in listOf("work","allocation","preserved")) {
            val maximum=counts[resource] ?: 0L
            if(maximum == 0L) continue
            var used=0L
            operation(policy { kind,amount -> if(kind==resource) { used+=amount; check(used<=maximum) } })
            used=0L
            val original=IllegalStateException("Original one-tight admission failure")
            var caught: Throwable?=null
            try { operation(policy { kind,amount -> if(kind==resource) { used+=amount; if(used>maximum-1L) throw original } }) }
            catch(failure: Throwable) { caught=failure }
            check(caught === original) { "Nested declaration reset $resource or replaced its failure" }
        }
        val cancellation=IllegalStateException("Original wildcard cancellation")
        var caught: Throwable?=null
        try { operation(XmlLimits().withCancellation { throw cancellation }) } catch(failure: Throwable) { caught=failure }
        check(caught === cancellation)
        operation(XmlLimits())
        check(native.contentEquals(value.encode(Protocol.BINARY_NONE)))
    }
    if(!RETENTION_SKIP) {
        val packet=value.srlXmlExtensions!!.children.first().node
        val count=packet.content.first() as SrlXmlForeignNode
        val previous=count.content[0];count.content[0]="0"
        var rejected=false
        try { value.encodeXml() } catch(expected:IllegalArgumentException) { rejected=true }
        finally { count.content[0]=previous }
        check(rejected) { "Edited invalid declared wildcard published" }
        check(native.contentEquals(value.encode(Protocol.BINARY_NONE)))
    }
}
'''
SWIFT_ADMISSION = r'''
// Keep admission failures distinguishable from generated lexical rejection.
final class DeclarationAdmissionFailure: Error {}
// Exercise one shared operation across nested private declaration replay.
func declarationAdmission(_ input:[UInt8]) throws {
  let options=RETENTION_SKIP ? XmlExtensionOptions(limits:XmlExtensionLimits(maxPreservedBytes:0)) : XmlExtensionOptions()
  let value=try OrderedRoot.decodeXml(input,extensionOptions:options)
  let native=try value.encode(.BINARY_NONE)
  let operations:[(XmlLimits)throws->Void]=[
    { limits in _=try OrderedRoot.decodeXml(input,limits,extensionOptions:options) },
    { limits in _=try value.encodeXml(limits,extensionOptions:options) }]
  for operation in operations {
    var counts:[String:Int]=[:];var baseline=XmlLimits()
    baseline.srlPackageConsume={kind,amount in try require(amount>=0);counts[kind,default:0]+=amount}
    try operation(baseline)
    try require(counts["work",default:0]>0 && counts["allocation",default:0]>0)
    try require(RETENTION_SKIP ? counts["preserved",default:0]==0 : counts["preserved",default:0]>0)
    for resource in ["work","allocation","preserved"] {
      let maximum=counts[resource,default:0];if maximum==0{continue}
      var used=0;var exact=XmlLimits()
      exact.srlPackageConsume={kind,amount in if kind==resource{used+=amount;try require(used<=maximum)}}
      try operation(exact)
      used=0;let original=DeclarationAdmissionFailure();var caught:(any Error)?;var tight=XmlLimits()
      tight.srlPackageConsume={kind,amount in if kind==resource{used+=amount;if used>maximum-1{throw original}}}
      do{try operation(tight)}catch{caught=error}
      try require((caught as AnyObject?) === original)
    }
    let original=DeclarationAdmissionFailure();var caught:(any Error)?
    do{try operation(XmlLimits().withCancellation{throw original})}catch{caught=error}
    try require((caught as AnyObject?) === original)
    try operation(XmlLimits());try require(value.encode(.BINARY_NONE)==native)
  }
}
'''
FOREIGN_XSD = f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:f="{FOREIGN}"
 targetNamespace="{FOREIGN}" elementFormDefault="qualified">
 <xs:simpleType name="Code"><xs:restriction base="xs:string"><xs:length value="2"/><xs:pattern value="[A-Z]{{2}}"/><xs:enumeration value="AB"/><xs:enumeration value="CD"/></xs:restriction></xs:simpleType>
 <xs:simpleType name="Number"><xs:restriction base="xs:int"><xs:minInclusive value="1"/><xs:maxInclusive value="9"/></xs:restriction></xs:simpleType>
 <xs:simpleType name="Name"><xs:restriction base="xs:QName"><xs:enumeration value="f:Chosen"/></xs:restriction></xs:simpleType>
 <xs:simpleType name="Names"><xs:list itemType="f:Name"/></xs:simpleType>
 <xs:element name="code" type="f:Code"/>
 <xs:element name="number" type="f:Number"/>
 <xs:element name="name" type="f:Name"/>
 <xs:element name="names" type="f:Names"/>
 <xs:element name="fixedValue" type="xs:int" fixed="5" nillable="true"/>
 <xs:element name="defaultValue" type="xs:string" default="fallback"/>
 <xs:complexType name="Packet"><xs:sequence>
  <xs:element name="count" type="f:Number"/>
  <xs:any namespace="##targetNamespace" processContents="strict" minOccurs="0" maxOccurs="1"/>
 </xs:sequence><xs:attribute name="mode" type="f:Code" use="required"/>
 <xs:anyAttribute namespace="##targetNamespace" processContents="strict"/></xs:complexType>
 <xs:element name="packet" type="f:Packet"/>
 <xs:element name="nilPacket" type="f:Packet" nillable="true"/>
 <xs:element name="abstractPacket" type="f:Packet" abstract="true"/>
 <xs:complexType name="Base"><xs:sequence><xs:element name="value" type="f:Number"/></xs:sequence>
  <xs:attribute name="mode" type="f:Code" use="required"/></xs:complexType>
 <xs:complexType name="Derived"><xs:complexContent><xs:extension base="f:Base"><xs:sequence>
  <xs:element name="extra" type="f:Code"/></xs:sequence></xs:extension></xs:complexContent></xs:complexType>
 <xs:complexType name="AbstractBase" abstract="true"><xs:sequence><xs:element name="value" type="f:Number"/></xs:sequence>
  <xs:attribute name="mode" type="f:Code" use="required"/></xs:complexType>
 <xs:complexType name="Concrete"><xs:complexContent><xs:extension base="f:AbstractBase"><xs:sequence>
  <xs:element name="detail" type="f:Code"/></xs:sequence></xs:extension></xs:complexContent></xs:complexType>
 <xs:element name="typed" type="f:Base" nillable="true"/>
 <xs:element name="blocked" type="f:Base" block="extension"/>
 <xs:element name="abstractType" type="f:AbstractBase" nillable="true"/>
 <xs:element name="decimal" type="xs:decimal"/>
 <xs:element name="unordered"><xs:complexType><xs:all><xs:element name="left" type="f:Number"/>
  <xs:element name="right" type="f:Code" minOccurs="0"/></xs:all></xs:complexType></xs:element>
 <xs:element name="choice"><xs:complexType><xs:choice><xs:element name="integer" type="f:Number"/>
  <xs:element name="label" type="f:Code"/></xs:choice></xs:complexType></xs:element>
 <xs:attribute name="flag" type="xs:boolean"/>
 <xs:attribute name="number" type="f:Number"/>
 <xs:attribute name="name" type="f:Name"/>
 <xs:attribute name="names" type="f:Names"/>
 <xs:attribute name="defaultValue" type="xs:string" default="fallback"/>
 <xs:attribute name="fixedValue" type="xs:string" fixed="fixed"/>
</xs:schema>'''


def configure(process='strict', retention='preserve'):
    """Collect only local declaration bundles and keep each real SDK build in the ignored output tree."""
    execute_sdk = ordered.execute
    all_extensions.configure()
    from xml_wildcard_admission_consumers import wrap_consumer
    ordered.consumer = wrap_consumer(ordered.consumer, retention == "skip")
    ordered.MODEL = f'''serializer version 1.21.0;
namespace ordered {{
 using schema "validation.xsd";
 xml(namespace("{CORE}"){', unknown(skip)' if retention == 'skip' else ''});
 class root {{ public string first; public string second; }}
 class fixed {{ public string first; public string second; }}
}}
'''
    ordered.CONFIG = '[xml]\nenabled=true\n'
    ordered.PYTHON = PYTHON_CONSUMER.replace('# Require rejected complete operations',
        PYTHON_ADMISSION.replace('RETENTION_SKIP', str(retention == 'skip')) + '\n# Require rejected complete operations').replace(
        '    path.with_name', "    if path.name.startswith('root-sequence-nested.valid'):\n        admission_probe(kind, path.read_bytes(), value, limits)\n    path.with_name")
    ordered.JAVASCRIPT = JAVASCRIPT_CONSUMER.replace('// Match fixture validity',
        JAVASCRIPT_ADMISSION.replace('RETENTION_SKIP', str(retention == 'skip').lower()) + '\n// Match fixture validity').replace(
        '  writeFileSync', "  if(path.includes('root-sequence-nested.valid'))admissionProbe(Kind,readFileSync(path,'utf8'),value);\n  writeFileSync")
    ordered.KOTLIN = KOTLIN_CONSUMER.replace('// Private variants remain private',
        KOTLIN_ADMISSION.replace('RETENTION_SKIP', str(retention == 'skip').lower()) + '\n// Private variants remain private').replace(
        '    check(valid);Files.write', '    if(args[index].contains("root-sequence-nested.valid")) declarationAdmission(Files.readAllBytes(path),OrderedRoot.decodeXml(Files.readAllBytes(path)))\n    check(valid);Files.write')
    ordered.SWIFT = SWIFT_CONSUMER.replace('let args=Array',
        SWIFT_ADMISSION.replace('RETENTION_SKIP', str(retention == 'skip').lower()) + '\nlet args=Array').replace(
        '  try require(valid);try Data(output)',
        '  if path.contains("root-sequence-nested.valid"){try declarationAdmission(input)}\n  try require(valid);try Data(output)')
    xsd = f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="{CORE}" elementFormDefault="qualified">
 <xs:import namespace="{FOREIGN}" schemaLocation="foreign.xsd"/><xs:import schemaLocation="local.xsd"/>''' + ''.join(
        f'''<xs:element name="{owner}"><xs:complexType><xs:sequence><xs:element name="first" type="xs:string"/>
 <xs:element name="second" type="xs:string"/><xs:any namespace="##other" processContents="{process}" minOccurs="0" maxOccurs="3"/>
 </xs:sequence><xs:anyAttribute namespace="##any" processContents="{process}"/></xs:complexType></xs:element>'''
        for owner in ('root', 'fixed')) + '</xs:schema>'

    def execute(args):
        """Validate original and generated documents using the same independent local XSD bundle."""
        from lxml import etree as provider
        (args.build / 'foreign.xsd').write_text(FOREIGN_XSD, encoding='utf-8')
        (args.build / 'local.xsd').write_text(f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:f="{FOREIGN}">
 <xs:simpleType name="LocalName"><xs:restriction base="xs:QName"><xs:enumeration value="f:Chosen"/></xs:restriction></xs:simpleType>
 <xs:attribute name="localName" type="LocalName"/>
</xs:schema>''', encoding='utf-8')
        (args.build / 'validation.xsd').write_text(xsd, encoding='utf-8')
        validator = provider.XMLSchema(provider.parse(str(args.build / 'validation.xsd')))
        for name, text, _, valid in ordered.cases():
            assert validator.validate(provider.fromstring(text.encode())) == valid, (name, validator.error_log)
        execute_sdk(args)
        for path in args.build.glob('*.valid.xml.*.xml'):
            assert validator.validate(provider.parse(str(path))), (path, validator.error_log)
            if retention == 'skip':
                root = provider.parse(str(path)).getroot()
                assert not root.attrib and len(root) == 2, path

    ordered.execute = execute

    def cases():
        """Exercise global kinds, complete private particles, original QName scopes and nil/type rules."""
        result = []
        for owner in ('root', 'fixed'):
            def add(label, child='', valid=True, attributes=''):
                """Keep wildcard inputs independent of generated native field representation."""
                result.append((owner + '-' + label,
                    f'<c:{owner} xmlns:c="{CORE}" xmlns:f="{FOREIGN}" xmlns:xsi="{XSI}" {attributes}>'
                    '<c:first>A&amp;B</c:first><c:second>C</c:second>' + child + f'</c:{owner}>', 64, valid))
            add('empty')
            add('scalar', '<f:code>AB</f:code><f:number>+5</f:number>')
            add('scalar-enum-bad', '<f:code>ZZ</f:code>', False)
            add('scalar-pattern-bad', '<f:code>ab</f:code>', False)
            add('scalar-bound-bad', '<f:number>10</f:number>', False)
            add('scalar-markup-bad', '<f:code><f:code>AB</f:code></f:code>', False)
            add('qname-alias', f'<f:name xmlns:q="{FOREIGN}">q:Chosen</f:name>')
            add('qname-scope-bad', '<f:name xmlns:q="urn:wrong">q:Chosen</f:name>', False)
            add('qname-unbound', '<f:name>q:Chosen</f:name>', False)
            add('qname-list', '<f:names>f:Chosen f:Chosen</f:names>')
            add('qname-list-bad', '<f:names>f:Chosen f:Other</f:names>', False)
            packet = '<f:packet mode="AB"><f:count>3</f:count><f:code>CD</f:code></f:packet>'
            add('sequence-nested', packet)
            add('sequence-bound-bad', packet.replace('>3<', '>0<'), False)
            add('sequence-required-attr', packet.replace(' mode="AB"', ''), False)
            add('sequence-attr-facet', packet.replace('mode="AB"', 'mode="ZZ"'), False)
            add('sequence-nested-bad', packet.replace('>CD<', '>ZZ<'), False)
            add('sequence-nested-undeclared', packet.replace('<f:code>CD</f:code>', '<f:unknown/>'), False)
            add('all-permuted', '<f:unordered><f:right>AB</f:right><f:left>4</f:left></f:unordered>')
            add('all-optional', '<f:unordered><f:left>4</f:left></f:unordered>')
            add('all-required', '<f:unordered><f:right>AB</f:right></f:unordered>', False)
            add('all-duplicate', '<f:unordered><f:left>4</f:left><f:left>4</f:left></f:unordered>', False)
            add('choice-integer', '<f:choice><f:integer>4</f:integer></f:choice>')
            add('choice-label', '<f:choice><f:label>AB</f:label></f:choice>')
            add('choice-empty', '<f:choice/>', False)
            add('choice-both', '<f:choice><f:integer>4</f:integer><f:label>AB</f:label></f:choice>', False)
            add('nil-required-attr', '<f:nilPacket mode="AB" xsi:nil="true"/>')
            add('nil-whitespace', '<f:nilPacket mode="AB" xsi:nil=" &#9;true &#13;"/>')
            add('nil-missing-attr', '<f:nilPacket xsi:nil="true"/>', False)
            add('nil-bad-attr', '<f:nilPacket mode="ZZ" xsi:nil="true"/>', False)
            add('nil-with-content', '<f:nilPacket mode="AB" xsi:nil="true"><f:count>3</f:count></f:nilPacket>', False)
            add('nil-false', '<f:nilPacket mode="AB" xsi:nil="false"><f:count>3</f:count></f:nilPacket>')
            add('nil-nonnillable', '<f:packet mode="AB" xsi:nil="true"/>', False)
            add('nil-bad-lexical', '<f:nilPacket mode="AB" xsi:nil="yes"/>', False)
            add('nil-fixed', '<f:fixedValue xsi:nil="true"/>', False)
            add('fixed-empty', '<f:fixedValue/>')
            add('fixed-bad', '<f:fixedValue>6</f:fixedValue>', False)
            add('default-empty', '<f:defaultValue/>')
            add('type-alias', f'<f:packet xmlns:q="{FOREIGN}" mode="AB" xsi:type="q:Packet"><f:count>3</f:count></f:packet>')
            add('type-wrong', '<f:packet mode="AB" xsi:type="f:Code"><f:count>3</f:count></f:packet>', False)
            add('type-unbound', '<f:packet mode="AB" xsi:type="q:Packet"><f:count>3</f:count></f:packet>', False)
            add('abstract', '<f:abstractPacket mode="AB"><f:count>3</f:count></f:abstractPacket>', False)
            add('type-derived', '<f:typed mode="AB" xsi:type="f:Derived"><f:value>3</f:value><f:extra>CD</f:extra></f:typed>')
            add('type-derived-bound', '<f:typed mode="AB" xsi:type="f:Derived"><f:value>0</f:value><f:extra>CD</f:extra></f:typed>', False)
            add('type-derived-required', '<f:typed mode="AB" xsi:type="f:Derived"><f:value>3</f:value></f:typed>', False)
            add('type-derived-extra', '<f:typed mode="AB" xsi:type="f:Derived"><f:value>3</f:value><f:extra>ZZ</f:extra></f:typed>', False)
            add('type-derived-nil', '<f:typed mode="AB" xsi:type="f:Derived" xsi:nil="true"/>')
            add('type-derived-nil-attr', '<f:typed xsi:type="f:Derived" xsi:nil="true"/>', False)
            add('type-derived-blocked', '<f:blocked mode="AB" xsi:type="f:Derived"><f:value>3</f:value><f:extra>CD</f:extra></f:blocked>', False)
            add('type-abstract-default', '<f:abstractType mode="AB"><f:value>3</f:value></f:abstractType>', False)
            add('type-abstract-equal', '<f:abstractType mode="AB" xsi:type="f:AbstractBase"><f:value>3</f:value></f:abstractType>', False)
            add('type-abstract-derived', '<f:abstractType mode="AB" xsi:type="f:Concrete"><f:value>3</f:value><f:detail>CD</f:detail></f:abstractType>')
            add('type-abstract-derived-nil', '<f:abstractType mode="AB" xsi:type="f:Concrete" xsi:nil="true"/>')
            add('type-simple-derived', '<f:decimal xsi:type="f:Number">7</f:decimal>')
            add('type-simple-derived-bad', '<f:decimal xsi:type="f:Number">11</f:decimal>', False)
            add('type-simple-derived-lexical', '<f:decimal xsi:type="f:Number">7.5</f:decimal>', False)
            add('attribute-valid', packet, attributes='f:flag="true" f:number="7" f:name="f:Chosen" f:names="f:Chosen f:Chosen"')
            add('attribute-boolean', packet, False, 'f:flag="yes"')
            add('attribute-bound', packet, False, 'f:number="0"')
            add('attribute-qname', packet, False, 'f:name="f:Other"')
            add('attribute-qname-unbound', packet, False, 'f:name="q:Chosen"')
            add('attribute-list', packet, False, 'f:names="f:Chosen f:Other"')
            add('attribute-empty-default', attributes='f:defaultValue=""')
            add('attribute-empty-fixed', valid=False, attributes='f:fixedValue=""')
            add('attribute-fixed', attributes='f:fixedValue="fixed"')
            add('attribute-local-default', attributes=f'xmlns="{FOREIGN}" localName="Chosen"')
            add('attribute-local-wrong-default', valid=False, attributes='xmlns="urn:wrong" localName="Chosen"')
            add('unknown-element', '<f:unknown unknown="opaque">unbound:opaque<f:nested/>tail</f:unknown>', process == 'lax')
            add('unknown-attribute', valid=process == 'lax', attributes='f:unknown="unbound:opaque"')
            add('wrong-kind-element', '<f:flag/>', process == 'lax')
            add('wrong-kind-attribute', valid=process == 'lax', attributes='f:packet="opaque"')
            add('unlisted-uri', '<u:item xmlns:u="urn:unknown"/>', process == 'lax')
        return result

    ordered.cases = cases

    def observe(root):
        """Compare opaque lexical payloads, excluding discarded wildcards only for explicit skip policy."""
        def node(value):
            """Use expanded names and preserve foreign mixed-text order independently of prefixes."""
            children = list(value)
            attributes = dict(value.attrib)
            if retention == 'skip' and value is root:
                children = [child for child in children if child.tag.startswith('{' + CORE + '}')]
                attributes = {}
            return value.tag, attributes, value.text or '', [(node(child), child.tail or '') for child in children]
        return node(root)

    ordered.observe = observe


if __name__ == '__main__':
    settings = {}
    for key in ('process', 'retention'):
        option = '--' + key
        if option in sys.argv:
            index = sys.argv.index(option)
            settings[key] = sys.argv[index + 1]
            del sys.argv[index:index + 2]
    configure(**settings)
    ordered.main()
