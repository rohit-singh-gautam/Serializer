// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {readFile,writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {PipelineArchive as A,SerializerPackage as P,SerializerCompression as C} from './schema.mjs';

const [directory,name,codec,decision]=process.argv.slice(2),provider=await C.nodeProvider(),host=await P.nodeHost(),profiles={main:{codec}};
let checks=0;
// Require assertions in actual SDK invocations without depending on language optimization flags.
function check(condition,message){if(!condition)throw Error(message);++checks;}
let loaded;const decodeBudget=new P.Budget(),source=new Uint8Array(await readFile(join(directory,name+'.opc')));
try{loaded=await A.load(source,{profiles,compressionProvider:provider,budget:decodeBudget});}
catch(error){check(error instanceof P.Error&&decision==='LOAD_ERROR','unexpected load rejection: '+error.stack+' cause: '+error.cause?.stack);console.log(JSON.stringify({case:name,decision,checks}));process.exit(0);}
check(decision==='OK','invalid input published');const value=loaded.value;
for(const resource of ['workUnits','decoderAllocationBytes']){const used=decodeBudget.used[resource];for(const allowance of [used,used-1]){let rejected=false;try{await A.load(source,{profiles,compressionProvider:provider,limits:new P.Limits({[resource==='workUnits'?'maxWorkUnits':'maxDecoderAllocationBytes']:allowance})});}catch(error){check(error instanceof P.Error&&error.code===P.Code.ResourceLimit,'decode quota classification');rejected=true;}check(rejected===(allowance<used),'decode exact and one-tight quota');}}
check(value.main.id===17&&value.main.label==='pipeline'&&value.main.values.join(',')==='2,4,8','native model changed');
check(value.links.length===1&&value.links[0].contentType===''&&value.links[0].data.length===0,'external role is not inert');
const input=new A.Input({main:value.main,fixed:value.fixed,links:value.links,extensions:loaded.extensions}),options={profiles,compressionProvider:provider},observed=new P.Budget();
await writeFile(join(directory,name+'-encoded.opc'),await A.encode(input,{...options,budget:observed}));
await A.save(join(directory,name+'-saved.opc'),input,{...options,host});
for(const resource of ['workUnits','decoderAllocationBytes']){const used=observed.used[resource];check(used>0,'missing resource admission');for(const allowance of [used,used-1]){let rejected=false;try{await A.encode(input,{...options,limits:new P.Limits({[resource==='workUnits'?'maxWorkUnits':'maxDecoderAllocationBytes']:allowance})});}catch(error){check(error instanceof P.Error&&error.code===P.Code.ResourceLimit,'quota classification');rejected=true;}check(rejected===(allowance<used),'exact and one-tight quota');}}
for(const invalid of [{fixed:{codec:'binary_none'}},{fixed:'deflate'},{missing:'stored'}]){let rejected=false;try{await A.encode(input,{...options,profiles:invalid});}catch(error){check(error instanceof P.Error,'profile failure ownership');rejected=true;}check(rejected,'invalid selection accepted');}
const path=join(directory,name+'-atomic.opc'),previous=Buffer.from('previous destination');await writeFile(path,previous);
const bad=new A.Input({main:value.main,fixed:value.fixed,links:[new P.BinaryPart(new Uint8Array([1]),value.links[0].name,'')],extensions:loaded.extensions});let rejected=false;
try{await A.save(path,bad,{...options,host});}catch(error){check(error instanceof P.Error,'external error ownership');rejected=true;}check(rejected&&(await readFile(path)).equals(previous),'external payload changed destination');
console.log(JSON.stringify({case:name,decision,checks}));
