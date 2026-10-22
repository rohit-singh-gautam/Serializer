# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Run the independent positioned wildcard corpus through actual generated C++ tree/token facades."""
import argparse
import os
from pathlib import Path
import shutil
import tempfile

import xml_wildcard_skip_test as shared

ordered = shared.ordered
ROOT, sdk = ordered.ROOT, ordered.sdk
CPP = r'''#include "schema.hpp"
#include <rohit/xml.hpp>
#include <rohit/xml_identity.hpp>
#include <rohit/xml_tokens.hpp>
#include <array>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
namespace srl=rohit::serializer;namespace xml=srl::xml;
// A private declared owner must use the caller's independent identity quota in both directions.
template<typename Model>void identity_probe(const std::string& input,const Model& value){
xml::identity_limits zero{};zero.max_tuples=0;
for(bool token:{false,true}){for(bool encoding:{false,true}){
xml::operation_context context{};context.identity_bounds=&zero;bool rejected=false;
try{if(encoding){static_cast<void>(token?xml::serialize_tokens(value,{},&context):xml::serialize(value,{},nullptr,&context));}
else{static_cast<void>(token?xml::deserialize_tokens<Model>(input,{},&context):xml::deserialize<Model>(input,{},nullptr,&context));}}
catch(const std::length_error&){rejected=true;}
if(!rejected||context.identity_bounds!=&zero){throw std::runtime_error{"Private declaration reset identity policy"};}
}}
}
// Keep mutable caller-owned counters observable across generated policy overlays and nested DTOs.
struct admission {
std::array<std::size_t,3> counts{};std::optional<xml::operation_resource> selected{};
std::size_t maximum{};const int* failure{};
// Reject before any work or storage exceeds the selected cumulative admission.
void operator()(xml::operation_resource resource,std::size_t count){auto& used=counts[static_cast<std::size_t>(resource)];used+=count;if(selected==resource&&used>maximum){throw failure;}}
};
// Admit exact boundaries, reject one-tight budgets with original failure identity, and recover fresh.
template<typename Model>void admission_probe(const std::string& input,const Model& value){
constexpr bool preserve=!RETENTION_SKIP;
xml::extension_limits extensions{};if constexpr(!preserve){extensions.max_preserved_bytes=0;}
for(bool token:{false,true}){for(bool encoding:{false,true}){
const auto operation=[&](xml::operation_context& context){context.extension_bounds=&extensions;
if(encoding){static_cast<void>(token?xml::serialize_tokens(value,{},&context):xml::serialize(value,{},nullptr,&context));}
else{static_cast<void>(token?xml::deserialize_tokens<Model>(input,{},&context):xml::deserialize<Model>(input,{},nullptr,&context));}};
xml::operation_context baseline{};baseline.charge=admission{};operation(baseline);
const auto counts=baseline.charge.target<admission>()->counts;
if(counts[0]==0||counts[1]==0){throw std::runtime_error{"Nested validation copied the caller ledger"};}
if constexpr(!preserve){if(counts[2]!=0){throw std::runtime_error{"Temporary declaration retained opaque bytes"};}}
for(std::size_t index=0;index<counts.size();++index){if(counts[index]==0){continue;}
const auto resource=static_cast<xml::operation_resource>(index);const int original=1;
xml::operation_context exact{};exact.charge=admission{{},resource,counts[index],&original};operation(exact);
xml::operation_context tight{};tight.charge=admission{{},resource,counts[index]-1U,&original};const int* caught=nullptr;
try{operation(tight);}catch(const int* failure){caught=failure;}if(caught!=&original){throw std::runtime_error{"Nested declaration reset admission"};}
}
operation(baseline);
}}
if constexpr(preserve){auto edited=value;auto* state=edited.xml_extensions.get();if(!state||state->children.empty()){throw std::runtime_error{"Missing declared extension"};}
auto& child=state->children.front().value;if(child.children.empty()){throw std::runtime_error{"Missing declared nested field"};}child.children.front().text="0";
for(bool token:{false,true}){bool rejected=false;try{static_cast<void>(token?xml::serialize_tokens(edited):xml::serialize(edited));}catch(const std::invalid_argument&){rejected=true;}if(!rejected){throw std::runtime_error{"Edited declared content accepted"};}}
}}
// Observe native payload bytes independently from the XML-only opaque companion.
template<template<srl::serialize_type>class Protocol,typename Model>
std::string native_bytes(const Model& value){rohit::full_stream_auto_alloc bytes{};value.template serialize_out<Protocol>(bytes);return {reinterpret_cast<const char*>(bytes.begin()),bytes.current_offset()};}
// Check both public XML paths, all native protocols, and bounded publication for one typed owner.
template<typename Model>void check(const std::string& input,const std::string& path,bool valid){
for(bool token:{false,true}){Model value{};try{value=token?xml::deserialize_tokens<Model>(input):xml::deserialize<Model>(input);}catch(const std::exception&){if(valid)throw;continue;}
if(!valid)throw std::runtime_error{"Invalid wildcard input accepted: "+path};
if(path.ends_with("sequence-nested.valid.xml")){admission_probe(input,value);}
if(path.ends_with("key-valid.valid.xml")){identity_probe(input,value);}
const auto output=token?xml::serialize_tokens(value):xml::serialize(value);const auto copy=xml::deserialize_tokens<Model>(output);
if(native_bytes<srl::json>(value)!=native_bytes<srl::json>(copy)||native_bytes<srl::binary_none>(value)!=native_bytes<srl::binary_none>(copy)||native_bytes<srl::binary_integer>(value)!=native_bytes<srl::binary_integer>(copy)||native_bytes<srl::binary_string>(value)!=native_bytes<srl::binary_string>(copy))throw std::runtime_error{"Native wire identity changed"};
xml::limits tight{};tight.max_output_bytes=output.size()-1U;std::string published{"previous"};bool rejected=false;
try{published=xml::serialize_tokens(value,tight);}catch(const std::length_error&){rejected=true;}if(!rejected||published!="previous")throw std::runtime_error{"Failed XML output published"};
std::ofstream file{path+".cpp.xml",std::ios::binary};file.write(output.data(),static_cast<std::streamsize>(output.size()));if(!file)throw std::runtime_error{"Cannot write result"};}}
// Dispatch untouched fixtures by owning type; the second argument is the common corpus's fragment bound.
int main(int argc,char** argv){try{for(int index=1;index+1<argc;index+=2){const std::string path{argv[index]};const bool valid=path.ends_with(".valid.xml");std::ifstream file{path,std::ios::binary};const std::string input{std::istreambuf_iterator<char>{file},{}};
try{if(std::filesystem::path{path}.filename().string().starts_with("fixed-"))check<ordered::fixed>(input,path,valid);else check<ordered::root>(input,path,valid);}catch(const std::exception& failure){throw std::runtime_error{path+": "+failure.what()};}}return 0;}catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}}
'''


def main():
    """Reuse independently classified XSD fixtures while compiling one bounded C++ SDK per variant."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--namespace-rule', default='##any')
    parser.add_argument('--position', choices=('prefix', 'middle', 'suffix'), default='suffix')
    parser.add_argument('--process', choices=('strict', 'lax'))
    parser.add_argument('--group', choices=('all', 'choice', 'choice-explicit'))
    parser.add_argument('--identity-policy', action='store_true')
    parser.add_argument('--empty-inventory', action='store_true')
    parser.add_argument('--retention', choices=('preserve', 'skip'), default='preserve')
    parser.add_argument('--unknown-policy', action='store_true')
    parser.add_argument('--field-count', type=int, default=5)
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--cpp-include', type=Path, default=ROOT / 'include')
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Output must remain under ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='wildcard-cpp-', dir=args.build) as directory:
        args.build = Path(directory)

        def execute(selected):
            """Use the real compiler and SDK, then observe original and emitted documents independently."""
            from lxml import etree
            source, config, header = selected.build / 'model.serializer', selected.build / 'options.ini', selected.build / 'schema.hpp'
            source.write_text(ordered.MODEL, encoding='utf-8')
            config.write_text(ordered.CONFIG, encoding='utf-8')
            sdk.run([selected.compiler, '--input', source, '--config', config, '--language', 'cpp',
                     '--output', header, '--cpp.format', 'false'])
            (selected.build / 'main.cpp').write_text(
                CPP.replace('RETENTION_SKIP', str(selected.retention == 'skip').lower()), encoding='utf-8')
            executable = selected.build / ('main.exe' if os.name == 'nt' else 'main')
            library = selected.compiler.parent / ('serializer_runtime.lib' if os.name == 'nt' else 'libserializer_runtime.a')
            prefix = selected.libxml2_prefix or selected.compiler.parent / 'vcpkg_installed/x64-windows'
            if os.name == 'nt':
                sdk.run(['cl', '/nologo', '/std:c++20', '/EHsc', '/MD', '/O2', '/W4', '/WX', '/utf-8',
                         '/I'+str(selected.cpp_include.resolve()), 'main.cpp', str(library),
                         *[str(prefix / 'lib' / name) for name in ('zstd.lib', 'lz4.lib', 'z.lib', 'libxml2.lib')],
                         '/Fe:'+str(executable)], cwd=selected.build)
                for dll in (prefix / 'bin').glob('*.dll'):
                    shutil.copyfile(dll, selected.build / dll.name)
            else:
                sdk.run(['c++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                         '-I'+str(selected.cpp_include.resolve()), 'main.cpp', str(library), '-lxml2', '-lzstd', '-llz4', '-lz',
                         '-o', executable], cwd=selected.build)
            schema_name = 'groups.xsd' if selected.group else 'validation.xsd' if selected.process or selected.identity_policy else 'wildcards.xsd'
            validator = None if selected.unknown_policy or selected.group == 'choice-explicit' else etree.XMLSchema(etree.parse(str(selected.build / schema_name)))
            probes, arguments = ordered.cases(), []
            for name, document, maximum, valid in probes:
                path = selected.build / f'{name}.{"valid" if valid else "invalid"}.xml'
                path.write_text(document, encoding='utf-8')
                arguments += [path, maximum]
            sdk.run([executable, *arguments])
            for name, document, _, valid in probes:
                path = selected.build / f'{name}.{"valid" if valid else "invalid"}.xml.cpp.xml'
                if valid:
                    output = etree.parse(str(path))
                    if validator is not None:
                        assert validator.validate(output), validator.error_log
                    assert ordered.observe(etree.fromstring(document.encode())) == ordered.observe(output.getroot()), path
                else:
                    assert not path.exists(), path
            print(f'PASS: cpp: {len(probes)} wildcard decisions, both XML paths, four native protocols', flush=True)

        ordered.execute = execute
        if args.empty_inventory:
            import xml_wildcard_empty_inventory_test as empty_inventory
            empty_inventory.configure(args.process or 'strict', args.retention)
        elif args.identity_policy:
            import xml_wildcard_identity_policy_test as identities
            identities.configure()
        elif args.group:
            import xml_group_extensions_test as groups
            groups.configure(args.group, args.process or 'skip')
        elif args.unknown_policy:
            import xml_unknown_policy_test as unknown
            unknown.configure(args.field_count, args.retention)
        elif args.process:
            import xml_wildcard_validation_test as validation
            validation.configure(args.process, args.retention)
        else:
            shared.configure(args.namespace_rule, args.position)
        ordered.execute(args)


if __name__ == '__main__':
    main()
