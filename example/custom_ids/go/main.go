package main

import (
  "bytes"
  "fmt"
)

const widgetDomain uint32 = 1
const gadgetDomain uint32 = 2

// hostKey compares the complete generated record ID without depending on pointer identity.
type hostKey struct { scope uint32; domain uint32; local uint64 }

// makeID returns an independently owned key; the registry copies its fields when binding.
func makeID(scope, domain uint32, local uint64) *CustomId {
  id := NewCustomId(); id.Scope = scope; id.Domain = domain; id.Local = local
  return id
}
// key includes the scope and stable type domain in every host lookup.
func key(id *CustomId) hostKey { return hostKey{id.Scope, id.Domain, id.Local} }
// valid reserves zero values and rejects unknown domain tags.
func valid(id *CustomId) bool { return id != nil && id.Scope != 0 && id.Local != 0 && (id.Domain == widgetDomain || id.Domain == gadgetDomain) }
// check stops execution on any policy or codec mismatch.
func check(ok bool) { if !ok { panic("Custom-ID registry contract failed") } }

// applicationRegistry is a single-threaded host policy over ordinary generated records.
type applicationRegistry struct { state *RegistryState }

// validate rejects malformed imports, duplicate keys, and live/retired overlap.
func (registry *applicationRegistry) validate() bool {
  seen := map[hostKey]bool{}
  for _, binding := range registry.state.Bindings {
    if binding == nil || !valid(binding.Id) || seen[key(binding.Id)] { return false }
    seen[key(binding.Id)] = true
  }
  for _, id := range registry.state.Retired {
    if !valid(id) || seen[key(id)] { return false }
    seen[key(id)] = true
  }
  return true
}
// lookup requires the expected type domain; missing and retired keys have no value.
func (registry *applicationRegistry) lookup(id *CustomId, expectedDomain uint32) (uint32, bool) {
  if valid(id) && id.Domain == expectedDomain {
    for _, binding := range registry.state.Bindings { if key(binding.Id) == key(id) { return binding.Payload, true } }
  }
  return 0, false
}
// bind copies a fresh key and permanently rejects retired identity reuse.
func (registry *applicationRegistry) bind(id *CustomId, payload uint32) bool {
  if !valid(id) { return false }
  if _, found := registry.lookup(id, id.Domain); found { return false }
  for _, retired := range registry.state.Retired { if key(retired) == key(id) { return false } }
  binding := NewEntityBinding(); binding.Id = makeID(id.Scope, id.Domain, id.Local); binding.Payload = payload
  registry.state.Bindings = append(registry.state.Bindings, binding)
  return true
}
// rebind changes payload while preserving every component of logical identity.
func (registry *applicationRegistry) rebind(id *CustomId, payload uint32) bool {
  if !valid(id) { return false }
  for _, binding := range registry.state.Bindings { if key(binding.Id) == key(id) { binding.Payload = payload; return true } }
  return false
}
// retire persists a copied tombstone before removing the live binding.
func (registry *applicationRegistry) retire(id *CustomId) bool {
  if !valid(id) { return false }
  for index, binding := range registry.state.Bindings {
    if key(binding.Id) != key(id) { continue }
    registry.state.Retired = append(registry.state.Retired, makeID(id.Scope, id.Domain, id.Local))
    registry.state.Bindings = append(registry.state.Bindings[:index], registry.state.Bindings[index+1:]...)
    return true
  }
  return false
}
// mustLookup checks the expected domain and payload in the demonstration flow.
func mustLookup(registry *applicationRegistry, id *CustomId, domain, expected uint32) {
  value, found := registry.lookup(id, domain); check(found && value == expected)
}
// missing checks wrong-domain, scoped misses, and absent local IDs.
func missing(registry *applicationRegistry, id *CustomId, domain uint32) {
  _, found := registry.lookup(id, domain); check(!found)
}
// main checks host invariants and fresh codec restoration in all four protocols.
func main() {
  registry := &applicationRegistry{NewRegistryState()}
  first, scoped, typed := makeID(7, widgetDomain, 42), makeID(8, widgetDomain, 42), makeID(7, gadgetDomain, 42)
  check(registry.bind(first, 10) && registry.bind(scoped, 20) && registry.bind(typed, 30))
  check(!registry.bind(first, 99)); mustLookup(registry, first, widgetDomain, 10)
  missing(registry, typed, widgetDomain); missing(registry, makeID(9, widgetDomain, 42), widgetDomain); missing(registry, makeID(7, widgetDomain, 99), widgetDomain)
  check(registry.rebind(first, 11)); mustLookup(registry, first, widgetDomain, 11); check(key(registry.state.Bindings[0].Id) == key(first))
  check(registry.retire(first)); missing(registry, first, widgetDomain)
  check(!registry.bind(first, 12) && !registry.rebind(first, 12) && registry.validate())
  canonical, err := registry.state.Encode(BINARY_NONE); if err != nil { panic(err) }
  for _, protocol := range []Protocol{JSON, BINARY_NONE, BINARY_INTEGER, BINARY_STRING} {
    encoded, err := registry.state.Encode(protocol); if err != nil { panic(err) }
    state, err := DecodeRegistryState(encoded, protocol); if err != nil { panic(err) }
    restored := &applicationRegistry{state}; check(restored.validate() && !restored.bind(first, 12)); mustLookup(restored, scoped, widgetDomain, 20)
    decoded, err := state.Encode(BINARY_NONE); if err != nil { panic(err) }; check(bytes.Equal(decoded, canonical))
  }
  fmt.Println("Scoped custom IDs and four protocols passed")
}
