// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml.hpp>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::xml {

// A strict XSD 1.0 identity XPath projection; unprefixed steps have no namespace.
struct identity_path {
  std::vector<name> steps{};
  bool descendants{};
  std::optional<name> attribute{};
  // Compare compiled expanded identities independently of prefix spellings.
  bool operator==(const identity_path&) const = default;
};

enum class identity_kind { key, unique, keyref };

// Fields retain their built-in scalar value space rather than their native storage spelling.
struct identity_field {
  identity_path path{};
  std::string lexical_kind{"string"};
  // XSD key fields cannot select an element declared nillable, even with present non-nil content.
  // Manual two-member aggregate contracts retain their original non-nillable semantics.
  bool nillable{};
  // Compare the exact static projection and value-space contract.
  bool operator==(const identity_field&) const = default;
};

// Each declaration owns one selector and a nonempty, ordered tuple of field projections.
struct identity_constraint {
  name id{};
  identity_kind kind{identity_kind::key};
  identity_path selector{};
  std::vector<identity_field> fields{};
  std::optional<name> refer{};
  // Compare portable identity metadata without document instance state.
  bool operator==(const identity_constraint&) const = default;
};

// References are deliberately confined to key/unique declarations in this exact owner scope.
struct identity_scope {
  name owner{};
  std::vector<identity_constraint> constraints{};
  // Compare identity contracts for compatibility diagnostics.
  bool operator==(const identity_scope&) const = default;
};

// Bound selector work and tuple indexes in addition to the caller's parsed-document limits.
struct identity_limits {
  std::size_t max_constraints{128U};
  std::size_t max_fields{32U};
  std::size_t max_xpath_bytes{4096U};
  std::size_t max_path_steps{64U};
  std::size_t max_depth{128U};
  std::size_t max_selected_nodes{100000U};
  std::size_t max_tuples{100000U};
  // Bound one lexical field, one candidate tuple, and aggregate canonical indexed value bytes.
  std::size_t max_tuple_bytes{8U * 1024U * 1024U};
  std::size_t max_work_units{64U * 1024U * 1024U};
};

struct event_view;

// Incremental identity checking retains open paths, selected scalar fields and bounded tuple
// indexes. Supply structurally checked events through end_document; this mutable pass is neither
// thread-safe nor reentrant. The immutable scope and optional context must outlive this pass.
class identity_validator {
  struct implementation;
  std::unique_ptr<implementation> state_;
public:
  // Borrow checked metadata and seed namespace bindings inherited from outside this owner scope.
  explicit identity_validator(const identity_scope& scope, identity_limits bounds = {},
      operation_context* context = nullptr,
      const std::map<std::string, std::string>& namespace_bindings = {});
  // Discard unfinished indexes without authorizing model publication.
  ~identity_validator();
  identity_validator(const identity_validator&) = delete;
  identity_validator& operator=(const identity_validator&) = delete;
  // Validate one borrowed event synchronously; every exception permanently fences this pass.
  void accept(const event_view& value);
  // Require checked owner-scope EOF and resolved local keyrefs; idempotent after success.
  void finish();
  // Report full identity completion, including every forward reference.
  [[nodiscard]] bool complete() const noexcept;
  // Report terminal failure, including cancellation and allocation callback exceptions.
  [[nodiscard]] bool failed() const noexcept;
};

// Selectors support '.', child QName paths and an optional initial './/'; fields support '.',
// child paths and a terminal @QName. Wildcards, predicates, unions, axes and other functions reject.
[[nodiscard]] identity_path compile_identity_path(std::string_view expression, const element& scope,
    bool selector, const identity_limits& limits = {});

// Check exact local-scope references, tuple arity and compatible scalar value-space families.
// Supported fields are strings/names, booleans, exact decimal/integer types, and expanded QNames.
void validate_identity_scope_contract(const identity_scope& scope, const identity_limits& limits = {},
    operation_context* context = nullptr);

// Validate a normalized document transactionally; nothing is retained or published on failure.
// Key fields must be complete/non-nil, unique/keyref incomplete tuples are omitted, and all
// forward keyrefs resolve only after the complete owner scope's key/unique tables are built.
void validate_identity_scope(const element& document, const identity_scope& scope,
    const identity_limits& limits = {}, operation_context* context = nullptr);

} // namespace rohit::serializer::xml
