// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <cstddef>
#include <concepts>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <memory_resource>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::serializer {
namespace detail {
struct payload_allocation_context {
  std::shared_ptr<std::pmr::memory_resource> resource;
};
inline thread_local const payload_allocation_context* active_payload_allocation{};
} // namespace detail

// Bind construction and decoder temporaries on this thread without changing the process PMR default.
// Every allocator captures shared ownership; values may safely outlive the scope and move to other threads.
class payload_allocation_scope {
  detail::payload_allocation_context context_;
  const detail::payload_allocation_context* previous_;
public:
  // A null resource explicitly selects new/delete; nested scopes restore their predecessor on every exit.
  explicit payload_allocation_scope(std::shared_ptr<std::pmr::memory_resource> resource) noexcept
      : context_{std::move(resource)}, previous_{detail::active_payload_allocation} {
    detail::active_payload_allocation = &context_;
  }
  // An active stack binding has one owner and cannot be moved between stack locations or threads.
  payload_allocation_scope(const payload_allocation_scope&) = delete;
  // Prevent rebinding a live stack scope.
  payload_allocation_scope& operator=(const payload_allocation_scope&) = delete;
  // Restore the exact enclosing domain, including during allocation or decode failure.
  ~payload_allocation_scope() { detail::active_payload_allocation = previous_; }
};

// Own one immutable allocation-domain binding, selecting the active destination during a scoped copy.
class payload_allocation_binding {
  std::shared_ptr<std::pmr::memory_resource> resource_{};
public:
  // Capture the active domain; ordinary context-free construction uses the new/delete resource.
  payload_allocation_binding() noexcept
      : resource_{detail::active_payload_allocation ? detail::active_payload_allocation->resource : nullptr} {}
  // A scoped clone selects its destination domain; an ordinary copy retains the source lifetime.
  payload_allocation_binding(const payload_allocation_binding& source) noexcept
      : resource_{detail::active_payload_allocation ? detail::active_payload_allocation->resource : source.resource_} {}
  // Moving a complete value retains its existing allocation domain.
  payload_allocation_binding(payload_allocation_binding&& source) noexcept : resource_{source.resource_} {}
  // Container assignment retains the destination's allocator, so its record binding must do likewise.
  payload_allocation_binding& operator=(const payload_allocation_binding&) noexcept { return *this; }
  // Preserve the destination domain when member containers perform elementwise move assignment.
  payload_allocation_binding& operator=(payload_allocation_binding&&) noexcept { return *this; }
  // Inspect retained ownership without allocating or mutating the selected resource.
  const std::shared_ptr<std::pmr::memory_resource>& resource() const noexcept { return resource_; }
};

// A standard owning allocator retaining the selected PMR resource for every container allocation.
// It never propagates on assignment/swap; swapping unequal domains has the usual standard precondition.
template <typename Value>
class payload_allocator {
  template <typename> friend class payload_allocator;
  std::shared_ptr<std::pmr::memory_resource> resource_{};
  // Null ownership uses the immortal standard new/delete resource, independently of PMR defaults.
  std::pmr::memory_resource* effective_resource() const noexcept {
    return resource_ ? resource_.get() : std::pmr::new_delete_resource();
  }
public:
  using value_type = Value;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::false_type;
  using propagate_on_container_swap = std::false_type;
  using is_always_equal = std::false_type;
  // Capture the current thread's explicit binding at container construction.
  payload_allocator() noexcept
      : resource_{detail::active_payload_allocation ? detail::active_payload_allocation->resource : nullptr} {}
  // Retain a caller-selected domain without altering any thread or process binding.
  explicit payload_allocator(std::shared_ptr<std::pmr::memory_resource> resource) noexcept
      : resource_{std::move(resource)} {}
  // Allocator copies retain the exact resource independently of thread construction scopes.
  payload_allocator(const payload_allocator&) noexcept = default;
  // Moved-from containers can retain or replace sentinels; both allocators must retain ownership.
  payload_allocator(payload_allocator&& source) noexcept : resource_{source.resource_} {}
  // Explicit allocator assignment preserves shared ownership of the selected source domain.
  payload_allocator& operator=(const payload_allocator&) noexcept = default;
  // A moved-from allocator must still deallocate its container's implementation-owned storage.
  payload_allocator& operator=(payload_allocator&& source) noexcept {
    resource_ = source.resource_;
    return *this;
  }
  // Preserve shared ownership when standard containers rebind for map nodes or implementation storage.
  template <typename Other>
  payload_allocator(const payload_allocator<Other>& other) noexcept : resource_{other.resource_} {}
  // Reject multiplication overflow before the resource can observe an allocation request.
  [[nodiscard]] Value* allocate(std::size_t count) {
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(Value)) { throw std::bad_array_new_length{}; }
    return static_cast<Value*>(effective_resource()->allocate(count * sizeof(Value), alignof(Value)));
  }
  // Return storage to its original retained domain with the standard allocator's matching count.
  void deallocate(Value* address, std::size_t count) noexcept {
    effective_resource()->deallocate(address, count * sizeof(Value), alignof(Value));
  }
  // Propagate this container's domain into nested strings, pairs and generated default/copy construction.
  template <typename Other, typename... Arguments>
  void construct(Other* address, Arguments&&... arguments) {
    payload_allocation_scope scope{resource_};
    std::uninitialized_construct_using_allocator(address, *this, std::forward<Arguments>(arguments)...);
  }
  // Scoped copies explicitly rebind; context-free copies retain their original allocation domain.
  payload_allocator select_on_container_copy_construction() const noexcept {
    return detail::active_payload_allocation ? payload_allocator{detail::active_payload_allocation->resource} : *this;
  }
  // Allow node interchange only when both allocators can deallocate each other's storage.
  template <typename Other>
  bool operator==(const payload_allocator<Other>& other) const noexcept {
    return effective_resource()->is_equal(*other.effective_resource());
  }
  // Expose retained ownership for generated decoder scopes and explicit domain validation.
  const std::shared_ptr<std::pmr::memory_resource>& resource() const noexcept { return resource_; }
};

using payload_string = std::basic_string<char, std::char_traits<char>, payload_allocator<char>>;
template <typename Value> using payload_vector = std::vector<Value, payload_allocator<Value>>;
template <typename Key, typename Value>
using payload_map = std::map<Key, Value, std::less<Key>, payload_allocator<std::pair<const Key, Value>>>;

// Validate a complete owning payload before publication; foreign-domain moves cannot bypass a store's bound.
template <typename Value>
bool payload_resources_match(const Value& value, const std::shared_ptr<std::pmr::memory_resource>& resource) {
  if constexpr (requires { value.serializer_payload_resources_match(resource); }) {
    return value.serializer_payload_resources_match(resource);
  } else if constexpr (requires { value.get_allocator().resource(); }) {
    if (value.get_allocator().resource().get() != resource.get()) { return false; }
    if constexpr (requires { typename Value::mapped_type; }) {
      for (const auto& [key, child] : value) {
        if (!payload_resources_match(key, resource) || !payload_resources_match(child, resource)) { return false; }
      }
    } else if constexpr (!std::same_as<typename Value::value_type, char>) {
      for (const auto& child : value) { if (!payload_resources_match(child, resource)) { return false; } }
    }
    return true;
  } else if constexpr (requires { value.has_value(); *value; }) {
    return !value.has_value() || payload_resources_match(*value, resource);
  } else if constexpr (requires { value.begin(); value.end(); }) {
    for (const auto& child : value) { if (!payload_resources_match(child, resource)) { return false; } }
    return true;
  } else {
    return true;
  }
}
} // namespace rohit::serializer
