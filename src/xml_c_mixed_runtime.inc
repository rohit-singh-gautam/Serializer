// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"cmixed(
/* Independent sequence policies retain the existing native and complete XML limit layouts. */
typedef struct srl_xml_mixed_limits { size_t max_fragments; size_t max_text_bytes; } srl_xml_mixed_limits;
/* Apply shared conservative defaults separately to every owning mixed-content sequence. */
static inline srl_xml_mixed_limits srl_xml_default_mixed_limits(void) {
  const srl_xml_mixed_limits value={100000u,8u*1024u*1024u}; return value;
}
)cmixed"
