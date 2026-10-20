# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify real generated owning C11 ZIP/OPC with independent ZIP/XML and failure oracles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
import zipfile

import rust_package_test as corpus
import python_package_test as independent

ROOT = Path(__file__).resolve().parents[1]


def main():
    """Generate actual C, compile the configured provider profile, and independently inspect its output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--cc', default=shutil.which('cl') if os.name == 'nt' else shutil.which('cc'))
    parser.add_argument('--provider-prefix', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(strict=True), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    if not args.cc:
        parser.error('Provide an actual C11 SDK compiler in --cc')
    build.mkdir(parents=True, exist_ok=True)
    (build / 'verification.json').unlink(missing_ok=True)
    environment = os.environ.copy()
    spool_temp = build / 'spool-temp'
    spool_temp.mkdir(exist_ok=True)
    environment['TMP'] = environment['TEMP'] = str(spool_temp)
    msvc = Path(args.cc).name.lower() in ('cl', 'cl.exe')
    prefix = args.provider_prefix.resolve() if args.provider_prefix else None
    if prefix and os.name == 'nt':
        environment['PATH'] = str(prefix / 'bin') + os.pathsep + environment.get('PATH', '')
    if os.name == 'nt':
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    started, commands = time.monotonic(), []

    def run(command, name, failure=False):
        """Capture exact SDK execution and reject any unsuccessful qualification command."""
        argv = [str(value) for value in command]
        result = subprocess.run(argv, capture_output=True, text=True, encoding='utf-8', env=environment, timeout=900)
        (build / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'argv': argv, 'returncode': result.returncode})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if failure:
            assert result.returncode, 'Invalid source unexpectedly succeeded'
        elif result.returncode:
            print(result.stdout + result.stderr)
            result.check_returncode()
        return result.stdout + result.stderr

    schema = build / 'profile.serializer'
    schema.write_text((ROOT / 'test/resources/opc_profile.serializer').read_text(encoding='utf-8').replace(
        'serializer version 1.20.0;', 'serializer version 1.21.0;') + corpus.CONTRACT + corpus.application_schema(), encoding='utf-8')
    output = build / 'schema.h'
    run([compiler, '--input', schema, '--language', 'c', '--output', output], 'generation')
    generated = output.read_text(encoding='utf-8')
    helper = re.search(r'/\* Generated C package prefix: (\w+) \*/', generated).group(1)
    (build / 'main.c').write_text((ROOT / 'test/resources/package_c_main.c').read_text(encoding='utf-8').replace(
        '@p@', helper), encoding='utf-8')
    rows = corpus.fixtures(build)
    application_rows = corpus.application_fixtures(build)
    executable = build / ('consumer.exe' if os.name == 'nt' else 'consumer')
    if msvc:
        compile_command = [args.cc, '/nologo', '/std:c11', '/W4', '/WX', '/O2', '/utf-8',
                           '/D_CRT_SECURE_NO_WARNINGS', '/DSERIALIZER_COMPRESSION_ZLIB=1',
                           build / 'main.c', '/Fe' + str(executable), '/Fo' + str(build / 'main.obj')]
        if prefix:
            compile_command.extend(['/I' + str(prefix / 'include'), '/I' + str(prefix / 'include/libxml2')])
        compile_command.append('/link')
        if prefix:
            compile_command.append('/LIBPATH:' + str(prefix / 'lib'))
        compile_command.extend(['libxml2.lib', 'z.lib'])
    else:
        compile_command = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                           '-DSERIALIZER_COMPRESSION_ZLIB=1', build / 'main.c', '-o', executable]
        compile_command.extend(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libxml-2.0'], text=True).split())
        compile_command.extend(['-lz', '-lm'])
    sdk = run([args.cc, '/Bv'] if msvc else [args.cc, '--version'], 'sdk', failure=msvc).strip()
    run(compile_command, 'compile')
    run([executable, build], 'acceptance')
    if args.sanitize:
        sanitized_executable = build / ('consumer_asan.exe' if os.name == 'nt' else 'consumer_asan')
        if msvc:
            sanitized = [str(value).replace(str(executable), str(sanitized_executable)).replace(
                str(build / 'main.obj'), str(build / 'main_asan.obj')) for value in compile_command if value != '/O2']
            sanitized[1:1] = ['/Od', '/Zi', '/fsanitize=address', '/Fd' + str(build / 'native_asan.pdb')]
        else:
            sanitized = [str(value).replace(str(executable), str(sanitized_executable)) for value in compile_command]
            sanitized[1:1] = ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        run(sanitized, 'sanitize-compile')
        run([sanitized_executable, build], 'sanitize-acceptance')
    companion = build / 'companion.serializer'
    companion.write_text(corpus.COMPANION_GRAPH, encoding='utf-8')
    (build / 'companion.h').unlink(missing_ok=True)
    diagnostic = run([compiler, '--input', companion, '--language', 'c', '--output', build / 'companion.h'], 'companion-graph-rejection', failure=True)
    assert 'document-companion' in diagnostic and not (build / 'companion.h').exists()
    run([compiler, '--input', ROOT / 'test/resources/opc_identity.serializer',
         '--config', ROOT / 'test/resources/opc_identity.ini', '--language', 'c',
         '--output', build / 'identity.h'], 'identity-package-generation')
    assert 'identity_package_archive_load' in (build / 'identity.h').read_text(encoding='utf-8')
    run([compiler, '--input', ROOT / 'test/resources/xml_identity_generated.serializer',
         '--config', ROOT / 'test/resources/opc_identity.ini', '--language', 'c',
         '--output', build / 'identity_native.h'], 'ordinary-identity-generation')
    collision = build / 'collision.serializer'
    collision.write_text(schema.read_text() + '\nclass serializer_package {} class serializer_package_2 {}\n', encoding='utf-8')
    run([compiler, '--input', collision, '--language', 'c', '--output', build / 'collision.h'], 'collision')
    # C normalizes the second schema class to serializer_package2, leaving suffix _2 free.
    assert 'Generated C package prefix: serializer_package_2' in (build / 'collision.h').read_text()
    (build / 'collision_main.c').write_text('#define SERIALIZER_COMPRESSION_ZLIB 1\n#include "collision.h"\nint main(void){profile_archive_input input={0};srl_buffer bytes={0};serializer_package_2_budget budget=serializer_package_2_default_budget();return profile_archive_encode(&input,&bytes,&budget)?0:1;}\n', encoding='utf-8')
    collision_executable = build / ('collision_consumer.exe' if os.name == 'nt' else 'collision_consumer')
    collision_command = [str(value).replace(str(build / 'main.c'), str(build / 'collision_main.c')).replace(
        str(executable), str(collision_executable)).replace(str(build / 'main.obj'), str(build / 'collision_main.obj')) for value in compile_command]
    run(collision_command, 'collision-compile')
    run([collision_executable], 'collision-acceptance')
    stored_schema = build / 'stored_only.serializer'
    stored_schema.write_text('serializer version 1.21.0; package blob { part content:bytes { name(dynamic); content_type(dynamic); relationship("urn:serializer:blob"); occurs(0,unbounded); } }\n', encoding='utf-8')
    run([compiler, '--input', stored_schema, '--language', 'c', '--output', build / 'stored_only.h'], 'stored-only-generation')
    # The provider-free profile is independent; common native XML support currently prevents
    # combining distinct generated XML headers in one translation unit.
    (build / 'stored_only.c').write_text('#include "stored_only.h"\n#include "stored_only.h"\nint main(void){blob_input input={0};blob_value value={0};srl_buffer bytes={0};serializer_package_policy policy={0};serializer_package_budget budget=serializer_package_default_budget();int status=blob_encode(&input,&bytes,&budget);if(!status){budget=serializer_package_default_budget();status=blob_load(&value,bytes.data,bytes.size,policy,&budget);}blob_free(&value);srl_buffer_free(&bytes);return status;}\n', encoding='utf-8')
    stored_executable = build / ('stored_consumer.exe' if os.name == 'nt' else 'stored_consumer')
    stored_command = [str(value).replace(str(build / 'main.c'), str(build / 'stored_only.c')).replace(
        str(executable), str(stored_executable)).replace(str(build / 'main.obj'), str(build / 'stored_only.obj'))
        for value in compile_command if str(value) not in ('/DSERIALIZER_COMPRESSION_ZLIB=1',
                                                         '-DSERIALIZER_COMPRESSION_ZLIB=1', 'z.lib', '-lz')]
    run(stored_command, 'stored-only-compile')
    run([stored_executable], 'stored-only-acceptance')
    for filename in ('generated.opc', 'generated64.opc', 'preserved.opc', 'canonical-a.opc', 'canonical-b.opc'):
        with zipfile.ZipFile(build / filename) as archive:
            assert archive.testzip() is None
            assert archive.namelist() == sorted(archive.namelist())
            assert all(item.date_time == (1980, 1, 1, 0, 0, 0) for item in archive.infolist())
    assert (build / 'canonical-a.opc').read_bytes() == (build / 'canonical-b.opc').read_bytes()
    with zipfile.ZipFile(build / 'preserved.opc') as archive:
        assert archive.read('Extensions/vendor.xml') == independent.independent_parts(extension=True)['Extensions/vendor.xml']
        assert b'independent' in archive.read('Alternate/root.xml')
    with zipfile.ZipFile(build / 'scoped-preserved.opc') as archive:
        assert b'../Extensions/vendor.xml?choice=one#node' in archive.read('Alternate/_rels/root.xml.rels')
        assert b'https://example.invalid/a?x=1#f' in archive.read('Alternate/_rels/root.xml.rels')
        assert archive.read('Extensions/vendor.xml') == independent.independent_parts(extension=True)['Extensions/vendor.xml']
    assert b'PK\x06\x06' in (build / 'generated64.opc').read_bytes()
    assert not list(build.glob('*.serializer-package-*.tmp'))
    assert not list(spool_temp.iterdir()), 'Memory-only spooling created temporary files'
    for count in (65534, 65535, 65536):
        with zipfile.ZipFile(build / f'count-output-{count}.zip') as archive:
            assert len(archive.infolist()) == count and archive.testzip() is None
    for filename in ('stored.zip', 'stored64.zip'):
        with zipfile.ZipFile(build / filename) as archive:
            assert archive.read('stored.bin') == b'independent stored bytes'
            assert archive.getinfo('stored.bin').compress_type == zipfile.ZIP_STORED
    report = {'status': 'passed', 'scope': 'Actual owning C11 libxml2/zlib profile on ' + os.sys.platform,
              'sdk': sdk, 'independent_cases': len(rows), 'application_cases': len(application_rows), 'source_bytes': output.stat().st_size,
              'source_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
              'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
              'sanitized': args.sanitize,
              'elapsed_seconds': round(time.monotonic() - started, 3), 'commands': commands}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
