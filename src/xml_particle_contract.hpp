// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/xml.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rohit::serializer::writer::xml_binding {
struct schema_bundle;
struct schema_component;

enum class particle_kind { element, sequence, choice, all, wildcard };

// Own the complete compositor tree independently of native field-storage decisions.
struct content_particle {
  particle_kind kind{particle_kind::sequence};
  xml::name name{};
  std::size_t minimum{1U};
  std::optional<std::size_t> maximum{1U};
  std::vector<content_particle> children{};
  std::string namespace_rule{};
  std::string target_namespace{};
  // Compare expanded identities and exact group structure without AST pointers.
  bool operator==(const content_particle&) const = default;
};

struct particle_contract {
  content_particle root{};
  // Grouped/repeated heterogeneous particles require an explicit ordered owning representation.
  bool advanced{};
  // Compare complete shared particle contracts for conservative evolution diagnostics.
  bool operator==(const particle_contract&) const = default;
};
struct particle_limits {
  std::size_t max_particles{xml::limits{}.max_nodes};
  std::size_t max_depth{xml::limits{}.max_depth};
  std::size_t max_trace_names{100000U};
  std::size_t max_states{100000U};
  std::size_t max_work_units{64U * 1024U * 1024U};
};

// State zero rejects; state one is initial. Symbols use the owning variant's stable alternative order.
struct ordered_automaton {
  std::vector<std::vector<std::size_t>> transitions{};
  std::vector<bool> accepting{};
};

// Compile bounded ordered element grammars once, keeping generated readers allocation-free per event.
[[nodiscard]] ordered_automaton compile_ordered_automaton(const particle_contract& contract,
    std::span<const xml::name> symbols, const particle_limits& limits = {});

// Compile expanded names, exact group bounds and deterministic branch attribution from pinned XSD.
[[nodiscard]] particle_contract compile_particle_contract(const schema_bundle& bundle,
    std::span<const schema_component> content, const particle_limits& limits = {});
// Validate only the ordered expanded-name trace. Scalar values and wildcard policies are separate.
// All temporary matching state is bounded and discarded on failure; cancellation shares context.
void validate_particle_trace(const particle_contract& contract, std::span<const xml::name> trace,
    const particle_limits& limits = {}, xml::operation_context* context = nullptr);
} // namespace rohit::serializer::writer::xml_binding
