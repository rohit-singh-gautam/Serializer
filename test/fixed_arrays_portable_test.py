"""Qualify generated fixed-array owning codecs against native C++ bytes."""

import argparse
import importlib.util
from pathlib import Path
import shutil
import subprocess


def reject(callback, message=None):
    """Require malformed input or storage to fail before a value is returned."""
    try:
        callback()
    except ValueError as error:
        if message and message not in str(error):
            raise
    else:
        raise AssertionError("Invalid fixed array accepted")


def main():
    """Generate both portable backends, then compare all native wire protocols."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--native", required=True)
    parser.add_argument("--node", required=True)
    parser.add_argument("--build", type=Path, required=True)
    arguments = parser.parse_args()
    directory = arguments.build.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parent
    schema = source / "resources/fixed_arrays_portable.serializer"
    for language, filename in (("js", "schema.mjs"), ("typescript", "schema.d.mts"),
                               ("python", "schema.py")):
        subprocess.run([arguments.compiler, "--input", str(schema), "--language", language,
                        "--output", str(directory / filename)], check=True)
    subprocess.run([arguments.native, str(directory)], check=True)
    shutil.copyfile(source / "fixed_arrays_portable_test.mjs", directory / "test.mjs")
    subprocess.run([arguments.node, str(directory / "test.mjs"), str(directory)], check=True)
    spec = importlib.util.spec_from_file_location("fixed_array_schema", directory / "schema.py")
    model = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(model)
    for protocol in model.Protocol:
        for alternative in range(3):
            expected = (directory / f"{alternative}-{int(protocol)}.bin").read_bytes()
            value = model.Fixture.decode(expected, protocol)
            assert value.encode(protocol) == expected
            fresh = model.Fixture()
            fresh.short_hash.bytes = list(range(1, 21))
            fresh.long_hash.bytes = list(range(21, 53))
            fresh.nested[0].value, fresh.nested[1].value = 41, 42
            fresh.commit_index = alternative
            fresh.commit_sha1.bytes = fresh.short_hash.bytes.copy()
            fresh.commit_sha256.bytes = fresh.long_hash.bytes.copy()
            assert fresh.encode(protocol) == expected
        for count in (0, 20, 31, 33):
            variable = model.VariableBytes()
            variable.bytes = [1] * count
            reject(lambda: model.FixedBytes.decode(variable.encode(protocol), protocol), "extent")
            invalid = model.FixedBytes()
            invalid.bytes = [1] * count
            reject(lambda: invalid.encode(protocol), "extent")
        value = model.FixedBytes()
        encoded = value.encode(protocol)
        assert len(model.FixedBytes.decode(encoded, protocol).bytes) == 32
        for end in range(len(encoded)):
            reject(lambda: model.FixedBytes.decode(encoded[:end], protocol))
        reject(lambda: model.FixedBytes.decode(encoded, protocol, model.Limits(max_elements=31)))
    reject(lambda: model.FixedBytes.decode(bytes((33,)), model.Protocol.BINARY_NONE), "extent")
    assert len(model.FixedBytes.decode(b"{}", model.Protocol.JSON).bytes) == 32
    defaults, separate = model.Fixture(), model.Fixture()
    defaults.nested[0].value = 99
    assert defaults.nested[1].value == 7 and separate.nested[0].value == 7
    reject(lambda: model.FixedBytes.decode(b'{"bytes":[' + b'1,' * 32 + b'{}]}',
                                          model.Protocol.JSON), "extent")
    print("C++/Python fixed-array wire, defaults, union, and rejection checks passed")


if __name__ == "__main__":
    main()
