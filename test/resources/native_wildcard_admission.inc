// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
/* Native descriptors keep recursive declaration replay on the exact borrowed operation. */
typedef struct declaration_admission {size_t used[3];size_t selected,maximum;} declaration_admission;
/* Preserve an original caller status through every generated parser/DTO boundary. */
static int declaration_charge(declaration_admission* ledger,size_t resource,size_t amount){require(amount<=SIZE_MAX-ledger->used[resource]);ledger->used[resource]+=amount;return ledger->selected==resource&&ledger->used[resource]>ledger->maximum?777:0;}
/* Count actual work/allocation while leaving other primary logical resources independent. */
static int declaration_consume(void* userdata,srl_xml_operation_resource resource,size_t amount,const char* field){(void)field;declaration_admission* ledger=(declaration_admission*)userdata;if(resource==srl_xml_operation_work){return declaration_charge(ledger,0,amount);}if(resource==srl_xml_operation_allocation){return declaration_charge(ledger,1,amount);}return 0;}
/* Retained XML remains separate from validation-only temporary allocation/work. */
static int declaration_preserve(void* userdata,size_t amount,const char* field){(void)field;return declaration_charge((declaration_admission*)userdata,2,amount);}
/* Search only admitted bytes; fixtures need not have a trailing NUL terminator. */
static size_t declaration_find(const srl_buffer* bytes,const char* text){size_t length=strlen(text);for(size_t index=0;length<=bytes->size&&index<=bytes->size-length;++index){if(!memcmp(bytes->data+index,text,length)){return index;}}return SIZE_MAX;}
/* Admit complete operations without replacing old native/state/output ownership on rejection. */
static void declaration_admission_probe(const srl_buffer* source,const ordered_root_xml_document* value){
  const srl_xml_package_document_codec* codec=srl_xml_package_document_ordered_root();const srl_limits bounds=srl_default_limits();
  const srl_xml_package_contract package_contract={false,NULL,0};size_t measured[2][3]={{0}};
  for(size_t direction=0;direction<2;++direction){declaration_admission ledger={{0},SIZE_MAX,0};srl_xml_operation operation={0};operation.consume=declaration_consume;operation.preserve=declaration_preserve;operation.userdata=&ledger;operation.package_contract=&package_contract;srl_buffer output={0};ordered_root candidate={0};srl_xml_extension_state* state=NULL;
    srl_status status=direction?codec->encode(&value->value,value->extensions,&output,&bounds,NULL,&operation):codec->decode(&candidate,&state,source->data,source->size,&bounds,NULL,&operation);require(status==srl_ok&&operation.package_contract==&package_contract);memcpy(measured[direction],ledger.used,sizeof(ledger.used));ordered_root_free(&candidate);srl_xml_extension_state_free(state);srl_buffer_free(&output);
    for(size_t resource=0;resource<3;++resource){const size_t count=measured[direction][resource];if(!count){continue;}for(size_t tight=0;tight<2;++tight){memset(&ledger,0,sizeof(ledger));ledger.selected=resource;ledger.maximum=count-tight;memset(&operation,0,sizeof(operation));operation.consume=declaration_consume;operation.preserve=declaration_preserve;operation.userdata=&ledger;operation.package_contract=&package_contract;memset(&candidate,0,sizeof(candidate));state=NULL;const ordered_root previous=candidate;
      status=direction?codec->encode(&value->value,value->extensions,&output,&bounds,NULL,&operation):codec->decode(&candidate,&state,source->data,source->size,&bounds,NULL,&operation);require((status==srl_ok)==(tight==0)&&operation.package_contract==&package_contract);if(tight){require(operation.caller_status==777);if(direction){require(!output.data);}else{require(!memcmp(&candidate,&previous,sizeof(candidate))&&!state);}}ordered_root_free(&candidate);srl_xml_extension_state_free(state);srl_buffer_free(&output);
    }}
  }
  if(RETENTION_SKIP){require(!measured[0][2]&&!measured[1][2]);srl_xml_extension_options options=srl_xml_default_extension_options();options.limits.max_preserved_bytes=0;ordered_root_xml_document candidate={0};require(ordered_root_decode_xml_document(&candidate,source->data,source->size,NULL,&options)==srl_ok);ordered_root_xml_document_free(&candidate);}
  else{require(value->extensions&&value->extensions->owner_count);bool found=false;for(size_t owner=0;owner<value->extensions->owner_count&&!found;++owner){for(size_t child=0;child<value->extensions->owners[owner].child_count&&!found;++child){srl_buffer* bytes=&value->extensions->owners[owner].children[child].xml;size_t index=declaration_find(bytes,">3<");if(index!=SIZE_MAX){found=true;const uint8_t previous=bytes->data[index+1];bytes->data[index+1]='0';srl_buffer output={0};require(ordered_root_encode_xml_document(value,&output,NULL,NULL)!=srl_ok&&!output.data);bytes->data[index+1]=previous;}}}require(found);}
}
