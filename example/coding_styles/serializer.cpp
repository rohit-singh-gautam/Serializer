#include <account.hpp>

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <span>
#include <string_view>

// Exercise the serializer profile's renamed owning API, mutable view, and unchanged wire names.
int main() {
  using rohit::serializer::serialize_type;
  using rohit::serializer::storage_mode;
  using owning_record = style_demo::account_record<storage_mode::owning>;
  using mutable_record = style_demo::account_record<storage_mode::mutable_view>;
  owning_record record{};
  record.account_id = 42;
  record.current_state = style_demo::account_state::active;

  rohit::full_stream_auto_alloc bytes{};
  record.serialize_out<rohit::serializer::binary_none>(bytes);
  auto editor = mutable_record::map(std::span<std::uint8_t>{bytes.begin(), bytes.current_offset()});
  editor.set_account_id(43);
  if (editor.get_account_id() != 43) {
    return 1;
  }

  owning_record decoded{};
  const auto input = rohit::make_constant_stream(bytes.begin(), bytes.current_offset());
  rohit::serializer::binary_none<serialize_type::in> decoder{input};
  decoder.serialize_in(decoded);
  decoder.finish();
  if (decoded.account_id != 43 || decoded.current_state != style_demo::account_state::active) {
    return 1;
  }
  rohit::full_stream_auto_alloc json{};
  decoded.serialize_out<rohit::serializer::json>(json);
  const std::string_view text{reinterpret_cast<const char*>(json.begin()), json.current_offset()};
  return text.find("\"accountID\":43") != std::string_view::npos &&
                 text.find("\"currentState\":\"Active\"") != std::string_view::npos
             ? 0
             : 1;
}
