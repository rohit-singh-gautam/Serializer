// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rohit::detail {
// Borrow ordinary short absolute paths without allocation. Own only a transient Windows I/O
// spelling when full resolution or an extended-length prefix is needed. The caller's logical
// path stays unchanged, including Unicode code units and explicitly supplied device syntax.
class native_file_path {
  const std::filesystem::path& original_;
  std::filesystem::path resolved_{};
  bool resolved_active_{}, prefix_added_{};
public:
  // The original lvalue must survive this adapter. force_extended is for recursive filesystem
  // operations whose descendants can exceed the boundary even when their root is short.
  explicit native_file_path(const std::filesystem::path& path, bool force_extended = false)
      : original_{path} {
#ifdef _WIN32
    constexpr auto extended_prefix = std::wstring_view{LR"(\\?\)"};
    constexpr auto device_prefix = std::wstring_view{LR"(\\.\)"};
    constexpr auto unc_prefix = std::wstring_view{LR"(\\?\UNC\)"};
    constexpr auto maximum_native_characters = std::size_t{32'767};
    constexpr auto directory_boundary_characters = std::size_t{MAX_PATH - 12};
    const auto& input = path.native();
    if (input.empty() || input.find(L'\0') != std::wstring::npos) {
      throw std::system_error{ERROR_INVALID_NAME, std::system_category(), "Resolve native file path"};
    }
    // Explicit namespace syntax has caller-selected normalization semantics. Never prefix it
    // again or normalize away literal trailing characters admitted by that namespace.
    if (input.starts_with(extended_prefix) || input.starts_with(device_prefix)) { return; }
    if (!force_extended && path.is_absolute() && input.size() < directory_boundary_characters) { return; }
    // GetFullPathNameW performs Win32 separator/dot/current-directory normalization before
    // extended syntax disables those rules. Do not perform Unicode normalization or case folding.
    const auto required = GetFullPathNameW(input.c_str(), 0, nullptr, nullptr);
    if (!required) {
      throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), "Resolve native file path"};
    }
    if (required > maximum_native_characters) {
      throw std::system_error{ERROR_FILENAME_EXCED_RANGE, std::system_category(), "Resolve native file path"};
    }
    std::wstring absolute(required, L'\0');
    const auto written = GetFullPathNameW(input.c_str(), required, absolute.data(), nullptr);
    if (!written || written >= required) {
      const auto failure = written ? ERROR_INSUFFICIENT_BUFFER : GetLastError();
      throw std::system_error{static_cast<int>(failure), std::system_category(), "Resolve native file path"};
    }
    absolute.resize(written);
    if (force_extended || absolute.size() >= directory_boundary_characters) {
      const auto unc = absolute.starts_with(LR"(\\)");
      const auto extra = unc ? unc_prefix.size() - std::wstring_view{LR"(\\)"}.size() : extended_prefix.size();
      if (absolute.size() >= maximum_native_characters - extra) {
        throw std::system_error{ERROR_FILENAME_EXCED_RANGE, std::system_category(), "Resolve native file path"};
      }
      absolute = unc ? std::wstring{unc_prefix} + absolute.substr(2) : std::wstring{extended_prefix} + absolute;
      prefix_added_ = true;
    }
    resolved_ = std::filesystem::path{std::move(absolute)};
    resolved_active_ = true;
#else
    static_cast<void>(force_extended);
#endif
  }
  native_file_path(std::filesystem::path&&, bool = false) = delete;
  native_file_path(const native_file_path&) = delete;
  native_file_path& operator=(const native_file_path&) = delete;
  // Borrow the stable native argument only for the lifetime of this adapter and original owner.
  const std::filesystem::path& get() const noexcept { return resolved_active_ ? resolved_ : original_; }
  // Existing native call sites retain their exact flags/sharing while changing only path spelling.
  const std::filesystem::path::value_type* c_str() const noexcept { return get().c_str(); }
  // Prefixes introduced by this adapter are not a new canonical journal/source identity.
  bool prefix_added() const noexcept { return prefix_added_; }
};

// Resolve the existing journal identity while keeping internally added I/O prefixes transient.
inline std::filesystem::path canonical_file_path(const std::filesystem::path& path) {
#ifdef _WIN32
  const native_file_path native{path};
  auto canonical = std::filesystem::weakly_canonical(native.get());
  if (native.prefix_added()) {
    const auto& value = canonical.native();
    constexpr auto unc_prefix = std::wstring_view{LR"(\\?\UNC\)"};
    constexpr auto extended_prefix = std::wstring_view{LR"(\\?\)"};
    if (value.starts_with(unc_prefix)) { return std::filesystem::path{std::wstring{LR"(\\)"} + value.substr(unc_prefix.size())}; }
    if (value.starts_with(extended_prefix)) { return std::filesystem::path{value.substr(extended_prefix.size())}; }
  }
  return canonical;
#else
  return std::filesystem::weakly_canonical(std::filesystem::absolute(path));
#endif
}
} // namespace rohit::detail
