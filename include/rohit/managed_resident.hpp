// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include "managed_memory.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <utility>

namespace rohit::managed::detail {

// Admission accounting follows shared model pins even when readers release them on another thread.
class resident_ledger {
  const std::size_t maximum_;
  std::atomic<std::size_t> bytes_{};
public:
  // Store one budget for the accepted document, private candidates and outstanding immutable pins.
  explicit resident_ledger(std::size_t maximum) : maximum_{maximum} {}
  // Reserve before acceptance; failed reservations do not change the previous accounting.
  void reserve(std::size_t amount) {
    auto bytes = bytes_.load(std::memory_order_relaxed);
    do {
      if (amount > maximum_ || bytes > maximum_ - amount) {
        throw std::length_error{"Managed resident admission budget exceeded"};
      }
    } while (!bytes_.compare_exchange_weak(bytes, bytes + amount, std::memory_order_relaxed));
  }
  // Destruction can only return previously admitted storage; release never allocates or throws.
  void release(std::size_t amount) noexcept {
    bytes_.fetch_sub(amount, std::memory_order_relaxed);
  }
  // Report accounted storage, including old root values still pinned outside their store.
  std::size_t bytes() const noexcept {
    return bytes_.load(std::memory_order_relaxed);
  }
};

// One independently resizeable reservation belongs to a candidate, immutable pin or metadata owner.
class resident_charge {
  std::shared_ptr<resident_ledger> ledger_;
  std::size_t bytes_{};
public:
  // Admission failure happens before a new charge becomes visible.
  explicit resident_charge(std::shared_ptr<resident_ledger> ledger, std::size_t bytes = 0)
      : ledger_{std::move(ledger)} {
    resize(bytes);
  }
  resident_charge(const resident_charge&) = delete;
  resident_charge& operator=(const resident_charge&) = delete;
  // Return charges after the last owner releases its pin or rejects its private candidate.
  ~resident_charge() { ledger_->release(bytes_); }
  // Grow atomically before updating the charge; shrinking requires no additional allocation.
  void resize(std::size_t bytes) {
    if (bytes > bytes_) {
      ledger_->reserve(bytes - bytes_);
    } else {
      ledger_->release(bytes_ - bytes);
    }
    bytes_ = bytes;
  }
  // Read the owner-thread reservation, rather than inferring it from serialized payload size.
  std::size_t bytes() const noexcept { return bytes_; }
  // Exchange same-ledger metadata ownership after a previously validated state swap.
  void swap(resident_charge& other) noexcept {
    ledger_.swap(other.ledger_);
    std::swap(bytes_, other.bytes_);
  }
};

// Reserve the larger accepted metadata estimate before I/O and roll back accounting on failure.
class resident_admission {
  resident_charge& charge_;
  const std::size_t before_;
  const std::size_t after_;
  bool committed_{};
public:
  // The staged maximum intentionally describes admission, not a hard peak allocator limit.
  resident_admission(resident_charge& charge, std::size_t after)
      : charge_{charge}, before_{charge.bytes()}, after_{after} {
    charge_.resize(std::max(before_, after_));
  }
  resident_admission(const resident_admission&) = delete;
  // Shrinking to either accepted estimate cannot fail after a journal has acknowledged publication.
  ~resident_admission() { charge_.resize(committed_ ? after_ : before_); }
  // Mark only after all no-throw publication swaps have completed.
  void commit() noexcept { committed_ = true; }
};

// Retain the optional allocation resource inside every rebound shared-control-block allocator.
template <typename T>
struct resident_allocator {
  using value_type = T;
  std::shared_ptr<std::pmr::memory_resource> resource{};
  // Default callers keep the standard allocator and need no resource lifetime contract.
  resident_allocator() = default;
  // The shared owner outlives weak references as well as immutable model pins.
  explicit resident_allocator(std::shared_ptr<std::pmr::memory_resource> value)
      : resource{std::move(value)} {}
  // Preserve resource ownership when allocate_shared rebinds to its implementation control block.
  template <typename U>
  resident_allocator(const resident_allocator<U>& other) : resource{other.resource} {}
  // Scope the resource hook to root/control-block allocations, without changing model containers.
  T* allocate(std::size_t count) {
    if (!resource) { return std::allocator<T>{}.allocate(count); }
    return static_cast<T*>(resource->allocate(multiply_memory_estimate(count, sizeof(T)), alignof(T)));
  }
  // Return allocations through the same retained resource, with their original alignment.
  void deallocate(T* pointer, std::size_t count) noexcept {
    if (!resource) { std::allocator<T>{}.deallocate(pointer, count); }
    else { resource->deallocate(pointer, count * sizeof(T), alignof(T)); }
  }
  // Standard allocator comparisons identify the actual shared resource instance.
  template <typename U>
  bool operator==(const resident_allocator<U>& other) const noexcept {
    return resource.get() == other.resource.get();
  }
};

// Alias the model and its reservation from one shared holder so both follow immutable pin lifetime.
template <typename Storage>
struct resident_storage {
  Storage value;
  resident_charge charge;
  // Move the owning model before admitting its conservative estimate; failed admission destroys it.
  resident_storage(Storage input, std::shared_ptr<resident_ledger> ledger, std::size_t estimate)
      : value{std::move(input)}, charge{std::move(ledger), estimate} {}
};

// Construct a model/candidate and a resize handle without exposing the implementation holder.
template <typename Storage>
auto make_resident_storage(Storage value, const std::shared_ptr<resident_ledger>& ledger,
                           const std::shared_ptr<std::pmr::memory_resource>& resource,
                           std::size_t estimate) {
  auto holder = std::allocate_shared<resident_storage<Storage>>(
      resident_allocator<resident_storage<Storage>>{resource}, std::move(value), ledger, estimate);
  return std::pair{std::shared_ptr<Storage>{holder, &holder->value},
                   std::shared_ptr<resident_charge>{holder, &holder->charge}};
}
} // namespace rohit::managed::detail
