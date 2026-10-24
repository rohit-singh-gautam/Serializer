// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/payload_allocator.hpp>
#include <rohit/serializer.hpp>

#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <utility>

namespace rohit::serializer {

// Clone a complete allocation-profile payload into one domain before exposing it.
// Failure leaves the source unchanged; the returned value retains resource ownership independently.
template <typename Value>
Value clone_payload(const Value& source, std::shared_ptr<std::pmr::memory_resource> resource) {
  payload_allocation_scope scope{resource};
  Value candidate{source};
  if (!payload_resources_match(candidate, resource)) {
    throw std::invalid_argument{"Payload clone retained a foreign allocation domain"};
  }
  return candidate;
}

// Decode an exact message into a fresh allocation-bound candidate, publishing only complete success.
// Input/output buffers, diagnostics and allocations outside the selected payload profile are excluded.
template <typename Value, template <serialize_type> class Protocol, typename Input>
Value deserialize_payload_exact(Input&& input, std::shared_ptr<std::pmr::memory_resource> resource,
                                decode_limits limits = {}) {
  payload_allocation_scope scope{resource};
  auto candidate = deserialize_exact<Value, Protocol>(std::forward<Input>(input), limits);
  if (!payload_resources_match(candidate, resource)) {
    throw std::invalid_argument{"Decoded payload retained a foreign allocation domain"};
  }
  return candidate;
}
} // namespace rohit::serializer
