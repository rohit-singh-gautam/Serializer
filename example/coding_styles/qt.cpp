#include <account.hpp>

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <span>
#include <string_view>

// Exercise the qt profile's renamed owning API, mutable view, and unchanged wire names.
int main()
{
    using rohit::serializer::serialize_type;
    using rohit::serializer::storage_mode;
    using OwningRecord = style_demo::AccountRecord<storage_mode::owning>;
    using MutableRecord = style_demo::AccountRecord<storage_mode::mutable_view>;
    OwningRecord record{};
    record.accountId = 42;
    record.currentState = style_demo::AccountState::Active;

    rohit::full_stream_auto_alloc bytes{};
    record.serialize_out<rohit::serializer::binary_none>(bytes);
    auto editor
        = MutableRecord::map(std::span<std::uint8_t>{bytes.begin(), bytes.current_offset()});
    editor.setAccountId(43);
    if (editor.getAccountId() != 43) {
        return 1;
    }

    OwningRecord decoded{};
    const auto input = rohit::make_constant_stream(bytes.begin(), bytes.current_offset());
    rohit::serializer::binary_none<serialize_type::in> decoder{input};
    decoder.serialize_in(decoded);
    decoder.finish();
    if (decoded.accountId != 43 || decoded.currentState != style_demo::AccountState::Active) {
        return 1;
    }
    rohit::full_stream_auto_alloc json{};
    decoded.serialize_out<rohit::serializer::json>(json);
    const std::string_view text{reinterpret_cast<const char *>(json.begin()),
                                json.current_offset()};
    return text.find("\"accountID\":43") != std::string_view::npos
                   && text.find("\"currentState\":\"Active\"") != std::string_view::npos
               ? 0
               : 1;
}
