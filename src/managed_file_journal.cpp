#include <rohit/managed_file_journal.hpp>

#include <rohit/file_stream.hpp>
#include <rohit/managed_journal.hpp>
#include <rohit/managed_journal_stream.hpp>

#include <exception>
#include <limits>
#include <random>
#include <string>
#include <system_error>
#include <utility>

namespace rohit::managed::detail {
namespace {
using bytes = std::vector<std::uint8_t>;
constexpr std::size_t file_header_bytes = 9 * wire_word_bytes;
constexpr std::uint64_t file_magic = 0x314c4e4a5a5253;
constexpr std::uint64_t record_version = 2;
constexpr std::uint64_t base_file_kind = 1;
constexpr std::uint64_t sidecar_file_kind = 2;

// Preserve nested causes without a forwarding constructor hijacking Windows Clang EH copies.
class nested_journal_error final : public journal_indeterminate_error,
                                   public std::nested_exception {
public:
  // Capture the active exception while keeping the public journal error type and message.
  explicit nested_journal_error(const char* message)
      : journal_indeterminate_error{message}, std::nested_exception{} {}
  // Copy the retained cause rather than recursively capturing the exception being copied.
  nested_journal_error(const nested_journal_error&) = default;
};

// Bound the physical input before allocating and reading the file into memory.
bytes read_file(const std::filesystem::path& path, const journal_options& options) {
  file_stream input{path, file_open_mode::read};
  const auto size = input.size();
  if (size > options.max_file_bytes) {
    throw std::length_error{"Managed journal file budget exceeded"};
  }
  bytes result(static_cast<std::size_t>(size));
  read_stream_exact(input, result);
  return result;
}

struct file_header {
  journal_storage_mode mode{};
  std::uint64_t kind{};
  std::uint64_t generation_high{};
  std::uint64_t generation_low{};
  std::uint64_t sequence{};
  std::uint64_t base_digest{};
};

// Encode a versioned header binding physical mode, generation, sequence, and base frame checksum.
bytes encode_header(const file_header& header) {
  bytes result;
  result.reserve(file_header_bytes);
  for (const auto word :
       {file_magic, record_version, static_cast<std::uint64_t>(header.mode), header.kind,
        header.generation_high, header.generation_low, header.sequence, header.base_digest}) {
    put_word(result, word);
  }
  put_word(result, checksum(result));
  return result;
}

// Reject damaged or unsupported headers before trusting their mode or generation.
file_header decode_header(std::span<const std::uint8_t> input, std::uint64_t kind) {
  if (input.size() < file_header_bytes) {
    throw std::invalid_argument{"Incomplete managed journal header"};
  }
  const auto header = input.first(file_header_bytes);
  if (get_word(header, 0) != file_magic || get_word(header, 1) != record_version ||
      get_word(header, 3) != kind ||
      checksum(header.first(file_header_bytes - wire_word_bytes)) != get_word(header, 8)) {
    throw std::invalid_argument{"Invalid managed journal header"};
  }
  const auto mode = static_cast<journal_storage_mode>(get_word(header, 2));
  if ((mode != journal_storage_mode::appended && mode != journal_storage_mode::sidecar) ||
      (get_word(header, 4) == 0 && get_word(header, 5) == 0)) {
    throw std::invalid_argument{"Invalid managed journal mode or generation"};
  }
  return {mode,
          kind,
          get_word(header, 4),
          get_word(header, 5),
          get_word(header, 6),
          get_word(header, 7)};
}

// Derive companion/staging names from a random generation; filenames alone never establish binding.
std::filesystem::path generation_path(const std::filesystem::path& base, const file_header& header,
                                      const char* role) {
  auto result = base;
  result += std::string{role} + std::to_string(header.generation_high) + "-" +
            std::to_string(header.generation_low);
  return result;
}

// Consume a verified record through the same buffer concept used by ordinary deserialization.
std::optional<decoded_frame> read_file_frame(std::span<const std::uint8_t> bytes,
                                             std::size_t offset, std::uint64_t sequence,
                                             std::uint64_t previous,
                                             const journal_options& options) {
  if (offset > bytes.size()) {
    throw std::invalid_argument{"Invalid managed journal offset"};
  }
  const auto input = make_constant_full_stream(bytes.data() + offset, bytes.size() - offset);
  const auto frame = read_journal_frame(input, sequence, previous, options);
  if (!frame) {
    return {};
  }
  return decoded_frame{frame->payload(), frame->digest,
                       offset + frame_overhead_bytes + frame->payload().size()};
}

// This adapter coordinates file generations only; transport and framing are reusable stream code.
class file_journal final : public journal_sink {
  struct implementation;
  std::unique_ptr<implementation> state_;
  // Acquire the document's stable lock before reading or changing its files.
  file_journal(const std::filesystem::path& path, journal_options options);

public:
  // Release streams and the writer lock without implicitly saving.
  ~file_journal() override;
  // Build a new file-backed baseline without overwriting existing data.
  static std::unique_ptr<file_journal> create(const std::filesystem::path& path,
                                              journal_storage_mode mode, const bytes& envelope,
                                              journal_options options, bool& indeterminate);
  // Validate container identity and replay its ordered records before tail repair.
  static std::unique_ptr<file_journal>
  recover(const std::filesystem::path& path, journal_options options,
          const std::function<void(bool, std::span<const std::uint8_t>)>& consume);
  // Apply validated tail repair and synchronize accepted data.
  void finish_recovery() override;
  // Commit one borrowed record through a durable file stream.
  void append(std::span<const std::uint8_t> prefix, std::span<const std::uint8_t> label,
              std::span<const std::uint8_t> snapshot) override;
  // Atomically replace the base and rotate the selected companion.
  void save(const bytes& envelope) override;
  // Return the last acknowledged operation sequence.
  std::uint64_t sequence() const noexcept override;
  // Expose fencing after an uncertain durable operation.
  bool needs_recovery() const noexcept override;
};

struct file_journal::implementation {
  std::filesystem::path path{};
  journal_options options{};
  std::unique_ptr<file_stream> lock{};
  std::unique_ptr<file_stream> writer{};
  file_header header{};
  std::filesystem::path append_path{};
  std::uint64_t sequence{};
  std::uint64_t digest{};
  std::size_t append_offset{};
  bool fenced{};
  bool recovering{};
  bool incomplete_tail{};

  // Invoke qualification hooks only at documented I/O boundaries.
  void event(journal_io_event point) const {
    if (options.fault_injector) {
      options.fault_injector(point);
    }
  }

  // Enforce fencing even if the caller catches an indeterminate error and attempts another write.
  void require_ready() const {
    if (fenced || recovering) {
      throw journal_indeterminate_error{"Managed journal must be recovered before writing"};
    }
  }

  // Validate additive file sizes before arithmetic, allocation, or any publication attempt.
  void check_size(std::size_t prefix, std::size_t payload) const {
    if (prefix > options.max_file_bytes || frame_overhead_bytes > options.max_file_bytes - prefix ||
        payload > options.max_file_bytes - prefix - frame_overhead_bytes) {
      throw std::length_error{"Managed journal file budget exceeded; perform a full Save"};
    }
  }

  // Install a fully flushed generation; until replacement, every failure leaves the old base valid.
  void replace(const bytes& envelope, journal_storage_mode mode, bool existing) {
    require_ready();
    if (mode != journal_storage_mode::appended && mode != journal_storage_mode::sidecar) {
      throw std::invalid_argument{"Unsupported managed journal storage mode"};
    }
    check_size(file_header_bytes, envelope.size());
    auto frame = encode_frame({envelope}, sequence, 0, options);
    std::random_device entropy;
    std::uniform_int_distribution<std::uint64_t> word;
    file_header replacement{mode,          base_file_kind, word(entropy),
                            word(entropy), sequence,       frame.digest};
    if (replacement.generation_high == 0 && replacement.generation_low == 0) {
      replacement.generation_low = 1;
    }
    const auto staging = generation_path(path, replacement, ".pending-");
    auto destination = mode == journal_storage_mode::appended
                           ? path
                           : generation_path(path, replacement, ".journal-");
    const auto old_sidecar =
        header.mode == journal_storage_mode::sidecar ? append_path : std::filesystem::path{};
    const auto encoded_header = encode_header(replacement);
    std::unique_ptr<file_stream> next_writer;
    if (mode == journal_storage_mode::sidecar) {
      auto companion = replacement;
      companion.kind = sidecar_file_kind;
      const auto companion_header = encode_header(companion);
      next_writer = std::make_unique<file_stream>(destination, file_open_mode::create);
      write_stream_bytes(*next_writer, companion_header);
      next_writer->sync();
      sync_parent_directory(destination);
      event(journal_io_event::after_sidecar_flush);
      file_stream output{staging, file_open_mode::create};
      write_stream_bytes(output, encoded_header);
      write_stream_bytes(output, frame.header);
      write_stream_bytes(output, envelope);
      write_stream_bytes(output, frame.footer);
      output.sync();
    } else {
      // Keep the prepared handle open across replacement; subsequent appends use this exact generation.
      next_writer = std::make_unique<file_stream>(staging, file_open_mode::create);
      write_stream_bytes(*next_writer, encoded_header);
      write_stream_bytes(*next_writer, frame.header);
      write_stream_bytes(*next_writer, envelope);
      write_stream_bytes(*next_writer, frame.footer);
      next_writer->sync();
    }
    event(journal_io_event::after_base_flush);
    event(journal_io_event::before_replace);
    // Windows replacement requires closing the old destination handle. The stable document lock
    // remains held; every failure after this point fences the store until reopening.
    if (mode == journal_storage_mode::appended) {
      writer.reset();
    }
    try {
      publish_file(staging, path, existing);
      event(journal_io_event::after_replace);
      sync_parent_directory(path);
      event(journal_io_event::after_publish);
    } catch (...) {
      fenced = true;
      throw nested_journal_error{"Managed full Save outcome is indeterminate"};
    }
    // All throwing preparation is complete before these publication assignments.
    header = replacement;
    append_path.swap(destination);
    writer.swap(next_writer);
    digest = frame.digest;
    append_offset = mode == journal_storage_mode::appended
                        ? file_header_bytes + frame_overhead_bytes + envelope.size()
                        : file_header_bytes;
    // Cleanup is optional: a failure here cannot turn a durable Save into a failed operation.
    try {
      event(journal_io_event::before_cleanup);
      if (!old_sidecar.empty()) {
        std::error_code ignored;
        std::filesystem::remove(old_sidecar, ignored);
      }
    } catch (...) {
    }
  }
};

// Resolve the path once and keep the lock file stable across all base-file replacements.
file_journal::file_journal(const std::filesystem::path& path, journal_options options)
    : state_{std::make_unique<implementation>()} {
  if (path.empty() || options.max_record_bytes == 0 ||
      options.max_file_bytes < file_header_bytes + frame_overhead_bytes) {
    throw std::invalid_argument{"Invalid managed journal path or limits"};
  }
  state_->path = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
  state_->options = std::move(options);
  auto lock_path = state_->path;
  lock_path += ".lock";
  state_->lock = std::make_unique<file_stream>(lock_path, file_open_mode::lock);
}

// Out-of-line destruction hides platform handles from public headers.
file_journal::~file_journal() = default;

// Create a new baseline under exclusive ownership without replacing an existing document.
std::unique_ptr<file_journal> file_journal::create(const std::filesystem::path& path,
                                                   journal_storage_mode mode, const bytes& envelope,
                                                   journal_options options, bool& indeterminate) {
  auto result = std::unique_ptr<file_journal>{new file_journal{path, std::move(options)}};
  if (std::filesystem::exists(result->state_->path)) {
    throw std::invalid_argument{"Managed journal document already exists"};
  }
  try {
    result->state_->replace(envelope, mode, false);
  } catch (...) {
    // Preserve uncertainty even if allocating the richer exception itself failed.
    indeterminate = result->state_->fenced;
    throw;
  }
  return result;
}

// Feed validated borrowed record slices to isolated replay; no extra payload buffer is allocated.
std::unique_ptr<file_journal>
file_journal::recover(const std::filesystem::path& path, journal_options options,
                      const std::function<void(bool, std::span<const std::uint8_t>)>& consume) {
  auto result = std::unique_ptr<file_journal>{new file_journal{path, std::move(options)}};
  auto& state = *result->state_;
  auto input = read_file(state.path, state.options);
  state.header = decode_header(input, base_file_kind);
  const auto baseline =
      read_file_frame(input, file_header_bytes, state.header.sequence, 0, state.options);
  if (!baseline || baseline->digest != state.header.base_digest) {
    throw std::invalid_argument{"Incomplete or mismatched managed journal base"};
  }
  consume(true, baseline->payload);
  state.sequence = state.header.sequence;
  state.digest = baseline->digest;
  state.append_offset = baseline->end_offset;
  state.append_path = state.path;
  if (state.header.mode == journal_storage_mode::sidecar) {
    if (state.append_offset != input.size()) {
      throw std::invalid_argument{"Trailing bytes in managed sidecar base"};
    }
    state.append_path = generation_path(state.path, state.header, ".journal-");
    input = read_file(state.append_path, state.options);
    auto companion = decode_header(input, sidecar_file_kind);
    companion.kind = base_file_kind;
    if (encode_header(companion) != encode_header(state.header)) {
      throw std::invalid_argument{"Managed sidecar does not match its base generation"};
    }
    state.append_offset = file_header_bytes;
  }
  while (state.append_offset < input.size()) {
    if (state.sequence == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error{"Managed journal sequence exhausted"};
    }
    auto frame = read_file_frame(input, state.append_offset, state.sequence + 1, state.digest,
                                 state.options);
    if (!frame) {
      state.incomplete_tail = true;
      break;
    }
    ++state.sequence;
    state.digest = frame->digest;
    state.append_offset = frame->end_offset;
    consume(false, frame->payload);
  }
  state.recovering = true;
  return result;
}

// Flush recovered complete records too: a prior failed flush may have left them only in the OS cache.
void file_journal::finish_recovery() {
  auto& state = *state_;
  if (!state.recovering || state.fenced) {
    throw std::logic_error{"Managed journal is not awaiting recovery validation"};
  }
  try {
    auto writer = std::make_unique<file_stream>(state.append_path, file_open_mode::update);
    if (state.incomplete_tail) {
      state.event(journal_io_event::before_tail_repair);
      writer->truncate(state.append_offset);
      state.event(journal_io_event::after_tail_repair);
    } else {
      writer->seek(state.append_offset);
    }
    writer->sync();
    // A previous Save may have failed after rename but before directory flush.
    sync_parent_directory(state.path);
    state.writer.swap(writer);
    state.recovering = false;
  } catch (...) {
    state.fenced = true;
    throw nested_journal_error{"Managed tail repair must be recovered"};
  }
}

// Write prefix/optional label/already encoded snapshot without combining or copying their buffers.
void file_journal::append(std::span<const std::uint8_t> prefix, std::span<const std::uint8_t> label,
                          std::span<const std::uint8_t> snapshot) {
  auto& state = *state_;
  state.require_ready();
  if (state.sequence == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"Managed journal sequence exhausted"};
  }
  const auto frame =
      encode_frame({prefix, label, snapshot}, state.sequence + 1, state.digest, state.options);
  state.check_size(state.append_offset, frame.payload_bytes);
  // The exclusive writer retains its positioned handle; edits do not reopen or seek the file.
  auto& output = *state.writer;
  state.event(journal_io_event::before_append);
  try {
    write_frame(output, frame, {prefix, label, snapshot}, state.options);
    output.sync();
    state.event(journal_io_event::after_flush);
  } catch (...) {
    state.fenced = true;
    throw nested_journal_error{"Managed journal commit outcome is indeterminate"};
  }
  ++state.sequence;
  state.digest = frame.digest;
  state.append_offset += frame_overhead_bytes + frame.payload_bytes;
}

// A synchronous full Save covers the current durable sequence and retains the complete envelope.
void file_journal::save(const bytes& envelope) {
  state_->replace(envelope, state_->header.mode, true);
}

// Return the monotonically increasing durable operation position, independently of history cursors.
std::uint64_t file_journal::sequence() const noexcept {
  return state_->sequence;
}

// A fenced adapter cannot resume writes through an in-memory retry.
bool file_journal::needs_recovery() const noexcept {
  return state_->fenced;
}

} // namespace

// Keep the concrete file backend out of the managed store's owned sink type.
std::unique_ptr<journal_sink> create_file_journal(const std::filesystem::path& path,
                                                  journal_storage_mode mode,
                                                  const std::vector<std::uint8_t>& envelope,
                                                  journal_options options, bool& indeterminate) {
  return file_journal::create(path, mode, envelope, std::move(options), indeterminate);
}

// Return the same sink interface after file-specific generation selection and stream replay.
std::unique_ptr<journal_sink>
recover_file_journal(const std::filesystem::path& path, journal_options options,
                     const std::function<void(bool, std::span<const std::uint8_t>)>& consume) {
  return file_journal::recover(path, std::move(options), consume);
}

} // namespace rohit::managed::detail
