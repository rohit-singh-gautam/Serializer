// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <utility>

namespace rohit::managed {

// A hard bound on bytes requested through this resource, independent of model admission estimates.
// Upstream allocator metadata, allocations bypassing this resource and process residency are excluded.
class bounded_memory_resource final : public std::pmr::memory_resource {
  const std::size_t maximum_bytes_;
  std::shared_ptr<std::pmr::memory_resource> upstream_;
  std::atomic<std::size_t> reserved_bytes_{};
  std::atomic<std::size_t> peak_reserved_bytes_{};

  // Reserve before asking upstream; concurrent allocation attempts cannot oversubscribe the bound.
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    auto current = reserved_bytes_.load(std::memory_order_relaxed);
    do {
      if (bytes > maximum_bytes_ || current > maximum_bytes_ - bytes) { throw std::bad_alloc{}; }
    } while (!reserved_bytes_.compare_exchange_weak(current, current + bytes, std::memory_order_relaxed));
    auto peak = peak_reserved_bytes_.load(std::memory_order_relaxed);
    while (peak < current + bytes && !peak_reserved_bytes_.compare_exchange_weak(
        peak, current + bytes, std::memory_order_relaxed)) {}
    try { return upstream_->allocate(bytes, alignment); }
    catch (...) {
      reserved_bytes_.fetch_sub(bytes, std::memory_order_relaxed);
      throw;
    }
  }

  // PMR callers must return the same byte count/alignment; the upstream must support their concurrency.
  void do_deallocate(void* address, std::size_t bytes, std::size_t alignment) override {
    upstream_->deallocate(address, bytes, alignment);
    reserved_bytes_.fetch_sub(bytes, std::memory_order_relaxed);
  }

  // Only this exact accounting domain can interchange allocations and deallocations.
  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }

public:
  // Retain upstream ownership for this resource's lifetime; PMR containers must keep this resource alive.
  explicit bounded_memory_resource(std::size_t maximum_bytes,
      std::shared_ptr<std::pmr::memory_resource> upstream)
      : maximum_bytes_{maximum_bytes}, upstream_{std::move(upstream)} {
    if (!upstream_) { throw std::invalid_argument{"Bounded allocation requires an upstream resource"}; }
  }

  // Use the process-independent new/delete resource when the host needs no custom upstream lifetime.
  explicit bounded_memory_resource(std::size_t maximum_bytes)
      : bounded_memory_resource{maximum_bytes, std::shared_ptr<std::pmr::memory_resource>{
          std::pmr::new_delete_resource(), [](std::pmr::memory_resource*) {}}} {}

  // Report currently reserved requested bytes, including allocation attempts in progress.
  std::size_t reserved_bytes() const noexcept { return reserved_bytes_.load(std::memory_order_relaxed); }
  // Failed upstream attempts can contribute to this high-water mark; this is not process peak memory.
  std::size_t peak_reserved_bytes() const noexcept {
    return peak_reserved_bytes_.load(std::memory_order_relaxed);
  }
  // The immutable domain limit applies across all PMR containers and managed roots sharing this resource.
  std::size_t maximum_bytes() const noexcept { return maximum_bytes_; }
};
} // namespace rohit::managed
