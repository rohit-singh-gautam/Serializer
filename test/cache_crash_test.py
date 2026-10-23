"""Qualify cache atomic selection and graceful-close retention with real process exits."""

import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile


def run(arguments, expected=0):
    """Run a bounded child without a shell; injected termination has a distinct exit status."""
    result = subprocess.run([str(value) for value in arguments], capture_output=True,
                            text=True, timeout=30, check=False)
    assert result.returncode == expected, (arguments, result.returncode, result.stdout, result.stderr)
    return result.stdout.strip()


def verify_cache(directory, value):
    """Independently verify fixed wire words, exact payload/key extents and both SHA-256 bindings."""
    entries = list(directory.glob('*.srcache'))
    assert len(entries) == 1
    encoded = entries[0].read_bytes()
    assert len(encoded) == 216 + len(value)
    assert struct.unpack_from('<4Q', encoded) == (0x3145484341435253, 1, 200, len(value))
    assert hashlib.sha256(encoded[:-32]).digest() == encoded[-32:]
    assert hashlib.sha256(encoded[32:184]).hexdigest() + '.srcache' == entries[0].name
    assert encoded[184:-32] == value


def main():
    """Exercise independent cache writes and native/external close publication and retirement."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--cache-helper', required=True)
    parser.add_argument('--journal-helper', required=True)
    parser.add_argument('--directory', required=True)
    args = parser.parse_args()
    root = Path(args.directory)
    root.mkdir(parents=True, exist_ok=True)
    checks = 0
    with tempfile.TemporaryDirectory(prefix='cache-crash-', dir=root) as temporary:
        directory = Path(temporary)
        for point in ('before_stage', 'after_stage', 'before_publish', 'after_publish'):
            cache = directory / point
            run([args.cache_helper, cache, 'init', 'none'])
            run([args.cache_helper, cache, 'update', point], 86)
            expected = 2 if point == 'after_publish' else 1
            assert run([args.cache_helper, cache, 'read', 'none']) == str(expected)
            verify_cache(cache, bytes([expected]))
            assert not list(cache.glob('*.tmp'))
            checks += 1
        cache = directory / 'invalidate'
        run([args.cache_helper, cache, 'init', 'none'])
        run([args.cache_helper, cache, 'invalidate', 'before_remove'], 86)
        assert run([args.cache_helper, cache, 'read', 'none']) == '1'
        checks += 1
        for mode in ('appended', 'sidecar', 'document'):
            points = ['before_retire', 'after_base_flush', 'before_replace', 'after_replace',
                      'after_publish', 'before_cleanup', 'after_retire']
            if mode == 'document':
                points.append('after_sidecar_flush')
            for point in points:
                path = directory / f'{mode}-{point}'
                run([args.journal_helper, path, mode, 'create', 'before_append'])
                run([args.journal_helper, path, mode, 'edit', 'before_append'])
                run([args.journal_helper, path, mode, 'close', point], 86)
                cache = path.parent / ('.' + path.name + '.serializer') / 'cache'
                verify_cache(cache, bytes([1, 2, 3]))
                restored = run([args.journal_helper, path, mode, 'inspect', 'before_append']).split()
                assert restored[0] == 'Changed' and restored[1] == '1' and restored[3] == '1', restored
                verify_cache(cache, bytes([1, 2, 3]))
                checks += 1
    print(f'{checks} cache/close process-exit boundaries passed with independent cache wire checks')


if __name__ == '__main__':
    main()
