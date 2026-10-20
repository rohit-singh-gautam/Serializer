// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"compressrust(/// Dependency-free default framing with explicitly borrowed optional provider capabilities.
pub mod @compression_name@ {
    use std::sync::Arc;

    /// Concrete application, transport and cancellation errors retain their types and shared identity.
    pub type Error = Arc<dyn std::error::Error + Send + Sync>;
    /// Fallible owning and borrowed operations share one explicit error-handle contract.
    pub type Result<T> = std::result::Result<T, Error>;
    /// Caller-selected outer framing never selects the inner native protocol.
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum Format { None, Gzip, Zlib, Deflate, Zstd, Lz4 }
    /// Compression failures are separate from unchanged native and transport error objects.
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum Code { Unavailable, InvalidOptions, InvalidData, ResourceLimit }
    /// Bounded diagnostics contain no payload excerpts; inactive wrappers retain their first cause.
    #[derive(Debug)]
    pub struct Failure { pub code: Code, pub message: &'static str, pub cause: Option<Error> }
    impl std::fmt::Display for Failure {
        /// Describe a stable category without formatting untrusted bytes.
        fn fmt(&self, output: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { output.write_str(self.message) }
    }
    impl std::error::Error for Failure {
        /// Expose the retained original failure through the standard error chain.
        fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
            self.cause.as_ref().map(|error| error.as_ref() as &(dyn std::error::Error + 'static))
        }
    }
    /// Adapt a concrete native or transport error once, without replacing its original value.
    pub fn into_error<E: std::error::Error + Send + Sync + 'static>(error: E) -> Error { Arc::new(error) }
    /// Construct one private shared compression failure.
    fn failure(code: Code, message: &'static str) -> Error { Arc::new(Failure { code, message, cause: None }) }
    /// Bound logical complete-message bytes; callback allocations remain caller-owned.
    #[derive(Clone, Copy)]
    pub struct EncodeOptions<'a> {
        pub format: Format, pub max_input_bytes: usize, pub max_output_bytes: usize,
        pub level: i32, pub check_cancelled: Option<&'a dyn Fn() -> Result<()>>,
    }
    impl Default for EncodeOptions<'_> {
        /// Select identity bytes, ordinary finite caps and no application callback.
        fn default() -> Self { Self { format: Format::None, max_input_bytes: 64*1024*1024,
            max_output_bytes: 64*1024*1024, level: 6, check_cancelled: None } }
    }
    /// Compression history limits remain independent of generated native object limits.
    #[derive(Clone, Copy)]
    pub struct DecodeOptions<'a> {
        pub format: Format, pub max_input_bytes: usize, pub max_output_bytes: usize,
        pub max_window_bytes: usize, pub check_cancelled: Option<&'a dyn Fn() -> Result<()>>,
    }
    impl Default for DecodeOptions<'_> {
        /// Require explicit framing while allowing a bounded ordinary history window.
        fn default() -> Self { Self { format: Format::None, max_input_bytes: 64*1024*1024,
            max_output_bytes: 64*1024*1024, max_window_bytes: 8*1024*1024, check_cancelled: None } }
    }
    const BATCH_BYTES: usize = 32768;
    const WINDOW_BYTES: usize = 32768;
    const CHECK_INTERVAL: usize = 4096;
    /// Probe the standalone profile without invoking application code or provider loading.
    pub fn available(format: Format) -> bool { !matches!(format, Format::Zstd | Format::Lz4) }
    /// Poll bounded work without changing the caller's original error object.
    fn poll(check: Option<&dyn Fn() -> Result<()>>) -> Result<()> { if let Some(check) = check { check()?; } Ok(()) }
    /// Admit every encoder policy before native, cancellation or transport callbacks.
    fn validate_encode(options: EncodeOptions<'_>) -> Result<()> {
        if !(0..=9).contains(&options.level) { return Err(failure(Code::InvalidOptions, "Invalid compression level")); }
        if !available(options.format) { return Err(failure(Code::Unavailable, "Selected compression provider is unavailable")); }
        Ok(())
    }
    /// Require conservative DEFLATE history admission before callbacks or input reads.
    fn validate_decode(options: DecodeOptions<'_>) -> Result<()> {
        if !available(options.format) { return Err(failure(Code::Unavailable, "Selected compression provider is unavailable")); }
        if options.format != Format::None && options.max_window_bytes < WINDOW_BYTES {
            return Err(failure(Code::ResourceLimit, "DEFLATE window exceeds limit"));
        }
        Ok(())
    }
    /// Check byte spans before indexing, addition or subtraction of untrusted extents.
    fn span(data: &[u8], position: usize, count: usize) -> Result<()> {
        if position > data.len() || count > data.len()-position { return Err(failure(Code::InvalidData, "Truncated compression data")); }
        Ok(())
    }
    /// Decode externally specified little-endian words independently of native object layout.
    fn little(data: &[u8], position: usize, count: usize) -> Result<u32> {
        span(data, position, count)?;
        Ok((0..count).fold(0, |value, index| value | (u32::from(data[position+index]) << (index*8))))
    }
    /// Calculate CRC-32 with fixed workspace and bounded cooperative intervals.
    fn crc(data: &[u8], check: Option<&dyn Fn() -> Result<()>>) -> Result<u32> {
        let mut value = u32::MAX;
        for (index, byte) in data.iter().enumerate() {
            if index % CHECK_INTERVAL == 0 { poll(check)?; }
            value ^= u32::from(*byte);
            for _ in 0..8 { value = (value >> 1) ^ if value & 1 == 0 { 0 } else { 0xedb88320 }; }
        }
        poll(check)?; Ok(!value)
    }
    /// Bound intermediate Adler sums before overflow.
    fn adler(data: &[u8], check: Option<&dyn Fn() -> Result<()>>) -> Result<u32> {
        let (mut first, mut second) = (1u32, 0u32);
        for (index, byte) in data.iter().enumerate() {
            if index % CHECK_INTERVAL == 0 { poll(check)?; }
            first = (first + u32::from(*byte)) % 65521; second = (second + first) % 65521;
        }
        poll(check)?; Ok((second << 16) | first)
    }
    /// Convert actual allocation failure into a bounded compression category.
    fn reserve(bytes: &mut Vec<u8>, additional: usize) -> Result<()> {
        bytes.try_reserve(additional).map_err(|error| Arc::new(Failure { code: Code::ResourceLimit,
            message: "Compression allocation failed", cause: Some(into_error(error)) }) as Error)
    }
    /// Own one logical output; allocator spare capacity is not a process-memory guarantee.
    struct Buffer<'a> { bytes: Vec<u8>, maximum: usize, check: Option<&'a dyn Fn() -> Result<()>> }
    impl<'a> Buffer<'a> {
        /// Start with no untrusted-size allocation.
        fn new(maximum: usize, check: Option<&'a dyn Fn() -> Result<()>>) -> Self { Self { bytes: Vec::new(), maximum, check } }
        /// Admit bytes before growth and poll at bounded output intervals.
        fn push(&mut self, byte: u8) -> Result<()> {
            if self.bytes.len() == self.maximum { return Err(failure(Code::ResourceLimit, "Compression output exceeds limit")); }
            if self.bytes.len() % BATCH_BYTES == 0 { poll(self.check)?; }
            self.bytes.try_reserve(1).map_err(|_| failure(Code::ResourceLimit, "Compression allocation failed"))?;
            self.bytes.push(byte); Ok(())
        }
        /// Append a word with explicit standard byte order.
        fn word(&mut self, value: u32, big_endian: bool) -> Result<()> {
            for byte in if big_endian { value.to_be_bytes() } else { value.to_le_bytes() } { self.push(byte)?; } Ok(())
        }
    }
    /// Consume bounded little-bit-endian fields from immutable in-memory input.
    struct Bits<'a> { data: &'a [u8], position: usize, value: u32, count: usize, work: usize,
        check: Option<&'a dyn Fn() -> Result<()>> }
    impl<'a> Bits<'a> {
        /// Start at an already validated frame header boundary.
        fn new(data: &'a [u8], start: usize, check: Option<&'a dyn Fn() -> Result<()>>) -> Self {
            Self { data, position: start, value: 0, count: 0, work: 0, check }
        }
        /// Read at most sixteen bits without pulling bytes beyond the exact grammar requirement.
        fn read(&mut self, size: usize) -> Result<usize> {
            self.work = (self.work + 1) % CHECK_INTERVAL;
            if self.work == 0 { poll(self.check)?; }
            while self.count < size {
                if self.position == self.data.len() { return Err(failure(Code::InvalidData, "Truncated DEFLATE bits")); }
                self.value |= u32::from(self.data[self.position]) << self.count;
                self.position += 1; self.count += 8;
            }
            let result = self.value & ((1u32 << size)-1);
            self.value >>= size; self.count -= size; Ok(result as usize)
        }
        /// Stored blocks discard only padding in the current byte.
        fn align(&mut self) { self.value = 0; self.count = 0; }
    }
    /// Fixed-size canonical Huffman tables never allocate from untrusted code counts.
    #[derive(Clone)]
    struct Huffman { counts: [usize; 16], first: [usize; 16], offsets: [usize; 16], symbols: [usize; 288], maximum: usize }
    impl Huffman {
        /// Reject over/incomplete trees except the standard single-symbol or unused-distance cases.
        fn new(lengths: &[usize], code_lengths: bool, allow_empty: bool) -> Result<Self> {
            let mut result = Self { counts: [0;16], first: [0;16], offsets: [0;16], symbols: [0;288], maximum: 0 };
            let mut used = 0;
            for &size in lengths {
                if size > 15 { return Err(failure(Code::InvalidData, "Invalid DEFLATE code length")); }
                if size != 0 { result.counts[size] += 1; used += 1; result.maximum = result.maximum.max(size); }
            }
            if used == 0 {
                if allow_empty { return Ok(result); }
                return Err(failure(Code::InvalidData, "Empty DEFLATE Huffman tree"));
            }
            let mut left = 1i32;
            for size in 1..=15 {
                left = (left << 1) - result.counts[size] as i32;
                if left < 0 { return Err(failure(Code::InvalidData, "Oversubscribed DEFLATE tree")); }
            }
            if left != 0 && (code_lengths || used != 1 || result.maximum != 1) {
                return Err(failure(Code::InvalidData, "Incomplete DEFLATE tree"));
            }
            let (mut code, mut offset) = (0, 0);
            for size in 1..=15 {
                code = (code + result.counts[size-1]) << 1; result.first[size] = code;
                result.offsets[size] = offset; offset += result.counts[size];
            }
            let mut next = result.offsets;
            for (symbol, &size) in lengths.iter().enumerate() {
                if size != 0 { result.symbols[next[size]] = symbol; next[size] += 1; }
            }
            Ok(result)
        }
        /// Read canonical code bits in most-significant-code-bit order.
        fn decode(&self, bits: &mut Bits<'_>) -> Result<usize> {
            let mut code = 0;
            for size in 1..=self.maximum {
                code = (code << 1) | bits.read(1)?;
                if code >= self.first[size] && code-self.first[size] < self.counts[size] {
                    return Ok(self.symbols[self.offsets[size]+code-self.first[size]]);
                }
            }
            Err(failure(Code::InvalidData, "Invalid DEFLATE Huffman code"))
        }
    }
    const LENGTH_BASE: [usize;29] = [3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258];
    const LENGTH_EXTRA: [usize;29] = [0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0];
    const DISTANCE_BASE: [usize;30] = [1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577];
    const DISTANCE_EXTRA: [usize;30] = [0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13];
    const CODE_ORDER: [usize;19] = [16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15];
    /// Construct one standard fixed alphabet without heap allocation.
    fn fixed(distance: bool) -> Result<Huffman> {
        let mut lengths = [0usize;288]; let count = if distance {32} else {288};
        for (index, length) in lengths[..count].iter_mut().enumerate() {
            *length = if distance {5} else if index < 144 {8} else if index < 256 {9} else if index < 280 {7} else {8};
        }
        Huffman::new(&lengths[..count], false, false)
    }
    /// Validate/count before expansion; the second pass reserves only the exact admitted output.
    fn inflate(data: &[u8], start: usize, maximum: usize, window: usize,
        check: Option<&dyn Fn() -> Result<()>>, expand: bool) -> Result<(usize, usize, Vec<u8>)> {
        let mut bits = Bits::new(data, start, check); let mut output = 0; let mut bytes = Vec::new();
        if expand { reserve(&mut bytes, maximum)?; }
        let fixed_literal = fixed(false)?; let fixed_distance = fixed(true)?;
        loop {
            poll(check)?; let final_block = bits.read(1)?; let kind = bits.read(2)?;
            if kind == 0 {
                bits.align(); let size = bits.read(16)?; let complement = bits.read(16)?;
                if size ^ 65535 != complement { return Err(failure(Code::InvalidData, "Invalid stored DEFLATE length")); }
                span(data, bits.position, size)?;
                if size > maximum-output { return Err(failure(Code::ResourceLimit, "Expanded output exceeds limit")); }
                if expand {
                    for index in 0..size { if index % CHECK_INTERVAL == 0 { poll(check)?; } bytes.push(data[bits.position+index]); }
                }
                bits.position += size; output += size;
            } else {
                if kind == 3 { return Err(failure(Code::InvalidData, "Reserved DEFLATE block type")); }
                let (mut literal, mut distance) = (fixed_literal.clone(), fixed_distance.clone());
                if kind == 2 {
                    let literal_count = bits.read(5)?+257; let distance_count = bits.read(5)?+1; let code_count = bits.read(4)?+4;
                    if literal_count > 286 { return Err(failure(Code::InvalidData, "Invalid DEFLATE literal alphabet")); }
                    let mut code_lengths = [0usize;19];
                    for index in 0..code_count { code_lengths[CODE_ORDER[index]] = bits.read(3)?; }
                    let codes = Huffman::new(&code_lengths, true, false)?;
)compressrust"
R"compressrust(                    let total = literal_count+distance_count; let mut lengths = [0usize;320]; let mut position = 0;
                    while position < total {
                        let symbol = codes.decode(&mut bits)?;
                        if symbol < 16 { lengths[position] = symbol; position += 1; }
                        else {
                            let count = bits.read(if symbol == 16 {2} else if symbol == 17 {3} else {7})? + if symbol == 18 {11} else {3};
                            if (symbol == 16 && position == 0) || count > total-position { return Err(failure(Code::InvalidData, "Invalid DEFLATE length repeat")); }
                            let value = if symbol == 16 { lengths[position-1] } else {0};
                            lengths[position..position+count].fill(value); position += count;
                        }
                    }
                    if lengths[256] == 0 { return Err(failure(Code::InvalidData, "Missing DEFLATE end code")); }
                    literal = Huffman::new(&lengths[..literal_count], false, false)?;
                    distance = Huffman::new(&lengths[literal_count..total], false, true)?;
                }
                loop {
                    let symbol = literal.decode(&mut bits)?;
                    if symbol == 256 { break; }
                    if symbol < 256 {
                        if output == maximum { return Err(failure(Code::ResourceLimit, "Expanded output exceeds limit")); }
                        output += 1; if expand { bytes.push(symbol as u8); } continue;
                    }
                    let length_code = symbol-257;
                    if length_code >= LENGTH_BASE.len() { return Err(failure(Code::InvalidData, "Reserved DEFLATE length")); }
                    let count = LENGTH_BASE[length_code]+bits.read(LENGTH_EXTRA[length_code])?;
                    let distance_code = distance.decode(&mut bits)?;
                    if distance_code >= DISTANCE_BASE.len() { return Err(failure(Code::InvalidData, "Reserved DEFLATE distance")); }
                    let back = DISTANCE_BASE[distance_code]+bits.read(DISTANCE_EXTRA[distance_code])?;
                    if back > output || back > window { return Err(failure(Code::InvalidData, "DEFLATE history is unavailable")); }
                    if count > maximum-output { return Err(failure(Code::ResourceLimit, "Expanded output exceeds limit")); }
                    if expand { for _ in 0..count { let byte = bytes[bytes.len()-back]; bytes.push(byte); } }
                    output += count;
                }
            }
            if final_block != 0 { break; }
        }
        poll(check)?; Ok((bits.position, output, bytes))
    }
    /// Emit little-bit-endian fields through one bounded complete-message buffer.
    struct BitOutput<'a> { output: Buffer<'a>, value: u32, count: usize }
    impl<'a> BitOutput<'a> {
        /// Borrow the same output policy while retaining at most seven pending bits.
        fn new(output: Buffer<'a>) -> Self { Self { output, value: 0, count: 0 } }
        /// Append at most sixteen bits without overflowing the fixed accumulator.
        fn write(&mut self, value: usize, size: usize) -> Result<()> {
            self.value |= (value as u32) << self.count; self.count += size;
            while self.count >= 8 { self.output.push(self.value as u8)?; self.value >>= 8; self.count -= 8; } Ok(())
        }
        /// Reverse canonical code bits for RFC1951 transport order.
        fn code(&mut self, value: usize, size: usize) -> Result<()> {
            let mut reversed = 0;
            for index in 0..size { reversed = (reversed << 1) | ((value >> index) & 1); }
            self.write(reversed, size)
        }
        /// Emit a standard fixed literal/length symbol.
        fn symbol(&mut self, value: usize) -> Result<()> {
            if value < 144 { self.code(48+value, 8) }
            else if value < 256 { self.code(400+value-144, 9) }
            else if value < 280 { self.code(value-256, 7) }
            else { self.code(192+value-280, 8) }
        }
        /// Zero-pad only the final byte and return the unchanged bounded output owner.
        fn finish(mut self) -> Result<Buffer<'a>> { if self.count != 0 { self.output.push(self.value as u8)?; } Ok(self.output) }
    }
    /// Allocate only a fixed standard-history workspace; this is independent of byte output caps.
    fn workspace(count: usize) -> Result<Vec<usize>> {
        let mut values = Vec::new();
        values.try_reserve_exact(count).map_err(|_| failure(Code::ResourceLimit, "Compression workspace allocation failed"))?;
        values.resize(count, usize::MAX); Ok(values)
    }
    /// Hash only an already checked three-byte substring.
    fn hash(input: &[u8], index: usize) -> usize { (usize::from(input[index])*251+usize::from(input[index+1])*31+usize::from(input[index+2])) & 4095 }
    /// Emit stored blocks or bounded fixed-Huffman LZ77 matches without external providers.
    fn deflate<'a>(input: &[u8], options: EncodeOptions<'_>, mut output: Buffer<'a>) -> Result<Buffer<'a>> {
        if options.level == 0 {
            let mut position = 0;
            loop {
                poll(options.check_cancelled)?; let size = 65535.min(input.len()-position);
                output.push(u8::from(position+size == input.len()))?;
                output.push(size as u8)?; output.push((size >> 8) as u8)?;
                let complement = size ^ 65535;
                output.push(complement as u8)?; output.push((complement >> 8) as u8)?;
                for &byte in &input[position..position+size] { output.push(byte)?; }
                position += size; if position == input.len() { break; }
            }
            return Ok(output);
        }
        let mut bits = BitOutput::new(output); bits.write(3,3)?;
        let effort = [0,4,8,12,16,24,32,48,64,96][options.level as usize];
        let mut heads = workspace(4096)?; let mut chain = workspace(WINDOW_BYTES)?;
        let (mut position, mut work) = (0,0);
        while position < input.len() {
            poll(options.check_cancelled)?; let (mut best, mut back) = (0,0);
            if input.len()-position >= 3 {
                let mut candidate = heads[hash(input,position)]; let mut attempts = 0;
                while candidate != usize::MAX && candidate < position && position-candidate <= WINDOW_BYTES && attempts < effort {
                    attempts += 1; let maximum = 258.min(input.len()-position); let mut size = 0;
                    while size < maximum && input[candidate+size] == input[position+size] {
                        size += 1; work = (work+1) % CHECK_INTERVAL; if work == 0 { poll(options.check_cancelled)?; }
                    }
                    if size > best && size >= 3 { best = size; back = position-candidate; }
                    if best == maximum { break; }
                    let previous = chain[candidate % WINDOW_BYTES];
                    if previous >= candidate { break; } candidate = previous;
                }
            }
            let consumed = if best >= 3 {best} else {1};
            if best >= 3 {
                let mut length = 0;
                while length+1 < LENGTH_BASE.len() && LENGTH_BASE[length+1] <= best { length += 1; }
                bits.symbol(length+257)?; bits.write(best-LENGTH_BASE[length], LENGTH_EXTRA[length])?;
                let mut distance = 0;
                while distance+1 < DISTANCE_BASE.len() && DISTANCE_BASE[distance+1] <= back { distance += 1; }
                bits.code(distance,5)?; bits.write(back-DISTANCE_BASE[distance], DISTANCE_EXTRA[distance])?;
            } else { bits.symbol(usize::from(input[position]))?; }
            for index in position..position+consumed {
                if input.len()-index >= 3 { let key = hash(input,index); chain[index % WINDOW_BYTES] = heads[key]; heads[key] = index; }
            }
            position += consumed;
        }
        bits.symbol(256)?; bits.finish()
    }
    /// Copy a bounded complete byte span with cooperative cancellation.
    fn copy(input: &[u8], maximum: usize, check: Option<&dyn Fn() -> Result<()>>) -> Result<Vec<u8>> {
        if input.len() > maximum { return Err(failure(Code::ResourceLimit, "Compression byte limit exceeded")); }
        let mut output = Buffer::new(maximum,check);
        for &byte in input { output.push(byte)?; } poll(check)?; Ok(output.bytes)
    }
    /// Encode one standard outer frame without changing native message bytes.
    pub fn compress(input: &[u8], options: EncodeOptions<'_>) -> Result<Vec<u8>> {
        validate_encode(options)?; poll(options.check_cancelled)?;
        if input.len() > options.max_input_bytes { return Err(failure(Code::ResourceLimit, "Serialized input exceeds limit")); }
        if options.format == Format::None { return copy(input, options.max_output_bytes, options.check_cancelled); }
        let mut output = Buffer::new(options.max_output_bytes,options.check_cancelled);
        if options.format == Format::Gzip { for byte in [31,139,8,0,0,0,0,0,0,255] { output.push(byte)?; } }
        if options.format == Format::Zlib { output.push(120)?; output.push(156)?; }
        output = deflate(input,options,output)?;
        if options.format == Format::Gzip { output.word(crc(input,options.check_cancelled)?,false)?; output.word(input.len() as u32,false)?; }
        if options.format == Format::Zlib { output.word(adler(input,options.check_cancelled)?,true)?; }
        poll(options.check_cancelled)?; Ok(output.bytes)
    }
    /// Parse optional gzip fields with exact extents and the independent header CRC.
    fn gzip_start(input: &[u8], check: Option<&dyn Fn() -> Result<()>>) -> Result<usize> {
        span(input,0,10)?;
        if input[0] != 31 || input[1] != 139 || input[2] != 8 || input[3] & 224 != 0 {
            return Err(failure(Code::InvalidData, "Invalid gzip header"));
        }
        let flags = input[3]; let mut position = 10;
        if flags & 4 != 0 { let size = little(input,position,2)? as usize; position += 2; span(input,position,size)?; position += size; }
        for flag in [8,16] {
            if flags & flag != 0 {
                loop {
                    if position % CHECK_INTERVAL == 0 { poll(check)?; } span(input,position,1)?;
                    let byte = input[position]; position += 1; if byte == 0 { break; }
                }
            }
        }
        if flags & 2 != 0 {
            let expected = little(input,position,2)?;
            if expected != crc(&input[..position],check)? & 65535 { return Err(failure(Code::InvalidData, "Invalid gzip header checksum")); }
            position += 2;
        }
        Ok(position)
    }
    /// Validate exact grammar-derived extent and expanded count before allocating output storage.
    pub fn decompress(input: &[u8], options: DecodeOptions<'_>) -> Result<Vec<u8>> {
        validate_decode(options)?; poll(options.check_cancelled)?;
        if input.len() > options.max_input_bytes { return Err(failure(Code::ResourceLimit, "Compressed input exceeds limit")); }
        if options.format == Format::None { return copy(input,options.max_output_bytes,options.check_cancelled); }
        let (mut start, mut trailer, mut window) = (0,0,WINDOW_BYTES);
        if options.format == Format::Gzip { start = gzip_start(input,options.check_cancelled)?; trailer = 8; }
        else if options.format == Format::Zlib {
            span(input,0,2)?; let cmf = usize::from(input[0]); let flags = usize::from(input[1]);
            if cmf & 15 != 8 || cmf >> 4 > 7 || ((cmf << 8) | flags) % 31 != 0 || flags & 32 != 0 {
                return Err(failure(Code::InvalidData, "Invalid or dictionary-dependent zlib header"));
            }
            start = 2; trailer = 4; window = 1 << ((cmf >> 4)+8);
        }
        let (end,size,_) = inflate(input,start,options.max_output_bytes,window,options.check_cancelled,false)?;
        span(input,end,trailer)?;
        if input.len()-end != trailer { return Err(failure(Code::InvalidData, "Trailing or concatenated compression data")); }
        let (_,_,output) = inflate(input,start,size,window,options.check_cancelled,true)?;
        if options.format == Format::Gzip && (little(input,end,4)? != crc(&output,options.check_cancelled)? || little(input,end+4,4)? != output.len() as u32) {
            return Err(failure(Code::InvalidData, "Invalid gzip trailer"));
        }
        if options.format == Format::Zlib {
            let mut expected = 0u32;
            for index in 0..4 { expected = (expected << 8) | u32::from(input[end+index]); }
            if expected != adler(&output,options.check_cancelled)? { return Err(failure(Code::InvalidData, "Invalid zlib checksum")); }
        }
        poll(options.check_cancelled)?; Ok(output)
    }
    /// Invoke an owning native encoder after admission; its allocations remain outside compression caps.
    pub fn encode(encoder: impl FnOnce() -> Result<Vec<u8>>, options: EncodeOptions<'_>) -> Result<Vec<u8>> {
        validate_encode(options)?; poll(options.check_cancelled)?; let input = encoder()?;
        poll(options.check_cancelled)?; compress(&input,options)
    }
    /// Return a fresh native candidate only after complete framing and final cancellation checks.
    pub fn decode<T>(input: &[u8], decoder: impl FnOnce(&[u8]) -> Result<T>, options: DecodeOptions<'_>) -> Result<T> {
        let bytes = decompress(input,options)?; poll(options.check_cancelled)?;
        let candidate = decoder(&bytes)?; poll(options.check_cancelled)?; Ok(candidate)
    }
    /// Exclusive mutable wrapper access enforces sequential use without interior aliases or unsafe code.
    #[derive(Clone, Copy, PartialEq, Eq)]
    enum Phase { Ready, Busy, Finished, Closed, Failed }
    /// Enter Busy before callbacks so an unwinding panic also leaves later borrowed operations fenced.
    struct State { phase: Phase, first: Option<Error> }
    impl State {
        /// Create independently owned lifecycle state.
        fn new() -> Self { Self { phase: Phase::Ready, first: None } }
        /// Admit one exclusive public operation before application callbacks or transport I/O.
        fn begin(&mut self) -> Result<()> {
            if self.phase != Phase::Ready {
                return Err(Arc::new(Failure { code: Code::InvalidOptions,
                    message: "Inactive compression wrapper", cause: self.first.clone() }));
            }
            self.phase = Phase::Busy; Ok(())
        }
        /// Keep the exact first error handle and restore activity only after successful completion.
        fn complete<T>(&mut self, result: Result<T>, finished: bool) -> Result<T> {
            match result {
)compressrust"
R"compressrust(                Ok(value) => { self.phase = if finished {Phase::Finished} else {Phase::Ready}; Ok(value) }
                Err(error) => {
                    if self.first.is_none() { self.first = Some(error); }
                    self.phase = Phase::Failed; Err(self.first.as_ref().unwrap().clone())
                }
            }
        }
    }
    /// Stage native bytes until explicit finish; pass &mut W to retain transport ownership.
    pub struct Output<'a, W: std::io::Write> { sink: W, options: EncodeOptions<'a>, state: State, staged: Vec<u8>, provider: Option<&'a dyn Provider> }
    impl<'a, W: std::io::Write> Output<'a, W> {
        /// Borrow one explicitly selected provider without changing default constructor admission.
        pub fn with_provider(sink: W, options: EncodeOptions<'a>, provider: &'a dyn Provider) -> Result<Self> {
            validate_encode_with_provider(options,provider)?; Ok(Self { sink, options, state: State::new(), staged: Vec::new(), provider: Some(provider) })
        }
        /// Validate policy without callbacks, output or implicit transport ownership transfer.
        pub fn new(sink: W, options: EncodeOptions<'a>) -> Result<Self> {
            validate_encode(options)?; Ok(Self { sink, options, state: State::new(), staged: Vec::new(), provider: None })
        }
        /// Copy only admitted complete bytes with bounded cancellation intervals.
        fn stage(&mut self, bytes: &[u8]) -> Result<()> {
            poll(self.options.check_cancelled)?;
            if bytes.len() > self.options.max_input_bytes-self.staged.len() {
                return Err(failure(Code::ResourceLimit, "Serialized input exceeds limit"));
            }
            reserve(&mut self.staged,bytes.len())?;
            for block in bytes.chunks(BATCH_BYTES) { poll(self.options.check_cancelled)?; self.staged.extend_from_slice(block); }
            poll(self.options.check_cancelled)
        }
        /// Append borrowed bytes; any error makes the wrapper permanently inactive.
        pub fn write(&mut self, bytes: &[u8]) -> Result<()> {
            self.state.begin()?; let result = self.stage(bytes); self.state.complete(result,false)
        }
        /// Serialize privately after admission and stage no native candidate on callback failure.
        pub fn encode(&mut self, encoder: impl FnOnce() -> Result<Vec<u8>>) -> Result<()> {
            self.state.begin()?;
            let result = (|| { poll(self.options.check_cancelled)?; let bytes = encoder()?;
                poll(self.options.check_cancelled)?; self.stage(&bytes) })();
            self.state.complete(result,false)
        }
        /// Check activity and cancellation without forwarding flush or publishing staged bytes.
        pub fn flush(&mut self) -> Result<()> {
            self.state.begin()?; let result = poll(self.options.check_cancelled); self.state.complete(result,false)
        }
        /// Publish exactly one completed frame, accepting legal short writes but refusing zero progress.
        pub fn finish(&mut self) -> Result<()> {
            self.state.begin()?;
            let result = (|| {
                let packed = match self.provider {Some(provider) => compress_with_provider(&self.staged,self.options,provider),None => compress(&self.staged,self.options)}?; let mut position = 0;
                while position < packed.len() {
                    poll(self.options.check_cancelled)?; let count = BATCH_BYTES.min(packed.len()-position);
                    let written = self.sink.write(&packed[position..position+count]).map_err(into_error)?;
                    poll(self.options.check_cancelled)?;
                    if written == 0 { return Err(into_error(std::io::Error::new(std::io::ErrorKind::WriteZero,"Compression sink made no progress"))); }
                    if written > count { return Err(failure(Code::InvalidData,"Compression sink exceeded its requested write")); }
                    position += written;
                }
                poll(self.options.check_cancelled)?; self.staged.clear(); Ok(())
            })();
            self.state.complete(result,true)
        }
        /// Abandon private staging; never finish, flush or otherwise call the borrowed sink.
        pub fn close(&mut self) { self.state.phase = Phase::Closed; self.staged.clear(); }
    }
    /// Validate one EOF-delimited complete frame before exposing independent expanded bytes.
    pub struct Input<'a, R: std::io::Read> { source: R, options: DecodeOptions<'a>, state: State,
        staged: Option<Vec<u8>>, position: usize, provider: Option<&'a dyn Provider> }
    impl<'a, R: std::io::Read> Input<'a, R> {
        /// Borrow one explicitly selected provider without changing default constructor admission.
        pub fn with_provider(source: R, options: DecodeOptions<'a>, provider: &'a dyn Provider) -> Result<Self> {
            validate_decode_with_provider(options,provider)?; Ok(Self { source, options, state: State::new(), staged: None, position: 0, provider: Some(provider) })
        }
        /// Admit the full provider/window policy before source reads or application callbacks.
        pub fn new(source: R, options: DecodeOptions<'a>) -> Result<Self> {
            validate_decode(options)?; Ok(Self { source, options, state: State::new(), staged: None, position: 0, provider: None })
        }
        /// Read bounded chunks plus one overflow byte; preserve std::io's zero-at-EOF convention.
        fn prepare(&mut self) -> Result<()> {
            if self.staged.is_some() { return Ok(()); }
            let mut bytes = Vec::new(); let mut buffer = [0u8; BATCH_BYTES];
            loop {
                poll(self.options.check_cancelled)?; let remaining = self.options.max_input_bytes-bytes.len();
                let requested = if remaining >= BATCH_BYTES {BATCH_BYTES} else {remaining+1};
                let count = self.source.read(&mut buffer[..requested]).map_err(into_error)?;
                poll(self.options.check_cancelled)?;
                if count > requested { return Err(failure(Code::InvalidData,"Compression source exceeded its requested read")); }
                if count == 0 { break; }
                if count > remaining { return Err(failure(Code::ResourceLimit,"Compressed input exceeds limit")); }
                reserve(&mut bytes,count)?; bytes.extend_from_slice(&buffer[..count]);
            }
            let candidate = match self.provider {Some(provider) => decompress_with_provider(&bytes,self.options,provider),None => decompress(&bytes,self.options)}?; poll(self.options.check_cancelled)?;
            self.staged = Some(candidate); Ok(())
        }
        /// Return at most one bounded expanded chunk; an empty destination performs no source I/O.
        pub fn read(&mut self, destination: &mut [u8]) -> Result<usize> {
            self.state.begin()?;
            let result = (|| {
                poll(self.options.check_cancelled)?; if destination.is_empty() { return Ok(0); }
                self.prepare()?; poll(self.options.check_cancelled)?;
                let bytes = self.staged.as_ref().unwrap();
                let count = destination.len().min(BATCH_BYTES).min(bytes.len()-self.position);
                poll(self.options.check_cancelled)?;
                destination[..count].copy_from_slice(&bytes[self.position..self.position+count]);
                self.position += count; Ok(count)
            })();
            self.state.complete(result,false)
        }
        /// Require complete expanded consumption and observe cancellation even after cached EOF.
        pub fn finish(&mut self) -> Result<()> {
            self.state.begin()?;
            let result = (|| {
                poll(self.options.check_cancelled)?; self.prepare()?;
                if self.position != self.staged.as_ref().unwrap().len() { return Err(failure(Code::InvalidData,"Unread expanded compression input")); }
                poll(self.options.check_cancelled)
            })();
            self.state.complete(result,true)
        }
        /// Decode one fresh native candidate from an unconsumed complete frame, then finish atomically.
        pub fn decode<T>(&mut self, decoder: impl FnOnce(&[u8]) -> Result<T>) -> Result<T> {
            self.state.begin()?;
            let result = (|| {
                poll(self.options.check_cancelled)?;
                if self.position != 0 { return Err(failure(Code::InvalidOptions,"Native decode requires unconsumed input")); }
                self.prepare()?; poll(self.options.check_cancelled)?;
                let bytes = self.staged.as_ref().unwrap(); let candidate = decoder(bytes)?;
                poll(self.options.check_cancelled)?; self.position = bytes.len(); Ok(candidate)
            })();
            self.state.complete(result,true)
        }
        /// Abandon only private staging and leave the borrowed source under caller ownership.
        pub fn close(&mut self) { self.state.phase = Phase::Closed; self.staged = None; }
    }
    /// Read exactly one EOF-delimited frame through the same borrowed lifecycle and resource policy.
    pub fn decompress_from<R: std::io::Read>(source: &mut R, options: DecodeOptions<'_>) -> Result<Vec<u8>> {
        let mut input = Input::new(source,options)?; let mut bytes = Vec::new(); let mut buffer = [0u8; BATCH_BYTES];
        loop { let count = input.read(&mut buffer)?; if count == 0 { break; } reserve(&mut bytes,count)?; bytes.extend_from_slice(&buffer[..count]); }
        input.finish()?; Ok(bytes)
    }
}
)compressrust"
