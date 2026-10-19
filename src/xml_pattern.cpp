// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml_pattern.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace rohit::serializer::xml {
namespace {
constexpr std::uint32_t continuation_mask{0x3fU};
constexpr std::uint32_t maximum_scalar{0x10ffffU};

// Decode exactly one shortest-form XML 1.0 Unicode scalar, rejecting malformed input.
std::uint32_t next_scalar(std::string_view text, std::size_t& offset) {
  if (offset >= text.size()) { throw std::invalid_argument{"Incomplete portable XML regex scalar"}; }
  const auto first = static_cast<unsigned char>(text[offset++]);
  std::uint32_t value{first};
  std::size_t continuation{};
  std::uint32_t minimum{};
  if (first >= 0xc2U && first <= 0xdfU) { value = first & 0x1fU; continuation = 1U; minimum = 0x80U; }
  else if (first >= 0xe0U && first <= 0xefU) { value = first & 0x0fU; continuation = 2U; minimum = 0x800U; }
  else if (first >= 0xf0U && first <= 0xf4U) { value = first & 0x07U; continuation = 3U; minimum = 0x10000U; }
  else if (first >= 0x80U) { throw std::invalid_argument{"Invalid UTF-8 in portable XML regex"}; }
  if (continuation > text.size() - offset) { throw std::invalid_argument{"Truncated portable XML regex UTF-8"}; }
  for (std::size_t index{}; index < continuation; ++index) {
    const auto byte = static_cast<unsigned char>(text[offset++]);
    if ((byte & 0xc0U) != 0x80U) { throw std::invalid_argument{"Invalid portable XML regex UTF-8 continuation"}; }
    value = (value << 6U) | (byte & continuation_mask);
  }
  const bool xml_character = value == 0x09U || value == 0x0aU || value == 0x0dU ||
      (value >= 0x20U && value <= 0xd7ffU) || (value >= 0xe000U && value <= 0xfffdU) ||
      (value >= 0x10000U && value <= maximum_scalar);
  if (value < minimum || !xml_character) { throw std::invalid_argument{"Invalid XML Unicode scalar in portable regex"}; }
  return value;
}

// Apply both a local deterministic work bound and the caller's shared cancellation/budget policy.
class work_budget {
  std::size_t remaining_{};
  operation_context* context_{};
public:
  // Borrow the operation context for this synchronous compilation or match.
  work_budget(std::size_t maximum, operation_context* context) : remaining_{maximum}, context_{context} {}
  // Charge before execution; overflow cannot bypass the local bound.
  void consume(std::size_t amount = 1U) {
    if (amount > remaining_) { throw std::length_error{"Portable XML regex work limit exceeded"}; }
    remaining_ -= amount;
    if (context_) { context_->consume(operation_resource::work_units, amount); }
  }
  // Charge storage before retaining it without claiming allocator-capacity accounting.
  void allocate(std::size_t amount) const {
    if (context_) { context_->consume(operation_resource::allocation_bytes, amount); }
  }
};

enum class expression_kind { empty, character, sequence, alternative, repeat };
struct expression_node {
  expression_kind kind{expression_kind::empty};
  std::vector<std::size_t> children{};
  std::vector<pattern_range> ranges{};
  std::size_t minimum{};
  std::optional<std::size_t> maximum{};
  bool inverted{}, dot{};
};

// Parse only the documented subset rather than guessing translations into host dialects.
class pattern_parser {
  std::string_view text_{};
  std::size_t offset_{};
  const pattern_limits& limits_;
  work_budget& budget_;
  std::vector<expression_node> nodes_{};
  // Report a stable, byte-positioned unsupported or malformed-expression diagnostic.
  [[noreturn]] void fail(std::string_view detail) const {
    throw std::invalid_argument{"Unsupported portable XSD regex at byte " + std::to_string(offset_) +
        ": " + std::string{detail}};
  }
  // Retain one bounded syntax node before expansion into automaton states.
  std::size_t append(expression_node value) {
    budget_.consume();
    if (nodes_.size() >= limits_.max_states) { throw std::length_error{"Portable XML regex syntax limit exceeded"}; }
    budget_.allocate(sizeof(expression_node));
    nodes_.push_back(std::move(value));
    return nodes_.size() - 1U;
  }
  // Read an escaped singleton; multi-character and Unicode-category escapes are not portable here.
  std::uint32_t character(bool in_class) {
    if (offset_ >= text_.size()) { fail("missing character"); }
    if (text_[offset_] != '\\') { return next_scalar(text_, offset_); }
    ++offset_;
    if (offset_ == text_.size()) { fail("trailing escape"); }
    const auto value = text_[offset_++];
    if (value == 'n') { return '\n'; }
    if (value == 'r') { return '\r'; }
    if (value == 't') { return '\t'; }
    const auto escapes = in_class ? std::string_view{"\\|.-^?*+{}()[]"} : std::string_view{"\\|.-^?*+{}()[]"};
    if (escapes.find(value) == std::string_view::npos) { fail("only singleton punctuation and n/r/t escapes are supported"); }
    return static_cast<unsigned char>(value);
  }
  // Parse a range class without subtraction, nested classes or locale-dependent categories.
  std::size_t character_class() {
    ++offset_;
    expression_node value{};
    value.kind = expression_kind::character;
    if (offset_ < text_.size() && text_[offset_] == '^') { value.inverted = true; ++offset_; }
    while (offset_ < text_.size() && text_[offset_] != ']') {
      if (text_[offset_] == '[') { fail("class subtraction and nested classes are unsupported"); }
      const auto first = character(true);
      auto last = first;
      if (offset_ < text_.size() && text_[offset_] == '-' && offset_ + 1U < text_.size() && text_[offset_ + 1U] != ']') {
        ++offset_;
        if (text_[offset_] == '[') { fail("class subtraction is unsupported"); }
        last = character(true);
        if (last < first) { fail("descending character range"); }
      }
      budget_.consume();
      budget_.allocate(sizeof(pattern_range));
      value.ranges.push_back({first, last});
    }
    if (offset_ == text_.size() || value.ranges.empty()) { fail("unterminated or empty character class"); }
    ++offset_;
    return append(std::move(value));
  }
  // Parse one atom and its optional bounded or unbounded repetition.
  std::size_t piece(std::size_t depth) {
    std::size_t selected{};
    if (text_[offset_] == '(') {
      ++offset_;
      if (offset_ < text_.size() && text_[offset_] == '?') { fail("lookaround and special groups are unsupported"); }
      selected = expression(depth + 1U);
      if (offset_ == text_.size() || text_[offset_] != ')') { fail("unclosed group"); }
      ++offset_;
    } else if (text_[offset_] == '[') { selected = character_class(); }
    else {
      expression_node value{};
      value.kind = expression_kind::character;
      if (text_[offset_] == '.') { value.dot = true; ++offset_; }
      else {
        if (std::string_view{"?*+{}]"}.find(text_[offset_]) != std::string_view::npos) { fail("quantifier or delimiter without atom"); }
        const auto scalar = character(false);
        budget_.allocate(sizeof(pattern_range));
        value.ranges.push_back({scalar, scalar});
      }
      selected = append(std::move(value));
    }
    if (offset_ == text_.size()) { return selected; }
    const auto token = text_[offset_];
    if (std::string_view{"?*+{"}.find(token) == std::string_view::npos) { return selected; }
    expression_node repeated{};
    repeated.kind = expression_kind::repeat;
    budget_.allocate(sizeof(std::size_t));
    repeated.children.push_back(selected);
    if (token == '?') { repeated.maximum = 1U; ++offset_; }
    else if (token == '*') { ++offset_; }
    else if (token == '+') { repeated.minimum = 1U; ++offset_; }
    else if (token == '{') {
      ++offset_;
      repeated.minimum = number();
      repeated.maximum = repeated.minimum;
      if (offset_ < text_.size() && text_[offset_] == ',') {
        ++offset_;
        repeated.maximum = offset_ < text_.size() && text_[offset_] == '}' ?
            std::optional<std::size_t>{} : std::optional<std::size_t>{number()};
      }
      if (offset_ == text_.size() || text_[offset_] != '}') { fail("unclosed repetition"); }
      ++offset_;
      if (repeated.maximum && *repeated.maximum < repeated.minimum) { fail("descending repetition interval"); }
    }
    return append(std::move(repeated));
  }
  // Parse a checked repeat count without integer overflow or expansion surprises.
  std::size_t number() {
    if (offset_ == text_.size() || text_[offset_] < '0' || text_[offset_] > '9') { fail("missing repetition count"); }
    std::size_t result{};
    while (offset_ < text_.size() && text_[offset_] >= '0' && text_[offset_] <= '9') {
      const auto digit = static_cast<std::size_t>(text_[offset_++] - '0');
      if (result > limits_.max_repetitions / 10U ||
          result * 10U > limits_.max_repetitions || digit > limits_.max_repetitions - result * 10U) {
        throw std::length_error{"Portable XML regex repetition limit exceeded"};
      }
      result = result * 10U + digit;
    }
    return result;
  }
  // Parse concatenation and alternatives while keeping recursive group depth bounded.
  std::size_t expression(std::size_t depth) {
    if (depth > limits_.max_nesting_depth) { throw std::length_error{"Portable XML regex nesting limit exceeded"}; }
    expression_node alternatives{};
    alternatives.kind = expression_kind::alternative;
    do {
      expression_node sequence{};
      sequence.kind = expression_kind::sequence;
      while (offset_ < text_.size() && text_[offset_] != '|' && text_[offset_] != ')') {
        const auto child = piece(depth);
        budget_.allocate(sizeof(std::size_t));
        sequence.children.push_back(child);
      }
      const auto branch = append(std::move(sequence));
      budget_.allocate(sizeof(std::size_t));
      alternatives.children.push_back(branch);
      if (offset_ == text_.size() || text_[offset_] != '|') { break; }
      ++offset_;
    } while (true);
    return append(std::move(alternatives));
  }
public:
  // Borrow source and bounds only during compilation.
  pattern_parser(std::string_view text, const pattern_limits& limits, work_budget& budget)
      : text_{text}, limits_{limits}, budget_{budget} {}
  // Return the complete syntax inventory and root; trailing unmatched delimiters reject.
  std::pair<std::vector<expression_node>, std::size_t> parse() {
    const auto root = expression(0U);
    if (offset_ != text_.size()) { fail("unmatched closing group"); }
    return {std::move(nodes_), root};
  }
};

// Expand only bounded repetitions into a Thompson automaton; matching never backtracks.
class program_builder {
  const std::vector<expression_node>& nodes_;
  const pattern_limits& limits_;
  work_budget& budget_;
  std::vector<pattern_state> states_{};
  // Allocate and charge one complete instruction before linking any edges to it.
  std::size_t state(pattern_state value = {}) {
    budget_.consume(1U + value.ranges.size() + value.epsilon.size());
    if (states_.size() >= limits_.max_states) { throw std::length_error{"Portable XML regex state limit exceeded"}; }
    budget_.allocate(sizeof(pattern_state));
    states_.push_back(std::move(value));
    return states_.size() - 1U;
  }
  // Construct one fragment ending at a supplied successor without retaining AST pointers.
  std::size_t compile(std::size_t selected, std::size_t next, std::size_t depth) {
    budget_.consume();
    if (depth > nodes_.size()) {
      throw std::length_error{"Portable XML regex expansion depth exceeded"};
    }
    const auto& node = nodes_[selected];
    if (node.kind == expression_kind::character) {
      pattern_state value{};
      budget_.allocate(node.ranges.size() * sizeof(pattern_range));
      value.ranges = node.ranges;
      value.next = next;
      value.consumes = true;
      value.inverted = node.inverted;
      value.dot = node.dot;
      return state(std::move(value));
    }
    if (node.kind == expression_kind::sequence) {
      for (auto item = node.children.rbegin(); item != node.children.rend(); ++item) {
        next = compile(*item, next, depth + 1U);
      }
      return next;
    }
    if (node.kind == expression_kind::alternative) {
      pattern_state split{};
      for (const auto child : node.children) {
        const auto branch = compile(child, next, depth + 1U);
        budget_.allocate(sizeof(std::size_t));
        split.epsilon.push_back(branch);
      }
      return state(std::move(split));
    }
    if (node.kind == expression_kind::repeat) {
      if (!node.maximum) {
        const auto split = state();
        const auto body = compile(node.children.front(), split, depth + 1U);
        budget_.allocate(2U * sizeof(std::size_t));
        states_[split].epsilon = {next, body};
        next = split;
      } else {
        for (auto count = node.minimum; count < *node.maximum; ++count) {
          const auto body = compile(node.children.front(), next, depth + 1U);
          pattern_state split{};
          budget_.allocate(2U * sizeof(std::size_t));
          split.epsilon = {next, body};
          next = state(std::move(split));
        }
      }
      for (std::size_t count{}; count < node.minimum; ++count) {
        next = compile(node.children.front(), next, depth + 1U);
      }
    }
    return next;
  }
public:
  // Borrow the parsed inventory while producing a standalone portable program.
  program_builder(const std::vector<expression_node>& nodes, const pattern_limits& limits,
      work_budget& budget) : nodes_{nodes}, limits_{limits}, budget_{budget} {}
  // Finish with exactly one accepting state and return independently owned instructions.
  std::pair<std::vector<pattern_state>, std::size_t> build(std::size_t root) {
    pattern_state accepting{};
    accepting.accepting = true;
    const auto end = state(std::move(accepting));
    const auto start = compile(root, end, 0U);
    return {std::move(states_), start};
  }
};
} // namespace

// Move prepared instructions behind shared immutable ownership.
portable_pattern::portable_pattern(std::vector<pattern_state> states, std::size_t start)
    : states_{std::make_shared<const std::vector<pattern_state>>(std::move(states))}, start_{start} {}

// Borrow read-only instructions for the lifetime of this program or another copy.
std::span<const pattern_state> portable_pattern::states() const noexcept { return *states_; }

// Preserve the compiled initial state without provider-specific handles.
std::size_t portable_pattern::start() const noexcept { return start_; }

// Validate the complete string before returning a match result, even after the active set empties.
bool portable_pattern::matches(std::string_view text, const pattern_limits& limits,
    operation_context* context) const {
  if (text.size() > limits.max_input_bytes || states_->size() > limits.max_states) {
    throw std::length_error{"Portable XML regex input/state limit exceeded"};
  }
  work_budget budget{limits.max_work_units, context};
  // The compiler emits at most two epsilon edges per state on average; closure's pending
  // edges plus its two active inventories therefore fit four state-index arrays.
  budget.allocate(states_->size() * (sizeof(std::size_t) * 4U + sizeof(bool)));
  std::vector<std::size_t> active{}, next{}, stack{};
  std::vector<bool> visited(states_->size(), false);
  // One closure visits each state once, including epsilon loops from nullable repeated groups.
  const auto closure = [&](std::vector<std::size_t>& selected) {
    budget.consume(visited.size());
    std::fill(visited.begin(), visited.end(), false);
    stack.swap(selected);
    selected.clear();
    while (!stack.empty()) {
      budget.consume();
      const auto index = stack.back();
      stack.pop_back();
      if (visited[index]) { continue; }
      visited[index] = true;
      const auto& instruction = (*states_)[index];
      if (instruction.consumes || instruction.accepting) { selected.push_back(index); }
      for (const auto successor : instruction.epsilon) { stack.push_back(successor); }
    }
  };
  active.push_back(start_);
  closure(active);
  std::size_t offset{};
  while (offset < text.size()) {
    budget.consume();
    const auto scalar = next_scalar(text, offset);
    next.clear();
    for (const auto index : active) {
      budget.consume();
      const auto& instruction = (*states_)[index];
      if (!instruction.consumes) { continue; }
      bool matched{};
      if (instruction.dot) { matched = scalar != '\n' && scalar != '\r'; }
      else {
        for (const auto& range : instruction.ranges) {
          budget.consume();
          matched = matched || (scalar >= range.first && scalar <= range.last);
        }
        if (instruction.inverted) { matched = !matched; }
      }
      if (matched) { next.push_back(instruction.next); }
    }
    closure(next);
    active.swap(next);
  }
  return std::any_of(active.begin(), active.end(), [&](const auto index) { return (*states_)[index].accepting; });
}

// Compile the explicit portable grammar without loading or invoking the optional XML provider.
portable_pattern compile_portable_pattern(std::string_view expression, const pattern_limits& limits,
    operation_context* context) {
  if (expression.size() > limits.max_expression_bytes) {
    throw std::length_error{"Portable XML regex expression byte limit exceeded"};
  }
  work_budget budget{limits.max_work_units, context};
  auto [nodes, root] = pattern_parser{expression, limits, budget}.parse();
  auto [states, start] = program_builder{nodes, limits, budget}.build(root);
  budget.allocate(sizeof(std::vector<pattern_state>));
  return portable_pattern{std::move(states), start};
}
} // namespace rohit::serializer::xml
