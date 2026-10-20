# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Build actual JVM/JS package producers and pin their selected compression providers."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil


def digest(path):
    """Hash actual tool/provider bytes without loading the complete binary into memory."""
    checksum = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            checksum.update(block)
    return checksum.hexdigest()


def executable(value):
    """Resolve the real selected SDK executable, retaining explicit tool selection."""
    candidate = shutil.which(str(value))
    return Path(candidate if candidate else value).resolve(strict=True)


def last_json(output):
    """Accept one actual JSON probe record while leaving ordinary SDK warnings in retained logs."""
    return json.loads([line for line in output.splitlines() if line.startswith('{')][-1])


def jvm_artifacts(java, output, execute):
    """Pin the Java runtime image, native ZIP implementation and dynamically linked zlib where present."""
    settings = execute([str(java), '-XshowSettings:properties', '-version'], 'java-properties')
    match = re.search(r'^\s*java\.home\s*=\s*(.+)$', settings, re.MULTILINE)
    if match is None:
        raise RuntimeError('The actual JVM did not report java.home')
    home = Path(match.group(1).strip()).resolve(strict=True)
    artifacts = {str(java): digest(java)}
    modules = home / 'lib/modules'
    if not modules.is_file():
        raise RuntimeError('The selected JVM has no runtime image to fingerprint')
    artifacts[str(modules)] = digest(modules)
    native = []
    for relative in ['bin/zip.dll', 'lib/libzip.so', 'lib/libzip.dylib']:
        path = home / relative
        if path.is_file():
            resolved = path.resolve(strict=True)
            artifacts[str(resolved)] = digest(resolved)
            native.append(resolved)
    if not native:
        raise RuntimeError('The selected JVM native ZIP provider could not be fingerprinted')
    for relative in ['bin/java.dll', 'bin/zlib.dll', 'lib/libjava.so']:
        path = home / relative
        if path.is_file():
            resolved = path.resolve(strict=True)
            artifacts[str(resolved)] = digest(resolved)
    if os.name != 'nt':
        for path in native:
            if path.suffix == '.so':
                dependencies = execute(['ldd', str(path)], 'java-zip-dependencies')
                for value in re.findall(r'\blibz[^\s]*\s+=>\s+(/[^\s]+)', dependencies):
                    library = Path(value).resolve(strict=True)
                    artifacts[str(library)] = digest(library)
    for path in output:
        artifacts[str(path.resolve(strict=True))] = digest(path)
    return home, artifacts


def build(language, root, compiler, directory, options, run):
    """Generate, strictly compile and probe one actual external producer without changing SDK installations."""
    root, compiler, directory = Path(root).resolve(), Path(compiler).resolve(strict=True), Path(directory).resolve()
    if language not in ('java', 'kotlin', 'javascript', 'typescript'):
        raise ValueError('Unsupported JVM/JS producer language: ' + language)
    directory.mkdir(parents=True, exist_ok=True)
    source = root / 'qualification/package_reproducibility'
    schema = root / 'test/resources/package_relocation.serializer'

    def execute(argv, label, *, cwd=None, env=None):
        """Preserve the orchestrator's logging, environment, failure and command provenance contract."""
        return run([str(value) for value in argv], language + '-' + label,
                   cwd=directory if cwd is None else cwd, env=env)

    if language in ('java', 'kotlin'):
        java = executable(options.get('java') or 'java')
        tool = execute([java, '-version'], 'java-version').strip()
        if language == 'java':
            generated = directory / 'Generated.java'
            execute([compiler, '--input', schema, '--language', 'java', '--java.package', 'generated',
                     '--java.output', generated], 'generate')
            shutil.copyfile(source / 'Main.java', directory / 'Main.java')
            javac = executable(options.get('javac') or java.parent / ('javac.exe' if os.name == 'nt' else 'javac'))
            execute([javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
                     '-d', directory / 'classes', generated, directory / 'Main.java'], 'compile')
            launch = [str(java), '-cp', str(directory / 'classes'), 'reproducibility.Main']
            images = list((directory / 'classes').rglob('*.class'))
            image_tools = [javac]
        else:
            configured = options.get('kotlinc') or shutil.which('kotlinc') or shutil.which('kotlinc.bat')
            if not configured:
                raise RuntimeError('Select an actual Kotlin compiler in options.kotlinc')
            kotlin_home = Path(configured).resolve(strict=True).parent.parent
            compiler_jar = kotlin_home / 'lib/kotlin-compiler.jar'
            if not compiler_jar.is_file():
                raise RuntimeError('Expected the standard Kotlin compiler distribution')
            command = [java, '-Xmx2g', '-cp', compiler_jar, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                       '-kotlin-home', kotlin_home, '-jvm-target', '17', '-Werror']
            tool += '\n' + execute(command[:7] + ['-version'], 'kotlin-version').strip()
            generated = directory / 'Schema.kt'
            execute([compiler, '--input', schema, '--language', 'kotlin', '--kotlin.package', 'generated',
                     '--kotlin.output', generated], 'generate')
            shutil.copyfile(source / 'Main.kt', directory / 'Main.kt')
            execute(command + ['-module-name', 'package_reproducibility_library', '-d', directory / 'generated.jar', generated], 'library-compile')
            execute(command + ['-module-name', 'package_reproducibility_consumer', '-classpath', directory / 'generated.jar',
                               '-include-runtime', '-d', directory / 'producer.jar', directory / 'Main.kt'], 'consumer-compile')
            launch = [str(java), '-cp', str(directory / 'producer.jar') + os.pathsep + str(directory / 'generated.jar'), 'reproducibility.MainKt']
            images = [directory / 'generated.jar', directory / 'producer.jar']
            image_tools = [compiler_jar, kotlin_home / 'lib/kotlin-stdlib.jar']
        provider = last_json(execute(launch + ['--provider'], 'provider-probe'))
        _, artifacts = jvm_artifacts(java, [*images, *image_tools, generated], execute)
        provider['artifacts'] = artifacts
        return {'launch': launch, 'generated': [str(generated)], 'tool': tool, 'provider': provider}

    node = executable(options.get('node') or 'node')
    dependency_spec = importlib.util.spec_from_file_location('serializer_package_reproducibility_node_dependencies', root / 'test/package_node_dependencies.py')
    dependencies = importlib.util.module_from_spec(dependency_spec)
    dependency_spec.loader.exec_module(dependencies)
    modules = dependencies.prepare_node_dependencies(directory, Path(options.get('node_dependencies') or root), execute)
    generated = [directory / 'schema.mjs']
    command = [compiler, '--input', schema]
    if language == 'typescript':
        generated.append(directory / 'schema.d.mts')
        execute(command + ['--language', 'js,typescript', '--js.output', generated[0], '--typescript.output', generated[1]], 'generate')
        for name in ['main.mts', 'node.d.mts']:
            shutil.copyfile(source / name, directory / name)
        typescript = Path(options.get('typescript_cli') or modules / 'typescript/bin/tsc').resolve(strict=True)
        typescript_version = execute([node, typescript, '--version'], 'typescript-version').strip()
        execute([node, typescript, '--strict', '--noEmitOnError', '--target', 'ES2022', '--module', 'NodeNext',
                 '--moduleResolution', 'NodeNext', '--outDir', directory, directory / 'main.mts', directory / 'node.d.mts'], 'strict-consumer-compile')
    else:
        execute(command + ['--language', 'js', '--js.output', generated[0]], 'generate')
        shutil.copyfile(source / 'main.mjs', directory / 'main.mjs')
        typescript = None
        typescript_version = ''
    launch = [str(node), str(directory / 'main.mjs')]
    versions = last_json(execute([node, '-p', 'JSON.stringify({executable:process.execPath,node:process.version,zlib:process.versions.zlib})'], 'node-versions'))
    actual_node = Path(versions['executable']).resolve(strict=True)
    provider = last_json(execute(launch + ['--provider'], 'provider-probe'))
    if provider['version'] != versions['zlib']:
        raise RuntimeError('The actual selected Node provider reported inconsistent zlib versions')
    files = [actual_node, directory / 'main.mjs', directory / 'package-lock.json', *generated]
    for relative in ['saxes/package.json', 'saxes/saxes.js', 'xmlchars/package.json', 'xmlchars/xmlchars.js',
                     '@xmldom/xmldom/package.json', '@xmldom/xmldom/lib/dom-parser.js']:
        files.append(modules / relative)
    if typescript is not None:
        files += [typescript, modules / 'typescript/package.json', directory / 'main.mts', directory / 'node.d.mts']
        implementation = typescript.parent.parent / 'lib/_tsc.js'
        if implementation.is_file():
            files.append(implementation)
    provider['artifacts'] = {str(path.resolve(strict=True)): digest(path) for path in files}
    if os.name != 'nt':
        dependencies = execute(['ldd', actual_node], 'node-native-dependencies')
        for value in re.findall(r'\blibz[^\s]*\s+=>\s+(/[^\s]+)', dependencies):
            library = Path(value).resolve(strict=True)
            provider['artifacts'][str(library)] = digest(library)
    return {'launch': launch, 'generated': [str(path) for path in generated],
            'tool': versions['node'] + ('\n' + typescript_version if typescript_version else ''), 'provider': provider}
