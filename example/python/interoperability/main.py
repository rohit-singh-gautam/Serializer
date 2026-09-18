"""Exchange every native protocol using the shared generated schema."""
from pathlib import Path
import sys
import time
from schema import InteropMessage, InteropDetail, InteropState, Protocol


def fixture(variant):
    """Construct expected values independently of other language implementations."""
    value = InteropMessage()
    value.text = 'Ada "Lovelace" 🚀\n' + 'x' * 64
    value.numbers = [-2147483648, -1, 0, 2147483647]
    value.decimals = [-0.0, 1.5, -2.25]
    value.flags = [False, True]
    value.labels = ['', 'é', '🚀']
    child = InteropDetail()
    child.code, child.note = -9, 'child'
    value.children = [InteropDetail(), child]
    value.states = [InteropState.Paused, InteropState.Ready]
    value.counts = {'🚀': 18446744073709551615, 'é': 7, 'a': 0}
    value.indexed = {18446744073709551615: child, 0: InteropDetail()}
    value.toggles = {True: 'yes', False: 'no'}
    value.enums = {InteropState.Paused: -2, InteropState.Ready: 1}
    value.payload_index = variant
    value.payload_number = -1234567890123456789
    value.payload_ratio = -3.5
    value.payload_state = InteropState.Paused
    return value


def exchange(directory, mode):
    """Emit fixtures or verify full data and canonical binary bytes from every producer."""
    directory.mkdir(parents=True, exist_ok=True)
    languages = (directory / 'producers.txt').read_text().split()
    for variant in range(3):
        expected = fixture(variant)
        canonical = expected.encode(Protocol.BINARY_NONE)
        for protocol in Protocol:
            encoded = expected.encode(protocol)
            if mode == 'emit':
                (directory / f'python_{protocol.name}_{variant}.bin').write_bytes(encoded)
            else:
                for language in languages:
                    data = (directory / f'{language}_{protocol.name}_{variant}.bin').read_bytes()
                    actual = InteropMessage.decode(data, protocol)
                    assert actual.encode(Protocol.BINARY_NONE) == canonical, (language, protocol, variant)
                    if protocol != Protocol.JSON:
                        assert data == encoded, 'Canonical binary mismatch'
    print(f'python {mode} passed')


def benchmark():
    """Report local encode/decode samples without claiming cross-runtime equivalence."""
    value, iterations = fixture(0), 10000
    for protocol in Protocol:
        encoded = value.encode(protocol)
        for _ in range(1000):
            InteropMessage.decode(value.encode(protocol), protocol)
        start = time.perf_counter()
        for _ in range(iterations):
            value.encode(protocol)
        encode_seconds = time.perf_counter() - start
        start = time.perf_counter()
        for _ in range(iterations):
            InteropMessage.decode(encoded, protocol)
        print(f'python {protocol.name}: {len(encoded)} bytes; encode {iterations/encode_seconds:.0f}/s; decode {iterations/(time.perf_counter()-start):.0f}/s')


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[2] not in ('emit', 'verify', 'benchmark'):
        raise SystemExit('Usage: python main.py <fixtures-directory> emit|verify|benchmark')
    if sys.argv[2] == 'benchmark':
        benchmark()
    else:
        exchange(Path(sys.argv[1]), sys.argv[2])
