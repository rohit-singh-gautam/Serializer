// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/paged_index_file.hpp>

#include <rohit/digest.hpp>
#include <rohit/file_stream.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <memory_resource>
#include <new>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace rohit::managed::detail {
namespace {
constexpr std::uint64_t manifest_magic = 0x31464e4d495053;
constexpr std::uint64_t page_magic = 0x3145474150495053;
constexpr std::uint64_t leaf_kind = 1;
constexpr std::uint64_t branch_kind = 2;
constexpr std::size_t word_bytes = 8;
constexpr std::size_t entry_bytes = 2 * word_bytes;
constexpr std::size_t page_header_bytes = 6 * word_bytes;
constexpr std::size_t manifest_header_bytes = 3 * word_bytes;
constexpr std::size_t checksum_bytes = digest_size(digest_algorithm::sha256);
constexpr std::size_t run_frame_overhead_bytes = word_bytes + checksum_bytes;
constexpr std::size_t minimum_page_bytes = 256;
constexpr std::size_t maximum_page_bytes = 64 * 1024;

// Track actual standard-container/control-block allocation requests before forwarding to new/delete.
class page_cache_resource final : public std::pmr::memory_resource {
  std::size_t maximum_;
  std::size_t current_{};
  std::size_t peak_{};
  // Reserve the requested allocation before touching the upstream allocator.
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    charge(bytes);
    try { return std::pmr::new_delete_resource()->allocate(bytes, alignment); }
    catch (...) { release(bytes); throw; }
  }
  // Return exactly the charge reported by the allocator on the matching deallocation.
  void do_deallocate(void* value, std::size_t bytes, std::size_t alignment) override {
    std::pmr::new_delete_resource()->deallocate(value, bytes, alignment);
    release(bytes);
  }
  // Distinct source snapshots never share an allocator admission domain implicitly.
  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }
public:
  // Create one immutable generation's controlled allocation budget.
  explicit page_cache_resource(std::size_t maximum) : maximum_{maximum} {}
  // Charge externally owned permanent manifest capacity as well as allocator-routed storage.
  void charge(std::size_t bytes) {
    if (bytes > maximum_ - current_) {
      throw std::length_error{"Paged index cache byte budget exceeded"};
    }
    current_ += bytes;
    peak_ = std::max(peak_, current_);
  }
  // Release a previously established charge; allocator clients guarantee matching extents.
  void release(std::size_t bytes) noexcept { current_ -= bytes; }
  // Inspect all currently charged page, map, cursor, metadata and control-block allocations.
  std::size_t current() const noexcept { return current_; }
  // Inspect the actual peak of controlled allocation requests independently of OS residency.
  std::size_t peak() const noexcept { return peak_; }
};

// Keep the budget alive through shared control-block deallocation, including final cursor release.
template <typename T>
struct page_cache_allocator {
  using value_type = T;
  std::shared_ptr<page_cache_resource> resource;
  // Select an existing generation admission domain.
  explicit page_cache_allocator(std::shared_ptr<page_cache_resource> selected)
      : resource{std::move(selected)} {}
  // Rebind standard shared-control-block allocations to the same owning resource.
  template <typename U>
  page_cache_allocator(const page_cache_allocator<U>& other) : resource{other.resource} {}
  // Check multiplication before forwarding the exact allocation request.
  T* allocate(std::size_t count) {
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) { throw std::bad_array_new_length{}; }
    return static_cast<T*>(resource->allocate(count * sizeof(T), alignof(T)));
  }
  // Match the standard allocator's exact count and alignment on release.
  void deallocate(T* value, std::size_t count) noexcept {
    resource->deallocate(value, count * sizeof(T), alignof(T));
  }
  // Container allocator identity follows its owning cache generation.
  template <typename U>
  bool operator==(const page_cache_allocator<U>& other) const noexcept { return resource == other.resource; }
};

// Compare full unsigned keys and identities numerically, without native/varint byte ordering.
bool entry_less(const paged_index_entry& left, const paged_index_entry& right) {
  return left.key < right.key || (left.key == right.key && left.identity < right.identity);
}

// Encode an explicit little-endian wire integer into a previously checked fixed buffer.
void put_word(std::span<std::uint8_t> bytes, std::size_t offset, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8) {
    bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
  }
}

// Decode one bounded wire integer without relying on host endianness or object layout.
std::uint64_t get_word(std::span<const std::uint8_t> bytes, std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < word_bytes) {
    throw std::invalid_argument{"Truncated paged index wire integer"};
  }
  std::uint64_t value{};
  for (unsigned shift = 0; shift < 64; shift += 8) {
    value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
  }
  return value;
}

// Bound every file-stream conversion before narrowing to the native stream interface.
std::streamsize stream_extent(std::size_t bytes) {
  if (bytes > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
    throw std::length_error{"Paged index stream extent exceeded"};
  }
  return static_cast<std::streamsize>(bytes);
}

// Require exact input bytes under an immutable file handle; EOF never becomes a false index miss.
void read_exact(file_stream& stream, std::span<std::uint8_t> bytes) {
  stream.read(reinterpret_cast<char*>(bytes.data()), stream_extent(bytes.size()));
  if (stream.gcount() != stream_extent(bytes.size())) {
    throw std::invalid_argument{"Truncated paged index file"};
  }
}

// Write a bounded complete buffer; callers own publication and durability decisions.
void write_exact(file_stream& stream, std::span<const std::uint8_t> bytes) {
  stream.write(reinterpret_cast<const char*>(bytes.data()), stream_extent(bytes.size()));
}

// Reject invalid budgets before opening files or reserving construction/cache memory.
void validate_options(const paged_index_options& options) {
  if (options.page_bytes < minimum_page_bytes || options.page_bytes > maximum_page_bytes ||
      (options.page_bytes & (options.page_bytes - 1)) != 0 || options.max_cache_pages == 0 ||
      options.max_cache_bytes < options.page_bytes || options.max_run_entries == 0 ||
      options.max_run_entries > std::numeric_limits<std::size_t>::max() / sizeof(paged_index_entry) ||
      options.merge_fan_in < 2 || options.merge_fan_in > options.max_run_entries ||
      options.max_depth == 0 || options.max_depth > 64 || options.max_entries == 0 ||
      options.max_file_bytes < 2 * options.page_bytes || options.max_temporary_bytes == 0) {
    throw std::invalid_argument{"Invalid paged index options"};
  }
}

// Check cooperative cancellation at bounded extraction/merge/page/query work boundaries.
void check_cancelled(const paged_index_options& options) {
  if (options.is_cancelled && options.is_cancelled()) {
    throw std::runtime_error{"Paged index operation cancelled"};
  }
}

// Execute deterministic qualification hooks independently of authoritative model acceptance.
void fault(const paged_index_options& options, paged_index_io_event event) {
  if (options.fault_injector) { options.fault_injector(event); }
}

// Produce a nonzero generation identity and unique task-owned staging name.
std::pair<std::uint64_t, std::uint64_t> new_generation() {
  std::random_device random;
  std::uniform_int_distribution<std::uint64_t> word;
  std::pair<std::uint64_t, std::uint64_t> result;
  do { result = {word(random), word(random)}; } while (result.first == 0 && result.second == 0);
  return result;
}

// Own only an exclusively created sibling directory; errors never remove a preexisting path.
class staging_directory {
  std::filesystem::path path_{};
public:
  // Reserve a fresh path under the exact destination parent before creating any scratch files.
  staging_directory(const std::filesystem::path& target,
                    std::pair<std::uint64_t, std::uint64_t> generation) {
    auto name = target.filename();
    name += ".pages-" + std::to_string(generation.first) + "-" +
            std::to_string(generation.second);
    path_ = target.parent_path() / name;
    if (!std::filesystem::create_directory(path_)) {
      throw std::runtime_error{"Paged index staging directory already exists"};
    }
  }
  // Remove this task's verified exclusively created directory after its streams have closed.
  ~staging_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  // Resolve a generated filename without consulting source filenames or untrusted metadata.
  std::filesystem::path file(std::string name) const { return path_ / std::move(name); }
};

// Account live staging bytes, including buffered pending output, independently of final-file limits.
struct temporary_budget {
  std::uint64_t current{};
  std::uint64_t maximum{};
  paged_index_build_statistics& statistics;
  // Reserve every pending byte before appending to an output buffer or file.
  void add(std::uint64_t bytes) {
    if (bytes > maximum - current) {
      throw std::length_error{"Paged index temporary disk budget exceeded"};
    }
    current += bytes;
    statistics.peak_temporary_bytes = std::max(statistics.peak_temporary_bytes, current);
  }
  // Delete only a completed task-owned run/catalog, then release its exact physical byte charge.
  void remove(const std::filesystem::path& path) {
    const auto bytes = std::filesystem::file_size(path);
    if (bytes > current || !std::filesystem::remove(path)) {
      throw std::runtime_error{"Cannot remove paged index temporary run"};
    }
    current -= bytes;
  }
};

// Buffered portable run records avoid one system call per numeric key during external sorting.
class run_writer {
  file_stream stream_;
  std::vector<std::uint8_t> buffer_;
  std::size_t used_{};
  temporary_budget& budget_;
public:
  // Open an exclusively created run with a fixed page-sized write buffer.
  run_writer(const std::filesystem::path& path, std::size_t buffer_bytes, temporary_budget& budget)
      : stream_{path, file_open_mode::create}, buffer_(buffer_bytes), budget_{budget} {}
  // Append one charged wire pair; no input pointers are retained.
  void append(paged_index_entry entry) {
    budget_.add(entry_bytes);
    put_word(buffer_, used_, entry.key);
    put_word(buffer_, used_ + word_bytes, entry.identity);
    used_ += entry_bytes;
    if (used_ == buffer_.size()) { flush(); }
  }
  // Write the final buffered prefix before another stage opens this run.
  void flush() {
    if (used_ != 0) {
      budget_.add(run_frame_overhead_bytes);
      std::array<std::uint8_t, word_bytes> header{};
      put_word(header, 0, used_ / entry_bytes);
      const auto payload = std::span<const std::uint8_t>{buffer_}.first(used_);
      const auto checksum = make_digest<digest_algorithm::sha256>(payload);
      write_exact(stream_, header);
      write_exact(stream_, payload);
      write_exact(stream_, checksum);
      used_ = 0;
    }
  }
};

// Retain one fixed read buffer per bounded merge input; file lengths never allocate key tables.
class run_reader {
  file_stream stream_;
  std::vector<std::uint8_t> buffer_;
  std::uint64_t remaining_{};
  std::size_t offset_{};
  std::size_t available_{};
public:
  // Keep one fixed input buffer; each emitted block validates its own extent and checksum.
  run_reader(const std::filesystem::path& path, std::size_t buffer_bytes)
      : stream_{path, file_open_mode::read}, buffer_(buffer_bytes), remaining_{stream_.size()} {}
  // Read one unsigned pair, returning false only at the exact checked run boundary.
  bool next(paged_index_entry& entry) {
    if (offset_ == available_) {
      if (remaining_ == 0) { return false; }
      if (remaining_ < run_frame_overhead_bytes + entry_bytes) {
        throw std::invalid_argument{"Truncated paged index temporary run block"};
      }
      std::array<std::uint8_t, word_bytes> header{};
      read_exact(stream_, header);
      const auto count = get_word(header, 0);
      if (count == 0 || count > buffer_.size() / entry_bytes ||
          count > (remaining_ - run_frame_overhead_bytes) / entry_bytes) {
        throw std::invalid_argument{"Invalid paged index temporary run block extent"};
      }
      available_ = static_cast<std::size_t>(count) * entry_bytes;
      const auto payload = std::span<std::uint8_t>{buffer_}.first(available_);
      read_exact(stream_, payload);
      std::array<std::uint8_t, checksum_bytes> checksum{};
      read_exact(stream_, checksum);
      if (checksum != make_digest<digest_algorithm::sha256>(payload)) {
        throw std::invalid_argument{"Paged index temporary run checksum mismatch"};
      }
      remaining_ -= run_frame_overhead_bytes + available_;
      offset_ = 0;
    }
    entry = {get_word(buffer_, offset_), get_word(buffer_, offset_ + word_bytes)};
    offset_ += entry_bytes;
    return true;
  }
};

// Derive every temporary run name from bounded numeric pass/ordinal counters, without a RAM catalog.
std::filesystem::path run_path(const staging_directory& staging, std::uint64_t pass,
                               std::uint64_t ordinal) {
  return staging.file("run-" + std::to_string(pass) + "-" + std::to_string(ordinal));
}

// Merge at most fan-in sorted streams; reject duplicate key/identity pairs across every run boundary.
void merge_runs(const staging_directory& staging, std::uint64_t pass, std::uint64_t first,
                std::size_t count, std::uint64_t output, temporary_budget& budget,
                const paged_index_options& options, paged_index_build_statistics& statistics) {
  struct head { paged_index_entry entry; std::size_t input; };
  const auto later = [](const head& left, const head& right) {
    return entry_less(right.entry, left.entry);
  };
  std::vector<std::unique_ptr<run_reader>> readers;
  readers.reserve(count);
  std::vector<head> heap_storage;
  heap_storage.reserve(count);
  std::priority_queue<head, std::vector<head>, decltype(later)> heap{later, std::move(heap_storage)};
  for (std::size_t index = 0; index < count; ++index) {
    readers.push_back(std::make_unique<run_reader>(run_path(staging, pass, first + index),
                                                  options.page_bytes));
    paged_index_entry entry;
    if (readers.back()->next(entry)) { heap.push({entry, index}); }
  }
  statistics.peak_merge_inputs = std::max(statistics.peak_merge_inputs, count);
  {
    run_writer writer{run_path(staging, pass + 1, output), options.page_bytes, budget};
    std::optional<paged_index_entry> previous;
    while (!heap.empty()) {
      check_cancelled(options);
      const auto selected = heap.top();
      heap.pop();
      if (selected.entry.identity == 0 || (previous && !entry_less(*previous, selected.entry))) {
        throw std::invalid_argument{"Duplicate or unordered paged index key/identity"};
      }
      writer.append(selected.entry);
      previous = selected.entry;
      paged_index_entry next;
      if (readers[selected.input]->next(next)) { heap.push({next, selected.input}); }
    }
    writer.flush();
  }
  readers.clear();
  for (std::size_t index = 0; index < count; ++index) {
    budget.remove(run_path(staging, pass, first + index));
  }
}

// Reserve a bounded page and emit its checksum after all structural fields and pairs are prepared.
std::vector<std::uint8_t> make_page(std::size_t page_bytes, std::uint64_t id, std::uint64_t kind,
                                  std::uint64_t next, std::span<const paged_index_entry> entries) {
  std::vector<std::uint8_t> bytes(page_bytes);
  put_word(bytes, 0, page_magic);
  put_word(bytes, word_bytes, id);
  put_word(bytes, 2 * word_bytes, kind);
  put_word(bytes, 3 * word_bytes, entries.size());
  put_word(bytes, 4 * word_bytes, next);
  put_word(bytes, 5 * word_bytes, entries.empty() ? 0 : entries.back().key);
  std::size_t offset = page_header_bytes;
  for (const auto entry : entries) {
    put_word(bytes, offset, entry.key);
    put_word(bytes, offset + word_bytes, entry.identity);
    offset += entry_bytes;
  }
  const auto digest = make_digest<digest_algorithm::sha256>(
      std::span<const std::uint8_t>{bytes}.first(page_bytes - checksum_bytes));
  std::copy(digest.begin(), digest.end(), bytes.end() - static_cast<std::ptrdiff_t>(checksum_bytes));
  return bytes;
}

// Validate complete page integrity before interpreting structural offsets or entry counts.
void verify_checksum(std::span<const std::uint8_t> bytes) {
  const auto digest = make_digest<digest_algorithm::sha256>(bytes.first(bytes.size() - checksum_bytes));
  if (!std::equal(digest.begin(), digest.end(), bytes.end() -
                  static_cast<std::ptrdiff_t>(checksum_bytes))) {
    throw std::invalid_argument{"Paged index page checksum mismatch"};
  }
}
} // namespace

// Build external sorted runs, merge with bounded fan-in, and stream each directory level to disk.
paged_index_build_statistics build_paged_index_from(const std::filesystem::path& path,
    std::span<const std::uint8_t> binding, const paged_index_producer& source,
    const paged_index_encode_metadata& encode, const paged_index_options& options) {
  validate_options(options);
  if (!source || !encode || binding.empty() || binding.size() > options.page_bytes) {
    throw std::invalid_argument{"Invalid paged index build source or binding"};
  }
  auto lock_path = path;
  lock_path += ".writer.lock";
  file_stream writer_lock{lock_path, file_open_mode::lock};
  const auto generation = new_generation();
  const staging_directory staging{path, generation};
  paged_index_build_statistics statistics;
  temporary_budget budget{0, options.max_temporary_bytes, statistics};
  std::uint64_t run_count{};
  std::vector<paged_index_entry> buffer;
  buffer.reserve(options.max_run_entries);
  const auto flush_run = [&] {
    statistics.peak_sort_entries = std::max(statistics.peak_sort_entries, buffer.size());
    std::sort(buffer.begin(), buffer.end(), entry_less);
    if (std::adjacent_find(buffer.begin(), buffer.end()) != buffer.end()) {
      throw std::invalid_argument{"Duplicate paged index key/identity"};
    }
    {
      run_writer writer{run_path(staging, 0, run_count), options.page_bytes, budget};
      for (const auto entry : buffer) { writer.append(entry); }
      writer.flush();
    }
    fault(options, paged_index_io_event::after_run_write);
    ++run_count;
    buffer.clear();
  };
  source([&](paged_index_entry entry) {
    check_cancelled(options);
    if (entry.identity == 0 || statistics.entries == options.max_entries) {
      throw std::length_error{"Paged index identity or input entry budget exceeded"};
    }
    buffer.push_back(entry);
    ++statistics.entries;
    if (buffer.size() == options.max_run_entries) { flush_run(); }
  });
  if (!buffer.empty() || run_count == 0) { flush_run(); }
  statistics.initial_runs = run_count;
  // Release the extraction/sort buffer before allocating bounded merge-input buffers.
  std::vector<paged_index_entry>{}.swap(buffer);
  std::uint64_t pass{};
  while (run_count > 1) {
    std::uint64_t produced{};
    for (std::uint64_t first = 0; first < run_count;) {
      const auto count = static_cast<std::size_t>(
          std::min<std::uint64_t>(run_count - first, options.merge_fan_in));
      merge_runs(staging, pass, first, count, produced++, budget, options, statistics);
      first += count;
    }
    run_count = produced;
    ++pass;
    ++statistics.merge_passes;
  }
  const auto output_path = staging.file("index");
  {
    file_stream output{output_path, file_open_mode::create};
    std::vector<std::uint8_t> empty(options.page_bytes);
    budget.add(empty.size());
    write_exact(output, empty);
    const auto capacity = (options.page_bytes - page_header_bytes - checksum_bytes) / entry_bytes;
    std::vector<paged_index_entry> entries;
    entries.reserve(capacity);
    std::uint64_t page_count{};
    // All page offsets derive from checked page count and configured fixed page size.
    const auto write_page = [&](std::uint64_t kind, std::uint64_t next) {
      check_cancelled(options);
      if (page_count >= options.max_file_bytes / options.page_bytes - 1) {
        throw std::length_error{"Paged index output file budget exceeded"};
      }
      const auto bytes = make_page(options.page_bytes, ++page_count, kind, next, entries);
      budget.add(bytes.size());
      fault(options, paged_index_io_event::before_page_write);
      write_exact(output, bytes);
      return paged_index_entry{entries.empty() ? 0 : entries.back().key, page_count};
    };
    std::uint64_t nodes{};
    auto catalog = staging.file("catalog-0");
    {
      run_reader sorted{run_path(staging, pass, 0), options.page_bytes};
      run_writer directory{catalog, options.page_bytes, budget};
      std::uint64_t remaining = statistics.entries;
      do {
        entries.clear();
        const auto selected = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, capacity));
        for (std::size_t index = 0; index < selected; ++index) {
          paged_index_entry entry;
          if (!sorted.next(entry)) { throw std::invalid_argument{"Truncated sorted index run"}; }
          entries.push_back(entry);
        }
        remaining -= selected;
        directory.append(write_page(leaf_kind, remaining == 0 ? 0 : page_count + 2));
        ++nodes;
      } while (remaining != 0);
      paged_index_entry trailing;
      if (sorted.next(trailing)) {
        throw std::invalid_argument{"Unexpected entries after sorted index run boundary"};
      }
      directory.flush();
    }
    budget.remove(run_path(staging, pass, 0));
    std::uint64_t depth = 1;
    unsigned catalog_slot{};
    while (nodes > 1) {
      if (depth == options.max_depth) { throw std::length_error{"Paged index tree depth exceeded"}; }
      const auto next_catalog = staging.file("catalog-" + std::to_string(1U - catalog_slot));
      std::uint64_t produced{};
      {
        run_reader children{catalog, options.page_bytes};
        run_writer directory{next_catalog, options.page_bytes, budget};
        std::uint64_t remaining = nodes;
        while (remaining != 0) {
          entries.clear();
          const auto selected = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, capacity));
          for (std::size_t index = 0; index < selected; ++index) {
            paged_index_entry entry;
            if (!children.next(entry)) { throw std::invalid_argument{"Truncated index directory run"}; }
            entries.push_back(entry);
          }
          directory.append(write_page(branch_kind, 0));
          ++produced;
          remaining -= selected;
        }
        paged_index_entry trailing;
        if (children.next(trailing)) {
          throw std::invalid_argument{"Unexpected entries after index directory boundary"};
        }
        directory.flush();
      }
      budget.remove(catalog);
      catalog = next_catalog;
      catalog_slot = 1U - catalog_slot;
      nodes = produced;
      ++depth;
    }
    paged_index_entry root;
    {
      run_reader directory{catalog, options.page_bytes};
      if (!directory.next(root)) { throw std::invalid_argument{"Paged index has no root descriptor"}; }
      paged_index_entry trailing;
      if (directory.next(trailing)) {
        throw std::invalid_argument{"Multiple paged index root descriptors"};
      }
    }
    budget.remove(catalog);
    const paged_index_metadata metadata{options.page_bytes, page_count, root.identity, depth,
        statistics.entries, generation.first, generation.second,
        std::vector<std::uint8_t>(binding.begin(), binding.end())};
    const auto manifest = encode(metadata);
    if (manifest.size() > options.page_bytes - manifest_header_bytes - checksum_bytes) {
      throw std::length_error{"Generated paged index manifest exceeds header page"};
    }
    std::fill(empty.begin(), empty.end(), 0);
    put_word(empty, 0, manifest_magic);
    put_word(empty, word_bytes, options.page_bytes);
    put_word(empty, 2 * word_bytes, manifest.size());
    std::copy(manifest.begin(), manifest.end(), empty.begin() + manifest_header_bytes);
    const auto digest = make_digest<digest_algorithm::sha256>(
        std::span<const std::uint8_t>{empty}.first(empty.size() - checksum_bytes));
    std::copy(digest.begin(), digest.end(), empty.end() - static_cast<std::ptrdiff_t>(checksum_bytes));
    output.seek(0);
    write_exact(output, empty);
    output.sync();
    statistics.pages = page_count;
    fault(options, paged_index_io_event::after_flush);
  }
  check_cancelled(options);
  fault(options, paged_index_io_event::before_publish);
  replace_file_generation(output_path, path);
  sync_parent_directory(path);
  sync_parent_directory(output_path);
  fault(options, paged_index_io_event::after_publish);
  return statistics;
}

// Adapt a pull stream without collecting another owning set of source keys.
paged_index_build_statistics build_paged_index(const std::filesystem::path& path,
    std::span<const std::uint8_t> binding, const paged_index_source& source,
    const paged_index_encode_metadata& encode, const paged_index_options& options) {
  if (!source) { throw std::invalid_argument{"Missing paged index source"}; }
  return build_paged_index_from(path, binding, [&](const paged_index_emit& emit) {
    paged_index_entry entry;
    while (source(entry)) { emit(entry); }
  }, encode, options);
}

struct paged_index_file::implementation {
  struct page {
    std::pmr::vector<std::uint8_t> bytes;
    std::uint64_t id{};
    std::uint64_t kind{};
    std::uint64_t count{};
    std::uint64_t next{};
    std::uint64_t maximum{};
    // Route the page buffer and its exact capacity through this generation's admission resource.
    explicit page(std::pmr::memory_resource* resource) : bytes{resource} {}
    // Return a checked page-local pair without retaining a pointer into an evictable buffer.
    paged_index_entry entry(std::size_t index) const {
      const auto offset = page_header_bytes + index * entry_bytes;
      return {get_word(bytes, offset), get_word(bytes, offset + word_bytes)};
    }
  };
  struct cache_entry { std::shared_ptr<const page> value; std::uint64_t touched{}; };
  std::shared_ptr<page_cache_resource> resource;
  file_stream file;
  paged_index_options options;
  paged_index_metadata metadata{};
  std::pmr::map<std::uint64_t, cache_entry> cache;
  paged_index_statistics statistics{};
  std::uint64_t clock{};
  std::thread::id owner{std::this_thread::get_id()};
  std::size_t metadata_charge{};
  std::size_t cursors{};
  bool active{};

  // Reject recursive query/cursor operations before invoking any cancellation callback.
  struct operation_guard {
    implementation& state;
    // Acquire one owner-thread cache operation without calling user code during acquisition.
    explicit operation_guard(implementation& selected) : state{selected} {
      state.check_thread();
      if (state.active) { throw std::logic_error{"Reentrant paged index query"}; }
      state.active = true;
    }
    // Restore operation admission on both ordinary completion and resource/I/O failure.
    ~operation_guard() { state.active = false; }
  };

  // Read the fixed prelude before allocating a bounded manifest page and invoking its record decoder.
  implementation(const std::filesystem::path& path, std::span<const std::uint8_t> binding,
                 const paged_index_decode_metadata& decode, const paged_index_options& configuration,
                 std::shared_ptr<page_cache_resource> allocation)
      : resource{std::move(allocation)}, file{path, file_open_mode::read}, options{configuration},
        cache{resource.get()} {
    validate_options(options);
    if (!decode) { throw std::invalid_argument{"Missing paged index manifest decoder"}; }
    const auto size = file.size();
    if (size > options.max_file_bytes || size < minimum_page_bytes) {
      throw std::length_error{"Paged index file extent exceeds configured bounds"};
    }
    std::array<std::uint8_t, manifest_header_bytes> prefix{};
    read_exact(file, prefix);
    const auto page_bytes = get_word(prefix, word_bytes);
    if (get_word(prefix, 0) != manifest_magic || page_bytes != options.page_bytes) {
      throw std::invalid_argument{"Incompatible paged index header or page size"};
    }
    std::vector<std::uint8_t> header(options.page_bytes);
    file.seek(0);
    read_exact(file, header);
    verify_checksum(header);
    const auto manifest_bytes = get_word(header, 2 * word_bytes);
    if (manifest_bytes == 0 || manifest_bytes > options.page_bytes - manifest_header_bytes - checksum_bytes) {
      throw std::invalid_argument{"Invalid paged index manifest extent"};
    }
    metadata = decode(std::span<const std::uint8_t>{header}.subspan(manifest_header_bytes,
                         static_cast<std::size_t>(manifest_bytes)));
    if (metadata.page_bytes != page_bytes || metadata.page_count == 0 ||
        metadata.page_count >= options.max_file_bytes / page_bytes ||
        (metadata.page_count + 1) * page_bytes != size || metadata.root_page == 0 ||
        metadata.root_page > metadata.page_count || metadata.depth == 0 ||
        metadata.depth > options.max_depth || metadata.entries > options.max_entries ||
        (metadata.generation_high == 0 && metadata.generation_low == 0) ||
        metadata.binding.size() != binding.size() ||
        !std::equal(metadata.binding.begin(), metadata.binding.end(), binding.begin())) {
      throw std::invalid_argument{"Paged index source binding or structure mismatch"};
    }
    resource->charge(metadata.binding.capacity());
    metadata_charge = metadata.binding.capacity();
  }

  // Release manually accounted generated-manifest capacity before its ordinary owning vector dies.
  ~implementation() { resource->release(metadata_charge); }

  // Confine file cursor and mutable cache bookkeeping to their creating thread.
  void check_thread() const {
    if (std::this_thread::get_id() != owner) {
      throw std::logic_error{"Paged index accessed from another thread"};
    }
  }

  // Evict one unpinned cache node; retained cursors keep every page/control-block charge alive.
  bool evict() {
    auto victim = cache.end();
    for (auto item = cache.begin(); item != cache.end(); ++item) {
      if (item->second.value.use_count() == 1 &&
          (victim == cache.end() || item->second.touched < victim->second.touched)) { victim = item; }
    }
    if (victim == cache.end()) { return false; }
    cache.erase(victim);
    return true;
  }

  // Reserve one cache slot before allocating a page; pinned pages are never silently evicted.
  std::shared_ptr<const page> load(std::uint64_t id) {
    check_thread();
    check_cancelled(options);
    if (id == 0 || id > metadata.page_count) {
      throw std::invalid_argument{"Paged index child page is outside the bound file"};
    }
    if (clock == std::numeric_limits<std::uint64_t>::max()) {
      for (auto& [key, value] : cache) { static_cast<void>(key); value.touched = 0; }
      clock = 0;
    }
    if (const auto found = cache.find(id); found != cache.end()) {
      found->second.touched = ++clock;
      ++statistics.cache_hits;
      return found->second.value;
    }
    if (cache.size() == options.max_cache_pages && !evict()) {
      throw std::length_error{"Paged index cache is exhausted by pinned pages"};
    }
    std::shared_ptr<page> value;
    for (;;) {
      try {
        value = std::allocate_shared<page>(page_cache_allocator<page>{resource}, resource.get());
        value->bytes.resize(options.page_bytes);
        // Reserve the map node before performing I/O so metadata admission cannot overcommit.
        cache.emplace(id, cache_entry{value, ++clock});
        statistics.peak_resident_pages = std::max(statistics.peak_resident_pages, cache.size());
        break;
      } catch (const std::length_error&) {
        value.reset();
        if (!evict()) { throw; }
      }
    }
    struct incomplete_page {
      implementation& state;
      std::uint64_t id;
      bool complete{};
      // A failed read/check must never leave an apparently ready cache entry behind.
      ~incomplete_page() { if (!complete) { state.cache.erase(id); } }
    } admission{*this, id};
    file.seek(id * metadata.page_bytes);
    read_exact(file, value->bytes);
    ++statistics.page_reads;
    verify_checksum(value->bytes);
    value->id = get_word(value->bytes, word_bytes);
    value->kind = get_word(value->bytes, 2 * word_bytes);
    value->count = get_word(value->bytes, 3 * word_bytes);
    value->next = get_word(value->bytes, 4 * word_bytes);
    value->maximum = get_word(value->bytes, 5 * word_bytes);
    const auto max_count = (options.page_bytes - page_header_bytes - checksum_bytes) / entry_bytes;
    if (get_word(value->bytes, 0) != page_magic || value->id != id || value->count > max_count ||
        (value->kind != leaf_kind && value->kind != branch_kind) ||
        (value->kind == branch_kind && (value->count == 0 || value->next != 0)) ||
        (value->next != 0 && (value->next <= id || value->next > metadata.page_count)) ||
        (value->count == 0 && (value->kind != leaf_kind || metadata.entries != 0 ||
                              value->next != 0 || value->maximum != 0))) {
      throw std::invalid_argument{"Invalid paged index page structure"};
    }
    std::optional<paged_index_entry> previous;
    for (std::size_t index = 0; index < value->count; ++index) {
      const auto entry = value->entry(index);
      if (entry.identity == 0 || (value->kind == branch_kind && entry.identity >= id) ||
          (previous && (value->kind == leaf_kind ? !entry_less(*previous, entry) :
              previous->key > entry.key || previous->identity >= entry.identity))) {
        throw std::invalid_argument{"Unordered or invalid paged index page entries"};
      }
      previous = entry;
    }
    if (previous && previous->key != value->maximum) {
      throw std::invalid_argument{"Paged index directory maximum mismatch"};
    }
    admission.complete = true;
    statistics.peak_resident_pages = std::max(statistics.peak_resident_pages, cache.size());
    return value;
  }
};

struct paged_index_file::cursor::implementation {
  std::shared_ptr<paged_index_file::implementation> owner;
  std::shared_ptr<const paged_index_file::implementation::page> page{};
  std::uint64_t page_id{};
  std::size_t offset{};
  std::uint64_t upper{};
  std::optional<paged_index_entry> previous{};
  std::uint64_t emitted{};
  // Keep cursor metadata charged with its source generation, independently of page pins.
  explicit implementation(std::shared_ptr<paged_index_file::implementation> selected)
      : owner{std::move(selected)} { ++owner->cursors; }
  // Release the visible cursor count before the owning source/control block is destroyed.
  ~implementation() { --owner->cursors; }
};

// Retain an already checked immutable source/index generation and its initial page pin.
paged_index_file::cursor::cursor(std::shared_ptr<implementation> state) : state_{std::move(state)} {}
// Release cursor-owned pins without mutating the immutable index file.
paged_index_file::cursor::~cursor() = default;
// Transfer the exact traversal position and its existing page pin.
paged_index_file::cursor::cursor(cursor&&) noexcept = default;
// Replace the old traversal with the selected owning cursor state.
paged_index_file::cursor& paged_index_file::cursor::operator=(cursor&&) noexcept = default;

// Stream leaf pairs while retaining at most one cursor page; advance only after copied results.
std::optional<paged_index_entry> paged_index_file::cursor::next() {
  if (!state_) { throw std::logic_error{"Moved paged index cursor"}; }
  paged_index_file::implementation::operation_guard guard{*state_->owner};
  check_cancelled(state_->owner->options);
  while (state_->page_id != 0) {
    if (!state_->page) { state_->page = state_->owner->load(state_->page_id); }
    if (state_->page->kind != leaf_kind) { throw std::invalid_argument{"Paged index leaf chain references a branch"}; }
    if (state_->offset == state_->page->count) {
      state_->page_id = state_->page->next;
      state_->offset = 0;
      state_->page.reset();
      continue;
    }
    const auto entry = state_->page->entry(state_->offset);
    if (entry.key > state_->upper) { state_->page_id = 0; state_->page.reset(); return {}; }
    if ((state_->previous && !entry_less(*state_->previous, entry)) ||
        state_->emitted == state_->owner->metadata.entries) {
      throw std::invalid_argument{"Paged index leaf chain ordering/count mismatch"};
    }
    ++state_->offset;
    ++state_->emitted;
    state_->previous = entry;
    return entry;
  }
  return {};
}

// Validate the bound header before any query can expose a page or a false missing result.
paged_index_file::paged_index_file(const std::filesystem::path& path,
    std::span<const std::uint8_t> binding, const paged_index_decode_metadata& decode,
    const paged_index_options& options)
    : state_{} {
  validate_options(options);
  auto resource = std::make_shared<page_cache_resource>(options.max_cache_bytes);
  state_ = std::allocate_shared<implementation>(page_cache_allocator<implementation>{resource},
      path, binding, decode, options, resource);
}
// Cursors independently retain this immutable file generation and its cache.
paged_index_file::~paged_index_file() = default;
// Transfer owning file/cache state without reopening a mutable path.
paged_index_file::paged_index_file(paged_index_file&&) noexcept = default;
// Replace one handle while independently pinned cursors retain their original source generation.
paged_index_file& paged_index_file::operator=(paged_index_file&&) noexcept = default;

// Descend a pageable sorted directory, releasing each parent before loading the child.
paged_index_file::cursor paged_index_file::range(std::uint64_t lower, std::uint64_t upper) const {
  if (!state_) { throw std::logic_error{"Moved paged index handle"}; }
  implementation::operation_guard guard{*state_};
  check_cancelled(state_->options);
  std::shared_ptr<cursor::implementation> result;
  for (;;) {
    try {
      result = std::allocate_shared<cursor::implementation>(
          page_cache_allocator<cursor::implementation>{state_->resource}, state_);
      break;
    } catch (const std::length_error&) {
      if (!state_->evict()) { throw; }
    }
  }
  result->upper = upper;
  if (lower > upper) { return cursor{std::move(result)}; }
  std::uint64_t id = state_->metadata.root_page;
  std::optional<std::uint64_t> expected_maximum;
  for (std::uint64_t depth = state_->metadata.depth; depth != 0; --depth) {
    auto page = state_->load(id);
    if (expected_maximum && page->maximum != *expected_maximum) {
      throw std::invalid_argument{"Paged index child maximum differs from parent directory"};
    }
    if (depth == 1) {
      if (page->kind != leaf_kind) { throw std::invalid_argument{"Paged index declared depth differs from leaf"}; }
      std::size_t first{};
      while (first < page->count && page->entry(first).key < lower) { ++first; }
      if (first == page->count) { return cursor{std::move(result)}; }
      result->page_id = id;
      result->offset = first;
      result->page = std::move(page);
      return cursor{std::move(result)};
    }
    if (page->kind != branch_kind) { throw std::invalid_argument{"Paged index directory depth mismatch"}; }
    std::size_t first{};
    while (first < page->count && page->entry(first).key < lower) { ++first; }
    if (first == page->count) { return cursor{std::move(result)}; }
    const auto selected = page->entry(first);
    id = selected.identity;
    expected_maximum = selected.key;
  }
  throw std::invalid_argument{"Paged index has no reachable leaf"};
}

// Calculate current buffer/pin totals without allocating or treating pinned pages as evictable.
paged_index_statistics paged_index_file::statistics() const {
  if (!state_) { throw std::logic_error{"Moved paged index handle"}; }
  state_->check_thread();
  auto result = state_->statistics;
  result.resident_pages = state_->cache.size();
  result.resident_page_bytes = result.resident_pages * state_->options.page_bytes;
  result.resident_cache_bytes = state_->resource->current();
  result.peak_cache_bytes = state_->resource->peak();
  result.cursors = state_->cursors;
  for (const auto& [id, entry] : state_->cache) {
    static_cast<void>(id);
    if (entry.value.use_count() > 1) { ++result.pinned_pages; }
  }
  return result;
}

// Expose only the manifest verified against this handle's exact expected source binding.
const paged_index_metadata& paged_index_file::metadata() const {
  if (!state_) { throw std::logic_error{"Moved paged index handle"}; }
  state_->check_thread();
  return state_->metadata;
}

} // namespace rohit::managed::detail
