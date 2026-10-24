# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Independently produce and inspect generated native/XML OPC profile pipelines."""
import argparse
import hashlib
import json
import os
import re
import shutil
from pathlib import Path
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NAMESPACE = 'urn:serializer:package-pipeline'
MIME = 'application/vnd.serializer.pipeline'
CT = 'http://schemas.openxmlformats.org/package/2006/content-types'
RELS = 'http://schemas.openxmlformats.org/package/2006/relationships'
REL_MIME = 'application/vnd.openxmlformats-package.relationships+xml'
EXTERNAL = 'https://example.invalid/never-fetch?q=%2f#inert'
MODEL = {'id': 17, 'label': 'pipeline', 'values': [2, 4, 8]}


def payload(codec, attributes=''):
    """Write documented scalar/array wire bytes without invoking a generated model codec."""
    if codec == 'json':
        return json.dumps(MODEL, separators=(',', ':')).encode()
    if codec == 'binary_none':
        text = MODEL['label'].encode()
        return struct.pack('<I', MODEL['id']) + bytes([len(text)]) + text + bytes([len(MODEL['values'])]) + struct.pack('<III', *MODEL['values'])
    return (f'<p:payload xmlns:p="{NAMESPACE}" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" '
            f'xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" '
            f'xmlns:k="urn:serializer:pipeline:understood" xmlns:t="urn:serializer:native:pipeline" xsi:type="t:payload" id="17" label="pipeline" {attributes}>'
            '<p:value>2</p:value><p:value>4</p:value><p:value>8</p:value></p:payload>').encode()


def author(path, data, fixed_method=zipfile.ZIP_STORED, link_mode='External', fixed_codec='json'):
    """Create ordinary OPC infrastructure with independent ZIP and expanded-name XML providers."""
    content = ET.Element(f'{{{CT}}}Types')
    relationships = ET.Element(f'{{{RELS}}}Relationships')
    parts = [('main.data', data, zipfile.ZIP_DEFLATED), ('fixed.data', payload(fixed_codec), fixed_method)]
    for name, _, _ in parts:
        ET.SubElement(content, f'{{{CT}}}Override', PartName='/' + name, ContentType=MIME)
        ET.SubElement(relationships, f'{{{RELS}}}Relationship', Id='input_' + name.split('.')[0],
                      Type='urn:serializer:pipeline:' + name.split('.')[0], Target=name)
    if link_mode:
        ET.SubElement(relationships, f'{{{RELS}}}Relationship', Id='external', Type='urn:serializer:pipeline:link', Target=EXTERNAL, TargetMode=link_mode)
    ET.SubElement(content, f'{{{CT}}}Override', PartName='/_rels/.rels', ContentType=REL_MIME)
    with zipfile.ZipFile(path, 'w') as archive:
        archive.writestr('[Content_Types].xml', ET.tostring(content), compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr('_rels/.rels', ET.tostring(relationships), compress_type=zipfile.ZIP_DEFLATED)
        for name, value, method in parts:
            archive.writestr(name, value, compress_type=method)


def cases(directory, binary_only=False):
    """Materialize acceptance and rejection fixtures, retaining expected selected envelopes in a manifest."""
    directory.mkdir(parents=True, exist_ok=True)
    result = []
    for codec in ('xml', 'json', 'binary_none'):
        variants = [('valid', payload(codec), 'OK'), ('trailing', payload(codec) + b'!', 'LOAD_ERROR'),
                    ('truncated', payload(codec)[:-1], 'LOAD_ERROR')]
        if codec == 'xml':
            variants += [('capability', payload(codec, 'mc:MustUnderstand="k"'), 'OK'),
                         ('unknown-capability', payload(codec, 'mc:MustUnderstand="mc"'), 'LOAD_ERROR'),
                         ('unbound-capability', payload(codec, 'mc:MustUnderstand="missing"'), 'LOAD_ERROR'),
                         ('unknown-attribute', payload(codec, 'surprise="1"'), 'LOAD_ERROR'),
                         ('missing-type', payload(codec).replace(b' xsi:type="t:payload"', b''), 'LOAD_ERROR'),
                         ('wrong-type', payload(codec).replace(b'xsi:type="t:payload"', b'xsi:type="t:other"'), 'LOAD_ERROR')]
        for suffix, data, decision in variants:
            name = codec + '-' + suffix
            if binary_only and codec == 'json':
                decision = 'LOAD_ERROR'
            author(directory / (name + '.opc'), data, fixed_codec='xml' if binary_only else 'json')
            result.append({'name': name, 'codec': codec, 'decision': decision})
    author(directory / 'fixed-storage.opc', payload('xml'), zipfile.ZIP_DEFLATED, fixed_codec='xml' if binary_only else 'json')
    result.append({'name': 'fixed-storage', 'codec': 'xml', 'decision': 'LOAD_ERROR'})
    (directory / 'cases.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def inspect(path, codec, binary_only=False):
    """Validate physical methods, independent model values, mandatory XML type and exact inert URI output."""
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert archive.getinfo('fixed.data').compress_type == zipfile.ZIP_STORED
        data = archive.read('main.data')
        if codec == 'xml':
            root = ET.fromstring(data)
            assert root.tag == '{' + NAMESPACE + '}payload'
            assert root.attrib['{http://www.w3.org/2001/XMLSchema-instance}type'].split(':')[-1] == 'payload'
            assert int(root.attrib['id']) == MODEL['id'] and root.attrib['label'] == MODEL['label']
            assert [int(value.text) for value in root] == MODEL['values']
        elif codec == 'json':
            assert json.loads(data) == MODEL
        else:
            assert data == payload('binary_none')
        if binary_only:
            fixed = ET.fromstring(archive.read('fixed.data'))
            assert fixed.tag == '{' + NAMESPACE + '}payload'
            assert int(fixed.attrib['id']) == MODEL['id'] and fixed.attrib['label'] == MODEL['label']
            assert [int(value.text) for value in fixed] == MODEL['values']
        else:
            assert json.loads(archive.read('fixed.data')) == MODEL
        relationships = ET.fromstring(archive.read('_rels/.rels'))
        external = [link for link in relationships if link.attrib['Type'] == 'urn:serializer:pipeline:link']
        assert len(external) == 1 and external[0].attrib['TargetMode'] == 'External' and external[0].attrib['Target'] == EXTERNAL
        assert len(archive.namelist()) == 4


def main():
    """Run real generated SDK consumers, then inspect every accepted archive independently."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', choices=('cpp', 'c', 'python', 'javascript', 'typescript', 'java', 'csharp', 'go', 'kotlin', 'swift', 'rust'), required=True)
    parser.add_argument('--cc')
    parser.add_argument('--provider-prefix', type=Path)
    parser.add_argument('--cargo', default='cargo')
    parser.add_argument('--consumer', type=Path)
    parser.add_argument('--kotlinc', default='kotlinc')
    parser.add_argument('--swiftc', default='swiftc')
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--typescript-cli', type=Path)
    parser.add_argument('--java', default='java')
    parser.add_argument('--javac', default='javac')
    parser.add_argument('--dotnet', default='dotnet')
    parser.add_argument('--go', default='go')
    parser.add_argument('--binary-only', action='store_true', help='Qualify positional-only generation and rejection of JSON selections')
    parser.add_argument('--schema-local', action='store_true', help='Use authored role profiles without INI overrides')
    args = parser.parse_args()
    compiler_sha256 = hashlib.sha256(args.compiler.read_bytes()).hexdigest()
    build = args.build.resolve(); build.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    command_log = []
    def run(command, stage, expect_failure=False):
        """Retain exact tool invocations and complete output before reporting a stage failure."""
        command = list(map(str, command))
        completed = subprocess.run(command, cwd=build, capture_output=True, text=True, encoding='utf-8', env=environment)
        command_log.append({'stage': stage, 'argv': command, 'exit_code': completed.returncode, 'stdout': completed.stdout, 'stderr': completed.stderr})
        (build / 'commands.json').write_text(json.dumps(command_log, indent=2))
        if bool(completed.returncode) != expect_failure:
            raise RuntimeError(stage + ': ' + completed.stdout + completed.stderr)
        return completed.stdout
    profile_arguments = [] if args.schema_local else ['--config', ROOT / 'test/resources/package_pipeline.ini']
    base = [args.compiler.resolve(), '--input', ROOT / 'test/resources/package_pipeline.serializer', *profile_arguments]
    if args.binary_only:
        # Reject a statically selected disabled native envelope before publishing generated output.
        language = 'js' if args.language in ('javascript', 'typescript') else args.language
        rejected = build / 'rejected-output'
        run(base + ['--protocols', 'binary_none', '--language', language, '--output', rejected], 'static-disabled-json', expect_failure=True)
        assert not rejected.exists()
        model = build / 'binary-only.serializer'
        model.write_text((ROOT / 'test/resources/package_pipeline.serializer').read_text().replace('codec(json);', 'codec(xml);'))
        base = [args.compiler.resolve(), '--input', model, *profile_arguments, '--protocols', 'binary_none']
    if args.language == 'cpp':
        if not args.consumer:
            parser.error('C++ requires --consumer')
        launch = [args.consumer.resolve(strict=True)]
    elif args.language == 'c':
        if not args.cc:
            parser.error('C requires an actual C11 compiler in --cc')
        source = build / 'schema.h'
        run(base + ['--language', 'c', '--output', source], 'generation')
        text = source.read_text(encoding='utf-8')
        helper = re.search(r'/\* Generated C package prefix: (\w+) \*/', text).group(1)
        selected = re.search(r'/\* Generated C selected-codec prefix: (\w+) \*/', text).group(1)
        (build / 'main.c').write_text((ROOT / 'test/resources/package_pipeline_main.c').read_text(encoding='utf-8').replace('@p@', helper).replace('@selected@', selected), encoding='utf-8')
        executable = build / ('consumer.exe' if os.name == 'nt' else 'consumer')
        if Path(args.cc).name.lower() in ('cl', 'cl.exe'):
            command = [args.cc, '/nologo', '/std:c11', '/W4', '/WX', '/O2', '/utf-8', '/D_CRT_SECURE_NO_WARNINGS',
                       '/DSERIALIZER_COMPRESSION_ZLIB=1', build / 'main.c', '/Fe' + str(executable), '/Fo' + str(build / 'main.obj')]
            if args.provider_prefix:
                prefix = args.provider_prefix.resolve()
                command += ['/I' + str(prefix / 'include'), '/I' + str(prefix / 'include/libxml2'), '/link', '/LIBPATH:' + str(prefix / 'lib')]
                environment['PATH'] = str(prefix / 'bin') + os.pathsep + environment.get('PATH', '')
            command += ['libxml2.lib', 'z.lib']
        else:
            command = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2', '-DSERIALIZER_COMPRESSION_ZLIB=1', build / 'main.c', '-o', executable]
            command += subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libxml-2.0'], text=True).split() + ['-lz', '-lm']
        run(command, 'compile')
        launch = [executable]
    elif args.language == 'rust':
        source=build / 'schema.rs'
        run(base + ['--language', 'rust', '--rust.output', source], 'generation')
        helper=re.search(r'// Generated Rust package helper module: (\w+)',source.read_text()).group(1)
        (build / 'main.rs').write_text((ROOT / 'test/resources/package_pipeline_main.rs').read_text().replace('@p@',helper))
        for filename in ('Cargo.toml','Cargo.lock'):
            shutil.copyfile(ROOT / 'example/xml/rust' / filename,build / filename)
        run([args.cargo,'build','--locked','--offline','--release','--manifest-path',build / 'Cargo.toml'],'compile')
        launch=[build / 'target/release' / ('serializer-xml-example.exe' if os.name == 'nt' else 'serializer-xml-example')]
    elif args.language == 'kotlin':
        run(base + ['--language', 'kotlin', '--output', build / 'Schema.kt', '--kotlin.package', 'generated'], 'generation')
        (build / 'Main.kt').write_text((ROOT / 'test/resources/package_pipeline_main.kt').read_text())
        kotlinc=Path(args.kotlinc).resolve() if Path(args.kotlinc).exists() else args.kotlinc
        command = [kotlinc]
        if isinstance(kotlinc, Path):
            home = kotlinc.parent.parent
            # Invoke an explicitly selected SDK directly, avoiding batch launcher's path assumptions.
            if (home / 'lib/kotlin-compiler.jar').is_file():
                command = [args.java, '-Xmx2g', '-cp', home / 'lib/kotlin-compiler.jar',
                           'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-kotlin-home', home, '-jvm-target', '17']
        run(command + ['-Werror', build / 'Schema.kt', build / 'Main.kt', '-include-runtime', '-d', build / 'consumer.jar'], 'compile')
        launch = [args.java, '-jar', build / 'consumer.jar']
    elif args.language == 'swift':
        run(base + ['--language', 'swift', '--output', build / 'Schema.swift'], 'generation')
        (build / 'main.swift').write_text((ROOT / 'test/resources/package_pipeline_main.swift').read_text())
        executable=build / ('consumer.exe' if os.name == 'nt' else 'consumer')
        run([args.swiftc, '-warnings-as-errors', build / 'Schema.swift', build / 'main.swift', '-o', executable], 'compile')
        launch = [executable]
    elif args.language == 'python':
        run(base + ['--language', 'python', '--python.output', build / 'schema.py'], 'generation')
        (build / 'main.py').write_text((ROOT / 'test/resources/package_pipeline_main.py').read_text())
        launch = [sys.executable, '-B', build / 'main.py']
    elif args.language == 'go':
        source = build / 'model/schema.go'
        source.parent.mkdir(exist_ok=True)
        run(base + ['--language', 'go', '--go.output', source, '--go.package', 'model'], 'generation')
        helper = re.search(r'// Generated package helper prefix: (\w+)', source.read_text(encoding='utf-8')).group(1)
        (build / 'main.go').write_text((ROOT / 'test/resources/package_pipeline_main.go').read_text(encoding='utf-8').replace('@p@', helper), encoding='utf-8')
        (build / 'go.mod').write_text('module serializer-pipeline\n\ngo 1.23\n', encoding='utf-8')
        environment.update(GOWORK='off', GOFLAGS='-mod=readonly', GOCACHE=str(build / 'gocache'))
        executable = build / ('consumer.exe' if os.name == 'nt' else 'consumer')
        run([args.go, 'build', '-o', executable, '.'], 'compile')
        launch = [executable]
    elif args.language == 'csharp':
        import csharp_compression_test as csharp
        versions = re.findall(r'^(8\.0\.\d+) ', run([args.dotnet, '--list-sdks'], 'versions'), re.M)
        if not versions:
            parser.error('Provide the actual .NET8 SDK/runtime')
        sdk = max(versions, key=lambda value: tuple(map(int, value.split('.'))))
        csharp.project(build, sdk)
        environment.update(DOTNET_CLI_HOME=str(build / 'dotnet-home'), DOTNET_CLI_TELEMETRY_OPTOUT='1',
                           NUGET_PACKAGES=str(build / 'nuget'), DOTNET_CLI_USE_MSBUILD_SERVER='0', MSBUILDDISABLENODEREUSE='1')
        source = build / 'Schema.cs'
        run(base + ['--language', 'csharp', '--output', source, '--csharp.namespace', 'SerializerGenerated'], 'generation')
        (build / 'Program.cs').write_text((ROOT / 'test/resources/package_pipeline_main.cs').read_text(encoding='utf-8'), encoding='utf-8')
        run([args.dotnet, 'build', build / 'Compression.csproj', '--configuration', 'Release', '--nologo',
             '--disable-build-servers', '-nodeReuse:false'], 'compile')
        launch = [args.dotnet, build / 'bin/Release/net8.0/Compression.dll']
    elif args.language == 'java':
        source = build / 'Generated.java'
        run(base + ['--language', 'java', '--output', source, '--java.package', 'generated'], 'generation')
        consumer = build / 'PipelineMain.java'
        consumer.write_text((ROOT / 'test/resources/package_pipeline_main.java').read_text(encoding='utf-8'), encoding='utf-8')
        classes = build / 'classes'
        classes.mkdir(exist_ok=True)
        run([args.javac, '-Xlint:all', '-Werror', '-d', classes, source, consumer], 'compile')
        launch = [args.java, '-cp', classes, 'consumer.PipelineMain']
    else:
        from package_node_dependencies import prepare_node_dependencies
        prepare_node_dependencies(build, args.node_dependencies, run)
        command = base + ['--language', 'js,typescript' if args.language == 'typescript' else 'js', '--js.output', build / 'schema.mjs']
        if args.language == 'typescript':
            command += ['--typescript.output', build / 'schema.d.mts']
        run(command, 'generation')
        if args.language == 'typescript':
            if not args.typescript_cli:
                parser.error('TypeScript requires --typescript-cli')
            (build / 'api.mts').write_text((ROOT / 'test/resources/package_pipeline_api.mts').read_text())
            run(['node', args.typescript_cli.resolve(), '--target', 'ES2022', '--module', 'NodeNext', '--moduleResolution', 'NodeNext', '--strict', '--noEmitOnError', '--outDir', build, build / 'api.mts'], 'strict-api-compile')
        (build / 'main.mjs').write_text((ROOT / 'test/resources/package_pipeline_main.mjs').read_text())
        launch = ['node', build / 'main.mjs']
    fixtures = cases(build, args.binary_only)
    outputs = []
    for case in fixtures:
        result = json.loads(run(launch + [build, case['name'], case['codec'], case['decision']], case['name']).splitlines()[-1])
        assert result['decision'] == case['decision']
        if case['decision'] == 'OK':
            for suffix in ('encoded', 'saved'):
                output = build / (case['name'] + '-' + suffix + '.opc'); inspect(output, case['codec'], args.binary_only); outputs.append(output.name)
    report = {'language': args.language, 'binary_only': args.binary_only, 'inputs': len(fixtures), 'outputs': outputs, 'compiler_sha256': compiler_sha256}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
