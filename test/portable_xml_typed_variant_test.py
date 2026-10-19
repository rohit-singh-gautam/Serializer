"""Qualify xsi:type variants with strict independent abstract-base XSDs and namespace scopes."""
import argparse
from pathlib import Path

from lxml import etree

import portable_xml_variant_test as fixture


ROOT = fixture.ROOT
sdk = fixture.sdk
NS = 'urn:serializer:variants'
ATTRIBUTE_NS = 'urn:serializer:attributes/srltype'
MODEL = fixture.MODEL.replace(
    'class child { public uint16 code; public string label; }',
    'class detail { public uint8 count; public string marker xml(attribute, namespace("' + ATTRIBUTE_NS + '")); }\n'
    '  class child { public uint16 code; public string label; public detail detail; public string marker xml(attribute, namespace("' + ATTRIBUTE_NS + '")); }').replace(
        'public variant(uint32=code',
        'public string anchor xml(attribute, namespace("' + ATTRIBUTE_NS + '"));\n    public variant(uint32=code')


def config(empty_code):
    """Bind complete inventories while preserving native alternative aliases and storage."""
    selected = fixture.CONFIG.replace('variant.value = element', 'variant.value = xsi_type').replace(
        'variant.primitive = element', 'variant.primitive = xsi_type')
    selected = '\n'.join(line for line in selected.splitlines()
                         if not line.startswith('variant.value.tag.') and not line.startswith('variant.value.type.')) + '\n'
    selected = selected.replace('[xml.class.variants::child]',
        'variant.value.tag.code = ' + ('{}codeType' if empty_code else '{' + NS + '}codeType') + '\n'
        'variant.value.tag.text = {' + NS + '}textType\n'
        'variant.value.tag.record = {' + NS + '}recordType\n'
        'variant.value.tag.state = {' + NS + '}stateType\n'
        'variant.primitive.tag.flag = {' + NS + '}flagType\n'
        'variant.primitive.tag.amount = {' + NS + '}amountType\n'
        '[xml.class.variants::child]')
    return selected + '[xml.class.variants::detail]\nnamespace = ' + NS + '\n'


def prepare_oracle(directory, empty_code):
    """Require concrete known derived types for every payload, including cross-namespace QNames."""
    attrs = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="' + ATTRIBUTE_NS + '"><xs:attribute name="anchor" type="xs:string"/><xs:attribute name="marker" type="xs:string"/></xs:schema>'
    (directory / 'attributes.xsd').write_text(attrs, encoding='utf-8')
    schema = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="' + NS + '" xmlns:a="' + ATTRIBUTE_NS + '" targetNamespace="' + NS + '" elementFormDefault="qualified">'
    schema += '<xs:import namespace="' + ATTRIBUTE_NS + '" schemaLocation="attributes.xsd"/>'
    if empty_code:
        schema += '<xs:import schemaLocation="zero-types.xsd"/>'
    schema += '<xs:complexType name="baseType" mixed="true" abstract="true"><xs:sequence><xs:any minOccurs="0" maxOccurs="unbounded" processContents="lax"/></xs:sequence><xs:anyAttribute processContents="lax"/></xs:complexType>'
    for name, primitive in [('codeType', 'unsignedInt'), ('textType', 'string'), ('flagType', 'boolean'), ('amountType', 'double')]:
        definition = '<xs:complexType name="' + name + '"><xs:simpleContent><xs:restriction base="t:baseType"><xs:simpleType><xs:restriction base="xs:' + primitive + '"/></xs:simpleType></xs:restriction></xs:simpleContent></xs:complexType>'
        if empty_code and name == 'codeType':
            zero = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="' + NS + '"><xs:import namespace="' + NS + '" schemaLocation="variants.xsd"/>' + definition + '</xs:schema>'
            (directory / 'zero-types.xsd').write_text(zero, encoding='utf-8')
        else:
            schema += definition
    schema += '<xs:complexType name="stateType"><xs:simpleContent><xs:restriction base="t:baseType"><xs:simpleType><xs:restriction base="xs:string"><xs:enumeration value="zero"/><xs:enumeration value="one"/></xs:restriction></xs:simpleType></xs:restriction></xs:simpleContent></xs:complexType>'
    schema += '<xs:complexType name="detailType"><xs:sequence><xs:element name="count" type="xs:unsignedByte"/></xs:sequence><xs:attribute ref="a:marker" use="required"/></xs:complexType>'
    schema += '<xs:complexType name="recordType"><xs:complexContent><xs:restriction base="t:baseType"><xs:sequence><xs:element name="code" type="xs:unsignedShort"/><xs:element name="label" type="xs:string"/><xs:element name="detail" type="t:detailType"/></xs:sequence><xs:attribute ref="a:marker" use="required"/></xs:restriction></xs:complexContent></xs:complexType>'
    schema += '<xs:complexType name="sampleType"><xs:sequence>'
    for wrapper in ['choice', 'primitive']:
        schema += '<xs:element name="' + wrapper + '"><xs:complexType><xs:sequence><xs:element name="' + wrapper + '" type="t:baseType"/></xs:sequence></xs:complexType></xs:element>'
    schema += '</xs:sequence><xs:attribute ref="a:anchor" use="required"/></xs:complexType><xs:element name="sample" type="t:sampleType"/><xs:element name="holder"><xs:complexType><xs:sequence><xs:element name="value" type="t:sampleType"/></xs:sequence></xs:complexType></xs:element></xs:schema>'
    path = directory / 'variants.xsd'; path.write_text(schema, encoding='utf-8')
    return etree.XMLSchema(etree.parse(str(path), etree.XMLParser(no_network=True)))


def cases(empty_code):
    """Exercise exact tag/type pairs, namespace aliases, empty type namespaces and nested attributes."""
    start = '<sample xmlns="' + NS + '" xmlns:v="' + NS + '" xmlns:t="' + NS + '" xmlns:a="' + ATTRIBUTE_NS + '" xmlns:i="http://www.w3.org/2001/XMLSchema-instance" a:anchor="root">'
    values = ['4294967295', 'hi🙂&amp;<![CDATA[<>]]>', '<code>65535</code><label>nested</label><detail a:marker="leaf"><count>255</count></detail>', 'one']
    names = ['codeType', 'textType', 'recordType', 'stateType']
    primitives = ['0', 'INF']; primitive_names = ['flagType', 'amountType']
    probes = []
    for selected, body in enumerate(values):
        for primitive, primitive_body in enumerate(primitives):
            tag = 'v:choice xmlns=""' if empty_code and selected == 0 else 'choice'
            end = 'v:choice' if empty_code and selected == 0 else 'choice'
            identity = names[selected] if empty_code and selected == 0 else 't:' + names[selected]
            marker = ' a:marker="payload"' if selected == 2 else ''
            payload = '<' + tag + ' i:type="' + identity + '"' + marker + '>' + body + '</' + end + '>'
            document = start + '<choice>' + payload + '</choice><primitive><primitive i:type="t:' + primitive_names[primitive] + '">' + primitive_body + '</primitive></primitive></sample>'
            probes.append((selected, primitive, document, True))
            probes.append((selected, primitive, document.replace('i:type="' + identity + '"', 'i:type=" &#9;' + identity + '&#10; "', 1), True))
            if not (empty_code and selected == 0):
                probes.append((selected, primitive, document.replace('i:type="t:' + names[selected] + '"', 'xmlns:q="' + NS + '" i:type="q:' + names[selected] + '"', 1), True))
                probes.append((selected, primitive, document.replace('i:type="t:' + names[selected] + '"', 'i:type="' + names[selected] + '"', 1), True))
    base = next(document for selected, primitive, document, valid in probes if selected == 2 and primitive == 0 and valid)
    for before, after in [
        (' i:type="t:recordType"', ''), ('i:type="t:recordType"', 'i:type="t:missing"'),
        ('i:type="t:recordType"', 'i:type="unbound:recordType"'), ('i:type="t:recordType"', 'i:type=""'),
        ('i:type="t:recordType"', 'i:type="t::recordType"'), ('i:type="t:recordType"', 'i:type="t:"'),
        ('i:type="t:recordType"', 'i:type=":recordType"'), ('i:type="t:recordType"', 'xmlns:t="urn:wrong" i:type="t:recordType"'),
        ('<choice i:type="t:recordType"', '<record i:type="t:recordType"'), ('</detail></choice>', '</detail></record>'),
        (' a:marker="payload"', ''), (' a:marker="leaf"', ''), ('a:marker="payload"', 'xmlns:a="urn:wrong" a:marker="payload"'),
        ('<code>65535</code>', '<code>65536</code>'), ('<count>255</count>', '<count>256</count>'),
        ('<label>nested</label>', ''), ('<code>65535</code>', 'text<code>65535</code>'),
        ('<choice i:type=', '<choice i:nil="true" i:type='), ('a:marker="payload"', 'extra="x" a:marker="payload"'),
        ('<primitive i:type="t:flagType">0</primitive>', '<primitive i:type="t:flagType">yes</primitive>'),
        ('<primitive i:type="t:flagType">0</primitive>', '<primitive>0</primitive>'),
        ('<primitive i:type="t:flagType">0</primitive>', '<primitive i:type="t:recordType">0</primitive>'),
        ('</primitive></sample>', '<primitive i:type="t:flagType">0</primitive></primitive></sample>'),
        ('</sample>', '<extra/></sample>'), ('a:anchor="root"', 'extra="x" a:anchor="root"'),
    ]:
        if before in ('<choice i:type="t:recordType"', '</detail></choice>'):
            continue
        probes.append((2, 0, base.replace(before, after), False))
    probes.append((2, 0, base.replace('<choice i:type="t:recordType"', '<record i:type="t:recordType"').replace('</detail></choice>', '</detail></record>'), False))
    if empty_code:
        zero = probes[0][2]
        probes.append((0, 0, zero.replace(' xmlns=""', ''), False))
        probes.append((0, 0, zero.replace('i:type="codeType"', 'i:type="t:codeType"'), False))
    return probes


def normalized_namespaces(root):
    """Preserve every expanded name and xsi:type identity while removing default undeclarations.

    The independent libxml2 oracle accepts a no-namespace type with no default binding,
    but rejects the equivalent explicit xmlns="" scope; rebuilding with named prefixes
    keeps that provider quirk from changing the QName identity being checked.
    """
    type_name = '{http://www.w3.org/2001/XMLSchema-instance}type'
    identities = {}; uris = set()
    for node in root.iter():
        if not isinstance(node.tag, str):
            continue
        for name in [node.tag, *node.attrib]:
            uri = etree.QName(name).namespace
            if uri:
                uris.add(uri)
        if type_name in node.attrib:
            lexical = node.attrib[type_name].strip(' \t\r\n'); parts = lexical.split(':')
            if len(parts) == 1:
                uri, local = node.nsmap.get(None) or '', parts[0]
            elif len(parts) == 2 and parts[0] and parts[1] and (parts[0] in node.nsmap or parts[0] == 'xml'):
                uri = 'http://www.w3.org/XML/1998/namespace' if parts[0] == 'xml' else node.nsmap[parts[0]]
                local = parts[1]
            else:
                raise ValueError('Malformed or undeclared independent xsi:type QName')
            identities[node] = (uri, local)
            if uri:
                uris.add(uri)
    prefixes = {uri: f'n{index}' for index, uri in enumerate(sorted(uris))
                if uri != 'http://www.w3.org/XML/1998/namespace'}
    mapping = {prefix: uri for uri, prefix in prefixes.items()}

    def copy(node, outer=False):
        """Rebuild lexical declarations after capturing QName identities from original scopes."""
        result = etree.Element(node.tag, nsmap=mapping if outer else None)
        for name, value in node.attrib.items():
            if name == type_name:
                uri, local = identities[node]
                prefix = 'xml' if uri == 'http://www.w3.org/XML/1998/namespace' else prefixes.get(uri, '')
                value = (prefix + ':' if prefix else '') + local
            result.set(name, value)
        result.text = node.text; result.tail = node.tail
        for child in node:
            result.append(copy(child))
        return result
    return copy(root, True)


def main():
    """Run all actual SDKs in both namespace modes and revalidate every emitted typed payload."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp,go')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler = args.compiler.resolve(); args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(language not in ('java', 'csharp', 'go') for language in languages):
        parser.error('Select distinct language names from java,csharp,go')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    runner = sdk.Runner(args); total = 0
    for empty_code in [False, True]:
        directory = args.build / ('empty-code' if empty_code else 'namespaced'); directory.mkdir(parents=True, exist_ok=True)
        oracle = prepare_oracle(directory, empty_code); paths = []
        for index, (selected, primitive, document, valid) in enumerate(cases(empty_code)):
            try:
                accepted = oracle.validate(normalized_namespaces(etree.fromstring(document.encode())))
            except ValueError:
                accepted = False
            if accepted != valid:
                raise AssertionError(f'Independent XSD decision differs for {index}: {oracle.error_log}')
            path = directory / f'v{selected}u{primitive}-{index}.{"valid" if valid else "invalid"}.xml'; path.write_text(document, encoding='utf-8'); paths.append(path)
        model = directory / 'model.serializer'; model.write_text(MODEL, encoding='utf-8')
        options = directory / 'variants.ini'; options.write_text(config(empty_code), encoding='utf-8')
        command = [args.compiler, '--input', model, '--config', options, '--language', ','.join(languages), '--protocols', 'binary_none', '--go.package', 'main']
        for language in languages:
            target = directory / language; target.mkdir(parents=True, exist_ok=True)
            command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
        sdk.run(command)
        sources = {'java': ('Main.java', fixture.JAVA), 'csharp': ('Program.cs', fixture.CSHARP), 'go': ('main.go', fixture.GO)}
        for language in languages:
            source = directory / 'source' / language; source.mkdir(parents=True, exist_ok=True)
            filename, content = sources[language]; (source / filename).write_text(content, encoding='utf-8')
            for path in paths:
                Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
            launch = runner.build(language, 'xml_typed_variants', directory / language, source)
            sdk.run([*launch, *(runner.path(language, path) for path in paths)])
            for path in paths:
                emitted = Path(str(path) + f'.{language}.xml')
                if path.name.endswith('.valid.xml'):
                    oracle.assertValid(normalized_namespaces(etree.parse(str(emitted)).getroot()))
                elif emitted.exists():
                    raise AssertionError(f'Invalid typed output published: {emitted}')
        total += len(paths)
    print(f'PASS: {total} independent xsi:type probes across {len(languages)} SDKs, including empty type namespaces and inherited prefix collisions', flush=True)


if __name__ == '__main__':
    main()
