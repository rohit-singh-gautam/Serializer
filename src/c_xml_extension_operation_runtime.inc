// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"extoperation(
/* Own only callback plumbing for this pull reader; the operation remains synchronously borrowed. */
static inline srl_status srl_extension_operation_input_init(srl_xml_input* input,const uint8_t* bytes,size_t size,const srl_limits* limits,srl_native_operation* operation){
  srl_xml_operation_reader* context=(srl_xml_operation_reader*)srl_native_operation_calloc(1u,sizeof(*context),operation);
  if(!context){memset(input,0,sizeof(*input));input->status=srl_allocation;return input->status;}
  context->operation=operation?operation->admission:NULL;
  const srl_status status=srl_xml_operation_input_init(input,bytes,size,limits,context);
  if(!input->reader){free(context->path);free(context);}return status;
}
/* Cleanup never consumes exhausted admission and cannot replace the first failure. */
static inline void srl_extension_operation_input_free(srl_xml_input* input,srl_native_operation* operation){
  (void)operation;srl_xml_operation_reader* context=srl_xml_operation_reader_context(input->reader);
  srl_xml_input_free(input);if(context){free(context->path);free(context);}
}
)extoperation"
