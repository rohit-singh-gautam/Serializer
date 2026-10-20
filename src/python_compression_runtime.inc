// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(

class _Compression:
    """Explicit standard compression of one complete message; wrappers borrow their streams."""
    _chunk_bytes = 64 * 1024
    _default_message_bytes = 64 * 1024 * 1024
    _default_window_bytes = 8 * 1024 * 1024
    _deflate_window_bytes = 32 * 1024
    _protocols = (0, 1, 2, 3)

    class Format(_IntEnum):
        """Choose a standard wire format explicitly, without sniffing or fallback."""
        NONE = 0
        GZIP = 1
        ZLIB = 2
        DEFLATE = 3
        ZSTD = 4
        LZ4 = 5

    class Error(ValueError):
        """Report a compression category without retaining payload or provider excerpts."""
        def __init__(self, code, message):
            """Retain the stable error code alongside a bounded diagnostic."""
            self.code = code
            super().__init__(message)

    class EncodeLimits:
        """Logical serialized/compressed byte bounds, plus cooperative cancellation."""
        __slots__ = ('max_input_bytes', 'max_output_bytes', 'check_cancelled')

        def __init__(self, max_input_bytes=64*1024*1024, max_output_bytes=64*1024*1024,
                     check_cancelled=None):
            """Validate policy before provider construction, serialization or sink writes."""
            _Compression._limits(max_input_bytes, max_output_bytes)
            _Compression._callback(check_cancelled)
            self.max_input_bytes = max_input_bytes
            self.max_output_bytes = max_output_bytes
            self.check_cancelled = check_cancelled

    class DecodeOptions:
        """Independent compressed/expanded/window bounds for exactly one frame or member."""
        __slots__ = ('format', 'max_input_bytes', 'max_output_bytes', 'max_window_bytes',
                     'check_cancelled')

        def __init__(self, format=0, max_input_bytes=64*1024*1024,
                     max_output_bytes=64*1024*1024, max_window_bytes=8*1024*1024,
                     check_cancelled=None):
            """Validate byte policies; provider-specific window checks precede source reads."""
            self.format = _Compression._format(format)
            _Compression._limits(max_input_bytes, max_output_bytes, max_window_bytes)
            _Compression._callback(check_cancelled)
            self.max_input_bytes = max_input_bytes
            self.max_output_bytes = max_output_bytes
            self.max_window_bytes = max_window_bytes
            self.check_cancelled = check_cancelled

    @staticmethod
    def _limits(*values):
        """Reject negative bounds, Boolean coercions and unbounded policy sentinels."""
        if any(type(value) is not int or value < 0 for value in values):
            raise _Compression.Error('invalid_options', 'Invalid compression byte limit')

    @staticmethod
    def _callback(callback):
        """Reject invalid cancellation hooks before invoking user or provider code."""
        if callback is not None and not callable(callback):
            raise _Compression.Error('invalid_options', 'Invalid compression cancellation hook')

    @staticmethod
    def _poll(policy):
        """Preserve the caller's exception when cooperative cancellation interrupts a call."""
        if policy.check_cancelled is not None:
            policy.check_cancelled()

    @staticmethod
    def _format(value):
        """Normalize an explicit supported format without accepting Boolean aliases."""
        if not isinstance(value, (int, _Compression.Format)) or isinstance(value, bool):
            raise _Compression.Error('invalid_options', 'Unknown compression format')
        try:
            return _Compression.Format(value)
        except ValueError:
            raise _Compression.Error('invalid_options', 'Unknown compression format') from None

    @staticmethod
    def _provider(format):
        """Load only the selected optional provider; do no serialization or stream I/O."""
        format = _Compression._format(format)
        if format == _Compression.Format.NONE:
            return None
        name = ('zlib' if format in (_Compression.Format.GZIP, _Compression.Format.ZLIB,
                                    _Compression.Format.DEFLATE) else
                'compression.zstd' if format == _Compression.Format.ZSTD else 'lz4.frame')
        try:
            return __import__(name, fromlist=['*'])
        except ImportError:
            raise _Compression.Error('unavailable', 'Compression provider is unavailable') from None

    @staticmethod
    def available(format):
        """Report whether the selected Python provider can be imported on this host."""
        try:
            _Compression._provider(format)
            return True
        except _Compression.Error as error:
            if error.code != 'unavailable':
                raise
            return False

    @staticmethod
    def _view(data):
        """Borrow a contiguous byte view; never coerce text, integers or iterables."""
        try:
            view = memoryview(data)
            if not view.c_contiguous:
                raise TypeError()
            return view.cast('B')
        except (TypeError, ValueError):
            raise _Compression.Error('invalid_options', 'Compression requires contiguous bytes') from None

    @staticmethod
    def _encode_policy(limits):
        """Copy validated limits so caller mutation cannot change an active operation."""
        if limits is None:
            return _Compression.EncodeLimits()
        if not isinstance(limits, _Compression.EncodeLimits):
            raise _Compression.Error('invalid_options', 'Expected compression encode limits')
        return _Compression.EncodeLimits(limits.max_input_bytes, limits.max_output_bytes,
                                         limits.check_cancelled)

    @staticmethod
    def _decode_policy(options):
        """Copy and validate the complete decoder policy before reading its source."""
        if options is None:
            return _Compression.DecodeOptions()
        if not isinstance(options, _Compression.DecodeOptions):
            raise _Compression.Error('invalid_options', 'Expected compression decode options')
        return _Compression.DecodeOptions(options.format, options.max_input_bytes,
                                          options.max_output_bytes, options.max_window_bytes,
                                          options.check_cancelled)

    @staticmethod
    def _window(options):
        """Enforce mandatory DEFLATE history before reading input or allocating its decoder."""
        if (options.format in (_Compression.Format.GZIP, _Compression.Format.ZLIB,
                               _Compression.Format.DEFLATE) and
                options.max_window_bytes < _Compression._deflate_window_bytes):
            raise _Compression.Error('resource_limit', 'Compression window limit')
        if options.format == _Compression.Format.ZSTD and options.max_window_bytes < 1024:
            raise _Compression.Error('resource_limit', 'Compression window limit')

)serializer"
R"serializer(    @staticmethod
    def _zlib_history(data, options):
        """Enforce a valid RFC1950 header's sub-32-KiB distance limit before native expansion."""
        if len(data) < 6:
            return
        header, flags = data[0], data[1]
        if (header & 15 != 8 or header >> 4 >= 7 or
                ((header << 8) | flags) % 31 or flags & 32):
            return
        window = 1 << ((header >> 4) + 8)
        grammar_poll_mask = 4096 - 1
        position, end, pending, available, work, expanded = 2, len(data) - 4, 0, 0, 0, 0
        length_base = (3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
                       31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258)
        length_extra = (0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                        2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0)
        distance_base = (1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                         193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                         6145, 8193, 12289, 16385, 24577)
        distance_extra = (0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                          6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13)
        order = (16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15)

        def invalid():
            """Reject malformed grammar without retaining payload or provider diagnostics."""
            raise _Compression.Error('invalid_data', 'Invalid zlib DEFLATE history or grammar')

        def read(count):
            """Read exact required bits and poll bounded grammar work without speculative input."""
            nonlocal position, pending, available, work
            work += 1
            if work & grammar_poll_mask == 0:
                _Compression._poll(options)
            while available < count:
                if position == end:
                    invalid()
                pending |= data[position] << available
                position += 1
                available += 8
            result = pending & ((1 << count) - 1)
            pending >>= count
            available -= count
            return result

        def build(lengths, code_lengths=False, allow_empty=False):
            """Build bounded canonical code tables, permitting only specified incomplete alphabets."""
            counts, first, offsets = [0] * 16, [0] * 16, [0] * 16
            maximum, used = 0, 0
            for length in lengths:
                if not 0 <= length <= 15:
                    invalid()
                if length:
                    counts[length] += 1
                    used += 1
                    maximum = max(maximum, length)
            if not used:
                if allow_empty:
                    return counts, first, offsets, [], 0
                invalid()
            left = 1
            for length in range(1, 16):
                left = (left << 1) - counts[length]
                if left < 0:
                    invalid()
            if left and (code_lengths or maximum != 1):
                invalid()
            code, offset = 0, 0
            for length in range(1, 16):
                code = (code + counts[length - 1]) << 1
                first[length], offsets[length] = code, offset
                offset += counts[length]
            next_offsets, symbols = list(offsets), [0] * used
            for symbol, length in enumerate(lengths):
                if length:
                    symbols[next_offsets[length]] = symbol
                    next_offsets[length] += 1
            return counts, first, offsets, symbols, maximum

        def symbol(tree):
            """Decode a bounded canonical code in wire transmission order."""
            counts, first, offsets, symbols, maximum = tree
            code = 0
            for length in range(1, maximum + 1):
                code = (code << 1) | read(1)
                if first[length] <= code < first[length] + counts[length]:
                    return symbols[offsets[length] + code - first[length]]
            invalid()

        def grow(count):
            """Admit actual expanded count before native provider allocation or output publication."""
            nonlocal expanded
            if count > options.max_output_bytes - expanded:
                raise _Compression.Error('resource_limit', 'Expanded compression output limit')
            expanded += count

        fixed = None
        final = False
        while not final:
            _Compression._poll(options)
            final, kind = bool(read(1)), read(2)
            if kind == 0:
                pending, available = 0, 0
                count, complement = read(16), read(16)
                if count ^ complement != 65535 or count > end - position:
                    invalid()
                grow(count)
                position += count
                continue
            if kind == 1:
                if fixed is None:
                    lengths = [8] * 144 + [9] * 112 + [7] * 24 + [8] * 8
                    fixed = build(lengths), build([5] * 32)
                literal, distance = fixed
            elif kind == 2:
                literal_count, distance_count, code_count = read(5) + 257, read(5) + 1, read(4) + 4
                if literal_count > 286:
                    invalid()
                code_lengths = [0] * 19
                for index in range(code_count):
                    code_lengths[order[index]] = read(3)
                codes = build(code_lengths, code_lengths=True)
                lengths, total = [], literal_count + distance_count
                while len(lengths) < total:
                    value = symbol(codes)
                    if value <= 15:
                        lengths.append(value)
                        continue
                    if value == 16:
                        if not lengths:
                            invalid()
                        repeated, count = lengths[-1], read(2) + 3
                    elif value == 17:
                        repeated, count = 0, read(3) + 3
                    elif value == 18:
                        repeated, count = 0, read(7) + 11
                    else:
                        invalid()
                    if count > total - len(lengths):
                        invalid()
                    lengths.extend([repeated] * count)
                if lengths[256] == 0:
                    invalid()
                literal = build(lengths[:literal_count])
                distance = build(lengths[literal_count:], allow_empty=True)
            else:
                invalid()
            while True:
                value = symbol(literal)
                if value < 256:
                    grow(1)
                elif value == 256:
                    break
                else:
                    if value > 285:
                        invalid()
                    count = length_base[value - 257] + read(length_extra[value - 257])
                    code = symbol(distance)
                    if code >= len(distance_base):
                        invalid()
                    backward = distance_base[code] + read(distance_extra[code])
                    if backward > expanded or backward > window:
                        invalid()
                    grow(count)
        if position != end:
            invalid()
        _Compression._poll(options)

)serializer"
R"serializer(    @staticmethod
    def _zstd_window(data, provider, options):
        """Validate the small frame header and its dictionary/window requirements before decoding."""
        try:
            frame = provider.get_frame_info(data)
        except provider.ZstdError:
            raise _Compression.Error('invalid_data', 'Invalid Zstandard frame header') from None
        if frame.dictionary_id:
            raise _Compression.Error('invalid_data', 'Zstandard dictionaries are unsupported')
        # The standard descriptor encodes a base power of two plus eighths of that base.
        if data[4] & 0x20:
            window_bytes = frame.decompressed_size
        else:
            descriptor = data[5]
            base_bytes = 1 << (10 + (descriptor >> 3))
            window_bytes = base_bytes + (base_bytes >> 3) * (descriptor & 7)
        effective_window_bytes = 1 << (options.max_window_bytes.bit_length() - 1)
        if window_bytes > effective_window_bytes:
            raise _Compression.Error('resource_limit', 'Compression window limit')

    class _MemorySink:
        """Own staged output so a failed whole-message helper publishes no bytes."""
        def __init__(self):
            """Start with independent empty byte storage."""
            self.data = bytearray()

        def write(self, data):
            """Append one already bounded output batch."""
            self.data.extend(data)
            return len(data)

    class _Buffer(bytearray):
        """Enforce serialized-byte admission before generated writer buffer growth."""
        def __init__(self, policy):
            """Borrow one immutable operation policy for the generated writer's lifetime."""
            super().__init__()
            self.policy = policy

        def _admit(self, count):
            """Poll cancellation and reject growth before modifying the serialized buffer."""
            _Compression._poll(self.policy)
            if count > self.policy.max_input_bytes - len(self):
                raise _Compression.Error('resource_limit', 'Serialized compression input limit')

        def append(self, value):
            """Admit a single generated wire byte before appending it."""
            self._admit(1)
            super().append(value)

        def extend(self, value):
            """Admit a generated contiguous wire batch before copying it."""
            value = _Compression._view(value)
            self._admit(len(value))
            super().extend(value)

        def __setitem__(self, key, value):
            """Keep generated fixed-width backpatches from bypassing byte admission."""
            if isinstance(key, slice):
                value = _Compression._view(value)
                replaced = len(range(*key.indices(len(self))))
                self._admit(max(0, len(value) - replaced))
            else:
                self._admit(0)
            super().__setitem__(key, value)

)serializer"
R"serializer(    class OutputStream:
        """Compress borrowed byte batches into one message; finish explicitly, never close the sink."""
        def __init__(self, sink, format=0, level=None, limits=None, checksum=True):
            """Validate provider/options before any sink write; external failures can leave a prefix."""
            self._state = 'failed'
            self._busy = False
            self._failure = None
            self._format = _Compression._format(format)
            self._policy = _Compression._encode_policy(limits)
            self._provider = _Compression._provider(self._format)
            self._sink = sink
            self._input_bytes = self._output_bytes = 0
            if not callable(getattr(sink, 'write', None)) or type(checksum) is not bool:
                raise _Compression.Error('invalid_options', 'Invalid compression sink or checksum option')
            self._encoder = None
            _Compression._poll(self._policy)
            provider = self._provider
            if self._format in (_Compression.Format.GZIP, _Compression.Format.ZLIB,
                                _Compression.Format.DEFLATE):
                level = 6 if level is None else level
                if type(level) is not int or not 0 <= level <= 9 or not checksum:
                    raise _Compression.Error('invalid_options', 'Invalid DEFLATE compression options')
                window = { _Compression.Format.GZIP: 31, _Compression.Format.ZLIB: 15,
                           _Compression.Format.DEFLATE: -15 }[self._format]
                self._encoder = provider.compressobj(level=level, wbits=window)
            elif self._format == _Compression.Format.ZSTD:
                level = 3 if level is None else level
                minimum, maximum = provider.CompressionParameter.compression_level.bounds()
                if type(level) is not int or not minimum <= level <= maximum:
                    raise _Compression.Error('invalid_options', 'Invalid Zstandard compression level')
                self._encoder = provider.ZstdCompressor(options={
                    provider.CompressionParameter.compression_level: level,
                    provider.CompressionParameter.checksum_flag: int(checksum)})
            elif self._format == _Compression.Format.LZ4:
                level = 0 if level is None else level
                if type(level) is not int or not 0 <= level <= 16:
                    raise _Compression.Error('invalid_options', 'Invalid LZ4 compression level')
                self._encoder = provider.LZ4FrameCompressor(
                    block_size=provider.BLOCKSIZE_MAX64KB, block_linked=True,
                    compression_level=level, content_checksum=checksum, auto_flush=True)
            elif level is not None:
                raise _Compression.Error('invalid_options', 'Identity compression has no level')
            self._state = 'active'
            self._started = False

        def _active(self):
            """Fence every later operation after finish or the first failure."""
            if self._failure is not None and self._busy:
                raise self._failure
            if self._state != 'active':
                raise RuntimeError('Compression stream is not active') from self._failure

        def _fail(self, failure):
            """Retain the first failure object even when a nested callback ignores its exception."""
            if self._failure is None:
                self._failure = failure
            self._state = 'failed'
            return self._failure

        def _enter(self):
            """Admit one public operation and reject synchronous same-wrapper callback reentry."""
            self._active()
            if self._busy:
                raise self._fail(_Compression.Error('invalid_options',
                                                   'Reentrant compression output operation'))
            self._busy = True

        def _poll(self):
            """Check active state before and after the caller-owned cooperative cancellation hook."""
            self._active()
            _Compression._poll(self._policy)
            self._active()

        def _emit(self, data):
            """Check logical output bounds before sink writes and handle bounded short writes."""
            self._poll()
            if len(data) > self._policy.max_output_bytes - self._output_bytes:
                raise _Compression.Error('resource_limit', 'Compressed output limit')
            self._output_bytes += len(data)
            view = memoryview(data)
            offset = 0
            while offset < len(view):
                self._poll()
                end = min(len(view), offset + _Compression._chunk_bytes)
                count = self._sink.write(view[offset:end])
                self._active()
                if type(count) is not int or not 0 < count <= end - offset:
                    raise OSError('Invalid compression sink write count')
                offset += count
                self._poll()

        def _start(self):
            """Delay an LZ4 header until the first write or explicit finish."""
            if not self._started:
                self._started = True
                if self._format == _Compression.Format.LZ4:
                    self._emit(self._encoder.begin())

        def write(self, data):
            """Admit and compress one byte batch; any failure permanently fences this wrapper."""
            self._enter()
            try:
                view = _Compression._view(data)
                self._poll()
                if len(view) > self._policy.max_input_bytes - self._input_bytes:
                    raise _Compression.Error('resource_limit', 'Serialized compression input limit')
                self._input_bytes += len(view)
                self._start()
                for offset in range(0, len(view), _Compression._chunk_bytes):
                    self._poll()
                    batch = view[offset:offset + _Compression._chunk_bytes]
                    self._emit(batch if self._encoder is None else self._encoder.compress(batch))
                return len(view)
            except BaseException as failure:
                raise self._fail(failure)
            finally:
                self._busy = False

        def finish(self):
            """Emit exactly one trailer; do not flush, close or synchronize the borrowed sink."""
            self._enter()
            try:
                self._poll()
                self._start()
                if self._encoder is not None:
                    self._emit(self._encoder.flush())
                self._state = 'finished'
            except BaseException as failure:
                raise self._fail(failure)
            finally:
                self._busy = False

    @staticmethod
    def compress(data, format=0, level=None, limits=None, checksum=True):
        """Return owned standard bytes only after a complete bounded encoding succeeds."""
        sink = _Compression._MemorySink()
        output = _Compression.OutputStream(sink, format, level, limits, checksum)
        output.write(data)
        output.finish()
        return bytes(sink.data)

    @staticmethod
    def _read_source(source, options):
        """Stage one bounded EOF-delimited source with finite reads and cancellation polling."""
        if not callable(getattr(source, 'read', None)):
            view = _Compression._view(source)
            _Compression._poll(options)
            if len(view) > options.max_input_bytes:
                raise _Compression.Error('resource_limit', 'Compressed input limit')
            return view
        result = bytearray()
        while True:
            _Compression._poll(options)
            requested = min(_Compression._chunk_bytes, options.max_input_bytes - len(result) + 1)
            batch = _Compression._view(source.read(requested))
            _Compression._poll(options)
            if len(batch) > requested:
                raise OSError('Compression source exceeded its read request')
            if not batch:
                return memoryview(result)
            if len(batch) > options.max_input_bytes - len(result):
                raise _Compression.Error('resource_limit', 'Compressed input limit')
            result.extend(batch)

    @staticmethod
    def decompress(source, options=None):
        """Validate one frame/member before returning owned expanded bytes; never parse fields early."""
        options = _Compression._decode_policy(options)
        provider = _Compression._provider(options.format)
        _Compression._window(options)
        data = _Compression._read_source(source, options)
        if options.format == _Compression.Format.NONE:
            if len(data) > options.max_output_bytes:
                raise _Compression.Error('resource_limit', 'Expanded compression output limit')
            _Compression._poll(options)
            return bytes(data)
        if options.format == _Compression.Format.ZSTD:
            if bytes(data[:4]) != b'\x28\xb5\x2f\xfd':
                raise _Compression.Error('invalid_data', 'Expected one standard Zstandard frame')
            _Compression._zstd_window(data, provider, options)
            maximum = provider.DecompressionParameter.window_log_max.bounds()[1]
            decoder = provider.ZstdDecompressor(options={
                provider.DecompressionParameter.window_log_max:
                    min(maximum, options.max_window_bytes.bit_length() - 1)})
            provider_error = provider.ZstdError
        elif options.format == _Compression.Format.LZ4:
            if bytes(data[:4]) != b'\x04\x22\x4d\x18':
                raise _Compression.Error('invalid_data', 'Expected one standard LZ4 frame')
            if len(data) > 4 and data[4] & 1:
                raise _Compression.Error('invalid_data', 'LZ4 dictionaries are unsupported')
            try:
                frame = provider.get_frame_info(data)
            except RuntimeError:
                raise _Compression.Error('invalid_data', 'Invalid LZ4 frame header') from None
            if frame['block_size'] > options.max_window_bytes:
                raise _Compression.Error('resource_limit', 'Compression window limit')
            decoder = provider.LZ4FrameDecompressor()
            provider_error = RuntimeError
        else:
            if options.format == _Compression.Format.ZLIB:
                _Compression._zlib_history(data, options)
            window = { _Compression.Format.GZIP: 31, _Compression.Format.ZLIB: 15,
                       _Compression.Format.DEFLATE: -15 }[options.format]
            decoder = provider.decompressobj(window)
            provider_error = provider.error
        result = bytearray()
        offset = 0
        pending = None
        is_deflate = options.format in (_Compression.Format.GZIP, _Compression.Format.ZLIB,
                                        _Compression.Format.DEFLATE)
        while not decoder.eof:
            _Compression._poll(options)
            if (pending is None if is_deflate else decoder.needs_input):
                if offset == len(data):
                    raise _Compression.Error('invalid_data', 'Truncated compression message')
                pending = data[offset:offset + _Compression._chunk_bytes]
                offset += len(pending)
            elif not is_deflate:
                pending = b''
            capacity = min(_Compression._chunk_bytes, options.max_output_bytes - len(result) + 1)
            try:
                batch = decoder.decompress(pending, max_length=capacity)
            except provider_error:
                raise _Compression.Error('invalid_data', 'Invalid compression message') from None
            _Compression._poll(options)
            if len(batch) > options.max_output_bytes - len(result):
                raise _Compression.Error('resource_limit', 'Expanded compression output limit')
            result.extend(batch)
            if is_deflate:
                pending = decoder.unconsumed_tail or (b'' if len(batch) == capacity else None)
            if decoder.eof:
                if decoder.unused_data or offset != len(data):
                    raise _Compression.Error('invalid_data', 'Trailing compression input')
            elif is_deflate and pending == b'' and not batch:
                pending = None
        return bytes(result)

)serializer"
R"serializer(    class InputStream:
        """Validate a complete borrowed source before exposing its bounded expanded byte view."""
        def __init__(self, source, options=None):
            """Consume source to EOF, preserving the caller's source ownership and exceptions."""
            self._state = 'failed'
            self._busy = False
            self._failure = None
            self._policy = _Compression._decode_policy(options)
            self._data = _Compression.decompress(source, self._policy)
            self._offset = 0
            self._state = 'active'

        def _active(self):
            """Preserve the first failure and fence every operation after terminal input state."""
            if self._failure is not None and self._busy:
                raise self._failure
            if self._state != 'active':
                raise RuntimeError('Compression stream is not active') from self._failure

        def _fail(self, failure):
            """Keep one original callback, framing or policy exception for all later operations."""
            if self._failure is None:
                self._failure = failure
            self._state = 'failed'
            return self._failure

        def _enter(self):
            """Reject callback reentry before a nested read can change the validated byte cursor."""
            self._active()
            if self._busy:
                raise self._fail(_Compression.Error('invalid_options',
                                                   'Reentrant compression input operation'))
            self._busy = True

        def _poll(self):
            """Observe a nested failure even when the cooperative callback returns normally."""
            self._active()
            _Compression._poll(self._policy)
            self._active()

        def read(self, size=-1):
            """Read only validated expanded bytes; never reach the provider or source again."""
            self._enter()
            try:
                self._poll()
                if type(size) is not int or size < -1:
                    raise ValueError('Invalid compression read size')
                end = len(self._data) if size == -1 else min(len(self._data), self._offset + size)
                result = self._data[self._offset:end]
                self._poll()
                self._offset = end
                return result
            except BaseException as failure:
                raise self._fail(failure)
            finally:
                self._busy = False

        def finish(self):
            """Require exact expanded-byte consumption and permanently fence the input wrapper."""
            self._enter()
            try:
                self._poll()
                if self._offset != len(self._data):
                    raise _Compression.Error('invalid_data', 'Trailing expanded message bytes')
                self._state = 'finished'
            except BaseException as failure:
                raise self._fail(failure)
            finally:
                self._busy = False

    @staticmethod
    def encode(value, protocol, format=0, level=None, limits=None, checksum=True):
        """Serialize one generated native object under input bounds, then compress it once."""
        policy = _Compression._encode_policy(limits)
        sink = _Compression._MemorySink()
        output = _Compression.OutputStream(sink, format, level, policy, checksum)
        writer = _Writer(protocol)
        if writer.protocol not in _Compression._protocols:
            raise ValueError('Protocol disabled during generation')
        if type(value).__module__ != __name__:
            raise _Compression.Error('invalid_options', 'Expected a generated model from this module')
        writer.data = _Compression._Buffer(policy)
        _Compression._poll(policy)
        value._write(writer)
        output.write(writer.data)
        output.finish()
        return bytes(sink.data)

    @staticmethod
    def decode(value_type, source, protocol, options=None, limits=None):
        """Return a fresh generated native object after exact frame and inner-message validation."""
        options = _Compression._decode_policy(options)
        protocol = _protocol(protocol)
        if protocol not in _Compression._protocols:
            raise ValueError('Protocol disabled during generation')
        limits = Limits() if limits is None else limits
        if not isinstance(limits, Limits):
            raise ValueError('Expected Limits')
        limits = Limits(limits.max_bytes, limits.max_string_bytes, limits.max_elements,
                        limits.max_depth, limits.read_policy)
        # Inner admission also bounds expansion before allocating the complete serialized staging.
        options.max_output_bytes = min(options.max_output_bytes, limits.max_bytes)
        expanded = _Compression.decompress(source, options)
        _Compression._poll(options)
        result = value_type.decode(expanded, protocol, limits)
        _Compression._poll(options)
        return result

__serializer_compression__ = _Compression
)serializer"
