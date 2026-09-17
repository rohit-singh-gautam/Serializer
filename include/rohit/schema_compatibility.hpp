#pragma once

#include <rohit/serializer_creator.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer {

// Select the established message contract; this checker never selects a new wire format.
enum class compatibility_protocol {
  binary_none,
  binary_integer,
  binary_string,
  json,
  protobuf_binary
};

// Reserve retired parent/field IDs and wire names in one fully qualified class scope.
struct reserved_fields {
  std::string type{};
  std::vector<std::uint32_t> ids{};
  std::vector<std::string> names{};
};

struct compatibility_policy {
  std::vector<reserved_fields> reservations{};
};

// A conservative incompatibility or identity hazard; direction refers to schema revisions.
struct compatibility_issue {
  std::string path{};
  std::string message{};
  bool breaks_old_reader{};
  bool breaks_new_reader{};
};

// Accept only supported explicit protocol names; unknown names throw invalid_argument.
compatibility_protocol parse_compatibility_protocol(std::string_view name);

// Read a strict version-1 JSON policy, rejecting unknown/duplicate keys and invalid reservations.
// Input is bounded to 1 MiB; failure throws without returning a partially read policy.
compatibility_policy read_compatibility_policy(const std::filesystem::path& path);

// Compare resolved schema revisions and enforce the policy on the new revision.
// Changes are reported conservatively for all declarations, including included files.
// Callers must agree on fixed-width byte order separately; semantics cannot be inferred from schemas.
// Reservations persist only while the caller retains them in its version-controlled policy.
std::vector<compatibility_issue>
check_schema_compatibility(const parser::parsed_schema& previous,
                           const parser::parsed_schema& current, compatibility_protocol protocol,
                           const compatibility_policy& policy = {});

} // namespace rohit::serializer
