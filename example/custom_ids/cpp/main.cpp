#include "message.hpp"
#include <cstdint>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace {
constexpr std::uint32_t widget_domain = 1;
constexpr std::uint32_t gadget_domain = 2;

// Create a caller-owned key; the registry copies its value rather than retaining an address.
custom_id make_id(std::uint32_t scope, std::uint32_t domain, std::uint64_t local) {
  custom_id result{};
  result.scope = scope; result.domain = domain; result.local = local;
  return result;
}
// Inspect the complete application identity, including its namespace and type domain.
auto key(const custom_id& id) { return std::tuple{id.scope, id.domain, id.local}; }
// Reserve zero values and reject unknown application-defined domain tags.
bool valid(const custom_id& id) {
  return id.scope != 0 && id.local != 0 && (id.domain == widget_domain || id.domain == gadget_domain);
}
// Stop on a violated example contract without silently accepting a partial check.
void require(bool condition) {
  if (!condition) { throw std::runtime_error{"Custom-ID registry contract failed"}; }
}

// This single-threaded host owns binding and retirement policy over ordinary generated records.
class application_registry {
public:
  registry_state state{};
  // Validate imported state before exposing application lookups; no live/retired key may repeat.
  bool validate() const {
    std::set<decltype(key(custom_id{}))> seen;
    for (const auto& binding : state.bindings) {
      if (!valid(binding.id) || !seen.insert(key(binding.id)).second) { return false; }
    }
    for (const auto& id : state.retired) {
      if (!valid(id) || !seen.insert(key(id)).second) { return false; }
    }
    return true;
  }
  // Find a typed key in the current state; missing, retired, and wrong-domain IDs have no value.
  std::optional<std::uint32_t> lookup(const custom_id& id, std::uint32_t expected_domain) const {
    if (!valid(id) || id.domain != expected_domain) { return {}; }
    for (const auto& binding : state.bindings) {
      if (key(binding.id) == key(id)) { return binding.payload; }
    }
    return {};
  }
  // Bind a fresh logical key; tombstones ensure retirement cannot authorize identity reuse.
  bool bind(const custom_id& id, std::uint32_t payload) {
    if (!valid(id) || lookup(id, id.domain)) { return false; }
    for (const auto& retired : state.retired) { if (key(retired) == key(id)) { return false; } }
    entity_binding binding{}; binding.id = id; binding.payload = payload;
    state.bindings.push_back(std::move(binding));
    return true;
  }
  // Replace payload in place while preserving scope, domain, and local logical identity.
  bool rebind(const custom_id& id, std::uint32_t payload) {
    for (auto& binding : state.bindings) {
      if (key(binding.id) == key(id)) { binding.payload = payload; return true; }
    }
    return false;
  }
  // Record retirement before erasing the live entry; allocation failure leaves the entry live.
  bool retire(const custom_id& id) {
    for (auto iterator = state.bindings.begin(); iterator != state.bindings.end(); ++iterator) {
      if (key(iterator->id) == key(id)) {
        state.retired.push_back(iterator->id);
        state.bindings.erase(iterator);
        return true;
      }
    }
    return false;
  }
};

// Serialize ordinary durable state; application indexes and runtime addresses never enter bytes.
template <template <rohit::serializer::serialize_type> typename Protocol>
auto encode(const registry_state& state) {
  rohit::full_stream_auto_alloc output{};
  state.serialize_out<Protocol>(output);
  return std::string{reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}
// Exact decoding creates a fresh owned registry state and rejects trailing input.
template <template <rohit::serializer::serialize_type> typename Protocol>
registry_state decode(const std::string& bytes) {
  auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  return rohit::serializer::deserialize_exact<registry_state, Protocol>(input);
}
// Restore and revalidate the host policy after each complete codec round trip.
template <template <rohit::serializer::serialize_type> typename Protocol>
void round_trip(const application_registry& source, const custom_id& retired) {
  application_registry restored{};
  restored.state = decode<Protocol>(encode<Protocol>(source.state));
  require(restored.validate() && !restored.bind(retired, 99));
  require(encode<rohit::serializer::binary_none>(restored.state) == encode<rohit::serializer::binary_none>(source.state));
}
}

// Exercise scoped/type-tag IDs, duplicate rejection, preserved identity, retirement, and all codecs.
int main() {
  application_registry registry{};
  const auto first = make_id(7, widget_domain, 42);
  const auto scoped = make_id(8, widget_domain, 42);
  const auto typed = make_id(7, gadget_domain, 42);
  require(registry.bind(first, 10) && registry.bind(scoped, 20) && registry.bind(typed, 30));
  require(!registry.bind(first, 99) && registry.lookup(first, widget_domain) == 10u);
  require(!registry.lookup(typed, widget_domain) && !registry.lookup(make_id(9, widget_domain, 42), widget_domain));
  require(!registry.lookup(make_id(7, widget_domain, 99), widget_domain));
  require(registry.rebind(first, 11) && registry.lookup(first, widget_domain) == 11u);
  require(key(registry.state.bindings.front().id) == key(first));
  require(registry.retire(first) && !registry.lookup(first, widget_domain));
  require(!registry.bind(first, 12) && !registry.rebind(first, 12) && registry.validate());
  round_trip<rohit::serializer::json>(registry, first);
  round_trip<rohit::serializer::binary_none>(registry, first);
  round_trip<rohit::serializer::binary_integer>(registry, first);
  round_trip<rohit::serializer::binary_string>(registry, first);
  std::cout << "Scoped custom IDs and four protocols passed\n";
}
