// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"cmap(
/* Independent per-wrapper map policy; ordinary native and XML limits retain their layout. */
typedef struct srl_xml_map_limits { size_t max_entries; size_t max_key_bytes; } srl_xml_map_limits;
/* Apply the shared conservative defaults independently to every nested wrapper. */
static inline srl_xml_map_limits srl_xml_default_map_limits(void) {
  const srl_xml_map_limits result = {1024u, 1024u*1024u}; return result;
}
/* Admit complete native keys while retaining the larger original lexical spelling's cost. */
static inline bool srl_xml_map_admit(const srl_xml_map_limits* limits, size_t* entries, size_t* bytes,
    size_t lexical, size_t canonical) {
  const size_t charged = lexical > canonical ? lexical : canonical;
  if (*entries >= limits->max_entries || *bytes > limits->max_key_bytes || charged > limits->max_key_bytes-*bytes) { return false; }
  ++*entries; *bytes += charged; return true;
}
)cmap"
