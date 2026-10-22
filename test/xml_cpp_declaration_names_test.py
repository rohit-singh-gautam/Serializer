# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Deliberately collide authored C++ names with generated wildcard namespaces before compiling the SDK."""
from pathlib import Path
import re
import sys

import xml_wildcard_skip_cpp_test as shared


def main():
    """Discover deterministic private names, regenerate with collisions, and retain the independent corpus."""
    original_run = shared.sdk.run
    generated = False
    profile_collision = '--profile-collision' in sys.argv
    if profile_collision:
        sys.argv.remove('--profile-collision')

    def run(command, *args, **kwargs):
        """Regenerate the same authored model after discovering its private declaration namespace."""
        nonlocal generated
        result = original_run(command, *args, **kwargs)
        arguments = [str(value) for value in command]
        if generated or '--output' not in arguments or '--input' not in arguments:
            return result
        generated = True
        header = Path(arguments[arguments.index('--output') + 1])
        source = Path(arguments[arguments.index('--input') + 1])
        namespaces = sorted(set(re.findall(r'\bxml_decl_validator_[0-9a-f]{64}\b', header.read_text(encoding='utf-8'))))
        if not namespaces:
            raise AssertionError('The strict wildcard corpus emitted no private DTO namespaces')
        selected = namespaces[0]
        authored_private = selected.replace('xml_decl_validator_', 'xmlDeclValidator_', 1) if profile_collision else selected
        callback_declaration = ('namespace serializerXmlGeneratedDeclarations { class authored { public string payload; } }'
                                if profile_collision else 'class serializer_xml_generated_declarations { public string payload; }')
        source.write_text(source.read_text(encoding='utf-8') + '\n' +
                          callback_declaration + '\n' +
                          f'namespace {authored_private} {{ class authored {{ public string payload; }} }}\n',
                          encoding='utf-8')
        original_run(command, *args, **kwargs)
        regenerated = header.read_text(encoding='utf-8')
        # Retain the exact collision result even when the SDK harness removes its temporary directory.
        (header.parent.parent / 'collision-schema.hpp').write_text(regenerated, encoding='utf-8')
        (header.parent.parent / 'collision-model.serializer').write_text(source.read_text(encoding='utf-8'), encoding='utf-8')
        if not re.search(r'namespace\s+' + re.escape(selected) + r'_private_\d+\b', regenerated):
            raise AssertionError('The private DTO namespace did not avoid the authored namespace')
        if not re.search(r'namespace\s+serializer_xml_generated_declarations_\d+\b', regenerated):
            raise AssertionError('The callback namespace did not avoid the authored class')
        checks = '''const auto same_native=[](const auto& first,const auto& second){return
native_bytes<srl::json>(first)==native_bytes<srl::json>(second)&&
native_bytes<srl::binary_none>(first)==native_bytes<srl::binary_none>(second)&&
native_bytes<srl::binary_integer>(first)==native_bytes<srl::binary_integer>(second)&&
native_bytes<srl::binary_string>(first)==native_bytes<srl::binary_string>(second);};
const char* collision_phase="callback";try{
CALLBACK_TYPE callback{};callback.payload="authored callback";
const auto callback_copy=xml::deserialize_tokens<CALLBACK_TYPE>(xml::serialize(callback));
if(callback_copy.payload!=callback.payload||!same_native(callback,callback_copy))throw std::runtime_error{"Authored callback identity changed"};
collision_phase="private namespace";NAMESPACE::authored payload{};payload.payload="authored private namespace";
const auto payload_copy=xml::deserialize<NAMESPACE::authored>(xml::serialize_tokens(payload));
if(payload_copy.payload!=payload.payload||!same_native(payload,payload_copy))throw std::runtime_error{"Authored private namespace identity changed"};
}catch(const std::exception& failure){throw std::runtime_error{std::string{"Authored "}+collision_phase+": "+failure.what()};}
'''.replace('NAMESPACE', selected).replace('CALLBACK_TYPE',
    'serializer_xml_generated_declarations::authored' if profile_collision else 'serializer_xml_generated_declarations')
        shared.CPP = shared.CPP.replace('int main(int argc,char** argv){try{',
                                        'int main(int argc,char** argv){try{' + checks, 1)
        shared.CPP = shared.CPP.replace('catch(const std::exception&){if(valid)throw;',
                                        'catch(const std::exception& failure){if(valid)throw std::runtime_error{path+": "+failure.what()};')
        return result

    shared.sdk.run = run
    if '--process' not in sys.argv:
        sys.argv += ['--process', 'strict']
    shared.main()
    if not generated:
        raise AssertionError('The real C++ generation pipeline was not exercised')
    print('PASS: authored C++ private/callback namespace collisions retain XML and native identities')


if __name__ == '__main__':
    main()
