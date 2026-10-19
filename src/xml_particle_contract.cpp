// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_particle_contract.hpp"
#include "xml_schema.hpp"

#include <algorithm>
#include <charconv>
#include <functional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace rohit::serializer::writer::xml_binding {
namespace {
// Read one exact unqualified schema attribute without allocating a second inventory.
std::string_view attribute(const xml::element& node, std::string_view name) {
  for (const auto& value : node.attributes) {
    if (value.qualified_name.namespace_uri.empty() && value.qualified_name.local == name) { return value.value; }
  }
  return {};
}
// Parse finite group bounds without truncation or permissive suffixes.
std::size_t count(std::string_view text) {
  if (text.empty()) { return 1U; }
  std::size_t value{};
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
    throw std::invalid_argument{"Invalid XSD particle occurrence"};
  }
  return value;
}
// Decide whether a compositor body can match no names, independently of its own occurrence.
bool nullable_body(const content_particle& value);
// Include the particle's optional occurrence in its empty-trace contract.
bool nullable(const content_particle& value) {
  return value.minimum == 0U || nullable_body(value);
}
// Calculate empty-body acceptance through nested compositors without materializing repetitions.
bool nullable_body(const content_particle& value) {
  if (value.kind == particle_kind::element || value.kind == particle_kind::wildcard) { return false; }
  if (value.kind == particle_kind::choice) {
    return std::any_of(value.children.begin(), value.children.end(), nullable);
  }
  return std::all_of(value.children.begin(), value.children.end(), nullable);
}
// Borrow every first consuming leaf for bounded unique-particle-attribution checks.
std::vector<const content_particle*> first(const content_particle& value) {
  if (value.maximum == std::optional<std::size_t>{0U}) { return {}; }
  if (value.kind == particle_kind::element || value.kind == particle_kind::wildcard) { return {&value}; }
  std::vector<const content_particle*> result;
  for (const auto& child : value.children) {
    auto items = first(child);
    result.insert(result.end(), items.begin(), items.end());
    if (value.kind == particle_kind::sequence && !nullable(child)) { break; }
  }
  return result;
}
// Compare finite names and symbolic wildcard namespace sets without enumerating an open universe.
bool overlaps(const content_particle& left, const content_particle& right) {
  if (left.kind == particle_kind::element && right.kind == particle_kind::element) { return left.name == right.name; }
  if (left.kind == particle_kind::element) {
    return xml::matches_namespace_rule(left.name.namespace_uri, right.namespace_rule, right.target_namespace);
  }
  if (right.kind == particle_kind::element) { return overlaps(right, left); }
  std::string other{"urn:serializer:particle:other"};
  while (other == left.target_namespace || other == right.target_namespace) { other += '_'; }
  std::set<std::string> candidates{"", left.target_namespace, right.target_namespace, other};
  for (const auto* rule : {&left.namespace_rule, &right.namespace_rule}) {
    std::string_view words{*rule};
    while (!words.empty()) {
      const auto begin = words.find_first_not_of(" \t\r\n");
      if (begin == std::string_view::npos) { break; }
      words.remove_prefix(begin);
      const auto end = words.find_first_of(" \t\r\n");
      const auto word = words.substr(0U, end);
      if (!word.starts_with("##")) { candidates.emplace(word); }
      if (end == std::string_view::npos) { break; }
      words.remove_prefix(end);
    }
  }
  return std::any_of(candidates.begin(), candidates.end(), [&](const auto& uri) {
    return xml::matches_namespace_rule(uri, left.namespace_rule, left.target_namespace) &&
        xml::matches_namespace_rule(uri, right.namespace_rule, right.target_namespace);
  });
}
// Reject competing first leaves at a choice boundary before accepting a compiled model.
void check_choice(const content_particle& value) {
  for (std::size_t left{}; left < value.children.size(); ++left) {
    for (std::size_t right = left + 1U; right < value.children.size(); ++right) {
      for (const auto* a : first(value.children[left])) {
        for (const auto* b : first(value.children[right])) {
          if (a != b && overlaps(*a, *b)) { throw std::invalid_argument{"Ambiguous XSD choice branch expanded names"}; }
        }
      }
    }
  }
}
} // namespace

// Compile a finite tree, retaining each referenced component's defining qualification defaults.
particle_contract compile_particle_contract(const schema_bundle& bundle,
    std::span<const schema_component> content, const particle_limits& limits) {
  particle_contract result;
  std::size_t remaining = limits.max_particles;
  std::set<const xml::element*> active;
  const auto compile = [&](auto&& self, schema_component source, std::size_t depth) -> content_particle {
    if (depth > limits.max_depth || remaining == 0U || !active.insert(source.node).second) {
      throw std::length_error{"XSD particle graph depth/count/cycle limit exceeded"};
    }
    --remaining;
    const auto& node = *source.node;
    const auto& kind = node.qualified_name.local;
    content_particle value;
    value.minimum = count(attribute(node, "minOccurs"));
    const auto maximum = attribute(node, "maxOccurs");
    value.maximum = maximum == "unbounded" ? std::optional<std::size_t>{} : std::optional{count(maximum)};
    if (value.maximum && value.minimum > *value.maximum) { throw std::invalid_argument{"Empty XSD particle occurrence interval"}; }
    if (kind == "group") {
      const auto selected = resolve_model_group(bundle, *source.owner, node);
      value.children.push_back(self(self, selected, depth + 1U));
      result.advanced = result.advanced || value.minimum != 1U || value.maximum != std::optional<std::size_t>{1U};
    } else if (kind == "element") {
      value.kind = particle_kind::element;
      auto declaration = source;
      const bool referenced = !attribute(node, "ref").empty();
      if (referenced) {
        const auto name = xml::resolve_qname(attribute(node, "ref"), node);
        declaration = {};
        for (const auto& document : bundle.documents) {
          if (document.target != name.namespace_uri) { continue; }
          for (const auto& child : document.root.children) {
            if (child.qualified_name.local == "element" && attribute(child, "name") == name.local) {
              if (declaration.node) { throw std::invalid_argument{"Ambiguous XSD particle element reference"}; }
              declaration = {&document, &child};
            }
          }
        }
        if (!declaration.node) { throw std::invalid_argument{"Unresolved XSD particle element reference"}; }
      }
      const auto form = attribute(*declaration.node, "form");
      const bool qualified = referenced || form == "qualified" || (form.empty() &&
          attribute(declaration.owner->root, "elementFormDefault") == "qualified");
      value.name = {std::string{attribute(*declaration.node, "name")}, qualified ? declaration.owner->target : std::string{}};
    } else if (kind == "any") {
      value.kind = particle_kind::wildcard;
      value.namespace_rule = attribute(node, "namespace").empty() ? "##any" : attribute(node, "namespace");
      value.target_namespace = source.owner->target;
    } else if (kind == "sequence" || kind == "choice" || kind == "all") {
      value.kind = kind == "sequence" ? particle_kind::sequence : kind == "choice" ?
          particle_kind::choice : particle_kind::all;
      for (const auto& child : node.children) {
        if (child.qualified_name.local == "annotation") { continue; }
        value.children.push_back(self(self, {source.owner, &child}, depth + 1U));
      }
      if (kind == "choice") {
        check_choice(value);
        const auto known = std::count_if(value.children.begin(), value.children.end(),
            [](const auto& child) { return child.kind != particle_kind::wildcard; });
        result.advanced = result.advanced || std::any_of(value.children.begin(), value.children.end(),
            [](const auto& child) { return child.kind != particle_kind::element && child.kind != particle_kind::wildcard; }) ||
            (known > 1 && (value.maximum != std::optional<std::size_t>{1U} ||
                std::any_of(value.children.begin(), value.children.end(), [](const auto& child) {
                  return child.maximum != std::optional<std::size_t>{1U};
                })));
      }
      if (kind == "sequence" && (value.minimum != 1U || value.maximum != std::optional<std::size_t>{1U})) {
        result.advanced = true;
      }
      if (kind == "all" && (value.maximum != std::optional<std::size_t>{1U} || value.minimum > 1U ||
          std::any_of(value.children.begin(), value.children.end(), [](const auto& child) {
            return child.kind != particle_kind::element || child.maximum != std::optional<std::size_t>{1U};
          }))) { throw std::invalid_argument{"XSD 1.0 all requires singleton element children"}; }
    } else { throw std::invalid_argument{"Unsupported XSD content particle: " + kind}; }
    active.erase(source.node);
    return value;
  };
  for (const auto& component : content) {
    const auto& kind = component.node->qualified_name.local;
    if (kind == "element" || kind == "sequence" || kind == "choice" || kind == "all" || kind == "group" || kind == "any") {
      result.root.children.push_back(compile(compile, component, 0U));
    }
  }
  std::size_t work_left = limits.max_work_units;
  const auto attribution = [&](auto&& self, const content_particle& value,
                               std::vector<const content_particle*> following) -> void {
    if (work_left == 0U) { throw std::length_error{"XSD particle attribution work limit exceeded"}; }
    --work_left;
    if (!value.maximum || *value.maximum > 1U) {
      const auto again = first(value);
      following.insert(following.end(), again.begin(), again.end());
    }
    if (value.kind == particle_kind::sequence) {
      for (std::size_t index = value.children.size(); index != 0U; --index) {
        const auto& child = value.children[index - 1U];
        if (nullable(child) || !child.maximum || child.minimum != *child.maximum) {
          for (const auto* a : first(child)) {
            for (const auto* b : following) {
              if (work_left == 0U) { throw std::length_error{"XSD particle attribution work limit exceeded"}; }
              --work_left;
              if (a != b && overlaps(*a, *b)) {
                throw std::invalid_argument{"Ambiguous XSD optional/repeated particle boundary"};
              }
            }
          }
        }
        self(self, child, following);
        auto starts = first(child);
        if (nullable(child)) { following.insert(following.end(), starts.begin(), starts.end()); }
        else { following = std::move(starts); }
      }
    } else {
      for (const auto& child : value.children) { self(self, child, following); }
    }
  };
  attribution(attribution, result.root, {});
  return result;
}

// Use bounded position sets rather than expanding finite/unbounded occurrence counts into a tree.
void validate_particle_trace(const particle_contract& contract, std::span<const xml::name> trace,
    const particle_limits& limits, xml::operation_context* context) {
  if (trace.size() > limits.max_trace_names) { throw std::length_error{"XML particle trace name limit exceeded"}; }
  std::size_t remaining = limits.max_work_units;
  const auto work = [&]() {
    if (remaining == 0U) { throw std::length_error{"XML particle matching work limit exceeded"}; }
    --remaining;
    if (context) { context->consume(xml::operation_resource::work_units, 1U); }
  };
  using positions = std::set<std::size_t>;
  const auto add = [&](positions& target, const positions& source) {
    for (const auto position : source) {
      work();
      target.insert(position);
      if (target.size() > limits.max_states) { throw std::length_error{"XML particle matching state limit exceeded"}; }
    }
  };
  std::function<positions(const content_particle&, std::size_t, std::size_t)> match;
  const auto body = [&](const content_particle& value, std::size_t start, std::size_t depth) -> positions {
    work();
    if (value.kind == particle_kind::element || value.kind == particle_kind::wildcard) {
      return start < trace.size() && (value.kind == particle_kind::element ? trace[start] == value.name :
          xml::matches_namespace_rule(trace[start].namespace_uri, value.namespace_rule, value.target_namespace))
          ? positions{start + 1U} : positions{};
    }
    if (value.kind == particle_kind::choice) {
      positions result;
      for (const auto& child : value.children) { add(result, match(child, start, depth + 1U)); }
      return result;
    }
    if (value.kind == particle_kind::all) {
      std::vector<bool> seen(value.children.size());
      positions result;
      const auto complete = [&]() {
        for (std::size_t index{}; index < seen.size(); ++index) {
          if (!seen[index] && value.children[index].minimum != 0U) { return false; }
        }
        return true;
      };
      if (complete()) { result.insert(start); }
      for (std::size_t position = start; position < trace.size(); ++position) {
        work();
        const auto found = std::find_if(value.children.begin(), value.children.end(),
            [&](const auto& child) { return child.name == trace[position]; });
        if (found == value.children.end()) { break; }
        const auto index = static_cast<std::size_t>(found - value.children.begin());
        if (seen[index]) { break; }
        seen[index] = true;
        if (complete()) { result.insert(position + 1U); }
      }
      return result;
    }
    positions current{start};
    for (const auto& child : value.children) {
      positions next;
      for (const auto position : current) { add(next, match(child, position, depth + 1U)); }
      current = std::move(next);
      if (current.empty()) { break; }
    }
    return current;
  };
  match = [&](const content_particle& value, std::size_t start, std::size_t depth) -> positions {
    work();
    if (depth > limits.max_depth) { throw std::length_error{"XML particle matching depth limit exceeded"}; }
    const bool empty = nullable_body(value);
    if (!empty && value.minimum > trace.size() - start) { return {}; }
    positions accepted;
    if (value.minimum == 0U || empty) { accepted.insert(start); }
    positions current{start};
    const auto maximum = std::min(value.maximum.value_or(trace.size() - start), trace.size() - start);
    for (std::size_t repetition{}; repetition < maximum && !current.empty(); ++repetition) {
      positions next;
      for (const auto position : current) {
        auto ends = body(value, position, depth);
        ends.erase(position);
        add(next, ends);
      }
      current = std::move(next);
      if (empty || repetition + 1U >= value.minimum) { add(accepted, current); }
    }
    return accepted;
  };
  if (!match(contract.root, 0U, 0U).contains(trace.size())) {
    throw std::invalid_argument{"XML child trace violates its compiled XSD particle model"};
  }
}
} // namespace rohit::serializer::writer::xml_binding
