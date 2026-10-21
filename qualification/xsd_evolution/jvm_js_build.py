# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Build strict external JVM/JS consumers for two fixed-XSD native model variants."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil


def digest(path):
    """Pin complete input, SDK, provider and consumer bytes without normalizing them."""
    checksum = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            checksum.update(block)
    return checksum.hexdigest()


def executable(value):
    """Resolve the selected actual executable while preserving explicit SDK selection."""
    resolved = shutil.which(str(value))
    return Path(resolved if resolved else value).resolve(strict=True)


def build(language, root, compiler, directory, options, run):
    """Generate one version and strictly compile a consumer outside its generated library module."""
    root, compiler, directory = Path(root).resolve(), Path(compiler).resolve(strict=True), Path(directory).resolve()
    if language not in ('java', 'kotlin', 'javascript', 'typescript'):
        raise ValueError('Unsupported JVM/JS XSD evolution language: ' + language)
    variant = options.get('variant')
    if variant not in ('v1', 'v2'):
        raise ValueError('Select variant v1 or v2 in options.variant')
    directory.mkdir(parents=True, exist_ok=True)
    source = root / 'qualification/xsd_evolution'
    schema = Path(options.get('schema_directory') or source) / variant / 'order.serializer'
    artifacts = {}

    def pin(path):
        """Capture an actual file by resolved absolute path for later stability checks."""
        path = Path(path).resolve(strict=True)
        artifacts[str(path)] = digest(path)

    def execute(argv, label, *, cwd=None, env=None):
        """Keep command logging, environment and SDK failure handling in the shared orchestrator."""
        return run([str(value) for value in argv], language + '-' + variant + '-' + label,
                   cwd=directory if cwd is None else cwd, env=env)

    if language in ('java', 'kotlin'):
        java = executable(options.get('java') or 'java')
        tool = execute([java, '-version'], 'java-version').strip()
        properties = execute([java, '-XshowSettings:properties', '-version'], 'java-properties')
        home_match = re.search(r'^\s*java\.home\s*=\s*(.+)$', properties, re.MULTILINE)
        if home_match is None:
            raise RuntimeError('The selected JVM did not report java.home')
        home = Path(home_match.group(1).strip()).resolve(strict=True)
        pin(java)
        pin(home / 'lib/modules')
        for relative in ('bin/java.dll', 'bin/server/jvm.dll', 'lib/libjava.so', 'lib/server/libjvm.so',
                         'lib/libjava.dylib', 'lib/server/libjvm.dylib'):
            path = home / relative
            if path.is_file():
                pin(path)
        if language == 'java':
            generated = directory / 'Schema.java'
            execute([compiler, '--input', schema, '--language', 'java', '--java.package', 'generated',
                     '--java.output', generated], 'generate')
            shutil.copyfile(source / 'Main.java', directory / 'Main.java')
            javac = executable(options.get('javac') or java.parent / ('javac.exe' if os.name == 'nt' else 'javac'))
            strict = [javac, '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror']
            execute(strict + ['-d', directory / 'classes', generated], 'library-compile')
            execute(strict + ['-classpath', directory / 'classes', '-d', directory / 'classes',
                              directory / 'Main.java'], 'consumer-compile')
            pin(javac)
            for path in (directory / 'classes').rglob('*.class'):
                pin(path)
            launch = [str(java), '-cp', str(directory / 'classes'), 'evolution_consumer.Main']
        else:
            selected = options.get('kotlinc') or shutil.which('kotlinc') or shutil.which('kotlinc.bat')
            if not selected:
                raise RuntimeError('Select the actual Kotlin compiler distribution')
            kotlin_home = Path(selected).resolve(strict=True).parent.parent
            compiler_jar = kotlin_home / 'lib/kotlin-compiler.jar'
            command = [java, '-Xmx2g', '-cp', compiler_jar, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                       '-kotlin-home', kotlin_home, '-jvm-target', '17', '-Werror']
            tool += '\n' + execute(command[:7] + ['-version'], 'kotlin-version').strip()
            generated = directory / 'Schema.kt'
            execute([compiler, '--input', schema, '--language', 'kotlin', '--kotlin.package', 'generated',
                     '--kotlin.output', generated], 'generate')
            shutil.copyfile(source / 'Main.kt', directory / 'Main.kt')
            execute(command + ['-module-name', 'xsd_evolution_library', '-d', directory / 'generated.jar', generated], 'library-compile')
            execute(command + ['-module-name', 'xsd_evolution_consumer', '-classpath', directory / 'generated.jar',
                               '-include-runtime', '-d', directory / 'consumer.jar', directory / 'Main.kt'], 'consumer-compile')
            for path in (compiler_jar, kotlin_home / 'lib/kotlin-stdlib.jar', directory / 'generated.jar', directory / 'consumer.jar'):
                pin(path)
            launch = [str(java), '-cp', str(directory / 'consumer.jar') + os.pathsep + str(directory / 'generated.jar'),
                      'evolution_consumer.MainKt']
        pin(generated)
        return {'launch': launch, 'generated': [str(generated)], 'tool': tool, 'artifacts': artifacts}

    node = executable(options.get('node') or 'node')
    dependency_path = root / 'test/package_node_dependencies.py'
    spec = importlib.util.spec_from_file_location('serializer_xsd_evolution_node_dependencies', dependency_path)
    dependencies = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(dependencies)
    modules = dependencies.prepare_node_dependencies(directory, Path(options.get('node_dependencies') or root), execute)
    generated = [directory / 'schema.mjs']
    generate = [compiler, '--input', schema]
    if language == 'typescript':
        generated.append(directory / 'schema.d.mts')
        execute(generate + ['--language', 'js,typescript', '--js.output', generated[0], '--typescript.output', generated[1]], 'generate')
        for name in ('main.mts', 'node.d.mts'):
            shutil.copyfile(source / name, directory / name)
        typescript = Path(options.get('typescript_cli') or modules / 'typescript/bin/tsc').resolve(strict=True)
        typescript_version = execute([node, typescript, '--version'], 'typescript-version').strip()
        execute([node, typescript, '--strict', '--noEmitOnError', '--target', 'ES2022', '--module', 'NodeNext',
                 '--moduleResolution', 'NodeNext', '--outDir', directory, directory / 'main.mts', directory / 'node.d.mts'], 'strict-consumer-compile')
        pin(typescript)
        pin(directory / 'main.mts')
        pin(directory / 'node.d.mts')
    else:
        execute(generate + ['--language', 'js', '--js.output', generated[0]], 'generate')
        shutil.copyfile(source / 'main.mjs', directory / 'main.mjs')
        typescript_version = ''
    versions = execute([node, '-p', 'JSON.stringify({executable:process.execPath,node:process.version})'], 'node-version')
    runtime = json.loads([line for line in versions.splitlines() if line.startswith('{')][-1])
    pin(runtime['executable'])
    pin(directory / 'main.mjs')
    pin(directory / 'package-lock.json')
    for name in ('saxes', 'xmlchars', '@xmldom/xmldom') + (('typescript',) if language == 'typescript' else ()):
        for path in (modules / name).rglob('*'):
            if path.is_file():
                pin(path)
    for path in generated:
        pin(path)
    return {'launch': [str(node), str(directory / 'main.mjs')], 'generated': [str(path) for path in generated],
            'tool': runtime['node'] + ('\n' + typescript_version if typescript_version else ''), 'artifacts': artifacts}
