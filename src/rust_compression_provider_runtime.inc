// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compressrust(
    /// Progress through caller-owned bounded slices; completion denotes one frame or one LZ4 block.
    #[derive(Clone, Copy, Debug)]
    pub struct Progress { pub bytes_read: usize, pub bytes_written: usize, pub finished: bool }
    /// Private per-operation encoding context; no application callbacks are passed into providers.
    pub trait Encoder {
        /// Process a bounded slice; Zstandard finishes on empty final input, LZ4 completes one block.
        fn process(&mut self, input: &[u8], output: &mut [u8], finish: bool) -> Result<Progress>;
    }
    /// Private per-operation decoding context; explicit history is supplied only for linked LZ4 blocks.
    pub trait Decoder {
        /// Process bounded input/output without retaining slices; report exact consumed/produced extents.
        fn process(&mut self, input: &[u8], output: &mut [u8], history: &[u8]) -> Result<Progress>;
    }
    /// Trusted, caller-owned optional capability; implementations must remain stable through the borrow.
    pub trait Provider {
        /// Identify this provider's one optional format without effects or data access.
        fn format(&self) -> Format;
        /// Admit capabilities and the full level policy before application callbacks or transport access.
        fn validate_encode(&self, level: i32) -> Result<()>;
        /// Admit capabilities and the history policy before application callbacks or transport access.
        fn validate_decode(&self, max_window_bytes: usize) -> Result<()>;
        /// Create owned codec state, pledging the complete serialized input size for Zstandard.
        fn encoder(&self, level: i32, input_bytes: usize) -> Result<Box<dyn Encoder + '_>>;
        /// Create owned codec state with the admitted conservative provider window.
        fn decoder(&self, max_window_bytes: usize) -> Result<Box<dyn Decoder + '_>>;
    }
    /// Probe explicit optional selection without installing a global registry or invoking application code.
    pub fn available_with_provider(format: Format, provider: Option<&dyn Provider>) -> bool {
        available(format) || matches!(format, Format::Zstd | Format::Lz4) && provider.is_some_and(|value| value.format() == format)
    }
    /// Admit optional encoding while retaining the original core policy and option struct fields.
    fn validate_encode_with_provider(options: EncodeOptions<'_>, provider: &dyn Provider) -> Result<()> {
        if available(options.format) { return validate_encode(options); }
        if provider.format() != options.format { return Err(failure(Code::InvalidOptions,"Mismatched compression provider")); }
        provider.validate_encode(options.level)
    }
    /// Floor non-power-of-two Zstandard budgets without overflowing the native unsigned width.
    fn optional_window(maximum: usize) -> usize {
        if maximum == 0 { 0 } else { 1usize << (usize::BITS-1-maximum.leading_zeros()) }
    }
    /// Admit every window/provider capability before cancellation, native codecs or borrowed I/O.
    fn validate_decode_with_provider(options: DecodeOptions<'_>, provider: &dyn Provider) -> Result<()> {
        if available(options.format) { return validate_decode(options); }
        if provider.format() != options.format { return Err(failure(Code::InvalidOptions,"Mismatched compression provider")); }
        provider.validate_decode(options.max_window_bytes)?;
        let minimum = if options.format == Format::Zstd {1024} else {65536};
        if options.max_window_bytes < minimum { return Err(failure(Code::ResourceLimit,"Compression history exceeds limit")); }
        Ok(())
    }
    /// Read bounded little-endian frame extents independently of native platform layout.
    fn little64(data: &[u8], position: usize, count: usize) -> Result<u64> {
        span(data,position,count)?;
        Ok((0..count).fold(0,|value,index| value | (u64::from(data[position+index]) << (index*8))))
    }
    /// Grow only admitted logical bytes, polling between bounded copies.
    fn optional_append(output: &mut Buffer<'_>, bytes: &[u8]) -> Result<()> {
        if bytes.len() > output.maximum-output.bytes.len() { return Err(failure(Code::ResourceLimit,"Compression output exceeds limit")); }
        reserve(&mut output.bytes,bytes.len())?;
        for block in bytes.chunks(BATCH_BYTES) { poll(output.check)?; output.bytes.extend_from_slice(block); }
        poll(output.check)
    }
    /// Append externally specified little-endian framing without object-layout assumptions.
    fn optional_word(output: &mut Buffer<'_>, value: u64, count: usize) -> Result<()> {
        for byte in value.to_le_bytes().iter().take(count) { output.push(*byte)?; } Ok(())
    }
    /// Reject impossible provider progress before changing offsets or observing returned bytes.
    fn optional_progress(progress: Progress, input: usize, output: usize) -> Result<()> {
        if progress.bytes_read > input || progress.bytes_written > output ||
            !progress.finished && progress.bytes_read == 0 && progress.bytes_written == 0 {
            return Err(failure(Code::InvalidData,"Invalid compression provider progress"));
        }
        Ok(())
    }
    /// Apply XXHash's externally specified 32-bit lane round with explicit modular arithmetic.
    fn xxh_round32(accumulator: u32, lane: u32) -> u32 {
        accumulator.wrapping_add(lane.wrapping_mul(2246822519)).rotate_left(13).wrapping_mul(2654435761)
    }
    /// Validate standard LZ4 descriptor, block and content digests without a provider dependency.
    fn xxh32(data: &[u8], check: Option<&dyn Fn() -> Result<()>>) -> Result<u32> {
        const P1: u32 = 2654435761; const P2: u32 = 2246822519; const P3: u32 = 3266489917;
        const P4: u32 = 668265263; const P5: u32 = 374761393;
        let mut position = 0;
        let mut digest = if data.len() >= 16 {
            let mut lanes = [P1.wrapping_add(P2),P2,0,0u32.wrapping_sub(P1)];
            while data.len()-position >= 16 {
                if position % CHECK_INTERVAL == 0 { poll(check)?; }
                for lane in &mut lanes { *lane = xxh_round32(*lane,little(data,position,4)?); position += 4; }
            }
            lanes[0].rotate_left(1).wrapping_add(lanes[1].rotate_left(7)).wrapping_add(lanes[2].rotate_left(12)).wrapping_add(lanes[3].rotate_left(18))
        } else { P5 };
        digest = digest.wrapping_add(data.len() as u32);
        while data.len()-position >= 4 {
            if position % CHECK_INTERVAL == 0 { poll(check)?; }
            digest = digest.wrapping_add(little(data,position,4)?.wrapping_mul(P3)).rotate_left(17).wrapping_mul(P4); position += 4;
        }
        for &byte in &data[position..] { digest = digest.wrapping_add(u32::from(byte).wrapping_mul(P5)).rotate_left(11).wrapping_mul(P1); }
        digest ^= digest >> 15; digest = digest.wrapping_mul(P2); digest ^= digest >> 13;
        digest = digest.wrapping_mul(P3); digest ^= digest >> 16; poll(check)?; Ok(digest)
    }
    /// Apply XXHash's standard 64-bit lane round with explicit wrapping multiplication.
    fn xxh_round64(accumulator: u64, lane: u64) -> u64 {
        accumulator.wrapping_add(lane.wrapping_mul(14029467366897019727)).rotate_left(31).wrapping_mul(11400714785074694791)
    }
    /// Independently verify the low 32 bits of Zstandard's XXHash64 content digest.
    fn xxh64(data: &[u8], check: Option<&dyn Fn() -> Result<()>>) -> Result<u64> {
        const P1: u64 = 11400714785074694791; const P2: u64 = 14029467366897019727;
        const P3: u64 = 1609587929392839161; const P4: u64 = 9650029242287828579; const P5: u64 = 2870177450012600261;
        let mut position = 0;
        let mut digest = if data.len() >= 32 {
            let mut lanes = [P1.wrapping_add(P2),P2,0,0u64.wrapping_sub(P1)];
            while data.len()-position >= 32 {
                if position % CHECK_INTERVAL == 0 { poll(check)?; }
                for lane in &mut lanes { *lane = xxh_round64(*lane,little64(data,position,8)?); position += 8; }
            }
            let mut value = lanes[0].rotate_left(1).wrapping_add(lanes[1].rotate_left(7)).wrapping_add(lanes[2].rotate_left(12)).wrapping_add(lanes[3].rotate_left(18));
            for lane in lanes { value ^= xxh_round64(0,lane); value = value.wrapping_mul(P1).wrapping_add(P4); } value
        } else { P5 };
        digest = digest.wrapping_add(data.len() as u64);
        while data.len()-position >= 8 {
            if position % CHECK_INTERVAL == 0 { poll(check)?; }
            digest ^= xxh_round64(0,little64(data,position,8)?); digest = digest.rotate_left(27).wrapping_mul(P1).wrapping_add(P4); position += 8;
        }
        if data.len()-position >= 4 { digest ^= u64::from(little(data,position,4)?).wrapping_mul(P1); digest = digest.rotate_left(23).wrapping_mul(P2).wrapping_add(P3); position += 4; }
        for &byte in &data[position..] { digest ^= u64::from(byte).wrapping_mul(P5); digest = digest.rotate_left(11).wrapping_mul(P1); }
        digest ^= digest >> 33; digest = digest.wrapping_mul(P2); digest ^= digest >> 29;
        digest = digest.wrapping_mul(P3); digest ^= digest >> 32; poll(check)?; Ok(digest)
    }
    /// Frame facts retain scalar metadata rather than an input-size-dependent block directory.
    struct OptionalFrame { size: Option<u64>, digest: Option<u32>, start: usize, independent: bool, block_maximum: usize }
    /// Check one RFC 8878 frame's exact extent, dictionary absence, content size and conservative window.
    fn zstd_frame(data: &[u8], options: DecodeOptions<'_>) -> Result<OptionalFrame> {
        if little(data,0,4)? != 0xfd2fb528 { return Err(failure(Code::InvalidData,"Expected one standard Zstandard frame")); }
        span(data,4,1)?; let descriptor = data[4]; let single = descriptor & 32 != 0; let mut position = 5;
        if descriptor & 8 != 0 { return Err(failure(Code::InvalidData,"Reserved Zstandard descriptor")); }
        let mut window = 0u64;
        if !single {
            span(data,position,1)?; let encoded = data[position]; position += 1;
            let base = 1u64 << (10+(encoded >> 3)); window = base+(base >> 3)*u64::from(encoded & 7);
        }
        let dictionary_bytes = match descriptor & 3 { 3 => 4, value => usize::from(value) };
        let dictionary = little64(data,position,dictionary_bytes)?; position += dictionary_bytes;
        if dictionary != 0 { return Err(failure(Code::InvalidData,"Zstandard dictionaries are unsupported")); }
        let flag = descriptor >> 6;
        let size_bytes = if flag == 0 {usize::from(single)} else {1usize << flag};
        let mut size = little64(data,position,size_bytes)?; position += size_bytes;
        if size_bytes == 2 { size += 256; }
        let size = (size_bytes != 0).then_some(size);
        if size.is_some_and(|value| value > options.max_output_bytes as u64) { return Err(failure(Code::ResourceLimit,"Zstandard content exceeds output limit")); }
        if single { window = size.unwrap(); }
        if window > optional_window(options.max_window_bytes) as u64 { return Err(failure(Code::ResourceLimit,"Zstandard history exceeds window limit")); }
        let block_maximum = window.min(128*1024);
        loop {
            poll(options.check_cancelled)?; let block = little(data,position,3)?; position += 3;
            let kind = (block >> 1) & 3; let count = u64::from(block >> 3);
            if kind == 3 || count > block_maximum { return Err(failure(Code::InvalidData,"Invalid Zstandard block")); }
            let stored = if kind == 1 {1} else {count as usize}; span(data,position,stored)?; position += stored;
            if block & 1 != 0 { break; }
        }
        let digest = if descriptor & 4 != 0 { let value = little(data,position,4)?; position += 4; Some(value) } else {None};
        if position != data.len() { return Err(failure(Code::InvalidData,"Trailing or concatenated Zstandard input")); }
        Ok(OptionalFrame {size,digest,start:0,independent:false,block_maximum:0})
    }
    /// Count one LZ4 token's extension bytes without overflowing the admitted block expansion.
    fn lz4_length(data: &[u8], position: &mut usize, initial: usize, maximum: usize, check: Option<&dyn Fn()->Result<()>>) -> Result<usize> {
        let mut value = initial;
        if initial == 15 { loop {
            poll(check)?; span(data,*position,1)?; let byte = usize::from(data[*position]); *position += 1;
            if value > maximum || byte > maximum-value { return Err(failure(Code::InvalidData,"LZ4 token exceeds block maximum")); }
            value += byte; if byte != 255 { break; }
        } }
        Ok(value)
    }
    /// Validate token lengths, available history and expanded count before provider allocation.
    fn lz4_count(data: &[u8], history: usize, maximum: usize, output_limit: usize, check: Option<&dyn Fn()->Result<()>>) -> Result<usize> {
        let (mut position,mut count) = (0,0);
        while position < data.len() {
            poll(check)?; let token = data[position]; position += 1;
            let literals = lz4_length(data,&mut position,usize::from(token >> 4),maximum,check)?;
            if literals > maximum-count { return Err(failure(Code::InvalidData,"LZ4 literals exceed block maximum")); }
            if literals > output_limit-count { return Err(failure(Code::ResourceLimit,"LZ4 expansion exceeds output limit")); }
            span(data,position,literals)?; position += literals; count += literals;
            if position == data.len() { return Ok(count); }
            let distance = little(data,position,2)? as usize; position += 2;
            if distance == 0 || distance > history+count { return Err(failure(Code::InvalidData,"LZ4 distance exceeds available history")); }
            let matched = lz4_length(data,&mut position,usize::from(token & 15),maximum,check)?;
            if matched > maximum.saturating_sub(4) || matched+4 > maximum-count { return Err(failure(Code::InvalidData,"LZ4 match exceeds block maximum")); }
            if matched+4 > output_limit-count { return Err(failure(Code::ResourceLimit,"LZ4 expansion exceeds output limit")); }
            count += matched+4;
        }
        Err(failure(Code::InvalidData,"Missing LZ4 final literals"))
    }
    /// Validate the complete standard frame, all stored checksums and block expansion before allocation.
    fn lz4_frame(data: &[u8], options: DecodeOptions<'_>) -> Result<OptionalFrame> {
        if little(data,0,4)? != 0x184d2204 { return Err(failure(Code::InvalidData,"Expected one standard LZ4 frame")); }
        span(data,4,2)?; let flags = data[4]; let block = data[5]; let id = (block >> 4) & 7;
        if flags >> 6 != 1 || flags & 3 != 0 || block & 0x8f != 0 || !(4..=7).contains(&id) { return Err(failure(Code::InvalidData,"Invalid or dictionary-dependent LZ4 descriptor")); }
)compressrust"
R"compressrust(        let maximum = 1usize << (8+2*id); let mut position = 6;
        if maximum > options.max_window_bytes { return Err(failure(Code::ResourceLimit,"LZ4 block history exceeds window limit")); }
        let size = if flags & 8 != 0 { let value = little64(data,position,8)?; position += 8; Some(value) } else {None};
        if size.is_some_and(|value| value > options.max_output_bytes as u64) { return Err(failure(Code::ResourceLimit,"LZ4 content exceeds output limit")); }
        span(data,position,1)?;
        if data[position] != ((xxh32(&data[4..position],options.check_cancelled)? >> 8) & 255) as u8 { return Err(failure(Code::InvalidData,"Invalid LZ4 descriptor checksum")); }
        position += 1; let start = position; let independent = flags & 32 != 0; let mut total = 0usize;
        loop {
            poll(options.check_cancelled)?; let block = little(data,position,4)?; position += 4; if block == 0 { break; }
            let count = (block & 0x7fffffff) as usize;
            if count > maximum { return Err(failure(Code::InvalidData,"LZ4 stored block exceeds maximum")); }
            span(data,position,count)?; let payload = &data[position..position+count]; position += count;
            if flags & 16 != 0 {
                let expected = little(data,position,4)?; position += 4;
                if expected != xxh32(payload,options.check_cancelled)? { return Err(failure(Code::InvalidData,"Invalid LZ4 block checksum")); }
            }
            let expanded = if block & 0x80000000 != 0 { count } else { lz4_count(payload,if independent {0} else {total.min(65536)},maximum,options.max_output_bytes-total,options.check_cancelled)? };
            if expanded > options.max_output_bytes-total { return Err(failure(Code::ResourceLimit,"LZ4 expansion exceeds output limit")); } total += expanded;
        }
        let digest = if flags & 4 != 0 { let value = little(data,position,4)?; position += 4; Some(value) } else {None};
        if position != data.len() || size.is_some_and(|value| value != total as u64) { return Err(failure(Code::InvalidData,"Invalid LZ4 content size or exact EOF")); }
        Ok(OptionalFrame {size:Some(total as u64),digest,start,independent,block_maximum:maximum})
    }
    /// Compare content digests independently of codec-provider validation.
    fn optional_content(frame: &OptionalFrame, data: &[u8], format: Format, check: Option<&dyn Fn()->Result<()>>) -> Result<()> {
        if frame.size.is_some_and(|value| value != data.len() as u64) { return Err(failure(Code::InvalidData,"Invalid compression content size")); }
        if let Some(expected) = frame.digest {
            let actual = if format == Format::Zstd {xxh64(data,check)? as u32} else {xxh32(data,check)?};
            if expected != actual { return Err(failure(Code::InvalidData,"Invalid compression content checksum")); }
        }
        poll(check)
    }
    /// Drive Zstandard stream contexts through fixed caller-owned buffers and exact completion.
    fn optional_zstd_compress(input: &[u8], options: EncodeOptions<'_>, provider: &dyn Provider) -> Result<Vec<u8>> {
        let mut context = provider.encoder(options.level,input.len())?;
        let mut output = Buffer::new(options.max_output_bytes,options.check_cancelled); let mut buffer = [0u8;BATCH_BYTES]; let mut position = 0;
        loop {
            poll(options.check_cancelled)?; let finish = position == input.len(); let end = position+BATCH_BYTES.min(input.len()-position);
            let step = context.process(&input[position..end],&mut buffer,finish)?; poll(options.check_cancelled)?;
            optional_progress(step,end-position,buffer.len())?; position += step.bytes_read;
            optional_append(&mut output,&buffer[..step.bytes_written])?;
            if step.finished { if !finish || position != input.len() { return Err(failure(Code::InvalidData,"Premature Zstandard encoder completion")); } break; }
        }
        let frame = zstd_frame(&output.bytes,DecodeOptions {format:Format::Zstd,max_input_bytes:output.bytes.len(),max_output_bytes:input.len(),max_window_bytes:usize::MAX,check_cancelled:options.check_cancelled})?;
        if frame.digest.is_none() || frame.size != Some(input.len() as u64) { return Err(failure(Code::InvalidData,"Zstandard provider omitted checksum or pledged size")); }
        optional_content(&frame,input,Format::Zstd,options.check_cancelled)?; Ok(output.bytes)
    }
    /// Frame bounded independent LZ4 blocks around an explicit caller-selected block encoder.
    fn optional_lz4_compress(input: &[u8], options: EncodeOptions<'_>, provider: &dyn Provider) -> Result<Vec<u8>> {
        const BLOCK_BYTES: usize = 65536;
        let mut context = provider.encoder(options.level,input.len())?;
        let mut output = Buffer::new(options.max_output_bytes,options.check_cancelled);
        optional_word(&mut output,0x184d2204,4)?; output.push(0x7c)?; output.push(0x40)?;
        optional_word(&mut output,input.len() as u64,8)?;
        let header = ((xxh32(&output.bytes[4..],options.check_cancelled)? >> 8) & 255) as u8; output.push(header)?;
        // Safe block providers may require padding beyond the reference wire-size bound.
        const ENCODE_BUFFER_BYTES: usize = BLOCK_BYTES*2;
        let mut buffer = Vec::new(); reserve(&mut buffer,ENCODE_BUFFER_BYTES)?; buffer.resize(ENCODE_BUFFER_BYTES,0);
        for block in input.chunks(BLOCK_BYTES) {
            poll(options.check_cancelled)?; let step = context.process(block,&mut buffer,true)?; poll(options.check_cancelled)?;
            optional_progress(step,block.len(),buffer.len())?;
            if !step.finished || step.bytes_read != block.len() { return Err(failure(Code::InvalidData,"Incomplete LZ4 encoded block")); }
            let raw = step.bytes_written >= block.len(); let payload = if raw {block} else {&buffer[..step.bytes_written]};
            if !raw && lz4_count(payload,0,BLOCK_BYTES,block.len(),options.check_cancelled)? != block.len() { return Err(failure(Code::InvalidData,"Invalid LZ4 encoded block size")); }
            optional_word(&mut output,payload.len() as u64 | if raw {0x80000000} else {0},4)?;
            optional_append(&mut output,payload)?; optional_word(&mut output,u64::from(xxh32(payload,options.check_cancelled)?),4)?;
        }
        optional_word(&mut output,0,4)?; optional_word(&mut output,u64::from(xxh32(input,options.check_cancelled)?),4)?;
        poll(options.check_cancelled)?; Ok(output.bytes)
    }
    /// Compress through an explicit borrowed provider while keeping default calls dependency-free.
    pub fn compress_with_provider(input: &[u8], options: EncodeOptions<'_>, provider: &dyn Provider) -> Result<Vec<u8>> {
        validate_encode_with_provider(options,provider)?; poll(options.check_cancelled)?;
        if available(options.format) { return compress(input,options); }
        if input.len() > options.max_input_bytes { return Err(failure(Code::ResourceLimit,"Serialized input exceeds limit")); }
        if options.format == Format::Zstd { optional_zstd_compress(input,options,provider) } else {optional_lz4_compress(input,options,provider)}
    }
    /// Decode one preflighted Zstandard frame and reject provider progress beyond its exact extent.
    fn optional_zstd_decompress(input: &[u8], options: DecodeOptions<'_>, provider: &dyn Provider, frame: &OptionalFrame) -> Result<Vec<u8>> {
        let mut context = provider.decoder(optional_window(options.max_window_bytes))?;
        let mut output = Buffer::new(options.max_output_bytes,options.check_cancelled); let mut buffer = [0u8;BATCH_BYTES]; let mut position = 0;
        loop {
            poll(options.check_cancelled)?; let end = position+BATCH_BYTES.min(input.len()-position);
            let remaining = options.max_output_bytes-output.bytes.len(); let capacity = if remaining >= BATCH_BYTES {BATCH_BYTES} else {remaining+1};
            let step = context.process(&input[position..end],&mut buffer[..capacity],&[])?; poll(options.check_cancelled)?;
            optional_progress(step,end-position,capacity)?; position += step.bytes_read; optional_append(&mut output,&buffer[..step.bytes_written])?;
            if step.finished { if position != input.len() { return Err(failure(Code::InvalidData,"Premature Zstandard decoder completion")); } break; }
        }
        optional_content(frame,&output.bytes,Format::Zstd,options.check_cancelled)?; Ok(output.bytes)
    }
    /// Decode preflighted LZ4 blocks with explicit bounded prior history and exact expanded slices.
    fn optional_lz4_decompress(input: &[u8], options: DecodeOptions<'_>, provider: &dyn Provider, frame: &OptionalFrame) -> Result<Vec<u8>> {
        let mut context = provider.decoder(options.max_window_bytes)?;
        let mut output = Buffer::new(options.max_output_bytes,options.check_cancelled); let mut position = frame.start;
        loop {
            poll(options.check_cancelled)?; let block = little(input,position,4)?; position += 4; if block == 0 {break;}
            let count = (block & 0x7fffffff) as usize; let payload = &input[position..position+count]; position += count;
            if input[4] & 16 != 0 { position += 4; }
            if block & 0x80000000 != 0 { optional_append(&mut output,payload)?; continue; }
            let history_size = if frame.independent {0} else {output.bytes.len().min(65536)};
            let expected = lz4_count(payload,history_size,frame.block_maximum,options.max_output_bytes-output.bytes.len(),options.check_cancelled)?;
            let mut expanded = Vec::new(); reserve(&mut expanded,expected)?; expanded.resize(expected,0);
            let history = &output.bytes[output.bytes.len()-history_size..];
            poll(options.check_cancelled)?; let step = context.process(payload,&mut expanded,history)?; poll(options.check_cancelled)?;
            optional_progress(step,payload.len(),expected)?;
            if !step.finished || step.bytes_read != payload.len() || step.bytes_written != expected { return Err(failure(Code::InvalidData,"Incomplete LZ4 decoded block")); }
            optional_append(&mut output,&expanded)?;
        }
        optional_content(frame,&output.bytes,Format::Lz4,options.check_cancelled)?; Ok(output.bytes)
    }
    /// Validate a complete optional frame before expansion or native candidate decoding.
    pub fn decompress_with_provider(input: &[u8], options: DecodeOptions<'_>, provider: &dyn Provider) -> Result<Vec<u8>> {
        validate_decode_with_provider(options,provider)?; poll(options.check_cancelled)?;
        if available(options.format) { return decompress(input,options); }
        if input.len() > options.max_input_bytes { return Err(failure(Code::ResourceLimit,"Compressed input exceeds limit")); }
        if options.format == Format::Zstd {
            let frame = zstd_frame(input,options)?; optional_zstd_decompress(input,options,provider,&frame)
        } else { let frame = lz4_frame(input,options)?; optional_lz4_decompress(input,options,provider,&frame) }
    }
    /// Admit explicit capabilities before the unchanged owning model encoder callback.
    pub fn encode_with_provider(encoder: impl FnOnce()->Result<Vec<u8>>, options: EncodeOptions<'_>, provider: &dyn Provider) -> Result<Vec<u8>> {
        validate_encode_with_provider(options,provider)?; poll(options.check_cancelled)?; let input = encoder()?;
        poll(options.check_cancelled)?; compress_with_provider(&input,options,provider)
    }
    /// Withhold a fresh candidate until exact framing and final native cancellation checks complete.
    pub fn decode_with_provider<T>(input: &[u8], decoder: impl FnOnce(&[u8])->Result<T>, options: DecodeOptions<'_>, provider: &dyn Provider) -> Result<T> {
        let bytes = decompress_with_provider(input,options,provider)?; poll(options.check_cancelled)?;
        let candidate = decoder(&bytes)?; poll(options.check_cancelled)?; Ok(candidate)
    }
    /// Read a complete optional frame through the same borrowed wrapper lifecycle.
    pub fn decompress_from_with_provider<R: std::io::Read>(source: &mut R, options: DecodeOptions<'_>, provider: &dyn Provider) -> Result<Vec<u8>> {
        let mut input = Input::with_provider(source,options,provider)?; let mut bytes = Vec::new(); let mut buffer = [0u8;BATCH_BYTES];
        loop { let count = input.read(&mut buffer)?; if count == 0 { break; } reserve(&mut bytes,count)?; bytes.extend_from_slice(&buffer[..count]); }
        input.finish()?; Ok(bytes)
    }
)compressrust"
