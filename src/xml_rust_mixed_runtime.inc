// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustmixed(
/// Independent per-owner sequence policies leave ordinary XML limits and map policies compatible.
#[derive(Clone, Copy, Debug)]
pub struct XmlMixedLimits { pub max_fragments:usize, pub max_text_bytes:usize }
impl Default for XmlMixedLimits {
    /// Apply the shared conservative ordered-fragment defaults at every nested mixed owner.
    fn default() -> Self { Self {max_fragments:100000,max_text_bytes:8*1024*1024} }
}
impl SrlXmlInput<'_> {
    /// Join adjacent text/CDATA/entity spans, retaining their exact logical bytes until the next child.
    fn mixed_text(&mut self,maximum:usize) -> Result<String,Error> {
        let mut result=String::new();
        while let SrlXmlToken::Text(value)=&self.current {
            if value.len() > maximum.saturating_sub(result.len()) { return Err(Error("XML mixed text byte limit")); }
            result.try_reserve(value.len()).map_err(|_| Error("XML mixed text allocation"))?;
            result.push_str(value); self.advance()?;
        }
        Ok(result)
    }
}
)rustmixed"
