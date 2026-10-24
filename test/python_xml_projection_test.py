# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify Python readonly XML references/translators without constructing or copying owning targets."""
import argparse
import gc
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import weakref
import xml.etree.ElementTree as etree

ROOT=Path(__file__).resolve().parent.parent
NS='urn:serializer:xml-projection'


def load_module(path,name):
    """Load real compiler output using the target Python SDK."""
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec)
    sys.modules[name]=module;spec.loader.exec_module(module);return module


class python_xml_projection_test(unittest.TestCase):
    """Check live owner semantics, native controls, exact XML and genuine export-only restrictions."""
    def test_scoped_qname_namespace_and_byte_failure_recovery(self):
        """Bind only projection presentation while source strings and native protocol bytes remain borrowed."""
        module=self.module;source=module.XmlProjectionQualifiedSource();source.current.value='p:borrowed'
        native=[source.current.encode(protocol) for protocol in module.Protocol]
        def export(view):
            """Traverse live original text with independent namespace and output-byte admission."""
            self.assertIs(view.get_value(),source.current.value)
            with self.assertRaises(ValueError):view.encode_xml()
            view.xml_bind_namespace('p','urn:borrowed');output=view.encode_xml()
            self.assertEqual(view.encode_xml(module.Limits(max_bytes=len(output))),output)
            published=b'previous output'
            with self.assertRaises(ValueError):published=view.encode_xml(module.Limits(max_bytes=len(output)-1))
            self.assertEqual(published,b'previous output');self.assertEqual(view.encode_xml(),output)
            self.assertEqual([view.encode(protocol) for protocol in module.Protocol],native)
            return output
        with mock.patch.object(module.XmlProjectionQualifiedPayload,'__init__',side_effect=AssertionError('QName target constructed')):
            output=source.with_project(export)
        root=self.provider.fromstring(output);prefix,local=root.text.split(':')
        self.assertEqual((root.nsmap[prefix],local),('urn:borrowed','borrowed'))
        self.assertEqual([source.current.encode(protocol) for protocol in module.Protocol],native)

        # Combine a scoped translator with token-only nil presence and a root QName attribute.
        extra=self.extra;owner=extra.ProjectionExtraQualifiedSource();owner.name='p:borrowed'
        native=[owner.encode(protocol) for protocol in extra.Protocol]
        def token_export(view):
            """Keep root lexical scope and admission active through the borrowed token adapter."""
            with self.assertRaises(ValueError):view.encode_xml()
            view.xml_bind_namespace('p','urn:borrowed')
            output=view.encode_xml()
            with self.assertRaises(ValueError):view.encode_xml(extra.Limits(max_string_bytes=1))
            self.assertEqual(view.encode_xml(),output)
            return output
        with mock.patch.object(extra.ProjectionExtraQualifiedTarget,'__init__',side_effect=AssertionError('token target constructed')):
            with mock.patch.object(extra,'_xml_write',side_effect=AssertionError('token projection retained a tree')):
                output=owner.with_project(token_export)
        root=self.provider.fromstring(output);prefix,local=root.attrib['name'].split(':')
        self.assertEqual((root.nsmap[prefix],local),('urn:borrowed','borrowed'))
        self.assertEqual(root[0].attrib['{http://www.w3.org/2001/XMLSchema-instance}nil'],'true')
        self.assertEqual([owner.encode(protocol) for protocol in extra.Protocol],native)

    def test_live_record_and_scalar_reference_exports_preserve_native_storage(self):
        """Borrow selected original values, including false/zero/escaped text, while target construction is forbidden."""
        module=self.module;source=module.XmlProjectionSource();source.current.number=7;source.current.label='A<&"'
        envelope=module.XmlProjectionEnvelope();envelope.value.bind(source);envelope.label.bind(source)
        native_source=self.native.XmlProjectionSource();native_source.current.number=7;native_source.current.label=source.current.label
        native_envelope=self.native.XmlProjectionEnvelope();native_envelope.value.bind(native_source);native_envelope.label.bind(native_source)
        before=[envelope.encode(protocol) for protocol in module.Protocol]
        with mock.patch.object(module.XmlProjectionPayload,'__init__',side_effect=AssertionError('owning target constructed')):
            with mock.patch.object(module,'_xml_parse',side_effect=AssertionError('known XML tree retained')):
                direct=envelope.value.encode_xml();output=envelope.encode_xml()
        self.assertEqual(direct,source.current.encode_xml())
        root=etree.fromstring(output)
        self.assertEqual(root.attrib['label'],source.current.label)
        self.assertEqual(root[0].attrib,{'number':'7','label':source.current.label})
        self.assertIs(envelope.value.read_value(),source.current)
        self.assertEqual([envelope.encode(protocol) for protocol in module.Protocol],before)
        self.assertEqual([native_envelope.encode(protocol) for protocol in self.native.Protocol],before)
        source.current.number=0;source.current.label='updated'
        self.assertEqual(etree.fromstring(envelope.value.encode_xml()).attrib,{'number':'0','label':'updated'})
        with self.assertRaises(RuntimeError):
            module.XmlProjectionEnvelope().encode_xml()
        envelope.value.unbind()
        with self.assertRaises(RuntimeError):envelope.encode_xml()

    def test_scoped_translator_getters_and_borrowed_nested_collections(self):
        """Traverse readonly getters without materializing target models or copied nested objects/arrays."""
        module=self.module;source=module.XmlProjectionSource();source.current.number=19;source.current.label='borrowed'
        with mock.patch.object(module.XmlProjectionPayload,'__init__',side_effect=AssertionError('target constructed')):
            output=source.with_project(lambda view:view.encode_xml())
            for protocol in module.Protocol:
                self.assertEqual(source.with_project(lambda view:view.encode(protocol)),source.current.encode(protocol))
        self.assertEqual(output,source.current.encode_xml())
        extra=self.extra;owner=extra.ProjectionExtraSource();owner.values=[1,2];owner.nested.number=3
        def export(view):
            """Require reference identity before invoking either codec on the scoped view."""
            self.assertIs(view.get_values(),owner.values);self.assertIs(view.get_nested(),owner.nested)
            return view.encode_xml()
        with mock.patch.object(extra.ProjectionExtraTarget,'__init__',side_effect=AssertionError('target constructed')):
            with mock.patch.object(extra.ProjectionExtraChild,'__init__',side_effect=AssertionError('nested target copied')):
                exported=owner.with_project(export)
        value=extra.ProjectionExtraTarget.decode_xml(exported)
        self.assertEqual((value.values,value.nested.number,value.fallback),([1,2],3,'default'))
        owner.values.append(4);owner.nested.number=5
        value=extra.ProjectionExtraTarget.decode_xml(owner.with_project(export))
        self.assertEqual((value.values,value.nested.number),([1,2,4],5))

    def test_export_only_input_restrictions_before_candidate_construction(self):
        """Readonly references cannot invent external owners; ordinary target decoding remains available."""
        module=self.module;source=module.XmlProjectionSource();source.current.number=7;source.current.label='x'
        envelope=module.XmlProjectionEnvelope();envelope.value.bind(source);envelope.label.bind(source)
        output=envelope.encode_xml()
        with mock.patch.object(module.XmlProjectionEnvelope,'__init__',side_effect=AssertionError('candidate constructed')):
            for action in (lambda:module.XmlProjectionEnvelope.decode_xml(output),
                           lambda:module.XmlProjectionEnvelope.decode_xml_chunks((output,))):
                with self.assertRaisesRegex(ValueError,'export-only'):action()
        with self.assertRaisesRegex(ValueError,'export-only'):
            type(envelope.value).decode_xml(source.current.encode_xml())
        with self.assertRaisesRegex(ValueError,'owning field mapping'):
            envelope.label.encode_xml()
        self.assertEqual(module.XmlProjectionPayload.decode_xml(envelope.value.encode_xml()).number,7)

    def test_large_scoped_array_limits_and_fresh_recovery(self):
        """Borrow 25,000 source values while target construction and copied nested owners are forbidden."""
        extra=self.extra; owner=extra.ProjectionExtraSource()
        owner.values=list(range(25000)); owner.nested.number=17
        original_values=owner.values; original_nested=owner.nested
        native=[owner.encode(protocol) for protocol in extra.Protocol]
        def export(view, limits=None):
            """Assert storage identity on each complete pass before invoking its real codec."""
            self.assertIs(view.get_values(),original_values)
            self.assertIs(view.get_nested(),original_nested)
            return view.encode_xml(limits)
        with mock.patch.object(extra.ProjectionExtraTarget,'__init__',side_effect=AssertionError('large target constructed')):
            with mock.patch.object(extra.ProjectionExtraChild,'__init__',side_effect=AssertionError('large nested owner copied')):
                output=owner.with_project(export)
                exact=extra.Limits(max_bytes=len(output))
                self.assertEqual(owner.with_project(lambda view:export(view,exact)),output)
                self.assertEqual(owner.with_project(lambda view:export(view,extra.Limits(max_depth=3))),output)
                with self.assertRaises(ValueError):owner.with_project(lambda view:export(view,extra.Limits(max_depth=2)))
                published=b'previous output'
                with self.assertRaises(ValueError):
                    published=owner.with_project(lambda view:export(view,extra.Limits(max_bytes=len(output)-1)))
                self.assertEqual(published,b'previous output')
                self.assertEqual(owner.with_project(export),output)
        root=self.provider.fromstring(output)
        self.assertEqual([int(child.text) for child in root.findall('{urn:projection-extra}values')],owner.values)
        self.assertEqual(root.find('{urn:projection-extra}nested')[0].text,'17')
        self.assertEqual(root.find('{urn:projection-extra}fallback').text,'default')
        self.assertEqual([owner.encode(protocol) for protocol in extra.Protocol],native)
        self.assertIs(owner.values,original_values); self.assertIs(owner.nested,original_nested)

    def test_weak_qname_namespace_context_and_nil_target(self):
        """Preserve borrowed QName scope, native weak expiration and explicit nil target states."""
        module=self.module;source=module.XmlProjectionQualifiedSource();source.current.value='p:Thing'
        source.current.xml_bind_namespace('p','urn:qualified-value')
        envelope=module.XmlProjectionWeakEnvelope();envelope.value.bind(source)
        with mock.patch.object(module.XmlProjectionQualifiedPayload,'__init__',side_effect=AssertionError('QName target copied')):
            direct=envelope.value.encode_xml();output=envelope.encode_xml()
        root=self.provider.fromstring(output)
        self.assertEqual(root[0].text,'p:Thing');self.assertEqual(root[0].nsmap['p'],'urn:qualified-value')
        self.assertEqual(module.XmlProjectionQualifiedPayload.decode_xml(direct).value,'p:Thing')
        lifetime=weakref.ref(source);del source;gc.collect();self.assertIsNone(lifetime())
        with self.assertRaises(RuntimeError):envelope.encode_xml()
        owner=module.XmlProjectionNilSource();nil=module.XmlProjectionNilEnvelope();nil.value.bind(owner)
        with mock.patch.object(module,'_xml_write',side_effect=AssertionError('nil projection fell back to XML tree')):
            output=nil.encode_xml()
        self.assertEqual(etree.fromstring(output)[0][0].attrib['{http://www.w3.org/2001/XMLSchema-instance}nil'],'true')
        owner.current.text=('',)
        self.assertNotIn('{http://www.w3.org/2001/XMLSchema-instance}nil',etree.fromstring(nil.encode_xml())[0][0].attrib)

    def test_output_limits_provider_callbacks_and_portable_projection_contract(self):
        """Apply ordinary XML policy to borrowed output and retain exact application callback failures."""
        module=self.module;source=module.XmlProjectionSource();source.current.number=9;source.current.label='bounded'
        expected=source.current.encode_xml();exact=module.Limits(max_bytes=len(expected))
        self.assertEqual(source.with_project(lambda view:view.encode_xml(exact)),expected)
        with self.assertRaises(ValueError):
            source.with_project(lambda view:view.encode_xml(module.Limits(max_bytes=len(expected)-1)))
        marker=ValueError('application projection validator marker')
        class validator:
            """Reject at the caller's established complete-output boundary."""
            def validate(self,data,limits=None,context=None):
                """Preserve the exact application exception rather than wrapping it as a lexical failure."""
                raise marker
        with self.assertRaises(ValueError) as failure:
            source.with_project(lambda view:view.encode_xml(schema=validator()))
        self.assertIs(failure.exception,marker)

    def test_prepared_provider_completion_preserves_operation_fences(self):
        """Reject late cancellation before owning/projected output or input candidates can escape."""
        module=self.module;source=module.XmlProjectionSource();source.current.number=7;source.current.label='checked'
        schema=b'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:serializer:xml-projection">
          <xs:element name="payload"><xs:complexType><xs:attribute name="number" type="xs:unsignedInt" use="required"/>
          <xs:attribute name="label" type="xs:string" use="required"/></xs:complexType></xs:element></xs:schema>'''
        marker=RuntimeError('late prepared provider cancellation');cancelled=[False];calls=[0]
        class operation:
            """Model a caller deadline/cancellation becoming observable during the external provider call."""
            def check(self):
                """Keep the original caller error rather than translate it into an XML failure."""
                if cancelled[0]:raise marker
            def consume(self,resource,count):
                """Admit the fixture's ordinary accounting while polling the same operation."""
                self.check()
            def check_buffer(self,amount):
                """Apply the same caller interruption fence to temporary output reservations."""
                self.check()
        context=operation()
        class completing_schema(module.XmlSchema):
            """Use the real prepared lxml validator, then signal a cancellation before returning."""
            def validate(self,data,limits=None,context=None):
                """Finish actual validation before changing the borrowed operation's cancellation state."""
                super().validate(data,limits,context);calls[0]+=1;cancelled[0]=True
        validator=completing_schema(schema);data=source.current.encode_xml()
        before=[source.encode(protocol) for protocol in module.Protocol]
        for action in (lambda:source.current.encode_xml(schema=validator,context=context),
                       lambda:source.with_project(lambda view:view.encode_xml(schema=validator,context=context))):
            cancelled[0]=False
            with self.assertRaises(RuntimeError) as failure:action()
            self.assertIs(failure.exception,marker)
        cancelled[0]=False
        with mock.patch.object(module.XmlProjectionPayload,'__init__',side_effect=AssertionError('input candidate constructed after late cancellation')):
            with self.assertRaises(RuntimeError) as failure:
                module.XmlProjectionPayload.decode_xml(data,schema=validator,context=context)
        self.assertIs(failure.exception,marker)
        self.assertEqual(calls[0],3)
        self.assertEqual([source.encode(protocol) for protocol in module.Protocol],before)
        original=ValueError('original prepared provider rejection')
        class failing_schema(module.XmlSchema):
            """Preserve a provider failure even when cancellation also becomes true during validation."""
            def validate(self,data,limits=None,context=None):
                """Return the provider's own failure without replacing it by a later poll."""
                super().validate(data,limits,context);cancelled[0]=True;raise original
        cancelled[0]=False
        with self.assertRaises(ValueError) as failure:source.current.encode_xml(schema=failing_schema(schema),context=context)
        self.assertIs(failure.exception,original)


def main():
    """Generate actual projection XML/native controls and require a real independent QName-capable XML provider."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--python-dependencies',type=Path)
    args=parser.parse_args()
    if args.python_dependencies:sys.path.insert(0,str(args.python_dependencies.resolve()))
    from lxml import etree as provider
    build=args.build.resolve()
    if build==ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='python-xml-projection-',dir=build) as temporary:
        directory=Path(temporary);config=directory/'xml.ini'
        address=directory/'address.serializer'
        address.write_text('serializer version 1.20.0; class source stable_ids { public uint32 value (1); } reference address = borrowed(source.value); class envelope stable_ids { public address selected (1); }',encoding='utf-8')
        for language,suffix in (('cpp','.hpp'),('python','.py'),('js','.mjs'),('typescript','.d.mts'),('java','.java'),('csharp','.cs'),('go','.go'),('kotlin','.kt'),('swift','.swift'),('c','.h'),('rust','.rs')):
            destination=directory/('address_'+language+suffix)
            command=[str(args.compiler.resolve()),'--language',language,'--input',str(address),'--'+language+'.output',str(destination)]
            if language=='cpp':command+=['--cpp.format','false']
            native_result=subprocess.run([*command,'--xml.enabled','false'],capture_output=True,text=True,encoding='utf-8')
            if native_result.returncode!=0:raise AssertionError('Native address control failed for '+language+': '+native_result.stdout+native_result.stderr)
            destination.write_bytes(b'previous output')
            rejected=subprocess.run([*command,'--xml.enabled','true'],capture_output=True,text=True,encoding='utf-8')
            if rejected.returncode==0 or 'address' not in rejected.stderr+rejected.stdout:
                raise AssertionError('Durable XML address policy differs for '+language+': '+rejected.stdout+rejected.stderr)
            if destination.read_bytes()!=b'previous output':raise AssertionError('Rejected durable address output was published: '+language)
        print('PASS: eleven native durable-address controls and eleven atomic XML-address rejections',flush=True)
        config.write_text('[xml]\nenabled=true\nruntime_xsd=true\nvalidation_scope=both\n'+(ROOT/'test/resources/xml_projection.ini').read_text(),encoding='utf-8')
        source=ROOT/'test/resources/xml_projection.serializer';output=directory/'model.py';native=directory/'native.py'
        command=[str(args.compiler.resolve()),'--language','python','--input',str(source)]
        subprocess.run([*command,'--config',str(config),'--python.output',str(output)],check=True)
        subprocess.run([*command,'--xml.enabled','false','--python.output',str(native)],check=True)
        extra=directory/'extra.serializer';extra_output=directory/'extra.py'
        extra.write_text('''serializer version 1.20.0; namespace projection_extra {
          xml(namespace("urn:projection-extra"));
          class child { public uint32 number(1); }
          class target { public array<uint32> values(1); public child nested(2); public string fallback(3) { "default" }; }
          class source { public array<uint32> values(1); public child nested(2);
            public translator target as project { values=values; nested=nested; }; }
          class qualified_target { public string name xml(attribute,qname); public optional<uint32> number; }
          class qualified_source { public string name; public optional<uint32> number;
            public translator qualified_target as project { name=name; number=number; }; }}''',encoding='utf-8')
        extra_config=directory/'extra.ini'
        extra_config.write_text('[xml.class.projection_extra::qualified_target]\npresence.number=nil\n',encoding='utf-8')
        subprocess.run([str(args.compiler.resolve()),'--language','python','--input',str(extra),'--config',str(extra_config),'--xml.enabled','true','--python.output',str(extra_output)],check=True)
        python_xml_projection_test.module=load_module(output,'serializer_python_xml_projection')
        python_xml_projection_test.native=load_module(native,'serializer_python_xml_projection_native')
        python_xml_projection_test.extra=load_module(extra_output,'serializer_python_xml_projection_extra')
        python_xml_projection_test.provider=provider
        result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_xml_projection_test))
        if not result.wasSuccessful():raise SystemExit(1)


if __name__=='__main__':main()
