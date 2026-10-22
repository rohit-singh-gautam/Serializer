# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compile authored names which overlap new private wildcard helper families without changing XML identities."""
import argparse
import json
from pathlib import Path
import subprocess
import hashlib

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('SrlXmlDeclaration', 'SrlXmlDeclarations', 'SrlXmlDeclarationPolicy',
         'SrlXmlDeclarationType', 'SrlXmlDeclarationRead', 'SrlXmlDeclarationInventories',
         'SrlXmlWildcardValidation')


def main():
    """Retain real generation/compiler commands and verify unchanged native names and XML values."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--dotnet', default='dotnet')
    args = parser.parse_args()
    directory = args.build.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    schema = directory / 'names.serializer'
    schema.write_text('serializer version 1.21.0;\n' + '\n'.join(
        'class ' + name + ' { public string value; }' for name in NAMES) + '\n', encoding='utf-8')
    checks = '\n'.join('''{
var value=new NAME{Value="SrlXmlDeclarationPolicy"};
byte[] xml=value.EncodeXml();var root=System.Xml.Linq.XElement.Parse(System.Text.Encoding.UTF8.GetString(xml));
if(root.Name.ToString()!="NAME"||root.Element("value")?.Value!="SrlXmlDeclarationPolicy")throw new Exception("Authored XML identity changed");
var copy=NAME.DecodeXml(xml);foreach(var protocol in Enum.GetValues<Protocol>())if(!value.Encode(protocol).AsSpan().SequenceEqual(copy.Encode(protocol)))throw new Exception("Native identity changed");
}'''.replace('NAME', name) for name in NAMES)
    (directory / 'Program.cs').write_text('using System;using static SerializerGenerated.Schema;\ninternal static class Program{private static void Main(){' + checks + '\nConsole.WriteLine("PASS: 7 authored helper-name collisions, XML identities and four native protocols");}}\n', encoding='utf-8')
    (directory / 'Example.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><Nullable>enable</Nullable><TreatWarningsAsErrors>true</TreatWarningsAsErrors></PropertyGroup></Project>', encoding='utf-8')
    commands = []
    for command in ([str(args.compiler.resolve()), '--input', str(schema), '--xml.enabled', 'true', '--language', 'csharp', '--csharp.output', str(directory / 'Schema.cs')],
                    [args.dotnet, 'build', str(directory / 'Example.csproj'), '--configuration', 'Release', '--nologo', '-v:q'],
                    [args.dotnet, str(directory / 'bin/Release/net8.0/Example.dll')]):
        result = subprocess.run(command, cwd=directory, capture_output=True, text=True, encoding='utf-8')
        commands.append({'argv': command, 'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        (directory / 'commands.json').write_text(json.dumps(commands, indent=2), encoding='utf-8')
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
    (directory / 'verification.json').write_text(json.dumps({'authored_names': NAMES, 'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest()}, indent=2), encoding='utf-8')
    print(commands[-1]['stdout'].strip())


if __name__ == '__main__':
    main()
