# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exchange generated C++ fixed arrays with Google's independently installed Protobuf runtime."""
import argparse
from pathlib import Path
import subprocess
from google.protobuf import __version__, descriptor_pb2, descriptor_pool, json_format, message_factory, text_format


def message_type(packed):
    """Build the ordinary repeated-uint32 descriptor independently of Serializer's schema compiler."""
    source = descriptor_pb2.FileDescriptorProto(name='fixed_array_sdk.proto', package='sdk', syntax='proto3')
    record = source.message_type.add(name='ScalarArray')
    field = record.field.add(name='values', number=1, type=descriptor_pb2.FieldDescriptorProto.TYPE_UINT32,
                             label=descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED)
    field.options.packed = packed
    pool = descriptor_pool.DescriptorPool()
    pool.Add(source)
    return message_factory.GetMessageClass(pool.FindMessageTypeByName('sdk.ScalarArray'))


def main():
    """Check both producers, packed/unpacked arrays and valid-SDK cardinalities outside the fixed contract."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    values = [0, 150, 300]
    checks = 0
    for packed in (False, True):
        message = message_type(packed)
        for protocol in ('binary', 'json', 'text'):
            def encode(value):
                """Serialize through Google's runtime using the selected official wire/text format."""
                if protocol == 'binary':
                    return value.SerializeToString()
                return (json_format.MessageToJson(value) if protocol == 'json' else text_format.MessageToString(value)).encode()

            path = args.build / (protocol + '-' + str(packed))
            path.write_bytes(encode(message(values=values)))
            subprocess.run([str(args.probe), 'decode', protocol, str(path)], check=True, timeout=30)
            subprocess.run([str(args.probe), 'encode', protocol, str(path)], check=True, timeout=30)
            result = message()
            if protocol == 'binary':
                result.ParseFromString(path.read_bytes())
            elif protocol == 'json':
                json_format.Parse(path.read_text(), result)
            else:
                text_format.Parse(path.read_text(), result)
            assert list(result.values) == values
            checks += 2
            for count in (0, 1, 2, 4):
                path.write_bytes(encode(message(values=list(range(count)))))
                rejected = subprocess.run([str(args.probe), 'decode', protocol, str(path)], capture_output=True, timeout=30)
                if rejected.returncode == 0:
                    raise AssertionError('Fixed array accepted a valid SDK message with the wrong cardinality')
                checks += 1
    print(f'PASS: Google Protobuf {__version__}, {checks} independent binary/ProtoJSON/TextFormat exchanges and cardinality decisions')


if __name__ == '__main__':
    main()
