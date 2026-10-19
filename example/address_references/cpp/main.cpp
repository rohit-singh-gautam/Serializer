// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "message.hpp"

#include <rohit/serializer.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr char document_id[] = "document-1";
constexpr std::uint64_t owner_id = 7;
constexpr std::uint64_t generation = 1;

// Keep example checks active in optimized builds.
void require(bool condition, const char* message) {
  if (!condition) { throw std::runtime_error{message}; }
}

// Encode an owning record or a scoped projection through the same native API.
template <template <rohit::serializer::serialize_type> class Protocol>
std::string encode(const auto& value) {
  rohit::full_stream_auto_alloc output;
  value.template serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Resolve a loaded address explicitly and write its live target projection in one scope.
template <template <rohit::serializer::serialize_type> class Protocol>
void round_trip(const consumer& links, const std::shared_ptr<owner>& storage) {
  const auto bytes = encode<Protocol>(links);
  const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  consumer loaded{};
  loaded.template serialize_in<Protocol>(input);
  require(!loaded.d.is_bound(), "Loading must not restore a process-local pointer");
  loaded.d.resolve(storage, document_id, owner_id, generation);

  // This owning value is only an independent verification of the projected bytes.
  projected_value expected{};
  expected.label = "updated";
  expected.count = 2;
  loaded.d.with_value([&](const live_value& source) {
    require(std::addressof(source) == std::addressof(storage->b), "The reference must borrow b");
    source.with_to_wire([&](const auto& projected) {
      require(encode<Protocol>(projected) == encode<Protocol>(expected), "Projection differs from its wire shape");
    });
  });
}
} // namespace

// Demonstrate owner retention, explicit resolution and reference-to-translator composition.
int main() {
  auto storage = std::make_shared<owner>();
  storage->b.text = "initial";
  storage->b.count = 1;
  borrowed_b borrowed;
  borrowed.bind(*storage, document_id, owner_id, generation);
  weak_b weak;
  weak.bind(storage, document_id, owner_id, generation);
  consumer links;
  links.d.bind(storage, document_id, owner_id, generation);

  storage->b.text = "updated";
  storage->b.count = 2;
  require(borrowed.read_value().text == "updated", "A reference must follow live edits");
  round_trip<rohit::serializer::json>(links, storage);
  round_trip<rohit::serializer::binary_none>(links, storage);
  round_trip<rohit::serializer::binary_integer>(links, storage);
  round_trip<rohit::serializer::binary_string>(links, storage);

  links.d.with_value([](const live_value& source) {
    source.with_to_wire([](const auto& projected) {
      std::cout << encode<rohit::serializer::json>(projected) << '\n';
    });
  });

  borrowed.unbind(); // End this borrow before dropping the caller's owner handle.
  storage.reset();
  require(links.d.is_bound() && weak.is_bound(), "The shared reference must retain the owner");
  links.d.unbind();
  require(!weak.is_bound(), "A weak reference must observe owner destruction");
  return 0;
}
