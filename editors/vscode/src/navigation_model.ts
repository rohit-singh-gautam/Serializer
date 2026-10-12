import * as path from 'node:path';

export interface Span { start: number; end: number }

/** Accept editor carets at a selection's end, preferring a token beginning there. */
export function spanAt<T extends Span>(spans: T[], offset: number): T | undefined {
  return spans.find(span => offset >= span.start && offset < span.end)
    ?? spans.find(span => offset === span.end);
}
interface Token extends Span { text: string }
export interface SchemaInclude extends Span { name: string }
export interface TypeSymbol extends Span {
  name: string;
  qualified: string;
  kind: 'class' | 'enum';
  body: number;
  declaration: number;
  definition: boolean;
  schemaQualified?: string;
  genericSource?: Span & { name: string; scope: string[] };
  typeParameter?: boolean;
  bases?: Array<{ name: string; scope: string[] }>;
}
export interface SourceIndex {
  symbols: TypeSymbol[];
  includes: SchemaInclude[];
  references: Array<Span & { name: string; scope: string[]; kind?: TypeSymbol['kind'] }>;
  members?: Array<Span & { name: string; owner: string; type?: { name: string; scope: string[] }; wireOnly?: boolean; getter?: boolean }>;
  memberReferences?: Array<Span & { name: string; owner: { name: string; scope: string[] }; path: string[] }>;
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
  // Numeric separators must not open literals and hide later declarations or namespace braces.
  // Include character prefixes so their contents remain masked with the same source offsets.
  return text.replace(/\/\*[\s\S]*?(?:\*\/|$)|\/\/[^\r\n]*|(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\1"|"(?:\\[\s\S]|[^"\\])*"|(?<![A-Za-z_0-9])(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\])*'/g,
    match => match.replace(/[^\r\n]/g, ' '));
}

/** Read declarations and schema type positions, ignoring comments, literals and member names. */
export function indexSource(text: string, schema: boolean, classScopes = false): SourceIndex {
  const code = codeOnly(text);
  const language = code.match(/\bserializer\s+version\s+(\d+)(?:\.(\d+)\.(\d+))?\s*;/);
  const commonTypeContract = language !== null && (Number(language[1]) > 1 ||
    (Number(language[1]) === 1 && Number(language[2] ?? 0) >= 12));
  const tokens: Token[] = [...code.matchAll(/[A-Za-z_][A-Za-z_0-9]*|::|[^\s]/g)]
    .map(match => ({ text: match[0], start: match.index!, end: match.index! + match[0].length }));
  const result: SourceIndex = { symbols: [], includes: [], references: [], members: [], memberReferences: [] };
  const scopes: string[][] = [[]];
  const namespaceBodies = new Map<number, string[]>();
  let referenceScope: string[] | undefined;
  let functionScope: { scope: string[]; end: number } | undefined;
  const currentScope = (): string[] => referenceScope ?? functionScope?.scope ?? scopes[scopes.length - 1];

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
    if (token === 'layout' && tokens[start + 1]?.text === '(') {
      let next = start + 2;
      while (next < tokens.length && ![')', ';', '}'].includes(tokens[next].text)) { ++next; }
      return tokens[next]?.text === ')' ? typeReference(next + 1, depth + 1) : next;
    }
    if (token === 'digest' && ['(', '['].includes(tokens[start + 1]?.text ?? '')) {
      // Algorithm names and manual byte extents are metadata, not schema type references.
      const close = tokens[start + 1].text === '(' ? ')' : ']';
      let next = start + 2;
      while (next < tokens.length && ![close, ';', '}'].includes(tokens[next].text)) { ++next; }
      return tokens[next]?.text === close ? next + 1 : next;
    }
    if ((token === 'compact_prefix' || token === 'compact_varint') && tokens[start + 1]?.text !== '::') {
      const policy = tokens[start + 1]?.text;
      const operand = start + (policy === 'strict' || policy === 'lenient' ? 2 : 1);
      const name = qualified(operand);
      const metadata = name && tokens[name.next]?.text === 'omit' &&
        tokens[name.next + 1]?.text === '(' && /^[A-Za-z_]\w*$/.test(tokens[name.next + 2]?.text ?? '');
      // Older schemas may declare a type with an encoding keyword's spelling.
      if (name && !metadata && /^[A-Za-z_]\w*$/.test(tokens[name.next]?.text ?? '')) {
        return typeReference(operand, depth + 1);
      }
    }
    if (token === 'managed' || token === 'transient' || token === 'wire_only') { return typeReference(start + 1, depth + 1); }
    if ((['optional', 'array', 'map'].includes(token) || (token === 'common_type' && commonTypeContract)) &&
        tokens[start + 1]?.text === '<') {
      // Only user-declared constructors have destinations; retain references for compatible schemas.
      if (token !== 'common_type') {
        result.references.push({ ...qualified(start)!, scope: [...currentScope()] });
      }
      let next = start + 2;
      while (next < tokens.length && !['>', ';', '}'].includes(tokens[next].text)) {
        next = typeReference(next, depth + 1);
        if (tokens[next]?.text === ',') { ++next; }
      }
      return tokens[next]?.text === '>' ? next + 1 : next;
    }
    if (token === 'array') {
      let next = start + 1;
      if (tokens[next]?.text === '[') {
        ++next;
        while (next < tokens.length && ![']', ';', '}'].includes(tokens[next].text)) {
          const operand = qualified(next);
          if (operand) { result.references.push({ ...operand, scope: [...currentScope()] }); }
          next = operand?.next ?? next + 1;
        }
        if (tokens[next]?.text === ']') { ++next; }
      }
      return typeReference(next, depth + 1);
    }
    if (token === 'map' && tokens[start + 1]?.text === '(') {
      const next = typeReference(start + 2, depth + 1);
      return tokens[next]?.text === ')' ? typeReference(next + 1, depth + 1) : next;
    }
    if ((token === 'union' || token === 'variant') && tokens[start + 1]?.text === '(') {
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
    let next = name.next;
    if (tokens[next]?.text === '<') {
      ++next;
      while (next < tokens.length && !['>', ';', '}'].includes(tokens[next].text)) {
        next = typeReference(next, depth + 1);
        if (tokens[next]?.text === ',') { ++next; }
      }
      if (tokens[next]?.text === '>') { ++next; }
    }
    return next;
  }

  /** Skip factory-contract metadata while preserving actual type operands in constraints. */
  function constraintReference(start: number): number {
    if (tokens[start]?.text === 'factory' && tokens[start + 1]?.text === '(') {
      let next = start + 2;
      while (next < tokens.length && ![')', ';', '}'].includes(tokens[next].text)) { ++next; }
      return tokens[next]?.text === ')' ? next + 1 : next;
    }
    if (['stateless', 'integral', 'floating_point'].includes(tokens[start]?.text)) { return start + 1; }
    return typeReference(start);
  }

  /** Declare independent input parameters in a scope shared by their signature and body. */
  function inputParameters(start: number, scope: string[]): number {
    let next = start;
    let depth = 1;
    let parameterStart = true;
    let inConstraint = false;
    while (++next < tokens.length && depth) {
      const current = tokens[next].text;
      if (current === '<') { ++depth; }
      if (current === '>') { --depth; }
      if (depth === 1 && current === ',') { parameterStart = true; inConstraint = false; }
      if (depth === 1 && current === ':') { inConstraint = true; }
      if (depth === 1 && parameterStart && /^[A-Za-z_]\w*$/.test(current)) {
        const parameter = tokens[next], qualifiedName = [...scope, current].join('::');
        result.symbols.push({ ...parameter, name: current, qualified: qualifiedName, kind: 'class',
          body: parameter.start, declaration: parameter.start, definition: true, typeParameter: true });
        result.references.push({ ...parameter, name: qualifiedName, scope: [] });
        parameterStart = false;
      } else if (depth === 1 && inConstraint && /^[A-Za-z_]\w*$/.test(current)) {
        next = constraintReference(next) - 1;
      }
    }
    return next;
  }

  /** Index generic readonly signatures without exposing method names or native implementations. */
  function functionSignature(start: number): void {
    let next = start + 1;
    if (['virtual', 'abstract', 'override'].includes(tokens[next]?.text)) { ++next; }
    const returnStart = result.references.length;
    next = typeReference(next);
    if (tokens[next + 1]?.text === '<') {
      const scope = [...currentScope(), `$function_${tokens[start].start}`];
      referenceScope = scope;
      // The return type precedes the declarations of its independent input parameters.
      for (const reference of result.references.slice(returnStart)) { reference.scope = [...scope]; }
      next = inputParameters(next + 1, scope);
      let end = next, depth = 0;
      while (end < tokens.length) {
        const current = tokens[end].text;
        if (current === '{') { ++depth; }
        if (current === '}') { --depth; }
        if (current === ';' && depth === 0) { break; }
        ++end;
      }
      functionScope = { scope, end };
    } else {
      ++next;
    }
    if (tokens[next]?.text === '(') {
      ++next;
      while (next < tokens.length && ![')', ';', '}'].includes(tokens[next].text)) {
        next = typeReference(next) + 1;
        if (tokens[next]?.text === ',') { ++next; }
      }
    }
    referenceScope = undefined;
  }

  /** Index constructor signatures independently of an access token or an opaque C++ prefix. */
  function constructionSignature(start: number): number {
    let next = start + (tokens[start].text === 'constructor' ? 2 : 3);
    if (tokens[next - 1]?.text === '<') {
      const localScope = [...currentScope(), `$construction_${tokens[start].start}`];
      referenceScope = localScope;
      next = inputParameters(next - 1, localScope);
      if (tokens[next]?.text === '(') { ++next; }
    }
    while (next < tokens.length && ![')', ';', '}'].includes(tokens[next].text)) {
      next = typeReference(next) + 1;
      if (tokens[next]?.text === '=') {
        // Nested aggregate defaults own their braces; they must not close the surrounding class.
        const groups: string[] = [];
        ++next;
        while (next < tokens.length) {
          const current = tokens[next].text;
          if (!groups.length && [',', ')', ';', '}'].includes(current)) { break; }
          if (['(', '[', '{', '<'].includes(current)) { groups.push(current); }
          else if ([')', ']', '}', '>'].includes(current)) { groups.pop(); }
          ++next;
        }
      }
      if (tokens[next]?.text === ',') { ++next; }
    }
    if (referenceScope) {
      let body = next + 1;
      if (tokens[body]?.text === 'explicit') { ++body; }
      if (tokens[body]?.text === '{') { namespaceBodies.set(body, referenceScope); }
      referenceScope = undefined;
    }
    return next;
  }

  /** Index constructor/checked operands and enum types within a balanced portable expression. */
  function expressionReferences(open: number): number {
    let depth = 1;
    let next = open + 1;
    for (; next < tokens.length && depth; ++next) {
      if (tokens[next].text === '(') { ++depth; }
      if (tokens[next].text === ')') { --depth; }
      if (tokens[next].text === 'checked' && tokens[next + 1]?.text === '<') {
        next = typeReference(next + 2);
        if (tokens[next]?.text === '>') { ++next; }
        --next;
        continue;
      }
      const value = qualified(next);
      let application = value?.next ?? next;
      if (value && tokens[application]?.text === '<') {
        let argumentsDepth = 1;
        while (++application < tokens.length && argumentsDepth &&
            ![';', '{', '}'].includes(tokens[application].text)) {
          if (tokens[application].text === '<') { ++argumentsDepth; }
          if (tokens[application].text === '>') { --argumentsDepth; }
        }
      }
      if (value && tokens[next - 1]?.text !== '.' && tokens[application]?.text === '(') {
        // Ordinary functions without a declared type retain their language-service destination.
        next = typeReference(next) - 1;
        continue;
      }
      if (value && value.name.includes('::')) {
        const separator = value.next - 2;
        result.references.push({ start: value.start, end: tokens[separator - 1].end,
          name: value.name.slice(0, value.name.lastIndexOf('::')),
          scope: [...currentScope()], kind: 'enum' });
        next = value.next - 1;
      }
    }
    return next - 1;
  }

  /** Index enum type prefixes in field and magic defaults, excluding values and quoted text. */
  function defaultReference(member: number, metadata = false): void {
    if (!metadata && !/^[A-Za-z_]\w*$/.test(tokens[member]?.text ?? '')) { return; }
    for (let next = metadata ? member : member + 1; next < tokens.length; ++next) {
      const token = tokens[next].text;
      if ([';', '}', 'public', 'private', 'protected'].includes(token)) { return; }
      if (token !== '{') { continue; }
      let depth = 1;
      for (let entry = next + 1; entry < tokens.length && depth;) {
        if (tokens[entry].text === '{') { ++depth; }
        if (tokens[entry].text === '}') { --depth; }
        const value = qualified(entry);
        if (value && [',', '}'].includes(tokens[value.next]?.text)) {
          const separator = value.next - 2;
          if (separator > entry && tokens[separator].text === '::') {
            result.references.push({ start: value.start, end: tokens[separator - 1].end,
              name: value.name.slice(0, value.name.lastIndexOf('::')),
              scope: [...currentScope()], kind: 'enum' });
          }
        }
        entry = value?.next ?? entry + 1;
      }
      return;
    }
  }

  for (let i = 0; i < tokens.length; ++i) {
    if (functionScope && i >= functionScope.end) { functionScope = undefined; }
    const token = tokens[i];
    if (schema && ['cpp', 'csharp', 'java', 'javascript', 'typescript', 'go', 'rust',
      'python', 'swift', 'kotlin', 'c'].includes(token.text)
      && (tokens[i + 1]?.text === '{' || tokens[i + 1]?.text === 'preamble' ||
          (token.text === 'cpp' && tokens[i + 1]?.text === 'storage' && tokens[i + 2]?.text === '('))) {
      // Native blocks are opaque; their declarations are owned by the language service.
      let start = i + 1;
      if (tokens[start]?.text === 'preamble') { ++start; }
      if (tokens[start]?.text === 'storage' && tokens[start + 1]?.text === '(') {
        start += 2;
        while (start < tokens.length && ![')', '{', ';', '}'].includes(tokens[start].text)) { ++start; }
        if (tokens[start]?.text === ')') { ++start; }
      }
      if (tokens[start]?.text === '{') {
        let depth = 1;
        i = start;
        while (++i < tokens.length && depth) {
          if (tokens[i].text === '{') { ++depth; }
          if (tokens[i].text === '}') { --depth; }
        }
        --i;
      }
    } else if (schema && ['expression', 'predicate'].includes(token.text) &&
               tokens[i + 1]?.text === '(') {
      i = expressionReferences(i + 1);
    } else if (schema && token.text === 'checked' && tokens[i + 1]?.text === '<') {
      i = typeReference(i + 2);
    } else if (schema && token.text === 'expose' && tokens[i + 1]?.text === 'readonly') {
      const getter = tokens[i + 5]?.text === 'as' ? tokens[i + 6] : undefined;
      const field = result.members![result.members!.length - 1];
      if (getter && field && /^[A-Za-z_]\w*$/.test(getter.text)) {
        result.members!.push({ ...getter, name: getter.text, owner: field.owner,
          type: field.type, getter: true });
        i += 6;
      }
    } else if (schema && token.text === 'binary' && tokens[i + 1]?.text === '(' &&
        tokens[i + 3]?.text === ',') {
      // Arithmetic binary mappings explicitly declare their right operand's type.
      i = typeReference(i + 4);
    } else if (schema && token.text === 'constructor' &&
        ['(', '<'].includes(tokens[i + 1]?.text ?? '')) {
      i = constructionSignature(i);
    } else if (token.text === '{') {
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
    } else if (!schema && token.text === 'using' && tokens[i + 2]?.text === '=') {
      const name = tokens[i + 1];
      result.symbols.push({ ...name, name: name.text, qualified: [...currentScope(), name.text].join('::'),
        kind: 'class', body: name.start, declaration: token.start, definition: true });
    } else if (schema && token.text === 'instantiate') {
      const name = tokens[i + 1];
      if (name && tokens[i + 2]?.text === '=') {
        const qualifiedName = [...currentScope(), name.text].join('::');
        const family = qualified(i + 3);
        result.symbols.push({ ...name, name: name.text, qualified: qualifiedName, kind: 'class',
          body: name.start, declaration: token.start, definition: true,
          genericSource: family ? { start: family.start, end: family.end,
            name: family.name, scope: [...currentScope()] } : undefined });
        result.references.push({ ...name, name: qualifiedName, scope: [] });
        typeReference(i + 3);
      }
    } else if (schema && token.text === 'function') {
      // Namespace functions expose the same signature type references as class methods.
      functionSignature(i);
    } else if (schema && ['public', 'private', 'protected'].includes(token.text)) {
      // Typed magic can reference an enum; legacy magic and payload versions have no type operand.
      if (tokens[i + 1]?.text === 'conversion') {
        const target = qualified(i + 2);
        let next = typeReference(i + 2);
        if (target && tokens[next]?.text === 'as' && tokens[next + 2]?.text === '{') {
          const targetOwner = { name: target.name, scope: [...currentScope()] };
          const sourceOwner = { name: currentScope().join('::'), scope: [] };
          next += 3;
          while (next < tokens.length && !['}', '{'].includes(tokens[next].text)) {
            const destination = tokens[next];
            if (/^[A-Za-z_]\w*$/.test(destination.text) && tokens[next + 1]?.text === '=') {
              result.memberReferences!.push({ ...destination, name: destination.text, owner: targetOwner, path: [] });
              next += 2;
              const projection: string[] = [];
              while (/^[A-Za-z_]\w*$/.test(tokens[next]?.text ?? '')) {
                const component = tokens[next];
                result.memberReferences!.push({ ...component, name: component.text, owner: sourceOwner, path: [...projection] });
                projection.push(component.text);
                ++next;
                if (tokens[next]?.text !== '.') { break; }
                ++next;
              }
            }
            while (next < tokens.length && ![';', '}', '{'].includes(tokens[next].text)) { ++next; }
            if (tokens[next]?.text === ';') { ++next; }
          }
          // The conversion body is its own grammar; do not mistake its target names for declarations.
          i = next;
        }
      } else if ((tokens[i + 1]?.text === 'constructor' && ['(', '<'].includes(tokens[i + 2]?.text ?? '')) ||
          (tokens[i + 1]?.text === 'factory' && ['(', '<'].includes(tokens[i + 3]?.text ?? ''))) {
        i = constructionSignature(i + 1);
      } else if (tokens[i + 1]?.text === 'function') {
        functionSignature(i + 1);
        // The access-prefixed method signature was indexed here; skip its keyword.
        ++i;
      } else if (['cpp', 'csharp', 'java', 'javascript', 'typescript', 'go', 'rust',
        'python', 'swift', 'kotlin', 'c'].includes(tokens[i + 1]?.text)) {
        // Native blocks are skipped by the next iteration.
      } else if (tokens[i + 1]?.text === 'magic') {
        if (qualified(i + 2)) { defaultReference(typeReference(i + 2), true); }
      } else if (tokens[i + 1]?.text !== 'version') {
        const referenceStart = result.references.length;
        const next = typeReference(i + 1);
        const field = tokens[next];
        if (field && /^[A-Za-z_]\w*$/.test(field.text)) {
          result.members!.push({ start: field.start, end: field.end, name: field.text,
            owner: currentScope().join('::'),
            type: result.references[referenceStart] ? { name: result.references[referenceStart].name,
              scope: [...currentScope()] } : undefined,
            wireOnly: tokens[i + 1]?.text === 'wire_only' || undefined });
        }
        defaultReference(next);
      }
    } else if (token.text === 'class' || token.text === 'struct' || token.text === 'enum') {
      if (tokens[i - 1]?.text === 'enum') { continue; }
      const nameIndex = token.text === 'enum' && ['class', 'struct'].includes(tokens[i + 1]?.text)
        ? i + 2 : i + 1;
      const name = tokens[nameIndex];
      if (!name || !/^[A-Za-z_]\w*$/.test(name.text)) { continue; }
      let body = nameIndex + 1;
      // Generated storage-mode specializations retain the same public schema type name.
      const parameters: Token[] = [];
      if (tokens[body]?.text === '<') {
        let depth = 0;
        let parameterStart = true;
        let inDefault = false;
        let inConstraint = false;
        do {
          if (tokens[body].text === '<') { ++depth; }
          if (tokens[body].text === '>') { --depth; }
          if (schema) {
            const current = tokens[body].text;
            if (depth === 1 && current === ',') { parameterStart = true; inDefault = false; inConstraint = false; }
            if (depth === 1 && current === ':') { inConstraint = true; }
            if (depth === 1 && current === '=') { inDefault = true; inConstraint = false; }
            if (depth === 1 && parameterStart && /^[A-Za-z_]\w*$/.test(current)) {
              if (current !== 'uint64') { parameters.push(tokens[body]); parameterStart = false; }
            } else if (inConstraint && /^[A-Za-z_]\w*$/.test(current)) {
              // Exact type alternatives retain declaration navigation inside constraints.
              body = constraintReference(body) - 1;
            } else if (inDefault && current === 'digest' &&
                       ['(', '['].includes(tokens[body + 1]?.text ?? '')) {
              // Digest defaults may name an algorithm that also exists as an unrelated schema type.
              body = typeReference(body) - 1;
            } else if (inDefault && /^[A-Za-z_]\w*$/.test(current)) {
              const reference = qualified(body)!;
              result.references.push({ ...reference, scope: [...currentScope(), name.text] });
              body = reference.next - 1;
            }
          }
          ++body;
        } while (body < tokens.length && depth > 0);
      }
      // Keep primary-template forward declarations for C++ service results, but not parameters.
      if (!schema && !['{', ':', 'final', ';'].includes(tokens[body]?.text)) { continue; }
      const header = body;
      const bases: Array<{ name: string; scope: string[] }> = [];
      while (body < tokens.length && !['{', ';', '}'].includes(tokens[body].text)) { ++body; }
      if (tokens[body]?.text !== '{' && (schema || tokens[body]?.text !== ';')) { continue; }
      if (schema) {
        referenceScope = [...currentScope(), name.text];
        for (let next = header; next < body; ++next) {
          if (['public', 'private', 'protected'].includes(tokens[next].text)) {
            const referenceStart = result.references.length;
            next = typeReference(next + 1) - 1;
            const base = result.references[referenceStart];
            if (base) { bases.push({ name: base.name, scope: [...base.scope] }); }
          } else if (tokens[next].text === 'binary' && tokens[next + 1]?.text === '(' &&
              tokens[next + 3]?.text === ',') {
            next = typeReference(next + 4) - 1;
          } else if (tokens[next].text === 'expression' && tokens[next + 1]?.text === '(') {
            next = expressionReferences(next + 1);
          }
        }
        referenceScope = undefined;
      }
      if (schema && parameters.length) {
        const scope = [...currentScope(), name.text];
        namespaceBodies.set(body, scope);
        for (const parameter of parameters) {
          const qualifiedName = [...scope, parameter.text].join('::');
          result.symbols.push({ ...parameter, name: parameter.text, qualified: qualifiedName, kind: 'class',
            body: parameter.start, declaration: parameter.start, definition: true, typeParameter: true });
          result.references.push({ ...parameter, name: qualifiedName, scope: [] });
        }
      }
      const symbol: TypeSymbol = { name: name.text,
        qualified: [...currentScope(), name.text].join('::'),
        kind: token.text === 'enum' ? 'enum' : 'class', start: name.start, end: name.end,
        body: tokens[body].start, declaration: token.start, definition: tokens[body].text === '{',
        bases: bases.length ? bases : undefined };
      result.symbols.push(symbol);
      if ((classScopes || schema) && symbol.definition) {
        namespaceBodies.set(body, [...currentScope(), name.text]);
      }
      if (schema) {
        // Recognize the keyword as well as the name, including reversed `class name` selections.
        // Keep separate spans so comments between them never become navigation references.
        result.references.push({ start: token.start, end: token.end,
          name: symbol.qualified, scope: [] });
        result.references.push({ start: name.start, end: name.end,
          name: symbol.qualified, scope: [] });
        i = body - 1;
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
export function dependencyFiles(text: string): { targets: string[]; schemas: string[] } {
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
  return { targets: sides[0].filter(file => path.isAbsolute(file)).map(file => path.normalize(file)),
    schemas: sides[1].filter(file => path.isAbsolute(file) && file.endsWith('.serializer')).map(file => path.normalize(file)) };
}

/** Require exact depfile target ownership before using its ordered schema dependencies. */
export function dependencySchemas(text: string, header: string): string[] {
  const files = dependencyFiles(text);
  const equalPath = (left: string, right: string): boolean => process.platform === 'win32'
    ? path.normalize(left).toLowerCase() === path.normalize(right).toLowerCase()
    : path.normalize(left) === path.normalize(right);
  return files.targets.some(target => equalPath(target, header)) ? files.schemas : [];
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
  return offset >= pathStart && offset <= pathStart + name.length
    ? { name, start: pathStart, end: pathStart + name.length, quoted: !!match[1] } : undefined;
}

/** Map generated managed companions back to their original schema declaration. */
export function managedNames(qualified: string): string[] {
  const separator = qualified.lastIndexOf('::');
  const scope = separator < 0 ? '' : qualified.slice(0, separator + 2);
  const name = qualified.slice(separator + 2);
  const leaf = separator < 0 ? qualified : name;
  return [`managed_${leaf}_data`, `managed_${leaf}_storage`, `${leaf}_editor`]
    .flatMap(companion => generatedNames(scope + companion));
}
