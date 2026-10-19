// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustmodel(
/// Independent decoded-byte admission for every raw, fixed or algorithm-tagged digest.
#[derive(Clone, Copy, Debug)]
pub struct XmlDigestLimits { pub max_bytes:usize }
impl Default for XmlDigestLimits {
    /// Keep the shared one MiB default without changing XmlLimits or native codec layouts.
    fn default() -> Self { Self{max_bytes:1024*1024} }
}
/// Require canonical unsigned numeric revision grammar before exact native conversion.
fn srl_xml_revision_number(text:&str,floating:bool) -> Result<(),Error> {
    let bytes=text.as_bytes(); let mut offset=0;
    while offset < bytes.len() && bytes[offset].is_ascii_digit() { offset+=1; }
    if offset == 0 || offset > 1 && bytes[0] == b'0' { return Err(Error("Noncanonical XML revision")); }
    if floating && bytes.get(offset) == Some(&b'.') {
        offset+=1; let start=offset;
        while offset < bytes.len() && bytes[offset].is_ascii_digit() { offset+=1; }
        if offset == start { return Err(Error("Noncanonical XML revision")); }
    }
    if floating && matches!(bytes.get(offset),Some(b'e'|b'E')) {
        offset+=1; if matches!(bytes.get(offset),Some(b'+'|b'-')) { offset+=1; }
        let start=offset; while offset < bytes.len() && bytes[offset].is_ascii_digit() { offset+=1; }
        if offset == start { return Err(Error("Noncanonical XML revision")); }
    }
    if offset != bytes.len() { return Err(Error("Noncanonical XML revision")); } Ok(())
}
/// Decode admitted exact bytes, accepting either hexadecimal case without hashing.
fn srl_xml_digest_read(text:&str,extent:usize,limits:XmlDigestLimits) -> Result<Vec<u8>,Error> {
    let count=text.len()/2;
    if text.len()%2 != 0 || count > limits.max_bytes || extent != 0 && count != extent { return Err(Error("XML digest extent/limit")); }
    let mut result=Vec::new(); result.try_reserve_exact(count).map_err(|_| Error("XML digest allocation"))?;
    for pair in text.as_bytes().chunks_exact(2) {
        let digit=|byte:u8| match byte { b'0'..=b'9'=>Some(byte-b'0'),b'a'..=b'f'=>Some(byte-b'a'+10),b'A'..=b'F'=>Some(byte-b'A'+10),_=>None };
        result.push(digit(pair[0]).ok_or(Error("Invalid XML digest hex"))?*16+digit(pair[1]).ok_or(Error("Invalid XML digest hex"))?);
    } Ok(result)
}
/// Emit canonical lowercase hexadecimal from borrowed owning bytes under the same admission policy.
fn srl_xml_digest_text(value:&[u8],extent:usize,limits:XmlDigestLimits) -> Result<String,Error> {
    if value.len() > limits.max_bytes || extent != 0 && value.len() != extent { return Err(Error("XML digest extent/limit")); }
    let size=value.len().checked_mul(2).ok_or(Error("XML digest length"))?;
    let mut result=String::new(); result.try_reserve_exact(size).map_err(|_| Error("XML digest allocation"))?;
    const HEX:&[u8;16]=b"0123456789abcdef";
    for byte in value { result.push(HEX[(byte>>4) as usize] as char); result.push(HEX[(byte&15) as usize] as char); } Ok(result)
}
)rustmodel"
