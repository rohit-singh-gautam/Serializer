import {CustomId, EntityBinding, RegistryState, Protocol} from './schema.mjs';

const widgetDomain = 1, gadgetDomain = 2;
// Create an independent value key; BigInt preserves the full uint64 local representation.
function makeId(scope: number, domain: number, local: bigint): CustomId {
  const result = new CustomId();
  result.scope = scope; result.domain = domain; result.local = local;
  return result;
}
// Compare the complete scoped/type-tag tuple in an application-owned host index.
function key(id: CustomId): string { return `${id.scope}:${id.domain}:${id.local}`; }
// Reserve zero values and reject unknown stable application domain tags.
function valid(id: CustomId): boolean { return id.scope !== 0 && id.local !== 0n && [widgetDomain, gadgetDomain].includes(id.domain); }
// Fail immediately on any policy or codec mismatch.
function check(condition: boolean): void { if (!condition) throw Error('Custom-ID registry contract failed'); }

// Policy belongs to this single-threaded host, independently of the generated record codec.
class ApplicationRegistry {
  // Take ownership of decoded state and validate it before exposing application lookups.
  constructor(public readonly state: RegistryState = new RegistryState()) { check(this.validate()); }
  // Reject duplicates and keys simultaneously present in live bindings and durable tombstones.
  validate(): boolean {
    const ids = [...this.state.bindings.map(binding => binding.id), ...this.state.retired];
    return ids.every(valid) && new Set(ids.map(key)).size === ids.length;
  }
  // Require exact scope/local identity and the caller's expected type domain.
  lookup(id: CustomId, expectedDomain: number): number | undefined {
    if (!valid(id) || id.domain !== expectedDomain) return undefined;
    return this.state.bindings.find(binding => key(binding.id) === key(id))?.payload;
  }
  // Copy keys so mutating a caller-owned object cannot change the registered identity.
  bind(id: CustomId, payload: number): boolean {
    if (!valid(id) || this.lookup(id, id.domain) !== undefined || this.state.retired.some(value => key(value) === key(id))) return false;
    const binding = new EntityBinding();
    binding.id = makeId(id.scope, id.domain, id.local); binding.payload = payload;
    this.state.bindings.push(binding); return true;
  }
  // Rebinding replaces payload while keeping every logical key component intact.
  rebind(id: CustomId, payload: number): boolean {
    const binding = this.state.bindings.find(value => key(value.id) === key(id));
    if (!binding) return false;
    binding.payload = payload; return true;
  }
  // Retirement persists a tombstone and forbids key reuse after codec restoration.
  retire(id: CustomId): boolean {
    const index = this.state.bindings.findIndex(value => key(value.id) === key(id));
    if (index < 0) return false;
    this.state.retired.push(makeId(id.scope, id.domain, id.local));
    this.state.bindings.splice(index, 1); return true;
  }
}

const registry = new ApplicationRegistry();
const first = makeId(7, widgetDomain, 42n), scoped = makeId(8, widgetDomain, 42n), typed = makeId(7, gadgetDomain, 42n);
check(registry.bind(first, 10) && registry.bind(scoped, 20) && registry.bind(typed, 30));
check(!registry.bind(first, 99) && registry.lookup(first, widgetDomain) === 10);
check(registry.lookup(typed, widgetDomain) === undefined && registry.lookup(makeId(9, widgetDomain, 42n), widgetDomain) === undefined);
check(registry.lookup(makeId(7, widgetDomain, 99n), widgetDomain) === undefined);
check(registry.rebind(first, 11) && registry.lookup(first, widgetDomain) === 11 && key(registry.state.bindings[0].id) === key(first));
check(registry.retire(first) && registry.lookup(first, widgetDomain) === undefined);
check(!registry.bind(first, 12) && !registry.rebind(first, 12));
const canonical = registry.state.encode(Protocol.BINARY_NONE);
for (const protocol of Object.values(Protocol)) {
  const restored = new ApplicationRegistry(RegistryState.decode(registry.state.encode(protocol), protocol));
  const bytes = restored.state.encode(Protocol.BINARY_NONE);
  check(bytes.length === canonical.length && bytes.every((value, index) => value === canonical[index]));
  check(!restored.bind(first, 12) && restored.lookup(scoped, widgetDomain) === 20);
}
console.log('Scoped custom IDs and four protocols passed');
