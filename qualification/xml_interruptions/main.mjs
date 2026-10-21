// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {readFileSync,writeFileSync} from 'node:fs';
import {XmlExampleOrder,Protocol,XmlLimits} from './schema.mjs';

// Exercise the same public operation callback from actual JS and strict TS consumers.
for (let index=3; index<process.argv.length; ++index) {
  const text = new TextDecoder().decode(readFileSync(process.argv[index]));
  const value = XmlExampleOrder.decodeXml(text);
  const canonical = value.encode(Protocol.BINARY_NONE);
  let verified=0;
  for (const writing of [false,true]) {
    for (const threshold of [1,1000,0]) {
      let polls=0;
      const deadline=process.hrtime.bigint()-1n;
      const marker=new Error('Caller XML interruption');
      const limits=new XmlLimits({operation:()=>{
        if (++polls>=threshold && threshold!==0 || threshold===0 && process.hrtime.bigint()>=deadline) throw marker;
      }});
      let failed=false;
      try {
        if (writing) value.encodeXml(limits);
        else XmlExampleOrder.decodeXml(text,limits);
      } catch (failure) { if (failure!==marker) throw failure; failed=true; }
      if (!failed || threshold!==0 && polls!==threshold) throw new Error('Interrupted operation returned a candidate');
      const recovered=XmlExampleOrder.decodeXml(value.encodeXml()).encode(Protocol.BINARY_NONE);
      if (canonical.toString()!==recovered.toString() || canonical.toString()!==value.encode(Protocol.BINARY_NONE).toString()) throw new Error('Recovery changed values');
      ++verified;
    }
  }
  writeFileSync(process.argv[2]+'/'+(index-2)+'.wire',canonical);
  console.log(JSON.stringify({fixture:index-2,verified_interruptions:verified,recovered:true}));
}
