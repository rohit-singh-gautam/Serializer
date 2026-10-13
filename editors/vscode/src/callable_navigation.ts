import { CallableSymbol, codeOnly, Span } from './navigation_model';

interface Token extends Span { text: string }
export interface CppCallable extends Span {
  name: string;
  owner: string;
  ownerArguments: string[];
  inputs: string[];
  parameters: string[];
  readonly: boolean;
}
const maximumSignatureTokens = 4096;
const maximumSignatureDepth = 128;

/** Find a balanced signature group, rejecting malformed or excessively nested source. */
function closing(tokens: Token[], start: number, open: string, close: string): number | undefined {
  let depth = 0;
  for (let next = start; next < Math.min(tokens.length, start + maximumSignatureTokens); ++next) {
    if (tokens[next].text === open && ++depth > maximumSignatureDepth) { return undefined; }
    if (tokens[next].text === close && --depth === 0) { return next; }
  }
  return undefined;
}

/** Split type/parameter lists only at their outer commas, preserving nested templates and defaults. */
function separated(tokens: Token[]): Token[][] {
  const result: Token[][] = [[]];
  const groups: string[] = [];
  for (const token of tokens) {
    if (token.text === ',' && !groups.length) { result.push([]); continue; }
    result[result.length - 1].push(token);
    if (['(', '[', '{', '<'].includes(token.text)) { groups.push(token.text); }
    else if ([')', ']', '}', '>'].includes(token.text)) { groups.pop(); }
  }
  return result.filter(group => group.length);
}

/** Remove a parameter's name and default without treating a qualified type's leaf as a variable. */
function parameterType(tokens: Token[]): string {
  let depth = 0;
  const end = tokens.findIndex(token => {
    if (['(', '[', '{', '<'].includes(token.text)) { ++depth; }
    else if ([')', ']', '}', '>'].includes(token.text)) { --depth; }
    return token.text === '=' && depth === 0;
  });
  const type = tokens.slice(0, end < 0 ? undefined : end);
  const last = type[type.length - 1];
  if (type.length > 1 && /^[A-Za-z_]\w*$/.test(last?.text ?? '') &&
      type[type.length - 2].text !== '::' &&
      !['const', 'volatile', 'void', 'bool', 'char', 'float', 'double', 'int', 'long', 'short', 'signed', 'unsigned'].includes(last.text)) {
    type.pop();
  }
  return type.map(token => token.text).join(' ');
}

/** Index authored out-of-class C++ definitions; calls, declarations, comments and literals are excluded. */
export function cppCallables(text: string): CppCallable[] {
  const tokens: Token[] = [...codeOnly(text).matchAll(/[A-Za-z_]\w*|::|->|[^\s]/g)]
    .map(match => ({ text: match[0], start: match.index!, end: match.index! + match[0].length }));
  const result: CppCallable[] = [];
  const scopes: string[][] = [[]];
  const bodies = new Map<number, string[]>();
  let inputs: string[] = [];
  for (let index = 0; index < tokens.length; ++index) {
    const token = tokens[index];
    if (token.text === 'namespace') {
      let end = index + 1;
      const names: string[] = [];
      while (end < tokens.length && (tokens[end].text === '::' || /^[A-Za-z_]\w*$/.test(tokens[end].text))) {
        if (tokens[end].text !== '::') { names.push(tokens[end].text); }
        ++end;
      }
      if (tokens[end]?.text === '{') { bodies.set(end, [...scopes[scopes.length - 1], ...names]); }
    } else if (token.text === '{') {
      scopes.push(bodies.get(index) ?? scopes[scopes.length - 1]);
    } else if (token.text === '}') {
      if (scopes.length > 1) { scopes.pop(); }
    } else if (token.text === 'template' && tokens[index + 1]?.text === '<') {
      const end = closing(tokens, index + 1, '<', '>');
      if (end !== undefined) {
        inputs = separated(tokens.slice(index + 2, end)).map(group => {
          const assignment = group.findIndex(item => item.text === '=');
          return (assignment < 0 ? group : group.slice(0, assignment)).filter(item =>
            /^[A-Za-z_]\w*$/.test(item.text)).at(-1)?.text ?? '';
        }).filter(Boolean);
        index = end;
      }
    } else if (/^[A-Za-z_]\w*$/.test(token.text) && tokens[index + 1]?.text === '(' &&
        tokens[index - 1]?.text === '::') {
      // Walk the explicitly qualified owner backwards, discarding only its template argument lists.
      const components: string[] = [];
      let ownerArguments: string[] = [];
      let owner = index - 2;
      let ownerStart = owner;
      while (owner >= 0) {
        if (tokens[owner].text === '>') {
          let depth = 1, end = owner--;
          while (owner >= 0 && depth && end - owner < maximumSignatureTokens) {
            if (tokens[owner].text === '>') { ++depth; }
            if (tokens[owner].text === '<') { --depth; }
            if (depth) { --owner; }
          }
          if (depth) { break; }
          if (!components.length) {
            ownerArguments = separated(tokens.slice(owner + 1, end)).map(group => group.map(item => item.text).join(' '));
          }
          --owner;
        }
        if (!/^[A-Za-z_]\w*$/.test(tokens[owner]?.text ?? '')) { break; }
        components.unshift(tokens[owner].text);
        ownerStart = owner;
        if (tokens[owner - 1]?.text !== '::') { break; }
        // A global qualifier can follow a scalar/dependent return type without joining that type to the owner.
        if (['void', 'bool', 'char', 'float', 'double', 'int', 'long', 'short', 'signed', 'unsigned', 'auto'].includes(tokens[owner - 2]?.text) ||
            inputs.includes(tokens[owner - 2]?.text)) { break; }
        owner -= 2;
      }
      if (!components.length) { continue; }
      const close = closing(tokens, index + 1, '(', ')');
      if (close === undefined) { continue; }
      let body = close + 1;
      let readonly = false;
      let constraint = false;
      let requirementClause = false;
      while (body < Math.min(tokens.length, close + maximumSignatureTokens)) {
        const suffix = tokens[body].text;
        if ([';', '=', '}'].includes(suffix)) { break; }
        if (suffix === 'requires') {
          if (requirementClause) {
            let expression = body + 1;
            if (tokens[expression]?.text === '(') {
              const end = closing(tokens, expression, '(', ')');
              if (end === undefined) { break; }
              expression = end + 1;
            }
            if (tokens[expression]?.text === '{') {
              // A requirement expression's braces are not an authored function body.
              const end = closing(tokens, expression, '{', '}');
              if (end === undefined) { break; }
              body = end + 1;
              continue;
            }
          }
          requirementClause = true;
        }
        if (suffix === 'requires' || suffix === '->') { constraint = true; }
        if (suffix === 'const' && !constraint) { readonly = true; }
        if (suffix === '(') {
          const end = closing(tokens, body, '(', ')');
          if (end === undefined) { break; }
          body = end + 1;
        } else if (suffix === '{') { break; }
        else if (!constraint && !['const', 'volatile', '&', 'noexcept', 'override', 'final'].includes(suffix)) { break; }
        else { ++body; }
      }
      if (tokens[body]?.text !== '{') { continue; }
      const qualified = components.join('::');
      const scope = scopes[scopes.length - 1];
      const global = tokens[ownerStart - 1]?.text === '::';
      const parameters = separated(tokens.slice(index + 2, close)).map(parameterType);
      result.push({ start: token.start, end: token.end, name: token.text,
        owner: global || !scope.length || qualified.startsWith(scope.join('::') + '::')
          ? qualified : [...scope, ...components].join('::'),
        ownerArguments, inputs, parameters: parameters.length === 1 && parameters[0] === 'void' ? [] : parameters, readonly });
      inputs = [];
      // A matched function's body cannot contain another out-of-class member definition.
      const end = closing(tokens, body, '{', '}');
      if (end !== undefined) { index = end; }
    }
  }
  return result;
}

/** Normalize the typed schema's supported C++ representation without guessing aliases or conversions. */
function canonicalType(text: string, aliases: Map<string, string>, scope: string): string {
  let type = codeOnly(text).replace(/\b[A-Za-z_]\w*\b/g, word => aliases.get(word) ?? word).trim();
  const schemaBorrowed = /\bconst_ref\s*$/.test(type);
  const leadingConst = /^const\b/.test(type);
  const trailingConst = /\bconst\s*&?\s*$/.test(type);
  const borrowed = schemaBorrowed || (/&\s*$/.test(type) && !/&&\s*$/.test(type) && (leadingConst || trailingConst));
  if (schemaBorrowed) { type = type.replace(/\bconst_ref\s*$/, ''); }
  if (leadingConst) { type = type.replace(/^const\b\s*/, ''); }
  if (trailingConst) { type = type.replace(/\bconst(?=\s*&?\s*$)/, ''); }
  const schemaArray = /^array(?:\s|\[|<)/.test(type);
  type = type.replace(/\s+/g, '');
  if (borrowed && type.endsWith('&')) { type = type.slice(0, -1); }
  type = type.replace(/(^|[<,])::/g, '$1').replace(/\b(?:std::)?(u?int(?:8|16|32|64))_t\b/g, '$1')
    .replace(/\bstd::string\b/g, 'string').replace(/\bstd::expected\b/g, 'expected');
  const fixed = schemaArray && /^array\[([^\]]+)\](.+)$/.exec(type);
  if (fixed) { type = `std::array<${fixed[2]},${fixed[1]}>`; }
  const dynamic = schemaArray && /^array\s*(?:<(.+)>|(.+))$/.exec(type);
  if (dynamic) { type = `std::vector<${dynamic[1] ?? dynamic[2]}>`; }
  // An owner namespace's ordinary record types have the same identity whether explicitly qualified or not.
  if (scope) { type = type.replace(new RegExp(`(^|[<,(])${scope}::`, 'g'), '$1'); }
  return (borrowed ? 'borrow:' : '') + type;
}

/** Require exact owner, constness and parameter types, including renamed owner/input template parameters. */
export function callableMatches(declaration: CallableSymbol, definition: CppCallable,
  owners: string[], ownerInputs: string[]): boolean {
  if (declaration.name !== definition.name || !owners.includes(definition.owner) ||
      (declaration.static ? definition.readonly : declaration.readonly !== definition.readonly) ||
      declaration.parameters.length !== definition.parameters.length) { return false; }
  const aliases = new Map<string, string>();
  definition.ownerArguments.forEach((argument, index) => {
    if (/^[A-Za-z_]\w*$/.test(argument.trim()) && ownerInputs[index]) {
      aliases.set(argument.trim(), ownerInputs[index]);
    }
  });
  definition.inputs.filter(input => !aliases.has(input)).forEach((input, index) => {
    if (declaration.inputs[index]) { aliases.set(input, declaration.inputs[index]); }
  });
  const scope = declaration.owner.split('::').slice(0, -1).join('::');
  return declaration.parameters.every((type, index) =>
    canonicalType(type, new Map(), scope) === canonicalType(definition.parameters[index], aliases, scope));
}
