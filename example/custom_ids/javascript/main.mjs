import {CustomId, EntityBinding, RegistryState, Protocol} from './schema.mjs';

const widgetDomain = 1, gadgetDomain = 2;
// Create an independent key; IDs contain values and never runtime object addresses.
function makeId(scope, domain, local) {
  const result = new CustomId();
  result.scope = scope; result.domain = domain; result.local = local;
  return result;
}
// Include every key component in the host's composite-key comparison.
function key(id) { return `${id.scope}:${id.domain}:${id.local}`; }
// Reserve zero keys and reject unknown stable application domain tags.
function valid(id) { return id.scope !== 0 && id.local !== 0n && [widgetDomain, gadgetDomain].includes(id.domain); }
// Fail the example immediately if any policy or codec assertion is violated.
function check(condition) { if (!condition) throw Error('Custom-ID registry contract failed'); }

// This single-threaded host owns policy; generated schemas only persist ordinary durable state.
class ApplicationRegistry {
  // Validate decoded state before accepting lookups; callers transfer ownership of this model.
  constructor(state = new RegistryState()) {
    this.state = state;
    check(this.validate());
  }
  // Reject duplicate keys and overlap between the live bindings and retirement tombstones.
  validate() {
    const ids = [...this.state.bindings.map(binding => binding.id), ...this.state.retired];
    return ids.every(valid) && new Set(ids.map(key)).size === ids.length;
  }
  // Require the caller's expected type domain as well as the complete scoped key.
  lookup(id, expectedDomain) {
    if (!valid(id) || id.domain !== expectedDomain) return undefined;
    return this.state.bindings.find(binding => key(binding.id) === key(id))?.payload;
  }
  // Copy a fresh key and reject both live duplicate binding and permanent retired-key reuse.
  bind(id, payload) {
    if (!valid(id) || this.lookup(id, id.domain) !== undefined || this.state.retired.some(value => key(value) === key(id))) return false;
    const binding = new EntityBinding();
    binding.id = makeId(id.scope, id.domain, id.local); binding.payload = payload;
    this.state.bindings.push(binding);
    return true;
  }
  // Replacing a binding's payload preserves its logical ID.
  rebind(id, payload) {
    const binding = this.state.bindings.find(value => key(value.id) === key(id));
    if (!binding) return false;
    binding.payload = payload; return true;
  }
  // Persist retirement before removing the binding so the key cannot be allocated again.
  retire(id) {
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
