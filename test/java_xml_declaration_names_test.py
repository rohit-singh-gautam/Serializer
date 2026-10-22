# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Retain authored Java helper-like model names while private wildcard codecs choose distinct helpers."""
import xml_wildcard_validation_test as shared

shared.configure(process='strict', retention='preserve')
NAMES = ('SrlXmlDeclaration', 'SrlXmlDeclarationType', 'SrlXmlDeclarations', 'SrlXmlDeclarationPolicy')
for name in NAMES:
    shared.ordered.MODEL += f'\nclass {name} {{ public string payload {{ "{name}" }}; }}\n'
checks = '\n'.join(
    f'    if(!new String(new Schema.{name}().encode(Schema.Protocol.JSON),java.nio.charset.StandardCharsets.UTF_8).contains("{name}"))throw new AssertionError("Authored model identity changed");'
    for name in NAMES)
shared.ordered.JAVA = shared.ordered.JAVA.replace('    for(int index=0;', checks + '\n    for(int index=0;', 1)
if __name__ == '__main__':
    shared.ordered.main()
