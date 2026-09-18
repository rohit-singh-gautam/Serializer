#include <rohit/json_text.hpp>
#include <rohit/runtime_simd.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace detail = rohit::serializer::detail;

// Decode code points independently of the production sequence and vector-mask algorithms.
bool reference_utf8(std::span<const std::uint8_t> bytes) {
  std::size_t offset{};
  while (offset < bytes.size()) {
    const auto first = bytes[offset++];
    if (first < 0x80) {
      continue;
    }
    std::size_t continuation{};
    std::uint32_t point{};
    std::uint32_t minimum{};
    if ((first & 0xe0) == 0xc0) {
      continuation = 1;
      point = first & 0x1f;
      minimum = 0x80;
    } else if ((first & 0xf0) == 0xe0) {
      continuation = 2;
      point = first & 0x0f;
      minimum = 0x800;
    } else if ((first & 0xf8) == 0xf0) {
      continuation = 3;
      point = first & 0x07;
      minimum = 0x10000;
    } else {
      return false;
    }
    if (continuation > bytes.size() - offset) {
      return false;
    }
    for (std::size_t index = 0; index < continuation; ++index) {
      const auto next = bytes[offset++];
      if ((next & 0xc0) != 0x80) {
        return false;
      }
      point = (point << 6) | (next & 0x3f);
    }
    if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) {
      return false;
    }
  }
  return true;
}

// Check both backends against the independent reference for an exact input extent.
void compare_utf8(std::span<const std::uint8_t> bytes) {
  const auto expected = reference_utf8(bytes);
  ASSERT_EQ(detail::is_valid_utf8_baseline(bytes.data(), bytes.size()), expected);
  ASSERT_EQ(detail::is_valid_utf8(bytes.data(), bytes.size()), expected);
}
} // namespace

// Exhaust all one/two-byte classifications at vector and cross-vector positions.
TEST(utf8_validation, exhaustive_byte_pairs) {
  std::array<std::uint8_t, 96> bytes{};
  for (const std::size_t offset : {0u, 15u, 30u, 31u, 32u, 63u}) {
    bytes.fill('\n');
    for (unsigned first = 0; first <= 0xff; ++first) {
      for (unsigned second = 0; second <= 0xff; ++second) {
        bytes[offset] = static_cast<std::uint8_t>(first);
        bytes[offset + 1] = static_cast<std::uint8_t>(second);
        compare_utf8(bytes);
        if (HasFatalFailure()) {
          return;
        }
      }
    }
  }
}

// Cover every legal Unicode scalar in a dense stream with naturally varying vector boundaries.
TEST(utf8_validation, every_unicode_scalar) {
  std::vector<std::uint8_t> bytes{};
  for (std::uint32_t point = 0; point <= 0x10ffff; ++point) {
    if (point >= 0xd800 && point <= 0xdfff) {
      continue;
    }
    if (point < 0x80) {
      bytes.push_back(static_cast<std::uint8_t>(point));
    } else {
      const unsigned continuation = point < 0x800 ? 1 : point < 0x10000 ? 2 : 3;
      const auto prefix = continuation == 1 ? 0xc0u : continuation == 2 ? 0xe0u : 0xf0u;
      bytes.push_back(static_cast<std::uint8_t>(prefix | (point >> (6 * continuation))));
      for (auto remaining = continuation; remaining != 0; --remaining) {
        bytes.push_back(
            static_cast<std::uint8_t>(0x80 | ((point >> (6 * (remaining - 1))) & 0x3f)));
      }
    }
  }
  ASSERT_TRUE(reference_utf8(bytes));
  compare_utf8(bytes);
}

// Exact-size allocations expose overreads under sanitizers, including incomplete vector tails.
TEST(utf8_validation, unaligned_boundaries_and_truncation) {
  EXPECT_TRUE(detail::is_valid_utf8(nullptr, 0));
  EXPECT_TRUE(detail::is_valid_utf8_baseline(nullptr, 0));
  constexpr std::string_view sequences[] = {
      "\xc2\x80",         "\xdf\xbf",         "\xe0\xa0\x80",     "\xed\x9f\xbf",
      "\xee\x80\x80",     "\xf0\x90\x80\x80", "\xf4\x8f\xbf\xbf", "\x80",
      "\xc0\xaf",         "\xc1\xbf",         "\xe0\x9f\xbf",     "\xed\xa0\x80",
      "\xf0\x8f\xbf\xbf", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xff"};
  for (const auto sequence : sequences) {
    for (std::size_t prefix = 0; prefix <= 65; ++prefix) {
      for (std::size_t length = 0; length <= sequence.size(); ++length) {
        for (const std::size_t suffix : {0u, 1u, 32u}) {
          const auto text = std::string(prefix, '\0') + std::string{sequence.substr(0, length)} +
                            std::string(suffix, '\\');
          for (const std::size_t alignment : {0u, 1u, 7u, 15u}) {
            auto allocation = std::make_unique<std::uint8_t[]>(alignment + text.size());
            std::memcpy(allocation.get() + alignment, text.data(), text.size());
            compare_utf8({allocation.get() + alignment, text.size()});
            if (HasFatalFailure()) {
              return;
            }
          }
        }
      }
    }
  }
}

// Mutations exercise misplaced continuations, adjacent leads, and late errors in valid vectors.
TEST(utf8_validation, mutations_and_random_spans) {
  const std::string pattern{"\0\n\"\\\xc3\xa9\xe4\xb8\x96\xf0\x9f\x8c\x8d", 13};
  std::vector<std::uint8_t> bytes{};
  for (std::size_t repeat = 0; repeat < 8; ++repeat) {
    bytes.insert(bytes.end(), pattern.begin(), pattern.end());
  }
  ASSERT_TRUE(reference_utf8(bytes));
  for (std::size_t position = 0; position < bytes.size(); ++position) {
    const auto original = bytes[position];
    for (unsigned value = 0; value <= 0xff; ++value) {
      bytes[position] = static_cast<std::uint8_t>(value);
      compare_utf8(bytes);
      if (HasFatalFailure()) {
        return;
      }
    }
    bytes[position] = original;
  }
  std::mt19937 random{0x75746638};
  for (std::size_t sample = 0; sample < 10000; ++sample) {
    bytes.resize(random() % 257);
    for (auto& value : bytes) {
      value = static_cast<std::uint8_t>(random());
    }
    compare_utf8(bytes);
    if (HasFatalFailure()) {
      return;
    }
  }
}
