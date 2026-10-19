// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <cstdint>

// The bridge is a test fixture, not an installed runtime dependency or a public Serializer ABI.
#if defined(_WIN32)
#if defined(SERIALIZER_XML_PROVIDER_BRIDGE_BUILD)
#define SERIALIZER_XML_PROVIDER_BRIDGE_EXPORT __declspec(dllexport)
#else
#define SERIALIZER_XML_PROVIDER_BRIDGE_EXPORT __declspec(dllimport)
#endif
#else
#define SERIALIZER_XML_PROVIDER_BRIDGE_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
// Prepare one immutable schema from borrowed bytes; return a unique owning opaque handle or null.
// Errors use the calling OS thread's bounded message. External schema acquisition is prohibited.
SERIALIZER_XML_PROVIDER_BRIDGE_EXPORT void* serializer_xml_provider_prepare(
    const char* schema, std::uint64_t size) noexcept;

// Validate a complete borrowed document with independent per-call provider state and explicit limits.
// The handle must remain alive throughout every concurrent call. Return 0 on success, 1 for invalid
// data/schema, 2 for resource limits, and 3 for another provider failure; no exception crosses the ABI.
SERIALIZER_XML_PROVIDER_BRIDGE_EXPORT std::int32_t serializer_xml_provider_validate(
    const void* schema, const char* document, std::uint64_t size, std::uint64_t max_input_bytes,
    std::uint64_t max_depth, std::uint64_t max_nodes, std::uint64_t max_attributes,
    std::uint64_t max_text_bytes) noexcept;

// Release a unique prepared handle after every borrowed validation call completes; null is harmless.
SERIALIZER_XML_PROVIDER_BRIDGE_EXPORT void serializer_xml_provider_release(void* schema) noexcept;

// Copy a NUL-terminated calling-thread error into optional caller storage; return its full byte length.
// Clients using a migrating scheduler must keep validation and error retrieval on the same OS thread.
SERIALIZER_XML_PROVIDER_BRIDGE_EXPORT std::uint64_t serializer_xml_provider_error(
    char* output, std::uint64_t capacity) noexcept;

// Copy the compiled real provider version into optional caller storage; return its full byte length.
SERIALIZER_XML_PROVIDER_BRIDGE_EXPORT std::uint64_t serializer_xml_provider_version(
    char* output, std::uint64_t capacity) noexcept;
}
