# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Strictly build one C# or Swift consumer of an independently generated XSD variant."""
import hashlib
import json
import os
import re
import shutil
from pathlib import Path


def digest(path):
    """Fingerprint a generated, compiler or runtime artifact without modifying it."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def tool_path(command):
    """Resolve the executable actually selected by an explicit path or the current search path."""
    selected = shutil.which(command)
    if not selected:
        raise ValueError('Required managed SDK executable was not found: ' + command)
    return Path(selected).resolve(strict=True)


def linked_libraries(executable, run):
    """Read Linux dynamic dependencies to retain the Foundation/runtime implementation used by the consumer."""
    output = run(['ldd', str(executable)], 'runtime-dependencies')
    if re.search(r'=>\s+not found', output):
        raise ValueError('A selected Swift runtime dependency is unavailable')
    return [Path(path).resolve(strict=True) for path in re.findall(r'=>\s+(/\S+)', output)]


def build(language, root, compiler, directory, options, run):
    """Generate one schema variant, compile with warnings as errors, and pin its real SDK/XML dependencies."""
    if language not in ('csharp', 'swift'):
        raise ValueError('Unsupported managed XSD evolution consumer')
    variant = options.get('variant')
    if variant not in ('v1', 'v2'):
        raise ValueError('An explicit v1 or v2 schema variant is required')
    root, compiler, directory = Path(root).resolve(), Path(compiler).resolve(), Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    resources = root / 'qualification/xsd_evolution'
    schema = Path(options.get('schema_directory') or resources) / variant / 'order.serializer'
    command = [str(compiler), '--input', str(schema), '--language', language, '--output']
    artifacts = []
    if language == 'csharp':
        generated = directory / 'Schema.cs'
        run(command + [str(generated), '--csharp.namespace', 'SerializerGenerated'], 'generation')
        (directory / 'Program.cs').write_bytes((resources / 'main.cs').read_bytes())
        dotnet = str(options.get('dotnet') or 'dotnet')
        artifacts.append(tool_path(dotnet))
        sdk_version = '8.0.426'
        (directory / 'global.json').write_text(json.dumps({
            'sdk': {'version': sdk_version, 'rollForward': 'disable'}
        }) + '\n', encoding='utf-8')
        environment = dict(os.environ)
        environment.update({'DOTNET_CLI_TELEMETRY_OPTOUT': '1', 'DOTNET_NOLOGO': '1',
                            'MSBUILDDISABLENODEREUSE': '1', 'DOTNET_CLI_DO_NOT_USE_MSBUILD_SERVER': '1',
                            'DOTNET_CLI_HOME': str(directory / 'dotnet-home')})
        selected = run([dotnet, '--version'], 'dotnet-version', cwd=str(directory), env=environment).strip()
        if selected != sdk_version:
            raise ValueError('The pinned .NET8 SDK was not selected')
        (directory / 'Consumer.csproj').write_text(
            '<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup>'
            '<OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><LangVersion>12.0</LangVersion>'
            '<AssemblyName>xsd_evolution_consumer</AssemblyName><Nullable>enable</Nullable>'
            '<ImplicitUsings>disable</ImplicitUsings><TreatWarningsAsErrors>true</TreatWarningsAsErrors>'
            '<EnableDefaultCompileItems>false</EnableDefaultCompileItems>'
            '<RestoreIgnoreFailedSources>true</RestoreIgnoreFailedSources>'
            '</PropertyGroup><ItemGroup><Compile Include="Schema.cs"/><Compile Include="Program.cs"/>'
            '</ItemGroup></Project>\n', encoding='utf-8')
        run([dotnet, 'build', str(directory / 'Consumer.csproj'), '--nologo', '--configuration', 'Release',
             '--output', str(directory / 'app'), '--disable-build-servers', '-nodeReuse:false'],
            'compile', cwd=str(directory), env=environment)
        executable = directory / 'app/xsd_evolution_consumer.dll'
        launch = [dotnet, str(executable)]
        dependencies = json.loads(run(launch + ['--dependencies'], 'xml-runtime-dependencies'))
        artifacts.extend(Path(path).resolve(strict=True) for path in dependencies['artifacts'])
        installed = run([dotnet, '--list-sdks'], 'dotnet-sdk-paths', env=environment)
        matches = re.findall(r'^' + re.escape(sdk_version) + r'\s+\[([^\]]+)\]', installed, re.MULTILINE)
        if len(matches) != 1:
            raise ValueError('The selected .NET8 SDK implementation could not be pinned')
        artifacts.append(Path(matches[0]) / sdk_version / 'Roslyn/bincore/Microsoft.CodeAnalysis.CSharp.dll')
        tool = '.NET SDK ' + selected + '; ' + dependencies['framework']
    else:
        generated = directory / 'Schema.swift'
        run(command + [str(generated)], 'generation')
        (directory / 'main.swift').write_bytes((resources / 'main.swift').read_bytes())
        swiftc = str(options.get('swiftc') or 'swiftc')
        artifacts.append(tool_path(swiftc))
        tool = run([swiftc, '--version'], 'swift-version').strip()
        target = json.loads(run([swiftc, '-print-target-info'], 'swift-target-info'))
        executable = directory / ('consumer.exe' if os.name == 'nt' else 'consumer')
        run([swiftc, '-O', '-warnings-as-errors', str(generated), str(directory / 'main.swift'),
             '-o', str(executable)], 'compile')
        launch = [str(executable)]
        if os.name == 'nt':
            # The native XML scanner is generated source; Foundation supplies file I/O, not an XML provider.
            runtime_names = ('swiftCore.dll', 'Foundation.dll', 'FoundationEssentials.dll')
            for name in runtime_names:
                for location in target['paths']['runtimeLibraryPaths']:
                    candidate = Path(location) / name
                    if candidate.is_file():
                        artifacts.append(candidate.resolve(strict=True))
                        break
        else:
            artifacts.extend(linked_libraries(executable, run))
    artifacts.extend((generated, executable))
    return {'launch': launch, 'generated': [str(generated)], 'tool': tool,
            'artifacts': {str(path.resolve(strict=True)): digest(path) for path in artifacts}}
