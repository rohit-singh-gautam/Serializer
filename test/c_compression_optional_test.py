# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated C11 optional Zstandard/LZ4 providers and absence profiles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import subprocess

import c_compression_test as core
from java_compression_optional_test import fixtures as optional_fixtures
from go_compression_optional_test import native_fixtures


POLICY = r'''
/* Count forbidden policy callbacks when admission must fail before user work. */
static bool count_poll(void* context) { ++*(size_t*)context; return false; }
/* Replay independent standard frames, retaining previously initialized output on failure. */
static size_t optional_inputs(@c@_format kind, const char* path) {
  FILE* manifest = fopen(path, "r"); require(manifest != NULL, "optional manifest");
  size_t decisions = 0; char row[4096];
  while (fgets(row, sizeof(row), manifest)) {
    char* input = strtok(row, "\t"); char* expected = strtok(NULL, "\t");
    char* code = strtok(NULL, "\t"); char* compressed = strtok(NULL, "\t");
    char* expanded = strtok(NULL, "\t"); char* window = strtok(NULL, "\t\r\n");
    require(input && expected && code && compressed && expanded && window, "optional row");
    srl_buffer packed = load(input), result = {0};
    require(@c@_append(&result, (const uint8_t*)"old", 3, 3) == @c@_ok, "previous output");
    @c@_decode_options options = @c@_default_decode_options(); options.format = kind;
    options.max_input_bytes = (size_t)strtoull(compressed, NULL, 10);
    options.max_output_bytes = (size_t)strtoull(expanded, NULL, 10);
    options.max_window_bytes = (size_t)strtoull(window, NULL, 10);
    const @c@_code wanted = !strcmp(code, "ok") ? @c@_ok :
        !strcmp(code, "RESOURCE_LIMIT") ? @c@_resource_limit : @c@_invalid_data;
    const @c@_code status = @c@_decompress(packed.data, packed.size, options, &result);
    if (status != wanted) { fprintf(stderr, "fixture %s: wanted %d, got %d\n", input, (int)wanted, (int)status); }
    require(status == wanted, "optional fixture category");
    if (status == @c@_ok) {
      srl_buffer plain = load(expected); require(equal(&plain, &result), "optional independent payload");
      srl_buffer_free(&plain);
    } else { require(result.size == 3 && !memcmp(result.data, "old", 3), "failed result unchanged"); }
    srl_buffer_free(&packed); srl_buffer_free(&result); ++decisions;
  }
  require(fclose(manifest) == 0, "close optional manifest"); return decisions;
}
/* Reject absent providers and invalid native policies before application callbacks or transport. */
static void admission(void) {
  const @c@_format formats[] = {@c@_zstd, @c@_lz4};
  for (size_t index = 0; index < sizeof(formats)/sizeof(*formats); ++index) {
    const @c@_format kind = formats[index]; size_t polls = 0;
    @c@_encode_options encode = @c@_default_encode_options(); encode.format = kind;
    encode.is_cancelled = count_poll; encode.context = &polls;
    @c@_decode_options decode = @c@_default_decode_options(); decode.format = kind;
    decode.is_cancelled = count_poll; decode.context = &polls; decode.max_window_bytes = 8u*1024u*1024u;
    if (!@c@_available(kind)) {
      srl_buffer result = {0}; @c@_input input = {0}; @c@_output output = {0}; transport state = {0};
      require(@c@_compress(NULL, 0, encode, &result) == @c@_unavailable, "absent compress");
      require(@c@_decompress(NULL, 0, decode, &result) == @c@_unavailable, "absent decompress");
      require(@c@_output_init(&output, sink, &state, encode) == @c@_unavailable, "absent output");
      require(@c@_input_init(&input, source, &state, decode) == @c@_unavailable, "absent input");
      require(@c@_account_record_encode(NULL, srl_json, encode, &result, NULL) == @c@_unavailable, "absent native encode");
      require(!polls && !state.calls && !result.data, "absent provider side effect");
    } else {
      int minimum = 0, maximum = 0;
#if defined(SERIALIZER_COMPRESSION_ZSTD) && SERIALIZER_COMPRESSION_ZSTD
      if (kind == @c@_zstd) { minimum = ZSTD_minCLevel(); maximum = ZSTD_maxCLevel(); }
#endif
#if defined(SERIALIZER_COMPRESSION_LZ4) && SERIALIZER_COMPRESSION_LZ4
      if (kind == @c@_lz4) { maximum = LZ4F_compressionLevel_max(); }
#endif
      const int levels[] = {minimum, 0, maximum};
      for (size_t level = 0; level < sizeof(levels)/sizeof(*levels); ++level) {
        srl_buffer packed = {0}, plain = {0}; encode.level = levels[level];
        require(@c@_compress((const uint8_t*)"x", 1, encode, &packed) == @c@_ok, "provider level");
        require(@c@_decompress(packed.data, packed.size, decode, &plain) == @c@_ok && plain.size == 1 && plain.data[0] == 'x', "level frame");
        srl_buffer_free(&packed); srl_buffer_free(&plain);
      }
      const int invalid[] = {minimum-1, maximum+1};
      for (size_t level = 0; level < sizeof(invalid)/sizeof(*invalid); ++level) {
        encode.level = invalid[level]; polls = 0;
        srl_buffer result = {0}; @c@_output output = {0}; transport state = {0};
        require(@c@_compress(NULL, 0, encode, &result) == @c@_invalid_options, "level rejected");
        require(@c@_output_init(&output, sink, &state, encode) == @c@_invalid_options, "stream level rejected");
        require(@c@_account_record_encode(NULL, srl_json, encode, &result, NULL) == @c@_invalid_options, "native level admission");
        require(!polls && !state.calls && !result.data, "invalid level side effect");
      }
      @c@_input input = {0}; transport state = {0}; polls = 0;
      decode.max_window_bytes = kind == @c@_zstd ? 1023 : 65535;
      require(@c@_input_init(&input, source, &state, decode) == @c@_resource_limit, "history admission");
      require(!polls && !state.calls, "invalid history side effect");
    }
  }
}
'''


def consumer(prefix, full, binary=False):
    """Reuse maintained C lifecycle/native controls with the selected optional format pair."""
    start = core.CONSUMER[:core.CONSUMER.index('/* Require explicit absence')]
    begin = start.index('/* Map manifest selectors')
    end = start.index('typedef struct transport', begin)
    start = start[:begin] + start[end:]
    begin = start.index('/* Replay every valid/invalid frame')
    end = start.index('/* Round-trip actual owning models', begin)
    start = start[:begin] + start[end:]
    start = start.replace('{@c@_none, @c@_gzip, @c@_zlib, @c@_deflate}', '{@c@_zstd, @c@_lz4}')
    start = start.replace('@c@_default_decode_options();', '@c@_default_decode_options(); decode.max_window_bytes = 8u*1024u*1024u;')
    main = r'''
/* Execute the complete provider pair or an explicit absence/single-provider compile profile. */
int main(void) {
  (void)models; (void)lifecycle; (void)policy_tests; (void)levels; (void)optional_inputs;
  admission();
'''
    if full:
        main += r'''
  require(@c@_available(@c@_zstd) && @c@_available(@c@_lz4), "present provider pair");
  const size_t decisions = optional_inputs(@c@_zstd, "ZSTD.tsv") + optional_inputs(@c@_lz4, "LZ4.tsv");
  models(); lifecycle(); policy_tests(); levels();
  printf("{\"input_decisions\":%zu,\"independent_outputs\":24,\"zstd\":\"%s\",\"lz4\":\"%s\"}\n",
         decisions, ZSTD_versionString(), LZ4_versionString());
'''
    else:
        main += r'  puts("{\"admission_profile\":true}");' + '\n'
    imports = '\n#if defined(SERIALIZER_COMPRESSION_LZ4) && SERIALIZER_COMPRESSION_LZ4\n#include <lz4.h>\n#endif\n'
    result = (start + imports + POLICY + main + '  return 0;\n}\n').replace('@c@', prefix)
    if binary:
        result = result.replace('int protocol = srl_json; protocol <= srl_binary_string',
                                'int protocol = srl_binary_none; protocol <= srl_binary_none')
    return result


def execute_wsl(directory, cc, providers, provider_prefix):
    """Use the existing private Linux bridge with explicit system-library feature profiles."""
    previous = core.REMOTE
    flags = [value for name in providers for value in ('-DSERIALIZER_COMPRESSION_'+name+'=1', '-l'+name.lower())]
    if providers and provider_prefix:
        flags += ['-I'+provider_prefix+'/include', '-L'+provider_prefix+'/lib', '-Wl,-rpath,'+provider_prefix+'/lib']
    core.REMOTE = previous.replace("if sys.argv[2]=='1': command+=['-DSERIALIZER_COMPRESSION_ZLIB=1','-lz']",
                                   'command+=' + repr(flags))
    try:
        return core.execute_wsl(directory, cc, False)
    finally:
        core.REMOTE = previous


def verify_outputs(directory, cpp_helper, environment):
    """Require independent exact-frame decoders and unchanged native C payloads."""
    from compression import zstd
    import lz4.frame
    count = 0
    for index, name in enumerate(('ZSTD', 'LZ4')):
        outputs = [(directory / f'output-{index}-{protocol}.bin', (directory / f'native-{protocol}.bin').read_bytes())
                   for protocol in range(4)]
        outputs += [(directory / f'level-{index}-{level}-{sample}.bin', b'A'*65537 if sample else b'')
                    for level in (0, 1, 6, 9) for sample in (0, 1)]
        for path, expected in outputs:
            frame = path.read_bytes()
            decoder = zstd.ZstdDecompressor() if name == 'ZSTD' else lz4.frame.LZ4FrameDecompressor()
            if decoder.decompress(frame) != expected or not decoder.eof or decoder.unused_data:
                raise AssertionError('Independent C output differs: '+path.name)
            if not frame[4] & 4:
                raise AssertionError('Native checksum was not enabled: '+path.name)
            if cpp_helper:
                for mode in ('decode', 'stream_decode'):
                    destination = directory / (path.stem+'-'+mode+'.plain')
                    core.run([cpp_helper, mode, name.lower(), path, destination], directory, environment)
                    if destination.read_bytes() != expected:
                        raise AssertionError('Native C++ consumer differs: '+path.name)
            count += 1
    return count


def main():
    """Generate actual C output, execute native SDK profiles and retain reproducible evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--cc')
    parser.add_argument('--wsl', action='store_true')
    parser.add_argument('--include', type=Path)
    parser.add_argument('--library-directory', type=Path)
    parser.add_argument('--provider-path', type=Path)
    parser.add_argument('--wsl-provider-prefix', help='Existing native Linux provider installation; never a Windows mount')
    parser.add_argument('--cpp-helper', type=Path)
    parser.add_argument('--cpp-linked-helper', type=Path)
    args = parser.parse_args()
    if args.provider_path:
        sys.path.insert(0, str(args.provider_path.resolve()))
    cc = args.cc or ('/usr/bin/cc' if args.wsl else shutil.which('cl') if os.name == 'nt' else shutil.which('cc'))
    if not cc:
        parser.error('An actual C11 compiler is required')
    root, build = Path(__file__).resolve().parents[1], args.build.resolve()
    if build == root or root in build.parents and build.relative_to(root).parts[0] != 'out':
        parser.error('In-repository scratch must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    if args.library_directory:
        environment['PATH'] = str(args.library_directory.resolve().parent / 'bin') + os.pathsep + environment.get('PATH', '')
    msvc = Path(cc).name.lower() in ('cl', 'cl.exe')
    counts = optional_fixtures(build, ('ZSTD', 'LZ4'))
    if bool(args.cpp_helper) != bool(args.cpp_linked_helper):
        parser.error('Select both native interoperability helpers together')
    native_producers = 0
    if args.cpp_helper:
        args.cpp_helper, args.cpp_linked_helper = args.cpp_helper.resolve(), args.cpp_linked_helper.resolve()
        native_producers = native_fixtures(args, build, environment, counts)
    profiles = {}
    for name, providers, schema, extra in (
            ('normal', ('ZSTD', 'LZ4'), core.SCHEMA, ()),
            ('provider_free', (), core.SCHEMA, ()),
            ('zstd_only', ('ZSTD',), core.SCHEMA, ()),
            ('lz4_only', ('LZ4',), core.SCHEMA, ()),
            ('collision', ('ZSTD', 'LZ4'), core.SCHEMA+' class serializer_compression {}', ()),
            ('binary', ('ZSTD', 'LZ4'), core.SCHEMA, ('--protocols', 'binary_none'))):
        directory = build if name == 'normal' else build / name
        directory.mkdir(exist_ok=True)
        source = directory / 'model.serializer'; source.write_text(schema, encoding='utf-8')
        header = directory / 'schema.h'
        core.run([args.compiler.resolve(), '--input', source, '--language', 'c', '--output', header, *extra], directory, environment)
        prefix = re.search(r'Generated C compression prefix: (\w+)', header.read_text(encoding='utf-8')).group(1)
        if name == 'collision' and prefix == 'serializer_compression':
            raise AssertionError('C optional helper collision')
        program = consumer(prefix, name == 'normal', name == 'binary')
        if name in ('collision', 'binary'):
            program = program.replace('  admission();', '  admission(); models();')
        (directory / 'main.c').write_text(program, encoding='utf-8')
        if args.wsl:
            output, sdk, bundle_hash = execute_wsl(directory, cc, providers, args.wsl_provider_prefix)
        else:
            executable = directory / ('consumer.exe' if msvc else 'consumer')
            command = [cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2', directory/'main.c', '-o', executable, '-lm']
            if msvc:
                command = [cc, '/nologo', '/std:c11', '/W4', '/WX', '/O2', '/utf-8', '/D_CRT_SECURE_NO_WARNINGS',
                           directory/'main.c', '/Fe:'+str(executable), '/Fo:'+str(directory/'main.obj')]
            if args.include:
                command += ['/I' if msvc else '-I', args.include.resolve()]
            for provider in providers:
                command += [('/D' if msvc else '-D')+'SERIALIZER_COMPRESSION_'+provider+'=1']
                command += [args.library_directory.resolve()/(provider.lower()+'.lib') if args.library_directory else '-l'+provider.lower()]
            core.run(command, directory, environment)
            output = core.run([executable], directory, environment)
        report = json.loads(output.strip().splitlines()[-1])
        if name == 'normal':
            if report['input_decisions'] != sum(counts.values()) or verify_outputs(directory, args.cpp_helper, environment) != 24:
                raise AssertionError('Incomplete optional provider decisions')
        profiles[name] = {'report': report, 'generated_sha256': hashlib.sha256(header.read_bytes()).hexdigest()}
        if args.wsl:
            profiles[name]['native_archive_sha256'] = bundle_hash
    if not args.wsl:
        version = subprocess.run([cc, '/?' if msvc else '--version'], capture_output=True, text=True)
        sdk = '\n'.join(line for line in (version.stdout+version.stderr).splitlines() if not msvc or 'Compiler Version' in line)
    report = {'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(), 'cc': sdk,
              'native_wsl': args.wsl, 'input_decisions': counts, 'independent_outputs': 24, 'profiles': profiles,
              'native_cpp_producers': native_producers, 'native_cpp_consumers': 48 if args.cpp_helper else 0,
              'test_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (build/'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
