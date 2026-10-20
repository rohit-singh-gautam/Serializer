# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Independent RFC1950 advertised-history controls shared by the parity and Java runners."""
import zlib
import struct
import json



def small_zlib_windows(directory):
    """Write exact/one-beyond history and one-tight expansion controls for all seven small windows."""
    from csharp_compression_test import bits_frame, fixed_symbol, wire_bits
    distance_base = (1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                     193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                     6145, 8193, 12289, 16385, 24577)
    distance_extra = (0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13)
    rows = []

    def add(stem, raw, plain, exponent, valid):
        """Verify independent raw decoding and retain exact limits plus one-tight expansion for positives."""
        if zlib.decompress(raw, -15) != plain:
            raise AssertionError('Independent RFC1950 DEFLATE body differs')
        header = (exponent << 4) | 8
        frame = bytes((header, (-header * 256) % 31)) + raw + zlib.adler32(plain).to_bytes(4, 'big')
        (directory/(stem+'.bin')).write_bytes(frame)
        (directory/(stem+'.plain')).write_bytes(plain)
        rows.append((stem+'.bin', stem+'.plain', 'ok' if valid else 'invalid_data',
                     len(frame), len(plain), 8*1024*1024))
        if valid and plain:
            rows.append((stem+'.bin', stem+'.plain', 'resource_limit', len(frame), len(plain)-1, 8*1024*1024))

    for exponent in range(7):
        for extra in (0, 1):
            distance = (1 << (exponent + 8)) + extra
            prefix = bytes(index % 251 for index in range(distance))
            plain = prefix + prefix[:3]
            code = max(index for index, base in enumerate(distance_base) if base <= distance)
            body = ([1, 1, 0] + [bit for value in prefix for bit in fixed_symbol(value)] +
                    fixed_symbol(257) + [(code >> bit) & 1 for bit in reversed(range(5))] +
                    wire_bits(distance - distance_base[code], distance_extra[code]) + fixed_symbol(256))
            stem = 'window-' + str(exponent) + '-' + str(extra)
            add(stem, bits_frame(body), plain, exponent, not extra)
            # The complete code-length alphabet 0/1/2/18 describes an A/EOB/match tree.
            order = (16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1)
            header = [1, 0, 1] + wire_bits(1, 5) + wire_bits(code, 5) + wire_bits(14, 4)
            header += [bit for value in order for bit in wire_bits(2 if value in (0, 1, 2, 18) else 0, 3)]
            lengths = [1 if index == 65 else 2 if index in (256, 257) else 0 for index in range(258)]
            lengths += [int(index == code) for index in range(code + 1)]
            dynamic = bits_frame(header + [bit for value in lengths for bit in ((value >> 1) & 1, value & 1)] +
                                 [0]*distance + [1, 1, 0] + wire_bits(distance-distance_base[code], distance_extra[code]) + [1, 0])
            add('dynamic-window-'+str(exponent)+'-'+str(extra), dynamic, b'A'*(distance+3), exponent, not extra)
    for distance in (256, 257):
        prefix = bytes(index & 255 for index in range(distance))
        code = max(index for index, base in enumerate(distance_base) if base <= distance)
        raw = b'\0' + struct.pack('<HH', distance, distance ^ 65535) + prefix
        raw += bits_frame([1, 1, 0] + fixed_symbol(257) + [(code >> bit) & 1 for bit in reversed(range(5))] +
                          wire_bits(distance-distance_base[code], distance_extra[code]) + fixed_symbol(256))
        add('stored-fixed-'+str(distance), raw, prefix+prefix[:3], 0, distance == 256)
    plain = bytes(range(256))*3
    add('stored-without-matches', b'\1'+struct.pack('<HH', len(plain), len(plain) ^ 65535)+plain, plain, 0, True)
    order = (16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1)
    header = [1, 0, 1] + [0]*10 + wire_bits(14, 4)
    header += [bit for value in order for bit in wire_bits(int(value in (0, 1)), 3)]
    for literal in (False, True):
        lengths = [int(index == 256 or literal and index == 65) for index in range(258)]
        add('unused-distance-'+str(int(literal)), bits_frame(header+lengths+([0, 1] if literal else [0])),
            b'A' if literal else b'', 0, True)

    def repeat_codes(raw):
        """Independently inventory code-length repeat symbols in a provider-authored dynamic header."""
        position = 0

        def read(count):
            """Read this small oracle header without invoking any generated runtime validator."""
            nonlocal position
            value = sum(((raw[(position+bit)//8] >> ((position+bit) % 8)) & 1) << bit for bit in range(count))
            position += count
            return value

        read(1)
        if read(2) != 2:
            raise AssertionError('Independent provider did not emit a dynamic DEFLATE block')
        total = read(5)+257+read(5)+1
        code_count = read(4)+4
        order = (16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15)
        lengths = [0]*19
        for index in range(code_count):
            lengths[order[index]] = read(3)
        counts = [lengths.count(width) for width in range(8)]
        counts[0] = 0
        codes = {}; next_code = 0
        for width in range(1,8):
            next_code = (next_code+counts[width-1]) << 1
            for symbol, length in enumerate(lengths):
                if length == width:
                    codes[width,next_code] = symbol
                    next_code += 1
            next_code -= counts[width]
        consumed = 0; observed = set()
        while consumed < total:
            code = 0
            for width in range(1,8):
                code = (code << 1) | read(1)
                if (width,code) in codes:
                    symbol = codes[width,code]
                    break
            else:
                raise AssertionError('Independent dynamic-header canonical code differs')
            if symbol < 16:
                consumed += 1
            else:
                observed.add(symbol)
                consumed += read(2)+3 if symbol == 16 else read(3)+3 if symbol == 17 else read(7)+11
        return observed

    observed_repeats = set()
    for name, plain, strategy in (
            ('default', bytes(range(128))*100, zlib.Z_DEFAULT_STRATEGY),
            ('huffman', (b'A'*7+b'G'*3)*5000, zlib.Z_HUFFMAN_ONLY),
            ('rle', (b'A'*300+b'BCDEFGHIJ'*20)*100, zlib.Z_RLE)):
        encoder = zlib.compressobj(6,zlib.DEFLATED,-9,8,strategy)
        raw = encoder.compress(plain)+encoder.flush()
        observed_repeats.update(repeat_codes(raw))
        add('provider-dynamic-'+name,raw,plain,1,True)
    if observed_repeats != {16,17,18}:
        raise AssertionError('Independent dynamic headers did not cover all code-length repeat symbols: '+str(observed_repeats))
    malformed = (
        b'\x07', b'\x01\x01\0\0\0x',
        bits_frame([1,1,0]+fixed_symbol(257)+[0]*5+fixed_symbol(256)),
        bits_frame([1,1,0]+fixed_symbol(286)+fixed_symbol(256)),
        bits_frame([1,1,0]+fixed_symbol(65)+fixed_symbol(257)+[1,1,1,1,0]+fixed_symbol(256)),
        bits_frame([1,0,1]+wire_bits(30,5)+[0]*9),
        bits_frame([1,0,1]+[0]*14+[bit for value in (1,1,1,1) for bit in wire_bits(value,3)]),
        bits_frame([1,0,1]+[0]*14+[bit for value in (0,0,0,2) for bit in wire_bits(value,3)]))
    for index, raw in enumerate(malformed):
        try:
            zlib.decompress(raw,-15)
        except zlib.error:
            pass
        else:
            raise AssertionError('Independent malformed DEFLATE oracle accepted input')
        stem = 'malformed-small-window-'+str(index)
        frame = b'\x08\x1d'+raw+b'\0'*4
        (directory/(stem+'.bin')).write_bytes(frame)
        (directory/(stem+'.plain')).write_bytes(b'')
        rows.append((stem+'.bin',stem+'.plain','invalid_data',len(frame),8*1024*1024,8*1024*1024))
    (directory/'window-coverage.json').write_text(json.dumps({'repeat_codes':sorted(observed_repeats),'zlib':zlib.ZLIB_RUNTIME_VERSION,'provider_dynamic_profiles':3,'malformed_profiles':len(malformed)},indent=2)+'\n',encoding='utf-8')
    (directory/'windows.tsv').write_text(''.join('\t'.join(map(str,row))+'\n' for row in rows), encoding='utf-8')
    return rows
