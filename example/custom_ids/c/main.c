#include "schema.h"

static const uint32_t widget_domain = 1;
static const uint32_t gadget_domain = 2;
static const size_t maximum_registry_entries = 64;

/* Create a copied value key; no runtime address becomes a durable identity. */
static custom_id make_id(uint32_t scope, uint32_t domain, uint64_t local) {
  custom_id result = {0}; result.scope = scope; result.domain = domain; result.local = local;
  return result;
}
/* Compare all components, including the application scope and stable type domain. */
static bool same_id(custom_id left, custom_id right) {
  return left.scope == right.scope && left.domain == right.domain && left.local == right.local;
}
/* Reserve zero values and accept only documented domain tags. */
static bool valid_id(custom_id id) {
  return id.scope != 0 && id.local != 0 && (id.domain == widget_domain || id.domain == gadget_domain);
}
/* Stop on any failed policy, allocation, or codec check. */
static void require(bool success) {
  if (!success) { fputs("Custom-ID registry contract failed\n", stderr); exit(EXIT_FAILURE); }
}
/* Grow the owned live array within the demonstration's explicit host admission limit. */
static bool reserve_binding(registry_state* state) {
  if (state->bindings.size >= maximum_registry_entries) { return false; }
  if (state->bindings.size < state->bindings.capacity) { return true; }
  size_t capacity = state->bindings.capacity ? state->bindings.capacity * 2 : 1;
  if (capacity > maximum_registry_entries) { capacity = maximum_registry_entries; }
  entity_binding* data = (entity_binding*)realloc(state->bindings.data, capacity * sizeof(*data));
  if (!data) { return false; }
  state->bindings.data = data; state->bindings.capacity = capacity; return true;
}
/* Reserve retirement space before modifying live membership, preserving allocation-failure behavior. */
static bool reserve_retirement(registry_state* state) {
  if (state->retired.size >= maximum_registry_entries) { return false; }
  if (state->retired.size < state->retired.capacity) { return true; }
  size_t capacity = state->retired.capacity ? state->retired.capacity * 2 : 1;
  if (capacity > maximum_registry_entries) { capacity = maximum_registry_entries; }
  custom_id* data = (custom_id*)realloc(state->retired.data, capacity * sizeof(*data));
  if (!data) { return false; }
  state->retired.data = data; state->retired.capacity = capacity; return true;
}
/* Validate imported policy before lookup: keys are valid, unique, and never both live and retired. */
static bool validate_registry(const registry_state* state) {
  if (state->bindings.size > maximum_registry_entries || state->retired.size > maximum_registry_entries) { return false; }
  for (size_t index = 0; index < state->bindings.size; ++index) {
    const custom_id id = state->bindings.data[index].id;
    if (!valid_id(id)) { return false; }
    for (size_t previous = 0; previous < index; ++previous) {
      if (same_id(id, state->bindings.data[previous].id)) { return false; }
    }
    for (size_t retired = 0; retired < state->retired.size; ++retired) {
      if (same_id(id, state->retired.data[retired])) { return false; }
    }
  }
  for (size_t index = 0; index < state->retired.size; ++index) {
    if (!valid_id(state->retired.data[index])) { return false; }
    for (size_t previous = 0; previous < index; ++previous) {
      if (same_id(state->retired.data[index], state->retired.data[previous])) { return false; }
    }
  }
  return true;
}
/* Require exact domain/scoped identity; missing and retired keys do not yield a payload. */
static bool lookup(const registry_state* state, custom_id id, uint32_t expected_domain, uint32_t* payload) {
  if (!valid_id(id) || id.domain != expected_domain) { return false; }
  for (size_t index = 0; index < state->bindings.size; ++index) {
    if (same_id(state->bindings.data[index].id, id)) {
      if (payload) { *payload = state->bindings.data[index].payload; }
      return true;
    }
  }
  return false;
}
/* Bind a copied fresh key; duplicate live keys and retired identity reuse are rejected. */
static bool bind(registry_state* state, custom_id id, uint32_t payload) {
  if (!valid_id(id) || lookup(state, id, id.domain, NULL)) { return false; }
  for (size_t index = 0; index < state->retired.size; ++index) {
    if (same_id(id, state->retired.data[index])) { return false; }
  }
  if (!reserve_binding(state)) { return false; }
  entity_binding value = {0}; value.id = id; value.payload = payload;
  state->bindings.data[state->bindings.size++] = value; return true;
}
/* Replace payload without changing the complete logical key. */
static bool rebind(registry_state* state, custom_id id, uint32_t payload) {
  for (size_t index = 0; index < state->bindings.size; ++index) {
    if (same_id(state->bindings.data[index].id, id)) { state->bindings.data[index].payload = payload; return true; }
  }
  return false;
}
/* Persist retirement before removing a live entry; scalar records need no extra cleanup. */
static bool retire(registry_state* state, custom_id id) {
  for (size_t index = 0; index < state->bindings.size; ++index) {
    if (!same_id(state->bindings.data[index].id, id)) { continue; }
    if (!reserve_retirement(state)) { return false; }
    state->retired.data[state->retired.size++] = id;
    memmove(state->bindings.data + index, state->bindings.data + index + 1,
        (state->bindings.size - index - 1) * sizeof(*state->bindings.data));
    --state->bindings.size;
    memset(state->bindings.data + state->bindings.size, 0, sizeof(*state->bindings.data));
    return true;
  }
  return false;
}
/* Exercise ownership-safe policy operations and fresh exact codec restoration in all protocols. */
int main(void) {
  registry_state state = {0}; require(registry_state_init(&state) == srl_ok);
  const custom_id first = make_id(7, widget_domain, 42), scoped = make_id(8, widget_domain, 42), typed = make_id(7, gadget_domain, 42);
  uint32_t payload = 0;
  require(bind(&state, first, 10) && bind(&state, scoped, 20) && bind(&state, typed, 30));
  require(!bind(&state, first, 99) && lookup(&state, first, widget_domain, &payload) && payload == 10);
  require(!lookup(&state, typed, widget_domain, NULL) && !lookup(&state, make_id(9, widget_domain, 42), widget_domain, NULL));
  require(!lookup(&state, make_id(7, widget_domain, 99), widget_domain, NULL));
  require(rebind(&state, first, 11) && lookup(&state, first, widget_domain, &payload) && payload == 11);
  require(same_id(state.bindings.data[0].id, first));
  require(retire(&state, first) && !lookup(&state, first, widget_domain, NULL));
  require(!bind(&state, first, 12) && !rebind(&state, first, 12) && validate_registry(&state));
  srl_buffer canonical = {0}; require(registry_state_encode(&state, srl_binary_none, &canonical) == srl_ok);
  for (int protocol = srl_json; protocol <= srl_binary_string; ++protocol) {
    srl_buffer encoded = {0}, decoded = {0}; registry_state copy = {0};
    require(registry_state_encode(&state, (srl_protocol)protocol, &encoded) == srl_ok);
    require(registry_state_decode(&copy, encoded.data, encoded.size, (srl_protocol)protocol, NULL) == srl_ok);
    require(validate_registry(&copy) && !bind(&copy, first, 12));
    require(lookup(&copy, scoped, widget_domain, &payload) && payload == 20);
    require(registry_state_encode(&copy, srl_binary_none, &decoded) == srl_ok);
    require(decoded.size == canonical.size && memcmp(decoded.data, canonical.data, canonical.size) == 0);
    registry_state_free(&copy); srl_buffer_free(&encoded); srl_buffer_free(&decoded);
  }
  registry_state_free(&state); srl_buffer_free(&canonical);
  puts("Scoped custom IDs and four protocols passed"); return 0;
}
