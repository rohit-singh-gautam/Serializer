// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {readFileSync,writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {SerializerPackage as P,SerializerCompression as C} from './schema.mjs';

// Identify the actual selected Node zlib provider and explicit package compression defaults.
function fingerprint(metadataMethod=-1){return {implementation:'node:zlib.deflateRaw',version:process.versions.zlib,options:{format:'raw_deflate',level:6,window_bits:15,nowrap:true,strategy:0,dictionary:'none',flush:'finish'},...(metadataMethod<0?{}:{metadata_method:metadataMethod})};}
// Apply only the prescribed input-list permutation; generated writers choose output order.
function permute(values,index){
  if(!Number.isInteger(index)||index<0||index>5)throw Error('Expected permutation 0 through 5');const count=values.length;if(!count)return [];
  if(index===0)return values.slice();if(index===1)return values.slice().reverse();
  if(index===2||index===3){const offset=index===2?1:Math.floor(count/2);return values.slice(offset).concat(values.slice(0,offset));}
  if(index===4)return values.filter((_,at)=>at%2===0).concat(values.filter((_,at)=>at%2===1));
  return values.filter((_,at)=>at%2===1).reverse().concat(values.filter((_,at)=>at%2===0).reverse());
}
// Keep UTF-8 TSV text, empty hex payloads and URI selectors exactly as authored.
function rows(path,columns){return readFileSync(path,'utf8').split('\n').filter(line=>line!=='').map(line=>{const values=line.split('\t');if(values.length!==columns)throw Error('Invalid fixture column count');return values;});}
// Decode independent payload hex without invoking generated codecs.
function hex(value){if(value.length%2||!/^[0-9a-fA-F]*$/.test(value))throw Error('Invalid fixture hex');return Uint8Array.from({length:value.length/2},(_,index)=>parseInt(value.slice(index*2,index*2+2),16));}
const args=process.argv.slice(2),provider=await C.nodeProvider();
if(args.length===1&&args[0]==='--provider'){console.log(JSON.stringify(fingerprint()));}
else{
  if(args.length<4||args.length>5)throw Error('Expected FIXTURES PROFILE PERMUTATION OUTPUT [FORCE_ZIP64]');
  const [fixtures,profile,index,output]=args;if(!['zip-stored','zip-deflate','opc-stored','opc-deflate'].includes(profile))throw Error('Unknown profile');
  const force=args.length===5&&args[4]==='1';if(args.length===5&&!['0','1'].includes(args[4]))throw Error('Expected force flag 0 or 1');
  const opc=profile.startsWith('opc-'),compression=profile.endsWith('deflate')?'deflate':'stored',directory=join(fixtures,opc?'opc':'zip'),permutation=Number(index);
  const parts=rows(join(directory,'parts.tsv'),4).map(row=>new P.BinaryPart(hex(row[3]),row[0],row[1],0,compression));
  const links=rows(join(directory,'links.tsv'),5).map(row=>new P.Relationship(row[0],row[1],row[2],row[3],row[4]));
  const options={compressionProvider:provider,forceZip64:force};let data;
  if(opc)data=await P.encode(permute(parts,permutation),permute(links,permutation),options);
  else{const writer=new P.ZipWriter(options);try{for(const part of permute(parts,permutation))await writer.add({name:part.name,data:part.data,compression:part.compression});data=await writer.finish();}finally{writer.close();}}
  writeFileSync(output,data);console.log(JSON.stringify({profile,permutation,force_zip64:force,parts:parts.length,relationships:opc?links.length:0,provider:fingerprint(opc?8:0)}));
}
