R"serializer(
use std::collections::BTreeMap;
use std::fmt::Write as _;

/// Native wire protocols; binary scalars are little-endian.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Protocol { Json, BinaryNone, BinaryInteger, BinaryString }
/// Per-message limits; entries count cumulatively across collections.
#[derive(Clone, Copy, Debug)]
pub struct Limits { pub max_bytes: usize, pub max_string_bytes: usize, pub max_elements: usize, pub max_depth: usize }
impl Default for Limits {
    /// Construct an independent default resource policy.
    fn default() -> Self { Self { max_bytes: 64*1024*1024, max_string_bytes: 16*1024*1024, max_elements: 1_000_000, max_depth: 64 } }
}
/// Validation or allocation failure; partial decoded values are never returned.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error(pub &'static str);
impl std::fmt::Display for Error {
    /// Describe the failure without including untrusted payload data.
    fn fmt(&self, out: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { out.write_str(self.0) }
}
impl std::error::Error for Error {}
type SrlResult<T> = Result<T, Error>;
struct SrlWriter { protocol: Protocol, bytes: Vec<u8>, depth: usize }
impl SrlWriter {
    /// Create isolated owned output storage.
    fn new(protocol: Protocol) -> Self { Self { protocol, bytes: Vec::new(), depth: 0 } }
    /// Grow output with checked allocation before copying bytes.
    fn put(&mut self, bytes: &[u8]) -> SrlResult<()> {
        self.bytes.try_reserve(bytes.len()).map_err(|_| Error("Output allocation"))?;
        self.bytes.extend_from_slice(bytes); Ok(())
    }
    /// Select JSON framing.
    fn json(&self) -> bool { self.protocol == Protocol::Json }
    /// Write Serializer's two-bit length tag.
    fn compact(&mut self, value: usize) -> SrlResult<()> {
        if value > 0x3fffffff { return Err(Error("Compact integer out of range")); }
        let extra = if value <= 63 { 0 } else if value <= 16383 { 1 } else if value <= 4194303 { 2 } else { 3 };
        let mut bytes = [0u8; 4]; bytes[0] = ((extra as u8) << 6) | ((value >> (extra*8)) as u8);
        for index in 0..extra { bytes[index+1] = (value >> ((extra-index-1)*8)) as u8; }
        self.put(&bytes[..extra+1])
    }
    /// Preserve declared integer width, signedness, and all 64 bits.
    fn integer(&mut self, value: u64, width: usize, unsigned: bool) -> SrlResult<()> {
        if self.json() {
            let text = if unsigned { value.to_string() } else { let shift = (8-width)*8; (((value << shift) as i64) >> shift).to_string() };
            self.put(text.as_bytes())
        } else { self.put(&value.to_le_bytes()[..width]) }
    }
    /// Write binary32 without widening its binary NaN payload.
    fn float32(&mut self, value: f32) -> SrlResult<()> {
        if !self.json() { return self.put(&value.to_le_bytes()); }
        if !value.is_finite() { return Err(Error("Nonfinite JSON number")); }
        self.put(value.to_string().as_bytes())
    }
    /// Write binary64 or finite JSON numeric text.
    fn float64(&mut self, value: f64) -> SrlResult<()> {
        if !self.json() { return self.put(&value.to_le_bytes()); }
        if !value.is_finite() { return Err(Error("Nonfinite JSON number")); }
        self.put(value.to_string().as_bytes())
    }
    /// Write canonical boolean bytes or JSON literals.
    fn boolean(&mut self, value: bool) -> SrlResult<()> {
        if self.json() { self.put(if value { b"true" } else { b"false" }) } else { self.put(&[value as u8]) }
    }
    /// Encode valid Rust strings without a second complete JSON buffer.
    fn text(&mut self, value: &str) -> SrlResult<()> {
        if !self.json() { self.compact(value.len())?; return self.put(value.as_bytes()); }
        self.put(b"\"")?;
        let mut start = 0;
        for (index, byte) in value.bytes().enumerate() {
            if byte < 32 || byte == b'"' || byte == b'\\' {
                self.put(&value.as_bytes()[start..index])?;
                if byte == b'"' { self.put(b"\\\"")?; }
                else if byte == b'\\' { self.put(b"\\\\")?; }
                else { let mut escaped = String::new(); write!(&mut escaped, "\\u{:04x}", byte).map_err(|_| Error("Formatting"))?; self.put(escaped.as_bytes())?; }
                start = index+1;
            }
        }
        self.put(&value.as_bytes()[start..])?; self.put(b"\"")
    }
    /// Encode one ASCII JSON character or an unrestricted binary byte.
    fn character(&mut self, value: u8) -> SrlResult<()> {
        if self.json() { if value > 127 { return Err(Error("Non-ASCII JSON char")); } self.text(std::str::from_utf8(&[value]).map_err(|_| Error("Invalid char"))?) }
        else { self.put(&[value]) }
    }
    /// Preserve context-sensitive enum names and compact ordinals.
    fn enumeration(&mut self, value: usize, names: &[&str], collection: bool) -> SrlResult<()> {
        let name = names.get(value).ok_or(Error("Unknown enum"))?;
        if self.json() || (self.protocol == Protocol::BinaryString && !collection) { self.text(name) } else { self.compact(value) }
    }
    /// Bound recursion before traversing fields.
    fn begin_object(&mut self) -> SrlResult<()> {
        self.depth += 1; if self.depth > 64 { return Err(Error("Encode depth limit")); }
        if self.json() { self.put(b"{")?; } Ok(())
    }
    /// Append pre-encoded immutable schema key bytes.
    fn field(&mut self, id: usize, key: usize, first: bool) -> SrlResult<()> {
        if self.json() { if !first { self.put(b",")?; } self.put(SRL_FIELDS[key].0) }
        else if self.protocol == Protocol::BinaryInteger { self.compact(id) }
        else if self.protocol == Protocol::BinaryString { self.put(SRL_FIELDS[key].1) }
        else { Ok(()) }
    }
    /// Finish object framing and restore nesting depth.
    fn end_object(&mut self) -> SrlResult<()> {
        if self.json() { self.put(b"}")?; } else if self.protocol != Protocol::BinaryNone { self.put(&[0])?; }
        self.depth -= 1; Ok(())
    }
    /// Write array length or JSON opening delimiter.
    fn begin_array(&mut self, count: usize) -> SrlResult<()> { if self.json() { self.put(b"[") } else { self.compact(count) } }
    /// Separate JSON array entries.
    fn element(&mut self, index: usize) -> SrlResult<()> { if self.json() && index != 0 { self.put(b",")?; } Ok(()) }
    /// Close a JSON array.
    fn end_array(&mut self) -> SrlResult<()> { if self.json() { self.put(b"]")?; } Ok(()) }
}
enum SrlKey { Id(u32), Name(String) }
struct SrlReader<'a> { data: &'a [u8], protocol: Protocol, limits: Limits, offset: usize, depth: usize, elements: usize }
impl<'a> SrlReader<'a> {
    /// Borrow input only for this decode operation.
    fn new(data: &'a [u8], protocol: Protocol, limits: Limits) -> SrlResult<Self> {
        if data.len() > limits.max_bytes { return Err(Error("Message byte limit")); }
        Ok(Self { data, protocol, limits, offset: 0, depth: 0, elements: 0 })
    }
    /// Test the selected protocol.
    fn json(&self) -> bool { self.protocol == Protocol::Json }
    /// Check bounds before taking a borrowed input slice.
    fn take(&mut self, count: usize) -> SrlResult<&'a [u8]> {
        if count > self.data.len()-self.offset { return Err(Error("Truncated input")); }
        let start = self.offset; self.offset += count; Ok(&self.data[start..self.offset])
    }
    /// Skip exactly JSON whitespace.
    fn whitespace(&mut self) { while self.data.get(self.offset).is_some_and(|b| matches!(b, 32 | 9 | 10 | 13)) { self.offset += 1; } }
    /// Require one delimiter.
    fn expect(&mut self, value: u8) -> SrlResult<()> { self.whitespace(); if self.take(1)?[0] != value { return Err(Error("Unexpected delimiter")); } Ok(()) }
    /// Read compact integers without indexing incomplete input.
    fn compact(&mut self) -> SrlResult<usize> {
        let first = self.take(1)?[0]; let mut value = (first & 63) as usize;
        for byte in self.take((first >> 6) as usize)? { value = value*256 + *byte as usize; } Ok(value)
    }
    /// Borrow a numeric token instead of allocating temporary text.
    fn token(&mut self) -> SrlResult<&'a str> {
        self.whitespace(); let start = self.offset;
        while self.data.get(self.offset).is_some_and(|b| !matches!(b, 44 | 125 | 93 | 32 | 9 | 10 | 13)) { self.offset += 1; }
        if start == self.offset { return Err(Error("Missing token")); }
        std::str::from_utf8(&self.data[start..self.offset]).map_err(|_| Error("Invalid UTF-8"))
    }
    /// Return fixed-width integer bits with exact JSON range validation.
    fn integer(&mut self, width: usize, unsigned: bool) -> SrlResult<u64> {
        if !self.json() { let mut bytes = [0u8; 8]; bytes[..width].copy_from_slice(self.take(width)?); return Ok(u64::from_le_bytes(bytes)); }
        let text = self.token()?;
        if !srl_number(text, true) || (unsigned && text.starts_with('-')) { return Err(Error("Invalid integer")); }
        let bits = width*8;
        if unsigned {
            let value = text.parse::<u64>().map_err(|_| Error("Integer range"))?;
            if width < 8 && value >= (1u64 << bits) { return Err(Error("Integer range")); } Ok(value)
        } else {
            let value = text.parse::<i64>().map_err(|_| Error("Integer range"))?;
            if width < 8 && (value < -(1i64 << (bits-1)) || value >= (1i64 << (bits-1))) { return Err(Error("Integer range")); } Ok(value as u64)
        }
    }
    /// Decode binary32 directly, rejecting JSON overflow and underflow.
    fn float32(&mut self) -> SrlResult<f32> {
        if !self.json() { return Ok(f32::from_bits(self.integer(4, true)? as u32)); }
        let text = self.token()?; if !srl_number(text, false) { return Err(Error("Invalid number")); }
        let value = text.parse::<f32>().map_err(|_| Error("Floating-point range"))?;
        if !value.is_finite() || (value == 0.0 && srl_nonzero(text)) { return Err(Error("Floating-point range")); } Ok(value)
    }
    /// Decode binary64 directly, rejecting JSON overflow and underflow.
    fn float64(&mut self) -> SrlResult<f64> {
        if !self.json() { return Ok(f64::from_bits(self.integer(8, true)?)); }
        let text = self.token()?; if !srl_number(text, false) { return Err(Error("Invalid number")); }
        let value = text.parse::<f64>().map_err(|_| Error("Floating-point range"))?;
        if !value.is_finite() || (value == 0.0 && srl_nonzero(text)) { return Err(Error("Floating-point range")); } Ok(value)
    }
    /// Decode only canonical boolean values.
    fn boolean(&mut self) -> SrlResult<bool> {
        if self.json() { match self.token()? { "true" => Ok(true), "false" => Ok(false), _ => Err(Error("Invalid boolean")) } }
        else { match self.take(1)?[0] { 0 => Ok(false), 1 => Ok(true), _ => Err(Error("Invalid boolean")) } }
    }
    /// Read four hexadecimal digits of one JSON escape.
    fn hex(&mut self) -> SrlResult<u32> {
        let mut value = 0;
        for byte in self.take(4)? { value = value*16 + (*byte as char).to_digit(16).ok_or(Error("Invalid Unicode escape"))?; } Ok(value)
    }
    /// Decode bounded UTF-8 and surrogate-paired JSON strings.
    fn text(&mut self) -> SrlResult<String> {
        if !self.json() {
            let count = self.compact()?; if count > self.limits.max_string_bytes { return Err(Error("String limit")); }
            return Ok(std::str::from_utf8(self.take(count)?).map_err(|_| Error("Invalid UTF-8"))?.to_owned());
        }
        self.expect(b'"')?; let start = self.offset; let mut value = Vec::new();
        loop {
            let byte = self.take(1)?[0];
            if self.offset-start-1 > self.limits.max_string_bytes { return Err(Error("String limit")); }
            if byte == b'"' { break; }
            if byte < 32 { return Err(Error("Unescaped control")); }
            if byte != b'\\' { value.push(byte); continue; }
            let escaped = self.take(1)?[0];
            match escaped {
                b'"' | b'\\' | b'/' => value.push(escaped), b'b' => value.push(8), b'f' => value.push(12), b'n' => value.push(10), b'r' => value.push(13), b't' => value.push(9),
                b'u' => {
                    let mut code = self.hex()?;
                    if (0xd800..=0xdbff).contains(&code) {
                        if self.take(2)? != b"\\u" { return Err(Error("Invalid surrogate")); }
                        let low = self.hex()?; if !(0xdc00..=0xdfff).contains(&low) { return Err(Error("Invalid surrogate")); }
                        code = 0x10000 + ((code-0xd800)<<10) + low-0xdc00;
                    }
                    let ch = char::from_u32(code).ok_or(Error("Invalid Unicode scalar"))?;
                    let mut bytes = [0u8; 4]; value.extend_from_slice(ch.encode_utf8(&mut bytes).as_bytes());
                }, _ => return Err(Error("Invalid string escape"))
            }
        }
        if value.len() > self.limits.max_string_bytes { return Err(Error("String limit")); }
        String::from_utf8(value).map_err(|_| Error("Invalid UTF-8"))
    }
    /// Preserve binary bytes while limiting JSON characters to ASCII.
    fn character(&mut self) -> SrlResult<u8> {
        if !self.json() { return Ok(self.take(1)?[0]); }
        let text = self.text()?; if text.len() != 1 || text.as_bytes()[0] > 127 { return Err(Error("Invalid char")); } Ok(text.as_bytes()[0])
    }
    /// Decode declared names or ordinals in their collection context.
    fn enumeration(&mut self, names: &[&str], collection: bool) -> SrlResult<usize> {
        let index = if self.json() || (self.protocol == Protocol::BinaryString && !collection) { let text = self.text()?; names.iter().position(|name| *name == text).ok_or(Error("Unknown enum"))? } else { self.compact()? };
        if index >= names.len() { return Err(Error("Unknown enum")); } Ok(index)
    }
    /// Check object nesting before allocating children.
    fn begin_object(&mut self) -> SrlResult<()> { self.depth += 1; if self.depth > self.limits.max_depth { return Err(Error("Depth limit")); } if self.json() { self.expect(b'{')?; } Ok(()) }
    /// Yield positional IDs or keyed names, preserving repeated occurrences.
    fn key(&mut self, ids: &[u32], cursor: &mut usize) -> SrlResult<Option<SrlKey>> {
        if self.protocol == Protocol::BinaryNone { let value = ids.get(*cursor).copied(); *cursor += 1; return Ok(value.map(SrlKey::Id)); }
        if self.protocol == Protocol::BinaryInteger { let id = self.compact()? as u32; return Ok(if id == 0 { None } else { Some(SrlKey::Id(id)) }); }
        if self.json() {
            self.whitespace(); if self.data.get(self.offset) == Some(&b'}') { self.offset += 1; return Ok(None); }
            if *cursor != 0 { self.expect(b',')?; }
        }
        let name = self.text()?; if !self.json() && name.is_empty() { return Ok(None); }
        if self.json() { self.expect(b':')?; } *cursor += 1; Ok(Some(SrlKey::Name(name)))
    }
    /// Finish object depth accounting.
    fn end_object(&mut self) { self.depth -= 1; }
    /// Validate collection counts before reserving storage.
    fn begin_array(&mut self) -> SrlResult<Option<usize>> {
        if self.json() { self.expect(b'[')?; return Ok(None); }
        let count = self.compact()?; if count > self.limits.max_elements-self.elements { return Err(Error("Element limit")); } Ok(Some(count))
    }
    /// Reserve fixed-width arrays only after checking their minimum input size.
    fn capacity(&self, count: Option<usize>, width: usize) -> SrlResult<usize> {
        let count = count.unwrap_or(0);
        if width != 0 && count > (self.data.len()-self.offset)/width { return Err(Error("Truncated array")); }
        Ok(if width == 0 { count.min(256) } else { count })
    }
    /// Charge each collection element and reject trailing commas.
    fn next_element(&mut self, index: usize, count: Option<usize>) -> SrlResult<bool> {
        if let Some(count) = count { if index == count { return Ok(false); } }
        else { self.whitespace(); if self.data.get(self.offset) == Some(&b']') { self.offset += 1; return Ok(false); } if index != 0 { self.expect(b',')?; } }
        if self.elements == self.limits.max_elements { return Err(Error("Element limit")); } self.elements += 1; Ok(true)
    }
    /// Require exact consumption before exposing the fresh decoded object.
    fn finish(&mut self) -> SrlResult<()> { if self.json() { self.whitespace(); } if self.offset != self.data.len() { return Err(Error("Trailing input")); } Ok(()) }
}
/// Test for a nonzero mantissa to distinguish underflow from signed zero.
fn srl_nonzero(text: &str) -> bool { text.bytes().take_while(|b| *b != b'e' && *b != b'E').any(|b| (b'1'..=b'9').contains(&b)) }
/// Validate JSON number grammar without regular expressions or heap allocation.
fn srl_number(text: &str, integer: bool) -> bool {
    let b = text.as_bytes(); let mut i = usize::from(b.first() == Some(&b'-'));
    if b.get(i) == Some(&b'0') { i += 1; }
    else { let start = i; while b.get(i).is_some_and(u8::is_ascii_digit) { i += 1; } if i == start { return false; } }
    if !integer && b.get(i) == Some(&b'.') { i += 1; let start = i; while b.get(i).is_some_and(u8::is_ascii_digit) { i += 1; } if i == start { return false; } }
    if !integer && b.get(i).is_some_and(|b| *b == b'e' || *b == b'E') { i += 1; if b.get(i).is_some_and(|b| *b == b'+' || *b == b'-') { i += 1; } let start = i; while b.get(i).is_some_and(u8::is_ascii_digit) { i += 1; } if i == start { return false; } }
    i == b.len()
}
)serializer"
