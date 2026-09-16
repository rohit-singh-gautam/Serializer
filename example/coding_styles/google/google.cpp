#include <account.hpp>
#include <cstdint>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>
#include <span>
#include <string_view>

// Exercise the google profile's renamed owning API, mutable view, and unchanged
// wire names.
int main() {
  using rohit::serializer::serialize_type;
  using rohit::serializer::storage_mode;
  using OwningRecord = style_demo::AccountRecord<storage_mode::owning>;
  using MutableRecord = style_demo::AccountRecord<storage_mode::mutable_view>;
  OwningRecord record{};
  record.account_id_ = 42;
  record.current_state_ = style_demo::AccountState::kActive;

  rohit::full_stream_auto_alloc bytes{};
  record.serialize_out<rohit::serializer::binary_none>(bytes);
  auto editor = MutableRecord::map(
      std::span<std::uint8_t>{bytes.begin(), bytes.current_offset()});
  editor.SetAccountId(43);
  if (editor.GetAccountId() != 43) {
    return 1;
  }

  OwningRecord decoded{};
  const auto input =
      rohit::make_constant_stream(bytes.begin(), bytes.current_offset());
  rohit::serializer::binary_none<serialize_type::in> decoder{input};
  decoder.serialize_in(decoded);
  decoder.finish();
  if (decoded.account_id_ != 43 ||
      decoded.current_state_ != style_demo::AccountState::kActive) {
    return 1;
  }
  rohit::full_stream_auto_alloc json{};
  decoded.serialize_out<rohit::serializer::json>(json);
  const std::string_view text{reinterpret_cast<const char*>(json.begin()),
                              json.current_offset()};
  return text.find("\"accountID\":43") != std::string_view::npos &&
                 text.find("\"currentState\":\"Active\"") !=
                     std::string_view::npos
             ? 0
             : 1;
}
