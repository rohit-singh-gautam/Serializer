# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated native callbacks against a real immutable prepared libxml2 test bridge on Windows."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parent.parent
SOURCE=ROOT/'test/resources/xml_provider'
spec=importlib.util.spec_from_file_location('serializer_native_provider_sdk',ROOT/'example/run.py')
sdk=importlib.util.module_from_spec(spec);spec.loader.exec_module(sdk)
spec=importlib.util.spec_from_file_location('serializer_native_provider_xml_sdk',ROOT/'example/xml/run.py')
xml_sdk=importlib.util.module_from_spec(spec);spec.loader.exec_module(xml_sdk)


def main():
    """Require actual native SDKs and a supplied freshly built provider DLL; no mock provider is substituted."""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--bridge',required=True,type=Path);parser.add_argument('--build',required=True,type=Path);parser.add_argument('--language',default='go');parser.add_argument('--wsl-languages',default='');parser.add_argument('--libxml2-prefix',type=Path);parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args();args.compiler=args.compiler.resolve();args.bridge=args.bridge.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if os.name!='nt':parser.error('This real-provider SDK fixture requires the native Windows bridge and Windows SDKs')
    if not args.bridge.is_file():parser.error('Prepared provider DLL must already exist')
    if not languages or len(languages)!=len(set(languages)) or any(language not in ('go','swift','rust','c') for language in languages):parser.error('Choose distinct names from go,swift,rust,c')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:parser.error('In-repository output must remain beneath ignored out')
    args.build.mkdir(parents=True,exist_ok=True);command=[args.compiler,'--input',ROOT/'test/resources/xml_provider.serializer','--config',ROOT/'test/resources/xml_provider.ini','--xml.validation_scope','both','--language',','.join(languages),'--protocols','binary_none','--go.package','main']
    if args.libxml2_prefix:args.libxml2_prefix=args.libxml2_prefix.resolve()
    if 'rust' in languages:
        os.environ.setdefault('CARGO_HOME',str(args.build/'cargo-home'));os.environ['RUSTFLAGS']='-D warnings'
    for language in languages:
        source=SOURCE/language
        if not source.is_dir():parser.error('Actual prepared provider consumer is not available for '+language)
        directory=args.build/language;directory.mkdir(exist_ok=True)
        for path in source.iterdir():
            if path.is_file():shutil.copyfile(path,directory/path.name)
        command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args)
    for language in languages:
        launch=xml_sdk.build_language(args,runner,language,args.build/language,source_root=SOURCE) if language in ('c','rust') else runner.build(language,'native_xml_provider',args.build/language,SOURCE/language)
        sdk.run([*launch,runner.path(language,args.bridge),runner.path(language,ROOT/'test/resources/xml_xsd_fixture.xsd')])
    print(f'PASS: real prepared libxml2 complete-message callbacks, bounds, concurrency and no-fetch across {len(languages)} actual native SDKs',flush=True)


if __name__=='__main__':main()
