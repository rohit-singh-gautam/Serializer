// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapped_view_platform.hpp"
#include <rohit/decode.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <malloc.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
thread_local bool record_allocations{};
thread_local std::size_t allocation_count{};
thread_local std::size_t allocation_bytes{};

// Fail outside measured regions so diagnostic allocations cannot be mistaken for codec costs.
void require(bool condition, const char* message) {
  if (!condition) { throw std::runtime_error{message}; }
}

// Count every ordinary allocation made by the current thread during a selected native operation.
void count_allocation(std::size_t bytes) noexcept {
  if (record_allocations) { ++allocation_count; allocation_bytes += bytes; }
}

// Own one real shared file mapping; every borrowed view must die before this owner.
class mapped_file {
  std::filesystem::path path_{};
  std::size_t size_{};
  std::uint8_t* data_{};
#if defined(_WIN32)
  HANDLE file_{INVALID_HANDLE_VALUE};
  HANDLE mapping_{};
#else
  int file_{-1};
#endif
public:
  // Create an isolated temporary file with an unaligned message between guard bytes.
  explicit mapped_file(std::span<const std::uint8_t> bytes)
      : path_{std::filesystem::temp_directory_path() / ("serializer-mapped-view-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin")},
        size_{bytes.size() + 2} {
    try {
      std::ofstream output{path_, std::ios::binary};
      output.put('L');
      output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      output.put('R');
      output.close();
      require(!output.fail(), "Cannot create mapped fixture");
#if defined(_WIN32)
      file_ = CreateFileW(path_.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY, nullptr);
      require(file_ != INVALID_HANDLE_VALUE, "Cannot open mapped fixture");
      mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READWRITE, 0, 0, nullptr);
      require(mapping_ != nullptr, "Cannot create Windows mapping");
      data_ = static_cast<std::uint8_t*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, size_));
      require(data_ != nullptr, "Cannot map Windows fixture");
#else
      file_ = ::open(path_.c_str(), O_RDWR);
      require(file_ >= 0, "Cannot open mapped fixture");
      auto* memory = ::mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, file_, 0);
      require(memory != MAP_FAILED, "Cannot map POSIX fixture");
      data_ = static_cast<std::uint8_t*>(memory);
#endif
    } catch (...) { release(); throw; }
  }
  mapped_file(const mapped_file&) = delete;
  mapped_file& operator=(const mapped_file&) = delete;
  // Unmap and close before removing only the temporary file created by this owner.
  ~mapped_file() { release(); }
  // Release partial construction safely without throwing from cleanup.
  void release() noexcept {
#if defined(_WIN32)
    if (data_) { UnmapViewOfFile(data_); data_ = nullptr; }
    if (mapping_) { CloseHandle(mapping_); mapping_ = nullptr; }
    if (file_ != INVALID_HANDLE_VALUE) { CloseHandle(file_); file_ = INVALID_HANDLE_VALUE; }
#else
    if (data_) { ::munmap(data_, size_); data_ = nullptr; }
    if (file_ >= 0) { ::close(file_); file_ = -1; }
#endif
    std::error_code ignored;
    if (!path_.empty()) { std::filesystem::remove(path_, ignored); }
  }
  // Borrow only the encoded message, excluding unaligned guard bytes.
  std::span<std::uint8_t> bytes() const noexcept { return {data_ + 1, size_ - 2}; }
  // Verify a failed write or mapping never crosses the message's physical window.
  bool guards_intact() const noexcept { return data_[0] == 'L' && data_[size_ - 1] == 'R'; }
  // Synchronize size-preserving edits and independently read their file bytes.
  std::vector<std::uint8_t> persisted() const {
#if defined(_WIN32)
    require(FlushViewOfFile(data_, size_) != 0 && FlushFileBuffers(file_) != 0, "Cannot flush Windows mapping");
#else
    require(::msync(data_, size_, MS_SYNC) == 0, "Cannot flush POSIX mapping");
#endif
    std::ifstream input{path_, std::ios::binary};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
  }
};
} // namespace

// Record standard heap allocation while retaining the platform allocator's ownership contract.
void* operator new(std::size_t bytes) {
  count_allocation(bytes);
  if (auto* result = std::malloc(bytes == 0 ? 1 : bytes)) { return result; }
  throw std::bad_alloc{};
}
// Route array allocations through the same measured allocation family.
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
// Release ordinary allocation without attributing cleanup to mapping.
void operator delete(void* value) noexcept { std::free(value); }
// Release ordinary array allocation through the matching family.
void operator delete[](void* value) noexcept { std::free(value); }
// Preserve sized deallocation for compilers that select it.
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
// Preserve sized array deallocation for compilers that select it.
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

// Qualify file-backed aliasing, bounds, zero-allocation traversal and reproducible scoped costs.
int main() {
  try {
    using rohit::serializer::storage_mode;
    using owning = mapped_view_platform<storage_mode::owning>;
    using readonly = mapped_view_platform<storage_mode::read_only_view>;
    using mutable_view = mapped_view_platform<storage_mode::mutable_view>;
    constexpr std::size_t label_bytes = 64 * 1024;
    constexpr std::size_t element_count = 16 * 1024;
    constexpr std::size_t repetitions = 100;
    owning original{};
    original.value = 7;
    original.label.assign(label_bytes, 'x');
    original.values.resize(element_count);
    std::iota(original.values.begin(), original.values.end(), 0u);
    rohit::full_stream_auto_alloc encoded{};
    original.serialize_out<rohit::serializer::binary_none>(encoded);
    mapped_file file{{encoded.begin(), encoded.current_offset()}};
    auto reader = readonly::map(file.bytes());
    auto editor = mutable_view::map(file.bytes());
    require(reader.get_label().data() >= reinterpret_cast<const char*>(file.bytes().data()) &&
        reader.get_label().data() + label_bytes <= reinterpret_cast<const char*>(file.bytes().data() + file.bytes().size()),
        "String did not borrow mapped storage");
    editor.set_value(42);
    require(reader.get_value() == 42, "Read-only alias did not observe mutation");
    bool rejected{};
    try { editor.set_label("short"); } catch (const std::length_error&) { rejected = true; }
    require(rejected && reader.get_label() == original.label && file.guards_intact(), "Failed setter changed layout");
    const auto persisted = file.persisted();
    auto persisted_input = rohit::make_constant_stream(persisted.data() + 1, persisted.size() - 2);
    const auto decoded = rohit::serializer::deserialize_exact<owning, rohit::serializer::binary_none>(persisted_input);
    require(decoded.value == 42 && decoded.values == original.values, "Mapped edits were not persisted");
    rohit::serializer::decode_limits limits{};
    limits.max_input_bytes = file.bytes().size() - 1;
    rejected = false;
    try { static_cast<void>(readonly::map(file.bytes(), limits)); } catch (const std::exception&) { rejected = true; }
    require(rejected, "Mapping ignored input admission");
    std::uint64_t mapped_sum{};
    allocation_count = 0; allocation_bytes = 0; record_allocations = true;
    const auto map_start = std::chrono::steady_clock::now();
    for (std::size_t iteration{}; iteration < repetitions; ++iteration) {
      const auto view = readonly::map(file.bytes());
      for (const auto value : view.get_values()) { mapped_sum += value; }
    }
    const auto map_end = std::chrono::steady_clock::now();
    record_allocations = false;
    require(allocation_count == 0 && allocation_bytes == 0, "Mapping or traversal allocated heap storage");
    std::uint64_t owning_sum{};
    allocation_count = 0; allocation_bytes = 0; record_allocations = true;
    const auto owning_start = std::chrono::steady_clock::now();
    for (std::size_t iteration{}; iteration < repetitions; ++iteration) {
      auto input = rohit::make_constant_stream(file.bytes().data(), file.bytes().size());
      const auto value = rohit::serializer::deserialize_exact<owning, rohit::serializer::binary_none>(input);
      for (const auto entry : value.values) { owning_sum += entry; }
    }
    const auto owning_end = std::chrono::steady_clock::now();
    record_allocations = false;
    require(mapped_sum == owning_sum && allocation_count > 0, "Owning benchmark did not decode the same values");
    std::cout << "bytes=" << file.bytes().size() << " iterations=" << repetitions << " mapped_allocations=0 mapped_us="
        << std::chrono::duration_cast<std::chrono::microseconds>(map_end - map_start).count()
        << " owning_us=" << std::chrono::duration_cast<std::chrono::microseconds>(owning_end - owning_start).count()
        << " owning_allocations=" << allocation_count << " owning_requested_bytes=" << allocation_bytes
        << " view_object_bytes=" << sizeof(readonly) << " checksum=" << mapped_sum << '\n';
    return 0;
  } catch (const std::exception& failure) {
    record_allocations = false;
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
