// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/package.hpp>

#include <rohit/compression.hpp>
#include <rohit/file_stream.hpp>
#include <rohit/utf8.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <istream>
#include <limits>
#include <ostream>
#include <streambuf>
#include <utility>

namespace rohit::serializer::package {
namespace {
constexpr std::uint32_t local_signature = 0x04034b50;
constexpr std::uint32_t central_signature = 0x02014b50;
constexpr std::uint32_t end_signature = 0x06054b50;
constexpr std::uint32_t descriptor_signature = 0x08074b50;
constexpr std::uint32_t zip64_locator_signature = 0x07064b50;
constexpr std::uint16_t zip64_extra_id = 1;
constexpr std::uint16_t utf8_flag = 0x0800;
constexpr std::uint16_t descriptor_flag = 0x0008;
constexpr std::uint16_t supported_flags = utf8_flag | descriptor_flag | 0x0006;
constexpr std::uint16_t zip_version = 20;
constexpr std::uint16_t dos_epoch_date = 0x0021;
constexpr std::size_t local_header_bytes = 30;
constexpr std::size_t central_header_bytes = 46;
constexpr std::size_t end_header_bytes = 22;
constexpr std::size_t descriptor_bytes = 12;
constexpr std::size_t io_chunk_bytes = 64 * 1024;
constexpr auto zip16_limit = std::numeric_limits<std::uint16_t>::max();
constexpr auto zip32_limit = std::numeric_limits<std::uint32_t>::max();

// Read a little-endian ZIP integer only after the containing fixed record is bounded.
template <typename UInt>
UInt read_integer(std::span<const std::uint8_t> bytes, std::size_t offset) {
  UInt result{};
  for (std::size_t index = 0; index < sizeof(UInt); ++index) {
    result |= static_cast<UInt>(static_cast<UInt>(bytes[offset + index]) << (index * 8));
  }
  return result;
}

// Append a platform-independent little-endian wire integer.
template <typename UInt>
void write_integer(std::vector<std::uint8_t>& bytes, UInt value) {
  for (std::size_t index = 0; index < sizeof(UInt); ++index) {
    bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
  }
}

// Build the IEEE CRC table at compile time; stored ZIP has no compression dependency.
constexpr auto make_crc_table() {
  std::array<std::uint32_t, 256> result{};
  constexpr std::uint32_t polynomial = 0xedb88320;
  for (std::size_t index = 0; index < result.size(); ++index) {
    auto crc = static_cast<std::uint32_t>(index);
    for (unsigned bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) != 0 ? polynomial : 0);
    }
    result[index] = crc;
  }
  return result;
}

// Compute the wire CRC with cancellation checks independent of the compression backend.
std::uint32_t calculate_crc(std::span<const std::uint8_t> bytes, const budget& shared) {
  static constexpr auto table = make_crc_table();
  auto crc = std::numeric_limits<std::uint32_t>::max();
  for (std::size_t offset = 0; offset < bytes.size();) {
    shared.check_cancelled();
    const auto count = std::min(io_chunk_bytes, bytes.size() - offset);
    for (const auto value : bytes.subspan(offset, count)) {
      crc = table[(crc ^ value) & 0xff] ^ (crc >> 8);
    }
    offset += count;
  }
  return ~crc;
}

// Bound ZIP32 fields without treating their sentinel as an ordinary maximum value.
std::uint32_t zip32(std::uint64_t value) {
  if (value >= zip32_limit) {
    throw error{error_code::unsupported_feature, "ZIP64 output is not supported"};
  }
  return static_cast<std::uint32_t>(value);
}

// Validate logical archive names without interpreting them as operating-system paths.
void validate_zip_name(std::string_view name, bool utf8 = true) {
  if (name.empty() || name.size() > zip16_limit || name.front() == '/' ||
      name.find_first_of("\\:\0", 0, 3) != std::string_view::npos) {
    throw error{error_code::invalid_name, "Invalid ZIP entry name"};
  }
  for (std::size_t begin = 0; begin < name.size();) {
    const auto end = name.find('/', begin);
    const auto segment = name.substr(begin, end == std::string_view::npos ? end : end - begin);
    if (segment.empty() || segment == "." || segment == "..") {
      throw error{error_code::invalid_name, "Invalid ZIP entry segment"};
    }
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  }
  if (utf8) {
    try {
      serializer::detail::validate_utf8(name);
    } catch (const std::invalid_argument&) {
      throw error{error_code::invalid_name, "ZIP entry name is not UTF-8"};
    }
  } else if (std::any_of(name.begin(), name.end(), [](unsigned char value) {
               return value >= 0x80;
             })) {
    throw error{error_code::unsupported_feature, "Legacy ZIP filename encodings are unsupported"};
  }
}

// Reject security-sensitive flags and unsupported compression before reading payload bytes.
void validate_features(std::uint16_t flags, std::uint16_t method) {
  if ((flags & ~supported_flags) != 0) {
    throw error{error_code::unsupported_feature, "Encrypted or unsupported ZIP flags"};
  }
  if (method != static_cast<std::uint16_t>(zip_method::stored) &&
      method != static_cast<std::uint16_t>(zip_method::deflate)) {
    throw error{error_code::unsupported_feature, "Unsupported ZIP compression method"};
  }
}

// Check all TLV extents, rejecting ZIP64 rather than silently ignoring its sizes.
void validate_extra(std::span<const std::uint8_t> bytes) {
  while (!bytes.empty()) {
    if (bytes.size() < 4) {
      throw error{error_code::invalid_archive, "Truncated ZIP extra field"};
    }
    const auto id = read_integer<std::uint16_t>(bytes, 0);
    const auto length = read_integer<std::uint16_t>(bytes, 2);
    if (id == zip64_extra_id) {
      throw error{error_code::unsupported_feature, "ZIP64 is not supported"};
    }
    if (length > bytes.size() - 4) {
      throw error{error_code::invalid_archive, "Invalid ZIP extra field length"};
    }
    bytes = bytes.subspan(4 + length);
  }
}

// Stage one complete producer output while refusing an over-limit stream write.
class part_buffer final : public std::streambuf {
  budget& budget_;
  const std::uint64_t maximum_;
public:
  std::vector<std::uint8_t> bytes{};
  // Borrow the operation budget for cancellation and bounded complete-message storage.
  explicit part_buffer(budget& shared)
      : budget_{shared}, maximum_{std::min(shared.configured().max_part_bytes,
                                         shared.configured().max_buffer_bytes)} {}
protected:
  // Append a complete stream write or fail without appending its over-limit suffix.
  std::streamsize xsputn(const char* input, std::streamsize count) override {
    budget_.check_cancelled();
    if (count < 0 || static_cast<std::uint64_t>(count) > maximum_ - bytes.size()) {
      throw error{error_code::resource_limit, "Complete part buffer exceeds limit", {}, 0,
                  count < 0 ? maximum_ : static_cast<std::uint64_t>(count), maximum_};
    }
    const auto* begin = reinterpret_cast<const std::uint8_t*>(input);
    bytes.insert(bytes.end(), begin, begin + count);
    return count;
  }
  // Handle individual characters through the same complete-part bound.
  int_type overflow(int_type value) override {
    if (traits_type::eq_int_type(value, traits_type::eof())) {
      return traits_type::not_eof(value);
    }
    const auto byte = traits_type::to_char_type(value);
    xsputn(&byte, 1);
    return value;
  }
};

// Forward standard-stream writes to the existing unbuffered durable file adapter.
class file_output_buffer final : public std::streambuf {
  rohit::file_stream& file_;
public:
  // Borrow a native file until all package writers and the standard stream are destroyed.
  explicit file_output_buffer(rohit::file_stream& file) : file_{file} {}
protected:
  // Complete the native write or propagate the file adapter's latched I/O failure.
  std::streamsize xsputn(const char* input, std::streamsize count) override {
    file_.write(input, count);
    return count;
  }
  // Preserve the same failure policy for individual standard-stream characters.
  int_type overflow(int_type value) override {
    if (traits_type::eq_int_type(value, traits_type::eof())) {
      return traits_type::not_eof(value);
    }
    const auto byte = traits_type::to_char_type(value);
    file_.write(&byte, 1);
    return value;
  }
};

// Stage a full file using exclusive native creation in the destination directory.
template <typename Writer>
void save_file(const std::filesystem::path& destination,
               const std::function<void(Writer&)>& producer, limits configured) {
  static std::atomic<std::uint64_t> sequence{};
  const auto absolute = std::filesystem::absolute(destination);
  const auto parent = absolute.parent_path();
  std::filesystem::path staging_path;
  std::unique_ptr<rohit::file_stream> staging_file;
  for (unsigned attempt = 0; attempt < 32; ++attempt) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto suffix = std::to_string(stamp) + "-" + std::to_string(sequence.fetch_add(1));
    staging_path = parent / (".serializer-save-" + suffix + ".tmp");
    try {
      staging_file = std::make_unique<rohit::file_stream>(staging_path, rohit::file_open_mode::create);
      break;
    } catch (const std::system_error& status) {
      if (status.code() != std::errc::file_exists) {
        throw error{error_code::io, "Cannot create full-Save staging file"};
      }
    }
  }
  if (!staging_file) {
    throw error{error_code::io, "Cannot allocate full-Save staging file"};
  }
  // Cleanup owns exactly the file exclusively created above; no recursive deletion is needed.
  struct staging_guard {
    std::filesystem::path path;
    std::unique_ptr<rohit::file_stream>& file;
    // Remove only this operation's staging file, when publication has not moved it.
    ~staging_guard() {
      file.reset();
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  } cleanup{staging_path, staging_file};
  auto shared = std::make_shared<budget>(std::move(configured));
  {
    file_output_buffer buffer{*staging_file};
    std::ostream output{&buffer};
    output.exceptions(std::ios::badbit | std::ios::failbit);
    Writer writer{output, shared};
    producer(writer);
    writer.finish();
    output.flush();
    staging_file->sync();
  }
  staging_file.reset();
  shared->check_cancelled();
  try {
    rohit::publish_file(staging_path, absolute, true);
  } catch (const std::exception&) {
    throw error{error_code::publication, "Full-Save destination replacement failed"};
  }
  try {
    rohit::sync_parent_directory(absolute);
  } catch (const std::exception&) {
    throw error{error_code::publication_uncertain,
                "Full Save was replaced but directory durability is uncertain"};
  }
}
} // namespace

// Store only metadata supplied by the caller's bounded package implementation.
error::error(error_code category, const char* message, std::string source,
             std::uint64_t offset, std::uint64_t needed, std::uint64_t maximum)
    : std::runtime_error{message}, code{category}, part{std::move(source)},
      offset_bytes{offset}, required{needed}, permitted{maximum} {}

// Initialize cumulative counters once for an entire operation.
budget::budget(limits configured) : limits_{std::move(configured)} {}

// Return immutable limits for nested providers.
const limits& budget::configured() const noexcept { return limits_; }

// Expose remaining capacity without altering shared accounting.
std::uint64_t budget::remaining(resource selected) const noexcept {
  switch (selected) {
  case resource::io_bytes: return limits_.max_io_bytes - io_bytes_;
  case resource::metadata_bytes: return limits_.max_metadata_bytes - metadata_bytes_;
  case resource::expanded_bytes: return limits_.max_total_expanded_bytes - expanded_bytes_;
  case resource::entries: return limits_.max_entries - entries_;
  case resource::relationships: return limits_.max_relationships - relationships_;
  }
  return 0;
}

// Poll cancellation without introducing implicit global policy.
void budget::check_cancelled() const {
  if (limits_.is_cancelled && limits_.is_cancelled()) {
    throw error{error_code::cancelled, "Package operation cancelled"};
  }
}

// Reject cumulative overflow before charging the selected category.
void budget::consume(resource selected, std::uint64_t count, std::string_view part_name) {
  check_cancelled();
  std::uint64_t* current{};
  std::uint64_t maximum{};
  switch (selected) {
  case resource::io_bytes: current = &io_bytes_; maximum = limits_.max_io_bytes; break;
  case resource::metadata_bytes: current = &metadata_bytes_; maximum = limits_.max_metadata_bytes; break;
  case resource::expanded_bytes:
    current = &expanded_bytes_; maximum = limits_.max_total_expanded_bytes; break;
  case resource::entries: current = &entries_; maximum = limits_.max_entries; break;
  case resource::relationships:
    current = &relationships_; maximum = limits_.max_relationships; break;
  }
  if (count > maximum - *current) {
    throw error{error_code::resource_limit, "Shared package budget exceeded",
                std::string{part_name}, 0, count, maximum - *current};
  }
  *current += count;
}

// Bound scratch memory separately from cumulative work and persistent metadata.
void budget::check_buffer(std::uint64_t count, std::string_view part_name) const {
  check_cancelled();
  if (count > limits_.max_buffer_bytes || count > std::numeric_limits<std::size_t>::max()) {
    throw error{error_code::resource_limit, "Package buffer limit exceeded",
                std::string{part_name}, 0, count, limits_.max_buffer_bytes};
  }
}

// Borrow the destination while preserving caller-supplied operation accounting.
zip_writer::zip_writer(std::ostream& output, std::shared_ptr<budget> shared)
    : output_{output}, budget_{shared ? std::move(shared) : std::make_shared<budget>()} {}

// Enforce archive/I/O bounds before every bounded native stream write.
void zip_writer::append(std::span<const std::uint8_t> bytes) {
  if (bytes.size() > budget_->configured().max_archive_bytes - offset_bytes_) {
    throw error{error_code::resource_limit, "ZIP archive exceeds output limit"};
  }
  budget_->consume(resource::io_bytes, bytes.size());
  for (std::size_t position = 0; position < bytes.size();) {
    budget_->check_cancelled();
    const auto count = std::min(io_chunk_bytes, bytes.size() - position);
    output_.write(reinterpret_cast<const char*>(bytes.data() + position),
                  static_cast<std::streamsize>(count));
    if (!output_) {
      throw error{error_code::io, "ZIP output stream failed", {}, offset_bytes_};
    }
    position += count;
    offset_bytes_ += count;
  }
}

// Write local headers with known sizes, keeping the final directory explicit.
void zip_writer::add(std::string_view name, std::span<const std::uint8_t> bytes,
                     entry_options options) {
  if (finished_ || failed_) {
    throw error{error_code::invalid_state, "ZIP writer is finished or failed"};
  }
  validate_zip_name(name);
  validate_features(utf8_flag, static_cast<std::uint16_t>(options.method));
  if (std::any_of(entries_.begin(), entries_.end(), [name](const auto& item) {
        return item.name == name;
      })) {
    throw error{error_code::duplicate_name, "Duplicate ZIP entry", std::string{name}};
  }
  if (bytes.size() > budget_->configured().max_part_bytes) {
    throw error{error_code::resource_limit, "ZIP part exceeds limit", std::string{name}};
  }
  if (entries_.size() + 1 >= zip16_limit) {
    throw error{error_code::unsupported_feature, "ZIP64 entry counts are unsupported"};
  }
  budget_->consume(resource::entries, 1, name);
  budget_->consume(resource::metadata_bytes, sizeof(zip_entry) + name.size(), name);
  budget_->consume(resource::expanded_bytes, bytes.size(), name);
  zip_entry entry{std::string{name}, options.method, bytes.size(), bytes.size(),
                  calculate_crc(bytes, *budget_), utf8_flag, offset_bytes_, 0};
  std::vector<std::uint8_t> compressed;
  auto payload = bytes;
  if (options.method == zip_method::deflate) {
    budget_->check_buffer(bytes.size(), name);
    const auto available = budget_->configured().max_buffer_bytes - bytes.size();
    try {
      compressed = compression::compress(bytes, compression::deflate_options{options.compression_level},
          {bytes.size(), static_cast<std::size_t>(std::min<std::uint64_t>(
               available, std::numeric_limits<std::size_t>::max()))});
    } catch (const compression::error& failure) {
      const auto category = failure.code == compression::error_code::unavailable
          ? error_code::unavailable : failure.code == compression::error_code::resource_limit
          ? error_code::resource_limit : error_code::invalid_state;
      throw error{category, "ZIP DEFLATE encoding failed", std::string{name}};
    }
    budget_->check_cancelled();
    payload = compressed;
    entry.compressed_bytes = payload.size();
  }
  const auto compressed_size = zip32(payload.size());
  const auto expanded_size = zip32(bytes.size());
  const auto header_offset = zip32(offset_bytes_);
  static_cast<void>(header_offset);
  std::vector<std::uint8_t> header;
  header.reserve(local_header_bytes + name.size());
  write_integer(header, local_signature);
  write_integer(header, zip_version);
  write_integer(header, utf8_flag);
  write_integer(header, static_cast<std::uint16_t>(options.method));
  write_integer(header, std::uint16_t{});
  write_integer(header, dos_epoch_date);
  write_integer(header, entry.crc32);
  write_integer(header, compressed_size);
  write_integer(header, expanded_size);
  write_integer(header, static_cast<std::uint16_t>(name.size()));
  write_integer(header, std::uint16_t{});
  header.insert(header.end(), name.begin(), name.end());
  entries_.push_back(std::move(entry));
  try {
    append(header);
    entries_.back().data_offset_bytes = offset_bytes_;
    append(payload);
  } catch (...) {
    failed_ = true;
    throw;
  }
}

// Buffer only the current complete codec output; an unsuccessful producer adds nothing.
void zip_writer::write(std::string_view name,
                       const std::function<void(std::ostream&)>& producer, entry_options options) {
  if (finished_ || failed_) {
    throw error{error_code::invalid_state, "ZIP writer is finished or failed"};
  }
  part_buffer buffer{*budget_};
  std::ostream output{&buffer};
  output.exceptions(std::ios::badbit | std::ios::failbit);
  producer(output);
  if (!output) {
    throw error{error_code::io, "Typed ZIP part producer failed"};
  }
  add(name, buffer.bytes, options);
}

// Emit central records in insertion order with fixed timestamps for reproducible ZIP32 framing.
void zip_writer::finish() {
  if (failed_) {
    throw error{error_code::invalid_state, "Cannot finalize failed ZIP writer"};
  }
  if (finished_) {
    return;
  }
  const auto directory_offset = zip32(offset_bytes_);
  try {
    for (const auto& entry : entries_) {
      std::vector<std::uint8_t> header;
      header.reserve(central_header_bytes + entry.name.size());
      write_integer(header, central_signature);
      write_integer(header, zip_version);
      write_integer(header, zip_version);
      write_integer(header, entry.flags);
      write_integer(header, static_cast<std::uint16_t>(entry.method));
      write_integer(header, std::uint16_t{});
      write_integer(header, dos_epoch_date);
      write_integer(header, entry.crc32);
      write_integer(header, zip32(entry.compressed_bytes));
      write_integer(header, zip32(entry.expanded_bytes));
      write_integer(header, static_cast<std::uint16_t>(entry.name.size()));
      write_integer(header, std::uint16_t{});
      write_integer(header, std::uint16_t{});
      write_integer(header, std::uint16_t{});
      write_integer(header, std::uint16_t{});
      write_integer(header, std::uint32_t{});
      write_integer(header, zip32(entry.header_offset_bytes));
      header.insert(header.end(), entry.name.begin(), entry.name.end());
      append(header);
    }
    const auto directory_bytes = zip32(offset_bytes_ - directory_offset);
    std::vector<std::uint8_t> end;
    write_integer(end, end_signature);
    write_integer(end, std::uint16_t{});
    write_integer(end, std::uint16_t{});
    const auto count = static_cast<std::uint16_t>(entries_.size());
    write_integer(end, count);
    write_integer(end, count);
    write_integer(end, directory_bytes);
    write_integer(end, directory_offset);
    write_integer(end, std::uint16_t{});
    append(end);
    finished_ = true;
  } catch (...) {
    failed_ = true;
    throw;
  }
}

// Expose the same budget to caller-selected codec checks.
const std::shared_ptr<budget>& zip_writer::shared_budget() const noexcept { return budget_; }

// Read bounded source ranges in chunks; no input prefix or trailing bytes are ignored.
std::vector<std::uint8_t> zip_reader::read_at(std::uint64_t offset, std::size_t count) {
  if (offset > archive_bytes_ || count > archive_bytes_ - offset) {
    throw error{error_code::invalid_archive, "ZIP range exceeds archive", {}, offset};
  }
  budget_->check_buffer(count);
  budget_->consume(resource::io_bytes, count);
  std::vector<std::uint8_t> bytes(count);
  input_.clear();
  input_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!input_) {
    throw error{error_code::io, "ZIP source is not seekable", {}, offset};
  }
  for (std::size_t position = 0; position < count;) {
    budget_->check_cancelled();
    const auto length = std::min(io_chunk_bytes, count - position);
    input_.read(reinterpret_cast<char*>(bytes.data() + position),
                static_cast<std::streamsize>(length));
    if (input_.gcount() != static_cast<std::streamsize>(length) || !input_) {
      throw error{error_code::io, "ZIP source read failed", {}, offset + position};
    }
    position += length;
  }
  return bytes;
}

// Index directory/local headers and validate extents without expanding unrelated entries.
zip_reader::zip_reader(std::istream& input, std::shared_ptr<budget> shared)
    : input_{input}, budget_{shared ? std::move(shared) : std::make_shared<budget>()} {
  input_.clear();
  input_.seekg(0, std::ios::end);
  const auto end = input_.tellg();
  if (end < 0) {
    throw error{error_code::unsupported_feature, "ZIP input requires a seekable stream"};
  }
  archive_bytes_ = static_cast<std::uint64_t>(end);
  if (archive_bytes_ > budget_->configured().max_archive_bytes) {
    throw error{error_code::resource_limit, "ZIP archive exceeds input limit"};
  }
  if (archive_bytes_ < end_header_bytes) {
    throw error{error_code::invalid_archive, "Truncated ZIP archive"};
  }
  const auto tail_size = static_cast<std::size_t>(std::min<std::uint64_t>(
      archive_bytes_, end_header_bytes + zip16_limit));
  const auto tail = read_at(archive_bytes_ - tail_size, tail_size);
  std::size_t end_position = tail.size();
  for (auto candidate = tail.size() - end_header_bytes;; --candidate) {
    if (read_integer<std::uint32_t>(tail, candidate) == end_signature &&
        read_integer<std::uint16_t>(tail, candidate + 20) ==
            tail.size() - candidate - end_header_bytes) {
      end_position = candidate;
      break;
    }
    if (candidate == 0) {
      break;
    }
  }
  if (end_position == tail.size()) {
    throw error{error_code::invalid_archive, "Missing ZIP end directory record"};
  }
  const auto end_record = std::span{tail}.subspan(end_position);
  const auto directory_end = archive_bytes_ - tail.size() + end_position;
  if (directory_end >= 20) {
    const auto locator = read_at(directory_end - 20, 4);
    if (read_integer<std::uint32_t>(locator, 0) == zip64_locator_signature) {
      throw error{error_code::unsupported_feature, "ZIP64 is not supported"};
    }
  }
  const auto count = read_integer<std::uint16_t>(end_record, 10);
  const auto directory_bytes = read_integer<std::uint32_t>(end_record, 12);
  const auto directory_offset = read_integer<std::uint32_t>(end_record, 16);
  if (count == zip16_limit || directory_bytes == zip32_limit || directory_offset == zip32_limit) {
    throw error{error_code::unsupported_feature, "ZIP64 is not supported"};
  }
  if (read_integer<std::uint16_t>(end_record, 4) != 0 ||
      read_integer<std::uint16_t>(end_record, 6) != 0 ||
      read_integer<std::uint16_t>(end_record, 8) != count) {
    throw error{error_code::unsupported_feature, "Split ZIP archives are unsupported"};
  }
  if (directory_offset > directory_end || directory_bytes != directory_end - directory_offset) {
    throw error{error_code::invalid_archive, "Invalid ZIP central directory extent"};
  }
  budget_->consume(resource::entries, count);
  budget_->consume(resource::metadata_bytes,
      static_cast<std::uint64_t>(count) * (sizeof(zip_entry) +
          sizeof(std::pair<std::uint64_t, std::uint64_t>)) + directory_bytes);
  entries_.reserve(count);
  const auto directory = read_at(directory_offset, directory_bytes);
  std::size_t position{};
  for (std::size_t index = 0; index < count; ++index) {
    budget_->check_cancelled();
    if (directory.size() - position < central_header_bytes) {
      throw error{error_code::invalid_archive, "Truncated ZIP central header"};
    }
    const auto header = std::span{directory}.subspan(position);
    if (read_integer<std::uint32_t>(header, 0) != central_signature) {
      throw error{error_code::invalid_archive, "Invalid ZIP central signature"};
    }
    const auto flags = read_integer<std::uint16_t>(header, 8);
    const auto method = read_integer<std::uint16_t>(header, 10);
    validate_features(flags, method);
    if (read_integer<std::uint16_t>(header, 6) > zip_version) {
      throw error{error_code::unsupported_feature, "Unsupported ZIP extraction version"};
    }
    const auto compressed = read_integer<std::uint32_t>(header, 20);
    const auto expanded = read_integer<std::uint32_t>(header, 24);
    const auto name_size = read_integer<std::uint16_t>(header, 28);
    const auto extra_size = read_integer<std::uint16_t>(header, 30);
    const auto comment_size = read_integer<std::uint16_t>(header, 32);
    const auto header_offset = read_integer<std::uint32_t>(header, 42);
    if (compressed == zip32_limit || expanded == zip32_limit || header_offset == zip32_limit) {
      throw error{error_code::unsupported_feature, "ZIP64 is not supported"};
    }
    if (read_integer<std::uint16_t>(header, 34) != 0) {
      throw error{error_code::unsupported_feature, "Split ZIP entry is unsupported"};
    }
    const auto variable_size = static_cast<std::size_t>(name_size) + extra_size + comment_size;
    if (variable_size > header.size() - central_header_bytes) {
      throw error{error_code::invalid_archive, "Truncated ZIP central names/extra fields"};
    }
    const std::string name{reinterpret_cast<const char*>(header.data() + central_header_bytes),
                           name_size};
    validate_zip_name(name, (flags & utf8_flag) != 0);
    if (find(name)) {
      throw error{error_code::duplicate_name, "Duplicate ZIP entry", name};
    }
    validate_extra(header.subspan(central_header_bytes + name_size, extra_size));
    if (expanded > budget_->configured().max_part_bytes) {
      throw error{error_code::resource_limit, "Declared ZIP part exceeds limit", name};
    }
    if (header_offset > directory_offset || local_header_bytes > directory_offset - header_offset) {
      throw error{error_code::invalid_archive, "ZIP local header overlaps directory", name};
    }
    entries_.push_back({name, static_cast<zip_method>(method), compressed, expanded,
        read_integer<std::uint32_t>(header, 16), flags, header_offset, 0});
    position += central_header_bytes + variable_size;
  }
  if (position != directory.size()) {
    throw error{error_code::invalid_archive, "Unexpected ZIP central directory records"};
  }
  std::vector<std::pair<std::uint64_t, std::uint64_t>> extents;
  extents.reserve(entries_.size());
  for (auto& entry : entries_) {
    const auto header = read_at(entry.header_offset_bytes, local_header_bytes);
    if (read_integer<std::uint32_t>(header, 0) != local_signature ||
        read_integer<std::uint16_t>(header, 6) != entry.flags ||
        read_integer<std::uint16_t>(header, 8) != static_cast<std::uint16_t>(entry.method)) {
      throw error{error_code::invalid_archive, "ZIP local and central headers disagree", entry.name};
    }
    const bool descriptor = (entry.flags & descriptor_flag) != 0;
    const auto local_crc = read_integer<std::uint32_t>(header, 14);
    const auto local_compressed = read_integer<std::uint32_t>(header, 18);
    const auto local_expanded = read_integer<std::uint32_t>(header, 22);
    if (local_compressed == zip32_limit || local_expanded == zip32_limit) {
      throw error{error_code::unsupported_feature, "ZIP64 is not supported", entry.name};
    }
    if ((!descriptor || local_crc != 0) && local_crc != entry.crc32) {
      throw error{error_code::invalid_archive, "ZIP local CRC disagrees", entry.name};
    }
    if (((!descriptor || local_compressed != 0) && local_compressed != entry.compressed_bytes) ||
        ((!descriptor || local_expanded != 0) && local_expanded != entry.expanded_bytes)) {
      throw error{error_code::invalid_archive, "ZIP local sizes disagree", entry.name};
    }
    const auto name_size = read_integer<std::uint16_t>(header, 26);
    const auto extra_size = read_integer<std::uint16_t>(header, 28);
    const auto variable_size = static_cast<std::size_t>(name_size) + extra_size;
    entry.data_offset_bytes = entry.header_offset_bytes + local_header_bytes + variable_size;
    if (entry.data_offset_bytes > directory_offset ||
        entry.compressed_bytes > directory_offset - entry.data_offset_bytes) {
      throw error{error_code::invalid_archive, "ZIP payload overlaps directory", entry.name};
    }
    const auto local_names = read_at(entry.header_offset_bytes + local_header_bytes, variable_size);
    const std::string_view local_name{reinterpret_cast<const char*>(local_names.data()), name_size};
    if (local_name != entry.name) {
      throw error{error_code::invalid_archive, "ZIP local filename disagrees", entry.name};
    }
    validate_extra(std::span{local_names}.subspan(name_size));
    auto extent_end = entry.data_offset_bytes + entry.compressed_bytes;
    if (descriptor) {
      if (descriptor_bytes > directory_offset - extent_end) {
        throw error{error_code::invalid_archive, "Truncated ZIP descriptor", entry.name};
      }
      auto values = read_at(extent_end, descriptor_bytes);
      const bool unsigned_matches = read_integer<std::uint32_t>(values, 0) == entry.crc32 &&
          read_integer<std::uint32_t>(values, 4) == entry.compressed_bytes &&
          read_integer<std::uint32_t>(values, 8) == entry.expanded_bytes;
      // A CRC can equal the optional signature; prefer an already matching unsigned descriptor.
      if (!unsigned_matches && read_integer<std::uint32_t>(values, 0) == descriptor_signature) {
        if (descriptor_bytes + 4 > directory_offset - extent_end) {
          throw error{error_code::invalid_archive, "Truncated signed ZIP descriptor", entry.name};
        }
        values = read_at(extent_end + 4, descriptor_bytes);
        extent_end += 4;
      }
      if (read_integer<std::uint32_t>(values, 0) != entry.crc32 ||
          read_integer<std::uint32_t>(values, 4) != entry.compressed_bytes ||
          read_integer<std::uint32_t>(values, 8) != entry.expanded_bytes) {
        throw error{error_code::integrity, "ZIP descriptor disagrees", entry.name};
      }
      extent_end += descriptor_bytes;
    }
    if (entry.method == zip_method::stored && entry.compressed_bytes != entry.expanded_bytes) {
      throw error{error_code::invalid_archive, "Stored ZIP sizes disagree", entry.name};
    }
    extents.emplace_back(entry.header_offset_bytes, extent_end);
  }
  std::sort(extents.begin(), extents.end());
  for (std::size_t index = 1; index < extents.size(); ++index) {
    if (extents[index].first < extents[index - 1].second) {
      throw error{error_code::invalid_archive, "Overlapping ZIP entry extents"};
    }
  }
}

// Return immutable, reader-owned central-directory metadata.
const std::vector<zip_entry>& zip_reader::entries() const noexcept { return entries_; }

// Preserve ZIP's exact case-sensitive logical filename identity.
const zip_entry* zip_reader::find(std::string_view name) const noexcept {
  const auto found = std::find_if(entries_.begin(), entries_.end(), [name](const auto& entry) {
    return entry.name == name;
  });
  return found == entries_.end() ? nullptr : &*found;
}

// Expand and validate one entry, retaining no partially validated result on failure.
std::vector<std::uint8_t> zip_reader::read(std::string_view name) {
  const auto* entry = find(name);
  if (!entry) {
    throw error{error_code::missing_part, "ZIP entry does not exist", std::string{name}};
  }
  budget_->consume(resource::expanded_bytes, entry->expanded_bytes, name);
  const auto scratch = entry->compressed_bytes +
      (entry->method == zip_method::deflate ? entry->expanded_bytes : 0);
  budget_->check_buffer(scratch, name);
  auto bytes = read_at(entry->data_offset_bytes, static_cast<std::size_t>(entry->compressed_bytes));
  if (entry->method == zip_method::deflate) {
    try {
      bytes = compression::decompress(bytes, {
          .format = compression::format::deflate,
          .max_compressed_bytes = static_cast<std::size_t>(entry->compressed_bytes),
          .max_decompressed_bytes = static_cast<std::size_t>(entry->expanded_bytes),
          .max_window_bytes = 32 * 1024});
    } catch (const compression::error& failure) {
      const auto code = failure.code == compression::error_code::unavailable
          ? error_code::unavailable : failure.code == compression::error_code::resource_limit
          ? error_code::resource_limit : error_code::integrity;
      throw error{code, "ZIP DEFLATE decoding failed", entry->name, entry->data_offset_bytes};
    }
  }
  budget_->check_cancelled();
  if (bytes.size() != entry->expanded_bytes || calculate_crc(bytes, *budget_) != entry->crc32) {
    throw error{error_code::integrity, "ZIP payload size or CRC mismatch", entry->name,
                entry->data_offset_bytes};
  }
  return bytes;
}

// Force payload validation explicitly; selective readers otherwise avoid unrelated expansion.
void zip_reader::validate_all() {
  for (const auto& entry : entries_) {
    static_cast<void>(read(entry.name));
  }
}

// Expose the same operation budget for typed caller validation.
const std::shared_ptr<budget>& zip_reader::shared_budget() const noexcept { return budget_; }

// Finish a complete ZIP snapshot before its destination is replaced.
void save_zip_file(const std::filesystem::path& destination,
                   const std::function<void(zip_writer&)>& producer, limits configured) {
  save_file(destination, producer, std::move(configured));
}

// Finish complete OPC metadata and payloads before destination replacement.
void save_opc_file(const std::filesystem::path& destination,
                   const std::function<void(opc_writer&)>& producer, limits configured) {
  save_file(destination, producer, std::move(configured));
}
} // namespace rohit::serializer::package
