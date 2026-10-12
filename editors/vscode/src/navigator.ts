import { managedNames } from './navigation_model';
import * as path from 'node:path';
import { fileKey, includeCandidates } from './model';
import { cppIncludeAt, dependencySchemas, indexSource, resolveType, spanAt,
  SourceIndex, Span, TypeSymbol } from './navigation_model';
import { generatedBanner, generatedMember, indexGeneratedSource, outputMemberNames,
  outputNames } from './generated_navigation';

export interface NavigationTarget extends Span { file: string }
export interface NavigationFiles {
  schemas: string[];
  headers: string[];
  outputs?: string[];
  dependencies?: Map<string, string[]>;
}
export interface NavigationHost {
  read(file: string): Promise<string | undefined>;
  files(): Promise<NavigationFiles>;
  includeDirectories: string[][];
  cancelled(): boolean;
}
interface Source { file: string; text: string; index: SourceIndex }
interface Declaration extends TypeSymbol { file: string }
const maximumIncludeDepth = 128;
const maximumSchemaFiles = 4096;

/** Resolve read-only navigation from existing source, headers and optional dependency files. */
export class Navigator {
  private readonly sources = new Map<string, Promise<Source | undefined>>();
  private readonly graphs = new Map<string, Promise<Source[]>>();
  private readonly owners = new Map<string, Promise<string[]>>();
  private inventory?: Promise<NavigationFiles>;

  /** Keep request state isolated so cancellation or file changes cannot publish stale locations. */
  constructor(private readonly host: NavigationHost) {}

  /** Share one workspace discovery operation within this request. */
  private files(): Promise<NavigationFiles> {
    return this.inventory ??= this.host.files();
  }

  /** Read and index each file once per request; missing files are ordinary navigation misses. */
  private source(file: string): Promise<Source | undefined> {
    const key = fileKey(file);
    let source = this.sources.get(key);
    if (!source) {
      source = this.host.cancelled() ? Promise.resolve(undefined) : this.host.read(file).then(text =>
        text === undefined ? undefined : { file, text, index: file.endsWith('.serializer')
          ? indexSource(text, true) : indexGeneratedSource(text, file) });
      this.sources.set(key, source);
    }
    return source;
  }

  /** Traverse schema includes once, preserving source ownership through diamonds and cycles. */
  private graph(entry: string): Promise<Source[]> {
    const key = fileKey(entry);
    let graph = this.graphs.get(key);
    if (!graph) {
      graph = this.loadGraph(entry);
      this.graphs.set(key, graph);
    }
    return graph;
  }

  /** Bound malformed/cyclic include trees without dropping available declarations. */
  private async loadGraph(entry: string): Promise<Source[]> {
    const result: Source[] = [];
    const seen = new Set<string>();
    const pending = [{ file: entry, depth: 0 }];
    while (pending.length && seen.size < maximumSchemaFiles && !this.host.cancelled()) {
      const item = pending.pop()!;
      const key = fileKey(item.file);
      if (seen.has(key) || item.depth >= maximumIncludeDepth) { continue; }
      seen.add(key);
      const source = await this.source(item.file);
      if (!source) { continue; }
      result.push(source);
      for (const include of source.index.includes) {
        pending.push({ file: path.resolve(path.dirname(item.file), include.name), depth: item.depth + 1 });
      }
    }
    return result;
  }

  /** Identify entry schemas from authoritative depfiles, or all matching legacy basenames. */
  private headerEntries(header: string): Promise<string[]> {
    const key = fileKey(header);
    let entries = this.owners.get(key);
    if (!entries) {
      entries = this.findHeaderEntries(header);
      this.owners.set(key, entries);
    }
    return entries;
  }

  /** Use exact dependency ownership; legacy output additionally requires a Serializer banner. */
  private async findHeaderEntries(header: string): Promise<string[]> {
    const text = await this.host.read(header);
    if (text === undefined) { return []; }
    const depfile = await this.host.read(`${header}.d`);
    if (depfile !== undefined) {
      // An invalid/mismatched depfile must not fall back to an unrelated same-name schema.
      return dependencySchemas(depfile, header).slice(0, 1);
    }
    const inventory = await this.files();
    const dependencies = inventory.dependencies?.get(fileKey(header));
    if (dependencies) { return dependencies; }
    if (!generatedBanner(text)) { return []; }
    const stem = fileKey(path.basename(header, path.extname(header)));
    return inventory.schemas.filter(file =>
      fileKey(path.basename(file, '.serializer')) === stem);
  }

  /** Keep duplicate include paths and provider locations from appearing more than once. */
  private unique(targets: NavigationTarget[]): NavigationTarget[] {
    if (this.host.cancelled()) { return []; }
    return [...new Map(targets.map(target =>
      [`${fileKey(target.file)}:${target.start}:${target.end}`, target])).values()];
  }

  /** Return the exact declaration selected in a schema, including transitive included types. */
  private async declarations(file: string, offset: number): Promise<Declaration[]> {
    const source = await this.source(file);
    const reference = source && spanAt(source.index.references, offset);
    if (!reference) { return []; }
    const declarations = (await this.graph(file)).flatMap(item =>
      item.index.symbols.map(symbol => ({ ...symbol, file: item.file })));
    return resolveType(reference, declarations) as Declaration[];
  }

  /** Locate existing output headers containing this schema; favor its current entry compilation. */
  private async outputs(schema: string, context: string): Promise<string[]> {
    const preferred: string[] = [];
    const available: string[] = [];
    const inventory = await this.files();
    for (const header of inventory.outputs ?? inventory.headers) {
      if (this.host.cancelled()) { return []; }
      for (const entry of await this.headerEntries(header)) {
        const graph = await this.graph(entry);
        if (!graph.some(source => fileKey(source.file) === fileKey(schema))) { continue; }
        available.push(header);
        if (fileKey(entry) === fileKey(context)) { preferred.push(header); }
      }
    }
    return [...new Set(preferred.length ? preferred : available)];
  }

  /** Find generated field definitions through the same dependency and owner identity as reverse navigation. */
  private async memberDefinitions(file: string, member: Span & { name: string; owner: string; getter?: boolean }): Promise<NavigationTarget[]> {
    const targets: NavigationTarget[] = [];
    for (const output of await this.outputs(file, file)) {
      const generated = await this.source(output);
      if (!generated || this.host.cancelled()) { continue; }
      const names = outputMemberNames(member.name, output, generated.text);
      const owners = outputNames(member.owner, output, generated.text);
      for (const entry of await this.headerEntries(output)) {
        const graph = await this.graph(entry);
        const symbols = graph.flatMap(source => source.index.symbols);
        for (const alias of symbols.filter(symbol => symbol.genericSource)) {
          if (resolveType(alias.genericSource!, symbols).some(family => family.qualified === member.owner)) {
            owners.push(...outputNames(alias.qualified, output, generated.text));
          }
        }
      }
      for (const token of generated.text.matchAll(/[A-Za-z_]\w*/g)) {
        if (this.host.cancelled()) { return []; }
        if (!names.includes(token[0])) { continue; }
        const field = generatedMember(generated.text, output, generated.index, token.index!);
        if (field && !!field.getter === !!member.getter &&
            (field.owner.schemaQualified === member.owner || owners.includes(field.owner.qualified))) {
          targets.push({ file: output, start: field.start, end: field.end });
        }
      }
    }
    return this.unique(targets.length ? targets : [{ file, start: member.start, end: member.end }]);
  }

  /** Resolve each conversion projection against current included declarations, including unsaved field types. */
  private async conversionMember(file: string, offset: number): Promise<(NavigationTarget & { name: string; owner: string })[]> {
    const source = await this.source(file);
    const reference = source && spanAt(source.index.memberReferences ?? [], offset);
    if (!reference) { return []; }
    const graph = await this.graph(file);
    const symbols = graph.flatMap(item => item.index.symbols);
    let owners = resolveType({ ...reference.owner, start: reference.start, end: reference.end }, symbols);
    const members = graph.flatMap(item => (item.index.members ?? []).map(member => ({ ...member, file: item.file })));
    /** Follow authored bases and named generic declarations without depending on saved generated output. */
    const inherited = (initial: TypeSymbol[]): TypeSymbol[] => {
      const result: TypeSymbol[] = [];
      const pending = [...initial];
      const seen = new Set<string>();
      while (pending.length && seen.size < maximumIncludeDepth) {
        const owner = pending.pop()!;
        if (seen.has(owner.qualified)) { continue; }
        seen.add(owner.qualified); result.push(owner);
        for (const base of owner.bases ?? []) {
          pending.push(...resolveType({ ...base, start: owner.start, end: owner.end }, symbols));
        }
        if (owner.genericSource) { pending.push(...resolveType(owner.genericSource, symbols)); }
      }
      return result;
    };
    const projection = [...reference.path, reference.name];
    for (let index = 0; index < projection.length; ++index) {
      const name = projection[index];
      const selected = members.filter(member => member.name === name &&
        inherited(owners).some(owner => owner.qualified === member.owner));
      if (index === projection.length - 1) { return selected; }
      owners = selected.flatMap(member => member.type
        ? resolveType({ ...member.type, start: member.start, end: member.end }, symbols) : []);
    }
    return [];
  }

  /** Prefer generated type and field definitions, falling back to their schema without generating output. */
  async schema(file: string, offset: number, definition: boolean): Promise<NavigationTarget[]> {
    const source = await this.source(file);
    const include = source && spanAt(source.index.includes, offset);
    if (include) {
      const included = path.resolve(path.dirname(file), include.name);
      if (!await this.source(included)) { return []; }
      // An include identifies a schema, which may contribute many generated declarations.
      return [{ file: included, start: 0, end: 0 }];
    }
    const projected = await this.conversionMember(file, offset);
    if (projected.length) {
      return definition ? this.unique((await Promise.all(projected.map(member =>
        this.memberDefinitions(member.file, member)))).flat()) : this.unique(projected.map(member =>
        ({ file: member.file, start: member.start, end: member.end })));
    }
    const member = definition && source && spanAt(source.index.members ?? [], offset);
    if (member) { return this.memberDefinitions(file, member); }
    const declarations = await this.declarations(file, offset);
    if (!definition) { return this.unique(declarations); }
    const targets: NavigationTarget[] = [];
    for (const declaration of declarations) {
      for (const header of await this.outputs(declaration.file, file)) {
        const generated = await this.source(header);
        if (!generated) { continue; }
        const names = outputNames(declaration.qualified, header, generated.text);
        const matches = (generated?.index.symbols ?? []).filter(symbol =>
          symbol.definition && symbol.kind === declaration.kind &&
          (names.includes(symbol.qualified) || symbol.schemaQualified === declaration.qualified));
        // Preserve distinct identifiers in naming=preserve outputs that normalize alike.
        const preferredName = names.find(name => matches.some(symbol => symbol.qualified === name));
        for (const symbol of matches.filter(symbol => preferredName ? symbol.qualified === preferredName :
            symbol.schemaQualified === declaration.qualified)) {
          targets.push({ file: header, start: symbol.start, end: symbol.end });
        }
      }
    }
    return this.unique(targets.length ? targets : declarations);
  }

  /** Open only existing generated outputs for a whole schema, including included-only schemas. */
  async headers(file: string): Promise<NavigationTarget[]> {
    return (await this.implementations(file)).filter(target => /\.(hpp|h|hxx|hh)$/i.test(target.file));
  }

  /** Open existing implementations in any generated language, with the same ownership rules. */
  async implementations(file: string): Promise<NavigationTarget[]> {
    return this.unique((await this.outputs(file, file)).map(output => ({ file: output, start: 0, end: 0 })));
  }

  /** Resolve literal C++ includes in compiler search order, then use available workspace outputs. */
  async cppInclude(file: string, offset: number, definition: boolean): Promise<NavigationTarget[]> {
    const source = await this.source(file);
    const include = source && cppIncludeAt(source.text, offset);
    if (!include) { return []; }
    const headers: string[] = [];
    let shadowed = false;
    const groups = this.host.includeDirectories;
    for (const directories of groups.length ? groups : [[]]) {
      for (const candidate of includeCandidates(file, include, directories)) {
        const text = await this.host.read(candidate);
        if (text === undefined) { continue; }
        // An ordinary header earlier on the search path shadows Serializer outputs too.
        if (generatedBanner(text) || (await this.headerEntries(candidate)).length) { headers.push(candidate); }
        else { shadowed = true; }
        if (!groups.length) { return this.includeTargets(headers, definition); }
        break;
      }
    }
    if (!groups.length && !headers.length) {
      const suffix = fileKey(path.normalize(include.name));
      for (const header of (await this.files()).headers) {
        if (fileKey(header).endsWith(path.sep + suffix) &&
          await this.isGenerated(header)) {
          headers.push(header);
        }
      }
    }
    if (!definition && !headers.length && !shadowed && /\.(hpp|h|hxx|hh)$/.test(include.name)) {
      // Declaration navigation remains useful before the very first generation.
      const stem = fileKey(path.basename(include.name, path.extname(include.name)));
      return this.unique((await this.files()).schemas.filter(schema =>
        fileKey(path.basename(schema, '.serializer')) === stem)
        .map(schema => ({ file: schema, start: 0, end: 0 })));
    }
    return this.includeTargets(headers, definition);
  }

  /** Map include targets to entry schemas, excluding deleted destinations. */
  private async includeTargets(headers: string[], definition: boolean): Promise<NavigationTarget[]> {
    const targets: NavigationTarget[] = [];
    for (const header of headers) {
      for (const file of definition ? [header] : await this.headerEntries(header)) {
        if (await this.host.read(file) !== undefined) { targets.push({ file, start: 0, end: 0 }); }
      }
    }
    return this.unique(targets);
  }

  /** Recognize owned output without accepting an unrelated include with the same basename. */
  private async isGenerated(file: string): Promise<boolean> {
    const text = await this.host.read(file);
    return text !== undefined && (generatedBanner(text) || (await this.headerEntries(file)).length > 0);
  }

  /** Translate a language service's resolved class/enum location to its source schema. */
  async generatedDeclaration(header: string, offset: number, end = offset): Promise<NavigationTarget[]> {
    const source = await this.source(header);
    const symbol = source && (spanAt(source.index.symbols.map(item =>
      ({ ...item, start: item.declaration })), offset)
      ?? source.index.symbols.find(item => item.declaration >= offset && item.end <= end));
    const member = !symbol && source ? generatedMember(source.text, header, source.index, offset) : undefined;
    if (!symbol && !member) { return []; }
    const entries = await this.headerEntries(header);
    if (!entries.length) { return []; }
    const targets: NavigationTarget[] = [];
    for (const entry of entries) {
      const matches: Declaration[] = [];
      const graph = await this.graph(entry);
      const symbols = graph.flatMap(schema => schema.index.symbols);
      const families = new Set<string>();
      if (member) {
        // Named concrete copies retain field provenance from the generic declaration.
        for (const alias of symbols.filter(symbol => symbol.genericSource &&
            outputNames(symbol.qualified, header, source!.text).includes(member.owner.qualified))) {
          for (const family of resolveType(alias.genericSource!, symbols)) { families.add(family.qualified); }
        }
      }
      for (const schema of graph) {
        if (member) {
          const members = (schema.index.members ?? []).filter(declaration =>
            !!declaration.getter === !!member.getter &&
            (member.owner.schemaQualified === declaration.owner || families.has(declaration.owner) ||
             outputNames(declaration.owner, header, source!.text).includes(member.owner.qualified)) &&
            outputMemberNames(declaration.name, header, source!.text).includes(member.name));
          const exact = members.filter(declaration => declaration.name === member.name);
          for (const declaration of exact.length ? exact : members) {
            targets.push({ file: schema.file, start: declaration.start, end: declaration.end });
          }
          continue;
        }
        for (const declaration of schema.index.symbols) {
          if (declaration.kind === symbol!.kind && (symbol!.schemaQualified === declaration.qualified ||
            [...outputNames(declaration.qualified, header, source!.text),
            ...(/\.(hpp|h|hxx|hh)$/i.test(header) ? managedNames(declaration.qualified) : [])].includes(symbol!.qualified))) {
            matches.push({ ...declaration, file: schema.file });
          }
        }
      }
      const exact = matches.filter(declaration =>
        outputNames(declaration.qualified, header, source!.text)[0] === symbol!.qualified);
      targets.push(...(exact.length ? exact : matches));
    }
    return this.unique(targets);
  }
}
