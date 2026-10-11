// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once
#include <rohit/managed_owned.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <exception>
#include <cstdint>
#include <limits>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rohit::managed {
template <typename Root>
struct model_traits;
// Generated memory-only projection; normal value copies retain runtime fields.
template <typename Value>
struct runtime_traits;

namespace detail {
// Defer trait lookup until generated specializations and the concrete access type are complete.
template <typename Plain, typename Access>
auto make_editor(Access access) {
  return model_traits<Plain>::make_editor(std::move(access));
}

// One invalidatable channel per edited transaction; handles never retain a transaction pointer.
template <typename Root, typename Id>
struct edit_channel {
  void* owner{};
  void (*mutate)(void*, void*, void (*)(void*, Root&)){};
  Id (*allocate)(void*){};
  void (*fail)(void*, std::exception_ptr) noexcept {};
  std::uint64_t variant_generation{}; // Candidate-local replacement epoch; never serialized.

  // Check lifetime before dereferencing the erased transaction owner.
  void require_active() const {
    if (!owner) {
      throw std::logic_error{"Managed editor transaction is closed"};
    }
  }

  // Forward a synchronous callable without allocating a std::function per mutation.
  template <typename Callable>
  void update(Callable&& callback) const {
    require_active();
    mutate(owner, std::addressof(callback), [](void* function, Root& root) {
      (*static_cast<std::remove_reference_t<Callable>*>(function))(root);
    });
  }

  // Poison preparation failures as well as failures inside an actual candidate mutation.
  template <typename Callable>
  decltype(auto) guard(Callable&& callback) const {
    require_active();
    try {
      return std::forward<Callable>(callback)();
    } catch (...) {
      if (owner) {
        fail(owner, std::current_exception());
      }
      throw;
    }
  }
};

template <typename Node, bool Managed>
struct editor_value {
  using type = Node;
};
template <typename Node>
struct editor_value<Node, true> {
  using type =
      std::remove_reference_t<decltype(model_traits<Node>::payload(std::declval<Node&>()))>;
};

// Resolve an owning path afresh on every call, avoiding dangling pointers after vector/map edits.
template <typename Root, typename Node, bool Managed, typename Resolve, bool Runtime = false>
class editor_access {
  using channel_type = edit_channel<Root, decltype(Root::persistent_id)>;
  std::shared_ptr<channel_type> channel_;
  Resolve resolve_;

  // Select the ordinary payload without exposing the identity field to generated setters.
  static auto& payload(Node& node) {
    if constexpr (Managed) {
      return model_traits<Node>::payload(node);
    } else {
      return node;
    }
  }

public:
  using value_type = typename editor_value<Node, Managed>::type;
  using id_type = decltype(Root::persistent_id);
  static constexpr bool is_managed = Managed;
  static constexpr bool is_runtime_access = Runtime;

  // Keep only the channel and an owning resolver; no mutable candidate references escape.
  editor_access(std::shared_ptr<channel_type> channel, Resolve resolve)
      : channel_{std::move(channel)}, resolve_{std::move(resolve)} {}

  // Read or mutate a payload synchronously under the transaction's failure boundary.
  template <typename Callable>
  void update(Callable&& callback) const {
    channel_->update(
        [&](Root& root) { std::forward<Callable>(callback)(payload(resolve_(root))); });
  }

  // Return a copy so callers cannot bypass the managed mutation boundary through a getter.
  value_type copy() const {
    std::optional<value_type> result;
    update([&](const auto& value) { result.emplace(value); });
    return std::move(*result);
  }

  // Return the immutable identity of this occurrence, not an application map key.
  id_type id() const
    requires Managed
  {
    id_type result{};
    channel_->update([&](Root& root) { result = resolve_(root).persistent_id; });
    return result;
  }

  // Allocate monotonically, outside the mutable candidate callback.
  id_type create_id() const {
    channel_->require_active();
    return channel_->allocate(channel_->owner);
  }

  // Extend the resolver with a checked child selection; selector returns an owned lvalue.
  template <bool ChildManaged, typename Select>
  auto select(Select select) const {
    using child_type = std::remove_reference_t<decltype(select(std::declval<value_type&>()))>;
    auto resolve = [parent = resolve_, select = std::move(select)](Root& root) -> child_type& {
      return select(payload(parent(root)));
    };
    return editor_access<Root, child_type, ChildManaged, decltype(resolve), Runtime>{channel_,
                                                                            std::move(resolve)};
  }

  // Bind a generated data member without storing a pointer into snapshot storage.
  template <bool ChildManaged, typename Field>
  auto member(Field value_type::* pointer) const
    requires std::is_class_v<value_type>
  {
    return select<ChildManaged>([pointer](value_type& value) -> Field& { return value.*pointer; });
  }

  // Bind a public base occurrence while retaining dormant identity boundaries on ordinary values.
  template <typename Base, bool BaseManaged>
  auto base() const {
    static_assert(std::is_base_of_v<Base, value_type>, "Invalid inherited editor base");
    return select<BaseManaged && Managed>([](value_type& value) -> Base& {
      return static_cast<Base&>(value);
    });
  }

  // Invalidate every prior alternative handle before publishing a candidate replacement.
  template <typename Callable>
  void replace_variant(Callable&& callback) const {
    guard([&] {
      if (channel_->variant_generation == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error{"Managed variant generation exhausted"};
      }
      ++channel_->variant_generation;
      update(std::forward<Callable>(callback));
    });
  }

  // Select by both active index and replacement generation to prevent alternative ABA reuse.
  template <std::size_t Index, bool ChildManaged>
  auto alternative() const {
    return guard([&] {
      const auto generation = channel_->variant_generation;
      auto child = select<ChildManaged>(
          [channel = channel_, generation](value_type& value) -> auto& {
            if (channel->variant_generation != generation || value.index() != Index ||
                value.valueless_by_exception()) {
              throw std::out_of_range{"Managed variant alternative was replaced"};
            }
            return std::get<Index>(value);
          });
      static_cast<void>(child.copy());
      return child;
    });
  }

  // Re-resolve one engagement and reject disappearance or replacement before exposing its editor.
  template <bool ChildManaged>
  auto alternative_optional() const {
    return guard([&] {
      const auto generation = channel_->variant_generation;
      auto child = select<ChildManaged>(
          [channel = channel_, generation](value_type& value) -> auto& {
            if (channel->variant_generation != generation || !value) {
              throw std::out_of_range{"Managed optional engagement was replaced"};
            }
            return *value;
          });
      child.update([](const auto&) {});
      return child;
    });
  }

  // Include conversion/allocation errors in the transaction failure boundary.
  template <typename Callable>
  decltype(auto) guard(Callable&& callback) const {
    return channel_->guard(std::forward<Callable>(callback));
  }
};

// Scalar alternatives expose copies and checked assignments, never candidate references.
template <typename Access>
class scalar_editor {
  Access access_;

public:
  // Bind one already checked variant alternative path.
  explicit scalar_editor(Access access) : access_{std::move(access)} {}
  // Read the currently active value without exposing its mutable storage.
  auto get() const { return access_.copy(); }
  // Change only this active payload under the transaction failure boundary.
  void set(typename Access::value_type value) const requires (!Access::is_runtime_access) {
    access_.update([&](auto& target) { target = std::move(value); });
  }
};

// Edit ordinary collections under their nearest managed owner without copying the container.
template <typename Access>
class value_collection_editor {
  Access access_;
  using container_type = typename Access::value_type;
  static constexpr bool is_map = requires { typename container_type::mapped_type; };
  static constexpr bool is_bool_vector = std::same_as<container_type, std::vector<bool>>;

  // Resolve a map key or checked sequence index only for the duration of one candidate callback.
  template <typename Key, typename Values>
  static auto find_element(Values& values, const Key& key) {
    if constexpr (is_map) {
      const auto found = values.find(key);
      return found == values.end() ? nullptr : std::addressof(found->second);
    } else {
      static_assert(std::same_as<Key, std::size_t>, "Sequence selectors use std::size_t");
      return key < values.size() ? std::addressof(values[key]) : nullptr;
    }
  }

public:
  // Bind an ordinary schema collection to one guarded transaction path.
  explicit value_collection_editor(Access access) : access_{std::move(access)} {}

  // Read the current count without copying elements or exposing candidate storage.
  std::size_t size() const {
    std::size_t result{};
    access_.update([&](const auto& values) { result = values.size(); });
    return result;
  }

  // Borrow one const element synchronously; references may not outlive or reenter the callback.
  template <typename Key, typename Callable>
  bool with_element(const Key& key, Callable&& callback) const {
    bool found{};
    access_.update([&](const auto& values) {
      if constexpr (is_bool_vector) {
        static_assert(std::same_as<Key, std::size_t>, "Sequence selectors use std::size_t");
        if (key < values.size()) {
          // Packed vector<bool> has no borrowable bool object; inspect one scalar snapshot.
          const bool value = values[key];
          static_assert(std::same_as<std::invoke_result_t<Callable, const bool&>, void>);
          std::invoke(std::forward<Callable>(callback), value);
          found = true;
        }
      } else {
        if (const auto* value = find_element(values, key)) {
          static_assert(std::same_as<std::invoke_result_t<Callable, decltype(*value)>, void>,
                        "Borrowed collection callbacks must return void");
          std::invoke(std::forward<Callable>(callback), *value);
          found = true;
        }
      }
    });
    return found;
  }

  // Mutate one ordinary element under its owner's failure boundary; a missing selector fails.
  template <typename Key, typename Callable>
  void update_element(const Key& key, Callable&& callback) const {
    access_.update([&](auto& values) {
      if constexpr (is_bool_vector) {
        static_assert(std::same_as<Key, std::size_t>, "Sequence selectors use std::size_t");
        if (key >= values.size()) { throw std::out_of_range{"Collection element does not exist"}; }
        // Commit the packed bit only after the callback has completed successfully.
        bool value = values[key];
        static_assert(std::same_as<std::invoke_result_t<Callable, bool&>, void>);
        std::invoke(std::forward<Callable>(callback), value);
        values[key] = value;
      } else {
        auto* value = find_element(values, key);
        if (!value) { throw std::out_of_range{"Collection element does not exist"}; }
        static_assert(std::same_as<std::invoke_result_t<Callable, decltype(*value)>, void>,
                      "Collection mutation callbacks must return void");
        std::invoke(std::forward<Callable>(callback), *value);
      }
    });
  }

  // Append an ordinary sequence element atomically with respect to transaction publication.
  template <typename Value>
  void append(Value&& value) const
    requires (!is_map && requires (container_type& values,
                                  typename container_type::value_type element) {
      values.push_back(std::move(element));
    }) {
    access_.guard([&] {
      typename container_type::value_type element{std::forward<Value>(value)};
      access_.update([&](auto& values) { values.push_back(std::move(element)); });
    });
  }

  // Insert before a checked sequence index; fixed arrays deliberately provide no insertion.
  template <typename Value>
  void insert(std::size_t index, Value&& value) const
    requires (!is_map && requires (container_type& values,
                                  typename container_type::value_type element) {
      values.insert(values.begin(), std::move(element));
    }) {
    access_.guard([&] {
      typename container_type::value_type element{std::forward<Value>(value)};
      access_.update([&](auto& values) {
        if (index > values.size()) { throw std::out_of_range{"Collection insertion index exceeds size"}; }
        values.insert(values.begin() + static_cast<typename container_type::difference_type>(index),
                      std::move(element));
      });
    });
  }

  // Insert a unique map key; conversion, duplicate keys, and allocation failures fail the action.
  template <typename Key, typename Value>
  void insert(Key&& key, Value&& value) const requires is_map {
    access_.guard([&] {
      typename container_type::key_type owned_key{std::forward<Key>(key)};
      typename container_type::mapped_type owned_value{std::forward<Value>(value)};
      access_.update([&](auto& values) {
        if (!values.emplace(std::move(owned_key), std::move(owned_value)).second) {
          throw std::invalid_argument{"Collection map key already exists"};
        }
      });
    });
  }

  // Erase an existing selector and report absence; fixed arrays expose element updates only.
  template <typename Key>
  bool erase(const Key& key) const requires requires (container_type& values) {
    values.erase(values.begin());
  } {
    bool erased{};
    access_.update([&](auto& values) {
      if constexpr (is_map) {
        erased = values.erase(key) != 0;
      } else {
        static_assert(std::same_as<Key, std::size_t>, "Sequence selectors use std::size_t");
        if (key < values.size()) {
          values.erase(values.begin() + static_cast<typename container_type::difference_type>(key));
          erased = true;
        }
      }
    });
    return erased;
  }
};

template <typename Access, typename Plain> class optional_editor;
template <typename Access, typename Plain> class array_editor;
template <typename Access, typename Plain> class map_editor;

// Select the checked editor matching the current standard owning wrapper or identified leaf.
template <typename Plain, typename Access>
auto make_owned_editor(Access access) {
  if constexpr (requires (Plain value) { value.has_value(); value.value(); }) {
    return optional_editor<Access, typename Plain::value_type>{std::move(access)};
  } else if constexpr (requires { typename Plain::mapped_type; }) {
    return map_editor<Access, typename Plain::mapped_type>{std::move(access)};
  } else if constexpr (requires (Plain value) { value.begin(); value.end(); }) {
    return array_editor<Access, typename Plain::value_type>{std::move(access)};
  } else {
    return make_editor<Plain>(std::move(access));
  }
}

// Mutate optional engagement under the transaction boundary and reject stale engaged handles.
template <typename Access, typename Plain>
class optional_editor {
  Access access_;
  using storage_value = typename Access::value_type::value_type;
  using leaf_traits = model_traits<typename owned_value_traits<Plain>::leaf_plain>;
public:
  // Bind the optional occurrence without borrowing its candidate storage.
  explicit optional_editor(Access access) : access_{std::move(access)} {}
  // Read engagement without exposing the contained identity or mutable payload.
  bool has_value() const {
    bool result{};
    access_.update([&](const auto& value) { result = value.has_value(); });
    return result;
  }
  // Adopt a new value and assign fresh identities to every independently managed descendant.
  auto emplace(Plain value) const requires (!Access::is_runtime_access) {
    return access_.guard([&] {
      auto storage = convert_owned_value<storage_value>(std::move(value));
      visit_owned_children(storage, [&](auto& child) {
        leaf_traits::visit_entities(child, [&](auto& id, auto) { id = access_.create_id(); });
      });
      const auto id = first_owned_id(storage);
      access_.replace_variant([&](auto& target) { target.emplace(std::move(storage)); });
      return id;
    });
  }
  // Retire the engaged subtree; history and retained snapshots preserve its identities.
  void reset() const requires (!Access::is_runtime_access) {
    access_.replace_variant([](auto& target) { target.reset(); });
  }
  // Select the current engagement and invalidate this handle on any optional replacement.
  auto edit() const {
    auto selected = access_.template alternative_optional<owned_value_traits<Plain>::is_entity>();
    return make_owned_editor<Plain>(std::move(selected));
  }
};

// Map operations use application keys for storage and persistent IDs for editing identity.
template <typename Access, typename Plain>
class map_editor {
  Access access_;
  using traits = model_traits<typename owned_value_traits<Plain>::leaf_plain>;
  using map_type = typename Access::value_type;
  using key_type = typename map_type::key_type;
  using id_type = typename Access::id_type;

  // Resolve an ID once to a key; subsequent setters use logarithmic key lookup plus ID validation.
  key_type key_for(id_type id) const {
    std::optional<key_type> result;
    access_.update([&](const auto& values) {
      for (const auto& [key, value] : values) {
        if (find_owned_child<const typename traits::storage_type>(value, id)) {
          result = key;
          break;
        }
      }
      if (!result) {
        throw std::out_of_range{"Managed entity is not in this collection"};
      }
    });
    return std::move(*result);
  }

public:
  // Bind this collection to its owning transaction and member path.
  explicit map_editor(Access access) : access_{std::move(access)} {}

  // Borrow a managed payload by stable identity; callback references expire on return.
  template <typename Callable>
  bool with_element(id_type id, Callable&& callback) const {
    bool found{};
    access_.update([&](const auto& values) {
      for (const auto& [key, value] : values) {
        static_cast<void>(key);
        if (const auto* child = find_owned_child<const typename traits::storage_type>(value, id)) {
          const auto& payload = traits::payload(*child);
          static_assert(std::same_as<std::invoke_result_t<Callable, decltype(payload)>, void>);
          std::invoke(std::forward<Callable>(callback), payload);
          found = true;
          break;
        }
      }
    });
    return found;
  }

  // Create an entity editor which detects erasure or replacement before each operation.
  auto edit(id_type id) const {
    return access_.guard([&] {
      auto selected = access_.template select<true>([key = key_for(id), id](auto& values) -> auto& {
        auto found = values.find(key);
        if (found == values.end()) {
          throw std::out_of_range{"Managed entity was removed"};
        }
        auto* child = find_owned_child<typename traits::storage_type>(found->second, id);
        if (!child) { throw std::out_of_range{"Managed entity was removed"}; }
        return *child;
      });
      return traits::make_editor(std::move(selected));
    });
  }

  // Deep-convert an ordinary value and assign identities to precisely its managed occurrences.
  id_type insert(key_type key, Plain value) const requires (!Access::is_runtime_access) {
    return access_.guard([&] {
      auto storage = convert_owned_value<typename map_type::mapped_type>(std::move(value));
      visit_owned_children(storage, [&](auto& child) {
        traits::visit_entities(child, [&](auto& id, auto) { id = access_.create_id(); });
      });
      const auto id = static_cast<id_type>(first_owned_id(storage));
      access_.update([&](auto& values) {
        if (!values.emplace(std::move(key), std::move(storage)).second) {
          throw std::invalid_argument{"Managed map key already exists"};
        }
      });
      return id;
    });
  }

  // Delete the live occurrence; retained snapshots preserve its ID and value for undo.
  void erase(id_type id) const requires (!Access::is_runtime_access) {
    access_.guard([&] {
      const auto key = key_for(id);
      access_.update([&](auto& values) { values.erase(key); });
    });
  }
};

// Arrays use stable entity IDs rather than positions, so reallocation cannot retarget an editor.
template <typename Access, typename Plain>
class array_editor {
  Access access_;
  using traits = model_traits<typename owned_value_traits<Plain>::leaf_plain>;
  using id_type = typename Access::id_type;
  using container_type = typename Access::value_type;

public:
  // Bind the sequence to its owning transaction and member path.
  explicit array_editor(Access access) : access_{std::move(access)} {}

  // Borrow one identified payload synchronously without copying its owning array.
  template <typename Callable>
  bool with_element(id_type id, Callable&& callback) const {
    bool found{};
    access_.update([&](const auto& values) {
      for (const auto& value : values) {
        if (const auto* child = find_owned_child<const typename traits::storage_type>(value, id)) {
          const auto& payload = traits::payload(*child);
          static_assert(std::same_as<std::invoke_result_t<Callable, decltype(payload)>, void>);
          std::invoke(std::forward<Callable>(callback), payload);
          found = true;
          break;
        }
      }
    });
    return found;
  }

  // Select by identity on each operation; an erased occurrence is never silently replaced.
  auto edit(id_type id) const {
    auto selected = access_.template select<true>([id](auto& values) -> auto& {
      auto found = std::find_if(values.begin(), values.end(),
          [id](auto& value) { return find_owned_child<typename traits::storage_type>(value, id); });
      if (found == values.end()) {
        throw std::out_of_range{"Managed entity is not in this array"};
      }
      return *find_owned_child<typename traits::storage_type>(*found, id);
    });
    static_cast<void>(selected.id());
    return traits::make_editor(std::move(selected));
  }

  // Append an ordinary value with newly allocated identities for its managed descendants.
  id_type append(Plain value) const requires (!Access::is_runtime_access &&
      requires (container_type& values, typename container_type::value_type child) {
        values.push_back(std::move(child));
      }) {
    return access_.guard([&] {
      auto storage = convert_owned_value<typename container_type::value_type>(std::move(value));
      visit_owned_children(storage, [&](auto& child) {
        traits::visit_entities(child, [&](auto& id, auto) { id = access_.create_id(); });
      });
      const auto id = static_cast<id_type>(first_owned_id(storage));
      access_.update([&](auto& values) { values.push_back(std::move(storage)); });
      return id;
    });
  }

  // Remove exactly one occurrence, retaining undo storage independently from the live array.
  void erase(id_type id) const requires (!Access::is_runtime_access &&
      requires (container_type& values) { values.erase(values.begin()); }) {
    access_.update([&](auto& values) {
      const auto found = std::find_if(values.begin(), values.end(), [id](auto& value) {
        return find_owned_child<typename traits::storage_type>(value, id) != nullptr;
      });
      if (found == values.end()) {
        throw std::out_of_range{"Managed entity is not in this array"};
      }
      values.erase(found);
    });
  }
};
} // namespace detail
} // namespace rohit::managed
