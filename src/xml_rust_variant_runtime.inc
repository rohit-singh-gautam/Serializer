// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustvariant(
/// Validate the XML1.0 NCName inventory independently of namespace prefix spelling.
fn srl_xml_variant_ncname(text:&str) -> bool {
    let start=|value:char| matches!(value as u32,0x41..=0x5a|0x5f|0x61..=0x7a|0xc0..=0xd6|0xd8..=0xf6|
        0xf8..=0x2ff|0x370..=0x37d|0x37f..=0x1fff|0x200c..=0x200d|0x2070..=0x218f|0x2c00..=0x2fef|
        0x3001..=0xd7ff|0xf900..=0xfdcf|0xfdf0..=0xfffd|0x10000..=0xeffff);
    let mut values=text.chars();
    values.next().is_some_and(start) && values.all(|value| start(value) ||
        matches!(value as u32,0x2d..=0x2e|0x30..=0x39|0xb7|0x300..=0x36f|0x203f..=0x2040))
}
impl SrlXmlInput<'_,'_> {
    /// Resolve and remove one authorized xsi:type before the normal scalar or record consumes attributes.
    fn take_type(&mut self) -> Result<Option<(String,String)>,Error> {
        let lexical=if let SrlXmlToken::Start(_,attributes)=&mut self.current {
            attributes.iter().position(|(name,_)| name.uri == "http://www.w3.org/2001/XMLSchema-instance" && name.local == "type")
                .map(|index| attributes.remove(index).1)
        } else { return Err(Error("XML type requires a start token")); };
        let Some(lexical)=lexical else { return Ok(None); };
        let lexical=lexical.trim_matches([' ','\t','\r','\n']);
        let mut parts=lexical.split(':'); let first=parts.next().unwrap_or("");
        let (prefix,local)=if let Some(local)=parts.next() {
            if !srl_xml_variant_ncname(first) || parts.next().is_some() { return Err(Error("Malformed XML type QName")); }
            (first,local)
        } else { ("",first) };
        if !srl_xml_variant_ncname(local) { return Err(Error("Malformed XML type QName")); }
        Ok(Some((self.namespace(prefix,false)?,local.to_owned())))
    }
}
)rustvariant"
