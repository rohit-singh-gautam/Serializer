// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "model.hpp"

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace readonly_example {
// Existing native C++ friendship supplies authorized writes without generated setters.
struct writer {
  // Change the actual private payload and ordinary durable label in the owning model.
  static void set(document& owner, std::int32_t value, std::string label) {
    owner.contents_.payload_ = value;
    owner.label_ = std::move(label);
  }
  // Expose the backing field's address solely to verify the generated reference identity.
  static const integer_box* contents_address(const document& owner) {
    return std::addressof(owner.contents_);
  }
};
} // namespace readonly_example

namespace {
using document = readonly_example::document;

// Ref-qualified deleted overloads reject both mutable and const temporary owners.
template <typename Owner>
concept can_borrow_contents = requires(Owner&& owner) { std::forward<Owner>(owner).contents(); };
// The public getter cannot confer mutable access even through a mutable owner.
template <typename Owner>
concept can_mutate_payload = requires(Owner& owner) { owner.contents().payload() = 1; };

static_assert(can_borrow_contents<document&> && can_borrow_contents<const document&>);
static_assert(!can_borrow_contents<document> && !can_borrow_contents<const document>);
static_assert(!can_mutate_payload<document>);
static_assert(std::is_same_v<decltype(std::declval<document&>().contents()),
                             const readonly_example::integer_box&>);
static_assert(sizeof(readonly_example::integer_box) == sizeof(std::int32_t));
} // namespace

// Qualify address identity, friend writes, exact-message codec round trips and owner lifetime.
int main() {
  document owner;
  const auto& borrowed = owner.contents();
  if (std::addressof(borrowed) != readonly_example::writer::contents_address(owner)) {
    throw std::runtime_error{"Getter did not return its backing field"};
  }
  readonly_example::writer::set(owner, 42, "updated");
  if (borrowed.payload() != 42 || owner.label() != "updated") {
    throw std::runtime_error{"Borrowed getter did not reflect authorized writes"};
  }
  rohit::full_stream_auto_alloc output;
  owner.serialize_out<rohit::serializer::json>(output);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  const auto decoded = rohit::serializer::deserialize_exact<document, rohit::serializer::json>(input);
  if (decoded.contents().payload() != 42 || decoded.label() != "updated") {
    throw std::runtime_error{"Readonly getter codec round trip failed"};
  }
  std::cout << decoded.label() << ": " << decoded.contents().payload() << '\n';
}
