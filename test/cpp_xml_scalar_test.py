# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare real CMake-generated C++ scalar codecs with the shared independent XSD corpus."""
import argparse
from pathlib import Path
import subprocess

from lxml import etree

import portable_xml_facets_test as shared
import portable_xml_scalar_test as lexical


def main():
    """Verify both decisions and emitted documents while preserving all oracle evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build = args.build.resolve()
    if args.build == shared.ROOT or shared.ROOT in args.build.parents and 'out' not in args.build.relative_to(shared.ROOT).parts:
        parser.error('Repository output must remain beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    paths = []
    resources = shared.ROOT / 'test/resources/xml_scalar'
    lexical_paths = set()
    for mode in ('numeric', 'pattern', 'large'):
        xsd, _, probes = shared.numeric_cases(args.build) if mode == 'numeric' else shared.pattern_cases(mode == 'large')
        if (resources / (mode + '.xsd')).read_text(encoding='utf-8') != xsd:
            raise AssertionError(f'CMake scalar input drifted from the shared oracle: {mode}')
        oracle = etree.XMLSchema(etree.fromstring(xsd.encode()))
        for name, document, scalar, valid in probes:
            if name not in ('work', 'bytes') and oracle.validate(etree.fromstring(document)) != valid:
                raise AssertionError(f'Independent scalar decision differs: {name}: {oracle.error_log}')
            if mode == 'large' and name.startswith('pattern'):
                name = name.replace('pattern', 'large', 1)
            path = args.build / f'{name}.{"valid" if valid else "invalid"}.xml'
            path.write_bytes(document)
            if scalar is not None:
                Path(str(path) + '.txt').write_text(scalar, encoding='utf-8')
            Path(str(path) + '.cpp.xml').unlink(missing_ok=True)
            paths.append((path, oracle, valid))
    qname_oracle = etree.XMLSchema(etree.parse(str(resources.parent / 'xml_xsd_fixture.xsd')))
    scalar = '<qualified_names xmlns="urn:serializer:xsd" xmlns:p="urn:serializer:names" xmlns:t="urn:types" name="p:thing" names="p:one t:two" required="p t">t:value</qualified_names>'
    qnames = [scalar, scalar.replace('name="p:thing"', 'name="p:other"'),
              scalar.replace('t:value</', 'unbound:value</'), scalar.replace('p:one t:two', 'p:one'),
              scalar.replace('p:one t:two', 'p:one unbound:two'), scalar.replace('>t:value<', '>value<'),
              scalar.replace(' xmlns:t="urn:types"', ' xmlns:t="urn:types" xmlns:alias="urn:serializer:names"')
                    .replace('name="p:thing"', 'name="alias:thing"')]
    children = ['<qname_children xmlns="urn:serializer:xsd" xmlns:p="urn:types"><value>p:one</value><value xmlns:p="urn:types">p:two</value></qname_children>',
                '<qname_children xmlns="urn:serializer:xsd"><value>unbound:one</value></qname_children>',
                '<qname_children xmlns="urn:serializer:xsd"/>']
    for kind, probes in (('qname', qnames), ('qname-child', children)):
        for index, document in enumerate(probes):
            valid = qname_oracle.validate(etree.fromstring(document.encode()))
            path = args.build / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'
            path.write_text(document, encoding='utf-8')
            Path(str(path) + '.cpp.xml').unlink(missing_ok=True)
            paths.append((path, qname_oracle, valid))
    if (resources / 'scalars.xsd').read_text(encoding='utf-8') != lexical.XSD:
        raise AssertionError('CMake lexical input drifted from the shared independent corpus')
    lexical_oracle = lexical.oracle_schema()
    for index, (kind, document, valid) in enumerate(lexical.cases()):
        if lexical.independent(document.encode(), lexical_oracle) != (valid or kind in ('guard-work', 'items-limit', 'items-text')):
            raise AssertionError(f'Independent arbitrary-precision lexical decision differs: {kind}-{index}')
        path = args.build / f'scalar-{kind}-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8')
        Path(str(path) + '.cpp.xml').unlink(missing_ok=True)
        paths.append((path, lexical_oracle, valid)); lexical_paths.add(path)
    default_oracle = etree.XMLSchema(etree.parse(str(resources / 'defaults.xsd')))
    for kind, probes in (
            ('collapsed', ['<collapsed/>', '<collapsed> </collapsed>', '<collapsed> custom </collapsed>']),
            ('preserved', ['<preserved/>', '<preserved> </preserved>', '<preserved>custom</preserved>']),
            ('replaced', ['<replaced/>', '<replaced>\t</replaced>', '<replaced>custom</replaced>']),
            ('fixed', ['<required_fixed flag="true"/>', '<required_fixed flag="1"/>',
                       '<required_fixed/>', '<required_fixed flag="false"/>'])):
        for index, document in enumerate(probes):
            valid = default_oracle.validate(etree.fromstring(document.encode()))
            path = args.build / f'default-{kind}-{index}.{"valid" if valid else "invalid"}.xml'
            path.write_text(document, encoding='utf-8')
            Path(str(path) + '.cpp.xml').unlink(missing_ok=True)
            paths.append((path, default_oracle, valid))
    result = subprocess.run([str(args.executable.resolve()), *(str(path) for path, _, _ in paths)],
                            capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    print(result.stdout, end='')
    for path, oracle, valid in paths:
        output = Path(str(path) + '.cpp.xml')
        if valid:
            if path in lexical_paths:
                if not lexical.independent(output.read_bytes(), oracle):
                    raise AssertionError(f'C++ output differs from the independent arbitrary-precision oracle: {path}')
            else:
                oracle.assertValid(etree.parse(str(output)))
        elif output.exists():
            raise AssertionError(f'Rejected scalar probe published XML: {path}')
    print(f'PASS: {len(paths)} C++ exact numeric, Unicode and prepared-pattern oracle probes', flush=True)


if __name__ == '__main__':
    main()
