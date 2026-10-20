# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify actual generated C11 compression, optional zlib and independent frames."""
import argparse
import base64
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import zlib

from java_compression_test import fixtures as common_fixtures
from csharp_compression_test import extra_fixtures


SCHEMA = ('serializer version 1; class account_record stable_ids { '
          'public uint32 account_id (1); public string label (2); }')

CONSUMER = r'''#include "schema.h"
#include <assert.h>

/* Keep every assertion active even in a release SDK build. */
static void require(bool value, const char* message) {
  if (!value) { fprintf(stderr, "FAILED: %s\n", message); exit(1); }
}
/* Load small independently authored files without C-string interpretation. */
static srl_buffer load(const char* path) {
  FILE* file = fopen(path, "rb"); require(file != NULL, "open fixture");
  require(fseek(file, 0, SEEK_END) == 0, "seek fixture");
  const long length = ftell(file); require(length >= 0, "fixture size");
  require(fseek(file, 0, SEEK_SET) == 0, "rewind fixture");
  srl_buffer result = {0}; require(srl_reserve(&result, (size_t)length), "allocate fixture");
  result.size = (size_t)length;
  require(fread(result.data, 1, result.size, file) == result.size, "read fixture");
  require(fclose(file) == 0, "close fixture"); return result;
}
/* Write producer bytes for an independent Python decompressor. */
static void save(const char* path, const srl_buffer* bytes) {
  FILE* file = fopen(path, "wb"); require(file != NULL, "open output");
  require(fwrite(bytes->data, 1, bytes->size, file) == bytes->size, "write output");
  require(fclose(file) == 0, "close output");
}
/* Compare exact owning byte extents, including empty frames. */
static bool equal(const srl_buffer* left, const srl_buffer* right) {
  return left->size == right->size && (!left->size || !memcmp(left->data, right->data, left->size));
}
/* Map manifest selectors without accepting an implicit default format. */
static @c@_format format(const char* name) {
  if (!strcmp(name, "GZIP")) { return @c@_gzip; }
  if (!strcmp(name, "ZLIB")) { return @c@_zlib; }
  require(!strcmp(name, "DEFLATE"), "fixture format"); return @c@_deflate;
}

typedef struct transport {
  srl_buffer bytes;
  size_t position, calls, maximum;
  @c@_output* output;
  @c@_input* input;
  int action;
  bool cancel;
} transport;
/* Borrow a sink that can expose short writes, first failures and callback reentry. */
static @c@_code sink(void* context, const uint8_t* bytes, size_t size, size_t* written) {
  transport* value = (transport*)context; ++value->calls;
  if (size > value->maximum) { value->maximum = size; }
  if (value->action == 1) { return @c@_transport; }
  if (value->action == 2) { *written = 0; return @c@_ok; }
  require(@c@_append(&value->bytes, bytes, size, SIZE_MAX) == @c@_ok, "sink storage");
  *written = size;
  if (value->action == 3) { @c@_output_close(value->output); }
  if (value->action == 4) { require(@c@_output_finish(value->output) == @c@_invalid_state, "nested finish"); }
  if (value->action == 5) { value->cancel = true; }
  return @c@_ok;
}
/* Borrow a source with seven-byte reads and explicit zero-byte EOF. */
static @c@_code source(void* context, uint8_t* bytes, size_t capacity, size_t* count) {
  transport* value = (transport*)context; ++value->calls;
  if (capacity > value->maximum) { value->maximum = capacity; }
  if (value->action == 1) { return @c@_transport; }
  if (value->action == 2) { *count = capacity+1; return @c@_ok; }
  const size_t remaining = value->bytes.size-value->position;
  *count = remaining < capacity ? remaining : capacity;
  if (*count > 7) { *count = 7; }
  if (*count) { memcpy(bytes, value->bytes.data+value->position, *count); }
  value->position += *count;
  if (value->action == 3) { @c@_input_close(value->input); }
  if (value->action == 4) { require(@c@_input_finish(value->input) == @c@_invalid_state, "nested input finish"); }
  if (value->action == 5) { value->cancel = true; }
  return @c@_ok;
}
/* Expose cancellation while retaining the same caller-owned context. */
static bool cancelled(void* context) { return ((transport*)context)->cancel; }

/* Replay every valid/invalid frame at exact and one-tight admission limits. */
static size_t fixtures(void) {
  FILE* manifest = fopen("fixtures.tsv", "r"); require(manifest != NULL, "open manifest");
  size_t count = 0; char row[4096];
  while (fgets(row, sizeof(row), manifest)) {
    char* name = strtok(row, "\t"); char* kind = strtok(NULL, "\t");
    char* input = strtok(NULL, "\t"); char* oracle = strtok(NULL, "\t\r\n");
    require(name && kind && input && oracle, "manifest fields");
    srl_buffer packed = load(input), result = {0};
    @c@_decode_options policy = @c@_default_decode_options(); policy.format = format(name);
    if (!strcmp(kind, "invalid")) {
      const @c@_code status = @c@_decompress(packed.data, packed.size, policy, &result);
      if (status != @c@_invalid_data) { fprintf(stderr, "fixture %s got %d\n", input, (int)status); }
      require(status == @c@_invalid_data, "invalid frame category");
    } else {
      srl_buffer plain = load(oracle); policy.max_input_bytes = packed.size; policy.max_output_bytes = plain.size;
      require(@c@_decompress(packed.data, packed.size, policy, &result) == @c@_ok, input);
      require(equal(&plain, &result), "independent input payload");
      if (plain.size) {
        --policy.max_output_bytes;
        require(@c@_decompress(packed.data, packed.size, policy, &result) == @c@_resource_limit, "expanded bound");
        require(equal(&plain, &result), "transactional expanded failure"); ++policy.max_output_bytes;
      }
      if (packed.size) {
        --policy.max_input_bytes;
        require(@c@_decompress(packed.data, packed.size, policy, &result) == @c@_resource_limit, "compressed bound");
      }
      srl_buffer_free(&plain);
    }
    srl_buffer_free(&packed); srl_buffer_free(&result); ++count;
  }
  require(fclose(manifest) == 0, "close manifest"); return count;
}

/* Round-trip actual owning models and prove inner trailing bytes never publish a replacement. */
static void models(void) {
  account_record original = {0}, destination = {0};
  require(account_record_init(&original) == srl_ok && account_record_init(&destination) == srl_ok, "init models");
  original.account_id = 42; destination.account_id = 99;
  require(srl_string_set_cstr(&original.label, "owning \xf0\x9f\x98\x80") == srl_ok, "Unicode model");
  const @c@_format formats[] = {@c@_none, @c@_gzip, @c@_zlib, @c@_deflate};
  for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
    srl_buffer plain = {0};
    require(account_record_encode(&original, (srl_protocol)protocol, &plain) == srl_ok, "native encode");
    char path[128]; (void)snprintf(path, sizeof(path), "native-%d.bin", protocol); save(path, &plain);
    for (size_t index = 0; index < sizeof(formats)/sizeof(*formats); ++index) {
      @c@_encode_options encode = @c@_default_encode_options(); encode.format = formats[index];
      @c@_decode_options decode = @c@_default_decode_options(); decode.format = formats[index];
      srl_buffer frame = {0}; srl_status native = srl_invalid;
      require(@c@_account_record_encode(&original, (srl_protocol)protocol, encode, &frame, &native) == @c@_ok && native == srl_ok, "typed encode");
      (void)snprintf(path, sizeof(path), "output-%zu-%d.bin", index, protocol); save(path, &frame);
      require(@c@_account_record_decode(&destination, frame.data, frame.size, (srl_protocol)protocol, NULL, decode, &native) == @c@_ok, "typed decode");
      require(destination.account_id == 42 && srl_string_equal(&destination.label, "owning \xf0\x9f\x98\x80"), "owning native value");
      srl_buffer trailing = {0}; require(srl_reserve(&trailing, plain.size+1), "trailing allocation");
      memcpy(trailing.data, plain.data, plain.size); trailing.data[plain.size] = 0; trailing.size = plain.size+1;
      require(@c@_compress(trailing.data, trailing.size, encode, &frame) == @c@_ok, "valid outer trailing");
      destination.account_id = 99;
      require(@c@_account_record_decode(&destination, frame.data, frame.size, (srl_protocol)protocol, NULL, decode, &native) == @c@_codec && native != srl_ok, "inner suffix rejected");
      require(destination.account_id == 99, "failed native publication");
      srl_buffer_free(&trailing); srl_buffer_free(&frame);
    }
    srl_buffer_free(&plain);
  }
  account_record_free(&original); account_record_free(&destination);
}

/* Check callback ownership, short I/O, deferred close, cancellation and sticky failure categories. */
static void lifecycle(void) {
  uint8_t plain[70001]; memset(plain, 65, sizeof(plain));
  const @c@_format formats[] = {@c@_none, @c@_gzip, @c@_zlib, @c@_deflate};
  for (size_t index = 0; index < sizeof(formats)/sizeof(*formats); ++index) {
    @c@_encode_options encode = @c@_default_encode_options(); encode.format = formats[index];
    @c@_decode_options decode = @c@_default_decode_options(); decode.format = formats[index];
    srl_buffer packed = {0}; require(@c@_compress(plain, sizeof(plain), encode, &packed) == @c@_ok, "lifecycle frame");
    for (int action = 0; action <= 5; ++action) {
      @c@_output output = {0}; transport sink_state = {0}; sink_state.output = &output;
      sink_state.action = action; encode.is_cancelled = cancelled; encode.context = &sink_state;
      require(@c@_output_init(&output, sink, &sink_state, encode) == @c@_ok, "output init");
      require(@c@_output_write(&output, plain, sizeof(plain)) == @c@_ok && !sink_state.calls, "staged output");
      const @c@_code expected = !action ? @c@_ok : action <= 2 ? @c@_transport : action == 5 ? @c@_cancelled : @c@_invalid_state;
      require(@c@_output_finish(&output) == expected, "finish action");
      const size_t calls = sink_state.calls;
      require(@c@_output_finish(&output) == (action ? expected : @c@_invalid_state), "output fenced");
      require(sink_state.calls == calls && sink_state.maximum <= @c@_batch_bytes, "no retried sink");
      @c@_output_close(&output); srl_buffer_free(&sink_state.bytes);
      @c@_input input = {0}; transport source_state = {0}; source_state.bytes = packed; source_state.input = &input;
      source_state.action = action; decode.is_cancelled = cancelled; decode.context = &source_state;
      require(@c@_input_init(&input, source, &source_state, decode) == expected, "input action");
      if (!action) {
        uint8_t decoded[sizeof(plain)]; size_t read = 0;
        require(@c@_input_read(&input, decoded, sizeof(decoded), &read) == @c@_ok && read == sizeof(plain), "input bytes");
        require(!memcmp(decoded, plain, sizeof(plain)), "input payload");
        require(@c@_input_finish(&input) == @c@_ok, "explicit input finish");
      }
      const size_t reads = source_state.calls; @c@_input_close(&input);
      require(source_state.calls == reads && source_state.maximum <= @c@_batch_bytes, "borrowed source retained");
    }
    transport source_state = {0}; source_state.bytes = packed; @c@_input input = {0};
    decode.is_cancelled = NULL; decode.context = NULL; decode.max_input_bytes = packed.size-1;
    require(@c@_input_init(&input, source, &source_state, decode) == @c@_resource_limit, "one-byte source probe");
    require(source_state.position == packed.size, "exact overflow probe extent"); @c@_input_close(&input);
    srl_buffer_free(&packed);
  }
}

/* Invalid active arguments, unread input, admission and abandoned wrappers preserve fencing. */
static void policy_tests(void) {
  const @c@_format formats[] = {@c@_none, @c@_gzip, @c@_zlib, @c@_deflate};
  for (size_t index = 0; index < sizeof(formats)/sizeof(*formats); ++index) {
    @c@_encode_options encode = @c@_default_encode_options(); encode.format = formats[index];
    @c@_decode_options decode = @c@_default_decode_options(); decode.format = formats[index];
    @c@_output output = {0}; transport target = {0};
    require(@c@_output_init(&output, sink, &target, encode) == @c@_ok, "abandoned init");
    @c@_output_close(&output); require(!target.calls, "abandon wrote trailer");
    memset(&output, 0, sizeof(output));
    require(@c@_output_init(&output, sink, &target, encode) == @c@_ok, "invalid argument init");
    require(@c@_output_write(&output, NULL, 1) == @c@_invalid_options, "invalid argument category");
    require(@c@_output_finish(&output) == @c@_invalid_options && !target.calls, "invalid argument fenced");
    @c@_output_close(&output);
    srl_buffer frame = {0}; require(@c@_compress((const uint8_t*)"x", 1, encode, &frame) == @c@_ok, "policy frame");
    @c@_input input = {0}; transport from = {0}; from.bytes = frame;
    require(@c@_input_init(&input, source, &from, decode) == @c@_ok, "unread init");
    require(@c@_input_finish(&input) == @c@_invalid_data, "unread expanded bytes");
    @c@_input_close(&input); memset(&input, 0, sizeof(input)); from.position = 0;
    require(@c@_input_init(&input, source, &from, decode) == @c@_ok, "cached input init");
    size_t actual = 99;
    require(@c@_input_read(&input, NULL, 1, &actual) == @c@_invalid_options && !actual, "invalid input span");
    require(@c@_input_finish(&input) == @c@_invalid_options, "invalid input fenced");
    @c@_input_close(&input); srl_buffer_free(&frame);
  }
}

/* Produce empty and multichunk controls at all selected native provider levels. */
static void levels(void) {
  uint8_t repeated[65537]; memset(repeated, 65, sizeof(repeated));
  const @c@_format formats[] = {@c@_none, @c@_gzip, @c@_zlib, @c@_deflate};
  const int selected_levels[] = {0, 1, 6, 9};
  for (size_t index = 0; index < sizeof(formats)/sizeof(*formats); ++index) {
    for (size_t level = 0; level < sizeof(selected_levels)/sizeof(*selected_levels); ++level) {
      for (unsigned sample = 0; sample < 2; ++sample) {
        @c@_encode_options policy = @c@_default_encode_options();
        policy.format = formats[index]; policy.level = selected_levels[level];
        const size_t count = sample ? sizeof(repeated) : 0;
        srl_buffer frame = {0}, exact = {0};
        require(@c@_compress(repeated, count, policy, &frame) == @c@_ok, "level output");
        char path[128]; (void)snprintf(path, sizeof(path), "level-%zu-%d-%u.bin", index, policy.level, sample);
        save(path, &frame); policy.max_output_bytes = frame.size;
        require(@c@_compress(repeated, count, policy, &exact) == @c@_ok && equal(&frame, &exact), "exact output cap");
        if (frame.size) {
          --policy.max_output_bytes;
          require(@c@_compress(repeated, count, policy, &exact) == @c@_resource_limit && equal(&frame, &exact), "one-tight output cap");
        }
        srl_buffer_free(&exact); srl_buffer_free(&frame);
      }
    }
  }
}

/* Require explicit absence before any callbacks, then exercise the selected provider profile. */
int main(void) {
  (void)fixtures; (void)models; (void)lifecycle; (void)policy_tests; (void)levels;
  require(!@c@_available(@c@_zstd) && !@c@_available(@c@_lz4), "optional absence");
#if defined(SERIALIZER_COMPRESSION_ZLIB) && SERIALIZER_COMPRESSION_ZLIB
  const size_t count = fixtures(); models(); lifecycle(); policy_tests(); levels();
  printf("{\"input_decisions\":%zu,\"independent_outputs\":48,\"provider_zlib\":\"%s\"}\n", count, zlibVersion());
#else
  srl_buffer result = {0}; @c@_encode_options encode = @c@_default_encode_options();
  require(@c@_compress((const uint8_t*)"abc", 3, encode, &result) == @c@_ok, "provider-free identity");
  require(result.size == 3 && !memcmp(result.data, "abc", 3), "identity bytes");
  encode.format = @c@_gzip;
  require(@c@_compress((const uint8_t*)"abc", 3, encode, &result) == @c@_unavailable, "provider-free absent");
  srl_buffer_free(&result); puts("{\"provider_free\":true}");
#endif
  return 0;
}
'''

REMOTE = r'''import base64,json,os,pathlib,subprocess,sys,tarfile,tempfile
with tempfile.TemporaryDirectory(prefix='serializer-c-compression-',dir='/var/tmp') as temporary:
 root=pathlib.Path(temporary)
 with tarfile.open(fileobj=sys.stdin.buffer,mode='r|gz') as archive:
  for member in archive:
   target=(root/member.name).resolve()
   if not member.isfile() or not target.is_relative_to(root):
    raise RuntimeError('Unexpected C SDK input member')
   target.parent.mkdir(parents=True,exist_ok=True)
   target.write_bytes(archive.extractfile(member).read())
 environment=os.environ.copy();environment['PATH']='/usr/local/bin:/usr/bin:/bin'
 command=[sys.argv[1],'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-O2','main.c','-o','consumer','-lm']
 if sys.argv[2]=='1': command+=['-DSERIALIZER_COMPRESSION_ZLIB=1','-lz']
 for job in (command,[str(root/'consumer')]):
  print('RUN '+json.dumps(job),flush=True)
  result=subprocess.run(job,cwd=root,env=environment,text=True,capture_output=True,timeout=180)
  print(result.stdout+result.stderr,end='',flush=True)
  if result.returncode: raise RuntimeError('Native C SDK failure: '+str(result.returncode))
 version=subprocess.check_output([sys.argv[1],'--version'],env=environment,text=True)
 files={path.name:base64.b64encode(path.read_bytes()).decode() for path in root.glob('*.bin')
        if path.name.startswith(('native-','output-','level-'))}
 print('C_RESULT '+json.dumps({'program':result.stdout,'cc':version,'files':files}),flush=True)
'''


def execute_wsl(directory, cc, enabled):
    """Stream exact generated inputs into a private Linux-local SDK directory and remove it on exit."""
    payload = io.BytesIO()
    with tarfile.open(fileobj=payload, mode='w:gz') as archive:
        for path in sorted(directory.iterdir()):
            if path.is_file() and path.suffix in ('.bin', '.plain', '.tsv', '.h', '.c', '.serializer'):
                archive.add(path, arcname=path.name)
    result = subprocess.run(['wsl.exe', '--cd', '/tmp', '--exec', '/usr/bin/python3', '-c',
                             REMOTE, cc, '1' if enabled else '0'], input=payload.getvalue(),
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
    text = result.stdout.decode(errors='replace')
    output, marker, report = text.partition('C_RESULT ')
    print(output, end='', flush=True)
    if result.returncode or not marker:
        raise RuntimeError('Native Linux C qualification failed: ' + str(result.returncode))
    data = json.loads(report)
    for name, encoded in data.pop('files').items():
        if Path(name).name != name:
            raise AssertionError('Unexpected native C SDK output name')
        (directory / name).write_bytes(base64.b64decode(encoded))
    return data['program'], data['cc'], hashlib.sha256(payload.getvalue()).hexdigest()


def run(command, directory, environment):
    """Retain actual tool commands and require successful SDK execution."""
    command = [str(value) for value in command]
    print('RUN', subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=directory, env=environment,
                            capture_output=True, text=True, timeout=300)
    print(result.stdout, end='', flush=True)
    if result.stderr:
        print(result.stderr, end='', flush=True)
    result.check_returncode()
    return result.stdout


def main():
    """Compile strict C11 with/without zlib and compare actual output to independent providers."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--cc')
    parser.add_argument('--wsl', action='store_true', help='Stream Windows-emitted sources to a native Linux-local C SDK')
    parser.add_argument('--zlib-include', type=Path)
    parser.add_argument('--zlib-library', type=Path)
    args = parser.parse_args()
    args.cc = args.cc or ('/usr/bin/cc' if args.wsl else shutil.which('cl') if os.name == 'nt' else shutil.which('cc'))
    if not args.cc:
        parser.error('An actual C11 compiler is required')
    msvc = Path(args.cc).name.lower() in ('cl', 'cl.exe')
    root, build = Path(__file__).resolve().parents[1], args.build.resolve()
    if build == root or root in build.parents and build.relative_to(root).parts[0] != 'out':
        parser.error('In-repository scratch must remain beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    if args.zlib_library:
        provider_bin = args.zlib_library.resolve().parent.parent / 'bin'
        environment['PATH'] = str(provider_bin) + os.pathsep + environment.get('PATH', '')
    profiles = {}
    with tempfile.TemporaryDirectory(prefix='actual-c-', dir=build) as temporary:
        directory = Path(temporary)
        count = common_fixtures(directory) + extra_fixtures(directory)
        for profile, schema, enabled, flags in (
                ('normal', SCHEMA, True, ()),
                ('provider_free', SCHEMA, False, ()),
                ('collision', SCHEMA + ' class serializer_compression {} class serializer_compression_2 {}', True, ()),
                ('enum_collision', SCHEMA + ' enum serializer { compression_format, compression_2_format }', True, ()),
                ('binary', SCHEMA, True, ('--protocols', 'binary_none'))):
            case = directory if profile == 'normal' else directory / profile
            case.mkdir(exist_ok=True)
            source = case / 'model.serializer'; source.write_text(schema, encoding='utf-8')
            header = case / 'schema.h'
            run([args.compiler.resolve(), '--input', source, '--language', 'c', '--output', header, *flags], case, environment)
            text = header.read_text(encoding='utf-8')
            match = re.search(r'Generated C compression prefix: (\w+)', text)
            if not match:
                raise AssertionError('Actual generated compression helper is absent')
            prefix = match[1]
            if profile in ('collision', 'enum_collision') and (prefix == 'serializer_compression' or not prefix.startswith('serializer_compression_')):
                raise AssertionError('Existing C model names were not preserved')
            consumer = CONSUMER.replace('@c@', prefix)
            if profile in ('collision', 'enum_collision', 'binary'):
                # Compile the complete helper/model profile and exercise its actual native codecs.
                consumer = consumer[:consumer.index('/* Require explicit absence')] + (
                    'int main(void) { (void)fixtures; (void)lifecycle; (void)policy_tests; (void)levels; models(); return 0; }\n')
            if profile == 'binary':
                consumer = consumer.replace('int protocol = srl_json; protocol <= srl_binary_string',
                                            'int protocol = srl_binary_none; protocol <= srl_binary_none')
            (case / 'main.c').write_text(consumer, encoding='utf-8')
            executable = case / ('consumer.exe' if os.name == 'nt' else 'consumer')
            command = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                       '-O2', case / 'main.c', '-o', executable]
            if msvc:
                command = [args.cc, '/nologo', '/std:c11', '/W4', '/WX', '/O2', '/utf-8',
                           '/D_CRT_SECURE_NO_WARNINGS', case / 'main.c',
                           '/Fe:' + str(executable), '/Fo:' + str(case / 'main.obj')]
            if enabled:
                command += ['/DSERIALIZER_COMPRESSION_ZLIB=1' if msvc else '-DSERIALIZER_COMPRESSION_ZLIB=1']
                if args.zlib_include:
                    command += ['/I' if msvc else '-I', args.zlib_include.resolve()]
                command += [args.zlib_library.resolve() if args.zlib_library else '-lz']
            if os.name != 'nt':
                command += ['-lm']
            if args.wsl:
                output, sdk_version, bundle_hash = execute_wsl(case, args.cc, enabled)
            else:
                run(command, case, environment)
                output = run([executable], case, environment)
            profiles[profile] = {'generated_bytes': header.stat().st_size,
                                 'generated_sha256': hashlib.sha256(header.read_bytes()).hexdigest()}
            if args.wsl:
                profiles[profile]['native_input_archive_sha256'] = bundle_hash
            if profile == 'normal':
                report = json.loads(output.strip().splitlines()[-1])
                profiles[profile]['zlib_provider_version'] = report.pop('provider_zlib')
                if report != {'input_decisions': count, 'independent_outputs': 48}:
                    raise AssertionError('Incomplete C input inventory')
                for protocol in range(4):
                    ordinary = (case / f'native-{protocol}.bin').read_bytes()
                    for index, bits in enumerate((None, 31, 15, -15)):
                        packed = (case / f'output-{index}-{protocol}.bin').read_bytes()
                        if bits is None:
                            plain = packed
                        else:
                            decoder = zlib.decompressobj(bits)
                            plain = decoder.decompress(packed) + decoder.flush()
                            if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                                raise AssertionError('C emitted an inexact outer frame')
                        if plain != ordinary:
                            raise AssertionError('Compression changed native C bytes')
                for index, bits in enumerate((None, 31, 15, -15)):
                    for level in (0, 1, 6, 9):
                        for sample in (0, 1):
                            packed = (case / f'level-{index}-{level}-{sample}.bin').read_bytes()
                            expected = b'A' * 65537 if sample else b''
                            if bits is None:
                                plain = packed
                            else:
                                decoder = zlib.decompressobj(bits)
                                plain = decoder.decompress(packed) + decoder.flush()
                                if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                                    raise AssertionError('C level output is not exactly one frame')
                            if plain != expected:
                                raise AssertionError('C level/empty output changed payload bytes')
    if not args.wsl:
        version = subprocess.run([args.cc, '/?' if msvc else '--version'], capture_output=True, text=True)
        sdk_version = (version.stdout + version.stderr).strip()
        if msvc:
            sdk_version = '\n'.join(line for line in sdk_version.splitlines() if 'Compiler Version' in line)
    report = {'profiles': profiles, 'input_decisions': count, 'independent_output_decisions': 48,
              'test_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              'cc': sdk_version, 'native_wsl': args.wsl,
              'independent_zlib': zlib.ZLIB_RUNTIME_VERSION}
    (build / 'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
