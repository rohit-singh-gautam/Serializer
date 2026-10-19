// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "xml_provider_bridge.hpp"

#include <rohit/xml.hpp>

#include <libxml/xmlversion.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;
constexpr std::size_t error_capacity_bytes{512U};
thread_local std::array<char, error_capacity_bytes> last_error{};

enum class provider_status : std::int32_t { success, invalid, limit, failure };

// Check cross-language fixed-width sizes before converting them to the provider's host extent type.
std::size_t checked_extent(std::uint64_t value) {
  if (value > std::numeric_limits<std::size_t>::max()) {
    throw std::length_error{"Prepared XML provider extent exceeds the host range"};
  }
  return static_cast<std::size_t>(value);
}

// Construct only a synchronous view; the client retains the readable bytes for the complete call.
std::string_view borrowed_bytes(const char* data, std::uint64_t size) {
  const auto count = checked_extent(size);
  if (count == 0U) { return {}; }
  if (data == nullptr) { throw std::invalid_argument{"Null prepared XML provider input"}; }
  return {data, count};
}

// Copy bounded diagnostics without allocating or throwing from an exception handler.
void remember_error(std::string_view message) noexcept {
  const auto count = std::min(message.size(), last_error.size() - 1U);
  std::copy_n(message.data(), count, last_error.data());
  last_error[count] = '\0';
}

// Copy optional caller diagnostics/version text while reporting the complete required byte length.
std::uint64_t copy_text(std::string_view source, char* output, std::uint64_t capacity) noexcept {
  if (output != nullptr && capacity != 0U) {
    const auto available = static_cast<std::size_t>(std::min<std::uint64_t>(capacity - 1U,
        std::numeric_limits<std::size_t>::max()));
    const auto count = std::min(source.size(), available);
    std::copy_n(source.data(), count, output);
    output[count] = '\0';
  }
  return static_cast<std::uint64_t>(source.size());
}
} // namespace

// Each prepared handle owns immutable provider state and never retains the client's schema bytes.
void* serializer_xml_provider_prepare(const char* schema, std::uint64_t size) noexcept {
  last_error.front() = '\0';
  try {
    auto result = std::make_unique<xml::prepared_schema>(borrowed_bytes(schema, size));
    return result.release();
  } catch (const std::exception& error) {
    remember_error(error.what());
  } catch (...) {
    remember_error("Unknown prepared XML schema failure");
  }
  return nullptr;
}

// The runtime performs strict bounded no-fetch preflight before its fresh libxml2 validation context.
std::int32_t serializer_xml_provider_validate(const void* schema, const char* document,
    std::uint64_t size, std::uint64_t max_input_bytes, std::uint64_t max_depth,
    std::uint64_t max_nodes, std::uint64_t max_attributes, std::uint64_t max_text_bytes) noexcept {
  last_error.front() = '\0';
  try {
    if (schema == nullptr) { throw std::invalid_argument{"Null prepared XML schema handle"}; }
    xml::limits bounds{};
    bounds.max_input_bytes = checked_extent(max_input_bytes);
    bounds.max_depth = checked_extent(max_depth);
    bounds.max_nodes = checked_extent(max_nodes);
    bounds.max_attributes = checked_extent(max_attributes);
    bounds.max_text_bytes = checked_extent(max_text_bytes);
    static_cast<const xml::prepared_schema*>(schema)->validate(borrowed_bytes(document, size), bounds);
    return static_cast<std::int32_t>(provider_status::success);
  } catch (const std::length_error& error) {
    remember_error(error.what());
    return static_cast<std::int32_t>(provider_status::limit);
  } catch (const std::invalid_argument& error) {
    remember_error(error.what());
    return static_cast<std::int32_t>(provider_status::invalid);
  } catch (const std::exception& error) {
    remember_error(error.what());
  } catch (...) {
    remember_error("Unknown prepared XML validation failure");
  }
  return static_cast<std::int32_t>(provider_status::failure);
}

// Null deletion is harmless; non-null handles must be uniquely owned and no longer borrowed by workers.
void serializer_xml_provider_release(void* schema) noexcept {
  delete static_cast<xml::prepared_schema*>(schema);
}

// Preserve calling-thread diagnostics across the C ABI without exposing C++ object storage.
std::uint64_t serializer_xml_provider_error(char* output, std::uint64_t capacity) noexcept {
  return copy_text(std::string_view{last_error.data()}, output, capacity);
}

// Record the actual native provider used by every generated SDK callback in this qualification fixture.
std::uint64_t serializer_xml_provider_version(char* output, std::uint64_t capacity) noexcept {
  return copy_text(LIBXML_DOTTED_VERSION, output, capacity);
}
