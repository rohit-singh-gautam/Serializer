# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Build actual C/Rust/Go reproducibility producers and pin their SDK/provider artifacts."""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil


def _sha(path):
    """Fingerprint one actual source, SDK component, provider library or executable."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _artifact(result, path):
    """Retain existing regular artifacts by resolved absolute path, including symlink targets."""
    candidate = Path(path).resolve()
    if candidate.is_file():
        result[str(candidate)] = _sha(candidate)


def _tool(value):
    """Resolve a configured SDK command without invoking a shell or downloading a toolchain."""
    path = shutil.which(str(value))
    if path is None:
        candidate = Path(value)
        if not candidate.is_file():
            raise RuntimeError('Installed SDK tool is unavailable: ' + str(value))
        path = str(candidate)
    return str(Path(path).resolve())


def build(language, root, compiler, directory, options, run):
    """Generate/build one real producer; return its launch command, exact sources and provider pin.

    The caller owns command logging through run(argv, label, cwd=None, env=None).
    All generated files remain in the caller's ignored build directory; returned
    launch commands borrow the process environment and never edit global SDK settings.
    """
    root, compiler, directory = Path(root).resolve(), Path(compiler).resolve(), Path(directory).resolve()
    if root not in directory.parents or 'out' not in directory.relative_to(root).parts:
        raise ValueError('Native qualification output must stay beneath repository out/')
    directory.mkdir(parents=True, exist_ok=True)
    source_root = root / 'qualification/package_reproducibility'
    schema = root / 'test/resources/package_relocation.serializer'
    environment = os.environ.copy()
    artifacts = {}
    _artifact(artifacts, compiler)
    generated = []

    def invoke(arguments, label, *, cwd=None, env=None):
        """Use the orchestrator's exact captured command path for every generator and SDK action."""
        return run([str(item) for item in arguments], language + '-' + label, cwd=cwd, env=env)

    def consumer(suffix, substitutions):
        """Copy a maintained consumer template with compiler-owned helper names only."""
        destination = directory / ('main.' + suffix)
        text = (source_root / ('main.' + suffix)).read_text(encoding='utf-8')
        for before, after in substitutions.items():
            text = text.replace(before, after)
        if re.search(r'@[a-z_]+@', text):
            raise AssertionError('Unexpanded native consumer placeholder')
        destination.write_text(text, encoding='utf-8', newline='\n')
        generated.append(str(destination))
        return destination

    if language == 'c':
        output = directory / 'schema.h'
        invoke([compiler, '--input', schema, '--language', 'c', '--output', output], 'generation')
        generated.append(str(output))
        helper = re.search(r'/\* Generated C package prefix: (\w+) \*/', output.read_text(encoding='utf-8')).group(1)
        main = consumer('c', {'@p@': helper})
        cc = _tool(options.get('cc') or ('cl' if os.name == 'nt' else 'cc'))
        executable = directory / ('producer.exe' if os.name == 'nt' else 'producer')
        prefix_value = options.get('provider_prefix')
        prefix = Path(prefix_value).resolve() if prefix_value else None
        _artifact(artifacts, cc)
        if os.name == 'nt':
            if prefix is None:
                raise ValueError('Windows C qualification requires the selected provider_prefix')
            environment['PATH'] = str(prefix / 'bin') + os.pathsep + environment.get('PATH', '')
            command = [cc, '/nologo', '/Bv', '/std:c11', '/W4', '/WX', '/O2', '/utf-8',
                       '/D_CRT_SECURE_NO_WARNINGS', '/DSERIALIZER_COMPRESSION_ZLIB=1', main,
                       '/I' + str(prefix / 'include'), '/I' + str(prefix / 'include/libxml2'),
                       '/Fe' + str(executable), '/Fo' + str(directory / 'main.obj'), '/link',
                       '/LIBPATH:' + str(prefix / 'lib'), 'libxml2.lib', 'z.lib']
            details = invoke(command, 'compile', env=environment)
            tool = next((line for line in details.splitlines() if 'Compiler Version' in line), cc)
            for name in ('c1.dll', 'link.exe'):
                _artifact(artifacts, Path(cc).parent / name)
            for pattern in ('*zlib*.dll', 'z.dll', 'zlib1.dll', '*xml2*.dll'):
                for path in (prefix / 'bin').glob(pattern):
                    _artifact(artifacts, path)
            for name in ('z.lib', 'libxml2.lib'):
                _artifact(artifacts, prefix / 'lib' / name)
            launch = [str(executable)]
        else:
            flags = invoke(['pkg-config', '--cflags', '--libs', 'libxml-2.0'], 'provider-options').split()
            tool = invoke([cc, '--version'], 'sdk').splitlines()[0]
            invoke([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                    '-DSERIALIZER_COMPRESSION_ZLIB=1', main, '-o', executable, *flags, '-lz', '-lm'], 'compile')
            linked = invoke(['ldd', executable], 'linked-providers')
            for path in re.findall(r'(/[^\s()]+)\s+\(', linked):
                _artifact(artifacts, path)
            for component in ('cc1', 'collect2', 'ld'):
                path = invoke([cc, '-print-prog-name=' + component], 'sdk-' + component).strip()
                if Path(path).is_file():
                    _artifact(artifacts, path)
                elif shutil.which(path):
                    _artifact(artifacts, shutil.which(path))
            launch = [str(executable)]
    elif language == 'rust':
        output = directory / 'schema.rs'
        invoke([compiler, '--input', schema, '--language', 'rust', '--rust.output', output], 'generation')
        generated.append(str(output))
        text = output.read_text(encoding='utf-8')
        helper = re.search(r'// Generated Rust package helper module: (\w+)', text).group(1)
        consumer('rs', {'@p@': helper, '@provider_sha256@': _sha(output)})
        for name in ('Cargo.toml', 'Cargo.lock'):
            destination = directory / name
            shutil.copyfile(root / 'example/xml/rust' / name, destination)
            generated.append(str(destination))
            _artifact(artifacts, destination)
        cargo = _tool(options.get('cargo') or 'cargo')
        rustc = _tool('rustc')
        _artifact(artifacts, cargo)
        _artifact(artifacts, rustc)
        environment['RUSTFLAGS'] = '-D warnings'
        cache = options.get('cargo_home') or environment.get('CARGO_HOME')
        warm_cache = root / 'out/build/pending-native-operation-2026-10-21/cargo-home'
        if cache:
            environment['CARGO_HOME'] = str(Path(cache).resolve())
        elif warm_cache.is_dir():
            environment['CARGO_HOME'] = str(warm_cache)
        tool = invoke([cargo, '--version'], 'sdk', env=environment).strip()
        invoke([rustc, '-vV'], 'rustc-sdk', env=environment)
        sysroot = Path(invoke([rustc, '--print', 'sysroot'], 'rustc-sysroot', env=environment).strip())
        for name in ('rustc', 'cargo'):
            _artifact(artifacts, sysroot / 'bin' / (name + ('.exe' if os.name == 'nt' else '')))
        for directory_name, pattern in (('bin', '*rustc_driver*.dll'), ('bin', '*LLVM*.dll'),
                                        ('lib', 'librustc_driver*.so')):
            for path in (sysroot / directory_name).glob(pattern):
                _artifact(artifacts, path)
        invoke([cargo, 'build', '--locked', '--offline', '--release', '--manifest-path', directory / 'Cargo.toml'],
               'compile', env=environment)
        executable = directory / 'target/release' / ('serializer-xml-example.exe' if os.name == 'nt' else 'serializer-xml-example')
        launch = [str(executable)]
        _artifact(artifacts, root / 'src/rust_compression_runtime.inc')
    elif language == 'go':
        output = directory / 'model/schema.go'
        output.parent.mkdir(exist_ok=True)
        invoke([compiler, '--input', schema, '--language', 'go', '--go.output', output, '--go.package', 'model'], 'generation')
        generated.append(str(output))
        helper = re.search(r'// Generated package helper prefix: (\w+)', output.read_text(encoding='utf-8')).group(1)
        main = consumer('go', {'@p@': helper})
        module = directory / 'go.mod'
        module.write_text('module serializer-package-reproducibility\n\ngo 1.22\n', encoding='utf-8', newline='\n')
        generated.append(str(module))
        go = _tool(options.get('go') or 'go')
        _artifact(artifacts, go)
        environment.update({'GOTOOLCHAIN': 'local', 'GOPROXY': 'off', 'GOSUMDB': 'off'})
        environment['GOCACHE'] = str(directory / 'go-cache')
        environment['GOMODCACHE'] = str(directory / 'go-module-cache')
        tool = invoke([go, 'version'], 'sdk', env=environment).strip()
        sdk = json.loads(invoke([go, 'env', '-json', 'GOROOT', 'GOVERSION', 'GOOS', 'GOARCH', 'GOTOOLDIR'],
                                'sdk-options', env=environment))
        goroot = Path(sdk['GOROOT'])
        for name in ('compile', 'link'):
            _artifact(artifacts, Path(sdk['GOTOOLDIR']) / (name + ('.exe' if os.name == 'nt' else '')))
        for path in (goroot / 'src/compress/flate').glob('*.go'):
            _artifact(artifacts, path)
        executable = directory / ('producer.exe' if os.name == 'nt' else 'producer')
        invoke([go, 'build', '-trimpath', '-buildvcs=false', '-o', executable, '.'], 'compile', cwd=directory, env=environment)
        invoke([go, 'version', '-m', executable], 'binary-sdk', env=environment)
        launch = [str(executable)]
    else:
        raise ValueError('Unsupported native producer language: ' + language)

    _artifact(artifacts, executable)
    provider = json.loads(invoke([*launch, '--provider'], 'provider-query', env=environment))
    provider['artifacts'] = artifacts
    if language == 'rust':
        provider['cross_host_identity'] = {
            'implementation': provider['implementation'], 'version': provider['version'],
            'options': provider['options'], 'metadata_method': provider['metadata_method']}
    result = {'launch': launch, 'generated': generated, 'tool': tool, 'provider': provider}
    if language == 'c' and os.name == 'nt':
        # Each fresh process selects the pinned DLLs without a shell or another producer process.
        result['environment'] = {'PATH': environment['PATH']}
    return result
