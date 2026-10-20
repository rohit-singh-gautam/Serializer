// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"copreader(
/* Map provider lookahead to native field spelling before admitting the next XML event. */
static inline bool srl_xml_operation_reader_event(xmlTextReaderPtr reader,int kind) {
  srl_xml_operation_reader* context=srl_xml_operation_reader_context(reader);
  if(!context){return true;}srl_xml_operation* operation=context->operation;
  const int provider_depth=xmlTextReaderDepth(reader);
  if(provider_depth<0){return srl_xml_operation_check(operation);}const size_t depth=(size_t)provider_depth;
  if(kind==XML_READER_TYPE_ELEMENT){
    if(depth>=context->capacity){size_t next=context->capacity?context->capacity:8u;
      while(next<=depth){if(next>SIZE_MAX/2u){next=depth+1u;break;}next*=2u;}
      if(next>SIZE_MAX/sizeof(context->path[0])){srl_xml_operation_fail(operation,srl_limit);return false;}
      if(!srl_xml_operation_charge(operation,srl_xml_operation_allocation,(next-context->capacity)*sizeof(context->path[0]))){return false;}
      size_t* path=(size_t*)realloc(context->path,next*sizeof(context->path[0]));if(!path){srl_xml_operation_fail(operation,srl_allocation);return false;}
      context->path=path;context->capacity=next;
    }
    size_t current=depth?SIZE_MAX:0;
    const srl_xml_identity_node* graph=(const srl_xml_identity_node*)context->graph;
    const xmlChar* uri=xmlTextReaderConstNamespaceUri(reader);const xmlChar* local=xmlTextReaderConstLocalName(reader);
    if(depth&&graph&&context->path[depth-1u]<context->graph_count){const size_t owner=context->path[depth-1u];const srl_xml_identity_node* parent=&graph[owner];
      for(size_t index=0;index<parent->count;++index){const srl_xml_identity_edge* edge=&parent->edges[index];
        if(xmlStrEqual(local,BAD_CAST edge->name.local)&&strcmp(uri?(const char*)uri:"",edge->name.uri)==0){
          current=edge->target;if(context->diagnostics&&context->diagnostics[owner].edges){srl_xml_operation_select(operation,context->diagnostics[owner].edges[index]);}break;
        }
      }
    }else if(!depth&&context->diagnostics&&context->graph_count){srl_xml_operation_select(operation,context->diagnostics[0].owner);}
    context->path[depth]=current;
    if(!context->secondary&&!srl_xml_operation_charge(operation,srl_xml_operation_nodes,1u)){return false;}
    if(!context->secondary){const size_t local_bytes=local?(size_t)xmlStrlen(local):0,uri_bytes=uri?(size_t)xmlStrlen(uri):0;
      if(!srl_xml_operation_charge(operation,srl_xml_operation_scalar_bytes,local_bytes)||!srl_xml_operation_charge(operation,srl_xml_operation_scalar_bytes,uri_bytes)){return false;}}
  }
  return srl_xml_operation_charge(operation,srl_xml_operation_work,1u);
}
/* Charge each logical scalar once; the independent identity reader owns only its extra work. */
static inline bool srl_xml_operation_reader_scalar(xmlTextReaderPtr reader,const xmlChar* text,bool attribute) {
  srl_xml_operation_reader* context=srl_xml_operation_reader_context(reader);if(!context){return true;}
  srl_xml_operation* operation=context->operation;const size_t count=text?(size_t)xmlStrlen(text):0;
  const int provider_depth=xmlTextReaderDepth(reader);const size_t depth=provider_depth>0?(size_t)provider_depth-1u:0;
  if(context->diagnostics&&depth<context->capacity&&context->path[depth]<context->graph_count){
    const srl_xml_identity_diagnostic_node* diagnostic=&context->diagnostics[context->path[depth]];
    const xmlChar* uri=xmlTextReaderConstNamespaceUri(reader);const xmlChar* local=xmlTextReaderConstLocalName(reader);
    for(size_t index=0;index<diagnostic->scalar_count;++index){const srl_xml_identity_diagnostic_scalar* scalar=&diagnostic->scalars[index];
      if((attribute&&scalar->attribute&&xmlStrEqual(local,BAD_CAST scalar->local)&&strcmp(uri?(const char*)uri:"",scalar->uri)==0)||(!attribute&&scalar->text)){
        srl_xml_operation_select(operation,scalar->member);break;
      }
    }
  }
  if(!context->secondary){
    if(attribute&&!srl_xml_operation_charge(operation,srl_xml_operation_attributes,1u)){return false;}
    if(attribute){const xmlChar* local=xmlTextReaderConstLocalName(reader);const xmlChar* uri=xmlTextReaderConstNamespaceUri(reader);
      size_t local_bytes=local?(size_t)xmlStrlen(local):0,uri_bytes=uri?(size_t)xmlStrlen(uri):0;
      if(xmlTextReaderIsNamespaceDecl(reader)){const xmlChar* prefix=xmlTextReaderConstPrefix(reader);local_bytes=prefix&&*prefix?local_bytes:0;uri_bytes=0;}
      if(!srl_xml_operation_charge(operation,srl_xml_operation_scalar_bytes,local_bytes)||!srl_xml_operation_charge(operation,srl_xml_operation_scalar_bytes,uri_bytes)){return false;}}
    if(!srl_xml_operation_charge(operation,srl_xml_operation_scalar_bytes,count)){return false;}
  }
  return srl_xml_operation_charge(operation,srl_xml_operation_work,count+1u);
}
/* Observe finalized provider bytes once, including exactly the namespace attributes actually emitted. */
static inline srl_status srl_xml_operation_observe_output(const uint8_t* bytes,size_t size,const srl_xml_identity_node* graph,size_t graph_count,const srl_xml_identity_diagnostic_node* diagnostics,srl_xml_operation* operation) {
  if(!operation){return srl_ok;}
  srl_xml_operation_reader context={0};context.operation=operation;context.graph=graph;context.graph_count=graph_count;context.diagnostics=diagnostics;
  srl_limits bounds=srl_default_limits();bounds.max_bytes=size;bounds.max_string_bytes=SIZE_MAX;bounds.max_elements=SIZE_MAX;bounds.max_depth=SIZE_MAX;
  srl_xml_input input;srl_status status=srl_xml_operation_input_init(&input,bytes,size,&bounds,&context);
  while(status==srl_ok&&input.kind!=XML_READER_TYPE_NONE){if(!srl_xml_input_next(&input)){status=input.status==srl_ok?srl_invalid:input.status;}}
  srl_xml_input_free(&input);free(context.path);return srl_xml_operation_fail(operation,status);
}
)copreader"
