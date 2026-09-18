"""Qualify new native codecs with shared valid, hostile, truncated, and bounded inputs."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('examples', ROOT / 'example/run.py')
examples = importlib.util.module_from_spec(spec)
spec.loader.exec_module(examples)
TYPES = ('InteropMessage', 'CheckTextValue', 'CheckIntegerValue', 'CheckBoolValue',
         'CheckFloatValue', 'CheckBytesValue', 'CheckEmpty', 'CheckRecursive',
         'CheckVisibility', 'CheckDefaultValue')
DEFAULT_LIMITS = (64*1024*1024, 16*1024*1024, 1000000, 64)


def corpus(directory, schema):
    """Create independent edge cases and truncate every byte of a complete message."""
    cases = []

    def add(type_id, data, protocol=0, expected=None, limits=DEFAULT_LIMITS):
        index = len(cases)
        if isinstance(data, str):
            data = data.encode('utf-8')
        (directory / f'{index}.bin').write_bytes(bytes(data))
        cases.append((type_id, protocol, limits, expected))

    for protocol in schema.Protocol:
        value = schema.InteropMessage()
        encoded = value.encode(protocol)
        add(0, encoded, protocol, {'big_unsigned': 18446744073709551615})
        for length in range(len(encoded)):
            add(0, encoded[:length], protocol)
        add(0, encoded + b'\0', protocol)
        add(0, encoded, protocol, limits=(len(encoded)-1, *DEFAULT_LIMITS[1:]))
        add(0, encoded, protocol, limits=(*DEFAULT_LIMITS[:3], 0))
    add(3, [2], 1)
    add(3, '{"value":null}')
    for value in ('01', '-1', '18446744073709551616', '1e3', '+1', '1.0', 'true'):
        add(2, '{"value":' + value + '}')
    for data in ('{"value":1,}', '{"unknown":1}', '\ufeff{"value":1}', '{"value":1}x'):
        add(2, data)
    for value in ('NaN', '1e100', '1e-100', '+1', 'Infinity'):
        add(4, '{"value":' + value + '}')
    for value in ('"\\ud800"', '"\\udc00"', '"\\ud800x"', '"x\n"'):
        add(1, '{"value":' + value + '}')
    for data in ([2, 0xc0, 0xaf], [3, 0xed, 0xa0, 0x80], [4, 0xf4, 0x90, 0x80, 0x80]):
        add(1, data, 1)
    add(1, '{"value":"abcdef"}', limits=(100, 3, 100, 64))
    add(1, '{"value":"\\u0061"}', limits=(100, 3, 100, 64))
    add(5, [255]*4, 1)
    add(5, [3, 1, 2], 1)
    add(5, '{"values":[1,2,3]}', limits=(100, 100, 2, 64))
    add(5, '{"values":[1,]}')
    for data in ('{"payload:missing":0}', '{"states":["missing"]}', '{"counts":[{"key":"a"}]}',
                 '{"counts":[{"value":1}]}', '{"counts":[{"key":"a","value":1,"extra":0}]}',
                 '{"counts":[{"key":"a","value":1},]}'):
        add(0, data)
    add(2, '{"value":18446744073709551615}', expected={'value':18446744073709551615})
    add(1, [0x40, 0], 1, {'value':''})
    add(1, '{"value":"\\ud83d\\ude80"}', expected={'value':'🚀'})
    add(1, [3, 0xef, 0xbb, 0xbf], 1, {'value':'\ufeff'})
    # U+FEFF is string data, including at the start; it is never an ignorable field/token prefix.
    for text in ('\ufefftext', '\ufeff\ufeff'):
        encoded = text.encode('utf-8')
        payload = bytes((len(encoded),)) + encoded
        add(1, payload, 1, {'value':text})
        add(1, b'\x01' + payload + b'\0', 2, {'value':text})
        add(1, b'\x05value' + payload + b'\0', 3, {'value':text})
    add(1, '{"value":"\ufefftext"}', expected={'value':'\ufefftext'})
    add(1, '{"value":"\\ufefftext"}', expected={'value':'\ufefftext'})
    add(1, '{"\ufeffvalue":"text"}')
    add(2, '{"value":\ufeff1}')
    add(1, '{"value":"a\\u0000b"}', expected={'value':'a\0b'})
    add(0, '{"nested":{"code":99},"nested":{"note":"merged"},"numbers":[1,2],"numbers":[3],"counts":[{"value":1,"key":"a"},{"key":"a","value":2}]}',
        expected={'nested':{'code':99,'note':'merged'}, 'numbers':[3], 'counts':[{'key':'a','value':2}]})
    add(0, '{"indexed":[{"key":1,"value":{"code":99,"note":"old"}},{"value":{"note":"new"},"key":1}]}',
        expected={'indexed':[{'key':1,'value':{'code':7,'note':'new'}}]})
    add(0, '{"counts":[{"key":"é","value":1},{"key":"e\\u0301","value":2}]}',
        expected={'counts':[{'key':'e\u0301','value':2},{'key':'é','value':1}]})
    add(0, '{} \r\n\t', expected={'big_unsigned':18446744073709551615})
    add(9, '{}', expected={'nul':'\0','escaped':'a\\b\n"c'})
    add(8, '{}', expected={'hidden':'secret','counter':42,'visible':7})
    add(6, '{}', expected={})
    add(6, [], 1, {})
    add(7, '{"children":[{"children":[]}]}', limits=(100,100,100,1))
    add(7, '{"children":['*66 + '{"children":[]}' + ']}'*66)
    manifest = ''.join(f'{i}\t{kind}\t{int(protocol)}\t' + '\t'.join(map(str,limits)) + '\n'
                       for i,(kind,protocol,limits,_) in enumerate(cases))
    (directory / 'manifest.tsv').write_text(manifest)
    return cases


def contains(actual, expected):
    """Check independent expected fields without deriving expectations from generated code."""
    if isinstance(expected, dict):
        return isinstance(actual, dict) and all(k in actual and contains(actual[k], v) for k,v in expected.items())
    return actual == expected


def main():
    """Build each actual SDK and require identical acceptance and data preservation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/native-tests')
    parser.add_argument('--language', default='python,rust,swift,kotlin,c')
    parser.add_argument('--wsl-languages', default='')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args(); args.cpp_library = None
    args.build = args.build.resolve(); args.compiler = args.compiler.resolve()
    languages = args.language.split(',')
    runner = examples.Runner(args)
    outputs = {language:args.build / language / examples.OUTPUTS[language] for language in dict.fromkeys(['python', *languages])}
    command = [args.compiler, '--input', ROOT / 'test/portable/coverage.serializer', '--language', ','.join(outputs), '--kotlin.package=']
    for language, output in outputs.items():
        output.parent.mkdir(parents=True, exist_ok=True); command += [f'--{language}.output', output]
    examples.run(command)
    spec = importlib.util.spec_from_file_location('schema', outputs['python'])
    schema = importlib.util.module_from_spec(spec); spec.loader.exec_module(schema)
    directory = args.build / 'corpus'; directory.mkdir(parents=True, exist_ok=True)
    cases = corpus(directory, schema)
    for language in languages:
        executable = runner.build(language, 'codecs', args.build / language, ROOT / 'test/native' / language)
        output = args.build / language / 'results'; output.mkdir(parents=True, exist_ok=True)
        examples.run([*executable, runner.path(language, directory), runner.path(language, output)])
        statuses = (output / 'results.txt').read_text().splitlines()
        if len(statuses) != len(cases):
            raise AssertionError(f'{language}: missing test results')
        for index, ((_, _, _, expected), status) in enumerate(zip(cases, statuses)):
            if (status == 'OK') != (expected is not None):
                raise AssertionError(f'{language} case {index}: expected {expected}, status {status}, input {(directory / f"{index}.bin").read_bytes()!r}')
            if expected is not None:
                actual = json.loads((output / f'{index}.json').read_text(encoding='utf-8'))
                if not contains(actual, expected):
                    raise AssertionError(f'{language} case {index}: expected {expected}, got {actual}')
        print(f'PASS: {language}, {len(cases)} boundary cases', flush=True)


if __name__ == '__main__':
    main()
