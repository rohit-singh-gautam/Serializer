// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml_identity.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace rohit::serializer::xml {
namespace {
constexpr std::string_view xsi_namespace{"http://www.w3.org/2001/XMLSchema-instance"};

enum class value_family { text, uri, boolean, decimal, qname };

// Restrict tuple equality to explicitly implemented portable XSD value spaces.
value_family family(std::string_view kind) {
  if (kind == "boolean") { return value_family::boolean; }
  if (kind == "QName") { return value_family::qname; }
  if (kind == "anyURI") { return value_family::uri; }
  if (kind == "decimal" || kind == "integer" || kind == "positiveInteger" ||
      kind == "nonNegativeInteger" || kind == "negativeInteger" || kind == "nonPositiveInteger" ||
      kind == "byte" || kind == "short" || kind == "int" || kind == "long" ||
      kind == "unsignedByte" || kind == "unsignedShort" || kind == "unsignedInt" || kind == "unsignedLong") {
    return value_family::decimal;
  }
  if (kind == "string" || kind == "normalizedString" || kind == "token" ||
      kind == "language" || kind == "Name" || kind == "NCName" || kind == "ID" ||
      kind == "IDREF" || kind == "ENTITY" || kind == "NMTOKEN") { return value_family::text; }
  throw std::invalid_argument{"Unsupported portable XSD identity field value space: " + std::string{kind}};
}

// Bound logical work, tuple retention and traversal storage before each corresponding operation.
class identity_budget {
  const identity_limits& limits_;
  operation_context* context_{};
  std::size_t remaining_work_{}, tuple_count_{}, tuple_bytes_{};
public:
  // Borrow immutable limits and optional cumulative operation state during one validation.
  identity_budget(const identity_limits& limits, operation_context* context)
      : limits_{limits}, context_{context}, remaining_work_{limits.max_work_units} {}
  // Account selector/field comparisons and interruption without overflow.
  void work(std::size_t amount = 1U) {
    if (amount > remaining_work_) { throw std::length_error{"XSD identity work limit exceeded"}; }
    remaining_work_ -= amount;
    if (context_) { context_->consume(operation_resource::work_units, amount); }
  }
  // Charge logical storage, excluding allocator and container-capacity bookkeeping.
  void allocate(std::size_t amount) const {
    if (context_) { context_->consume(operation_resource::allocation_bytes, amount); }
  }
  // Keep intermediate selections bounded even when no final tuple is produced.
  void select(std::size_t count) const {
    if (count >= limits_.max_selected_nodes) { throw std::length_error{"XSD identity selected-node limit exceeded"}; }
    allocate(sizeof(const element*));
  }
  // Count complete candidate tuples and aggregate retained canonical bytes once per table entry.
  void tuple(const std::vector<std::string>& value) {
    if (tuple_count_ >= limits_.max_tuples) { throw std::length_error{"XSD identity tuple-count limit exceeded"}; }
    ++tuple_count_;
    for (const auto& item : value) {
      if (item.size() > limits_.max_tuple_bytes - tuple_bytes_) {
        throw std::length_error{"XSD identity tuple-byte limit exceeded"};
      }
      tuple_bytes_ += item.size();
    }
    allocate(sizeof(std::vector<std::string>));
  }
  // Account expanded-name comparison cost, including potentially long namespace identities.
  bool equal(const name& left, const name& right) {
    work();
    work(std::min(left.local.size(), right.local.size()));
    work(std::min(left.namespace_uri.size(), right.namespace_uri.size()));
    return left == right;
  }
};

// Resolve identity XPath names using XPath 1.0's empty default namespace for unprefixed names.
name path_name(std::string_view lexical, const element& scope) {
  if (lexical.empty()) { throw std::invalid_argument{"Empty name in XSD identity XPath"}; }
  if (lexical.find(':') == std::string_view::npos) {
    element empty{};
    return resolve_qname(lexical, empty);
  }
  return resolve_qname(lexical, scope);
}

// Traverse one exact child path with bounded intermediate result inventories.
std::vector<const element*> children_at(const element& root, const identity_path& path,
    identity_budget& budget, const identity_limits& limits, std::size_t base_depth = 0U) {
  budget.select(0U);
  std::vector<const element*> current{&root};
  auto depth = base_depth;
  for (const auto& step : path.steps) {
    std::vector<const element*> next{};
    for (const auto* parent : current) {
      for (const auto& child : parent->children) {
        if (budget.equal(child.qualified_name, step)) {
          if (depth >= limits.max_depth) { throw std::length_error{"XSD identity path depth exceeded"}; }
          budget.select(next.size());
          next.push_back(&child);
        }
      }
    }
    current = std::move(next);
    if (current.empty()) { break; }
    ++depth;
  }
  return current;
}

// Evaluate initial descendant-or-self contexts iteratively, retaining no document-owned strings.
std::vector<const element*> select_elements(const element& root, const identity_path& path,
    identity_budget& budget, const identity_limits& limits) {
  if (!path.descendants) { return children_at(root, path, budget, limits); }
  struct frame { const element* node{}; std::size_t depth{}; };
  budget.allocate(sizeof(frame));
  std::vector<frame> pending{{&root, 0U}};
  std::vector<const element*> result{};
  while (!pending.empty()) {
    budget.work();
    const auto current = pending.back();
    pending.pop_back();
    if (current.depth > limits.max_depth) {
      throw std::length_error{"XSD identity document depth exceeded"};
    }
    auto selected = children_at(*current.node, path, budget, limits, current.depth);
    for (const auto* item : selected) {
      budget.select(result.size());
      result.push_back(item);
    }
    for (const auto& child : current.node->children) {
      budget.select(pending.size());
      budget.allocate(sizeof(frame) - sizeof(const element*));
      pending.push_back({&child, current.depth + 1U});
    }
  }
  return result;
}

// Canonicalize finite exact decimal values without a binary floating-point round trip.
std::string decimal_key(std::string_view lexical) {
  bool negative{};
  if (!lexical.empty() && (lexical.front() == '+' || lexical.front() == '-')) {
    negative = lexical.front() == '-';
    lexical.remove_prefix(1U);
  }
  const auto point = lexical.find('.');
  auto integer = lexical.substr(0U, point);
  auto fraction = point == std::string_view::npos ? std::string_view{} : lexical.substr(point + 1U);
  const auto nonzero = integer.find_first_not_of('0');
  integer = nonzero == std::string_view::npos ? std::string_view{} : integer.substr(nonzero);
  const auto last = fraction.find_last_not_of('0');
  fraction = last == std::string_view::npos ? std::string_view{} : fraction.substr(0U, last + 1U);
  std::string result{};
  if (negative && (!integer.empty() || !fraction.empty())) { result += '-'; }
  result += integer.empty() ? std::string_view{"0"} : integer;
  if (!fraction.empty()) { result += '.'; result += fraction; }
  return result;
}

// Construct one complete value-space identity using the field's original namespace scope.
std::string field_key(std::string_view lexical, const identity_field& field, const element& scope,
    identity_budget& budget, operation_context* context, const identity_limits& limits) {
  budget.work(lexical.size());
  if (lexical.size() > limits.max_tuple_bytes) { throw std::length_error{"XSD identity field-byte limit exceeded"}; }
  const auto policy = field.lexical_kind == "string" ? whitespace_policy::preserve :
      field.lexical_kind == "normalizedString" ? whitespace_policy::replace : whitespace_policy::collapse;
  budget.allocate(lexical.size());
  auto normalized = normalize_whitespace(lexical, policy);
  const auto kind = family(field.lexical_kind);
  if (kind != value_family::qname) { validate_lexical_value(normalized, field.lexical_kind, scope, context); }
  if (kind == value_family::boolean) {
    const std::string_view canonical = normalized == "true" || normalized == "1" ? "true" : "false";
    budget.allocate(canonical.size());
    return std::string{canonical};
  }
  if (kind == value_family::decimal) {
    budget.allocate(normalized.size() + 1U);
    return decimal_key(normalized);
  }
  if (kind == value_family::qname) {
    const auto colon = normalized.find(':');
    const auto prefix = colon == std::string::npos ? std::string_view{} : std::string_view{normalized}.substr(0U, colon);
    budget.allocate(prefix.size());
    const auto binding = scope.namespace_bindings.find(std::string{prefix});
    const auto uri = prefix == "xml" ? std::string_view{"http://www.w3.org/XML/1998/namespace"} :
        binding == scope.namespace_bindings.end() ? std::string_view{} : std::string_view{binding->second};
    const auto local_size = colon == std::string::npos ? normalized.size() : normalized.size() - colon - 1U;
    if (uri.size() > limits.max_tuple_bytes || local_size > limits.max_tuple_bytes - uri.size()) {
      throw std::length_error{"XSD identity QName byte limit exceeded"};
    }
    budget.allocate(uri.size() + local_size);
    const auto expanded = resolve_qname(normalized, scope);
    if (expanded.namespace_uri.size() > limits.max_tuple_bytes ||
        expanded.local.size() > limits.max_tuple_bytes - expanded.namespace_uri.size()) {
      throw std::length_error{"XSD identity QName byte limit exceeded"};
    }
    budget.work(expanded.namespace_uri.size() + expanded.local.size());
    const auto prefix_bytes = std::to_string(expanded.namespace_uri.size());
    budget.allocate(prefix_bytes.size() + 1U + expanded.namespace_uri.size() + expanded.local.size());
    return prefix_bytes + ':' + expanded.namespace_uri + expanded.local;
  }
  return normalized;
}

// Read one field projection, rejecting multiple matches and element-valued complex content.
std::optional<std::string> read_field(const element& selected, const identity_field& field,
    identity_budget& budget, operation_context* context, const identity_limits& limits) {
  const auto nodes = select_elements(selected, field.path, budget, limits);
  if (!field.path.attribute && nodes.size() > 1U) {
    throw std::invalid_argument{"XSD identity field selects multiple elements"};
  }
  std::optional<std::string> result{};
  const auto set = [&](std::string_view text, const element& scope) {
    if (result) { throw std::invalid_argument{"XSD identity field selects multiple values"}; }
    result = field_key(text, field, scope, budget, context, limits);
  };
  for (const auto* node : nodes) {
    if (field.path.attribute) {
      for (const auto& attribute : node->attributes) {
        if (budget.equal(attribute.qualified_name, *field.path.attribute)) { set(attribute.value, *node); }
      }
    } else {
      bool nil{};
      for (const auto& attribute : node->attributes) {
        budget.work();
        if (attribute.qualified_name.local == "nil" && attribute.qualified_name.namespace_uri == xsi_namespace) {
          if (attribute.value == "true" || attribute.value == "1") { nil = true; }
          else if (attribute.value != "false" && attribute.value != "0") {
            throw std::invalid_argument{"Invalid xsi:nil in XSD identity field"};
          }
        }
      }
      if (nil) {
        if (!node->text.empty() || !node->children.empty()) {
          throw std::invalid_argument{"Nil XSD identity field has content"};
        }
        continue;
      }
      if (!node->children.empty()) { throw std::invalid_argument{"XSD identity field requires simple scalar content"}; }
      set(node->text, *node);
    }
  }
  return result;
}

// Validate one expanded identity before any temporary owned copy of its strings is constructed.
void check_identity_name(const name& value, const identity_limits& limits, identity_budget* budget) {
  if (value.local.size() > limits.max_xpath_bytes ||
      value.namespace_uri.size() > limits.max_xpath_bytes - value.local.size()) {
    throw std::length_error{"XSD identity expanded-name byte limit exceeded"};
  }
  if (budget) {
    budget->work(1U + value.local.size() + value.namespace_uri.size());
    budget->allocate(value.local.size());
  }
  element empty{};
  (void)resolve_qname(value.local, empty);
  if (value.local.find(':') != std::string::npos) { throw std::invalid_argument{"Identity expanded local name contains a colon"}; }
}

// Check raw compiled paths too, since public IR can be constructed without the XPath parser.
void check_path(const identity_path& path, bool selector, const identity_limits& limits,
    identity_budget* budget = nullptr) {
  if (path.steps.size() > limits.max_path_steps || path.steps.size() > limits.max_depth) {
    throw std::length_error{"XSD identity path-step limit exceeded"};
  }
  if ((selector && path.attribute) || (!selector && path.descendants) ||
      (path.descendants && path.steps.empty())) {
    throw std::invalid_argument{"Unsupported compiled XSD identity XPath projection"};
  }
  for (const auto& step : path.steps) { check_identity_name(step, limits, budget); }
  if (path.attribute) { check_identity_name(*path.attribute, limits, budget); }
}
} // namespace

// Reject everything outside the explicit location-path grammar before resolving its namespace names.
identity_path compile_identity_path(std::string_view expression, const element& scope, bool selector,
    const identity_limits& limits) {
  if (expression.size() > limits.max_xpath_bytes) { throw std::length_error{"XSD identity XPath byte limit exceeded"}; }
  identity_path result{};
  if (expression == ".") { return result; }
  if (selector && expression.starts_with(".//")) { result.descendants = true; expression.remove_prefix(3U); }
  else if (expression.starts_with("./")) { expression.remove_prefix(2U); }
  if (expression.empty() || expression.find_first_of(" \t\r\n[]()|*=" ) != std::string_view::npos ||
      expression.find("::") != std::string_view::npos || expression.find("//") != std::string_view::npos) {
    throw std::invalid_argument{"Unsupported XSD identity XPath; expected a strict QName location path"};
  }
  while (!expression.empty()) {
    const auto slash = expression.find('/');
    auto token = expression.substr(0U, slash);
    if (token.empty() || token == "." || token == "..") { throw std::invalid_argument{"Unsupported XSD identity XPath step"}; }
    if (token.front() == '@') {
      if (selector || slash != std::string_view::npos) {
        throw std::invalid_argument{"XSD identity attributes are supported only as terminal field steps"};
      }
      token.remove_prefix(1U);
      result.attribute = path_name(token, scope);
    } else { result.steps.push_back(path_name(token, scope)); }
    if (result.steps.size() > limits.max_path_steps) { throw std::length_error{"XSD identity XPath step limit exceeded"}; }
    if (slash == std::string_view::npos) { break; }
    expression.remove_prefix(slash + 1U);
    if (expression.empty()) { throw std::invalid_argument{"Trailing slash in XSD identity XPath"}; }
  }
  check_path(result, selector, limits);
  return result;
}

namespace {
// Reuse one cumulative work ledger across static checking and document tuple validation.
void check_scope_contract(const identity_scope& scope, const identity_limits& limits,
    identity_budget& budget) {
  if (scope.constraints.size() > limits.max_constraints) { throw std::length_error{"XSD identity constraint limit exceeded"}; }
  check_identity_name(scope.owner, limits, &budget);
  std::map<std::pair<std::string, std::string>, const identity_constraint*> names{};
  for (const auto& constraint : scope.constraints) {
    budget.work();
    check_identity_name(constraint.id, limits, &budget);
    budget.allocate(constraint.id.local.size() + constraint.id.namespace_uri.size() +
        sizeof(std::pair<std::string, std::string>) + sizeof(const identity_constraint*));
    if (constraint.id.local.empty() || !names.emplace(std::pair{constraint.id.namespace_uri, constraint.id.local}, &constraint).second) {
      throw std::invalid_argument{"Duplicate or empty XSD identity constraint name"};
    }
    check_path(constraint.selector, true, limits, &budget);
    if (constraint.fields.empty()) { throw std::invalid_argument{"XSD identity requires at least one field"}; }
    if (constraint.fields.size() > limits.max_fields) { throw std::length_error{"XSD identity field-count limit exceeded"}; }
    for (const auto& field : constraint.fields) {
      check_path(field.path, false, limits, &budget);
      if (field.lexical_kind.size() > limits.max_xpath_bytes) { throw std::length_error{"XSD identity type-name byte limit exceeded"}; }
      budget.work(field.lexical_kind.size());
      (void)family(field.lexical_kind);
    }
    if ((constraint.kind == identity_kind::keyref) != constraint.refer.has_value()) {
      throw std::invalid_argument{"Only XSD keyref declarations require a refer target"};
    }
    if (constraint.refer) { check_identity_name(*constraint.refer, limits, &budget); }
  }
  for (const auto& constraint : scope.constraints) {
    if (!constraint.refer) { continue; }
    const auto found = names.find({constraint.refer->namespace_uri, constraint.refer->local});
    if (found == names.end() || found->second->kind == identity_kind::keyref) {
      throw std::invalid_argument{"XSD keyref target is missing or outside this owner scope"};
    }
    if (constraint.fields.size() != found->second->fields.size()) { throw std::invalid_argument{"XSD keyref tuple arity mismatch"}; }
    for (std::size_t index{}; index < constraint.fields.size(); ++index) {
      budget.work();
      if (family(constraint.fields[index].lexical_kind) != family(found->second->fields[index].lexical_kind)) {
        throw std::invalid_argument{"XSD keyref field value-space mismatch"};
      }
    }
  }
}
} // namespace

// Resolve references by expanded identity in the same owner contract; never search another scope.
void validate_identity_scope_contract(const identity_scope& scope, const identity_limits& limits,
    operation_context* context) {
  identity_budget budget{limits, context};
  check_scope_contract(scope, limits, budget);
}

// Build local key tables first, then resolve every complete reference before allowing success.
void validate_identity_scope(const element& document, const identity_scope& scope,
    const identity_limits& limits, operation_context* context) {
  identity_budget budget{limits, context};
  check_scope_contract(scope, limits, budget);
  if (!budget.equal(document.qualified_name, scope.owner)) { throw std::invalid_argument{"XSD identity owner scope mismatch"}; }
  using tuple = std::vector<std::string>;
  struct tuple_less {
    identity_budget* budget{};
    // Charge the maximum compared prefix bytes before each ordered-index comparison.
    bool operator()(const tuple& left, const tuple& right) const {
      const auto count = std::min(left.size(), right.size());
      for (std::size_t index{}; index < count; ++index) {
        budget->work();
        budget->work(std::min(left[index].size(), right[index].size()));
        const auto order = left[index].compare(right[index]);
        if (order != 0) { return order < 0; }
      }
      return left.size() < right.size();
    }
  };
  using table = std::set<tuple, tuple_less>;
  std::vector<table> tables{};
  budget.allocate(scope.constraints.size() * sizeof(table));
  tables.reserve(scope.constraints.size());
  for (std::size_t index{}; index < scope.constraints.size(); ++index) { tables.emplace_back(tuple_less{&budget}); }
  for (std::size_t index{}; index < scope.constraints.size(); ++index) {
    const auto& constraint = scope.constraints[index];
    const auto selected = select_elements(document, constraint.selector, budget, limits);
    for (const auto* item : selected) {
      tuple candidate{};
      std::size_t candidate_bytes{};
      bool complete{true};
      for (const auto& field : constraint.fields) {
        auto value = read_field(*item, field, budget, context, limits);
        if (value) {
          if (value->size() > limits.max_tuple_bytes - candidate_bytes) {
            throw std::length_error{"XSD identity candidate tuple-byte limit exceeded"};
          }
          candidate_bytes += value->size();
          budget.allocate(sizeof(std::string));
          candidate.push_back(std::move(*value));
        }
        else { complete = false; }
      }
      if (!complete) {
        if (constraint.kind == identity_kind::key) { throw std::invalid_argument{"XSD key tuple is incomplete or nil"}; }
        continue;
      }
      budget.tuple(candidate);
      budget.work();
      // Canonical tuple ordering is bounded by aggregate retained bytes as well as tuple count.
      if (!tables[index].insert(std::move(candidate)).second && constraint.kind != identity_kind::keyref) {
        throw std::invalid_argument{"Duplicate XSD key/unique tuple"};
      }
    }
  }
  for (std::size_t index{}; index < scope.constraints.size(); ++index) {
    const auto& constraint = scope.constraints[index];
    if (!constraint.refer) { continue; }
    const auto target = std::find_if(scope.constraints.begin(), scope.constraints.end(), [&](const auto& value) {
      return budget.equal(value.id, *constraint.refer);
    });
    const auto target_index = static_cast<std::size_t>(target - scope.constraints.begin());
    for (const auto& value : tables[index]) {
      budget.work();
      if (!tables[target_index].contains(value)) { throw std::invalid_argument{"Unresolved XSD keyref tuple in owner scope"}; }
    }
  }
}
} // namespace rohit::serializer::xml
