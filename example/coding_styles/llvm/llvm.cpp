#include <account.hpp>

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <span>
#include <string_view>

// Exercise the llvm profile's renamed owning API, mutable view, and unchanged
// wire names.
int main() {
  using rohit::serializer::serialize_type;
  using rohit::serializer::storage_mode;
  using OwningRecord = style_demo::AccountRecord<storage_mode::owning>;
  using MutableRecord = style_demo::AccountRecord<storage_mode::mutable_view>;
  OwningRecord Record{};
  Record.AccountId = 42;
  Record.CurrentState = style_demo::AccountState::Active;

  rohit::full_stream_auto_alloc Bytes{};
  OwningRecord::serialize<rohit::serializer::binary_none>(Bytes, Record);
  auto Editor = MutableRecord::map(
      std::span<std::uint8_t>{Bytes.begin(), Bytes.current_offset()});
  Editor.setAccountId(43);
  if (Editor.getAccountId() != 43) {
    return 1;
  }

  OwningRecord Decoded{};
  const auto Input =
      rohit::make_constant_stream(Bytes.begin(), Bytes.current_offset());
  rohit::serializer::binary_none<serialize_type::in> Decoder{Input};
  Decoder.serialize_in(Decoded);
  Decoder.finish();
  if (Decoded.AccountId != 43 ||
      Decoded.CurrentState != style_demo::AccountState::Active) {
    return 1;
  }
  const auto FactoryInput =
      rohit::make_constant_stream(Bytes.begin(), Bytes.current_offset());
  const auto FactoryValue = OwningRecord::deserialize<rohit::serializer::binary_none>(
      FactoryInput, rohit::serializer::decode_limits{});
  if (FactoryValue.AccountId != Decoded.AccountId) {
    return 1;
  }
  rohit::full_stream_auto_alloc Json{};
  Decoded.serialize_out<rohit::serializer::json>(Json);
  const std::string_view Text{reinterpret_cast<const char *>(Json.begin()),
                              Json.current_offset()};
  return Text.find("\"accountID\":43") != std::string_view::npos &&
                 Text.find("\"currentState\":\"Active\"") !=
                     std::string_view::npos
             ? 0
             : 1;
}
