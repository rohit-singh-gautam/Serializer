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
struct SrlXmlMapBudget<'a> {operation:Option<@xml_operation@::State<'a>>, limits:XmlMapLimits, entries:usize, key_bytes:usize }
impl<'a> SrlXmlMapBudget<'a> {
    /// Start fresh independent accounting for one map wrapper.
    fn new(limits:XmlMapLimits) -> Self { Self {operation:None,limits,entries:0,key_bytes:0} }
    /// Attach one shared operation without extending the public map policy.
    fn new_with_operation(limits:XmlMapLimits,operation:Option<@xml_operation@::State<'a>>)->Self {let mut result=Self::new(limits);result.operation=operation;result}
    /// Admit one complete native key before allocating an owning map entry.
    fn admit(&mut self,lexical:usize,canonical:usize) -> Result<(),Error> {
        if self.entries >= self.limits.max_entries { return Err(@xml_operation@::quota(self.operation.as_ref(),"XML map entry limit")); }
        let bytes=lexical.max(canonical);
        if bytes > self.limits.max_key_bytes.saturating_sub(self.key_bytes) { return Err(@xml_operation@::quota(self.operation.as_ref(),"XML map key byte limit")); }
        @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::WorkUnits,bytes.saturating_add(1))?;
        self.entries+=1; self.key_bytes+=bytes; Ok(())
    }
}
)rustmap"
