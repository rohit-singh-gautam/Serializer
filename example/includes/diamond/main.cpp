#include <request.hpp>

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

// Verify included types through one of the existing JSON or binary protocols.
template <template <rohit::serializer::serialize_type> typename Protocol>
bool round_trip(const demo::request& source) {
  rohit::full_stream_auto_alloc output{};
  source.serialize_out<Protocol>(output);
  const auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
  demo::request decoded{};
  decoded.serialize_in<Protocol>(input);
  return decoded.first.owner.id == source.first.owner.id && decoded.second.id == source.second.id &&
         decoded.second.label == source.second.label;
}

// Run the include example through every standard wire protocol.
int main() {
  demo::request source{};
  source.first.owner.id = 42;
  source.second.id = 73;
  source.second.label = "diamond include";
  return round_trip<rohit::serializer::json>(source) &&
                 round_trip<rohit::serializer::binary_none>(source) &&
                 round_trip<rohit::serializer::binary_integer>(source) &&
                 round_trip<rohit::serializer::binary_string>(source)
             ? 0
             : 1;
}
