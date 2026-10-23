# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Check located scalar-default diagnostics, atomic publication and actual generated Python defaults."""
import argparse
import importlib.util
import math
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def invoke(compiler, source, cpp, python=None):
    """Run the real compiler with preserved native spelling and capture diagnostics for review."""
    command = [str(compiler), '--input', str(source), '--language', 'cpp,python' if python else 'cpp',
               '--cpp.output', str(cpp), '--cpp.format=false', '--cpp.naming=preserve']
    if python:
        command += ['--python.output', str(python)]
    return subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=60)


def execute(compiler, directory):
    """Exercise complete compiler invocations without retaining any scratch SDKs or fixture sources."""
    source, cpp, python = (directory / name for name in ('model.serializer', 'model.hpp', 'model.py'))
    sentinel = b'previous generated artifact\n'
    failures = [('float', '1e39'), ('float', '-3.4028236e38'), ('double', '1e400'),
                ('double', '9e999999999999999999999999999'), ('kind', 'kind::missing'),
                ('kind', '::kind::missing'), ('uint64', '9' * 65536),
                ('uint64', '0x' + 'F' * 65536), ('int64', '-' + '9' * 65536)]
    for kind, literal in failures:
        source.write_text('serializer version 1.14.0;\nenum kind { first, second }\n'
                          'class sample {\n  public '+kind+' value { '+literal+' };\n}\n', encoding='utf-8')
        cpp.write_bytes(sentinel)
        python.write_bytes(sentinel)
        result = invoke(compiler, source, cpp, python)
        diagnostic = result.stdout + result.stderr
        if result.returncode == 0 or 'model.serializer:4:3: member value:' not in diagnostic:
            raise AssertionError('Missing declaring-member diagnostic: '+diagnostic)
        expected = ('Unknown enumerator in default' if kind == 'kind' else
                    ('Numeric' if kind.startswith(('int', 'uint')) else 'Floating') +
                    ' default exceeds the declared ' + kind + ' range')
        if expected not in diagnostic:
            raise AssertionError('Missing precise typed-default diagnostic: '+diagnostic)
        if cpp.read_bytes() != sentinel or python.read_bytes() != sentinel:
            raise AssertionError('Rejected scalar default replaced generated output')
    controls = [('1.14.0', 'float', '1e-100'), ('1.14.0', 'double', '1e-1000'),
                ('1.14.0', 'float', '1.0F'), ('1.14.0', 'double', '0x1p1023'),
                ('1.14.0', 'float', 'native_float()'), ('1.14.0', 'kind', 'other::value'),
                ('1.14.0', 'bool', '42'), ('1.13.0', 'float', '1e39'),
                ('1.13.0', 'kind', 'kind::missing')]
    for version, kind, literal in controls:
        source.write_text('serializer version '+version+'; enum kind { first, second } '
                          'class sample { public '+kind+' value { '+literal+' }; }', encoding='utf-8')
        result = invoke(compiler, source, cpp)
        if result.returncode != 0 or literal not in cpp.read_text(encoding='utf-8'):
            raise AssertionError('Native or legacy default changed: '+result.stdout+result.stderr)
    source.write_text('''serializer version 1.14.0; namespace defaults {
      enum kind { first, second }
      class sample { public float single {0.5}; public double extended {1.7976931348623157e308};
        public float negative_zero {-0.0}; public kind selection {kind::second}; }
    }''', encoding='utf-8')
    result = invoke(compiler, source, cpp, python)
    if result.returncode != 0:
        raise AssertionError('Valid portable defaults failed: '+result.stdout+result.stderr)
    spec = importlib.util.spec_from_file_location('serializer_portable_default_fixture', python)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    try:
        value = module.DefaultsSample()
        if value.single != 0.5 or value.extended != sys.float_info.max or int(value.selection) != 1:
            raise AssertionError('Generated default values changed')
        for protocol in module.Protocol:
            decoded = module.DefaultsSample.decode(value.encode(protocol), protocol)
            if decoded.single != value.single or decoded.extended != value.extended or decoded.selection != value.selection or math.copysign(1.0, decoded.negative_zero) != -1.0:
                raise AssertionError('Native wire round trip changed checked default values')
    finally:
        del sys.modules[spec.name]
    # Large defaults exercise native literal classification and actual Python execution.
    large_text = 'a' * 32768
    source.write_text('serializer version 1.14.0; class sample { public string text { "' +
                      large_text + '" }; public double number { 1.' + '0' * 32768 + ' }; }', encoding='utf-8')
    result = invoke(compiler, source, cpp, python)
    if result.returncode != 0:
        raise AssertionError('Large portable defaults failed: '+result.stdout+result.stderr)
    spec = importlib.util.spec_from_file_location('serializer_large_default_fixture', python)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    try:
        value = module.Sample()
        for protocol in module.Protocol:
            decoded = module.Sample.decode(value.encode(protocol), protocol)
            if decoded.text != large_text or decoded.number != 1.0:
                raise AssertionError('Large native default changed')
    finally:
        del sys.modules[spec.name]
    source.write_text('serializer version 1.14.0; class sample { public string text { "' +
                      large_text + '\\q" }; }', encoding='utf-8')
    cpp.write_bytes(sentinel)
    python.write_bytes(sentinel)
    result = invoke(compiler, source, cpp, python)
    if result.returncode == 0 or 'Expected a portable literal default: text' not in result.stdout + result.stderr:
        raise AssertionError('Invalid large portable escape was not rejected cleanly')
    if cpp.read_bytes() != sentinel or python.read_bytes() != sentinel:
        raise AssertionError('Rejected large portable escape replaced generated output')
    print(f'PASS: {len(failures)} atomic scalar rejections, one large-string backend rejection, '
          f'{len(controls)} native/legacy controls and generated Python defaults across four protocols', flush=True)


def main():
    """Keep every generated artifact inside a disposable directory under the ignored build root."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='portable-defaults-', dir=build) as temporary:
        execute(compiler, Path(temporary))


if __name__ == '__main__':
    main()
