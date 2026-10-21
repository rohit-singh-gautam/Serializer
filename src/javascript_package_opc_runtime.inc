// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejs(
  const ctNamespace = 'http://schemas.openxmlformats.org/package/2006/content-types', relNamespace = 'http://schemas.openxmlformats.org/package/2006/relationships', relMime = 'application/vnd.openxmlformats-package.relationships+xml';
  /** An opaque owning payload; encoding snapshots its mutable data before asynchronous work. */
  class BinaryPart { constructor(data, name, contentType, sourceIndex = 0, compression = 'stored') { Object.assign(this, {data, name, contentType, sourceIndex, compression}); } }
  /** A dynamically named or repeated native typed payload with its source occurrence index. */
  class ModelPart { constructor(value, name, contentType, sourceIndex = 0) { Object.assign(this, {value, name, contentType, sourceIndex}); } }
  /** An inert source-scoped OPC relationship; external targets are never opened. */
  class Relationship { constructor(source, id, type, target, mode = 'internal') { Object.assign(this, {source, id, type, target, mode}); } }
  /** Whole unknown parts and authored scoped links, preserved without interpreting their XML. */
  class Extensions { constructor(parts = [], relationships = []) { this.parts = parts; this.relationships = relationships; } }
  /** Unknown optional parts and links independently reject, skip or preserve; required items always reject. */
  class ExtensionPolicy {
    constructor({parts = 'reject', relationships = 'reject', isRequiredPart = null, isRequiredRelationship = null} = {}) {
      if (![parts, relationships].every(value => ['reject', 'skip', 'preserve'].includes(value)) || isRequiredPart !== null && typeof isRequiredPart !== 'function' || isRequiredRelationship !== null && typeof isRequiredRelationship !== 'function') throw new PackageError(Code.InvalidOptions, 'Invalid extension policy');
      Object.assign(this, {parts, relationships, isRequiredPart, isRequiredRelationship}); Object.freeze(this);
    }
  }
  /** A complete fresh aggregate and its independently preserved unknown context. */
  class Result { constructor(value, extensions) { Object.assign(this, {value, extensions}); } }
  // Normalize only ASCII identity; Unicode OPC names remain an explicitly unsupported profile.
  function fold(value) { return value.replace(/[A-Z]/g, character => character.toLowerCase()); }
  // Validate a canonical ASCII whole-part URI and keep infrastructure owned by the runtime.
  function partName(value, infrastructure = false) {
    if (typeof value !== 'string' || value.length > 65536 || !value.startsWith('/') || value.endsWith('/') || /[?#\\\x00-\x20\x7f-\uffff]/.test(value)) throw new PackageError(Code.InvalidPartName, 'Invalid OPC part name', value);
    for (const segment of value.slice(1).split('/')) {
      if (!segment || segment.endsWith('.')) throw new PackageError(Code.InvalidPartName, 'Invalid OPC path segment', value);
      for (let at = 0; at < segment.length; ++at) {
        const character = segment[at];
        if (character === '%') {
          const escaped = segment.slice(at + 1, at + 3); if (!/^[0-9A-Fa-f]{2}$/.test(escaped)) throw new PackageError(Code.InvalidPartName, 'Malformed OPC URI escape', value);
          const byte = Number.parseInt(escaped, 16); if (byte >= 128 || /[A-Za-z0-9\-._~\/\\]/.test(String.fromCharCode(byte))) throw new PackageError(Code.InvalidPartName, 'Noncanonical OPC URI escape', value); at += 2;
        } else if (!/[A-Za-z0-9\-._~!$&'()*+,;=:@]/.test(character)) throw new PackageError(Code.InvalidPartName, 'Invalid OPC URI character', value);
      }
    }
    if (!infrastructure && (fold(value) === '/[content_types].xml' || relationshipSource(value) !== null)) throw new PackageError(Code.InvalidPartName, 'Reserved OPC infrastructure name', value);
    return value;
  }
  // Recover standard relationship-part source identities without opening any target.
  function relationshipSource(name) {
    if (fold(name) === '/_rels/.rels') return '/';
    const match = /^(.*\/)\_rels\/([^/]+)\.rels$/i.exec(name); return match ? partName(match[1] + match[2], true) : null;
  }
  // Reject case aliases and file/descendant collisions before retaining payload metadata.
  function uniquePart(names, name) {
    const key = fold(name); if (names.has(key)) throw new PackageError(Code.InvalidPartName, 'Duplicate OPC part identity', name);
    for (const old of names.keys()) if (key.startsWith(old + '/') || old.startsWith(key + '/')) throw new PackageError(Code.InvalidPartName, 'OPC part is an ancestor of another part', name);
    names.set(key, true); return key;
  }
  // Parse MIME tokens and quoted parameters, preserving case-sensitive parameter values.
  function mimeIdentity(value) {
    if (typeof value !== 'string') throw new PackageError(Code.InvalidMetadata, 'Invalid OPC content type');
    const token = "[!#$%&'*+.^_`|~0-9A-Za-z-]+", base = new RegExp('^(' + token + ')/(' + token + ')').exec(value);
    if (!base) throw new PackageError(Code.InvalidMetadata, 'Invalid OPC content type');
    const parameters = new Map(); let at = base[0].length;
    const pattern = new RegExp('^[ \\t]*;[ \\t]*(' + token + ')[ \\t]*=[ \\t]*(' + token + '|"(?:[\\x20\\x21\\x23-\\x5b\\x5d-\\x7e]|\\\\[\\x20-\\x7e])*")[ \\t]*');
    while (at < value.length) {
      const match = pattern.exec(value.slice(at)); if (!match) throw new PackageError(Code.InvalidMetadata, 'Malformed MIME parameter');
      const key = fold(match[1]); if (parameters.has(key)) throw new PackageError(Code.InvalidMetadata, 'Duplicate MIME parameter');
      parameters.set(key, match[2].startsWith('"') ? match[2].slice(1, -1).replace(/\\(.)/g, '$1') : match[2]); at += match[0].length;
    }
    const media = fold(base[0]);
    if (parameters.size && [relMime, 'application/vnd.openxmlformats-package.core-properties+xml', 'application/vnd.openxmlformats-package.digital-signature-xmlsignature+xml', 'application/vnd.openxmlformats-package.digital-signature-origin', 'application/vnd.openxmlformats-package.digital-signature-certificate'].includes(media)) throw new PackageError(Code.InvalidMetadata, 'Reserved OPC content type cannot have parameters');
    return [media, Array.from(parameters).sort((a, b) => a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0)];
  }
  // Match media identities without erasing parameter-value case or quoted-pair meaning.
  function mimeEqual(left, right) { return JSON.stringify(mimeIdentity(left)) === JSON.stringify(mimeIdentity(right)); }
  // Split only literal delimiters; percent escapes and authored Unicode spelling remain inert.
  function splitUri(value) {
    let fragment = null, query = null, scheme = null, authority = null, at = value.indexOf('#');
    if (at >= 0) { fragment = value.slice(at + 1); value = value.slice(0, at); }
    at = value.indexOf('?'); if (at >= 0) { query = value.slice(at + 1); value = value.slice(0, at); }
    const colon = value.indexOf(':'), slash = value.indexOf('/');
    if (colon >= 0 && (slash < 0 || colon < slash)) { scheme = value.slice(0, colon); value = value.slice(colon + 1); }
    if (value.startsWith('//')) { at = value.indexOf('/', 2); authority = value.slice(2, at < 0 ? undefined : at); value = at < 0 ? '' : value.slice(at); }
    return {scheme, authority, path: value, query, fragment};
  }
  // Validate RFC3987 component scalars, percent triplets and query-only private-use scalars.
  function iriComponent(value, extra = '', query = false) {
    for (let at = 0; at < value.length;) {
      const scalar = value.codePointAt(at), character = String.fromCodePoint(scalar); at += character.length;
      if (character === '%') { if (!/^[0-9A-Fa-f]{2}$/.test(value.slice(at, at + 2))) throw new PackageError(Code.InvalidRelationship, 'Malformed relationship URI escape'); at += 2; continue; }
      const ordinary = scalar >= 0xa0 && scalar <= 0xd7ff || scalar >= 0xf900 && scalar <= 0xfdcf || scalar >= 0xfdf0 && scalar <= 0xffef || scalar >= 0x10000 && scalar <= 0xdfffd && (scalar & 0xffff) <= 0xfffd || scalar >= 0xe1000 && scalar <= 0xefffd;
      const privateUse = query && (scalar >= 0xe000 && scalar <= 0xf8ff || scalar >= 0xf0000 && scalar <= 0xffffd || scalar >= 0x100000 && scalar <= 0x10fffd);
      if (!ordinary && !privateUse && !(/[A-Za-z0-9\-._~!$&'()*+,;=]/.test(character) && scalar < 128 || extra.includes(character))) throw new PackageError(Code.InvalidRelationship, 'Invalid relationship IRI character');
    }
  }
  // Validate an IPv6 literal without Node, URL normalization or scoped-address extensions.
  function ipv6(value) {
    if (!value || value.includes('%') || /[^0-9a-fA-F:.]/.test(value) || value.split('::').length > 2) return false;
    const halves = value.split('::'), compressed = halves.length === 2;
    const pieces = compressed ? (halves[0] ? halves[0].split(':') : []).concat(halves[1] ? halves[1].split(':') : []) : value.split(':');
    if (pieces.some(piece => !piece)) return false; let count = 0;
    for (let i = 0; i < pieces.length; ++i) {
      const piece = pieces[i];
      if (piece.includes('.')) { if (i !== pieces.length - 1 || !value.endsWith(piece) || !/^(?:0|[1-9][0-9]{0,2})(?:\.(?:0|[1-9][0-9]{0,2})){3}$/.test(piece) || piece.split('.').some(item => Number(item) > 255)) return false; count += 2; }
      else { if (!/^[0-9a-fA-F]{1,4}$/.test(piece)) return false; ++count; }
    }
    return compressed ? count < 8 : count === 8;
  }
  // Validate inert URI references; external links never trigger network or filesystem access.
  function uri(value, absolute = false) {
    if (typeof value !== 'string' || /[\x00-\x20]/.test(value)) throw new PackageError(Code.InvalidRelationship, 'Invalid relationship URI');
    const parts = splitUri(value);
    if (parts.scheme !== null && !/^[A-Za-z][A-Za-z0-9+.-]*$/.test(parts.scheme) || absolute && (parts.scheme === null || parts.fragment !== null)) throw new PackageError(Code.InvalidRelationship, 'Invalid relationship URI scheme or type');
    let authority = parts.authority || ''; const userAt = authority.indexOf('@'); if (userAt >= 0) { iriComponent(authority.slice(0, userAt), ':'); authority = authority.slice(userAt + 1); }
    if (authority.startsWith('[')) {
      const end = authority.indexOf(']'), literal = authority.slice(1, end);
      if (end < 0 || !(ipv6(literal) || /^[vV][0-9a-fA-F]+\.[A-Za-z0-9._~!$&'()*+,;=:-]+$/.test(literal)) || !/^(?::[0-9]*)?$/.test(authority.slice(end + 1))) throw new PackageError(Code.InvalidRelationship, 'Invalid relationship IP authority');
    } else {
      const colon = authority.indexOf(':'); iriComponent(colon < 0 ? authority : authority.slice(0, colon)); if (colon >= 0 && !/^[0-9]*$/.test(authority.slice(colon + 1))) throw new PackageError(Code.InvalidRelationship, 'Invalid relationship port');
    }
    iriComponent(parts.path, ':@/'); iriComponent(parts.query || '', ':@/?', true); iriComponent(parts.fragment || '', ':@/?'); return parts;
  }
  // Resolve dot segments to whole-part identity, retaining original authored link text separately.
  function targetName(source, target) {
    const parts = uri(target); if (parts.scheme !== null || parts.authority !== null) throw new PackageError(Code.InvalidRelationship, 'Internal target cannot have scheme or authority', source);
    if (!parts.path) return partName(source);
    if (parts.path.includes('//') || parts.path.endsWith('/') || ['.', '..'].includes(parts.path.split('/').pop())) throw new PackageError(Code.InvalidRelationship, 'Relationship target identifies a directory', source);
    const segments = parts.path.startsWith('/') ? [] : source.split('/').slice(1, -1);
    for (const segment of parts.path.split('/')) { if (!segment || segment === '.') continue; if (segment === '..') segments.pop(); else segments.push(segment); }
    return partName('/' + segments.join('/'));
  }
  // Metadata whitespace uses XML's four exact characters rather than JavaScript Unicode trim.
  function collapse(value) { return value.replace(/[ \t\r\n]+/g, ' ').replace(/^ | $/g, ''); }
  // Parse only bounded standard OPC infrastructure, rejecting DTDs and unexpected nested content.
  function metadataRead(data, namespace, root, budget) {
)packagejs"
R"packagejs(    budget.consume('metadataBytes', data.length); budget.consume('workUnits', data.length); budget.consume('decoderAllocationBytes', data.length * 4);
    let text; try {
      const wide = data.length >= 2 && (data[0] === 255 && data[1] === 254 || data[0] === 254 && data[1] === 255);
      text = wide ? new TextDecoder(data[0] === 255 ? 'utf-16le' : 'utf-16be', {fatal: true}).decode(data) : srlDecoder.decode(data);
      const declaration = /^\uFEFF?<\?xml\s[^?]*\?>/.exec(text);
      if (declaration) { const encoding = /\bencoding\s*=\s*(['"])(.*?)\1/.exec(declaration[0]); if (encoding && fold(encoding[2]) !== (wide ? 'utf-16' : 'utf-8')) throw new Error('encoding'); }
    } catch { throw new PackageError(Code.InvalidMetadata, 'Invalid OPC metadata text encoding'); }
    const parser = new SerializerPackageSaxes({xmlns: true, defaultXMLVersion: '1.0', forceXMLVersion: true}), stack = []; let document = null, malformed = false, hookFailed = false, hookFailure;
    const check = () => { try { budget.check(); } catch (error) { hookFailed = true; hookFailure = error; throw error; } };
    parser.on('error', () => { malformed = true; }); parser.on('doctype', () => { malformed = true; });
    parser.on('xmldecl', value => { if (value.version !== '1.0') malformed = true; });
    parser.on('opentag', node => {
      check(); budget.consume('workUnits', 1); if (stack.length >= 128) throw new PackageError(Code.ResourceLimit, 'OPC metadata depth limit exceeded');
      const parent = stack.length ? stack[stack.length - 1] : null, attrs = [];
      budget.consume('decoderAllocationBytes', 192 + Object.keys(node.ns).length * 64);
      for (const attr of Object.values(node.attributes)) { budget.consume('workUnits', 1); if (attr.uri === 'http://www.w3.org/2000/xmlns/') continue; budget.consume('decoderAllocationBytes', 64); attrs.push({uri: attr.uri || '', local: attr.local, value: attr.value}); }
      const element = {uri: node.uri || '', local: node.local, attrs, ns: Object.assign(Object.create(parent ? parent.ns : null), node.ns), children: [], text: ''};
      if (parent) parent.children.push(element); else { if (document !== null) malformed = true; document = element; } stack.push(element);
    });
    parser.on('closetag', () => { stack.pop(); });
    const textContent = value => { if (stack.length) stack[stack.length - 1].text += value; else if (/[^ \t\r\n]/.test(value)) malformed = true; };
    parser.on('text', textContent); parser.on('cdata', textContent);
    try { for (let at = 0; at < text.length; at += chunkBytes) { check(); parser.write(text.slice(at, at + chunkBytes)); } parser.close(); } catch (error) { if (hookFailed) throw hookFailure; if (error instanceof PackageError) throw error; malformed = true; }
    budget.check(); if (malformed || document === null || stack.length) throw new PackageError(Code.InvalidMetadata, 'Invalid OPC infrastructure XML');
    if (namespace === relNamespace) document = relationshipsCompatibility(document, budget);
    if (document.uri !== namespace || document.local !== root || document.attrs.length || collapse(document.text)) throw new PackageError(Code.InvalidMetadata, 'Invalid OPC infrastructure XML root');
    const children = [];
    for (const child of document.children) {
      if (child.uri !== namespace || child.children.length || namespace !== relNamespace && collapse(child.text)) throw new PackageError(Code.InvalidMetadata, 'Unexpected OPC infrastructure content');
      const attrs = Object.create(null); for (const attr of child.attrs) { if (attr.uri || Object.hasOwn(attrs, attr.local)) throw new PackageError(Code.InvalidMetadata, 'Invalid OPC metadata attribute'); attrs[attr.local] = attr.value; }
      children.push({name: child.local, attrs});
    }
    return children;
  }
  // Escape authored attribute values while emitting canonical namespace-owned infrastructure.
  function xmlAttribute(value) { return value.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/"/g, '&quot;').replace(/\r/g, '&#13;').replace(/\n/g, '&#10;').replace(/\t/g, '&#9;'); }
  // Produce stable UTF-8 metadata independent of host XML serializer prefix choices.
  function metadataWrite(namespace, root, children, budget) {
    let text = '';
    const append = value => { budget.check(); let count = 0; for (const character of value) { const scalar = character.codePointAt(0); count += scalar < 128 ? 1 : scalar < 2048 ? 2 : scalar < 65536 ? 3 : 4; } budget.consume('metadataBytes', count); budget.consume('workUnits', count); text += value; };
    append('<?xml version="1.0" encoding="UTF-8"?><' + root + ' xmlns="' + namespace + '">');
    for (const child of children) { append('<' + child.name); for (const [name, value] of Object.entries(child.attrs)) { if (value.length > budget.remaining('metadataBytes')) throw new PackageError(Code.ResourceLimit, 'Metadata attribute exceeds limit'); append(' ' + name + '="' + xmlAttribute(value) + '"'); } append('/>'); }
    append('</' + root + '>'); return srlUtf8(text);
  }
)packagejs"
