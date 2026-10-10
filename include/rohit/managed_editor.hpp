// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <algorithm>
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

// Map operations use application keys for storage and persistent IDs for editing identity.
template <typename Access, typename Plain>
class map_editor {
  Access access_;
  using traits = model_traits<Plain>;
  using map_type = typename Access::value_type;
  using key_type = typename map_type::key_type;
  using id_type = typename Access::id_type;

  // Resolve an ID once to a key; subsequent setters use logarithmic key lookup plus ID validation.
  key_type key_for(id_type id) const {
    std::optional<key_type> result;
    access_.update([&](const auto& values) {
      for (const auto& [key, value] : values) {
        if (value.persistent_id == id) {
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

  // Create an entity editor which detects erasure or replacement before each operation.
  auto edit(id_type id) const {
    return access_.guard([&] {
      auto selected = access_.template select<true>([key = key_for(id), id](auto& values) -> auto& {
        auto found = values.find(key);
        if (found == values.end() || found->second.persistent_id != id) {
          throw std::out_of_range{"Managed entity was removed"};
        }
        return found->second;
      });
      return traits::make_editor(std::move(selected));
    });
  }

  // Deep-convert an ordinary value and assign identities to precisely its managed occurrences.
  id_type insert(key_type key, Plain value) const requires (!Access::is_runtime_access) {
    return access_.guard([&] {
      auto storage = traits::make_storage(std::move(value));
      traits::visit_entities(storage, [&](auto& id, auto) { id = access_.create_id(); });
      const auto id = storage.persistent_id;
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
  using traits = model_traits<Plain>;
  using id_type = typename Access::id_type;
  using container_type = typename Access::value_type;

public:
  // Bind the sequence to its owning transaction and member path.
  explicit array_editor(Access access) : access_{std::move(access)} {}

  // Select by identity on each operation; an erased occurrence is never silently replaced.
  auto edit(id_type id) const {
    auto selected = access_.template select<true>([id](auto& values) -> auto& {
      auto found = std::find_if(values.begin(), values.end(),
                                [id](const auto& value) { return value.persistent_id == id; });
      if (found == values.end()) {
        throw std::out_of_range{"Managed entity is not in this array"};
      }
      return *found;
    });
    static_cast<void>(selected.id());
    return traits::make_editor(std::move(selected));
  }

  // Append an ordinary value with newly allocated identities for its managed descendants.
  id_type append(Plain value) const requires (!Access::is_runtime_access &&
      requires (container_type& values, typename traits::storage_type child) {
        values.push_back(std::move(child));
      }) {
    return access_.guard([&] {
      auto storage = traits::make_storage(std::move(value));
      traits::visit_entities(storage, [&](auto& id, auto) { id = access_.create_id(); });
      const auto id = storage.persistent_id;
      access_.update([&](auto& values) { values.push_back(std::move(storage)); });
      return id;
    });
  }

  // Remove exactly one occurrence, retaining undo storage independently from the live array.
  void erase(id_type id) const requires (!Access::is_runtime_access &&
      requires (container_type& values) { values.erase(values.begin()); }) {
    access_.update([&](auto& values) {
      const auto found = std::find_if(values.begin(), values.end(), [id](const auto& value) {
        return value.persistent_id == id;
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
