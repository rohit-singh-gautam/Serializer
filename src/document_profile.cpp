// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/document_profile.hpp>

#include <algorithm>
#include <charconv>
#include <set>
#include <stdexcept>
#include <tuple>

namespace rohit::serializer::document {
namespace {
// Reject unsupported envelope combinations and invalid provider-specific compression levels.
void validate(const profile& value) {
  if (value.format != payload_format::xml && value.format != payload_format::json &&
      value.format != payload_format::binary_none && value.format != payload_format::opaque_bytes) {
    throw std::invalid_argument{"Unknown document payload format"};
  }
  if (value.container != container_format::none && value.container != container_format::zip &&
      value.container != container_format::opc) {
    throw std::invalid_argument{"Unknown document container"};
  }
  if (value.policy != profile_policy::fixed && value.policy != profile_policy::defaults) {
    throw std::invalid_argument{"Unknown document profile policy"};
  }
  if (value.container != container_format::none && value.compression != compression::format::none) {
    throw std::invalid_argument{
        "Package entries select compression individually; outer compression is unsupported"};
  }
  switch (value.compression) {
  case compression::format::none:
    break;
  case compression::format::gzip:
  case compression::format::zlib:
  case compression::format::deflate:
    if (value.compression_level < -1 || value.compression_level > 9) {
      throw std::invalid_argument{"DEFLATE profile level must be -1 through 9"};
    }
    break;
  case compression::format::zstd:
    if (value.compression_level < -131072 || value.compression_level > 22) {
      throw std::invalid_argument{"Zstandard profile level is outside its portable range"};
    }
    break;
  case compression::format::lz4:
    if (value.compression_level < 0 || value.compression_level > 12) {
      throw std::invalid_argument{"LZ4 profile level must be 0 through 12"};
    }
    break;
  default:
    throw std::invalid_argument{
        "Custom compression requires an application-owned adapter, not portable profile metadata"};
  }
}
} // namespace

profile parse_profile(const std::map<std::string, std::string>& values) {
  profile result{};
  for (const auto& [key, value] : values) {
    if (key == "format") {
      if (value == "xml") {
        result.format = payload_format::xml;
      } else if (value == "json") {
        result.format = payload_format::json;
      } else if (value == "binary_none") {
        result.format = payload_format::binary_none;
      } else if (value == "bytes") {
        result.format = payload_format::opaque_bytes;
      } else {
        throw std::invalid_argument{"Unknown document profile format"};
      }
    } else if (key == "compression") {
      if (value == "none") {
        result.compression = compression::format::none;
      } else if (value == "gzip") {
        result.compression = compression::format::gzip;
      } else if (value == "zlib") {
        result.compression = compression::format::zlib;
      } else if (value == "deflate") {
        result.compression = compression::format::deflate;
      } else if (value == "zstd") {
        result.compression = compression::format::zstd;
      } else if (value == "lz4") {
        result.compression = compression::format::lz4;
      } else {
        throw std::invalid_argument{"Unknown document profile compression"};
      }
    } else if (key == "container") {
      if (value == "none") {
        result.container = container_format::none;
      } else if (value == "zip") {
        result.container = container_format::zip;
      } else if (value == "opc") {
        result.container = container_format::opc;
      } else {
        throw std::invalid_argument{"Unknown document profile container"};
      }
    } else if (key == "policy") {
      if (value == "fixed") {
        result.policy = profile_policy::fixed;
      } else if (value == "defaults") {
        result.policy = profile_policy::defaults;
      } else {
        throw std::invalid_argument{"Unknown document profile policy"};
      }
    } else if (key == "level") {
      const auto parsed =
          std::from_chars(value.data(), value.data() + value.size(), result.compression_level);
      if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument{"Document profile level requires a complete decimal integer"};
      }
    } else {
      throw std::invalid_argument{"Unknown document profile key: " + key};
    }
  }
  validate(result);
  return result;
}

profile resolve_profile(const profile& defaults, const profile_override& runtime) {
  validate(defaults);
  profile result = defaults;
  if (runtime.format) {
    result.format = *runtime.format;
  }
  if (runtime.compression) {
    result.compression = *runtime.compression;
  }
  if (runtime.container) {
    result.container = *runtime.container;
  }
  if (runtime.compression_level) {
    result.compression_level = *runtime.compression_level;
  }
  if (defaults.policy == profile_policy::fixed &&
      (result.format != defaults.format || result.compression != defaults.compression ||
       result.container != defaults.container ||
       result.compression_level != defaults.compression_level)) {
    throw std::invalid_argument{"Runtime overrides conflict with a fixed document profile"};
  }
  validate(result);
  return result;
}

void validate_dispatch_inventory(std::span<const dispatch_metadata> inventory) {
  std::set<std::tuple<std::string, std::string, std::string, std::string, std::string>> keys{};
  for (const auto& item : inventory) {
    if (item.model_identity.empty() || item.root.local.empty() ||
        (item.requires_type && !item.type) || (item.type && item.type->local.empty())) {
      throw std::invalid_argument{"Incomplete document decoder metadata"};
    }
    if (!keys.emplace(item.model_identity, item.root.namespace_uri, item.root.local,
                      item.type ? item.type->namespace_uri : "", item.type ? item.type->local : "")
             .second) {
      throw std::invalid_argument{"Ambiguous document decoder metadata"};
    }
    std::set<std::string> vocabularies{};
    for (const auto& vocabulary : item.vocabularies) {
      if (vocabulary.empty() || !vocabularies.insert(vocabulary).second) {
        throw std::invalid_argument{"Duplicate or empty required document vocabulary"};
      }
    }
  }
}

const dispatch_metadata* select_decoder(std::span<const dispatch_metadata> inventory,
                                        std::string_view expected_identity, const xml::name& root,
                                        const std::optional<xml::name>& type,
                                        std::span<const std::string> understood_vocabularies) {
  const dispatch_metadata* result{};
  for (const auto& item : inventory) {
    if (item.model_identity != expected_identity || item.root != root ||
        (item.requires_type && !type) || (type && item.type != type)) {
      continue;
    }
    if (std::any_of(
            item.vocabularies.begin(), item.vocabularies.end(), [&](const auto& vocabulary) {
              return std::find(understood_vocabularies.begin(), understood_vocabularies.end(),
                               vocabulary) == understood_vocabularies.end();
            })) {
      continue;
    }
    if (result) {
      return nullptr;
    }
    result = &item;
  }
  return result;
}

const dispatch_metadata* select_decoder(std::span<const dispatch_metadata> inventory,
                                        std::string_view expected_identity,
                                        const xml::element& root,
                                        std::span<const std::string> understood_vocabularies) {
  std::optional<xml::name> type{};
  for (const auto& attribute : root.attributes) {
    if (attribute.qualified_name ==
        xml::name{"type", "http://www.w3.org/2001/XMLSchema-instance"}) {
      if (type) {
        return nullptr;
      }
      try {
        type = xml::resolve_qname(attribute.value, root);
      } catch (const std::invalid_argument&) {
        return nullptr;
      }
    }
  }
  return select_decoder(inventory, expected_identity, root.qualified_name, type,
                        understood_vocabularies);
}
} // namespace rohit::serializer::document
