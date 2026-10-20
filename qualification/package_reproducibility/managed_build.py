# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Build the C# and Swift archive producers against real generated library bytes."""
import hashlib
import json
import os
import re
from pathlib import Path


def digest(path):
    """Fingerprint exact provider or generated artifacts without rewriting them."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def json_line(output):
    """Require a machine-readable provider record from the executed producer."""
    records = [json.loads(line) for line in output.splitlines() if line.startswith('{')]
    if len(records) != 1:
        raise ValueError('Producer did not return exactly one provider fingerprint')
    return records[0]


def build(language, root, compiler, directory, options, run):
    """Generate, strictly compile and identify one actual managed SDK producer for the shared runner."""
    if language not in ('csharp', 'swift'):
        raise ValueError('Unsupported managed reproducibility producer')
    root, compiler, directory = Path(root).resolve(), Path(compiler).resolve(), Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    schema = root / 'test/resources/package_relocation.serializer'
    resources = root / 'qualification/package_reproducibility'
    command = [str(compiler), '--input', str(schema), '--language', language, '--output']
    if language == 'csharp':
        generated = directory / 'Schema.cs'
        run(command + [str(generated), '--csharp.namespace', 'SerializerGenerated'], 'generation')
        (directory / 'Program.cs').write_bytes((resources / 'main.cs').read_bytes())
        dotnet = str(options.get('dotnet') or 'dotnet')
        sdk_version = '8.0.426'
        (directory / 'global.json').write_text(json.dumps({'sdk': {'version': sdk_version, 'rollForward': 'disable'}}) + '\n')
        sdk = run([dotnet, '--version'], 'dotnet-version', cwd=str(directory)).strip()
        if sdk != sdk_version:
            raise ValueError('The pinned .NET SDK was not selected')
        (directory / 'Producer.csproj').write_text(
            '<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup>'
            '<OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><LangVersion>12.0</LangVersion>'
            '<AssemblyName>package_reproducibility_producer</AssemblyName><Nullable>enable</Nullable>'
            '<ImplicitUsings>disable</ImplicitUsings><TreatWarningsAsErrors>true</TreatWarningsAsErrors>'
            '<EnableDefaultCompileItems>false</EnableDefaultCompileItems>'
            '<RestoreIgnoreFailedSources>true</RestoreIgnoreFailedSources>'
            '</PropertyGroup><ItemGroup><Compile Include="Schema.cs"/><Compile Include="Program.cs"/>'
            '</ItemGroup></Project>\n', encoding='utf-8')
        environment = dict(os.environ)
        environment.update({'DOTNET_CLI_TELEMETRY_OPTOUT': '1', 'DOTNET_NOLOGO': '1',
                            'MSBUILDDISABLENODEREUSE': '1', 'DOTNET_CLI_DO_NOT_USE_MSBUILD_SERVER': '1',
                            'DOTNET_CLI_HOME': str(directory / 'dotnet-home')})
        run([dotnet, 'build', str(directory / 'Producer.csproj'), '--nologo', '--configuration', 'Release',
             '--output', str(directory / 'app'), '--disable-build-servers', '-nodeReuse:false'],
            'compile', cwd=str(directory), env=environment)
        launch = [dotnet, str(directory / 'app/package_reproducibility_producer.dll')]
        provider = json_line(run(launch + ['--provider'], 'provider'))
        runtime = Path(provider['runtime_directory'])
        artifacts = [Path(provider['assembly_path'])]
        for name in ('System.IO.Compression.Native.dll', 'libSystem.IO.Compression.Native.so',
                     'libSystem.IO.Compression.Native.dylib'):
            candidate = runtime / name
            if candidate.is_file():
                artifacts.append(candidate)
                if candidate.suffix == '.so':
                    # The .NET shim may borrow system zlib; pin that implementation as well as the shim.
                    dependencies = run(['ldd', str(candidate)], 'native-compression-dependencies')
                    if re.search(r'\blibz[^\s]*\s+=>\s+not found', dependencies):
                        raise ValueError('The selected native compression dependency is unavailable')
                    for linked in re.findall(r'\blibz[^\s]*\s+=>\s+(/[^\s]+)', dependencies):
                        artifacts.append(Path(linked).resolve(strict=True))
        provider['artifacts'] = {str(path.resolve()): digest(path) for path in artifacts}
        tool = '.NET SDK ' + sdk + '; ' + provider['framework']
    else:
        generated = directory / 'Schema.swift'
        run(command + [str(generated)], 'generation')
        source_hash = digest(generated)
        (directory / 'main.swift').write_text((resources / 'main.swift').read_text(encoding='utf-8')
                                            .replace('@provider_version@', source_hash), encoding='utf-8')
        swiftc = str(options.get('swiftc') or 'swiftc')
        tool = run([swiftc, '--version'], 'swift-version').strip()
        executable = directory / ('producer.exe' if os.name == 'nt' else 'producer')
        run([swiftc, '-O', '-warnings-as-errors', str(generated), str(directory / 'main.swift'),
             '-o', str(executable)], 'compile')
        launch = [str(executable)]
        provider = json_line(run(launch + ['--provider'], 'provider'))
        provider['artifacts'] = {str(generated): source_hash}
        # The self-contained encoder's wire algorithm uses the same pinned source and fixed options.
        provider['cross_host_identity'] = 'serializer-swift-deflate:' + source_hash + ':' + hashlib.sha256(
            json.dumps(provider['options'], sort_keys=True).encode('utf-8')).hexdigest()
    return {'launch': launch, 'generated': [str(generated)], 'tool': tool, 'provider': provider}
