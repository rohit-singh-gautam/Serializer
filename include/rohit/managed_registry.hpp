// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/managed_editor.hpp>
#include <rohit/managed_identity.hpp>
#include <rohit/managed_reference.hpp>
#include <rohit/managed_resident.hpp>

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rohit::managed {

// Generated descriptors describe schema fields, never native object offsets.
template <typename Plain> struct fields;
// Ordinary nested values use their own field projection, including separate-value profiles.
template <typename Plain> struct value_fields;
enum class registry_error_code { missing_id, wrong_type, missing_member, wrong_member_type,
                                 duplicate_key, invalid_identity, expired_editor,
                                 expired_lease, wrong_version, foreign_document };

// Preserve a machine-readable failure without exposing a mutable catalog.
class registry_error : public std::logic_error {
  registry_error_code code_;
public:
  // Retain the precise lookup or binding failure alongside its diagnostic.
  registry_error(registry_error_code code, const char* message)
      : std::logic_error{message}, code_{code} {}
  // Inspect the failure independently of diagnostic wording.
  registry_error_code code() const noexcept { return code_; }
};

// Static metadata is shared by all occurrences of one generated type.
struct member_info {
  std::uint32_t id{};
  std::string_view name{};
  bool writable{};
};

// A typed descriptor uses generated access, including private schema fields.
template <typename Plain, typename Storage, typename Value>
struct member_descriptor {
  using plain_type = Plain;
  using storage_type = Storage;
  using value_type = Value;
  member_info info{};
  const Value& (*get)(const Storage&){};
  void (*set)(Storage&, Value){};
  Value& (*mutable_get)(Storage&){};

  // Copy a field without lending an alias beyond its owning snapshot or callback.
  Value read(const Storage& storage) const { return get(storage); }
  // Reject immutable metadata before applying a typed candidate assignment.
  void write(Storage& storage, Value value) const {
    if (!set) {
      throw registry_error{registry_error_code::missing_member, "Registry member is readonly"};
    }
    set(storage, std::move(value));
  }
};

// Native scalar index semantics are versioned independently of payload field encoding.
enum class index_method { ordered, unordered };
enum class index_site { inherit, local };
struct index_info {
  std::uint32_t id{};
  std::uint32_t field_id{};
  index_method method{};
  bool unique{};
  index_site site{};
  std::uint32_t comparator_version{1};
};

// Retain ordinary descriptor operations while making query capabilities statically checkable.
template <typename Plain, typename Storage, typename Value, index_method Method,
          bool Unique, index_site Site = index_site::inherit>
struct indexed_member_descriptor : member_descriptor<Plain, Storage, Value> {
  static constexpr auto method = Method;
  static constexpr bool unique = Unique;
  static constexpr auto site = Site;
  // Bind the same private-safe generated accessors without adding per-object data.
  constexpr indexed_member_descriptor(member_info metadata, const Value& (*getter)(const Storage&),
      void (*setter)(Storage&, Value), Value& (*mutable_getter)(Storage&) = nullptr)
      : member_descriptor<Plain, Storage, Value>{metadata, getter, setter, mutable_getter} {}
  // Field shorthand reserves its field ID in the separate type-local index identity domain.
  constexpr index_info index() const noexcept {
    return {this->info.id, this->info.id, Method, Unique, Site, 1};
  }
};

namespace detail {
// Exact paths contain only stable field/base IDs; collection positions remain separate addresses.
template <typename Descriptors, typename Storage, typename Value>
Value read_registry_path(const Storage& storage, std::span<const std::uint32_t> path) {
  if (path.empty()) { throw registry_error{registry_error_code::missing_member, "Empty registry field path"}; }
  std::optional<Value> result;
  bool exists{};
  Descriptors::visit([&](const auto& descriptor) {
    if (descriptor.info.id != path.front()) { return; }
    exists = true;
    using field_type = typename std::remove_cvref_t<decltype(descriptor)>::value_type;
    if (path.size() == 1) {
      if constexpr (std::same_as<Value, field_type>) { result.emplace(descriptor.read(storage)); }
    } else if constexpr (requires { value_fields<field_type>::visit([](const auto&) {}); }) {
      result.emplace(read_registry_path<value_fields<field_type>, field_type, Value>(
          descriptor.get(storage), path.subspan(1)));
    }
  });
  if (!result) {
    throw registry_error{exists ? registry_error_code::wrong_member_type : registry_error_code::missing_member,
                         exists ? "Registry path has a different type" : "Registry member path is missing"};
  }
  return std::move(*result);
}

// Traverse ordinary values only; entity/container boundaries need their own typed editor selection.
template <typename Descriptors, typename Storage, typename Value>
void write_registry_path(Storage& storage, std::span<const std::uint32_t> path, Value value) {
  if (path.empty()) { throw registry_error{registry_error_code::missing_member, "Empty registry field path"}; }
  bool changed{};
  bool exists{};
  Descriptors::visit([&](const auto& descriptor) {
    if (descriptor.info.id != path.front()) { return; }
    exists = true;
    using field_type = typename std::remove_cvref_t<decltype(descriptor)>::value_type;
    if (path.size() == 1) {
      if constexpr (std::same_as<Value, field_type>) {
        descriptor.write(storage, std::move(value));
        changed = true;
      }
    } else if constexpr (requires { value_fields<field_type>::visit([](const auto&) {}); }) {
      if (!descriptor.mutable_get) { throw registry_error{registry_error_code::missing_member, "Registry path is readonly"}; }
      changed = true;
      write_registry_path<value_fields<field_type>>(descriptor.mutable_get(storage), path.subspan(1), std::move(value));
    }
  });
  if (!changed) {
    throw registry_error{exists ? registry_error_code::wrong_member_type : registry_error_code::missing_member,
                         exists ? "Registry path has a different type" : "Registry member path is missing"};
  }
}
// Address identity is a local type check only; it is never persisted or used as a wire key.
template <typename Plain> inline constexpr unsigned char registry_type_token{};

// Adapt generated traversal while leaving existing two-argument identity visitors unchanged.
template <typename Callback>
struct registry_entity_visitor {
  Callback callback;
  // Carry the generated concrete type and live storage into one synchronous callback.
  template <typename Plain, typename Storage>
  void entity(Storage& storage) { callback.template operator()<Plain>(storage); }
};

// Visit identified occurrences through the generator's normal ownership boundaries.
template <typename Traits, typename Storage, typename Callback>
void visit_registry_entities(Storage& storage, Callback callback) {
  registry_entity_visitor<Callback> visitor{std::move(callback)};
  Traits::visit_entities(storage, visitor);
}

// Enforce generated type-wide scalar uniqueness before any candidate can be accepted.
// Index eviction cannot disable constraints; scratch maps are rebuilt and charged per validation.
template <typename Traits, typename Storage>
void validate_declared_unique_indexes(const Storage& root, const serializer::decode_limits& limits,
    const std::shared_ptr<resident_ledger>& ledger) {
  std::vector<const void*> visited;
  resident_charge charge{ledger};
  std::size_t work{};
  const auto consume_work = [&] {
    if (work >= limits.max_work_units) {
      throw std::length_error{"Schema index validation work limit exceeded"};
    }
    ++work;
  };
  visit_registry_entities<Traits>(root, [&]<typename Plain>(const auto&) {
    consume_work();
    const auto* token = &registry_type_token<Plain>;
    if (std::find(visited.begin(), visited.end(), token) != visited.end()) { return; }
    if (visited.size() >= limits.max_collection_elements) {
      throw std::length_error{"Schema index validation type budget exceeded"};
    }
    visited.push_back(token);
    const auto visited_bytes = multiply_memory_estimate(visited.capacity(), sizeof(const void*));
    if (visited_bytes > limits.max_allocation_bytes) {
      throw std::length_error{"Schema index validation allocation budget exceeded"};
    }
    charge.resize(visited_bytes);
    fields<Plain>::visit([&](const auto& descriptor) {
      using descriptor_type = std::remove_cvref_t<decltype(descriptor)>;
      if constexpr (requires { descriptor_type::unique; }) {
        if constexpr (descriptor_type::unique) {
          using key_type = typename descriptor_type::value_type;
          std::map<key_type, typename Traits::id_type> keys;
          std::size_t bytes = visited_bytes;
          visit_registry_entities<Traits>(root, [&]<typename Actual>(const auto& node) {
            consume_work();
            if constexpr (std::same_as<Plain, Actual>) {
              if (keys.size() >= limits.max_collection_elements) {
                throw std::length_error{"Schema unique index entity budget exceeded"};
              }
              const auto& key = descriptor.get(node);
              constexpr auto node_bytes = sizeof(typename decltype(keys)::value_type) + 6U * sizeof(void*);
              add_memory_estimate(bytes, node_bytes);
              add_memory_estimate(bytes, estimate_dynamic_memory(key));
              if (bytes > limits.max_allocation_bytes) {
                throw std::length_error{"Schema unique index allocation budget exceeded"};
              }
              charge.resize(bytes);
              if (!keys.emplace(key, node.persistent_id).second) {
                throw registry_error{registry_error_code::duplicate_key, "Duplicate schema unique index key"};
              }
            }
          });
          charge.resize(visited_bytes);
        }
      }
    });
  });
}

// Resolve anew after each structural edit rather than reusing a candidate pointer.
template <typename Plain, typename Traits, typename Root, typename Id>
auto& resolve_registry_entity(Root& root, Id id) {
  using storage_type = typename model_traits<Plain>::storage_type;
  storage_type* result{};
  bool exists{};
  visit_registry_entities<Traits>(root, [&]<typename Actual>(auto& storage) {
    if (storage.persistent_id != id) { return; }
    exists = true;
    if constexpr (std::same_as<Plain, Actual>) { result = std::addressof(storage); }
  });
  if (!result) {
    throw registry_error{exists ? registry_error_code::wrong_type : registry_error_code::missing_id,
                         exists ? "Registry entity has a different type" : "Registry entity is missing"};
  }
  return *result;
}
// Validate a complete candidate once so forward/cyclic target edges do not depend on decode order.
// The selected models pay for one bounded derived catalog, with expected O(entities + references) work.
template <typename Traits, typename Storage>
void validate_entity_references(const Storage& storage, document_id document,
    const serializer::decode_limits& limits, const std::shared_ptr<resident_ledger>& ledger) {
  using id_type = typename Traits::id_type;
  struct target { std::uint64_t class_id; const void* type_token; };
  std::unordered_map<id_type, target> targets;
  constexpr auto node_bytes = sizeof(typename decltype(targets)::value_type) + 8U * sizeof(void*);
  resident_charge charge{ledger};
  std::size_t work{};
  // Bound both traversal passes independently of target graph shape.
  const auto consume_work = [&] {
    if (work >= limits.max_work_units) { throw std::length_error{"Managed reference validation work limit exceeded"}; }
    ++work;
  };
  visit_registry_entities<Traits>(storage, [&]<typename Plain>(const auto& node) {
    consume_work();
    if (targets.size() >= limits.max_collection_elements ||
        targets.size() >= limits.max_allocation_bytes / node_bytes) {
      throw std::length_error{"Managed reference catalog budget exceeded"};
    }
    charge.resize(multiply_memory_estimate(targets.size() + 1U, node_bytes));
    if (node.persistent_id == 0U || !targets.emplace(node.persistent_id,
        target{model_traits<Plain>::class_id, &registry_type_token<Plain>}).second) {
      throw registry_error{registry_error_code::invalid_identity, "Invalid managed reference target identity"};
    }
  });
  Traits::visit_references(storage, [&](const auto& reference) {
    consume_work();
    reference.validate_envelope();
    if (reference.document().high != document.high || reference.document().low != document.low) {
      throw registry_error{registry_error_code::foreign_document, "Managed reference belongs to another document"};
    }
    using plain_type = typename std::remove_cvref_t<decltype(reference)>::target_type;
    const auto found = targets.find(reference.id());
    if (found == targets.end()) {
      throw registry_error{registry_error_code::missing_id, "Managed reference target is not live in the candidate"};
    }
    if (found->second.class_id != reference.class_id() ||
        found->second.type_token != &registry_type_token<plain_type>) {
      throw registry_error{registry_error_code::wrong_type, "Managed reference target has a different exact type"};
    }
  });
}

} // namespace detail

// Own one reusable exact-type ordinary-member path for binding and secondary indexing.
template <typename Plain, typename Value>
class member_path_descriptor {
  std::vector<std::uint32_t> path_;
public:
  using plain_type = Plain;
  using storage_type = typename model_traits<Plain>::storage_type;
  using value_type = Value;
  // Keep schema IDs alive independently of the caller's temporary path container.
  explicit member_path_descriptor(std::span<const std::uint32_t> path)
      : path_{path.begin(), path.end()} {
    if (path_.empty()) {
      throw registry_error{registry_error_code::missing_member, "Empty registry field path"};
    }
  }
  // Read the exact selected ordinary value through generated field/base projections.
  Value read(const storage_type& storage) const {
    return detail::read_registry_path<fields<Plain>, storage_type, Value>(storage, path_);
  }
  // Bind through the same generated mutability and exact-type checks as direct members.
  void write(storage_type& storage, Value value) const {
    detail::write_registry_path<fields<Plain>>(storage, path_, std::move(value));
  }
};

// Create a reusable path descriptor without retaining initializer-list storage.
template <typename Plain, typename Value>
auto field_path(std::initializer_list<std::uint32_t> path) {
  return member_path_descriptor<Plain, Value>{{path.begin(), path.size()}};
}

// Pins one immutable entity and its entire owning snapshot across store publication.
template <typename Plain>
class entity_read {
  using storage_type = typename model_traits<Plain>::storage_type;
  std::shared_ptr<const storage_type> storage_;
  document_id document_{};
public:
  // Alias the selected snapshot; ownership keeps storage valid across undo/load/commit.
  explicit entity_read(std::shared_ptr<const storage_type> storage, document_id document = {})
      : storage_{std::move(storage)}, document_{document} {
    if (!storage_) { throw std::invalid_argument{"Entity read needs a snapshot pin"}; }
  }
  // Borrow immutable storage while this handle or another snapshot pin remains alive.
  const storage_type& storage() const noexcept { return *storage_; }
  // Read the generated payload through the same representation binding as editors.
  const auto& value() const noexcept { return model_traits<Plain>::payload(*storage_); }
  // Read its stable document-local identity independently of application map keys.
  auto id() const noexcept { return storage_->persistent_id; }
  // Return the exact typed identity of this pinned occurrence; ownerless legacy handles reject.
  auto get_id() const { return managed_ref<Plain, typename model_traits<Plain>::id_type>{document_, id()}; }
};

// Bound derived catalog construction separately from payload/decode budgets.
struct registry_options {
  std::size_t max_entities{std::numeric_limits<std::size_t>::max()};
  std::size_t max_index_bytes{std::numeric_limits<std::size_t>::max()};
};

// A match range owns immutable entity pins and its selected derived index independently of a view.
template <typename Plain>
class match_range {
  std::vector<entity_read<Plain>> values_;
  std::shared_ptr<const void> index_;
  std::shared_ptr<detail::resident_charge> charge_;
public:
  // Transfer fully resolved handles; a temporary registry or subsequent publication cannot invalidate them.
  match_range(std::vector<entity_read<Plain>> values, std::shared_ptr<const void> index,
      std::shared_ptr<detail::resident_charge> charge)
      : values_{std::move(values)}, index_{std::move(index)}, charge_{std::move(charge)} {}
  // Begin immutable entity traversal in stable persistent-ID order within an equal key.
  auto begin() const noexcept { return values_.begin(); }
  // Mark the end of this snapshot's matching entities.
  auto end() const noexcept { return values_.end(); }
  // Report whether the exact equality query matched no entities.
  bool empty() const noexcept { return values_.empty(); }
  // Count the bounded materialized matches.
  std::size_t size() const noexcept { return values_.size(); }
};

// A secondary index pins its source snapshot and accounts its derived owned capacity.
template <typename Root, typename Map>
class secondary_index {
  std::shared_ptr<const Root> root_;
  document_id document_{};
  Map values_;
  std::shared_ptr<detail::resident_charge> charge_{};
public:
  // Bind copied keys/IDs to their immutable owning generation, never to a live mutable catalog.
  secondary_index(std::shared_ptr<const Root> root, document_id document, Map values,
                  std::shared_ptr<detail::resident_charge> charge)
      : root_{std::move(root)}, document_{document}, values_{std::move(values)}, charge_{std::move(charge)} {}
  secondary_index(const secondary_index&) = delete;
  secondary_index& operator=(const secondary_index&) = delete;
  secondary_index(secondary_index&&) = default;
  secondary_index& operator=(secondary_index&&) = default;
  // Read a selected key's matching stable IDs; missing keys retain map's checked failure.
  const auto& at(const typename Map::key_type& key) const { return values_.at(key); }
  // Look up one key without changing membership or pin lifetime.
  auto find(const typename Map::key_type& key) const { return values_.find(key); }
  // Begin immutable key/ID traversal in the declared ordered/hash representation.
  auto begin() const noexcept { return values_.begin(); }
  // Mark the end of immutable traversal.
  auto end() const noexcept { return values_.end(); }
  // Report distinct secondary keys, rather than the number of objects.
  std::size_t size() const noexcept { return values_.size(); }
  // Start an ordered range query; hash indexes intentionally do not expose this operation.
  auto lower_bound(const typename Map::key_type& key) const
    requires requires(const Map& value) { value.lower_bound(key); }
  { return values_.lower_bound(key); }
  // Inspect the namespace which owns every indexed identity.
  document_id document() const noexcept { return document_; }
  // Retain the exact root to resolve returned IDs through a compatible snapshot catalog.
  const auto& snapshot() const noexcept { return root_; }
  // Estimate owned map/key/ID capacity for aggregate automatic-cache admission.
  std::size_t storage_bytes() const { return detail::estimate_dynamic_memory(values_); }
};

// A rebuildable catalog belongs to exactly one immutable root; default stores allocate none.
template <typename Root, typename Traits = model_traits<Root>>
class registry_view {
public:
  using storage_type = typename Traits::storage_type;
  using id_type = typename Traits::id_type;
private:
  static constexpr std::size_t catalog_node_overhead_bytes = 6 * sizeof(void*);
  struct location {
    const void* pointer{};
    const void* type_token{};
    std::string_view type_key{};
  };
  std::shared_ptr<const storage_type> root_;
  document_id document_{};
  std::map<id_type, location> locations_{};
  registry_options options_{};
  std::shared_ptr<detail::resident_charge> charge_{};
  std::shared_ptr<detail::resident_ledger> ledger_{};
  struct declared_index_cache {
    std::mutex mutex{};
    std::map<std::pair<const void*, std::uint32_t>, std::shared_ptr<const void>> indexes{};
    std::size_t bytes{};
  };
  std::shared_ptr<declared_index_cache> declared_indexes_{std::make_shared<declared_index_cache>()};

  // Admit derived capacity without claiming a hard allocator or process peak ceiling.
  void admit(std::size_t bytes) {
    if (bytes > options_.max_index_bytes) {
      throw std::length_error{"Registry index admission budget exceeded"};
    }
    if (charge_) { charge_->resize(bytes); }
  }

  // Reuse one declared access path in this immutable registry; failed builds publish no cache entry.
  template <typename Descriptor>
  auto declared_index(const Descriptor& descriptor) const {
    if (!root_) { throw std::logic_error{"Registry snapshot handle was moved"}; }
    // Immutable snapshots may be queried concurrently; serialize cache construction and admission.
    const std::lock_guard lock{declared_indexes_->mutex};
    using plain_type = typename Descriptor::plain_type;
    const auto identity = std::make_pair(static_cast<const void*>(&detail::registry_type_token<Descriptor>),
        descriptor.info.id);
    const auto build = [&] {
      if constexpr (Descriptor::method == index_method::ordered) {
        return ordered(descriptor, Descriptor::unique);
      } else {
        return hashed(descriptor, Descriptor::unique);
      }
    };
    using index_type = decltype(build());
    const auto found = declared_indexes_->indexes.find(identity);
    if (found != declared_indexes_->indexes.end()) {
      return std::static_pointer_cast<const index_type>(found->second);
    }
    static_assert(std::same_as<typename Descriptor::storage_type,
        typename model_traits<plain_type>::storage_type>);
    auto index = std::make_shared<const index_type>(build());
    auto bytes = declared_indexes_->bytes;
    detail::add_memory_estimate(bytes, index->storage_bytes());
    if (bytes > options_.max_index_bytes) {
      throw std::length_error{"Declared schema index cache budget exceeded"};
    }
    declared_indexes_->indexes.emplace(identity, index);
    declared_indexes_->bytes = bytes;
    return index;
  }
public:
  // Build only from a pinned generated snapshot; duplicate/zero identities reject construction.
  explicit registry_view(std::shared_ptr<const storage_type> root, registry_options options = {},
                         std::shared_ptr<detail::resident_ledger> ledger = {}, document_id document = {})
      : root_{std::move(root)}, document_{document}, options_{options} {
    if (!root_) { throw std::invalid_argument{"Registry needs a snapshot pin"}; }
    ledger_ = std::move(ledger);
    if (ledger_) { charge_ = std::make_shared<detail::resident_charge>(ledger_); }
    detail::visit_registry_entities<Traits>(*root_, [&]<typename Plain>(const auto& storage) {
      if (locations_.size() >= options_.max_entities) {
        throw std::length_error{"Registry entity budget exceeded"};
      }
      const auto next = detail::multiply_memory_estimate(locations_.size() + 1,
          sizeof(typename decltype(locations_)::value_type) + catalog_node_overhead_bytes);
      admit(next);
      const location entry{std::addressof(storage), &detail::registry_type_token<Plain>,
                           model_traits<Plain>::type_key};
      if (storage.persistent_id == 0 || !locations_.emplace(storage.persistent_id, entry).second) {
        throw registry_error{registry_error_code::invalid_identity, "Invalid registry identity"};
      }
    });
  }
  registry_view(const registry_view&) = delete;
  registry_view& operator=(const registry_view&) = delete;
  registry_view(registry_view&&) = default;
  registry_view& operator=(registry_view&&) = default;

  // Query membership without conferring mutation or authorization rights.
  bool contains(id_type id) const noexcept { return locations_.contains(id); }
  // Preserve the selected document namespace even when the store later loads another document.
  document_id document() const noexcept { return document_; }
  // Count only independently managed occurrences in this selected snapshot.
  std::size_t size() const noexcept { return locations_.size(); }
  // Inspect a generated schema type key, never a native RTTI spelling.
  std::string_view type(id_type id) const {
    const auto found = locations_.find(id);
    if (found == locations_.end()) {
      throw registry_error{registry_error_code::missing_id, "Registry entity is missing"};
    }
    return found->second.type_key;
  }
  // Resolve in logarithmic catalog time and pin the exact selected occurrence.
  template <typename Plain>
  entity_read<Plain> find(id_type id) const {
    if (!root_) { throw std::logic_error{"Registry snapshot handle was moved"}; }
    const auto found = locations_.find(id);
    if (found == locations_.end()) {
      throw registry_error{registry_error_code::missing_id, "Registry entity is missing"};
    }
    if (found->second.type_token != &detail::registry_type_token<Plain>) {
      throw registry_error{registry_error_code::wrong_type, "Registry entity has a different type"};
    }
    using entity_storage = typename model_traits<Plain>::storage_type;
    return entity_read<Plain>{std::shared_ptr<const entity_storage>{
        root_, static_cast<const entity_storage*>(found->second.pointer)}, document_};
  }
  // Create an exact typed token only after checking membership in this pinned document snapshot.
  template <typename Plain>
  managed_ref<Plain, id_type> reference(id_type id) const {
    static_cast<void>(find<Plain>(id));
    return managed_ref<Plain, id_type>{document_, id};
  }
  // Reject foreign document/type metadata before lookup, then pin the exact live occurrence.
  template <typename Plain>
  entity_read<Plain> resolve(const managed_ref<Plain, id_type>& reference) const {
    reference.validate_envelope();
    if (reference.document().high != document_.high || reference.document().low != document_.low) {
      throw registry_error{registry_error_code::foreign_document, "Managed reference belongs to another document"};
    }
    return find<Plain>(reference.id());
  }
  // Inspect the catalog synchronously without exposing its mutable membership table.
  template <typename Callback>
  void visit(Callback&& callback) const {
    for (const auto& [id, entry] : locations_) { callback(id, entry.type_key); }
  }
  // Resolve exact scalar matches through the declared method and retain both snapshot and index pins.
  template <typename Descriptor>
    requires requires { Descriptor::method; Descriptor::unique; }
  auto find_by(const Descriptor& descriptor, const typename Descriptor::value_type& key) const {
    using plain_type = typename Descriptor::plain_type;
    auto index = declared_index(descriptor);
    std::vector<entity_read<plain_type>> matches;
    const auto found = index->find(key);
    if (found != index->end()) {
      const auto bytes = detail::multiply_memory_estimate(found->second.size(), sizeof(entity_read<plain_type>));
      if (bytes > options_.max_index_bytes) {
        throw std::length_error{"Schema index match range budget exceeded"};
      }
      matches.reserve(found->second.size());
      for (const auto id : found->second) { matches.push_back(find<plain_type>(id)); }
    }
    const auto bytes = detail::multiply_memory_estimate(matches.capacity(), sizeof(entity_read<plain_type>));
    auto charge = ledger_ ? std::make_shared<detail::resident_charge>(ledger_, bytes) : nullptr;
    return match_range<plain_type>{std::move(matches), std::move(index), std::move(charge)};
  }
  // Require a declared exact unique key; missing values return an empty optional.
  template <typename Descriptor>
    requires requires { Descriptor::unique; } && Descriptor::unique
  auto find_unique(const Descriptor& descriptor, const typename Descriptor::value_type& key) const {
    using plain_type = typename Descriptor::plain_type;
    auto index = declared_index(descriptor);
    const auto found = index->find(key);
    if (found == index->end()) { return std::optional<entity_read<plain_type>>{}; }
    return std::optional<entity_read<plain_type>>{find<plain_type>(found->second.front())};
  }
  // Test equality membership without allocating a match range or exposing mutable entities.
  template <typename Descriptor>
    requires requires { Descriptor::method; Descriptor::unique; }
  bool contains_by(const Descriptor& descriptor, const typename Descriptor::value_type& key) const {
    const auto index = declared_index(descriptor);
    return index->find(key) != index->end();
  }
  // Read a statically selected exact field through generated descriptor access.
  template <typename Plain, typename Storage, typename Value>
  Value member(id_type id, const member_descriptor<Plain, Storage, Value>& descriptor) const {
    return descriptor.read(find<Plain>(id).storage());
  }
  // Resolve a numeric stable field ID with an exact expected value type.
  template <typename Plain, typename Value>
  Value member(id_type id, std::uint32_t field_id) const {
    const auto entity = find<Plain>(id);
    std::optional<Value> result;
    bool exists{};
    fields<Plain>::visit([&](const auto& descriptor) {
      if (descriptor.info.id != field_id) { return; }
      exists = true;
      if constexpr (std::same_as<Value, typename std::remove_cvref_t<decltype(descriptor)>::value_type>) {
        result.emplace(descriptor.read(entity.storage()));
      }
    });
    if (!result) {
      throw registry_error{exists ? registry_error_code::wrong_member_type : registry_error_code::missing_member,
          exists ? "Registry member has a different type" : "Registry member is missing"};
    }
    return std::move(*result);
  }
  // Resolve nested ordinary members and bases using their stable schema identity path.
  template <typename Plain, typename Value>
  Value member(id_type id, std::span<const std::uint32_t> path) const {
    return detail::read_registry_path<fields<Plain>, typename model_traits<Plain>::storage_type, Value>(
        find<Plain>(id).storage(), path);
  }
  // Produce an ordered snapshot-local secondary index; composite scope keys remain application data.
  template <typename Descriptor, typename Key = typename Descriptor::value_type,
            typename Compare = std::less<Key>>
  auto ordered(const Descriptor& descriptor, bool unique = false,
               Compare compare = {}) const {
    using Plain = typename Descriptor::plain_type;
    using Storage = typename Descriptor::storage_type;
    static_assert(std::same_as<Storage, typename model_traits<Plain>::storage_type>,
                  "Registry descriptors must match the generated entity storage");
    std::map<Key, std::vector<id_type>, Compare> result{std::move(compare)};
    for (const auto& [id, entry] : locations_) {
      if (entry.type_token != &detail::registry_type_token<Plain>) { continue; }
      auto key = descriptor.read(*static_cast<const Storage*>(entry.pointer));
      auto& members = result[std::move(key)];
      if (unique && !members.empty()) {
        throw registry_error{registry_error_code::duplicate_key, "Duplicate registry key"};
      }
      members.push_back(id);
    }
    const auto bytes = detail::estimate_dynamic_memory(result);
    if (bytes > options_.max_index_bytes) { throw std::length_error{"Registry secondary index budget exceeded"}; }
    auto charge = ledger_ ? std::make_shared<detail::resident_charge>(ledger_, bytes) : nullptr;
    return secondary_index<storage_type, decltype(result)>{root_, document_, std::move(result), std::move(charge)};
  }
  // Produce a hash index with caller-selected key hash/equality and expected constant-time lookup.
  template <typename Descriptor, typename Key = typename Descriptor::value_type, typename Hash = std::hash<Key>,
            typename Equal = std::equal_to<Key>>
  auto hashed(const Descriptor& descriptor, bool unique = false,
              Hash hash = {}, Equal equal = {}) const {
    using Plain = typename Descriptor::plain_type;
    using Storage = typename Descriptor::storage_type;
    static_assert(std::same_as<Storage, typename model_traits<Plain>::storage_type>,
                  "Registry descriptors must match the generated entity storage");
    std::unordered_map<Key, std::vector<id_type>, Hash, Equal> result{0, std::move(hash), std::move(equal)};
    for (const auto& [id, entry] : locations_) {
      if (entry.type_token != &detail::registry_type_token<Plain>) { continue; }
      auto& members = result[descriptor.read(*static_cast<const Storage*>(entry.pointer))];
      if (unique && !members.empty()) {
        throw registry_error{registry_error_code::duplicate_key, "Duplicate registry key"};
      }
      members.push_back(id);
    }
    const auto bytes = detail::estimate_dynamic_memory(result);
    if (bytes > options_.max_index_bytes) { throw std::length_error{"Registry secondary index budget exceeded"}; }
    auto charge = ledger_ ? std::make_shared<detail::resident_charge>(ledger_, bytes) : nullptr;
    return secondary_index<storage_type, decltype(result)>{root_, document_, std::move(result), std::move(charge)};
  }
};

// A callback-scoped factory for required/self/cyclic edges after native IDs are assigned.
// It lends no entity pointers and cannot outlive the staged storage owned by the constructor.
template <typename Root, typename Traits = model_traits<Root>>
class reference_builder {
  using storage_type = typename Traits::storage_type;
  using id_type = typename Traits::id_type;
  registry_view<Root, Traits> registry_;
public:
  // The no-op pin is private: only value tokens can escape this synchronous initialization scope.
  reference_builder(const storage_type& storage, document_id document, registry_options options = {},
                    std::shared_ptr<detail::resident_ledger> ledger = {})
      : registry_{std::shared_ptr<const storage_type>{std::addressof(storage), [](const auto*) {}},
                  options, std::move(ledger), document} {}
  reference_builder(const reference_builder&) = delete;
  reference_builder& operator=(const reference_builder&) = delete;
  // Build only an exact live token from the complete set of initial native identities.
  template <typename Plain>
  managed_ref<Plain, id_type> reference(id_type id) const { return registry_.template reference<Plain>(id); }
};

// An edit catalog holds only an invalidatable channel; each operation resolves the current candidate.
template <typename Root, typename Traits = model_traits<Root>>
class registry_edit {
  using storage_type = typename Traits::storage_type;
  using id_type = typename Traits::id_type;
  std::shared_ptr<detail::edit_channel<storage_type, id_type>> channel_;
  document_id document_{};
  // Convert only registry lifetime failures to typed errors; normal editor contracts stay unchanged.
  void require_active() const {
    if (!channel_ || !channel_->owner) {
      throw registry_error{registry_error_code::expired_editor, "Registry transaction is closed"};
    }
  }
  // Reassign every owning ID first, then rewrite only schema-declared tokens in a second pass.
  // Source-document external edges need a typed resolver; destination-document external edges retain identity.
  template <typename Plain, typename Imported, typename External>
  void prepare_import(Imported& imported, id_type root_id, document_id source,
                      bool require_saved_ids, External&& external) const {
    using imported_traits = model_traits<Plain>;
    if constexpr (requires { imported_traits::has_entity_references; }) {
      if constexpr (imported_traits::has_entity_references) {
        struct mapped_identity { id_type id; const void* type_token; };
        std::map<id_type, mapped_identity> remapping;
        const serializer::decode_limits bounds{};
        constexpr auto mapping_node_bytes = sizeof(typename decltype(remapping)::value_type) + 6U * sizeof(void*);
        detail::visit_registry_entities<imported_traits>(imported, [&]<typename Actual>(auto& node) {
          const auto old_id = node.persistent_id;
          if (require_saved_ids && old_id == 0U) {
            throw registry_error{registry_error_code::invalid_identity, "Imported managed storage requires assigned source IDs"};
          }
          const auto replacement = static_cast<const void*>(std::addressof(node)) ==
              static_cast<const void*>(std::addressof(imported)) ? root_id : reserve();
          if (old_id != 0U) {
            if (remapping.size() >= bounds.max_collection_elements ||
                remapping.size() >= bounds.max_allocation_bytes / mapping_node_bytes) {
              throw std::length_error{"Managed reference import mapping budget exceeded"};
            }
            if (!remapping.emplace(old_id,
                mapped_identity{replacement, &detail::registry_type_token<Actual>}).second) {
              throw registry_error{registry_error_code::invalid_identity, "Imported source IDs are duplicated"};
            }
          }
          node.persistent_id = replacement;
        });
        imported_traits::visit_references(imported, [&](auto& reference) {
          const auto scope = reference.document();
          // New values may be linked after attachment in the same transaction; imported saved
          // storage still requires complete envelopes, and publication validates every required edge.
          if (!require_saved_ids && scope.high == 0U && scope.low == 0U &&
              reference.class_id() == 0U && reference.id() == 0U) { return; }
          reference.validate_envelope();
          using reference_type = std::remove_cvref_t<decltype(reference)>;
          using target_type = typename reference_type::target_type;
          if (scope.high == source.high && scope.low == source.low) {
            const auto found = remapping.find(reference.id());
            if (found != remapping.end()) {
              if (found->second.type_token != &detail::registry_type_token<target_type>) {
                throw registry_error{registry_error_code::wrong_type, "Imported reference target has a different exact type"};
              }
              reference = reference_type{document_, found->second.id};
              return;
            }
          }
          if (scope.high == document_.high && scope.low == document_.low) { return; }
          auto replacement = std::invoke(external, std::as_const(reference));
          static_assert(std::same_as<decltype(replacement), reference_type>,
              "External managed reference mappings must preserve the exact target type");
          replacement.validate_envelope();
          if (replacement.document().high != document_.high || replacement.document().low != document_.low) {
            throw registry_error{registry_error_code::foreign_document, "Imported external mapping belongs to another document"};
          }
          reference = std::move(replacement);
        });
        return;
      }
    }
    detail::visit_registry_entities<imported_traits>(imported, [&]<typename Actual>(auto& node) {
      static_cast<void>(sizeof(Actual));
      node.persistent_id = static_cast<const void*>(std::addressof(node)) ==
          static_cast<const void*>(std::addressof(imported)) ? root_id : reserve();
    });
  }
public:
  // Share the normal transaction lifetime/failure channel, without owning its mutable candidate.
  explicit registry_edit(std::shared_ptr<detail::edit_channel<storage_type, id_type>> channel,
                         document_id document = {})
      : channel_{std::move(channel)}, document_{document} {}
  // Create tokens for existing candidate entities without exposing an unchecked integer constructor.
  template <typename Plain>
  managed_ref<Plain, id_type> reference(id_type id) const {
    require_active();
    channel_->guard([&] {
      channel_->update([&](storage_type& root) {
        static_cast<void>(detail::resolve_registry_entity<Plain, Traits>(root, id));
      });
    });
    return managed_ref<Plain, id_type>{document_, id};
  }
  // Resolve a scoped token through the same invalidatable candidate editor contract.
  template <typename Plain>
  auto resolve(const managed_ref<Plain, id_type>& reference) const {
    require_active();
    channel_->guard([&] {
      reference.validate_envelope();
      if (reference.document().high != document_.high || reference.document().low != document_.low) {
        throw registry_error{registry_error_code::foreign_document, "Managed reference belongs to another document"};
      }
    });
    return edit<Plain>(reference.id());
  }
  // Select a generated editor which revalidates identity/type after every structural mutation.
  template <typename Plain>
  auto edit(id_type id) const {
    require_active();
    channel_->guard([&] {
      channel_->update([&](storage_type& root) { detail::resolve_registry_entity<Plain, Traits>(root, id); });
    });
    using entity_storage = typename model_traits<Plain>::storage_type;
    auto resolve = [id](storage_type& root) -> entity_storage& {
      return detail::resolve_registry_entity<Plain, Traits>(root, id);
    };
    return model_traits<Plain>::make_editor(
        detail::editor_access<storage_type, entity_storage, true, decltype(resolve)>{channel_, resolve});
  }
  // Bind/rebind a durable application key or other ordinary field within the same transaction.
  template <typename Plain, typename Storage, typename Value>
  void bind(id_type id, const member_descriptor<Plain, Storage, Value>& descriptor, Value value) const {
    require_active();
    channel_->guard([&] {
      channel_->update([&](storage_type& root) {
        descriptor.write(detail::resolve_registry_entity<Plain, Traits>(root, id), std::move(value));
      });
    });
  }
  // Apply a nested ordinary-field binding through the existing candidate mutation boundary.
  template <typename Plain, typename Value>
  void bind(id_type id, std::span<const std::uint32_t> path, Value value) const {
    require_active();
    channel_->guard([&] {
      channel_->update([&](storage_type& root) {
        detail::write_registry_path<fields<Plain>>(detail::resolve_registry_entity<Plain, Traits>(root, id), path, std::move(value));
      });
    });
  }
  // Reserve through the existing flush-before-exposure allocator; cancellation never recycles IDs.
  id_type reserve() const {
    require_active();
    return channel_->guard([&] {
      const auto id = channel_->allocate(channel_->owner);
      if (!channel_->registry_reservations) {
        channel_->registry_reservations = std::make_unique<std::vector<id_type>>();
      }
      channel_->registry_reservations->push_back(id);
      return id;
    });
  }
  // Query current candidate membership without keeping addresses across structural changes.
  bool contains(id_type id) const {
    require_active();
    bool result{};
    channel_->update([&](storage_type& root) {
      detail::visit_registry_entities<Traits>(root, [&]<typename Plain>(const auto& node) {
        static_cast<void>(sizeof(Plain));
        result = result || node.persistent_id == id;
      });
    });
    return result;
  }
  // Adopt values under fresh IDs; only declared internal managed tokens are remapped.
  template <typename Plain, typename Attach>
  id_type adopt(Plain value, Attach attach) const {
    const auto id = reserve();
    return adopt_reserved(id, std::move(value), std::move(attach));
  }
  // Adopt a caller-selected reservation from this active transaction, rejecting retired/foreign IDs.
  // The trusted attachment callback changes owning model state, never an independent catalog.
  template <typename Plain, typename Attach>
  id_type adopt_reserved(id_type id, Plain value, Attach attach) const {
    require_active();
    return channel_->guard([&] {
      auto* reservations = channel_->registry_reservations.get();
      if (!reservations || std::find(reservations->begin(), reservations->end(), id) == reservations->end() || contains(id)) {
        throw registry_error{registry_error_code::invalid_identity, "Registry ID is not an unused transaction reservation"};
      }
      // Consume before initialization/attachment; retirement never makes this reservation reusable.
      std::erase(*reservations, id);
      auto imported = model_traits<Plain>::make_storage(std::move(value));
      prepare_import<Plain>(imported, id, document_, false,
          [](const auto& reference) -> std::remove_cvref_t<decltype(reference)> {
            throw registry_error{registry_error_code::foreign_document, "Foreign managed references require explicit import mapping"};
          });
      channel_->update([&](storage_type& root) {
        std::invoke(attach, root, std::move(imported));
        // Require the imported occurrence to become owned model state before accepting adoption.
        static_cast<void>(detail::resolve_registry_entity<Plain, Traits>(root, id));
      });
      return id;
    });
  }
  // Import identified storage in either native layout; the optional typed mapper handles external edges.
  template <typename Plain, typename Attach, typename External>
  id_type import_storage(document_id source, typename model_traits<Plain>::storage_type imported,
                         Attach attach, External external) const {
    require_active();
    return channel_->guard([&] {
      if (source.high == 0U && source.low == 0U) {
        throw registry_error{registry_error_code::invalid_identity, "Import needs the source document namespace"};
      }
      const auto id = reserve();
      std::erase(*channel_->registry_reservations, id);
      prepare_import<Plain>(imported, id, source, true, external);
      channel_->update([&](storage_type& root) {
        std::invoke(attach, root, std::move(imported));
        static_cast<void>(detail::resolve_registry_entity<Plain, Traits>(root, id));
      });
      return id;
    });
  }
  // Reject external source-document references unless the caller supplies an explicit typed mapping.
  template <typename Plain, typename Attach>
  id_type import_storage(document_id source, typename model_traits<Plain>::storage_type imported,
                         Attach attach) const {
    return import_storage<Plain>(source, std::move(imported), std::move(attach),
        [](const auto& reference) -> std::remove_cvref_t<decltype(reference)> {
          throw registry_error{registry_error_code::foreign_document, "Imported external reference needs explicit mapping"};
        });
  }
  // Retire an occurrence through a trusted owning-container edit; its logical ID remains consumed.
  template <typename Plain, typename Remove>
  void retire(id_type id, Remove remove) const {
    require_active();
    channel_->guard([&] {
      channel_->update([&](storage_type& root) {
        static_cast<void>(detail::resolve_registry_entity<Plain, Traits>(root, id));
        std::invoke(remove, root, id);
        bool remains{};
        detail::visit_registry_entities<Traits>(root, [&]<typename Actual>(const auto& node) {
          static_cast<void>(sizeof(Actual));
          remains = remains || node.persistent_id == id;
        });
        if (remains) { throw registry_error{registry_error_code::invalid_identity, "Registry retirement retained its entity"}; }
      });
    });
  }
};

// Typed external resolution returns owning immutable handles under the provider's lifetime contract.
// Provider state is not made undoable or authorized by storing its key in a Serializer model.
template <typename Key, typename Value>
class external_registry {
  std::function<std::shared_ptr<const Value>(const Key&)> resolve_;
public:
  // The provider owns synchronization, authentication, version selection and external transactions.
  explicit external_registry(std::function<std::shared_ptr<const Value>(const Key&)> resolve)
      : resolve_{std::move(resolve)} {
    if (!resolve_) { throw std::invalid_argument{"External registry needs a resolver"}; }
  }
  // Acquire a scoped owning handle; missing keys produce the same typed lookup failure.
  std::shared_ptr<const Value> find(const Key& key) const {
    auto result = resolve_(key);
    if (!result) { throw registry_error{registry_error_code::missing_id, "External registry key is missing"}; }
    return result;
  }
};

// A provider version admits immutable borrowed reads until explicitly invalidated.
// Invalidation is thread-safe; it never releases the storage or synchronization guards in leases.
class external_lease_state {
  const std::uint64_t version_;
  std::atomic_bool valid_{true};
public:
  // Reserve zero for absent versions; providers must not reuse a version for changed payloads.
  explicit external_lease_state(std::uint64_t version) : version_{version} {
    if (version_ == 0) {
      throw std::invalid_argument{"External lease needs a nonzero provider version"};
    }
  }
  // Inspect the immutable provider version, including after invalidation.
  std::uint64_t version() const noexcept { return version_; }
  // Observe whether this version still admits new checked reads.
  bool is_valid() const noexcept { return valid_.load(std::memory_order_acquire); }
  // Revoke future checked reads without reclaiming payloads retained by outstanding leases.
  void invalidate() noexcept { valid_.store(false, std::memory_order_release); }
};

// A borrowed immutable value retains a provider-owned storage/synchronization token.
// The provider must keep the referent alive and unchanged until the last guard is released,
// including after invalidation. Native lock guards may impose thread-affinity requirements.
template <typename Value>
class external_lease {
  const Value* value_;
  std::shared_ptr<const void> guard_;
  std::shared_ptr<const external_lease_state> state_;
public:
  // Bind a referent only with owning guard/state tokens; ownerless aliases cannot form a lease.
  external_lease(const Value& value, std::shared_ptr<const void> guard,
                 std::shared_ptr<const external_lease_state> state)
      : value_{std::addressof(value)}, guard_{std::move(guard)}, state_{std::move(state)} {
    if (!guard_ || guard_.use_count() == 0 || !state_ || state_.use_count() == 0) {
      throw std::invalid_argument{"External lease needs owning guard and version tokens"};
    }
  }
  // A temporary payload cannot remain alive merely because an unrelated guard is retained.
  external_lease(const Value&& value, std::shared_ptr<const void> guard,
                 std::shared_ptr<const external_lease_state> state) = delete;
  // Inspect validity without borrowing the payload; moved-from handles are invalid.
  bool is_valid() const noexcept { return guard_ && state_ && state_->is_valid(); }
  // Keep version metadata available after invalidation; moved-from handles have no version.
  std::uint64_t version() const {
    if (!state_) {
      throw registry_error{registry_error_code::expired_lease, "External lease is expired"};
    }
    return state_->version();
  }
  // Borrow only through a live handle; invalidation rejects subsequent checked access.
  // References remain safe only while a guard-retaining copy of this lease remains alive.
  const Value& value() const & {
    if (!is_valid()) {
      throw registry_error{registry_error_code::expired_lease, "External lease is expired"};
    }
    return *value_;
  }
  // Prevent a reference from immediately outliving a temporary lease's guard.
  const Value& value() const && = delete;
};

// External borrowed lookup requires explicit immutable-version leases, never bare pointers.
// Provider changes remain outside managed transactions, history, authorization and journals.
template <typename Key, typename Value>
class leased_external_registry {
  std::function<std::optional<external_lease<Value>>(const Key&)> resolve_;
public:
  // The resolver must acquire its lifetime/synchronization guard before selecting the referent.
  explicit leased_external_registry(
      std::function<std::optional<external_lease<Value>>(const Key&)> resolve)
      : resolve_{std::move(resolve)} {
    if (!resolve_) {
      throw std::invalid_argument{"Leased external registry needs a resolver"};
    }
  }
  // Return a checked lease; absence and invalidated provider versions are distinct failures.
  external_lease<Value> find(const Key& key) const {
    auto result = resolve_(key);
    if (!result) {
      throw registry_error{registry_error_code::missing_id, "External registry key is missing"};
    }
    if (!result->is_valid()) {
      throw registry_error{registry_error_code::expired_lease, "External lease is expired"};
    }
    return std::move(*result);
  }
  // Require the selected provider version; a mismatch cannot silently return a newer value.
  external_lease<Value> find(const Key& key, std::uint64_t expected_version) const {
    auto result = find(key);
    if (result.version() != expected_version) {
      throw registry_error{registry_error_code::wrong_version, "External provider version differs"};
    }
    return result;
  }
};
} // namespace rohit::managed
