// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "message.hpp"

#include <rohit/serializer.hpp>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using borrowed_reference = decltype(borrowed_consumer{}.d);
using shared_reference = decltype(shared_consumer{}.d);
using weak_reference = decltype(weak_consumer{}.d);
using shared_owner_reference = decltype(shared_owner{}.b);
using shared_value_reference = decltype(shared_value_consumer{}.d);
static_assert(sizeof(borrowed_reference) == sizeof(const payload*));
static_assert(sizeof(shared_reference) == sizeof(std::shared_ptr<const payload>));
static_assert(sizeof(weak_reference) == sizeof(std::weak_ptr<const payload>));
static_assert(sizeof(shared_owner_reference) == sizeof(std::shared_ptr<const payload>));
static_assert(sizeof(shared_value_reference) == sizeof(std::shared_ptr<const payload>));
static_assert(sizeof(shared_owner) == sizeof(std::shared_ptr<const payload>));
static_assert(sizeof(shared_value_consumer) == sizeof(std::shared_ptr<const payload>));
static_assert(std::is_assignable_v<borrowed_reference&, const payload&>);
static_assert(!std::is_assignable_v<borrowed_reference&, payload&&>);

// Keep example checks active in optimized builds.
void require(bool condition, const char* message) {
  if (!condition) { throw std::runtime_error{message}; }
}

// Encode through the same native traversal for owning values, references and translators.
template <template <rohit::serializer::serialize_type> class Protocol>
std::string encode(const auto& value) {
  rohit::full_stream_auto_alloc output;
  value.template serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Compare an independently shared value with its ordinary owning wire representation.
template <template <rohit::serializer::serialize_type> class Protocol>
void verify_shared_value_protocol(const shared_value_consumer& consumer, const payload& value) {
  // The independent copy establishes a test baseline; reference encoding borrows the payload.
  owning_consumer expected;
  expected.d = value;
  const auto bytes = encode<Protocol>(expected);
  require(encode<Protocol>(consumer) == bytes, "A shared value must encode as an ordinary owning field");
  owning_consumer decoded;
  const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  decoded.template serialize_in<Protocol>(input);
  require(decoded.d.values == value.values && decoded.d.codes == value.codes,
      "An ordinary consumer must decode the shared value's payload");
}

// Share one independently allocated payload between two ordinary stack-allocated objects.
void verify_independent_shared_value() {
  auto value = std::make_shared<payload>();
  value->values = {0.0F, 1.0F, 2.0F};
  value->codes = {0, 1, 2};
  auto* original = value.get();
  const std::weak_ptr<const payload> observer{value};
  shared_owner a;
  a.b.bind(value);
  shared_value_consumer c;
  c.d.bind(std::move(value));
  require(!value, "Moving the shared handle must leave the caller's handle empty");
  a.b.with_value([&](const payload& selected) {
    require(std::addressof(selected) == original, "The owner's field must share the original payload");
  });
  c.d.with_value([&](const payload& selected) {
    require(std::addressof(selected) == original, "The consumer's field must share the same payload");
    require(selected.values.data() == original->values.data(), "Shared fields must use the same values buffer");
  });
  original->values.front() = 4.5F;
  verify_shared_value_protocol<rohit::serializer::json>(c, *original);
  verify_shared_value_protocol<rohit::serializer::binary_none>(c, *original);
  verify_shared_value_protocol<rohit::serializer::binary_integer>(c, *original);
  verify_shared_value_protocol<rohit::serializer::binary_string>(c, *original);
  a.b.unbind();
  require(!observer.expired() && c.d.is_bound(), "The ordinary consumer must retain the independently shared value");
  c.d.unbind();
  require(observer.expired(), "The value must expire after its final shared field releases it");
}

// Compare every byte with an ordinary owning export and decode an independent payload value.
template <template <rohit::serializer::serialize_type> class Protocol>
void verify_protocol(const shared_consumer& projected, const borrowed_consumer& borrowed,
    const weak_consumer& weak, const std::shared_ptr<owner>& source) {
  // This copy is a verification baseline; the reference and translator paths never make it.
  owning_consumer expected;
  expected.d = source->b;
  const auto bytes = encode<Protocol>(expected);
  require(encode<Protocol>(projected) == bytes, "Shared projection changed the ordinary wire value");
  require(encode<Protocol>(borrowed) == bytes, "Borrowed projection changed the ordinary wire value");
  require(encode<Protocol>(weak) == bytes, "Weak projection changed the ordinary wire value");
  source->with_as_consumer([&](const auto& translated) {
    require(std::addressof(translated.get_d()) == std::addressof(source->b),
        "A translator must borrow the original payload");
    require(encode<Protocol>(translated) == bytes, "The translator changed the ordinary wire value");
  });

  owning_consumer decoded;
  const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  decoded.template serialize_in<Protocol>(input);
  require(decoded.d.values == expected.d.values && decoded.d.codes == expected.d.codes,
      "Decoded buffers differ from the ordinary owning value");
  require(decoded.d.values.data() != source->b.values.data(), "The owning consumer must allocate independent buffers");
  require(source->b.values == expected.d.values && source->b.codes == expected.d.codes,
      "Decoding the owning consumer must leave its source unchanged");

  shared_consumer rejected;
  rejected.d.bind(source);
  const auto reference_input = rohit::make_constant_stream(bytes.data(), bytes.size());
  bool reference_decode_rejected{};
  try { rejected.template serialize_in<Protocol>(reference_input); }
  catch (const std::logic_error&) { reference_decode_rejected = true; }
  require(reference_decode_rejected, "A nonowning export must reject decoding");
  require(rejected.d.is_bound(), "Rejected decoding must preserve the existing source binding");
  const auto previous = source->b.values.front();
  source->b.values.front() = previous + 1.0F;
  require(encode<Protocol>(decoded) == bytes, "A decoded payload still follows its previous owner");
  source->b.values.front() = previous;
}

} // namespace

// Export live payload buffers by reference, then load their ordinary owning wire representation.
int main() {
  verify_independent_shared_value();
  auto source = std::make_shared<owner>();
  source->b.values = {0.0F, 1.0F, 2.0F};
  source->b.codes = {0, 1, 2};
  borrowed_consumer borrowed;
  borrowed.d = source->b;
  shared_consumer projected;
  projected.d.bind(source);
  bool invalid_owner{};
  try { projected.d.bind(std::shared_ptr<const owner>{}); }
  catch (const std::invalid_argument&) { invalid_owner = true; }
  require(invalid_owner && projected.d.is_bound(), "An invalid owner must preserve the previous shared binding");
  weak_consumer weak;
  weak.d.bind(source);

  projected.d.with_value([&](const payload& value) {
    require(std::addressof(value) == std::addressof(source->b), "A reference must select the original payload");
    require(value.values.data() == source->b.values.data(), "A reference must borrow the values buffer");
    require(value.codes.data() == source->b.codes.data(), "A reference must borrow the codes buffer");
  });
  source->b.values.front() = 4.5F;
  borrowed.d.with_value([](const payload& value) {
    require(value.values.front() == 4.5F, "Live owner edits must remain visible");
  });
  verify_protocol<rohit::serializer::json>(projected, borrowed, weak, source);
  verify_protocol<rohit::serializer::binary_none>(projected, borrowed, weak, source);
  verify_protocol<rohit::serializer::binary_integer>(projected, borrowed, weak, source);
  verify_protocol<rohit::serializer::binary_string>(projected, borrowed, weak, source);
  std::cout << encode<rohit::serializer::json>(projected) << '\n';

  borrowed.d.unbind();
  const std::weak_ptr<owner> observer{source};
  source.reset();
  require(projected.d.is_bound() && weak.d.is_bound(), "Shared projection must retain the owner");
  projected.d.with_value([&](const payload& value) {
    projected.d.unbind();
    require(!observer.expired() && weak.d.is_bound(), "A callback must pin its aliased owner after self-unbind");
    require(value.values.front() == 4.5F, "A pinned callback must retain the original field");
  });
  require(observer.expired() && !weak.d.is_bound(), "Weak projection must observe the final owner release");
  bool expired{};
  try { weak.d.with_value([](const payload&) {}); }
  catch (const std::logic_error&) { expired = true; }
  require(expired, "An expired weak source must fail instead of exporting an owned value");
  bool unbound{};
  try { static_cast<void>(encode<rohit::serializer::json>(borrowed)); }
  catch (const std::logic_error&) { unbound = true; }
  require(unbound, "An unbound borrow must fail without constructing a default payload");
  return 0;
}
