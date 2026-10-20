// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"coprovider(
/* Parallel diagnostics preserve the layout of existing immutable identity metadata. */
typedef struct srl_xml_identity_diagnostic_constraint {const char* name;const char* const* fields;size_t count;} srl_xml_identity_diagnostic_constraint;
typedef struct srl_xml_identity_diagnostic_scalar {const char* uri;const char* local;const char* member;bool attribute;bool text;} srl_xml_identity_diagnostic_scalar;
typedef struct srl_xml_identity_diagnostic_node {
  const char* owner;const srl_xml_identity_diagnostic_constraint* constraints;const char* const* edges;
  const srl_xml_identity_diagnostic_scalar* scalars;size_t scalar_count;
} srl_xml_identity_diagnostic_node;
/* Reader-owned callback userdata is a borrowed sidecar, never a global or an aggregate extension. */
typedef struct srl_xml_operation_reader {
  srl_xml_operation* operation;const void* graph;const srl_xml_identity_diagnostic_node* diagnostics;
  size_t graph_count;size_t* path;size_t capacity;bool secondary;
} srl_xml_operation_reader;
/* Silence only this private reader while retaining its synchronous operation sidecar. */
static inline void srl_xml_operation_reader_error(void* context,const char* message,xmlParserSeverities severity,xmlTextReaderLocatorPtr locator) {
  (void)context;(void)message;(void)severity;(void)locator;
}
/* Recognize only our own handler before interpreting provider userdata. */
static inline srl_xml_operation_reader* srl_xml_operation_reader_context(xmlTextReaderPtr reader) {
  xmlTextReaderErrorFunc callback=NULL;void* context=NULL;
  if(!reader){return NULL;}xmlTextReaderGetErrorHandler(reader,&callback,&context);
  return callback==srl_xml_operation_reader_error?(srl_xml_operation_reader*)context:NULL;
}
/* Retrieve the explicit operation associated with this private pull reader. */
static inline srl_xml_operation* srl_xml_operation_input_context(xmlTextReaderPtr reader) {
  srl_xml_operation_reader* context=srl_xml_operation_reader_context(reader);return context?context->operation:NULL;
}
/* Forward declarations keep the existing input/identity metadata layouts independent. */
static inline bool srl_xml_operation_reader_event(xmlTextReaderPtr reader,int kind);
static inline bool srl_xml_operation_reader_scalar(xmlTextReaderPtr reader,const xmlChar* text,bool attribute);
/* A separate provider trampoline leaves every field in srl_xml_output unchanged. */
typedef struct srl_xml_operation_output {srl_xml_output* output;srl_xml_operation* operation;} srl_xml_operation_output;
/* Admit owned buffer growth and provider copy work before forwarding complete bytes. */
static inline int srl_xml_operation_output_write(void* context,const char* bytes,int size) {
  srl_xml_operation_output* value=(srl_xml_operation_output*)context;
  if(size<0||!srl_xml_operation_charge(value->operation,srl_xml_operation_work,(size_t)size)||
      !srl_xml_operation_charge(value->operation,srl_xml_operation_allocation,(size_t)size)){
    value->output->writer.status=srl_invalid;return -1;
  }
  const int written=srl_xml_output_write(value->output,bytes,size);
  if(written<0){srl_xml_operation_fail(value->operation,value->output->writer.status);}return written;
}
/* Admit modeled scalar work; finalized provider bytes supply the single logical XML event pass. */
static inline bool srl_xml_operation_emit_text(srl_xml_operation* operation,xmlTextWriterPtr writer,srl_xml_output* output,const char* local,const char* uri,const char* prefix,const char* text,size_t size) {
  if(!operation){return srl_xml_emit_text(writer,output,local,uri,prefix,text,size);}
  if(size>output->limits.max_string_bytes||output->text_bytes>output->limits.max_bytes||size>output->limits.max_bytes-output->text_bytes){
    output->writer.status=operation?srl_limit:srl_invalid;srl_xml_operation_fail(operation,output->writer.status);return false;
  }
  if(!srl_xml_operation_charge(operation,srl_xml_operation_work,size+1u)){output->writer.status=srl_invalid;return false;}
  if(!srl_xml_operation_characters(operation,text,size)||strlen(text)!=size){output->writer.status=srl_xml_operation_fail(operation,srl_invalid);return false;}
  output->text_bytes+=size;
  const int written=local?xmlTextWriterWriteAttributeNS(writer,*uri?BAD_CAST srl_xml_namespace_prefix(uri,prefix):NULL,BAD_CAST local,*uri?BAD_CAST uri:NULL,BAD_CAST text):xmlTextWriterWriteString(writer,BAD_CAST text);
  return written>=0;
}
/* Admit one modeled element before provider output or native depth growth. */
static inline bool srl_xml_operation_node(srl_xml_output* output,srl_xml_operation* operation) {
  if(!operation){return true;}
  if(output->nodes>=output->limits.max_elements||output->writer.depth>=output->limits.max_depth){output->writer.status=srl_xml_operation_fail(operation,srl_limit);return false;}
  if(!srl_xml_operation_charge(operation,srl_xml_operation_work,1u)){output->writer.status=srl_invalid;return false;}return true;
}
/* Classify native output admission without changing ordinary XML error behavior. */
static inline bool srl_xml_operation_output_limit(srl_xml_output* output,srl_xml_operation* operation) {
  if(operation){output->writer.status=srl_xml_operation_fail(operation,srl_limit);}return false;
}
)coprovider"
