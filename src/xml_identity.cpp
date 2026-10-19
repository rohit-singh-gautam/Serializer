// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml_identity.hpp>
#include <rohit/xml_events.hpp>

#include <algorithm>
#include <list>
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
    identity_budget& budget, operation_context* context, const identity_limits& limits, bool key) {
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
      if (key && field.nillable) {
        throw std::invalid_argument{"XSD key field selects an element declared nillable"};
      }
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
      if (field.nillable && field.path.attribute) {
        throw std::invalid_argument{"XSD identity attributes cannot be declared nillable"};
      }
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
        auto value = read_field(*item, field, budget, context, limits, constraint.kind == identity_kind::key);
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

// Retain scalar captures only while their selected field is open; siblings become tuple indexes.
struct identity_validator::implementation {
  using tuple = std::vector<std::string>;
  struct tuple_less {
    identity_budget* budget{};
    // Bound ordered-index comparisons by the canonical bytes actually examined.
    bool operator()(const tuple& left, const tuple& right) const {
      for (std::size_t index{}; index < std::min(left.size(), right.size()); ++index) {
        budget->work(1U + std::min(left[index].size(), right[index].size()));
        const auto order = left[index].compare(right[index]);
        if (order) { return order < 0; }
      }
      return left.size() < right.size();
    }
  };
  using table = std::set<tuple, tuple_less>;
  struct capture {
    std::size_t depth{};
    std::string text{};
    std::map<std::string, std::string> namespaces{};
    bool nil{};
  };
  struct candidate {
    std::size_t constraint{}, depth{}, lexical_bytes{}, canonical_bytes{};
    std::vector<std::optional<std::string>> values{};
    std::vector<bool> seen{};
    std::vector<std::optional<capture>> captures{};
  };
  struct namespace_undo {
    std::string prefix{};
    std::optional<std::string> previous{};
  };
  struct frame {
    name qualified_name{};
    std::vector<namespace_undo> namespaces{};
  };
  const identity_scope& scope;
  identity_limits bounds;
  operation_context* context{};
  identity_budget budget;
  std::vector<table> tables{};
  std::vector<frame> path{};
  std::map<std::string, std::string> namespaces{};
  std::list<candidate> active{};
  std::size_t selected_nodes{};
  bool started{}, root_seen{}, completed{}, failed{};

  // Validate immutable IR once and initialize one bounded ordered table per declaration.
  implementation(const identity_scope& selected, identity_limits limits, operation_context* operation,
      const std::map<std::string, std::string>& inherited)
      : scope{selected}, bounds{limits}, context{operation}, budget{bounds, operation} {
    check_scope_contract(scope, bounds, budget);
    budget.allocate(scope.constraints.size() * sizeof(table));
    tables.reserve(scope.constraints.size());
    for (std::size_t index{}; index < scope.constraints.size(); ++index) {
      tables.emplace_back(tuple_less{&budget});
    }
    for (const auto& [prefix, uri] : inherited) {
      budget.work(prefix.size() + uri.size());
      budget.allocate(prefix.size() + uri.size());
      namespaces.emplace(prefix, uri);
    }
  }

  // Compare borrowed expanded names without an uncharged temporary string allocation.
  bool equal(event_name actual, const name& expected) {
    budget.work(1U + std::min(actual.local.size(), expected.local.size()) +
        std::min(actual.namespace_uri.size(), expected.namespace_uri.size()));
    return actual.local == expected.local && actual.namespace_uri == expected.namespace_uri;
  }

  // Match a field relative to one selected node, or a selector relative to the owner root.
  bool matches(const identity_path& expected, std::size_t owner_depth) {
    const auto relative = path.size() - owner_depth;
    if ((!expected.descendants && relative != expected.steps.size()) ||
        (expected.descendants && relative < expected.steps.size())) { return false; }
    const auto begin = path.size() - expected.steps.size();
    for (std::size_t index{}; index < expected.steps.size(); ++index) {
      if (!budget.equal(path[begin + index].qualified_name, expected.steps[index])) { return false; }
    }
    return true;
  }

  // Admit both lexical intermediate bytes and the final canonical tuple before retaining them.
  void admit_field(candidate& selected, std::size_t bytes, bool canonical) {
    auto& total = canonical ? selected.canonical_bytes : selected.lexical_bytes;
    if (bytes > bounds.max_tuple_bytes - total) {
      throw std::length_error{"XSD identity candidate tuple-byte limit exceeded"};
    }
    total += bytes;
    budget.allocate(bytes);
  }

  // Canonicalize with the field's own namespace environment, independently of later redeclarations.
  void set_field(candidate& selected, std::size_t index, std::string_view lexical,
      const std::map<std::string, std::string>& bindings) {
    element scalar_scope{};
    if (family(scope.constraints[selected.constraint].fields[index].lexical_kind) == value_family::qname) {
      for (const auto& [prefix, uri] : bindings) {
        budget.work(prefix.size() + uri.size());
        budget.allocate(prefix.size() + uri.size());
        scalar_scope.namespace_bindings.emplace(prefix, uri);
      }
    }
    auto canonical = field_key(lexical, scope.constraints[selected.constraint].fields[index],
        scalar_scope, budget, context, bounds);
    admit_field(selected, canonical.size(), true);
    selected.values[index] = std::move(canonical);
  }

  // Open one checked element, discover matching selections, and begin only matching scalar captures.
  void start_element(const event_view& value) {
    if (!started || (path.empty() && root_seen)) { throw std::invalid_argument{"Invalid XSD identity document state"}; }
    if (path.size() >= bounds.max_depth) { throw std::length_error{"XSD identity document depth exceeded"}; }
    for (const auto& selected : active) {
      for (const auto& field : selected.captures) {
        budget.work();
        if (field) { throw std::invalid_argument{"XSD identity field requires simple scalar content"}; }
      }
    }
    if (path.empty() && !equal(value.qualified_name, scope.owner)) {
      throw std::invalid_argument{"XSD identity owner scope mismatch"};
    }
    budget.allocate(sizeof(frame) + value.qualified_name.local.size() + value.qualified_name.namespace_uri.size());
    frame current{{std::string{value.qualified_name.local}, std::string{value.qualified_name.namespace_uri}}, {}};
    for (const auto& declaration : value.namespaces) {
      budget.work(declaration.prefix.size() + declaration.namespace_uri.size());
      budget.allocate(sizeof(namespace_undo) + declaration.prefix.size() + declaration.namespace_uri.size());
      namespace_undo undo{std::string{declaration.prefix}, {}};
      if (const auto previous = namespaces.find(undo.prefix); previous != namespaces.end()) {
        budget.allocate(previous->second.size());
        undo.previous = previous->second;
      }
      namespaces[undo.prefix] = declaration.namespace_uri;
      current.namespaces.push_back(std::move(undo));
    }
    path.push_back(std::move(current));
    root_seen = true;
    for (std::size_t index{}; index < scope.constraints.size(); ++index) {
      budget.work();
      const auto& constraint = scope.constraints[index];
      if (!matches(constraint.selector, 1U)) { continue; }
      budget.select(selected_nodes);
      ++selected_nodes;
      budget.allocate(sizeof(candidate) + constraint.fields.size() *
          (sizeof(std::optional<std::string>) + sizeof(bool) + sizeof(std::optional<capture>)));
      candidate selected{};
      selected.constraint = index; selected.depth = path.size();
      selected.values.resize(constraint.fields.size());
      selected.seen.resize(constraint.fields.size());
      selected.captures.resize(constraint.fields.size());
      active.push_back(std::move(selected));
    }
    for (auto& selected : active) {
      const auto& constraint = scope.constraints[selected.constraint];
      for (std::size_t index{}; index < constraint.fields.size(); ++index) {
        budget.work();
        const auto& field = constraint.fields[index];
        if (!matches(field.path, selected.depth)) { continue; }
        if (field.path.attribute) {
          for (const auto& attribute : value.attributes) {
            if (!equal(attribute.qualified_name, *field.path.attribute)) { continue; }
            if (selected.seen[index]) { throw std::invalid_argument{"XSD identity field selects multiple values"}; }
            selected.seen[index] = true;
            admit_field(selected, attribute.value.size(), false);
            set_field(selected, index, attribute.value, namespaces);
          }
        } else {
          if (constraint.kind == identity_kind::key && field.nillable) {
            throw std::invalid_argument{"XSD key field selects an element declared nillable"};
          }
          if (selected.seen[index]) { throw std::invalid_argument{"XSD identity field selects multiple elements"}; }
          selected.seen[index] = true;
          capture scalar{}; scalar.depth = path.size();
          for (const auto& attribute : value.attributes) {
            budget.work();
            if (attribute.qualified_name.local != "nil" || attribute.qualified_name.namespace_uri != xsi_namespace) { continue; }
            budget.work(attribute.value.size()); budget.allocate(attribute.value.size());
            const auto nil = normalize_whitespace(attribute.value, whitespace_policy::collapse);
            if (nil == "true" || nil == "1") { scalar.nil = true; }
            else if (nil != "false" && nil != "0") { throw std::invalid_argument{"Invalid xsi:nil in XSD identity field"}; }
          }
          if (family(field.lexical_kind) == value_family::qname) {
            for (const auto& [prefix, uri] : namespaces) {
              budget.work(prefix.size() + uri.size()); budget.allocate(prefix.size() + uri.size());
              scalar.namespaces.emplace(prefix, uri);
            }
          }
          selected.captures[index] = std::move(scalar);
        }
      }
    }
  }

  // Store text only for currently selected scalar fields, charging each retained copy.
  void text(std::string_view value) {
    for (auto& selected : active) {
      for (auto& scalar : selected.captures) {
        budget.work();
        if (!scalar) { continue; }
        if (scalar->nil && !value.empty()) { throw std::invalid_argument{"Nil XSD identity field has content"}; }
        admit_field(selected, value.size(), false);
        budget.work(value.size());
        scalar->text.append(value);
      }
    }
  }

  // Finish captures and selected tuples at their exact closing depth, then restore namespaces.
  void end_element(const event_view& value) {
    if (path.empty() || !equal(value.qualified_name, path.back().qualified_name)) {
      throw std::invalid_argument{"Unbalanced XSD identity element"};
    }
    for (auto selected = active.begin(); selected != active.end();) {
      budget.work();
      for (std::size_t index{}; index < selected->captures.size(); ++index) {
        budget.work();
        auto& scalar = selected->captures[index];
        if (scalar && scalar->depth == path.size()) {
          if (!scalar->nil) { set_field(*selected, index, scalar->text, scalar->namespaces); }
          scalar.reset();
        }
      }
      if (selected->depth != path.size()) { ++selected; continue; }
      const auto& constraint = scope.constraints[selected->constraint];
      const bool complete_tuple = std::all_of(selected->values.begin(), selected->values.end(),
          [](const auto& field) { return field.has_value(); });
      if (!complete_tuple && constraint.kind == identity_kind::key) {
        throw std::invalid_argument{"XSD key tuple is incomplete or nil"};
      }
      if (complete_tuple) {
        budget.allocate(selected->values.size() * sizeof(std::string));
        tuple result{};
        for (auto& field : selected->values) { result.push_back(std::move(*field)); }
        budget.tuple(result); budget.work();
        if (!tables[selected->constraint].insert(std::move(result)).second && constraint.kind != identity_kind::keyref) {
          throw std::invalid_argument{"Duplicate XSD key/unique tuple"};
        }
      }
      selected = active.erase(selected);
    }
    for (auto restore = path.back().namespaces.rbegin(); restore != path.back().namespaces.rend(); ++restore) {
      if (restore->previous) { namespaces[restore->prefix] = std::move(*restore->previous); }
      else { namespaces.erase(restore->prefix); }
    }
    path.pop_back();
  }

  // Resolve only this owner's key tables after every forward declaration has been observed.
  void end_document() {
    if (!started || !root_seen || !path.empty() || !active.empty()) {
      throw std::invalid_argument{"Incomplete XSD identity document"};
    }
    for (std::size_t index{}; index < scope.constraints.size(); ++index) {
      const auto& constraint = scope.constraints[index];
      if (!constraint.refer) { continue; }
      const auto target = std::find_if(scope.constraints.begin(), scope.constraints.end(), [&](const auto& other) {
        return budget.equal(other.id, *constraint.refer);
      });
      const auto target_index = static_cast<std::size_t>(target - scope.constraints.begin());
      for (const auto& value : tables[index]) {
        budget.work();
        if (!tables[target_index].contains(value)) { throw std::invalid_argument{"Unresolved XSD keyref tuple in owner scope"}; }
      }
    }
    completed = true;
  }
};

// Seed one event pass with immutable contract metadata and the inherited namespace environment.
identity_validator::identity_validator(const identity_scope& scope, identity_limits bounds,
    operation_context* context, const std::map<std::string, std::string>& namespaces)
    : state_{std::make_unique<implementation>(scope, bounds, context, namespaces)} {}
// Release all uncommitted captures and indexes.
identity_validator::~identity_validator() = default;
// Fence every structural, lexical, budget and user-callback failure permanently.
void identity_validator::accept(const event_view& value) {
  if (state_->failed || state_->completed) { throw std::logic_error{"XSD identity pass is closed or failed"}; }
  try {
    state_->budget.work();
    switch (value.kind) {
    case event_kind::start_document:
      if (state_->started) { throw std::invalid_argument{"Duplicate XSD identity start document"}; }
      state_->started = true; break;
    case event_kind::start_element: state_->start_element(value); break;
    case event_kind::text:
      if (!state_->started) { throw std::invalid_argument{"XSD identity text before start document"}; }
      state_->text(value.text); break;
    case event_kind::end_element: state_->end_element(value); break;
    case event_kind::end_document: state_->end_document(); break;
    }
  } catch (...) { state_->failed = true; throw; }
}
// A closed root alone never authorizes successful reference resolution or publication.
void identity_validator::finish() {
  if (state_->failed) { throw std::logic_error{"XSD identity pass failed"}; }
  if (!state_->completed) { state_->failed = true; throw std::invalid_argument{"Incomplete XSD identity event stream"}; }
}
// Expose complete local-scope validation independently of parser state.
bool identity_validator::complete() const noexcept { return state_->completed && !state_->failed; }
// A failed identity pass cannot be resumed or reused with a different budget.
bool identity_validator::failed() const noexcept { return state_->failed; }
} // namespace rohit::serializer::xml
