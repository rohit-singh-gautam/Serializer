# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Run the independent wildcard corpus through C package document descriptors and exact operation bounds."""
from pathlib import Path
import sys
import xml_wildcard_validation_test as validation
import xml_ordered_portable_test as ordered


def main():
    """Add only a package declaration and public descriptor/resource checks to the shared semantic corpus."""
    settings = {}
    for key in ('process', 'retention'):
        option = '--' + key
        if option in sys.argv:
            index = sys.argv.index(option)
            settings[key] = sys.argv[index + 1]
            del sys.argv[index:index + 2]
    validation.configure(**settings)
    prefix, _ = ordered.MODEL.rsplit('}', 1)
    ordered.MODEL = prefix + '''package archive { part main : root { name("/main.xml"); content_type("application/vnd.serializer.wildcard"); relationship("urn:serializer:wildcard:main"); policy(defaults); } }\n}\n'''
    admission = (Path(__file__).parent / 'resources/native_wildcard_admission.inc').read_text(encoding='utf-8-sig').replace('RETENTION_SKIP', 'true' if settings.get('retention') == 'skip' else 'false')
    ordered.C = ordered.C.replace('CHECKS', admission + '\nCHECKS')
    ordered.C_CHECK = ordered.C_CHECK.replace('done:ordered_KIND_xml_document_free', '''if(!strcmp("KIND","root")&&status==srl_ok&&declaration_find(input,"<f:packet mode=\\"AB\\"")!=SIZE_MAX){static bool probed=false;if(!probed){probed=true;declaration_admission_probe(input,(const ordered_root_xml_document*)&value);}}
done:ordered_KIND_xml_document_free''')
    ordered.main()


if __name__ == '__main__':
    main()
