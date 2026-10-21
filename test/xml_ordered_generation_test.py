# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Require complete preflight rejection of unsupported ordered XML output requests."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    """Exercise the actual CLI without installing or claiming execution of other native SDKs."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    resources = Path(__file__).resolve().parent / 'resources'
    compiler = str(args.compiler.resolve())
    prefix = [compiler, '--input', str(resources / 'xml_ordered.serializer'),
              '--config', str(resources / 'xml_ordered.ini')]
    with tempfile.TemporaryDirectory(prefix='ordered-', dir=args.build) as temporary:
        directory = Path(temporary)
        for language, extension in [('python', '.py'), ('js', '.mjs'), ('typescript', '.d.mts'),
                                    ('java', '.java'), ('csharp', '.cs'), ('go', '.go'),
                                    ('kotlin', '.kt'), ('swift', '.swift'), ('c', '.h'), ('rust', '.rs')]:
            destination = directory / ('Model' + extension)
            destination.write_bytes(b'previous generated output\n')
            cpp = directory / 'control.hpp'
            cpp.write_bytes(b'previous C++ output\n')
            result = subprocess.run(prefix + ['--language', 'cpp,' + language,
                '--cpp.output', str(cpp), '--' + language + '.output', str(destination)],
                capture_output=True, text=True, encoding='utf-8')
            assert result.returncode != 0, (language, result.stdout, result.stderr)
            assert 'only for cpp' in result.stderr, (language, result.stdout, result.stderr)
            assert destination.read_bytes() == b'previous generated output\n', language
            assert cpp.read_bytes() == b'previous C++ output\n', language

        model = directory / 'invalid.serializer'
        model.write_text('serializer version 1.21.0; class child { public uint32 x; } '
            'class step { public variant(string=text,child=child) value; } '
            'class root { public array<step> items; }', encoding='utf-8')
        config = directory / 'invalid.ini'
        config.write_text('[xml]\nenabled=true\n[cpp]\nformat=false\n'
            '[xml.class.root]\nordered=items\n', encoding='utf-8')
        result = subprocess.run([compiler, '--input', str(model), '--config', str(config),
            '--language', 'cpp', '--output', str(cpp)], capture_output=True, text=True, encoding='utf-8')
        assert result.returncode != 0 and 'without text' in result.stderr, result.stderr
        assert cpp.read_bytes() == b'previous C++ output\n'

        xsd = directory / 'fixed.xsd'
        xsd.write_text('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">'
            '<xs:element name="root" type="xs:string"/></xs:schema>', encoding='utf-8')
        result = subprocess.run(prefix + ['--language', 'cpp', '--output', str(cpp), '--xml.xsd', str(xsd)],
            capture_output=True, text=True, encoding='utf-8')
        assert result.returncode != 0 and 'cannot override a fixed XSD' in result.stderr, result.stderr
        assert cpp.read_bytes() == b'previous C++ output\n'
        exported = directory / 'export.xsd'
        exported.write_bytes(b'previous schema output\n')
        result = subprocess.run(prefix + ['--language', 'cpp', '--output', str(cpp), '--xml.export-xsd', str(exported)],
            capture_output=True, text=True, encoding='utf-8')
        assert result.returncode != 0, result.stdout
        assert cpp.read_bytes() == b'previous C++ output\n'
        assert exported.read_bytes() == b'previous schema output\n'
    print('Thirteen atomic rejection cases pass: ten other backends, text storage, fixed XSD and schema export.')


if __name__ == '__main__':
    main()
