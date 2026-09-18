"""Execute a common corpus with strict fresh-value decode and independent expectations."""
from pathlib import Path
import sys
from schema import InteropMessage, CheckTextValue, CheckIntegerValue, CheckBoolValue, CheckFloatValue, CheckBytesValue, CheckEmpty, CheckRecursive, CheckVisibility, CheckDefaultValue, Protocol, Limits

TYPES = (InteropMessage, CheckTextValue, CheckIntegerValue, CheckBoolValue, CheckFloatValue, CheckBytesValue, CheckEmpty, CheckRecursive, CheckVisibility, CheckDefaultValue)


def main():
    """Keep file I/O outside the exception path reserved for codec rejection."""
    directory, output = map(Path, sys.argv[1:])
    statuses = []
    for line in (directory / 'manifest.tsv').read_text().splitlines():
        index, kind, protocol, *limits = map(int, line.split())
        data = (directory / f'{index}.bin').read_bytes()
        try:
            # Borrow a nonzero-offset input slice to catch accidental whole-buffer reads.
            view = memoryview(b'prefix' + data + b'suffix')[6:6+len(data)]
            value = TYPES[kind].decode(view, Protocol(protocol), Limits(*limits))
            result = value.encode(Protocol.JSON)
        except (ValueError, TypeError, OverflowError):
            statuses.append('ERR')
        else:
            statuses.append('OK')
            (output / f'{index}.json').write_bytes(result)
    (output / 'results.txt').write_text('\n'.join(statuses)+'\n')
    # Dynamic objects also reject invalid values, cycles, and malformed Unicode on encode.
    cyclic = CheckRecursive(); cyclic.children.append(cyclic)
    invalid = CheckIntegerValue(); invalid.value = 1 << 64
    text = CheckTextValue(); text.value = '\ud800'
    for value in (cyclic, invalid, text):
        try:
            value.encode(Protocol.JSON)
        except (ValueError, TypeError, OverflowError):
            pass
        else:
            raise AssertionError('Invalid value encoded')


if __name__ == '__main__':
    main()
