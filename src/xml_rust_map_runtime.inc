// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustmap(
/// Independent per-wrapper map admission without changing existing XML limits layouts.
#[derive(Clone, Copy, Debug)]
pub struct XmlMapLimits { pub max_entries:usize, pub max_key_bytes:usize }
impl Default for XmlMapLimits {
    /// Apply the shared conservative map defaults to every nested wrapper.
    fn default() -> Self { Self { max_entries:1024, max_key_bytes:1024*1024 } }
}
/// Account original and canonical key spellings before the native map discards lexical aliases.
struct SrlXmlMapBudget { limits:XmlMapLimits, entries:usize, key_bytes:usize }
impl SrlXmlMapBudget {
    /// Start fresh independent accounting for one map wrapper.
    fn new(limits:XmlMapLimits) -> Self { Self {limits,entries:0,key_bytes:0} }
    /// Admit one complete native key before allocating an owning map entry.
    fn admit(&mut self,lexical:usize,canonical:usize) -> Result<(),Error> {
        if self.entries >= self.limits.max_entries { return Err(Error("XML map entry limit")); }
        let bytes=lexical.max(canonical);
        if bytes > self.limits.max_key_bytes.saturating_sub(self.key_bytes) { return Err(Error("XML map key byte limit")); }
        self.entries+=1; self.key_bytes+=bytes; Ok(())
    }
}
)rustmap"
