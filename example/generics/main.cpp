#include "model.hpp"

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

// Query the generated template constraint without constructing an invalid specialization.
template <class T>
concept accepts_number = requires { typename shared::number<T>; };

static_assert(accepts_number<std::int32_t> && accepts_number<double>);
static_assert(!accepts_number<bool> && !accepts_number<std::string>);

// Use the template aliases and named root, then decode into a fresh concrete model.
int main() {
  using item_type = shared::pair<std::uint32_t, std::string>;
  using batch_type = shared::box<item_type>;
  using envelope_type = shared::envelope<batch_type>;
  static_assert(std::is_same_v<decltype(example_model::payload), batch_type>);
  static_assert(std::is_same_v<decltype(shared::number<>::value), std::int32_t>);
  shared::number<double> measurement{};
  measurement.value = 12.5;
  if (measurement.value != 12.5) {
    throw std::runtime_error{"Constrained generic changed its value"};
  }

  item_type item{};
  item.key = 7;
  item.value = "portable generics";
  batch_type batch{};
  batch.value = item;
  batch.items.push_back(item);

  example_model original{};
  original.revision = 1;
  original.payload = batch;
  original.lookup.emplace(item.key, batch);
  rohit::full_stream_auto_alloc bytes{};
  original.serialize_out<rohit::serializer::binary_integer>(bytes);
  const auto input = rohit::make_constant_stream(bytes.begin(), bytes.current_offset());
  const auto decoded =
      rohit::serializer::deserialize_exact<envelope_type, rohit::serializer::binary_integer>(input);
  if (decoded.payload.value.value != item.value || decoded.lookup.at(7).items.size() != 1) {
    throw std::runtime_error{"Generic round trip changed the payload"};
  }
  std::cout << "revision=" << decoded.revision << ", key=" << decoded.payload.value.key
            << ", value=" << decoded.payload.value.value << '\n';
}
