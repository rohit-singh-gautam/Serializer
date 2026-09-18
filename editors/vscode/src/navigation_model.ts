import * as path from 'node:path';

export interface Span { start: number; end: number }
interface Token extends Span { text: string }
export interface SchemaInclude extends Span { name: string }
export interface TypeSymbol extends Span {
  name: string;
  qualified: string;
  kind: 'class' | 'enum';
  body: number;
  declaration: number;
  definition: boolean;
}
export interface SourceIndex {
  symbols: TypeSymbol[];
  includes: SchemaInclude[];
  references: Array<Span & { name: string; scope: string[]; kind?: TypeSymbol['kind'] }>;
}

/** Resolve schema include shorthand without searching for alternate files or extensions. */
function schemaIncludeName(name: string): string | undefined {
  if (!/^(?!\/)[A-Za-z_0-9./-]+$/.test(name) || name.endsWith('/')) { return undefined; }
  const filename = path.posix.basename(name);
  if (filename === '.' || filename === '..') { return undefined; }
  const extension = path.posix.extname(name);
  if (extension === '.serializer') { return name; }
  return extension === '' ? `${name}.serializer` : undefined;
}

/** Hide comments and literals while preserving UTF-16 offsets used by VS Code. */
export function codeOnly(text: string): string {
  return text.replace(/\/\*[\s\S]*?(?:\*\/|$)|\/\/[^\r\n]*|(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\1"|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'/g,
    match => match.replace(/[^\r\n]/g, ' '));
}

/** Read declarations and schema type positions, ignoring comments, literals and member names. */
export function indexSource(text: string, schema: boolean): SourceIndex {
  const code = codeOnly(text);
  const tokens: Token[] = [...code.matchAll(/[A-Za-z_][A-Za-z_0-9]*|::|[^\s]/g)]
    .map(match => ({ text: match[0], start: match.index!, end: match.index! + match[0].length }));
  const result: SourceIndex = { symbols: [], includes: [], references: [] };
  const scopes: string[][] = [[]];
  const namespaceBodies = new Map<number, string[]>();
  const currentScope = (): string[] => scopes[scopes.length - 1];

  /** Consume a qualified identifier and retain every component as one navigation range. */
  function qualified(start: number): { next: number; name: string; start: number; end: number } | undefined {
    let next = start;
    if (tokens[next]?.text === '::') { ++next; }
    if (!/^[A-Za-z_]\w*$/.test(tokens[next]?.text ?? '')) { return undefined; }
    ++next;
    while (tokens[next]?.text === '::' && /^[A-Za-z_]\w*$/.test(tokens[next + 1]?.text ?? '')) {
      next += 2;
    }
    return { next, name: tokens.slice(start, next).map(token => token.text).join(''),
      start: tokens[start].start, end: tokens[next - 1].end };
  }

  /** Record only type operands in fields, containers, unions and base-class lists. */
  function typeReference(start: number, depth = 0): number {
    const maximumTypeDepth = 128;
    if (depth >= maximumTypeDepth) { return start + 1; }
    const token = tokens[start]?.text;
    if (token === 'array') { return typeReference(start + 1, depth + 1); }
    if (token === 'map' && tokens[start + 1]?.text === '(') {
      const next = typeReference(start + 2, depth + 1);
      return tokens[next]?.text === ')' ? typeReference(next + 1, depth + 1) : next;
    }
    if (token === 'union' && tokens[start + 1]?.text === '(') {
      let next = start + 2;
      while (next < tokens.length && tokens[next].text !== ')') {
        next = typeReference(next, depth + 1);
        while (next < tokens.length && ![',', ')', ';', '}'].includes(tokens[next].text)) { ++next; }
        if (tokens[next]?.text !== ',') { break; }
        ++next;
      }
      return next + 1;
    }
    const name = qualified(start);
    if (!name) { return start + 1; }
    result.references.push({ ...name, scope: [...currentScope()] });
    return name.next;
  }

  /** Index only the enum type prefix of a field default, excluding its value and quoted text. */
  function defaultReference(member: number): void {
    if (!/^[A-Za-z_]\w*$/.test(tokens[member]?.text ?? '')) { return; }
    for (let next = member + 1; next < tokens.length; ++next) {
      const token = tokens[next].text;
      if ([';', '}', 'public', 'private', 'protected'].includes(token)) { return; }
      if (token !== '{') { continue; }
      const value = qualified(next + 1);
      if (value && tokens[value.next]?.text === '}') {
        const separator = value.next - 2;
        if (separator > next + 1 && tokens[separator].text === '::') {
          result.references.push({ start: value.start, end: tokens[separator - 1].end,
            name: value.name.slice(0, value.name.lastIndexOf('::')),
            scope: [...currentScope()], kind: 'enum' });
        }
      }
      return;
    }
  }

  for (let i = 0; i < tokens.length; ++i) {
    const token = tokens[i];
    if (token.text === '{') {
      scopes.push(namespaceBodies.get(i) ?? currentScope());
    } else if (token.text === '}') {
      if (scopes.length > 1) { scopes.pop(); }
    } else if (token.text === 'namespace') {
      const name = qualified(i + 1);
      if (name && tokens[name.next]?.text === '{') {
        namespaceBodies.set(name.next, [...currentScope(), ...name.name.split('::')]);
      }
    } else if (schema && token.text === 'include' && scopes.length === 1) {
      let end = i + 1;
      while (end < tokens.length && ![';', '{', '}'].includes(tokens[end].text)) { ++end; }
      // Whitespace/comments are legal around the directive, never inside the path.
      const first = tokens[i + 1];
      const last = tokens[end - 1];
      const name = schemaIncludeName(first && last ? text.slice(first.start, last.end) : '');
      if (tokens[end]?.text === ';' && name !== undefined) {
        result.includes.push({ name, start: first.start, end: last.end });
      }
      i = end;
    } else if (schema && ['public', 'private', 'protected'].includes(token.text)) {
      defaultReference(typeReference(i + 1));
    } else if (token.text === 'class' || token.text === 'struct' || token.text === 'enum') {
      if (tokens[i - 1]?.text === 'enum') { continue; }
      const nameIndex = token.text === 'enum' && ['class', 'struct'].includes(tokens[i + 1]?.text)
        ? i + 2 : i + 1;
      const name = tokens[nameIndex];
      if (!name || !/^[A-Za-z_]\w*$/.test(name.text)) { continue; }
      let body = nameIndex + 1;
      // Generated storage-mode specializations retain the same public schema type name.
      if (!schema && tokens[body]?.text === '<') {
        let depth = 0;
        do {
          if (tokens[body].text === '<') { ++depth; }
          if (tokens[body].text === '>') { --depth; }
          ++body;
        } while (body < tokens.length && depth > 0);
      }
      // Keep primary-template forward declarations for C++ service results, but not parameters.
      if (!schema && !['{', ':', 'final', ';'].includes(tokens[body]?.text)) { continue; }
      while (body < tokens.length && !['{', ';', '}'].includes(tokens[body].text)) { ++body; }
      if (tokens[body]?.text !== '{' && (schema || tokens[body]?.text !== ';')) { continue; }
      const symbol: TypeSymbol = { name: name.text,
        qualified: [...currentScope(), name.text].join('::'),
        kind: token.text === 'enum' ? 'enum' : 'class', start: name.start, end: name.end,
        body: tokens[body].start, declaration: token.start, definition: tokens[body].text === '{' };
      result.symbols.push(symbol);
      if (schema) {
        result.references.push({ start: name.start, end: name.end,
          name: symbol.qualified, scope: [] });
      }
    }
  }
  return result;
}

/** Resolve the nearest declared namespace first, including globally qualified names. */
export function resolveType(reference: SourceIndex['references'][number], symbols: TypeSymbol[]): TypeSymbol[] {
  const scope = reference.name.startsWith('::') ? [] : [...reference.scope];
  const name = reference.name.replace(/^::/, '');
  for (;;) {
    const qualified = [...scope, name].join('::');
    const matches = symbols.filter(symbol => symbol.qualified === qualified);
    if (matches.length || !scope.length) {
      return matches.filter(symbol => !reference.kind || symbol.kind === reference.kind);
    }
    scope.pop();
  }
}

/** Reproduce the compiler's acronym/underscore boundaries for public C++ type names. */
function words(name: string): string[] {
  return name.replace(/([a-z0-9])([A-Z])/g, '$1_$2').replace(/([A-Z])([A-Z][a-z])/g, '$1_$2')
    .split('_').filter(Boolean).map(word => word.toLowerCase());
}

/** Match only actual preserved, snake-case or PascalCase output spellings and namespaces. */
export function generatedNames(qualified: string): string[] {
  const parts = qualified.split('::');
  const name = parts.pop()!;
  const namespaces = parts.map(part => words(part).join('_'));
  const typeWords = words(name);
  return [...new Set([qualified,
    [...namespaces, typeWords.join('_')].join('::'),
    [...namespaces, typeWords.map(word => word[0].toUpperCase() + word.slice(1)).join('')].join('::')])];
}

/** Decode Serializer's Make depfiles, including escaped drive colons, spaces, # and $. */
export function dependencySchemas(text: string, header: string): string[] {
  const sides: string[][] = [[], []];
  let side = 0;
  let word = '';
  /** Flush the current escaped path token. */
  function flush(): void { if (word) { sides[side].push(word); word = ''; } }
  for (let i = 0; i < text.length; ++i) {
    const ch = text[i];
    if (ch === '\\' && i + 1 < text.length) {
      if (text[i + 1] === '\r' || text[i + 1] === '\n') {
        ++i;
        if (text[i] === '\r' && text[i + 1] === '\n') { ++i; }
      } else { word += text[++i]; }
    } else if (ch === '$' && text[i + 1] === '$') {
      word += '$'; ++i;
    } else if (ch === ':' && side === 0) {
      flush(); side = 1;
    } else if (/\s/.test(ch)) {
      flush();
    } else if (ch === '#') {
      while (i < text.length && text[i] !== '\n') { ++i; }
      flush();
    } else { word += ch; }
  }
  flush();
  const equalPath = (left: string, right: string): boolean => process.platform === 'win32'
    ? path.normalize(left).toLowerCase() === path.normalize(right).toLowerCase()
    : path.normalize(left) === path.normalize(right);
  if (!sides[0].some(target => path.isAbsolute(target) && equalPath(target, header))) { return []; }
  return sides[1].filter(file => path.isAbsolute(file) && file.endsWith('.serializer')).map(file => path.normalize(file));
}

/** Restrict include navigation to the literal path under the cursor, excluding comments. */
export function cppIncludeAt(text: string, offset: number): (SchemaInclude & { quoted: boolean }) | undefined {
  const start = text.lastIndexOf('\n', offset - 1) + 1;
  const line = text.slice(start, text.indexOf('\n', start) < 0 ? text.length : text.indexOf('\n', start));
  const match = /^\s*#\s*include\s*(?:"([^"\r\n]+)"|<([^>\r\n]+)>)/.exec(line);
  if (!match) { return undefined; }
  const hash = start + line.indexOf('#');
  if (codeOnly(text).charAt(hash) !== '#') { return undefined; }
  const name = match[1] ?? match[2];
  const pathStart = start + match[0].indexOf(match[1] ? '"' : '<') + 1;
  return offset >= pathStart && offset < pathStart + name.length
    ? { name, start: pathStart, end: pathStart + name.length, quoted: !!match[1] } : undefined;
}
