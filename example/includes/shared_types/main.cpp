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
  return decoded.owner.id == source.owner.id && decoded.message == source.message &&
         decoded.owner.state == common::account_state::active;
}

// Run the include example through every standard wire protocol.
int main() {
  demo::request source{};
  source.owner.id = 42;
  source.message = "shared schema";
  return round_trip<rohit::serializer::json>(source) &&
                 round_trip<rohit::serializer::binary_none>(source) &&
                 round_trip<rohit::serializer::binary_integer>(source) &&
                 round_trip<rohit::serializer::binary_string>(source)
             ? 0
             : 1;
}
