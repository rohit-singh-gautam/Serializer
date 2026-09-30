#include "stream_test_support.hpp"

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
    std::filesystem::remove_all(directory_, ignored);
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
