// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
//! Explicit optional adapters instantiated against a caller-selected generated compression helper.
#[cfg(feature="zstd")]
#[doc(hidden)]
pub use zstd;
/// Define a private-context zstd provider module for the supplied generated helper path.
#[cfg(feature="zstd")]
#[macro_export]
macro_rules! zstd_provider {
    ($module:ident, $helper:path) => { pub mod $module {
        use $helper as c;
        use std::sync::Arc;
        use $crate::zstd::zstd_safe as safe;
        /// Construct bounded diagnostics independently of application callback objects.
        fn failure(code: c::Code, message: &'static str) -> c::Error { Arc::new(c::Failure {code,message,cause:None}) }

        /// Retain an unexpected native provider error with its original numeric code.
        #[derive(Debug)]
        struct ZstdError(usize);
        impl std::fmt::Display for ZstdError {
            /// Format only the provider's stable bounded diagnostic.
            fn fmt(&self, output: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { output.write_str(safe::get_error_name(self.0)) }
        }
        impl std::error::Error for ZstdError {}

        /// Classify only explicit data/window failures; unexpected provider errors keep their typed provenance.
        fn native_error(code: usize) -> c::Error {
            use safe::zstd_sys::ZSTD_ErrorCode as E;
            let number = code.wrapping_neg();
            if number == E::ZSTD_error_frameParameter_windowTooLarge as usize { return failure(c::Code::ResourceLimit,"Native Zstandard window exceeds limit"); }
            if [E::ZSTD_error_prefix_unknown,E::ZSTD_error_version_unsupported,E::ZSTD_error_frameParameter_unsupported,
                E::ZSTD_error_corruption_detected,E::ZSTD_error_checksum_wrong,E::ZSTD_error_literals_headerWrong,
                E::ZSTD_error_dictionary_corrupted,E::ZSTD_error_dictionary_wrong,E::ZSTD_error_srcSize_wrong]
                .iter().any(|value| number == *value as usize) { return failure(c::Code::InvalidData,"Invalid Zstandard provider data"); }
            c::into_error(ZstdError(code))
        }

        /// Pinned explicit capability; linking this adapter never changes the generated default profile.
        pub struct Zstd;
        impl c::Provider for Zstd {
            /// Report the selected standard format without global state.
            fn format(&self) -> c::Format { c::Format::Zstd }
            /// Admit the complete provider-advertised integer interval without application effects.
            fn validate_encode(&self, level: i32) -> c::Result<()> {
                if !$crate::zstd::compression_level_range().contains(&level) { return Err(failure(c::Code::InvalidOptions,"Unsupported Zstandard compression level")); }
                let _context = safe::CCtx::try_create().ok_or_else(|| failure(c::Code::ResourceLimit,"Zstandard context allocation failed"))?; Ok(())
            }
            /// Validate deployment and minimum history before any borrowed input or application callback.
            fn validate_decode(&self, maximum: usize) -> c::Result<()> {
                if maximum < 1024 { return Err(failure(c::Code::ResourceLimit,"Zstandard window exceeds limit")); }
                let _context = safe::DCtx::try_create().ok_or_else(|| failure(c::Code::ResourceLimit,"Zstandard context allocation failed"))?; Ok(())
            }
            /// Configure a private single-threaded checksummed context with the exact known input size.
            fn encoder(&self, level: i32, input_bytes: usize) -> c::Result<Box<dyn c::Encoder + '_>> {
                let mut context = safe::CCtx::try_create().ok_or_else(|| failure(c::Code::ResourceLimit,"Zstandard context allocation failed"))?;
                context.set_parameter(safe::CParameter::CompressionLevel(level)).map_err(native_error)?;
                context.set_parameter(safe::CParameter::ChecksumFlag(true)).map_err(native_error)?;
                context.set_parameter(safe::CParameter::ContentSizeFlag(true)).map_err(native_error)?;
                context.set_parameter(safe::CParameter::NbWorkers(0)).map_err(native_error)?;
                context.set_pledged_src_size(Some(input_bytes as u64)).map_err(native_error)?;
                Ok(Box::new(ZstdEncoder(context)))
            }
            /// Apply the generated ceiling to a fresh decoder, without dictionaries or context reuse.
            fn decoder(&self, maximum: usize) -> c::Result<Box<dyn c::Decoder + '_>> {
                let mut context = safe::DCtx::try_create().ok_or_else(|| failure(c::Code::ResourceLimit,"Zstandard context allocation failed"))?;
                // A larger caller budget must not become an out-of-range native parameter.
                let native_maximum_log = if usize::BITS == 64 {31} else {30};
                context.set_parameter(safe::DParameter::WindowLogMax((usize::BITS-1-maximum.leading_zeros()).min(native_maximum_log))).map_err(native_error)?;
                Ok(Box::new(ZstdDecoder(context)))
            }
        }

        /// RAII releases native encoding workspace on success, error or unwind.
        struct ZstdEncoder(safe::CCtx<'static>);
        impl c::Encoder for ZstdEncoder {
            /// Consume only caller-bounded buffers; finalization makes one exact standard frame.
            fn process(&mut self, input: &[u8], output: &mut [u8], finish: bool) -> c::Result<c::Progress> {
                let mut source = safe::InBuffer::around(input); let mut destination = safe::OutBuffer::around(output);
                let directive = if finish {safe::zstd_sys::ZSTD_EndDirective::ZSTD_e_end} else {safe::zstd_sys::ZSTD_EndDirective::ZSTD_e_continue};
                let remaining = self.0.compress_stream2(&mut destination,&mut source,directive).map_err(native_error)?;
                Ok(c::Progress {bytes_read:source.pos(),bytes_written:destination.pos(),finished:finish && remaining == 0})
            }
        }
        /// RAII releases native decoding history on success, error or unwind.
        struct ZstdDecoder(safe::DCtx<'static>);
        impl c::Decoder for ZstdDecoder {
            /// Stop at the first native frame end; generated framing independently rejects any suffix.
            fn process(&mut self, input: &[u8], output: &mut [u8], history: &[u8]) -> c::Result<c::Progress> {
                assert!(history.is_empty());
                let mut source = safe::InBuffer::around(input); let mut destination = safe::OutBuffer::around(output);
                let remaining = self.0.decompress_stream(&mut destination,&mut source).map_err(native_error)?;
                Ok(c::Progress {bytes_read:source.pos(),bytes_written:destination.pos(),finished:remaining == 0})
            }
        }

    } };
}
#[cfg(feature="lz4")]
#[doc(hidden)]
pub use lz4_flex;
/// Define a private-context lz4 provider module for the supplied generated helper path.
#[cfg(feature="lz4")]
#[macro_export]
macro_rules! lz4_provider {
    ($module:ident, $helper:path) => { pub mod $module {
        use $helper as c;
        use std::sync::Arc;
        /// Construct bounded diagnostics independently of application callback objects.
        fn failure(code: c::Code, message: &'static str) -> c::Error { Arc::new(c::Failure {code,message,cause:None}) }

        /// Pinned pure-Rust safe block capability; generated code owns every frame byte and checksum.
        pub struct Lz4;
        impl c::Provider for Lz4 {
            /// Report explicit standard LZ4 framing.
            fn format(&self) -> c::Format { c::Format::Lz4 }
            /// This safe fast encoder has one supported level; it does not pretend to expose HC levels.
            fn validate_encode(&self, level: i32) -> c::Result<()> {
                if level != 0 { return Err(failure(c::Code::InvalidOptions,"The pinned LZ4 adapter supports fast level zero")); } Ok(())
            }
            /// Admit one standard minimum block before any application callback or source read.
            fn validate_decode(&self, maximum: usize) -> c::Result<()> {
                if maximum < 65536 { return Err(failure(c::Code::ResourceLimit,"LZ4 block history exceeds limit")); } Ok(())
            }
            /// Create stateless block support after level admission.
            fn encoder(&self, level: i32, _input_bytes: usize) -> c::Result<Box<dyn c::Encoder + '_>> {
                self.validate_encode(level)?; Ok(Box::new(Lz4Codec))
            }
            /// History remains an explicit borrowed slice owned and bounded by the generated frame engine.
            fn decoder(&self, maximum: usize) -> c::Result<Box<dyn c::Decoder + '_>> {
                self.validate_decode(maximum)?; Ok(Box::new(Lz4Codec))
            }
        }
        /// No provider-global or cross-frame state is retained by safe block calls.
        struct Lz4Codec;
        impl c::Encoder for Lz4Codec {
            /// Encode one bounded independent block into generated preallocated storage.
            fn process(&mut self, input: &[u8], output: &mut [u8], finish: bool) -> c::Result<c::Progress> {
                assert!(finish);
                let count = $crate::lz4_flex::block::compress_into(input,output).map_err(c::into_error)?;
                Ok(c::Progress {bytes_read:input.len(),bytes_written:count,finished:true})
            }
        }
        impl c::Decoder for Lz4Codec {
            /// Decode linked blocks using the documented dictionary API, without a synthesized prefix.
            fn process(&mut self, input: &[u8], output: &mut [u8], history: &[u8]) -> c::Result<c::Progress> {
                let count = $crate::lz4_flex::block::decompress_into_with_dict(input,output,history)
                    .map_err(|_error| failure(c::Code::InvalidData,"Invalid LZ4 compressed block"))?;
                Ok(c::Progress {bytes_read:input.len(),bytes_written:count,finished:true})
            }
        }
    } };
}
