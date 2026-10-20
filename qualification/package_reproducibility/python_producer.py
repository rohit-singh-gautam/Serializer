# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Use a public generated OPC facade and a separately labeled physical ZIP diagnostic helper."""
import json
from pathlib import Path
import sys
import zlib

import schema as generated


def provider():
    """Identify the actual Python compressor with its existing default compression settings."""
    return {'implementation': 'zlib', 'version': zlib.ZLIB_RUNTIME_VERSION,
            'options': {'level': -1, 'window_bits': -15, 'memory_level': 8, 'strategy': 0},
            'metadata_method': 8}


def rows(path, columns):
    """Read literal UTF-8 TSV without interpreting URI characters or empty payload cells."""
    lines = path.read_text(encoding='utf-8').splitlines()
    result = [line.split('\t') for line in lines]
    if any(len(row) != columns for row in result):
        raise ValueError('Invalid fixture column count')
    return result


def permute(values, selected):
    """Apply the frozen independent permutations without sorting caller-supplied sets."""
    indexes = list(range(len(values)))
    if selected == 1:
        indexes.reverse()
    elif selected in (2, 3):
        shift = 1 if selected == 2 else len(values) // 2
        indexes = indexes[shift:] + indexes[:shift]
    elif selected == 4:
        indexes = indexes[::2] + indexes[1::2]
    elif selected == 5:
        indexes = indexes[1::2][::-1] + indexes[::2][::-1]
    return [values[index] for index in indexes]


def main():
    """Emit a completed archive and report the actual profile/provider used by this fresh process."""
    if sys.argv[1:] == ['--provider']:
        print(json.dumps(provider(), sort_keys=True))
        return
    if len(sys.argv) not in (5, 6):
        raise ValueError('Expected fixtures, profile, permutation, output and optional ZIP64 flag')
    directory, profile, selector, destination = sys.argv[1:5]
    selected = int(selector)
    wide = sys.argv[5] if len(sys.argv) == 6 else '0'
    if profile not in ('zip-stored', 'zip-deflate', 'opc-stored', 'opc-deflate') or selected not in range(6) or wide not in ('0', '1'):
        raise ValueError('Invalid reproducibility profile, permutation or ZIP64 flag')
    opc = profile.startswith('opc-')
    fixture = Path(directory) / ('opc' if opc else 'zip')
    compression = 'deflate' if profile.endswith('deflate') else 'stored'
    parts = permute(rows(fixture / 'parts.tsv', 4), selected)
    links = permute(rows(fixture / 'links.tsv', 5), selected)
    limits = generated.PackageLimits(force_zip64=wide == '1')
    if opc:
        extensions = generated.PreservedExtensions(
            tuple(generated.BinaryPart(bytes.fromhex(row[3]), row[0], row[1], compression=compression) for row in parts),
            tuple(generated.PackageRelationship(*row) for row in links))
        value = generated.ReproducibilityArchive.Input(extensions=extensions)
        generated.ReproducibilityArchive.save(destination, value, limits=limits)
        entrypoint = 'public schema-bound OPC Save'
    else:
        # Python currently exposes no generic public ZIP writer; this probes the generated physical helper only.
        payloads = [('/' + row[0], bytes.fromhex(row[3]), compression) for row in parts]
        data = generated._pkg_zip_write(payloads, generated.PackageBudget(limits))
        Path(destination).write_bytes(data)
        entrypoint = 'diagnostic generated private ZIP helper'
    print(json.dumps({'profile': profile, 'permutation': selected, 'force_zip64': wide == '1',
                      'parts': len(parts), 'relationships': len(links), 'provider': provider(), 'entrypoint': entrypoint}, sort_keys=True))


if __name__ == '__main__':
    main()
