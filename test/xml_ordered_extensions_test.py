# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify foreign end slots independently from owning ordered fragment quotas."""
import copy
import sys

from lxml import etree

import xml_ordered_portable_test as ordered


def configure():
    """Keep the established SDK consumers while adding exact foreign suffix permissions."""
    original_cases = ordered.cases
    properties = '''extension.suffix.kind=element
extension.suffix.namespaces=urn:foreign
extension.suffix.policy=preserve
extension.suffix.minimum=1
extension.suffix.maximum=1
extension.attributes.kind=attribute
extension.attributes.namespaces=urn:foreign
extension.attributes.policy=preserve
extension.attributes.maximum=1
'''
    for owner in ('root', 'fixed'):
        section = f'[xml.class.ordered::{owner}]\n'
        ordered.CONFIG = ordered.CONFIG.replace(section, section + properties)

    def cases():
        """Foreign items retain their own quotas and cannot occur before later native children."""
        result = []
        for name, document, maximum, valid in original_cases():
            root = etree.fromstring(document.encode())
            root.set('{urn:foreign}note', 'opaque')
            suffix = etree.SubElement(root, '{urn:foreign}suffix')
            suffix.text = 'unbound:opaque'
            result.append((name, etree.tostring(root, encoding='unicode'), maximum, valid))
        root = etree.fromstring(result[2][1].encode())
        suffix = root[-1]
        root.remove(suffix)
        result.append(('missing-suffix', etree.tostring(root, encoding='unicode'), 3, False))
        root.insert(0, suffix)
        result.append(('early-suffix', etree.tostring(root, encoding='unicode'), 3, False))
        root.remove(suffix)
        root.extend([suffix, copy.deepcopy(suffix)])
        result.append(('excess-suffix', etree.tostring(root, encoding='unicode'), 3, False))
        root.remove(root[-1])
        suffix.tail = 'late native text'
        result.append(('late-text', etree.tostring(root, encoding='unicode'), 3, False))
        return result

    ordered.cases = cases


if __name__ == '__main__':
    configure()
    if '--language' not in sys.argv:
        sys.argv += ['--language', 'python,javascript,typescript,java,csharp,go,kotlin,swift']
    ordered.main()
