// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejs(
  // Physical directory, device and symlink entries do not become OPC payload parts.
  function physicalFile(entry) {
    if (entry.name.endsWith('/')) return false;
    if ([0, 10, 14].includes(entry.creatorSystem) && (entry.externalAttributes & 0x18)) return false;
    if ([3, 19].includes(entry.creatorSystem)) { const kind = Math.floor(entry.externalAttributes / 65536) & 0xf000; if (kind && kind !== 0x8000) return false; }
    return true;
  }
  // Validate scoped IDs and all source/target membership before decoding models or publishing output.
  function validateLinks(links, parts, budget) {
    const seen = new Map(), result = [];
    for (const supplied of links) {
      budget.check();try{budget.consume('relationships',1,supplied.source);budget.consume('references',1,supplied.source);}catch(error){if(error instanceof PackageError){error.fieldPath='relationships';error.relationshipId=supplied.id;error.reference=supplied.target;}throw error;}
      const link = new Relationship(supplied.source, supplied.id, supplied.type, supplied.target, supplied.mode);
      if (![link.source, link.id, link.type, link.target].every(value => typeof value === 'string')) throw new PackageError(Code.InvalidRelationship, 'Relationship fields must be strings');
      const textBytes = link.source.length + link.id.length + link.type.length + link.target.length;
      if (textBytes > budget.limits.maxMetadataBytes) throw new PackageError(Code.ResourceLimit, 'Relationship text exceeds metadata limit');
      budget.consume('workUnits', textBytes);
      if (!SrlXmlNcName.test(link.id)) throw new PackageError(Code.InvalidRelationship, 'Invalid relationship ID', link.source);
      if (link.source !== '/') partName(link.source);
      if (link.source !== '/' && !parts.has(fold(link.source))) throw new PackageError(Code.MissingPart, 'Relationship source is missing', link.source, link.id);
      const key = fold(link.source) + '\0' + link.id; if (seen.has(key)) throw new PackageError(Code.InvalidRelationship, 'Duplicate source-scoped relationship ID', link.source, link.id); seen.set(key, true);
      uri(link.type, true);
      if (!['internal', 'external'].includes(link.mode)) throw new PackageError(Code.InvalidRelationship, 'Invalid relationship target mode', link.source, link.id);
      if (link.mode === 'external') uri(link.target);
      else { const target = targetName(link.source, link.target); if (!parts.has(fold(target))) throw new PackageError(Code.MissingPart, 'Relationship target is missing', target, link.id); }
      result.push(Object.freeze(link));
    }
    return result;
  }
  // Resolve standard metadata and verify excluded physical files without treating them as payloads.
  async function opcRead(archive, budget) {
    const manifests = archive.entries.filter(entry => fold(entry.name) === '[content_types].xml');
    if (manifests.length !== 1 || !physicalFile(manifests[0])) throw new PackageError(Code.InvalidMetadata, 'Missing or duplicate OPC content type manifest');
    const defaults = new Map(), overrides = new Map();
    for (const child of metadataRead(await archive.read(manifests[0].name), ctNamespace, 'Types', budget)) {
      const attrs = child.attrs, keys = Object.keys(attrs);
      if (child.name === 'Default' && keys.length === 2 && Object.hasOwn(attrs, 'Extension') && Object.hasOwn(attrs, 'ContentType')) {
        const key = fold(attrs.Extension); if (!/^(?:[A-Za-z0-9_~!$&'()*+,:=@-]|%[0-9a-fA-F]{2})+$/.test(key) || defaults.has(key)) throw new PackageError(Code.InvalidMetadata, 'Invalid or duplicate default extension');
        mimeIdentity(attrs.ContentType); defaults.set(key, attrs.ContentType);
      } else if (child.name === 'Override' && keys.length === 2 && Object.hasOwn(attrs, 'PartName') && Object.hasOwn(attrs, 'ContentType')) {
        const key = fold(partName(collapse(attrs.PartName), true)); if (overrides.has(key)) throw new PackageError(Code.InvalidMetadata, 'Duplicate content type override');
        mimeIdentity(attrs.ContentType); overrides.set(key, attrs.ContentType);
      } else throw new PackageError(Code.InvalidMetadata, 'Unknown OPC content type declaration');
    }
    if (!defaults.size && !overrides.size) throw new PackageError(Code.InvalidMetadata, 'Empty OPC content type manifest');
    const parts = new Map(), relationshipParts = [], identities = new Map(), mapped = new Map();
    for (const entry of archive.entries) {
      budget.check(); if (entry === manifests[0]) continue; const name = '/' + entry.name;
      let mappedPart = physicalFile(entry);
      if (mappedPart) {
        const unsupported = /[^\x00-\x7f]|%[89a-fA-F][0-9a-fA-F]/.test(name);
        const probe = unsupported ? name.replace(/%[89a-fA-F][0-9a-fA-F]/g, '%20').replace(/[^\x00-\x7f]/g, '_') : name;
        try { partName(probe, true); } catch (error) { if (!(error instanceof PackageError) || error.code !== Code.InvalidPartName) throw error; mappedPart = false; }
        if (mappedPart && unsupported) throw new PackageError(Code.Unsupported, 'Non-ASCII OPC part names are unsupported', name);
      }
      if (!mappedPart) { await archive.read(entry.name); continue; }
      const key = uniquePart(identities, name); mapped.set(key, true);
      const extension = entry.name.slice(entry.name.lastIndexOf('.') + 1), contentType = overrides.get(key) || defaults.get(fold(extension));
      if (!contentType) throw new PackageError(Code.InvalidMetadata, 'OPC part has no content type', name);
      const source = relationshipSource(name);
      if (source !== null) {
        if (fold(contentType) !== relMime) throw new PackageError(Code.InvalidMetadata, 'Relationship infrastructure has the wrong MIME type', name);
        relationshipParts.push({entry, source});
      } else {
        partName(name); if (fold(contentType) === relMime) throw new PackageError(Code.InvalidMetadata, 'Relationship MIME type requires infrastructure', name);
        parts.set(key, {name, contentType, entry});
      }
    }
    for (const key of overrides.keys()) if (!mapped.has(key)) throw new PackageError(Code.InvalidMetadata, 'Content type override names a missing or reserved part', key);
    const links = [];
    for (const {entry, source} of relationshipParts) {
      if (source !== '/' && !parts.has(fold(source))) throw new PackageError(Code.MissingPart, 'Relationship part source is missing', source);
      for (const child of metadataRead(await archive.read(entry.name), relNamespace, 'Relationships', budget)) {
        const attrs = child.attrs, keys = Object.keys(attrs);
        if (child.name !== 'Relationship' || !['Id', 'Type', 'Target'].every(key => Object.hasOwn(attrs, key)) || keys.some(key => !['Id', 'Type', 'Target', 'TargetMode'].includes(key))) throw new PackageError(Code.InvalidMetadata, 'Invalid relationship declaration', '/' + entry.name);
        const mode = collapse(attrs.TargetMode === undefined ? 'Internal' : attrs.TargetMode);
        if (!['Internal', 'External'].includes(mode)) throw new PackageError(Code.InvalidRelationship, 'Invalid relationship TargetMode', source);
        links.push(new Relationship(source, collapse(attrs.Id), collapse(attrs.Type), collapse(attrs.Target), fold(mode)));
      }
    }
    return {parts, relationships: validateLinks(links, parts, budget)};
  }
  // Snapshot all public opaque bytes before the first asynchronous provider or sink callback.
  function snapshotParts(parts, state) {
    if (!Array.isArray(parts) || parts.length > state.budget.limits.maxEntries) throw new PackageError(Code.ResourceLimit, 'Package part count exceeds limit');
    return parts.map(part => {
      if (typeof part.name !== 'string' || typeof part.contentType !== 'string') throw new PackageError(Code.InvalidOptions, 'Part metadata must be strings');
      state.budget.consume('metadataBytes', part.name.length + part.contentType.length, part.name);
      partName(part.name); mimeIdentity(part.contentType); if (fold(part.contentType) === relMime) throw new PackageError(Code.InvalidMetadata, 'Relationship MIME type is reserved', part.name);
      if (!(part.data instanceof Uint8Array) || part.data.length > state.budget.limits.maxPartBytes) throw new PackageError(Code.InvalidOptions, 'Invalid opaque package payload', part.name);
      admitCompression(part.compression === 'deflate' ? 8 : part.compression === 'stored' ? 0 : -1, state.provider);
      state.budget.consume('decoderAllocationBytes', part.data.length, part.name);
      state.hold(part.data.length, part.name);
      return new BinaryPart(copyBytes(part.data, state), part.name, part.contentType, part.sourceIndex, part.compression);
    });
  }
  // Assemble runtime-owned metadata and deterministic relationship groups using one shared budget.
  async function opcEncode(parts, relationships, options = {}) {
    const budget = getBudget(options), state = new State(budget); state.provider = options.compressionProvider;
    admitCompression(8, options.compressionProvider);
    try {
    const snapshots = snapshotParts(parts, state), identities = new Map(), lookup = new Map();
    for (const part of snapshots) lookup.set(uniquePart(identities, part.name), part);
    const links = validateLinks(relationships, lookup, budget), groups = new Map(), children = [], entries = snapshots.slice();
    for (const part of snapshots.slice().sort((a, b) => a.name < b.name ? -1 : a.name > b.name ? 1 : 0)) children.push({name: 'Override', attrs: {PartName: part.name, ContentType: part.contentType}});
    if (!snapshots.length && !links.length) children.push({name: 'Default', attrs: {Extension: 'rels', ContentType: relMime}});
    for (const link of links) { if (!groups.has(link.source)) groups.set(link.source, []); groups.get(link.source).push(link); }
    for (const source of Array.from(groups.keys()).sort()) {
      const at = source.lastIndexOf('/'), name = source === '/' ? '/_rels/.rels' : source.slice(0, at + 1) + '_rels/' + source.slice(at + 1) + '.rels'; uniquePart(identities, name);
      const declarations = groups.get(source).slice().sort((a, b) => a.id < b.id ? -1 : a.id > b.id ? 1 : 0).map(link => {
        const attrs = {Id: link.id, Type: link.type, Target: link.target}; if (link.mode === 'external') attrs.TargetMode = 'External'; return {name: 'Relationship', attrs};
      });
      children.push({name: 'Override', attrs: {PartName: name, ContentType: relMime}}); entries.push(new BinaryPart(metadataWrite(relNamespace, 'Relationships', declarations, budget), name, relMime, 0, 'deflate'));
    }
    entries.push(new BinaryPart(metadataWrite(ctNamespace, 'Types', children, budget), '/[Content_Types].xml', '', 0, 'deflate'));
    const writer = new ZipWriter({...options, limits: undefined, budget});
    try { for (const part of entries) { state.poll(); await writer.add({name: part.name.slice(1), data: part.data, compression: part.compression}); state.poll(); } const data = await writer.finish(); state.poll(); return data; }
    finally { writer.close(); }
    } finally { state.releaseHolds(); }
  }
  // Enforce total role cardinality before any complete aggregate can be published.
  function occurs(count, role) { if (count < role.minimum || role.maximum !== null && count > role.maximum) { const error = new PackageError(Code.Occurrence, 'Package role occurrence constraint failed'); error.fieldPath = role.field; throw error; } }
  // Create a schema-bound facade after model constructors exist; ordinary model codec APIs stay unchanged.
  function profile(roles) {
    // Bridge native scalar/events and identity tuples through the package ledger.
    function nativeLimits(state,role,part,link='') {
      return new XmlLimits({expectedType:role.expectedTypeUri+'\0'+role.expectedTypeLocal,maxBytes:Math.min(state.budget.limits.maxPartBytes,16777216),operation(resource,count,field){
        try{state.poll();}catch(error){throw state.fail(error);}try{state.budget.consume(resource==='allocation'?'decoderAllocationBytes':resource==='reference'?'references':'workUnits',count,part);}
        catch(error){if(error instanceof PackageError){error.fieldPath=role.field+(field?'.'+field:'');if(!error.relationshipId)error.relationshipId=link;}throw state.fail(error);}
        try{state.poll();}catch(error){throw state.fail(error);}
      }});
    }
    // Translate only internal XML quotas; codec and arbitrary caller failures keep their identities.
    function nativeFailure(cause,state,role,part,link){if(state.failed)return state.failure;if(!(cause instanceof SrlXmlLimitError))return cause;const failure=new PackageError(Code.ResourceLimit,'Native XML resource limit exceeded',part,link);failure.fieldPath=role.field;failure.cause=cause;return failure;}
    /** Borrow typed role values; async encoding snapshots binary spans and eagerly encodes native XML. */
    class Input { constructor(values = {}) { for (const role of roles) this[role.field] = values[role.field] === undefined ? role.singleton ? null : [] : values[role.field]; this.extensions = values.extensions || null; } }
    /** Own the fresh decoded role aggregate. */
    class Value extends Input {}
    // Build a complete typed key/reference inventory before any aggregate can escape.
    function references(value,budget,locations=null){
      const keys=new Map(),pending=[];
      function consume(resource,count,part,id,field){budget.check();try{budget.consume(resource,count,part);}catch(error){if(error instanceof PackageError){error.fieldPath=field;error.relationshipId=id;}throw error;}}
      for(const role of roles){if(!(role.keys||[]).length&&!(role.references||[]).length)continue;const supplied=value[role.field];if(!role.singleton&&!Array.isArray(supplied))throw new PackageError(Code.InvalidOptions,'Repeated package role requires an array');const items=role.singleton?(supplied==null?[]:[supplied]):supplied;
        for(let occurrence=0;occurrence<items.length;++occurrence){const item=items[occurrence],model=role.singleton?item:item?.value;if(!(model instanceof role.model))throw new PackageError(Code.InvalidOptions,'Invalid typed package model');const [part,id]=locations?locations.get(role.field)[occurrence]:[role.name===null?item.name:role.name,role.id===null?'srl_'+role.role+'_'+occurrence:role.id];
          for(const key of role.keys||[]){const raw=key.get(model),identity=typeof raw+':'+String(raw),slot=key.scope+'\0'+identity,path=role.field+'.'+key.field;consume('references',1,part,id,path);consume('workUnits',identity.length+1,part,id,path);consume('decoderAllocationBytes',128+2*slot.length,part,id,path);if(keys.has(slot)){const error=new PackageError(Code.InvalidRelationship,'Duplicate application key',part,id);error.fieldPath=path;throw error;}keys.set(slot,role.model);}
          for(const reference of role.references||[]){const raw=reference.get(model),identity=typeof raw+':'+String(raw),path=role.field+'.'+reference.field;consume('references',1,part,id,path);consume('decoderAllocationBytes',160+2*identity.length,part,id,path);pending.push([reference.scope+'\0'+identity,roles.find(item=>item.field===reference.target).model,path,part,id]);}
        }
      }
      for(const [key,target,field,part,id]of pending){consume('workUnits',key.length+1,part,id,field);if(keys.get(key)!==target){const error=new PackageError(Code.InvalidRelationship,'Missing or wrong-type application key',part,id);error.fieldPath=field;throw error;}}
    }
)packagejs"
R"packagejs(
    // Validate runtime overrides against the closed role inventory before provider work.
    function profiles(options){const selected=new Map(),supplied=options.profiles||{};for(const key of Object.keys(supplied))if(!roles.some(role=>role.role===key))throw new PackageError(Code.InvalidOptions,'Unknown package profile role');for(const role of roles){const compression=!Object.hasOwn(supplied,role.role)||supplied[role.role]===undefined?role.compression:supplied[role.role];if(!['stored','deflate'].includes(compression)||role.policy==='fixed'&&compression!==role.compression)throw new PackageError(Code.InvalidOptions,'Conflicting package profile override');selected.set(role.field,compression);}return selected;}
    // Snapshot selected synchronous directions before an asynchronous provider can change caller options.
    function codecs(options){const selected=new Map(),supplied=options.codecs;if(supplied===undefined)return selected;if(supplied===null||typeof supplied!=='object'||Array.isArray(supplied))throw new PackageError(Code.InvalidOptions,'Expected a role codec table');for(const key of Object.keys(supplied)){const role=roles.find(role=>role.role===key),value=supplied[key];if(!role||role.model===null||value===null||typeof value!=='object'||Array.isArray(value))throw new PackageError(Code.InvalidOptions,'Invalid selected codec role');const encode=value.encode,decode=value.decode;if(encode!==undefined&&typeof encode!=='function'||decode!==undefined&&typeof decode!=='function')throw new PackageError(Code.InvalidOptions,'Expected synchronous codec functions');selected.set(role.field,Object.freeze({encode,decode}));}return selected;}
    // Check immutable expanded root/type before a selected decoder receives the complete XML payload.
    function typedHeader(text,state,role,part,link){
      const stop={},parser=new SerializerPackageSaxes({xmlns:true,defaultXMLVersion:'1.0',forceXMLVersion:true});let found=false;
      const failure=()=>{const error=new PackageError(Code.InvalidMetadata,'Payload has unexpected XML root or type',part,link);error.fieldPath=role.field;return error;};
      const size=srlXmlUtf8Length(text);if(size>state.budget.limits.maxPartBytes)throw new PackageError(Code.ResourceLimit,'Payload exceeds part limit',part,link);state.budget.consume('workUnits',size,part);state.budget.consume('decoderAllocationBytes',Math.min(2*text.length,2*chunkBytes),part);
      parser.on('error',()=>{throw failure();});parser.on('doctype',()=>{throw failure();});
      parser.on('xmldecl',value=>{if(value.version!=='1.0'||value.encoding&&fold(value.encoding)!=='utf-8')throw failure();});
      parser.on('opentag',node=>{if(srlXmlTokenName(node.uri||'',node.local)!==role.model._srlXmlToken.root)throw failure();for(const attribute of Object.values(node.attributes)){if(attribute.uri!=='http://www.w3.org/2001/XMLSchema-instance'||attribute.local!=='type')continue;const parts=srlXmlTrim(attribute.value).split(':');if(parts.length>2||parts.some(value=>!SrlXmlNcName.test(value)))throw failure();const prefix=parts.length===2?parts[0]:'',local=parts[parts.length-1],uri=node.ns[prefix]||'';if(prefix&&!Object.hasOwn(node.ns,prefix)||uri!==role.expectedTypeUri||local!==role.expectedTypeLocal)throw failure();}found=true;throw stop;});
      try{for(let at=0;at<text.length;at+=chunkBytes){state.poll();parser.write(text.slice(at,at+chunkBytes));}parser.close();}catch(error){if(error!==stop)throw error;}if(!found)throw failure();state.poll();
    }
    // Retain even falsy thrown values and reject asynchronous codecs before any package publication.
    function selectedCodec(callback,value,limits,state,role,part,link){try{state.poll();const context=Object.freeze({budget:state.budget,role:role.role,part,relationshipId:link}),result=callback(value,limits,context);if(result&&typeof result.then==='function'){NativePromise.resolve(result).catch(()=>{});throw new PackageError(Code.InvalidOptions,'Selected codecs must return synchronously',part,link);}state.poll();return result;}catch(error){throw state.fail(nativeFailure(error,state,role,part,link));}}
    // Prepare all native payloads and metadata before the first asynchronous provider call.
    async function encode(value, options = {}) {
)packagejs"
R"packagejs(      const budget = getBudget(options), state = new State(budget), parts = [], links = [], sources = new Map(); state.provider = options.compressionProvider;
      const selectedProfiles=profiles(options),selectedCodecs=codecs(options);references(value,budget);admitCompression(8, options.compressionProvider);
      try {
      for (const role of roles) {
        const supplied = value[role.field];
        if (!role.singleton && !Array.isArray(supplied)) throw new PackageError(Code.InvalidOptions, 'Repeated package role requires an array');
        const items = role.singleton ? supplied === null || supplied === undefined ? [] : [supplied] : supplied; occurs(items.length, role);
        if (items.length > budget.limits.maxEntries - parts.length) throw new PackageError(Code.ResourceLimit, 'Package part count exceeds limit');
        const discovered = [], parents = role.source === '/' ? ['/'] : sources.get(role.source);
        for (let index = 0; index < items.length; ++index) {
          state.poll(); const item = items[index], name = role.name === null ? item.name : role.name, contentType = role.mime === null ? item.contentType : role.mime, parent = role.singleton ? 0 : integer(item.sourceIndex, 'source occurrence');
          if (typeof name !== 'string' || typeof contentType !== 'string') throw new PackageError(Code.InvalidOptions, 'Part metadata must be strings');
          budget.consume('metadataBytes', name.length + contentType.length, name);
          if (!parents || parent >= parents.length) throw new PackageError(Code.InvalidRelationship, 'Package source occurrence is missing', name);
          partName(name); mimeIdentity(contentType); let data;
          if (role.model !== null) {
            const model = role.singleton ? item : item.value; if (!(model instanceof role.model)) throw new PackageError(Code.InvalidOptions, 'Typed package resource has the wrong native type', name);
            const id=role.id===null?'srl_'+role.role+'_'+index:role.id,codec=selectedCodecs.get(role.field)?.encode;state.poll();try{data=srlXmlWithPackageQuota(()=>{const limits=nativeLimits(state,role,name,id),text=codec?selectedCodec(codec,model,limits,state,role,name,id):model.encodeXml(limits);if(typeof text!=='string')throw new PackageError(Code.InvalidOptions,'Selected encoder must return XML text',name,id);if(codec)typedHeader(text,state,role,name,id);return srlUtf8(text);});}catch(cause){throw nativeFailure(cause,state,role,name,id);}state.poll();
)packagejs"
R"packagejs(          } else data = item.data;
          if (!(data instanceof Uint8Array)) throw new PackageError(Code.InvalidOptions, 'Package resource requires bytes', name);
          if (data.length > budget.limits.maxPartBytes) throw new PackageError(Code.ResourceLimit, 'Package part extent exceeds limit', name);
          budget.consume('decoderAllocationBytes', data.length, name); state.hold(data.length, name);
          const independent = copyBytes(data, state); parts.push(new BinaryPart(independent, name, contentType, parent, selectedProfiles.get(role.field)));
          links.push(new Relationship(parents[parent], role.id === null ? 'srl_' + role.role + '_' + index : role.id, role.relationship, name)); discovered.push(name);
        }
        sources.set(role.field, discovered);
      }
      const extensions = value.extensions || new Extensions(); const opaque = snapshotParts(extensions.parts, state);
      if (opaque.length > budget.limits.maxEntries - parts.length) throw new PackageError(Code.ResourceLimit, 'Package part count exceeds limit');
      for (const part of opaque) parts.push(part);
      for (const link of extensions.relationships) links.push(new Relationship(link.source, link.id, link.type, link.target, link.mode));
      return await opcEncode(parts, links, {...options, limits: undefined, budget});
      } finally { state.releaseHolds(); }
    }
    // Discover alternate part names through source-scoped links and publish only after complete validation.
    async function load(source, options = {}) {
      const budget = getBudget(options), state = new State(budget), policy = options.policy === undefined ? new ExtensionPolicy() : options.policy;
      if (!(policy instanceof ExtensionPolicy) || options.validate !== undefined && typeof options.validate !== 'function') throw new PackageError(Code.InvalidOptions, 'Invalid package load policy');
      const selectedCodecs=codecs(options);
      const archive = await ZipReader.open(source, {...options, limits: undefined, budget});
      try {
        const metadata = await opcRead(archive, budget), candidate = new Value(), sources = new Map(), locations=new Map(), decoded = new Map(), selected = new Map();
        for (const role of roles) {
          const values = [], discovered = [], roleLocations=[], parents = role.source === '/' ? ['/'] : sources.get(role.source);
          for (let parent = 0; parent < parents.length; ++parent) for (let index = 0; index < metadata.relationships.length; ++index) {
            state.poll(); budget.consume('workUnits', 1); const link = metadata.relationships[index];
            if (fold(link.source) !== fold(parents[parent]) || link.type !== role.relationship) continue;
            if (link.mode !== 'internal') throw new PackageError(Code.InvalidRelationship, 'Typed role requires an internal target', link.source, link.id);
            const key = fold(targetName(link.source, link.target)), part = metadata.parts.get(key);
            if (role.mime !== null && !mimeEqual(part.contentType, role.mime)) throw new PackageError(Code.ContentType, 'Typed package MIME mismatch', part.name, link.id);
            if (decoded.has(key)) throw new PackageError(Code.InvalidRelationship, 'Package part is claimed by multiple role occurrences', part.name, link.id);
            if(role.policy==='fixed'&&part.entry.method!==role.compression)throw new PackageError(Code.InvalidOptions,'Fixed package compression mismatch',part.name,link.id);
            budget.consume('decoderAllocationBytes',64,part.name);roleLocations.push([part.name,link.id]);selected.set(index, true); decoded.set(key, true); const data = await archive.read(part.entry.name); state.poll();
            if (role.model !== null) {
              const codec=selectedCodecs.get(role.field)?.decode;let model; try { model = srlXmlWithPackageQuota(()=>{const text=srlDecoder.decode(data),limits=nativeLimits(state,role,part.name,link.id);if(codec)typedHeader(text,state,role,part.name,link.id);const value=codec?selectedCodec(codec,text,limits,state,role,part.name,link.id):role.model.decodeXml(text,limits);if(!(value instanceof role.model))throw new PackageError(Code.InvalidMetadata,'Selected decoder returned the wrong native model',part.name,link.id);return value;}); } catch (cause) { const selected=nativeFailure(cause,state,role,part.name,link.id);if(state.failed||selected instanceof PackageError)throw selected;const error = new PackageError(Code.InvalidMetadata, 'Typed package XML decoding failed', part.name, link.id); error.cause = selected; throw error; }
              state.poll(); values.push(role.singleton ? model : new ModelPart(model, part.name, part.contentType, parent));
            } else values.push(new BinaryPart(data, part.name, part.contentType, parent, part.entry.method)); discovered.push(part.name);
          }
          occurs(values.length, role); candidate[role.field] = role.singleton ? values.length ? values[0] : null : values; sources.set(role.field, discovered);locations.set(role.field,roleLocations);
        }
        const opaque = [], preservedLinks = [];
        for (const [key, part] of metadata.parts) {
          if (decoded.has(key)) continue; state.poll(); const required = policy.isRequiredPart ? policy.isRequiredPart(part) : false; state.poll();
          if (required && typeof required.then === 'function') NativePromise.resolve(required).catch(() => {});
          if (typeof required !== 'boolean') throw new PackageError(Code.InvalidOptions, 'Required-part policy must return a synchronous boolean');
          if (required || policy.parts === 'reject') throw new PackageError(Code.UnknownPart, 'Unknown required or rejected package part', part.name);
          const data = await archive.read(part.entry.name); state.poll(); if (policy.parts === 'preserve') { budget.consume('preservedBytes', data.length, part.name); opaque.push(new BinaryPart(data, part.name, part.contentType, 0, part.entry.method)); }
        }
        for (let index = 0; index < metadata.relationships.length; ++index) {
          if (selected.has(index)) continue; const link = metadata.relationships[index]; state.poll(); const required = policy.isRequiredRelationship ? policy.isRequiredRelationship(link) : false; state.poll();
          if (required && typeof required.then === 'function') NativePromise.resolve(required).catch(() => {});
          if (typeof required !== 'boolean') throw new PackageError(Code.InvalidOptions, 'Required-relationship policy must return a synchronous boolean');
          if (required || policy.relationships === 'reject') throw new PackageError(Code.UnknownRelationship, 'Unknown required or rejected package relationship', link.source, link.id);
          if (policy.relationships === 'preserve') { budget.consume('preservedBytes', srlUtf8(link.source + link.id + link.type + link.target).length); preservedLinks.push(link); }
        }
        references(candidate,budget,locations);candidate.extensions = new Extensions(opaque, preservedLinks);
        if (options.validate) { state.poll(); await options.validate(candidate, budget); state.poll(); }
        state.poll(); return new Result(candidate, candidate.extensions);
      } finally { await archive.close(); }
    }
    // Prepare the complete archive before publishing independent chunks to a borrowed sink.
    async function write(sink, value, options = {}) {
      if (!sink || typeof sink.write !== 'function') throw new PackageError(Code.InvalidOptions, 'Expected borrowed byte sink');
      const budget = getBudget(options), state = new State(budget), data = await encode(value, {...options, limits: undefined, budget});
      for (let at = 0; at < data.length; at += chunkBytes) { state.poll(); const chunk = data.slice(at, at + chunkBytes); budget.consume('ioBytes', chunk.length); await sink.write(chunk); state.poll(); } state.poll();
    }
    // Reload-validate staging, then use the explicitly acquired same-directory atomic filesystem host.
    async function save(path, value, options = {}) {
      if (options.host !== filesystemHost || !filesystemHost) throw new PackageError(Code.InvalidOptions, 'Atomic save requires an explicitly acquired Node host');
      const budget = getBudget(options), state = new State(budget), shared = {...options, limits: undefined, budget};
      const data = await encode(value, shared); state.poll(); await load(data, {...shared, policy: new ExtensionPolicy({parts: 'preserve', relationships: 'preserve'})}); state.poll();
      await options.host.save(path, data, budget, options.fault === undefined ? null : options.fault);
    }
    return Object.freeze({Input, Value, encode, load, write, save});
  }
  return Object.freeze({Code, Error: PackageError, Limits, Budget, ZipReader, ZipWriter, nodeHost, BinaryPart, ModelPart, Relationship, Extensions, ExtensionPolicy, Result, encode: opcEncode, profile});
})();
)packagejs"
