# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Check external Pack-IRI addressing with independent RFC3986 resolution and percent-encoding oracles."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys
from urllib.parse import quote, urljoin, urlsplit, uses_netloc, uses_relative

ROOT = Path(__file__).resolve().parent.parent
PREFIX = 'pack://https%3A,,example.org,package.opc'
BASE = PREFIX + '/b/c/d;p?q'


def invoke(probe, mode, *values):
    """Deliver arbitrary UTF-8 corpus bytes through ASCII hex, independent of the Windows argument codepage."""
    arguments = [str(probe), 'hex-' + mode]
    arguments.extend((item if isinstance(item, bytes) else item.encode('utf-8')).hex() for item in values)
    return subprocess.run(arguments, capture_output=True, timeout=15)


def text(result):
    """Require an actual successful native result; failure is never interpreted as an unavailable SDK skip."""
    if result.returncode:
        raise AssertionError(result.stderr.decode('utf-8', errors='replace'))
    return result.stdout.decode('utf-8').replace('\r\n', '\n')


def reference_compose(package, part):
    """Use the standard URL percent encoder; '%' is escaped before this independent slash-to-comma mapping."""
    package = package.split('#', 1)[0]
    authority = quote(package, safe="!$&'()*+;=/-._~").replace('/', ',')
    return 'pack://' + authority + (part or '/')


def execute(args):
    """Replay literal normative boundaries, complete RFC vectors and large repeated locator permutations."""
    output = args.build.resolve()
    if output == ROOT or (ROOT in output.parents and output.relative_to(ROOT).parts[0] not in ('out', 'build')):
        raise ValueError('In-repository output must stay inside an ignored build directory')
    output.mkdir(parents=True, exist_ok=True)
    if 'pack' not in uses_relative:
        uses_relative.append('pack')
    if 'pack' not in uses_netloc:
        uses_netloc.append('pack')
    packages = [
        'http://www.openxmlformats.org/my.container',
        'http://myalias:pswr@www.my.com/containers.aspx?my.container',
        'https://example.org/a,b%20c?x=%2F#removed',
        'https://[2001:db8::1]:443/package.opc',
        'file:///C:/models/package.opc', 'urn:example:package:42',
        'https://example.org/a?first=value&second=other',
        'https://example.org/a?next=https%3A%2F%2Fexample.net%2Fa',
    ]
    parts = ['', '/a.xml', '/a%3Ab.xml', '/a/b.foo', '/a!$&\'()+,;=@.xml']
    composition_count = 0
    for package in packages:
        for part in parts:
            actual = text(invoke(args.probe, 'compose', package, part))
            expected = reference_compose(package, part)
            if actual != expected:
                raise AssertionError(f'Independent composition mismatch: {actual!r} != {expected!r}')
            decoded = text(invoke(args.probe, 'parse', actual)).splitlines()
            if decoded != [package.split('#', 1)[0], part, 'absent:', 'absent:']:
                raise AssertionError(f'Component decode changed the resource: {decoded!r}')
            composition_count += 1
    unicode_package = 'https://example.org/模型.opc'
    actual = text(invoke(args.probe, 'compose', unicode_package, '/a.xml'))
    if text(invoke(args.probe, 'parse', actual)).splitlines()[0] != unicode_package:
        raise AssertionError('Unicode outer package locator failed UTF-8 round trip')
    # RFC3986 section5.4 normal and abnormal vectors; Python independently applies the reference algorithm.
    references = ['g:h', 'g', './g', 'g/', '/g', '//g', '?y', 'g?y', '#s', 'g#s', 'g?y#s',
                  ';x', 'g;x', 'g;x?y#s', '', '.', './', '..', '../', '../g', '../..', '../../',
                  '../../g', '../../../g', '../../../../g', '/./g', '/../g', 'g.', '.g', 'g..',
                  '..g', './../g', './g/.', 'g/./h', 'g/../h', 'g;x=1/./y', 'g;x=1/../y',
                  'g?y/./x', 'g?y/../x', 'g#s/./x', 'g#s/../x', 'http:g']
    for reference in references:
        actual = text(invoke(args.probe, 'resolve', BASE, reference))
        expected = urljoin(BASE, reference)
        if actual != expected:
            raise AssertionError(f'RFC resolution mismatch for {reference!r}: {actual!r} != {expected!r}')
    selectors = text(invoke(args.probe, 'parse', PREFIX + '/a.xml?outer?query#fragment/section')).splitlines()
    if selectors != ['https://example.org/package.opc', '/a.xml', 'present:outer?query', 'present:fragment/section']:
        raise AssertionError('Outer selectors were merged into the embedded package or part identity')
    empty_selectors = text(invoke(args.probe, 'parse', PREFIX + '/?#')).splitlines()
    if empty_selectors != ['https://example.org/package.opc', '', 'present:', 'present:']:
        raise AssertionError('Present-empty outer selectors lost their presence')
    invalid = ['pack:', 'pack://', 'pack:///a.xml', 'https://example.org/a.xml', 'pack://relative/a.xml',
               'pack://http:,,example.org/a.xml', 'pack://http%3c,,example.org/a.xml',
               PREFIX + '/a//b', PREFIX + '/a/../b', PREFIX + '/a.', PREFIX + '/a%2Fb',
               PREFIX + '/a%41b', PREFIX + '/a%', PREFIX + '/é.xml', PREFIX + '/%C3%A9.xml',
               'pack://pack%3A,,https%253A,,example.org/a.xml',
               'pack://https%3A,,example.org%23fragment/a.xml', PREFIX + '/a.xml#bad fragment',
               (PREFIX + '/a.xml').encode() + b'\x00', b'pack://http%3A,,example.org,\xff/a.xml']
    for value in invalid:
        if invoke(args.probe, 'parse', value).returncode != 2:
            raise AssertionError(f'Accepted or crashed on invalid/unsupported pack address: {value!r}')
    # Independent percent-decoding tripwire: the outer layer decodes exactly once, and never turns %252F into '/'.
    escaped = text(invoke(args.probe, 'parse', 'pack://https%3A,,example.org,a%252Fb/a.xml')).splitlines()
    if escaped[0] != 'https://example.org/a%2Fb':
        raise AssertionError('Pack authority was recursively percent-decoded')
    summary = {'composition_round_trips': composition_count, 'unicode_outer_round_trips': 1,
               'rfc3986_resolution_vectors': len(references), 'selector_controls': 2,
               'invalid_or_unsupported_addresses': len(invalid), 'single_decode_tripwires': 1,
               'python': sys.version, 'probe': str(args.probe.resolve()),
               'sources': ['ECMA-376 Part2 fifth edition clauses6.2-6.4', 'RFC3986 sections2.1 and5.4', 'RFC3987 section2.2']}
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: {composition_count} independent composition round trips, {len(references)} RFC vectors, '
          f'{len(invalid)} rejected addresses, Unicode/selectors/single-decode controls')


def main():
    """Suppress Windows child error dialogs and retain exact actual-SDK results in the selected build directory."""
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    execute(parser.parse_args())


if __name__ == '__main__':
    main()
