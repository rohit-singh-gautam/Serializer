// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {readFile,writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {RelocationArchive as A,SerializerPackage as P,SerializerCompression as C} from './schema.mjs';

const [directory,name,action,expected]=process.argv.slice(2),previous=Buffer.from('previous destination');let checks=0;
// Keep public consumer assertions enabled in the actual Node invocation.
function check(value,message){if(!value)throw Error(message);++checks;}
// Snapshot public retained values while preserving bytes, text and ordinary object shape.
function snapshot(extensions){return JSON.stringify({parts:extensions.parts.map(part=>({...part,data:Array.from(part.data)})),relationships:extensions.relationships});}
// Exact and one-tight budgets independently cover the complete canonicalizing composition.
async function bounds(input,provider){
  const observed=new P.Budget();await A.encode(input,{budget:observed,compressionProvider:provider});
  for(const resource of ['workUnits','decoderAllocationBytes']){
    const used=observed.used[resource];check(used>0,'positive resource inventory');
    for(const allowance of [used,used-1]){
      const limits=new P.Limits({[resource==='workUnits'?'maxWorkUnits':'maxDecoderAllocationBytes']:allowance});let rejected=false;
      try{await A.encode(input,{limits,compressionProvider:provider});}catch(error){check(error instanceof P.Error&&error.code===P.Code.ResourceLimit,'bounded failure classification');rejected=true;}
      check(rejected===(allowance<used),'exact and one-tight cumulative bound');
    }
  }
}
// Preserve the original thrown object and the destination even when Save preparation is cancelled.
async function cancellation(input,provider,host,path){
  await writeFile(path,previous);const original=Error('caller cancellation identity');let armed=false;
  const budget=new P.Budget(new P.Limits({checkCancelled(){if(armed)throw original;}}));armed=true;
  try{await A.save(path,input,{budget,compressionProvider:provider,host});throw Error('cancelled Save accepted');}
  catch(error){check(error===original,'exact caller exception identity');}
  check((await readFile(path)).equals(previous),'cancelled Save retains destination');
}
const provider=await C.nodeProvider(),host=await P.nodeHost(),policy=new P.ExtensionPolicy({parts:'preserve',relationships:'preserve'});let loaded;
try{loaded=await A.load(new Uint8Array(await readFile(join(directory,name+'.opc'))),{compressionProvider:provider,policy});}
catch(error){check(error instanceof P.Error&&expected==='LOAD_ERROR','unexpected load rejection');console.log(JSON.stringify({case:name,decision:expected,checks,failure_stage:'load'}));process.exit(0);}
check(expected!=='LOAD_ERROR','invalid load published');check(loaded.value.extensions===loaded.extensions,'owning result extension identity');
check(loaded.value.first.id===11&&loaded.value.first.label==='first','first typed payload');check(loaded.value.second.id===22&&loaded.value.second.label==='second','second typed payload');
check(loaded.value.children.length===1&&loaded.value.children[0].value.id===33&&loaded.value.children[0].sourceIndex===0,'dynamic child routing');
check(Object.keys(loaded.extensions).sort().join(',')==='parts,relationships','public extension shape preserved');
const input=new A.Input({first:loaded.value.first,second:action==='remove_second'?null:loaded.value.second,children:loaded.value.children,extensions:loaded.extensions});
const before=snapshot(input.extensions),childName=input.children[0].name,saved=join(directory,name+'-saved.opc');await writeFile(saved,previous);
if(expected==='REJECT'){
  let rejected=false;try{await A.encode(input,{compressionProvider:provider});}catch(error){check(error instanceof P.Error,'owned composition rejection');rejected=true;}check(rejected,'invalid relocation encoded');
  rejected=false;try{await A.save(saved,input,{compressionProvider:provider,host});}catch(error){check(error instanceof P.Error,'owned Save rejection');rejected=true;}check(rejected,'invalid relocation saved');check((await readFile(saved)).equals(previous),'failed Save retains destination');
}else{
  check(expected==='OK','unknown decision');await writeFile(join(directory,name+'-encoded.opc'),await A.encode(input,{compressionProvider:provider}));await writeFile(join(directory,name+'-repeat.opc'),await A.encode(input,{compressionProvider:provider}));await A.save(saved,input,{compressionProvider:provider,host});
  if(name==='alternate'){await bounds(input,provider);await cancellation(input,provider,host,join(directory,name+'-cancel.opc'));}
  @typed_api@
}
check(snapshot(input.extensions)===before,'extension input mutated');check(input.children[0].name===childName,'dynamic child renamed');check(input.first.id===11&&input.first.label==='first','typed model mutated');
console.log(JSON.stringify({case:name,decision:expected,checks}));
