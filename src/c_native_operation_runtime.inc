// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"nativeoperation(
/* Native operation state is a separate synchronous sidecar; legacy reader/writer layouts stay exact. */
typedef struct srl_native_operation {srl_xml_operation* admission;size_t max_output_bytes;} srl_native_operation;
/* Compiler-owned native adapters lend their source and publish only complete owning results. */
typedef struct srl_native_operation_codec {
  srl_status (*encode)(const void*,srl_buffer*,srl_protocol,const srl_limits*,srl_xml_operation*);
  srl_status (*decode)(void*,const uint8_t*,size_t,srl_protocol,const srl_limits*,srl_xml_operation*);
  srl_status (*initialize)(void*,srl_xml_operation*);
} srl_native_operation_codec;
typedef struct srl_xml_extension_state srl_xml_extension_state;
/* XML support definitions follow package declarations in a generated C header. */
static inline void srl_xml_extension_state_free(srl_xml_extension_state*);
static inline size_t srl_xml_extension_preserved_bytes(const srl_xml_extension_state*);
/* Document descriptors borrow native payloads and retain XML-only state in the package aggregate. */
typedef struct srl_xml_package_document_codec {
  srl_status (*encode)(const void*,const srl_xml_extension_state*,srl_buffer*,const srl_limits*,const srl_xml_validation*,srl_xml_operation*);
  srl_status (*decode)(void*,srl_xml_extension_state**,const uint8_t*,size_t,const srl_limits*,const srl_xml_validation*,srl_xml_operation*);
  srl_status (*prepare)(srl_xml_extension_state**,const uint8_t*,size_t,const srl_limits*,const srl_xml_validation*,srl_xml_operation*);
} srl_xml_package_document_codec;
/* Share native logical work and allocation with the enclosing package's exact failure identity. */
static inline bool srl_native_operation_charge(srl_native_operation* operation,srl_xml_operation_resource resource,size_t amount){return srl_xml_operation_charge(operation?operation->admission:NULL,resource,amount);}
/* Admit actual native allocation before obtaining independently owned storage. */
static inline void* srl_native_operation_malloc(size_t size,srl_native_operation* operation){if(!srl_native_operation_charge(operation,srl_xml_operation_allocation,size)){return NULL;}void* result=malloc(size);if(size&&!result){srl_xml_operation_fail(operation?operation->admission:NULL,srl_allocation);}return result;}
/* Preserve old storage on rejected or failed geometric native buffer replacement. */
static inline void* srl_native_operation_realloc(void* previous,size_t size,srl_native_operation* operation){if(!srl_native_operation_charge(operation,srl_xml_operation_allocation,size)){return NULL;}void* result=realloc(previous,size);if(size&&!result){srl_xml_operation_fail(operation?operation->admission:NULL,srl_allocation);}return result;}
/* Check native element multiplication before admission or zero-initialized ownership. */
static inline void* srl_native_operation_calloc(size_t count,size_t width,srl_native_operation* operation){if(count&&width>SIZE_MAX/count){srl_xml_operation_fail(operation?operation->admission:NULL,srl_limit);return NULL;}if(!srl_native_operation_charge(operation,srl_xml_operation_allocation,count*width)){return NULL;}void* result=calloc(count,width);if(count&&width&&!result){srl_xml_operation_fail(operation?operation->admission:NULL,srl_allocation);}return result;}
)nativeoperation"
