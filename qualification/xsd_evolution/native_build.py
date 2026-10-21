# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Build independent C/Rust/Go consumers of one authored XSD evolution variant."""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil


def _artifact(artifacts, path):
    """Pin an existing actual compiler, provider, source or executable by resolved path."""
    candidate = Path(path).resolve()
    if candidate.is_file():
        artifacts[str(candidate)] = hashlib.sha256(candidate.read_bytes()).hexdigest()


def _tool(value):
    """Resolve a supplied installed SDK command without shell evaluation or downloads."""
    path = shutil.which(str(value))
    if path is None:
        candidate = Path(value)
        if not candidate.is_file():
            raise RuntimeError('Installed SDK is unavailable: ' + str(value))
        path = candidate
    return str(Path(path).resolve())


def build(language, root, compiler, directory, options, run):
    """Generate and compile one variant through real SDKs, retaining exact artifact pins.

    run owns all subprocess logging. Outputs and per-build caches stay below the
    repository's ignored out directory. No global SDK configuration is modified.
    """
    root, compiler, directory = Path(root).resolve(), Path(compiler).resolve(), Path(directory).resolve()
    if root not in directory.parents or 'out' not in directory.relative_to(root).parts:
        raise ValueError('Qualification output must stay beneath repository out/')
    variant = options['variant']
    if variant not in ('v1', 'v2'):
        raise ValueError('Unsupported evolution variant: ' + str(variant))
    source_root = root / 'qualification/xsd_evolution'
    schema = source_root / variant / 'order.serializer'
    directory.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    artifacts, generated = {}, []
    for path in (compiler, schema, schema.with_suffix('.xsd')):
        _artifact(artifacts, path)

    def invoke(arguments, label, *, cwd=None, env=None):
        """Send every generator/SDK action through the orchestrator's captured command runner."""
        return run([str(item) for item in arguments], language + '-' + variant + '-' + label,
                   cwd=cwd, env=env)

    def consumer(suffix):
        """Select the compiled variant without rewriting generated identifiers or public API calls."""
        source = source_root / ('main.' + suffix)
        destination = directory / source.name
        destination.write_text(source.read_text(encoding='utf-8').replace('@variant@', variant),
                               encoding='utf-8', newline='\n')
        generated.append(str(destination))
        _artifact(artifacts, source)
        return destination

    if language == 'c':
        output = directory / 'schema.h'
        invoke([compiler, '--input', schema, '--language', 'c', '--output', output], 'generation')
        main = consumer('c')
        cc = _tool(options.get('cc') or ('cl' if os.name == 'nt' else 'cc'))
        _artifact(artifacts, cc)
        executable = directory / ('consumer.exe' if os.name == 'nt' else 'consumer')
        if os.name == 'nt':
            if not options.get('provider_prefix'):
                raise ValueError('Windows C requires provider_prefix for libxml2')
            prefix = Path(options['provider_prefix']).resolve()
            environment['PATH'] = str(prefix / 'bin') + os.pathsep + environment.get('PATH', '')
            details = invoke([cc, '/nologo', '/Bv', '/std:c11', '/W4', '/WX', '/O2', '/utf-8',
                              '/D_CRT_SECURE_NO_WARNINGS', main, '/I' + str(prefix / 'include/libxml2'),
                              '/Fe' + str(executable), '/Fo' + str(directory / 'main.obj'), '/link',
                              '/LIBPATH:' + str(prefix / 'lib'), 'libxml2.lib'], 'compile', env=environment)
            tool = next((line for line in details.splitlines() if 'Compiler Version' in line), cc)
            for name in ('c1.dll', 'link.exe'):
                _artifact(artifacts, Path(cc).parent / name)
            for path in (prefix / 'bin').glob('*.dll'):
                _artifact(artifacts, path)
            for path in (prefix / 'lib/libxml2.lib', prefix / 'include/libxml2/libxml/xmlversion.h'):
                _artifact(artifacts, path)
        else:
            flags = invoke(['pkg-config', '--cflags', '--libs', 'libxml-2.0'], 'xml-provider-options').split()
            tool = invoke([cc, '--version'], 'sdk').splitlines()[0]
            invoke([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2', main,
                    '-o', executable, *flags, '-lm'], 'compile')
            linked = invoke(['ldd', executable], 'linked-providers')
            if 'not found' in linked:
                raise RuntimeError('C XML provider has an unresolved shared dependency')
            for path in re.findall(r'(/[^\s()]+)\s+\(', linked):
                _artifact(artifacts, path)
            for component in ('cc1', 'collect2', 'ld'):
                path = invoke([cc, '-print-prog-name=' + component], 'sdk-' + component).strip()
                _artifact(artifacts, path if Path(path).is_file() else _tool(path))
        provider = {'implementation': 'libxml2', 'artifacts': artifacts}
    elif language == 'rust':
        output = directory / 'schema.rs'
        invoke([compiler, '--input', schema, '--language', 'rust', '--rust.output', output], 'generation')
        consumer('rs')
        for name in ('Cargo.toml', 'Cargo.lock'):
            destination = directory / name
            shutil.copyfile(root / 'example/xml/rust' / name, destination)
            generated.append(str(destination))
            _artifact(artifacts, destination)
        cargo, rustc = _tool(options.get('cargo') or 'cargo'), _tool('rustc')
        _artifact(artifacts, cargo)
        _artifact(artifacts, rustc)
        environment['RUSTFLAGS'] = '-D warnings'
        warm_cache = root / 'out/build/pending-native-operation-2026-10-21/cargo-home'
        cache = options.get('cargo_home') or environment.get('CARGO_HOME')
        if cache or warm_cache.is_dir():
            environment['CARGO_HOME'] = str(Path(cache).resolve() if cache else warm_cache)
        tool = invoke([cargo, '--version'], 'sdk', env=environment).strip()
        invoke([rustc, '-vV'], 'rustc-sdk', env=environment)
        sysroot = Path(invoke([rustc, '--print', 'sysroot'], 'rustc-sysroot', env=environment).strip())
        for parent, pattern in (('bin', '*rustc_driver*.dll'), ('bin', '*LLVM*.dll'),
                                ('lib', 'librustc_driver*.so')):
            for path in (sysroot / parent).glob(pattern):
                _artifact(artifacts, path)
        invoke([cargo, 'build', '--locked', '--offline', '--release', '--manifest-path', directory / 'Cargo.toml'],
               'compile', env=environment)
        executable = directory / 'target/release' / ('serializer-xml-example.exe' if os.name == 'nt' else 'serializer-xml-example')
        provider = {'implementation': 'roxmltree/xmlparser', 'version': '0.21.1/0.13.6', 'artifacts': artifacts}
    elif language == 'go':
        output = directory / 'model/schema.go'
        output.parent.mkdir(exist_ok=True)
        invoke([compiler, '--input', schema, '--language', 'go', '--go.output', output,
                '--go.package', 'model'], 'generation')
        consumer('go')
        module = directory / 'go.mod'
        module.write_text('module serializer-xsd-evolution\n\ngo 1.22\n', encoding='utf-8', newline='\n')
        generated.append(str(module))
        go = _tool(options.get('go') or 'go')
        _artifact(artifacts, go)
        environment.update({'GOTOOLCHAIN': 'local', 'GOPROXY': 'off', 'GOSUMDB': 'off',
                            'GOCACHE': str(directory / 'go-cache'), 'GOMODCACHE': str(directory / 'go-module-cache')})
        tool = invoke([go, 'version'], 'sdk', env=environment).strip()
        sdk = json.loads(invoke([go, 'env', '-json', 'GOROOT', 'GOTOOLDIR'], 'sdk-options', env=environment))
        for name in ('compile', 'link'):
            _artifact(artifacts, Path(sdk['GOTOOLDIR']) / (name + ('.exe' if os.name == 'nt' else '')))
        for path in (Path(sdk['GOROOT']) / 'src/encoding/xml').glob('*.go'):
            _artifact(artifacts, path)
        executable = directory / ('consumer.exe' if os.name == 'nt' else 'consumer')
        invoke([go, 'build', '-trimpath', '-buildvcs=false', '-o', executable, '.'], 'compile',
               cwd=directory, env=environment)
        invoke([go, 'version', '-m', executable], 'binary-sdk', env=environment)
        provider = {'implementation': 'Go encoding/xml', 'version': tool, 'artifacts': artifacts}
    else:
        raise ValueError('Unsupported native language: ' + language)

    generated.append(str(output))
    _artifact(artifacts, executable)
    result = {'launch': [str(executable)], 'generated': generated, 'tool': tool,
              'provider': provider, 'artifacts': artifacts}
    if language == 'c' and os.name == 'nt':
        result['environment'] = {'PATH': environment['PATH']}
    return result
