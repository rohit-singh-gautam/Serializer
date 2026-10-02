"""Import a JSON document, edit its typed model, and round-trip four protocols."""
from pathlib import Path
import sys
from schema import ExampleModel, Protocol


def main():
    """Check complete positional bytes after each protocol round trip."""
    value = ExampleModel.decode(Path(sys.argv[1]).read_bytes(), Protocol.JSON)
    value.revision += 1
    # Edit the uint32 key inside envelope<box<pair<uint32, string>>>.
    value.payload.value.key = 7
    canonical = value.encode(Protocol.BINARY_NONE)
    for protocol in Protocol:
        copy = ExampleModel.decode(value.encode(protocol), protocol)
        assert copy.encode(Protocol.BINARY_NONE) == canonical
    Path(sys.argv[2]).write_bytes(value.encode(Protocol.JSON))
    print('Generic model: four protocols passed')


if __name__ == '__main__':
    main()
