// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jsmixed(
// Bound one owning ordered mixed sequence independently of ordinary XML text/node limits.
export class XmlMixedLimits {
  constructor(values={}) {
    Object.assign(this,{maxFragments:100000,maxTextBytes:8388608},values);
    for(const key of ['maxFragments','maxTextBytes'])if(!Number.isSafeInteger(this[key]) || this[key]<0)throw new Error('Invalid XML mixed limits');
  }
}

// Append one statically selected native fragment without exposing an incomplete owning value.
function srlXmlMixedAppend(frame,index,value) {
  const fragments=frame.mixed.get(frame.value);
  if(fragments.length>=frame.limits.mixed.maxFragments)throw srlXmlQuotaError('XML mixed fragment limit');
  const factory=frame.mixed.fragment(),fragment=factory.create();factory.set(fragment,[index,value]);fragments.push(fragment);
}

// Merge only provider-split text of one semantic run; element boundaries always terminate the run.
function srlXmlMixedFlush(frame) {
  if(frame.mixedText.length){srlXmlMixedAppend(frame,frame.mixed.textIndex,frame.mixedText);frame.mixedText='';}
}

// Charge UTF-8 text before retaining it; empty parser notifications never create authored fragments.
function srlXmlMixedCharacters(frame,value) {
  if(frame.mixed.elementOnly){
    if(!/^[ \t\r\n]*$/.test(value))throw new Error('Text in element-only XML content');
    return;
  }
  const size=srlXmlUtf8Length(value);
  if(size>frame.limits.mixed.maxTextBytes-frame.mixedTextBytes)throw srlXmlQuotaError('XML mixed text limit');
  frame.mixedTextBytes+=size;frame.mixedText+=value;
}

// Visit every authored fragment, including adjacent/empty text, in its original native array order.
function srlXmlMixedWrite(owner,meta,output) {
  const fragments=meta.get(owner),bounds=output.limits.mixed,factory=meta.fragment();let textBytes=0;
  if(!Array.isArray(fragments))throw new Error('XML mixed fragment limit');
  if(fragments.length>bounds.maxFragments)throw srlXmlQuotaError('XML mixed fragment limit');
  if(meta.elementOnly&&meta.extent&&fragments.length!==meta.extent)throw new Error('XML ordered fixed extent mismatch');
  for(const fragment of fragments){
    const selected=factory.get(fragment),index=selected[0],value=selected[1];
    if(!meta.elementOnly&&index===meta.textIndex){
      const text=srlXmlScalar(value,'string',true),size=srlXmlUtf8Length(text);
      if(size>bounds.maxTextBytes-textBytes)throw srlXmlQuotaError('XML mixed text limit');
      textBytes+=size;output.escaped(text,false);
    }else{
      const child=meta.children.find(child=>child.alternative===index);
      if(!child)throw new Error('Unknown XML mixed alternative');
      (child.payload.projection ? value.readValue() : value)._srlXmlWrite(output,child.payload.local,child.payload.uri);
    }
  }
}
)jsmixed"
