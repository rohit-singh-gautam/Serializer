#include "stream_test_support.hpp"
#include "../src/native_file_path.hpp"

#include <gtest/gtest.h>
#include <person.hpp>
#include <rohit/file_stream.hpp>
#include <rohit/managed_journal_stream.hpp>
#include <rohit/serializer.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace managed = rohit::managed;

static_assert(rohit::type_check::input_stream<rohit::file_stream>);
static_assert(rohit::type_check::output_stream<rohit::file_stream>);
static_assert(rohit::type_check::durable_output_stream<rohit::file_stream>);
static_assert(!rohit::type_check::durable_output_stream<std::ofstream>);
static_assert(!rohit::type_check::durable_output_stream<rohit::full_stream_auto_alloc>);

// Keep every physical fixture inside a directory owned solely by this test instance.
class file_stream_test : public testing::Test {
protected:
  std::filesystem::path directory_{};
  // Create an isolated directory before each test.
  void SetUp() override {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
                 ("serializer_file_stream_" + std::to_string(stamp));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
  }
  // Release only this fixture's directory after all local stream owners are destroyed.
  void TearDown() override {
    std::error_code ignored;
    const rohit::detail::native_file_path native{directory_, true};
    std::filesystem::remove_all(native.get(), ignored);
  }
  // Name one fixture file under the owned directory.
  std::filesystem::path path(const char* name = "data") const {
    return directory_ / name;
  }
};

// Verify generated serialization uses the existing adapters and produces exactly the memory bytes.
template <template <codec::serialize_type> class Protocol>
void check_file_codec(const std::filesystem::path& path) {
  const test::test1::person original{std::string(200000, 'x'), 12345};
  rohit::full_stream_auto_alloc memory;
  original.serialize_out<Protocol>(memory);
  {
    rohit::file_stream file{path, rohit::file_open_mode::create};
    test::test1::person::serialize<Protocol>(file, original);
    EXPECT_EQ(file.size(), memory.current_offset());
    file.sync();
    file.seek(0);
    std::vector<std::uint8_t> bytes(memory.current_offset());
    rohit::read_stream_exact(file, bytes);
    EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), memory.begin()));
    file.seek(0);
    const auto decoded = test::test1::person::deserialize<Protocol>(file);
    EXPECT_EQ(decoded.name, original.name);
    EXPECT_EQ(decoded.id, original.id);
  }
  rohit::file_stream reopened{path, rohit::file_open_mode::read};
  const auto decoded = test::test1::person::deserialize<Protocol>(reopened);
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);
}

// Exercise all native protocols, implicit batching, explicit sync, and reopening.
TEST_F(file_stream_test, generated_serialization_uses_file_stream) {
  check_file_codec<codec::json>(path("json"));
  check_file_codec<codec::binary_integer>(path("integer"));
  check_file_codec<codec::binary_none>(path("none"));
  check_file_codec<codec::binary_string>(path("string"));
}

// Explicit shared reads coexist with the synchronized writer without moving its append cursor.
TEST_F(file_stream_test, shared_read_preserves_writer_ownership_and_position) {
  rohit::file_stream lock{path("lock"), rohit::file_open_mode::lock};
  rohit::file_stream writer{path(), rohit::file_open_mode::create};
  writer.write("abc", 3);
  writer.sync();
  rohit::file_stream reader{path(), rohit::file_open_mode::read_shared};
  std::array<char, 6> bytes{};
  reader.read(bytes.data(), 3);
  EXPECT_EQ(std::string(bytes.data(), 3), "abc");
#ifdef _WIN32
  EXPECT_THROW((rohit::file_stream{path(), rohit::file_open_mode::read}), std::system_error);
  EXPECT_THROW((rohit::file_stream{path(), rohit::file_open_mode::update}), std::system_error);
#endif
  writer.write("def", 3);
  writer.sync();
  reader.seek(0);
  reader.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  EXPECT_EQ(std::string(bytes.data(), bytes.size()), "abcdef");
}

// EOF/peek state must not shift logical reads or subsequent writes, and recovery can remove a tail.
TEST_F(file_stream_test, peek_seek_truncate_and_append_preserve_position) {
  rohit::file_stream file{path(), rohit::file_open_mode::create};
  file.write("abc", 3);
  file.seek(0);
  EXPECT_EQ(file.peek(), 'a');
  EXPECT_EQ(file.peek(), 'a');
  file.write("X", 1);
  file.seek(0);
  std::array<char, 5> data{};
  file.read(data.data(), static_cast<std::streamsize>(data.size()));
  EXPECT_EQ(file.gcount(), 3);
  EXPECT_EQ(std::string(data.data(), 3), "Xbc");
  EXPECT_TRUE(file.eof());
  EXPECT_TRUE(file.fail());
  EXPECT_FALSE(file.bad());
  EXPECT_EQ(file.peek(), std::char_traits<char>::eof());
  file.truncate(1);
  EXPECT_FALSE(file.fail());
  file.write("Y", 1);
  file.sync();
  EXPECT_EQ(file.size(), 2u);
  file.seek(0);
  EXPECT_EQ(file.peek(), 'X');
  file.read(data.data(), 1);
  EXPECT_EQ(data[0], 'X');
  file.read(data.data(), 1);
  EXPECT_EQ(data[0], 'Y');
  EXPECT_FALSE(file.eof());
  EXPECT_EQ(file.peek(), std::char_traits<char>::eof());
  EXPECT_TRUE(file.eof());
  EXPECT_FALSE(file.fail());
}

// Creation is exclusive, locks release on destruction, and native errors remain distinguishable.
TEST_F(file_stream_test, ownership_and_io_failures_are_explicit) {
  {
    rohit::file_stream file{path(), rohit::file_open_mode::create};
    file.write("x", 1);
    file.sync();
    EXPECT_THROW((rohit::file_stream{path(), rohit::file_open_mode::create}), std::system_error);
    EXPECT_THROW(file.seek(std::numeric_limits<std::uint64_t>::max()), std::length_error);
    EXPECT_EQ(file.size(), 1u);
  }
  rohit::file_stream input{path(), rohit::file_open_mode::read};
  EXPECT_THROW(input.write("y", 1), std::system_error);
  EXPECT_TRUE(input.fail());
  EXPECT_TRUE(input.bad());
  EXPECT_THROW(input.sync(), std::ios_base::failure);
  {
    rohit::file_stream lock{path("lock"), rohit::file_open_mode::lock};
    EXPECT_THROW((rohit::file_stream{path("lock"), rohit::file_open_mode::lock}),
                 std::system_error);
  }
  EXPECT_NO_THROW((rohit::file_stream{path("lock"), rohit::file_open_mode::lock}));
}

constexpr std::array<std::uint8_t, 3> sample_payload{3, 7, 11};

// Build two chained records into any supported stream without materializing an intermediate frame.
template <rohit::type_check::output_stream Output>
void write_pair(Output& output) {
  const auto first = managed::write_journal_frame(output, {sample_payload}, 1, 0);
  managed::write_journal_frame(output, {sample_payload}, 2, first);
}

// Recovery consumes one frame at a time, including from non-seekable sources.
template <rohit::type_check::input_stream Input>
void read_pair(Input& input) {
  const auto first = managed::read_journal_frame(input, 1, 0);
  ASSERT_TRUE(first);
  EXPECT_TRUE(std::equal(first->payload().begin(), first->payload().end(), sample_payload.begin(),
                         sample_payload.end()));
  const auto second = managed::read_journal_frame(input, 2, first->digest);
  ASSERT_TRUE(second);
  EXPECT_TRUE(std::equal(second->payload().begin(), second->payload().end(), sample_payload.begin(),
                         sample_payload.end()));
  EXPECT_FALSE(managed::read_journal_frame(input, 3, second->digest));
}

// Contiguous custom/native buffers and standard/file byte streams share one exact wire contract.
TEST_F(file_stream_test, journal_frames_interoperate_across_streams) {
  stream_test::output_buffer custom;
  write_pair(custom);
  const auto expected = custom.bytes();
  const stream_test::input_cursor custom_input{expected};
  read_pair(custom_input);
  rohit::full_stream_auto_alloc memory;
  write_pair(memory);
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(memory.begin()), memory.current_offset()),
            expected);
  const auto input = rohit::make_constant_full_stream(memory.begin(), memory.current_offset());
  const auto borrowed = managed::read_journal_frame(input, 1, 0);
  ASSERT_TRUE(borrowed);
  EXPECT_EQ(borrowed->payload().data(), memory.begin() + managed::detail::frame_header_bytes);
  std::stringstream io;
  write_pair(io);
  EXPECT_EQ(io.str(), expected);
  std::istream& erased = io;
  read_pair(erased);
  {
    std::ofstream output{path("standard"), std::ios::binary};
    write_pair(output);
  }
  std::ifstream standard_input{path("standard"), std::ios::binary};
  read_pair(standard_input);
  {
    rohit::file_stream file{path(), rohit::file_open_mode::create};
    write_pair(file);
    file.sync();
    file.seek(0);
    std::vector<std::uint8_t> bytes(expected.size());
    rohit::read_stream_exact(file, bytes);
    EXPECT_EQ(std::string(bytes.begin(), bytes.end()), expected);
    file.seek(0);
    read_pair(file);
  }
}

// An input stream whose buffer rejects all seeks confirms framing never seeks or reads to EOF.
class sequential_input : public std::streambuf {
  std::string bytes_;

public:
  // Own bytes and expose only the sequential get area.
  explicit sequential_input(std::string bytes) : bytes_{std::move(bytes)} {
    setg(bytes_.data(), bytes_.data(), bytes_.data() + bytes_.size());
  }
};

// Truncation at every byte is recoverable; corruption and sequencing errors must still fail.
TEST(journal_stream, bounded_records_handle_partial_tails_and_reject_damage) {
  std::ostringstream output;
  const auto digest = managed::write_journal_frame(output, {sample_payload}, 1, 0);
  const auto encoded = output.str();
  for (std::size_t count = 0; count < encoded.size(); ++count) {
    std::istringstream input{encoded.substr(0, count)};
    input.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
    EXPECT_FALSE(managed::read_journal_frame(input, 1, 0)) << count;
    const stream_test::input_cursor buffer{std::string_view{encoded}.substr(0, count)};
    EXPECT_FALSE(managed::read_journal_frame(buffer, 1, 0)) << count;
  }
  for (const auto index :
       {std::size_t{0}, managed::detail::frame_header_bytes, encoded.size() - 1}) {
    auto damaged = encoded;
    damaged[index] ^= 1;
    std::istringstream input{damaged};
    EXPECT_THROW(managed::read_journal_frame(input, 1, 0), std::invalid_argument);
  }
  std::istringstream wrong_sequence{encoded};
  EXPECT_THROW(managed::read_journal_frame(wrong_sequence, 2, 0), std::invalid_argument);
  managed::journal_options tiny;
  tiny.max_record_bytes = sample_payload.size() - 1;
  std::ostringstream untouched;
  EXPECT_THROW(managed::write_journal_frame(untouched, {sample_payload}, 1, 0, tiny),
               std::length_error);
  EXPECT_TRUE(untouched.str().empty());
  std::istringstream excessive{encoded};
  EXPECT_THROW(managed::read_journal_frame(excessive, 1, 0, tiny), std::length_error);
  EXPECT_EQ(excessive.tellg(), static_cast<std::streamoff>(managed::detail::frame_header_bytes));
  std::ostringstream pair;
  write_pair(pair);
  sequential_input sequential{pair.str()};
  std::istream input{&sequential};
  const auto first = managed::read_journal_frame(input, 1, 0);
  ASSERT_TRUE(first);
  EXPECT_EQ(first->digest, digest);
  auto owned_copy = *first;
  EXPECT_NE(owned_copy.payload().data(), first->payload().data());
  const auto second = managed::read_journal_frame(input, 2, first->digest);
  ASSERT_TRUE(second);
  EXPECT_FALSE(input.eof());
}

// Expose flush calls separately from byte delivery and simulate stream-buffer failures.
class observing_buffer : public std::stringbuf {
public:
  int flushes{};
  bool fail_flush{};
  // Record a normal stream flush; this is deliberately not a durable sync implementation.
  int sync() override {
    ++flushes;
    return fail_flush ? -1 : 0;
  }
};

// Adapter sync explicitly drains normal and Serializer buffers before invoking the host policy.
TEST(journal_stream, durable_adapter_orders_flush_and_fences_sync_errors) {
  observing_buffer buffer;
  std::ostream stream{&buffer};
  int synchronizations{};
  bool fail_sync{};
  rohit::durable_output_adapter output{stream, [&] {
                                         EXPECT_EQ(buffer.flushes, synchronizations + 1);
                                         EXPECT_FALSE(buffer.str().empty());
                                         ++synchronizations;
                                         if (fail_sync) {
                                           throw std::runtime_error{"injected sync failure"};
                                         }
                                       }};
  static_assert(rohit::type_check::durable_output_stream<decltype(output)>);
  managed::write_journal_frame(output, {sample_payload}, 1, 0);
  EXPECT_EQ(buffer.flushes, 0);
  EXPECT_EQ(synchronizations, 0);
  output.sync();
  EXPECT_EQ(synchronizations, 1);
  fail_sync = true;
  EXPECT_THROW(output.sync(), std::runtime_error);
  EXPECT_TRUE(output.fail());
  EXPECT_THROW(output.write("x", 1), std::ios_base::failure);
  EXPECT_THROW(output.sync(), std::ios_base::failure);
  EXPECT_EQ(synchronizations, 2);

  std::ostringstream destination;
  rohit::buffered_output_stream scratch{destination};
  int drained{};
  rohit::durable_output_adapter buffered{scratch, [&] {
                                           EXPECT_EQ(destination.str(), "abc");
                                           ++drained;
                                         }};
  buffered.write("abc", 3);
  EXPECT_TRUE(destination.str().empty());
  buffered.sync();
  EXPECT_EQ(drained, 1);
}

// A failed iostream flush must never reach the durability callback.
TEST(journal_stream, failed_stream_flush_prevents_durable_acknowledgment) {
  observing_buffer buffer;
  buffer.fail_flush = true;
  std::ostream stream{&buffer};
  bool called{};
  rohit::durable_output_adapter output{stream, [&] { called = true; }};
  output.write("x", 1);
  EXPECT_THROW(output.sync(), std::ios_base::failure);
  EXPECT_FALSE(called);
  EXPECT_TRUE(output.fail());
}

// Explicit errors and short reads without EOF are not accepted as recoverable tails.
TEST(journal_stream, stream_errors_are_not_incomplete_records) {
  std::istringstream input{"x"};
  input.setstate(std::ios::badbit);
  EXPECT_THROW(managed::read_journal_frame(input, 1, 0), std::ios_base::failure);
  std::ostringstream output;
  output.setstate(std::ios::badbit);
  EXPECT_THROW(managed::write_journal_frame(output, {sample_payload}, 1, 0),
               std::ios_base::failure);
}
} // namespace

#ifdef _WIN32
// Prefix conversion preserves UTF-16/UNC and explicit namespace semantics without network I/O.
TEST(native_file_path, windows_spelling_and_logical_identity_are_separate) {
  const auto short_path = std::filesystem::absolute("native-path-short");
  const rohit::detail::native_file_path borrowed{short_path};
  EXPECT_EQ(&borrowed.get(), &short_path);
  EXPECT_FALSE(borrowed.prefix_added());
  const auto absolute = short_path.parent_path() / L"unicode-\u03a9-\u4e2d" /
      std::wstring(140, L'x') / std::wstring(140, L'y');
  const rohit::detail::native_file_path extended{absolute};
  EXPECT_TRUE(extended.get().native().starts_with(LR"(\\?\)"));
  EXPECT_EQ(extended.get().native().substr(4), absolute.native());
  const auto unc = std::filesystem::path{LR"(\\server\share\)"} /
      std::wstring(140, L'x') / (std::wstring(140, L'y') + L"-\u03a9");
  const rohit::detail::native_file_path unc_native{unc};
  EXPECT_EQ(unc_native.get().native(), std::wstring{LR"(\\?\UNC\)"} + unc.native().substr(2));
  const auto explicit_path = std::filesystem::path{LR"(\\?\C:\literal. )"};
  const rohit::detail::native_file_path explicit_native{explicit_path};
  EXPECT_EQ(&explicit_native.get(), &explicit_path);
  EXPECT_FALSE(explicit_native.prefix_added());
}

// Real native files cross MAX_PATH with Unicode, stable lock exclusion and atomic retry semantics.
TEST_F(file_stream_test, extended_length_native_io_lock_and_publication) {
  auto base = directory_ / L"native-\u03a9-\u4e2d";
  while (base.native().size() < 210) { base /= "level-0123456789"; }
  ASSERT_LT(base.native().size() + 1, 249u);
  const rohit::detail::native_file_path native_directory{base, true};
  ASSERT_TRUE(std::filesystem::create_directories(native_directory.get()));
  constexpr std::array lengths{249u, 259u, 260u, 271u, 320u};
  const auto read_native = [](const std::filesystem::path& path) {
    rohit::file_stream input{path, rohit::file_open_mode::read};
    std::string value(static_cast<std::size_t>(input.size()), '\0');
    input.read(value.data(), static_cast<std::streamsize>(value.size()));
    EXPECT_EQ(input.gcount(), static_cast<std::streamsize>(value.size()));
    return value;
  };
  // Intermediate spaces/dots can be literal Win32 directory names. Keep distinct
  // native owners so indiscriminate component trimming cannot silently alias them.
  constexpr std::array components{L"component", L"component ", L"component. ", L"component..."};
  constexpr auto intermediate_path_characters = std::size_t{271};
  for (std::size_t index = 0; index < components.size(); ++index) {
    const auto component = std::wstring_view{components[index]};
    const auto literal_directory = native_directory.get() / component;
    ASSERT_TRUE(std::filesystem::create_directory(literal_directory));
    const auto logical_directory = base / component;
    ASSERT_LT(logical_directory.native().size() + 1, intermediate_path_characters);
    const auto filename = std::wstring(intermediate_path_characters - logical_directory.native().size() - 1, L'p');
    const auto literal_file = literal_directory / filename;
    const auto logical_file = logical_directory / filename;
    const auto value = "component-" + std::to_string(index);
    {
      rohit::file_stream output{literal_file, rohit::file_open_mode::create};
      output.write(value.data(), static_cast<std::streamsize>(value.size())); output.sync();
    }
    EXPECT_EQ(logical_file.native().size(), intermediate_path_characters);
    EXPECT_EQ(read_native(logical_file), value);
    if (index == 0) {
      const auto dotted_directory = base / L"component." / filename;
      EXPECT_EQ(read_native(dotted_directory), value);
    }
  }
  for (const auto length : lengths) {
    SCOPED_TRACE(length);
    const auto file = base / std::wstring(length - base.native().size() - 1, L'n');
    EXPECT_EQ(file.native().size(), length);
    {
      rohit::file_stream output{file, rohit::file_open_mode::create};
      output.write("before", 6); output.sync();
    }
    EXPECT_EQ(read_native(file), "before");
    auto dotted = file; dotted += L".";
    auto spaced = file; spaced += L" ";
    EXPECT_EQ(read_native(dotted), "before");
    EXPECT_EQ(read_native(spaced), "before");
    auto lock_path = file; lock_path += L".lock";
    rohit::file_stream lock{lock_path, rohit::file_open_mode::lock};
    const rohit::detail::native_file_path lock_native{lock_path, true};
    EXPECT_THROW((rohit::file_stream{lock_native.get(), rohit::file_open_mode::lock}), std::system_error);
    auto staged = file; staged += L".new";
    {
      rohit::file_stream output{staged, rohit::file_open_mode::create};
      output.write("after", 5); output.sync();
    }
    if (length == 271) {
      const rohit::detail::native_file_path destination{file};
      const auto held = CreateFileW(destination.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      ASSERT_NE(held, INVALID_HANDLE_VALUE);
      EXPECT_THROW(rohit::publish_file(staged, file, true), std::system_error);
      EXPECT_EQ(read_native(file), "before");
      EXPECT_NE(CloseHandle(held), 0);
    }
    rohit::publish_file(staged, file, true);
    EXPECT_EQ(read_native(file), "after");
    const rohit::detail::native_file_path source{staged};
    EXPECT_FALSE(std::filesystem::exists(source.get()));
    EXPECT_THROW((rohit::file_stream{lock_path, rohit::file_open_mode::lock}), std::system_error);
  }
}
#endif
