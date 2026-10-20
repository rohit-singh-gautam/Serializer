// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import SerializerOptionalProvider

// Keep reference adapter implementation names inside the collision-selected helper.
extension __COMPRESSION_NAME__ {
    /// Preserve native provider failures that are neither frame nor admitted-window errors.
    final class NativeProviderFailure: Swift.Error {
        let status: Int32
        /// Retain an opaque provider status without payload excerpts.
        init(_ status: Int32) { self.status = status }
    }
    /// Build explicit factories against the caller-linked stable native C APIs.
    static func nativeProvider(_ format: __COMPRESSION_NAME__.Format) -> __COMPRESSION_NAME__.Provider {
        typealias C = __COMPRESSION_NAME__
        let identifier: Int32 = format == .zstd ? 1 : 2
        /// Wrap one owned native context without allowing it to receive application callbacks.
        func context(_ encoding: Bool, _ level: Int, _ size: Int, _ window: Int) throws -> C.Context {
            guard let state = serializer_swift_provider_create(identifier, encoding ? 1 : 0, Int32(level), size, Int32(window)) else {
                throw NativeProviderFailure(-3)
            }
            return C.Context(step: { input, output, finish in
                var consumed = input.count; var produced = output.count
                let result = input.withUnsafeBufferPointer { source in
                    output.withUnsafeMutableBufferPointer { destination in
                        serializer_swift_provider_step(state, source.baseAddress, &consumed, destination.baseAddress, &produced, finish ? 1 : 0)
                    }
                }
                if result == -1 { throw C.Failure(.invalidData, "Native provider rejected frame") }
                if result == -2 { throw C.Failure(.resourceLimit, "Native provider history exceeds limit") }
                if result < 0 { throw NativeProviderFailure(result) }
                return C.Progress(consumed: consumed, produced: produced, finished: result == 1)
            }, close: { serializer_swift_provider_close(state) })
        }
        return C.Provider(format: format,
            minimumLevel: format == .zstd ? Int(serializer_swift_provider_minimum_level()) : 0,
            maximumLevel: format == .zstd ? Int(serializer_swift_provider_maximum_level()) : Int(serializer_swift_provider_lz4_maximum_level()),
            defaultLevel: format == .zstd ? 3 : 0,
            makeEncoder: { level, size in try context(true, level, size, 0) },
            makeDecoder: { window in try context(false, 0, 0, window) })
    }

    /// Report the caller-linked implementations without loading any unselected module.
    static func nativeProviderVersions() -> String { String(cString:serializer_swift_provider_zstd_version())+" "+String(cString:serializer_swift_provider_lz4_version()) }
}
