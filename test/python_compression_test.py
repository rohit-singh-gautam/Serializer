"""Qualify emitted Python whole-message compression against independent standard APIs."""
import argparse
import builtins
import gzip
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib


ROOT = Path(__file__).resolve().parent.parent
SCHEMA = '''serializer version 1.20.0;
class child {
  public uint32 id (1);
  public string text (2);
}
class record {
  public uint64 count (1);
  public string text (2);
  public array child children (3);
  public uint64 encoded_size (4) auto_size(binary_none, binary_integer, binary_string);
}
'''


class compression_source:
    """Record finite borrowed-source reads and optionally simulate short-read transports."""
    def __init__(self, data, batch=17):
        """Own immutable fixture bytes without transferring source ownership to the wrapper."""
        self.source = io.BytesIO(data)
        self.batch = batch
        self.requests = []

    def read(self, size):
        """Return at most the requested byte batch and record its admission size."""
        if not 0 < size <= 64*1024:
            raise AssertionError('Unbounded source read')
        self.requests.append(size)
        return self.source.read(min(size, self.batch))


class python_compression_test(unittest.TestCase):
    """Exercise the actual generated runtime and owning model codec, including optional providers."""
    def test_small_zlib_advertised_history(self):
        """Check shared small-window fixtures, pre-provider admission and every grammar cancel point."""
        from compression_window_fixtures import small_zlib_windows
        api = self.module.__serializer_compression__
        large = None
        with tempfile.TemporaryDirectory(prefix='serializer-python-window-') as temporary:
            directory = Path(temporary)
            for frame_name, plain_name, expected, maximum_input, maximum_output, maximum_window in small_zlib_windows(directory):
                with self.subTest(fixture=frame_name, expected=expected):
                    frame = (directory / frame_name).read_bytes()
                    options = api.DecodeOptions(api.Format.ZLIB, maximum_input, maximum_output, maximum_window)
                    if expected == 'ok':
                        self.assertEqual(api.decompress(frame, options), (directory / plain_name).read_bytes())
                        if large is None or len(frame) > len(large):
                            large = frame
                    else:
                        with mock.patch.object(zlib, 'decompressobj', side_effect=AssertionError('Provider created before admission')):
                            with self.assertRaises(api.Error) as caught:
                                api.decompress(frame, options)
                        self.assertEqual(caught.exception.code, expected)
        polls = []
        api._zlib_history(large, api.DecodeOptions(api.Format.ZLIB, check_cancelled=lambda: polls.append(None)))
        self.assertGreater(len(polls), 10)
        original = RuntimeError('original small-window validation cancellation')
        for stop_at in range(1, len(polls) + 1):
            observed = []
            def cancel():
                """Interrupt each observed bounded grammar boundary with the same exception object."""
                observed.append(None)
                if len(observed) == stop_at:
                    raise original
            with self.assertRaises(RuntimeError) as caught:
                api._zlib_history(large, api.DecodeOptions(api.Format.ZLIB, check_cancelled=cancel))
            self.assertIs(caught.exception, original)

    def fixture(self):
        """Build Unicode, nested collections, full-width integers and binary-size backpatches."""
        value = self.module.Record()
        value.count = (1 << 64) - 1
        value.text = 'unicode \U0001f642 \u03bb <&> ' * 200
        for index in range(3):
            item = self.module.Child()
            item.id, item.text = index, str(index) * 200
            value.children.append(item)
        return value

    def formats(self):
        """Select providers genuinely importable in the active Python installation."""
        return [format for format in self.api.Format if self.api.available(format)]

    def options(self, format, **kwargs):
        """Create explicit single-message decoder policy for one selected wire format."""
        return self.api.DecodeOptions(format, **kwargs)

    def assert_error(self, code, callback):
        """Check stable error categories without relying on provider diagnostics."""
        with self.assertRaises(self.api.Error) as caught:
            callback()
        self.assertEqual(caught.exception.code, code)

    def test_native_protocol_round_trips_and_ordinary_wire_equality(self):
        """Compression changes only the outer bytes and preserves every native protocol/backpatch."""
        value = self.fixture()
        for protocol in self.module.Protocol:
            plain = value.encode(protocol)
            for format in self.formats():
                with self.subTest(protocol=protocol, format=format):
                    packed = self.api.encode(value, protocol, format)
                    self.assertEqual(self.api.decompress(packed, self.options(format)), plain)
                    decoded = self.api.decode(self.module.Record, packed, protocol,
                                              self.options(format))
                    self.assertEqual(decoded.encode(protocol), plain)
                    self.assertEqual(decoded.children[2].text, value.children[2].text)
                    self.assertEqual(value.encode(protocol), plain)

    def test_independent_standard_producers_and_consumers(self):
        """Exchange frames with gzip/zlib and each available optional provider's separate APIs."""
        payload = bytes(range(256)) * 4096
        formats = self.api.Format
        cases = [(formats.GZIP, gzip.compress(payload, mtime=0), gzip.decompress),
                 (formats.ZLIB, zlib.compress(payload), zlib.decompress)]
        encoder = zlib.compressobj(wbits=-15)
        cases.append((formats.DEFLATE, encoder.compress(payload) + encoder.flush(),
                      lambda data: zlib.decompress(data, -15)))
        if self.api.available(formats.ZSTD):
            from compression import zstd
            cases.append((formats.ZSTD, zstd.compress(payload), zstd.decompress))
        if self.api.available(formats.LZ4):
            import lz4.frame
            cases.append((formats.LZ4, lz4.frame.compress(payload), lz4.frame.decompress))
        for format, independent, read in cases:
            with self.subTest(format=format):
                self.assertEqual(self.api.decompress(independent, self.options(format)), payload)
                self.assertEqual(read(self.api.compress(payload, format)), payload)

    def test_identity_and_composable_borrowed_wrappers(self):
        """Finish wrappers from the codec outward, without closing or flushing borrowed streams."""
        payload = self.fixture().encode(self.module.Protocol.JSON)
        sink = io.BytesIO()
        outer = self.api.OutputStream(sink, self.api.Format.GZIP)
        inner = self.api.OutputStream(outer, self.api.Format.ZLIB)
        for offset in range(0, len(payload), 17):
            self.assertEqual(inner.write(payload[offset:offset+17]), len(payload[offset:offset+17]))
        inner.finish()
        outer.finish()
        self.assertFalse(sink.closed)
        first = self.api.InputStream(io.BytesIO(sink.getvalue()), self.options(self.api.Format.GZIP))
        second = self.api.InputStream(first, self.options(self.api.Format.ZLIB))
        self.assertEqual(second.read(), payload)
        second.finish()
        first.finish()
        identity = self.api.InputStream(payload)
        identity.read(1)
        self.assert_error('invalid_data', identity.finish)
        with self.assertRaises(RuntimeError):
            identity.read()
        self.assertEqual(self.api.compress(payload), payload)

    def test_finish_is_explicit_and_successful_wrappers_are_fenced(self):
        """Destruction emits nothing; explicit finish emits one message and disallows reuse."""
        sink = io.BytesIO()
        writer = self.api.OutputStream(sink, self.api.Format.GZIP)
        writer.write(b'payload')
        prefix = sink.getvalue()
        del writer
        self.assertEqual(sink.getvalue(), prefix)
        writer = self.api.OutputStream(io.BytesIO(), self.api.Format.GZIP)
        writer.write(b'payload')
        writer.finish()
        for call in (writer.finish, lambda: writer.write(b'')):
            with self.assertRaises(RuntimeError):
                call()
        reader = self.api.InputStream(b'')
        reader.finish()
        with self.assertRaises(RuntimeError):
            reader.finish()

    def test_invalid_framing_truncation_concatenation_and_checksums(self):
        """Enforce exactly one selected frame/member, including empty and corrupt boundaries."""
        for format in self.formats():
            if format == self.api.Format.NONE:
                continue
            packed = self.api.compress(b'one complete message' * 100, format)
            for malformed in (b'', packed[:-1], packed[:4], packed + b'x', packed + packed):
                with self.subTest(format=format, length=len(malformed)):
                    self.assert_error('invalid_data', lambda: self.api.decompress(
                        malformed, self.options(format)))
            if format != self.api.Format.DEFLATE:
                corrupt = packed[:-1] + bytes((packed[-1] ^ 1,))
                self.assert_error('invalid_data', lambda: self.api.decompress(
                    corrupt, self.options(format)))
            self.assertEqual(self.api.decompress(self.api.compress(b'', format),
                                                 self.options(format)), b'')
        dictionary = zlib.compressobj(zdict=b'one complete message')
        packed = dictionary.compress(b'one complete message' * 100) + dictionary.flush()
        self.assert_error('invalid_data', lambda: self.api.decompress(
            packed, self.options(self.api.Format.ZLIB)))

    def test_exact_inner_object_before_publication(self):
        """A valid frame cannot hide a second object or malformed generated native input."""
        for protocol in self.module.Protocol:
            plain = self.fixture().encode(protocol)
            for format in self.formats():
                packed = self.api.compress(plain + plain, format)
                with self.assertRaises(ValueError):
                    self.api.decode(self.module.Record, packed, protocol, self.options(format))

    def test_input_output_and_inner_decode_byte_boundaries(self):
        """Accept exact bounds, reject one-byte excess, and bound input before buffer growth."""
        payload = bytes(range(256)) * 1024
        for format in self.formats():
            packed = self.api.compress(payload, format)
            self.assertEqual(self.api.decompress(packed, self.options(
                format, max_input_bytes=len(packed), max_output_bytes=len(payload))), payload)
            for kwargs in ({'max_input_bytes': len(packed)-1},
                           {'max_output_bytes': len(payload)-1}):
                self.assert_error('resource_limit', lambda: self.api.decompress(
                    packed, self.options(format, **kwargs)))
            for kwargs in ({'max_input_bytes': len(payload)-1},
                           {'max_output_bytes': len(packed)-1}):
                self.assert_error('resource_limit', lambda: self.api.compress(
                    payload, format, limits=self.api.EncodeLimits(**kwargs)))
        value = self.fixture()
        protocol = self.module.Protocol.BINARY_NONE
        size = len(value.encode(protocol))
        self.assert_error('resource_limit', lambda: self.api.encode(
            value, protocol, limits=self.api.EncodeLimits(max_input_bytes=size-1)))
        packed = self.api.encode(value, protocol, self.api.Format.GZIP)
        self.assert_error('resource_limit', lambda: self.api.decode(
            self.module.Record, packed, protocol, self.options(self.api.Format.GZIP),
            self.module.Limits(max_bytes=size-1)))

    def test_bounded_source_reads_and_short_writes(self):
        """Borrowed streams use bounded finite calls and support partial transport progress."""
        payload = bytes(range(256)) * 2048
        class short_sink(io.BytesIO):
            """Accept only a small positive prefix from every bounded write request."""
            def write(self, data):
                """Check write bounds and intentionally consume three bytes per call."""
                if len(data) > 64*1024:
                    raise AssertionError('Unbounded sink write')
                return super().write(data[:3])
        for format in self.formats():
            sink = short_sink()
            writer = self.api.OutputStream(sink, format)
            writer.write(payload)
            writer.finish()
            source = compression_source(sink.getvalue(), batch=101)
            self.assertEqual(self.api.decompress(source, self.options(format)), payload)
            self.assertTrue(source.requests)
            self.assertFalse(source.source.closed)

    def test_optional_provider_absence_precedes_all_user_io_and_serialization(self):
        """Every missing optional module rejects before reads, writes, or model traversal."""
        original = builtins.__import__
        class untouched:
            """Fail if unavailable-provider handling enters any caller I/O or codec."""
            def read(self, size):
                """Reject an unexpected early source read."""
                raise AssertionError('Source was read')
            def write(self, data):
                """Reject an unexpected early sink write."""
                raise AssertionError('Sink was written')
            def _write(self, writer):
                """Reject unexpected generated model traversal."""
                raise AssertionError('Object was serialized')
        for format in self.api.Format:
            if format == self.api.Format.NONE:
                continue
            name = ('compression.zstd' if format == self.api.Format.ZSTD else
                    'lz4.frame' if format == self.api.Format.LZ4 else 'zlib')
            def blocked(module, *args, **kwargs):
                """Simulate an absent named optional provider without intercepting unrelated imports."""
                if module == name:
                    raise ImportError('provider intentionally absent')
                return original(module, *args, **kwargs)
            with mock.patch.object(builtins, '__import__', side_effect=blocked):
                self.assertFalse(self.api.available(format))
                for call in (lambda: self.api.OutputStream(untouched(), format),
                             lambda: self.api.decompress(untouched(), self.options(format)),
                             lambda: self.api.encode(untouched(), self.module.Protocol.JSON, format)):
                    self.assert_error('unavailable', call)
        with mock.patch.object(builtins, '__import__', side_effect=RuntimeError('broken import')):
            with self.assertRaisesRegex(RuntimeError, 'broken import'):
                self.api.available(self.api.Format.GZIP)

    def test_cancellation_exception_identity_and_permanent_failure_fencing(self):
        """Cancellation propagates unchanged, including during zero-output provider batches."""
        cancellation = RuntimeError('cancelled by caller')
        calls = []
        def check():
            """Cancel after provider initialization but during the first bounded operation."""
            calls.append(None)
            if len(calls) >= 4:
                raise cancellation
        writer = self.api.OutputStream(io.BytesIO(), self.api.Format.GZIP,
                                       limits=self.api.EncodeLimits(check_cancelled=check))
        with self.assertRaises(RuntimeError) as caught:
            writer.write(b'a' * 200000)
        self.assertIs(caught.exception, cancellation)
        for call in (writer.finish, lambda: writer.write(b'')):
            with self.assertRaisesRegex(RuntimeError, 'not active'):
                call()
        calls.clear()
        packed = self.api.compress(b'a' * 200000, self.api.Format.GZIP)
        with self.assertRaises(RuntimeError) as caught:
            self.api.decompress(packed, self.options(self.api.Format.GZIP, check_cancelled=check))
        self.assertIs(caught.exception, cancellation)

    def test_sink_failure_and_no_progress_are_fenced(self):
        """External prefixes may survive, but failed writers never emit another byte."""
        class failed_sink:
            """Consume one short prefix, then throw from the borrowed transport."""
            def __init__(self):
                """Retain progress to prove later wrapper calls never retry I/O."""
                self.calls = 0
            def write(self, data):
                """Inject a transport failure after the first short write."""
                self.calls += 1
                if self.calls > 1:
                    raise OSError('transport failed')
                return min(3, len(data))
        for format in self.formats():
            sink = failed_sink()
            writer = self.api.OutputStream(sink, format)
            with self.assertRaises(OSError):
                writer.write(bytes(range(256)) * 1024)
                writer.finish()
            calls = sink.calls
            for call in (writer.finish, lambda: writer.write(b'')):
                with self.assertRaises(RuntimeError):
                    call()
            self.assertEqual(sink.calls, calls)
        for count in (None, 0, -1, True, 1000000):
            sink = mock.Mock()
            sink.write.return_value = count
            writer = self.api.OutputStream(sink)
            with self.assertRaises(OSError):
                writer.write(b'x')
            with self.assertRaises(RuntimeError):
                writer.finish()

    def test_staged_input_keeps_policy_and_cancellation_fences_read_and_finish(self):
        """Retain a policy snapshot and observe cancellation after source validation has completed."""
        cancellation = RuntimeError('cancelled while consuming staged input')
        armed = []
        def check():
            """Raise the same caller exception when staged consumption is cancelled."""
            if armed:
                raise cancellation
        packed = self.api.compress(b'complete payload', self.api.Format.GZIP)
        for operation in ('read', 'finish'):
            source = compression_source(packed)
            options = self.options(self.api.Format.GZIP, check_cancelled=check)
            reader = self.api.InputStream(source, options)
            options.check_cancelled = None
            if operation == 'finish':
                self.assertEqual(reader.read(), b'complete payload')
            calls = len(source.requests)
            armed.append(True)
            with self.assertRaises(RuntimeError) as caught:
                getattr(reader, operation)()
            self.assertIs(caught.exception, cancellation)
            for call in (reader.read, reader.finish):
                with self.assertRaisesRegex(RuntimeError, 'not active'):
                    call()
            self.assertEqual(len(source.requests), calls)
            armed.clear()

    def test_final_sink_write_cancellation_fences_completed_trailer_publication(self):
        """Poll after the last borrowed write so late cancellation cannot report finish success."""
        cancellation = RuntimeError('cancelled by final sink write')
        armed = []
        def check():
            """Preserve cancellation raised at the final provider-to-sink boundary."""
            if armed:
                raise cancellation
        class cancelling_sink(io.BytesIO):
            """Arm cancellation only when the caller starts publishing the final trailer."""
            def __init__(self):
                """Keep initial provider header writes uncancelled."""
                super().__init__()
                self.cancel_on_write = False
                self.calls = 0
            def write(self, data):
                """Retain the external bytes before marking the active operation cancelled."""
                self.calls += 1
                count = super().write(data)
                if self.cancel_on_write:
                    armed.append(True)
                return count
        sink = cancelling_sink()
        writer = self.api.OutputStream(sink, self.api.Format.GZIP,
                                       limits=self.api.EncodeLimits(check_cancelled=check))
        writer.write(b'complete payload')
        previous_size = len(sink.getvalue())
        sink.cancel_on_write = True
        with self.assertRaises(RuntimeError) as caught:
            writer.finish()
        self.assertIs(caught.exception, cancellation)
        self.assertGreater(len(sink.getvalue()), previous_size)
        calls = sink.calls
        for call in (writer.finish, lambda: writer.write(b'')):
            with self.assertRaisesRegex(RuntimeError, 'not active'):
                call()
        self.assertEqual(sink.calls, calls)

    def test_policy_validation_and_window_admission(self):
        """Reject coercions, invalid levels and insufficient DEFLATE windows before source reads."""
        for value in (-1, True, 1.5):
            self.assert_error('invalid_options', lambda: self.api.EncodeLimits(max_input_bytes=value))
            self.assert_error('invalid_options', lambda: self.api.DecodeOptions(max_window_bytes=value))
        for format in (True, -1, 99, 'gzip'):
            self.assert_error('invalid_options', lambda: self.api.available(format))
        for format in (self.api.Format.GZIP, self.api.Format.ZLIB, self.api.Format.DEFLATE):
            for level in (-1, 10, True):
                self.assert_error('invalid_options', lambda: self.api.compress(b'x', format, level))
            source = mock.Mock()
            self.assert_error('resource_limit', lambda: self.api.decompress(
                source, self.options(format, max_window_bytes=32767)))
            source.read.assert_not_called()

    def test_native_policy_validation_precedes_io_and_publication_checks_cancellation(self):
        """Invalid inner policies consume no source and late cancellation withholds decoded values."""
        source = mock.Mock()
        for protocol, limits in ((True, None), (99, None), (self.module.Protocol.JSON, object())):
            with self.assertRaises(ValueError):
                self.api.decode(self.module.Record, source, protocol, limits=limits)
        source.read.assert_not_called()
        cancelled = []
        class late_cancel:
            """Simulate cancellation during the inner native decode's last user hook."""
            @staticmethod
            def decode(data, protocol, limits):
                """Mark cancellation after outer frame validation and before publication."""
                cancelled.append(True)
                return object()
        def check():
            """Preserve the caller's late exception at the result-publication boundary."""
            if cancelled:
                raise RuntimeError('late cancellation')
        with self.assertRaisesRegex(RuntimeError, 'late cancellation'):
            self.api.decode(late_cancel, b'', self.module.Protocol.JSON,
                            self.options(self.api.Format.NONE, check_cancelled=check))
        binary_api = self.binary.__serializer_compression__
        for protocol in (self.binary.Protocol.JSON, self.binary.Protocol.BINARY_INTEGER,
                         self.binary.Protocol.BINARY_STRING):
            with self.assertRaisesRegex(ValueError, 'disabled during generation'):
                binary_api.decode(self.binary.Record, source, protocol)
            with self.assertRaisesRegex(ValueError, 'disabled during generation'):
                binary_api.encode(self.binary.Record(), protocol)
        source.read.assert_not_called()
        value = self.binary.Record()
        self.assertEqual(binary_api.decode(self.binary.Record, binary_api.encode(
            value, self.binary.Protocol.BINARY_NONE), self.binary.Protocol.BINARY_NONE).text, '')

    def test_zstd_window_floor_and_unsupported_frame_profiles(self):
        """Check standard magic, dictionaries, single-segment sizes and rounded history bounds."""
        format = self.api.Format.ZSTD
        if not self.api.available(format):
            self.skipTest('Python compression.zstd is unavailable')
        from compression import zstd
        payload = bytes(range(256)) * 256
        packed = zstd.compress(payload)
        self.assertEqual(self.api.decompress(packed, self.options(
            format, max_window_bytes=65537)), payload)
        self.assert_error('resource_limit', lambda: self.api.decompress(
            packed, self.options(format, max_window_bytes=65535)))
        for malformed in (b'\x50\x2a\x4d\x18\0\0\0\0',
                          b'\x28\xb5\x2f\xfd\x21\x01\0\x01\0\0'):
            self.assert_error('invalid_data', lambda: self.api.decompress(
                malformed, self.options(format)))
        encoder = zstd.ZstdCompressor(options={zstd.CompressionParameter.window_log: 16,
                                               zstd.CompressionParameter.content_size_flag: 0})
        streaming = encoder.compress(payload) + encoder.flush()
        self.assertEqual(self.api.decompress(streaming, self.options(
            format, max_window_bytes=65537)), payload)
        self.assert_error('resource_limit', lambda: self.api.decompress(
            streaming, self.options(format, max_window_bytes=65535)))

    def test_lz4_block_budget_and_unsupported_frame_profiles(self):
        """Reject oversized advertised blocks, legacy/skippable frames and dictionary headers."""
        format = self.api.Format.LZ4
        if not self.api.available(format):
            self.skipTest('Optional lz4.frame is unavailable')
        import lz4.frame
        payload = bytes(range(256)) * 4096
        packed = lz4.frame.compress(payload, block_size=lz4.frame.BLOCKSIZE_MAX4MB)
        self.assert_error('resource_limit', lambda: self.api.decompress(
            packed, self.options(format, max_window_bytes=65536)))
        for malformed in (b'\x50\x2a\x4d\x18\0\0\0\0', b'\x02\x21\x4c\x18',
                          b'\x04\x22\x4d\x18\x61\x40\0\0\0\0\0'):
            self.assert_error('invalid_data', lambda: self.api.decompress(
                malformed, self.options(format)))

    def test_output_callback_reentry_fences_active_operation(self):
        """Reject hook/sink reentry even when the callback ignores its nested exception."""
        for format in self.formats():
            for location in ('write-hook', 'finish-hook', 'sink-write', 'sink-finish'):
                with self.subTest(format=format.name, location=location):
                    armed = False
                    nested = []
                    writer = None

                    def reenter():
                        """Attempt exactly one nested operation without propagating its exception."""
                        nonlocal armed
                        if not armed:
                            return
                        armed = False
                        try:
                            if location in ('finish-hook', 'sink-write'):
                                writer.write(b'nested')
                            else:
                                writer.finish()
                        except BaseException as failure:
                            nested.append(failure)

                    class callback_sink(io.BytesIO):
                        """Record actual borrowed writes before entering an application callback."""
                        def __init__(self):
                            """Track write attempts without transferring sink ownership."""
                            super().__init__()
                            self.calls = 0

                        def write(self, data):
                            """Expose a same-wrapper callback after an externally visible write."""
                            self.calls += 1
                            count = super().write(data)
                            if location.startswith('sink-'):
                                reenter()
                            return count

                    sink = callback_sink()
                    limits = self.api.EncodeLimits(
                        check_cancelled=reenter if location.endswith('-hook') else None)
                    writer = self.api.OutputStream(sink, format, limits=limits)
                    if location == 'finish-hook':
                        writer.write(b'first')
                    calls = sink.calls
                    armed = True
                    with self.assertRaises(self.api.Error) as caught:
                        if location == 'finish-hook':
                            writer.finish()
                        else:
                            writer.write(b'outer')
                            if location.startswith('sink-'):
                                writer.finish()
                    self.assertEqual(caught.exception.code, 'invalid_options')
                    self.assertEqual(nested, [caught.exception])
                    self.assertEqual(sink.calls, calls + int(location.startswith('sink-')))
                    for operation in (lambda: writer.write(b'later'), writer.finish):
                        with self.assertRaisesRegex(RuntimeError, 'not active') as repeated:
                            operation()
                        self.assertIs(repeated.exception.__cause__, caught.exception)
                    self.assertEqual(sink.calls, calls + int(location.startswith('sink-')))

    def test_input_callback_reentry_fences_read_and_finish(self):
        """Prevent early/late read hooks and finish hooks from publishing a stale active state."""
        for format in self.formats():
            packed = self.api.compress(b'complete message', format)
            for location in ('read-before', 'read-after', 'finish'):
                with self.subTest(format=format.name, location=location):
                    armed = False
                    polls = 0
                    nested = []
                    reader = None

                    def reenter():
                        """Ignore one nested exception so the outer operation must recheck its state."""
                        nonlocal armed, polls
                        if not armed:
                            return
                        polls += 1
                        if location == 'read-after' and polls == 1:
                            return
                        armed = False
                        try:
                            if location == 'finish':
                                reader.read(0)
                            else:
                                reader.finish()
                        except BaseException as failure:
                            nested.append(failure)

                    reader = self.api.InputStream(io.BytesIO(packed),
                        self.options(format, check_cancelled=reenter))
                    if location == 'finish':
                        self.assertEqual(reader.read(), b'complete message')
                    armed = True
                    with self.assertRaises(self.api.Error) as caught:
                        if location == 'finish':
                            reader.finish()
                        else:
                            reader.read(1)
                    self.assertEqual(caught.exception.code, 'invalid_options')
                    self.assertEqual(nested, [caught.exception])
                    for operation in (lambda: reader.read(1), reader.finish):
                        with self.assertRaisesRegex(RuntimeError, 'not active') as repeated:
                            operation()
                        self.assertIs(repeated.exception.__cause__, caught.exception)

    def test_wrapper_first_failure_identity_is_sticky(self):
        """Retain original cancellation as the cause of the existing inactive-state RuntimeError."""
        sentinel = RuntimeError('original wrapper failure sentinel')
        for format in self.formats():
            armed = False

            def cancel():
                """Raise the same application-owned exception only after wrapper construction."""
                if armed:
                    raise sentinel

            writer = self.api.OutputStream(io.BytesIO(), format,
                limits=self.api.EncodeLimits(check_cancelled=cancel))
            packed = self.api.compress(b'complete message', format)
            reader = self.api.InputStream(io.BytesIO(packed),
                self.options(format, check_cancelled=cancel))
            armed = True
            for operation in (lambda: writer.write(b'x'), lambda: reader.read(1)):
                with self.subTest(format=format.name, operation=operation):
                    with self.assertRaises(RuntimeError) as caught:
                        operation()
                    self.assertIs(caught.exception, sentinel)
            armed = False
            for operation in (lambda: writer.write(b'later'), writer.finish,
                              lambda: reader.read(1), reader.finish):
                with self.subTest(format=format.name, operation=operation):
                    with self.assertRaisesRegex(RuntimeError, 'not active') as caught:
                        operation()
                    self.assertIs(caught.exception.__cause__, sentinel)

    def test_existing_compression_model_name_and_namespace_stability(self):
        """Preserve a formerly valid schema type named Compression and expose the stable namespace."""
        self.assertIs(self.module.Compression, self.api)
        self.assertIsNot(self.collision.Compression, self.collision.__serializer_compression__)
        value = self.collision.Compression()
        value.text = 'existing model'
        protocol = self.collision.Protocol.JSON
        api = self.collision.__serializer_compression__
        self.assertEqual(api.decode(self.collision.Compression, api.encode(
            value, protocol, api.Format.GZIP), protocol, api.DecodeOptions(api.Format.GZIP)).text,
            value.text)


def load_generated(compiler, directory, name, schema, options=()):
    """Run the real compiler and import its fresh emitted module without template substitution."""
    source = directory / (name + '.serializer')
    output = directory / (name + '.py')
    source.write_text(schema, encoding='utf-8')
    subprocess.run([str(compiler), '--input', str(source), '--language', 'python',
                    '--python.output', str(output), *options], check=True)
    spec = importlib.util.spec_from_file_location('serializer_compression_' + name, output)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main():
    """Generate actual consumers under ignored scratch and qualify every available provider."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--provider-path', type=Path,
                        help='Optional local provider directory, without changing the global environment')
    args = parser.parse_args()
    if args.provider_path is not None:
        sys.path.insert(0, str(args.provider_path.resolve()))
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='python-compression-', dir=build) as temporary:
        directory = Path(temporary)
        python_compression_test.module = load_generated(args.compiler.resolve(), directory, 'native', SCHEMA)
        python_compression_test.api = python_compression_test.module.__serializer_compression__
        python_compression_test.collision = load_generated(args.compiler.resolve(), directory, 'collision',
            'serializer version 1;\nclass compression { public string text (1); }\n')
        python_compression_test.binary = load_generated(args.compiler.resolve(), directory, 'binary',
            SCHEMA, ('--protocols', 'binary_none'))
        print('Python:', sys.version.split()[0], 'providers:',
              ', '.join(format.name for format in python_compression_test.api.Format
                        if python_compression_test.api.available(format)))
        result = unittest.TextTestRunner(verbosity=2).run(
            unittest.defaultTestLoader.loadTestsFromTestCase(python_compression_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
