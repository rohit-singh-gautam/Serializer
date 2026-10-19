// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jsprojection(
const srlXmlProjectionContracts=new WeakMap();

// Readonly projected metadata can export live values but cannot synthesize an external owner on input.
function srlXmlProjectionReadonly() { throw new Error('Readonly XML projections are export-only; decode the owning target'); }

// Cache only immutable metadata and getter callbacks; no owning target or selected native value is constructed or copied.
function srlXmlProjectionContract(type,create) {
  let meta=srlXmlProjectionContracts.get(type);
  if(meta)return meta;
  const source=create(),target=source.target;
  const fields=target.fields.map(field=>{
    const get=source.getters.get(field.native);
    if(typeof get!=='function')throw new Error('Missing readonly XML target getter');
    return Object.freeze({...field,get,set:srlXmlProjectionReadonly});
  });
  const revision=target.model.revision ? Object.freeze({...target.model.revision,get:source.revision,set:srlXmlProjectionReadonly}) : null;
  if(revision && typeof source.revision!=='function')throw new Error('Missing readonly XML revision getter');
  const mixed=target.mixed ? Object.freeze({...target.mixed,get:source.mixed,set:srlXmlProjectionReadonly}) : null;
  if(mixed && typeof source.mixed!=='function')throw new Error('Missing readonly XML mixed getter');
  meta=Object.freeze({...target,projection:true,create:srlXmlProjectionReadonly,fields:Object.freeze(fields),mixed,
    model:Object.freeze({...target.model,revision})});
  srlXmlProjectionContracts.set(type,meta);return meta;
}

// Reject complete input graphs containing readonly handles before allocating any owning candidate or invoking a validator.
function srlXmlProjectionInput(meta,active=new Set()) {
  if(meta.projection)srlXmlProjectionReadonly();
  if(active.has(meta))return;active.add(meta);
  const field=item=>{
    if(item.projection)srlXmlProjectionReadonly();
    if(item.map){field(item.map.key);field(item.map.value);}
    else if(item.variant)for(const alternative of item.variant.alternatives)field(alternative.payload);
    else if(item.model)srlXmlProjectionInput(item.model(),active);
  };
  for(const item of meta.fields)field(item);
  if(meta.mixed)for(const item of meta.mixed.children)field(item.payload);
}
)jsprojection"
