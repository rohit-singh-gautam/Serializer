# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Qualify CMake-generated C++ presence codecs against the shared independent XSD corpus."""
import argparse
from pathlib import Path
import subprocess

from lxml import etree

import portable_xml_presence_test as shared


def main():
    """Compare every decision/output with lxml and verify rejected bindings preserve existing files."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.build == shared.ROOT or shared.ROOT in args.build.parents and 'out' not in args.build.relative_to(shared.ROOT).parts:
        parser.error('Repository output must remain below the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    oracles = {kind: etree.XMLSchema(etree.fromstring(source.encode())) for kind, source in (
        ('base', shared.XSD), ('rich', shared.RICH_XSD), ('all', shared.ALL_XSD))}
    paths = []
    for index, (kind, text, valid) in enumerate(shared.cases() + shared.rich_cases() + shared.all_cases()):
        oracle = oracles[kind if kind in ('rich', 'all') else 'base']
        if oracle.validate(etree.fromstring(text.encode())) != valid:
            raise AssertionError(f'Independent presence decision differs at {index}: {oracle.error_log}')
        path = args.build / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(text, encoding='utf-8')
        Path(str(path) + '.cpp.xml').unlink(missing_ok=True)
        paths.append((path, oracle, valid))
    subprocess.run([str(args.executable.resolve()), *(str(path) for path, _, _ in paths)], check=True)
    for path, oracle, valid in paths:
        output = Path(str(path) + '.cpp.xml')
        if valid:
            oracle.assertValid(etree.parse(str(output)))
        elif output.exists():
            raise AssertionError(f'Rejected C++ document published an output: {output}')
    rejected = shared.binding_rejections(args, ['cpp'])
    print(f'PASS: {len(paths)} C++ XSD presence probes and {rejected} transactional binding rejections', flush=True)


if __name__ == '__main__':
    main()
