"""Import a JSON document, edit its typed model, and round-trip four protocols."""
from pathlib import Path
import sys
from schema import ExampleModel, Protocol


def main():
    """Check complete positional bytes after each protocol round trip."""
    defaults = ExampleModel()
    assert [len(defaults.pages), len(defaults.columns), len(defaults.headings)] == [2, 3, 2]
    defaults.pages[0].title = 'First'
    assert defaults.pages[1].title == ''
    value = ExampleModel.decode(Path(sys.argv[1]).read_bytes(), Protocol.JSON)
    value.lookup_cache = 77
    value.revision += 1
    canonical = value.encode(Protocol.BINARY_NONE)
    for protocol in Protocol:
        copy = ExampleModel.decode(value.encode(protocol), protocol)
        assert copy.lookup_cache == 0
        assert copy.encode(Protocol.BINARY_NONE) == canonical
    Path(sys.argv[2]).write_bytes(value.encode(Protocol.JSON))
    print('Four protocols passed')


if __name__ == '__main__':
    main()
