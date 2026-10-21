# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Independent RFC3986/3987 component and OPC relationship-attribute controls."""
import xml.etree.ElementTree as etree

import python_package_test as independent


def scalar_delimiter_cases():
    """Keep excluded tag-plane combining scalars visible immediately after actual ASCII delimiters."""
    for index,target in enumerate(('urn:\U000e0100value','https://user@\U000e0100host.invalid/',
                                   'https://host.invalid/?\U000e0100','https://host.invalid/#\U000e0100')):
        yield f'iri-scalar-delimiter-{index}', independent.independent_zip(independent.independent_external_relationships((target,))), False


def ip_literal_cases():
    """Reject three consecutive colons independently of the permitted single IPv6 compression marker."""
    for index,target in enumerate(('https://[:::]/','https://[1:::2]/','https://[:::1]/','https://[1:::]/')):
        yield f'iri-ip-literal-{index}', independent.independent_zip(independent.independent_external_relationships((target,))), False


def cases():
    """Apply generic IRI grammar without transport, DNS, scheme-specific policy or Unicode normalization."""
    good = ('', '?', '#', '../a/b?view=1#node', '//host.invalid/a',
            'https://left/right', 'https://left\uff0fright', 'https://left?right',
            'https://left\uff1fright', 'https://left#right', 'https://left\uff03right',
            'https://left@right', 'https://left\uff20right', 'https://left:80',
            'https://left\uff1a80', 'https://user\uff20name@host.invalid/a',
            'https://caf\u00e9.invalid/a', 'https://host%2Fname.invalid/a',
            'https://[2001:db8::1]:443/a', 'https://[vF.name:tail]/a', 'https:///path',
            'https://host.invalid/?private=\ue000', 'https://[::ffff:192.0.2.1]/',
            'urn:opaque:part%00value', 'foo:')
    bad = ('https://host:port/', 'https://host:\uff11/', 'https://[::1/',
           'https://[::1]tail/', 'https://[not-ip]/', 'https://[1:2:3]/',
           'https://[fe80::1%25zone]/', 'https://host]/', 'https://[::1]]/',
           'https://user@@host/', 'https://host/%zz', 'https://host/%',
           'https://host/a#one#two', '1scheme://host/', 'https://host\\evil/',
           'https://host\ue000.invalid/', 'https://host/\ue000', 'https://host/#\ue000',
           'https://[1:2:3:4:5:6:7:8:9]/', 'https://[::ffff:192.000.2.1]/',
           'https://[v1.]/', 'https://[v.invalid]/', 'https://host/^bad')
    for index, target in enumerate(good):
        yield f'iri-external-good-{index}', independent.independent_zip(independent.independent_external_relationships((target,))), True
    for index, target in enumerate(bad):
        yield f'iri-external-bad-{index}', independent.independent_zip(independent.independent_external_relationships((target,))), False
    for index, kind in enumerate(('urn:vendor:external', 'foo:', 'https://[::1]/type', 'urn:vendor:\u00e9')):
        yield f'iri-type-good-{index}', independent.independent_zip(independent.independent_external_relationships(('https://host.invalid/',), kind)), True
    for index, kind in enumerate(('', 'relative/type', 'https://host.invalid/#fragment', 'https://host:port/type', 'urn:invalid^type')):
        yield f'iri-type-bad-{index}', independent.independent_zip(independent.independent_external_relationships(('https://host.invalid/',), kind)), False
    for field in ('Id', 'Type', 'Target'):
        parts = independent.independent_external_relationships(('https://host.invalid/',))
        root = etree.fromstring(parts['_rels/.rels']); root[-1].attrib.pop(field)
        parts['_rels/.rels'] = etree.tostring(root)
        yield 'iri-missing-' + field.lower(), independent.independent_zip(parts), False
    parts = independent.independent_external_relationships(('https://host.invalid/',))
    root = etree.fromstring(parts['_rels/.rels']); root[-1].set('TargetMode', ' External ')
    parts['_rels/.rels'] = etree.tostring(root)
    yield 'iri-targetmode-string-whitespace', independent.independent_zip(parts), False
    yield from ip_literal_cases()
