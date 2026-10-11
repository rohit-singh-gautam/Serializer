// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// This implementation is included after the schema scanner helpers, inside parser.
// Share the checked constraint grammar with readonly callable inputs.
generic_constraint parse_construction_constraint(const rohit::type_check::schema_input_buffer auto& input,
                                                namespace_node* space);

// Locate new API declarations in the original schema, independently of generated line wrapping.
std::size_t behavior_source_offset(const rohit::type_check::schema_input_buffer auto& input) {
  if (!active_behavior_source) { return 0; }
  const auto* origin = reinterpret_cast<const std::uint8_t*>(active_behavior_source->source.data());
  return static_cast<std::size_t>(input.curr() - origin);
}

// Bind a complete lexical span without depending on advisory-warning collection.
void locate_behavior(auto& node, std::size_t begin, std::size_t end) {
  node.source_offset = begin; node.source_end = end;
  if (!active_behavior_source) { return; }
  const auto& context = *active_behavior_source;
  const auto line = std::upper_bound(context.line_starts.begin(), context.line_starts.end(), begin);
  const auto index = static_cast<std::size_t>(line - context.line_starts.begin() - 1);
  node.source_line = index + 1; node.source_column = begin - context.line_starts[index] + 1;
  node.source_path = context.path.empty() ? "<schema>" : std::string{context.path};
}

// New behavior and runtime-field syntax is unavailable in an older declared contract.
void require_behavior_language(const rohit::type_check::schema_input_buffer auto& input) {
  if (active_language_version < language_version{1u, 6u, 0u}) {
    throw exception::bad_member_spec{input, "Behavior declarations require serializer version 1.6.0"};
  }
}

// Recognize only documented output tags, preserving ordinary user identifiers elsewhere.
bool is_behavior_language(std::string_view name) {
  constexpr std::array<std::string_view, 11> languages{"cpp", "java", "javascript", "typescript", "go",
      "csharp", "rust", "python", "swift", "kotlin", "c"};
  return std::find(languages.begin(), languages.end(), name) != languages.end();
}

// Bound native payloads and honor target quotes/comments instead of counting literal braces.
native_code_block parse_native_block(const rohit::type_check::schema_input_buffer auto& input,
                                     std::string language, access_type access) {
  constexpr std::size_t maximum_native_bytes = 1024 * 1024;
  constexpr std::size_t maximum_native_depth = 128;
  native_code_block result{std::move(language), {}, access, behavior_source_offset(input)};
  skip_whitespace_and_comment(input);
  check_and_increase(input, '{');
  const auto* begin = input.curr();
  const auto source_begin = behavior_source_offset(input);
  std::size_t depth{1};
  while (!input.full()) {
    if (static_cast<std::size_t>(input.curr() - begin) > maximum_native_bytes) {
      throw exception::bad_member_spec{input, "Native code exceeds the 1 MiB payload bound"};
    }
    const std::string_view remaining{reinterpret_cast<const char*>(input.curr()), input.remaining_buffer()};
    if (remaining.starts_with("//") || (result.language == "python" && *input == '#')) {
      while (!input.full() && *input != '\n') { ++input; }
      continue;
    }
    if (remaining.starts_with("/*")) {
      const auto end = remaining.find("*/", 2);
      if (end == std::string_view::npos) {
        throw exception::bad_member_spec{input, "Unterminated native block comment"};
      }
      input += end + 2;
      continue;
    }
    if (result.language == "cpp" && remaining.starts_with("R\"")) {
      const auto open = remaining.find('(', 2);
      if (open == std::string_view::npos || open > 18) {
        throw exception::bad_member_spec{input, "Malformed C++ raw string delimiter"};
      }
      const auto close = ")" + std::string{remaining.substr(2, open - 2)} + "\"";
      const auto end = remaining.find(close, open + 1);
      if (end == std::string_view::npos) {
        throw exception::bad_member_spec{input, "Unterminated C++ raw string"};
      }
      input += end + close.size();
      continue;
    }
    if (result.language == "rust" && remaining.starts_with("r")) {
      std::size_t hashes{};
      while (hashes + 1 < remaining.size() && remaining[hashes + 1] == '#') { ++hashes; }
      if (hashes <= 255 && hashes + 1 < remaining.size() && remaining[hashes + 1] == '"') {
        const auto closing = "\"" + std::string(hashes, '#');
        const auto end = remaining.find(closing, hashes + 2);
        if (end == std::string_view::npos) { throw exception::bad_member_spec{input, "Unterminated Rust raw string"}; }
        input += end + closing.size(); continue;
      }
    }
    if (result.language == "csharp" && (*input == '"' || *input == '\'')) {
      // Interpolation expressions may contain their own strings and nested interpolated strings.
      const auto scan_string = [&](const auto& self, std::size_t position, bool verbatim,
                                   bool interpolated, std::size_t nesting) -> std::size_t {
        if (nesting > maximum_native_depth) { throw exception::bad_member_spec{input, "Native string nesting exceeds 128"}; }
        const auto quote = remaining[position++];
        const auto content_begin = position;
        std::size_t count{1};
        while (position < remaining.size() && remaining[position] == quote) { ++position; ++count; }
        if (count >= 3) {
          const auto close = remaining.find(std::string(count, quote), position);
          if (close == std::string_view::npos) { throw exception::bad_member_spec{input, "Unterminated C# raw string"}; }
          return close + count;
        }
        position = content_begin;
        while (position < remaining.size()) {
          const auto token = remaining[position++];
          if (token == quote) {
            if (verbatim && position < remaining.size() && remaining[position] == quote) { ++position; continue; }
            return position;
          }
          if (!verbatim && token == '\\') { if (position < remaining.size()) { ++position; } continue; }
          if (interpolated && token == '{') {
            if (position < remaining.size() && remaining[position] == '{') { ++position; continue; }
            std::size_t interpolation_depth{1};
            while (position < remaining.size() && interpolation_depth) {
              const auto expression = remaining.substr(position);
              if (expression.starts_with("//")) {
                const auto end = remaining.find('\n', position + 2);
                position = end == std::string_view::npos ? remaining.size() : end + 1; continue;
              }
              if (expression.starts_with("/*")) {
                const auto end = remaining.find("*/", position + 2);
                if (end == std::string_view::npos) { throw exception::bad_member_spec{input, "Unterminated interpolation comment"}; }
                position = end + 2; continue;
              }
              if (remaining[position] == '"' || remaining[position] == '\'') {
                const bool nested_verbatim = position && (remaining[position - 1] == '@' ||
                    (remaining[position - 1] == '$' && position > 1 && remaining[position - 2] == '@'));
                const bool nested_interpolated = position && (remaining[position - 1] == '$' ||
                    (remaining[position - 1] == '@' && position > 1 && remaining[position - 2] == '$'));
                position = self(self, position, nested_verbatim, nested_interpolated, nesting + 1); continue;
              }
              if (remaining[position] == '{' && ++interpolation_depth > maximum_native_depth) {
                throw exception::bad_member_spec{input, "Interpolation nesting exceeds 128"};
              }
              if (remaining[position] == '}') { --interpolation_depth; }
              ++position;
            }
            if (interpolation_depth) { throw exception::bad_member_spec{input, "Unterminated interpolation expression"}; }
          }
        }
        throw exception::bad_member_spec{input, "Unterminated C# string"};
      };
      const bool verbatim = input.curr() != begin && (input.curr()[-1] == '@' ||
          (input.curr()[-1] == '$' && input.curr() - begin > 1 && input.curr()[-2] == '@'));
      const bool interpolated = input.curr() != begin && (input.curr()[-1] == '$' ||
          (input.curr()[-1] == '@' && input.curr() - begin > 1 && input.curr()[-2] == '$'));
      input += scan_string(scan_string, 0, verbatim, interpolated, 0); continue;
    }
    if (*input == '"' || *input == '\'' || *input == '`') {
      const auto quote = *input;
      std::size_t quote_count{1};
      if ((result.language == "csharp" || result.language == "python" || result.language == "kotlin" || result.language == "swift" || result.language == "java") && quote != '`') {
        while (quote_count < remaining.size() && remaining[quote_count] == quote) { ++quote_count; }
        if (quote_count < 3) { quote_count = 1; }
      }
      const bool verbatim = result.language == "csharp" && input.curr() != begin && input.curr()[-1] == '@';
      input += quote_count;
      bool complete{};
      while (!input.full()) {
        const std::string_view contents{reinterpret_cast<const char*>(input.curr()), input.remaining_buffer()};
        if (contents.starts_with(std::string(quote_count, quote))) {
          input += quote_count;
          if (verbatim && !input.full() && *input == quote) { ++input; continue; }
          complete = true;
          break;
        }
        if (!verbatim && quote_count == 1 && *input == '\\') {
          ++input;
          if (input.full()) { break; }
        }
        ++input;
      }
      if (!complete) {
        throw exception::bad_member_spec{input, "Unterminated native string or character literal"};
      }
      continue;
    }
    if (*input == '{') {
      if (++depth > maximum_native_depth) {
        throw exception::bad_member_spec{input, "Native code nesting exceeds 128 blocks"};
      }
    } else if (*input == '}' && --depth == 0) {
      result.text.assign(reinterpret_cast<const char*>(begin), static_cast<std::size_t>(input.curr() - begin));
      locate_behavior(result, source_begin, behavior_source_offset(input));
      ++input;
      return result;
    }
    ++input;
  }
  throw exception::bad_member_spec{input, "Unterminated native code block"};
}

// Parse bounded pure numeric expressions with explicit conversion and conventional precedence.
template <typename Input> struct behavior_expression_parser {
  const Input& input;
  std::size_t nodes{};
  std::size_t depth{};
  namespace_node* space{};
  // New operators retain their declaring file's language contract, including includes.
  void require_checked_language() {
    if (active_language_version < language_version{1u, 8u, 0u}) {
      throw exception::bad_member_spec{input, "Checked guards and math require serializer version 1.8.0"};
    }
  }
  // Callable composition belongs to the declaring file's independent language contract.
  void require_call_language() {
    if (active_language_version < language_version{1u, 10u, 0u}) {
      throw exception::bad_member_spec{input, "Readonly function calls require serializer version 1.10.0"};
    }
  }
  // Consume an exact operator without advancing on a mismatch.
  bool consume(std::string_view token) {
    skip_whitespace_and_comment(input);
    const std::string_view remaining{reinterpret_cast<const char*>(input.curr()), input.remaining_buffer()};
    if (!remaining.starts_with(token)) { return false; }
    input += token.size();
    return true;
  }
  // Allocate one checked expression node with an original source location.
  behavior_expression make(behavior_expression::operation kind, std::string value = {}) {
    constexpr std::size_t maximum_expression_nodes = 1024;
    if (++nodes > maximum_expression_nodes) {
      throw exception::bad_member_spec{input, "Expression exceeds 1024 nodes"};
    }
    behavior_expression result{kind, std::move(value), {}, behavior_source_offset(input)};
    result.record_selection_enabled = active_language_version >= language_version{1u, 11u, 0u};
    result.numeric_contract_enabled = active_language_version >= language_version{1u, 11u, 0u};
    return result;
  }
  // Parse bounded positional arguments while preserving their left-to-right source order.
  behavior_expression call(std::string name) {
    require_call_language();
    auto result = make(behavior_expression::operation::call, std::move(name));
    check_and_increase(input, '(');
    skip_whitespace_and_comment(input);
    constexpr std::size_t maximum_call_arguments = 64;
    while (!input.full() && *input != ')') {
      if (result.operands.size() == maximum_call_arguments) {
        throw exception::bad_member_spec{input, "Function call exceeds 64 arguments"};
      }
      result.operands.push_back(conditional());
      skip_whitespace_and_comment(input);
      if (!consume(",")) { break; }
      skip_whitespace_and_comment(input);
      if (!input.full() && *input == ')') {
        throw exception::bad_member_spec{input, "Trailing function call argument comma"};
      }
    }
    check_and_increase(input, ')');
    return result;
  }
  // Parse constants, bound symbols, explicit conversions and parenthesized subexpressions.
  behavior_expression primary() {
    using operation = behavior_expression::operation;
    skip_whitespace_and_comment(input);
    if (++depth > 64) { throw exception::bad_member_spec{input, "Expression nesting exceeds 64"}; }
    behavior_expression result;
    if (*input == '(') {
      ++input;
      result = conditional();
      skip_whitespace_and_comment(input);
      check_and_increase(input, ')');
    } else if (*input == '+' || *input == '-' || *input == '!') {
      const auto sign = *input;
      ++input;
      if (sign == '!') { require_checked_language(); }
      result = make(sign == '+' ? operation::positive : sign == '-' ? operation::negative : operation::logical_not);
      result.operands.push_back(primary());
    } else if (is_number(*input)) {
      const auto* begin = input.curr();
      while (!input.full() && (is_number(*input) || *input == '.' || *input == 'e' || *input == 'E' ||
             ((*input == '+' || *input == '-') && input.curr() != begin &&
              (input.curr()[-1] == 'e' || input.curr()[-1] == 'E')))) { ++input; }
      std::string value{reinterpret_cast<const char*>(begin), static_cast<std::size_t>(input.curr() - begin)};
      double checked{};
      const auto parsed = std::from_chars(value.data(), value.data() + value.size(), checked);
      if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(checked)) {
        throw exception::bad_member_spec{input, "Expression literal must be finite binary64"};
      }
      result = make(operation::literal, std::move(value));
    } else {
      auto name = parse_identifier_impl(input);
      skip_whitespace_and_comment(input);
      if (name == "math" && *input == '.') {
        ++input;
        const auto member = parse_identifier_impl(input);
        if (member == "pi") { result = make(operation::pi); }
        else if (member == "sqrt" || member == "sin" || member == "cos" || member == "is_finite") {
          require_checked_language();
          skip_whitespace_and_comment(input);
          check_and_increase(input, '(');
          result = make(member == "sqrt" ? operation::square_root : member == "sin" ? operation::sine :
                        member == "cos" ? operation::cosine : operation::is_finite);
          result.operands.push_back(conditional());
          skip_whitespace_and_comment(input);
          check_and_increase(input, ')');
        } else { throw exception::bad_member_spec{input, "Unknown portable math constant or function"}; }
      } else if ((name == "all" || name == "any") && *input == '(') {
        require_checked_language(); ++input;
        result = make(name == "all" ? operation::all : operation::any);
        result.operands.push_back(conditional());
        skip_whitespace_and_comment(input); check_and_increase(input, ',');
        skip_whitespace_and_comment(input);
        const auto* begin = input.curr();
        while (!input.full() && is_number(*input)) { ++input; }
        const auto* end = input.curr();
        const auto bound = std::from_chars(reinterpret_cast<const char*>(begin),
                                           reinterpret_cast<const char*>(end), result.reduction_bound);
        constexpr auto maximum_reduction_bound = behavior_expression::maximum_reduction_visits;
        if (begin == end || bound.ec != std::errc{} || result.reduction_bound == 0 ||
            result.reduction_bound > maximum_reduction_bound) {
          throw exception::bad_member_spec{input, "Reduction bound must be a literal from 1 to 1000000"};
        }
        skip_whitespace_and_comment(input); check_and_increase(input, ',');
        skip_whitespace_and_comment(input); result.value = parse_identifier_impl(input);
        if (!consume("=>")) { throw exception::bad_member_spec{input, "Reduction requires item => boolean expression"}; }
        result.operands.push_back(conditional());
        skip_whitespace_and_comment(input); check_and_increase(input, ')');
      } else if (name == "checked" && *input == '<') {
        if (active_language_version < language_version{1u, 11u, 0u}) {
          throw exception::bad_member_spec{input, "Checked numeric binding requires serializer version 1.11.0"};
        }
        ++input;
        skip_whitespace_and_comment(input);
        auto target = parse_type_expression(input, space);
        skip_whitespace_and_comment(input);
        check_and_increase(input, '>');
        skip_whitespace_and_comment(input);
        check_and_increase(input, '(');
        result = make(operation::checked_cast, target.name);
        result.resolved_type = std::move(target);
        result.operands.push_back(conditional());
        skip_whitespace_and_comment(input);
        check_and_increase(input, ')');
      } else if ((name == "size" || name == "to_double") && *input == '(') {
        ++input;
        if (name == "size") { require_checked_language(); }
        result = make(name == "size" ? operation::collection_size : operation::to_double);
        result.operands.push_back(conditional());
        skip_whitespace_and_comment(input);
        check_and_increase(input, ')');
      } else if (*input == '(') {
        result = call(std::move(name));
      } else if ((name == "true" || name == "false") && active_language_version >= language_version{1u, 8u, 0u}) {
        result = make(operation::boolean, std::move(name));
      } else if (consume("::")) {
        require_checked_language();
        std::string qualified{name};
        qualified += "::" + parse_identifier_impl(input);
        while (consume("::")) { qualified += "::" + parse_identifier_impl(input); }
        result = !input.full() && *input == '(' ? call(std::move(qualified)) :
            make(operation::enum_constant, std::move(qualified));
      } else {
        result = make(operation::symbol, std::move(name));
      }
    }
    // Projections are typed after generic resolution and remain side-effect free.
    for (;;) {
      if (consume(".")) {
        require_checked_language();
        auto name = parse_identifier_impl(input);
        skip_whitespace_and_comment(input);
        if (!input.full() && *input == '(') {
          if (active_language_version < language_version{1u, 11u, 0u}) {
            throw exception::bad_member_spec{input, "Projected readonly calls require serializer version 1.11.0"};
          }
          auto next = call(std::move(name));
          next.call_receiver = true;
          next.operands.insert(next.operands.begin(), std::move(result));
          result = std::move(next);
        } else {
          auto next = make(operation::member_access, std::move(name));
          next.operands.push_back(std::move(result)); result = std::move(next);
        }
      } else if (consume("[")) {
        require_checked_language();
        auto next = make(operation::index_access);
        next.operands.push_back(std::move(result)); next.operands.push_back(conditional());
        skip_whitespace_and_comment(input); check_and_increase(input, ']'); result = std::move(next);
      } else { break; }
    }
    --depth;
    return result;
  }
  // Multiplication binds before addition and subtraction; nodes retain exact grouping.
  behavior_expression product() {
    auto result = primary();
    skip_whitespace_and_comment(input);
    while (!input.full() && (*input == '*' || *input == '/')) {
      const auto token = *input;
      if (token == '/') { require_checked_language(); }
      ++input;
      auto next = make(token == '*' ? behavior_expression::operation::multiply : behavior_expression::operation::divide);
      next.operands.push_back(std::move(result));
      next.operands.push_back(primary());
      result = std::move(next);
      skip_whitespace_and_comment(input);
    }
    return result;
  }
  // Preserve left-associative arithmetic without algebraic rewriting.
  behavior_expression sum() {
    auto result = product();
    skip_whitespace_and_comment(input);
    while (!input.full() && (*input == '+' || *input == '-')) {
      const auto sign = *input;
      ++input;
      auto next = make(sign == '+' ? behavior_expression::operation::add : behavior_expression::operation::subtract);
      next.operands.push_back(std::move(result));
      next.operands.push_back(product());
      result = std::move(next);
      skip_whitespace_and_comment(input);
    }
    return result;
  }
  // Comparison/equality precede boolean conjunction and disjunction; operands are type checked later.
  behavior_expression comparison() {
    auto result = sum();
    for (;;) {
      using operation = behavior_expression::operation;
      std::optional<operation> kind;
      if (consume("<=")) { kind = operation::less_equal; }
      else if (consume(">=")) { kind = operation::greater_equal; }
      else if (consume("<")) { kind = operation::less; }
      else if (consume(">")) { kind = operation::greater; }
      if (!kind) { return result; }
      require_checked_language();
      auto next = make(*kind); next.operands.push_back(std::move(result)); next.operands.push_back(sum());
      result = std::move(next);
    }
  }
  // Equality supports matching double or boolean operands only.
  behavior_expression equality() {
    auto result = comparison();
    for (;;) {
      using operation = behavior_expression::operation;
      std::optional<operation> kind;
      if (consume("==")) { kind = operation::equal; }
      else if (consume("!=")) { kind = operation::not_equal; }
      if (!kind) { return result; }
      require_checked_language();
      auto next = make(*kind); next.operands.push_back(std::move(result)); next.operands.push_back(comparison());
      result = std::move(next);
    }
  }
  // Logical operators evaluate their right operands only when needed.
  behavior_expression conjunction() {
    auto result = equality();
    while (consume("&&")) {
      require_checked_language(); auto next = make(behavior_expression::operation::logical_and);
      next.operands.push_back(std::move(result)); next.operands.push_back(equality()); result = std::move(next);
    }
    return result;
  }
  // Disjunction is weaker than conjunction, matching every generated target's native precedence.
  behavior_expression disjunction() {
    auto result = conjunction();
    while (consume("||")) {
      require_checked_language(); auto next = make(behavior_expression::operation::logical_or);
      next.operands.push_back(std::move(result)); next.operands.push_back(conjunction()); result = std::move(next);
    }
    return result;
  }
  // A guarded branch preserves short circuit evaluation and bounds recursively nested guards.
  behavior_expression conditional() {
    auto result = disjunction();
    if (!consume("?")) { return result; }
    require_checked_language();
    if (++depth > 64) { throw exception::bad_member_spec{input, "Expression nesting exceeds 64"}; }
    auto next = make(behavior_expression::operation::conditional);
    next.operands.push_back(std::move(result)); next.operands.push_back(conditional());
    skip_whitespace_and_comment(input); check_and_increase(input, ':');
    next.operands.push_back(conditional()); --depth;
    return next;
  }
};

// Parse one API method; native payloads and pure expressions have no durable identity.
function_declaration parse_function(const rohit::type_check::schema_input_buffer auto& input,
                                    access_type access, namespace_node* space) {
  require_behavior_language(input);
  function_declaration result;
  result.access = access;
  result.source_offset = behavior_source_offset(input);
  const auto source_begin = result.source_offset;
  parse_identifier_impl(input); // function
  skip_whitespace_and_comment(input);
  if (next_keyword(input, "virtual") || next_keyword(input, "abstract") || next_keyword(input, "override")) {
    const auto modifier = parse_identifier_impl(input);
    result.dispatch = modifier == "virtual" ? function_declaration::dispatch_type::virtual_method :
        modifier == "abstract" ? function_declaration::dispatch_type::abstract_method :
                                 function_declaration::dispatch_type::override_method;
    skip_whitespace_and_comment(input);
  }
  result.return_type = parse_type_expression(input, space);
  skip_whitespace_and_comment(input);
  result.name = parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  if (!input.full() && *input == '<') {
    if (active_language_version < language_version{1u, 12u, 0u}) {
      throw exception::bad_member_spec{input, "Generic readonly functions require serializer version 1.12.0"};
    }
    ++input;
    do {
      skip_whitespace_and_comment(input);
      generic_parameter parameter;
      parameter.name = parse_identifier_impl(input);
      constexpr std::size_t maximum_input_types = 8;
      if (result.generic_parameters.size() >= maximum_input_types ||
          !serializer::get_cpp_type_or_empty(parameter.name).empty() ||
          std::any_of(result.generic_parameters.begin(), result.generic_parameters.end(),
              [&](const auto& item) { return item.name == parameter.name; })) {
        throw exception::bad_member_spec{input, "Duplicate, reserved, or excessive readonly input types"};
      }
      skip_whitespace_and_comment(input);
      check_and_increase(input, ':');
      parameter.constraint = parse_construction_constraint(input, space);
      result.generic_parameters.push_back(std::move(parameter));
      skip_whitespace_and_comment(input);
      if (input.full() || *input != ',') { break; }
      ++input;
    } while (true);
    check_and_increase(input, '>');
    skip_whitespace_and_comment(input);
  }
  check_and_increase(input, '(');
  skip_whitespace_and_comment(input);
  std::unordered_set<std::string> parameters;
  while (!input.full() && *input != ')') {
    function_parameter parameter;
    parameter.type = parse_type_expression(input, space);
    skip_whitespace_and_comment(input);
    parameter.name = parse_identifier_impl(input);
    if (!parameters.insert(parameter.name).second) {
      throw exception::bad_member_spec{input, "Duplicate function parameter"};
    }
    result.parameters.push_back(std::move(parameter));
    if (result.parameters.size() > 64) {
      throw exception::bad_member_spec{input, "Function exceeds 64 parameters"};
    }
    skip_whitespace_and_comment(input);
    if (*input != ',') { break; }
    ++input;
    skip_whitespace_and_comment(input);
    if (*input == ')') { throw exception::bad_member_spec{input, "Trailing function parameter comma"}; }
  }
  check_and_increase(input, ')');
  skip_whitespace_and_comment(input);
  const auto effect = parse_identifier_impl(input);
  if (effect != "readonly" && effect != "edit") {
    throw exception::bad_member_spec{input, "Function requires readonly or edit effect"};
  }
  result.effect = effect == "edit" ? function_declaration::effect_type::edit :
                                   function_declaration::effect_type::readonly;
  if (!result.generic_parameters.empty() && (result.effect != function_declaration::effect_type::readonly ||
      result.dispatch != function_declaration::dispatch_type::ordinary)) {
    throw exception::bad_member_spec{input, "Generic input types require ordinary readonly functions"};
  }
  skip_whitespace_and_comment(input);
  while (next_keyword(input, "constexpr") || next_keyword(input, "noexcept")) {
    const auto qualifier = parse_identifier_impl(input);
    const bool nonthrowing = qualifier == "noexcept";
    if (nonthrowing && active_language_version < language_version{1u, 13u, 0u}) {
      throw exception::bad_member_spec{input, "Native nonthrowing contracts require serializer version 1.13.0"};
    }
    if (!nonthrowing && active_language_version < language_version{1u, 12u, 0u}) {
      throw exception::bad_member_spec{input, "Native constant-evaluation contracts require serializer version 1.12.0"};
    }
    auto& specified = nonthrowing ? result.cpp_noexcept : result.cpp_constexpr;
    if (specified) {
      throw exception::bad_member_spec{input, "Repeated function qualifier: " + qualifier};
    }
    skip_whitespace_and_comment(input);
    check_and_increase(input, '(');
    skip_whitespace_and_comment(input);
    if (parse_identifier_impl(input) != "cpp") {
      throw exception::bad_member_spec{input, "Only " + qualifier + "(cpp) is supported"};
    }
    skip_whitespace_and_comment(input);
    check_and_increase(input, ')');
    skip_whitespace_and_comment(input);
    specified = true;
  }
  if (next_keyword(input, "expression") || next_keyword(input, "predicate")) {
    if (active_language_version < language_version{1u, 10u, 0u} &&
        result.return_type.name != "double" && result.return_type.name != "bool") {
      throw exception::bad_member_spec{input, "Typed portable results require serializer version 1.10.0"};
    }
    result.predicate_body = next_keyword(input, "predicate");
    if (result.predicate_body) {
      if (active_language_version < language_version{1u, 8u, 0u} || result.return_type.name != "bool") {
        throw exception::bad_member_spec{input, "Portable predicate requires bool and serializer version 1.8.0"};
      }
    }
    parse_identifier_impl(input);
    skip_whitespace_and_comment(input);
    check_and_increase(input, '(');
    const auto expression_begin = behavior_source_offset(input);
    result.expression = behavior_expression_parser{input, 0, 0, space}.conditional();
    if (result.return_type.name == "bool" && active_language_version < language_version{1u, 8u, 0u}) {
      throw exception::bad_member_spec{input, "Boolean portable results require serializer version 1.8.0"};
    }
    locate_behavior(*result.expression, expression_begin, behavior_source_offset(input));
    skip_whitespace_and_comment(input);
    check_and_increase(input, ')');
  } else {
    std::unordered_set<std::string> targets;
    while (is_first_identifier(input)) {
      auto target = parse_identifier_impl(input);
      if (!is_behavior_language(target) || !targets.insert(target).second) {
        throw exception::bad_member_spec{input, "Unknown or repeated function target language"};
      }
      result.bodies.push_back(parse_native_block(input, std::move(target), access));
      skip_whitespace_and_comment(input);
    }
  }
  check_and_increase(input, ';');
  if (result.dispatch == function_declaration::dispatch_type::abstract_method &&
      (!result.bodies.empty() || result.expression)) {
    throw exception::bad_member_spec{input, "Abstract methods cannot have bodies"};
  }
  if (result.expression && result.effect != function_declaration::effect_type::readonly) {
    throw exception::bad_member_spec{input, "Portable expressions require readonly"};
  }
  if (result.cpp_noexcept && (result.effect != function_declaration::effect_type::readonly || result.expression)) {
    throw exception::bad_member_spec{input, "noexcept(cpp) requires a readonly native or external function"};
  }
  if (result.cpp_noexcept && !result.generic_parameters.empty() &&
      std::none_of(result.bodies.begin(), result.bodies.end(), [](const auto& body) { return body.language == "cpp"; })) {
    throw exception::bad_member_spec{input, "Independently generic noexcept(cpp) functions require a native C++ body"};
  }
  if (result.cpp_constexpr && (result.effect != function_declaration::effect_type::readonly ||
      result.dispatch != function_declaration::dispatch_type::ordinary || result.expression ||
      std::none_of(result.bodies.begin(), result.bodies.end(), [](const auto& body) {
        return body.language == "cpp";
      }))) {
    throw exception::bad_member_spec{input, "constexpr(cpp) requires an ordinary readonly native C++ body"};
  }
  locate_behavior(result, source_begin, behavior_source_offset(input));
  return result;
}

// Bind numeric literals to one exact scalar width without runtime narrowing or rounding.
bool bind_behavior_literal(behavior_expression& node, const std::string& target,
                           namespace_node* space) {
  using operation = behavior_expression::operation;
  if (target == "float") {
    std::string text;
    if (node.kind == operation::literal) { text = node.value; }
    else if ((node.kind == operation::positive || node.kind == operation::negative) &&
        node.operands.size() == 1 && node.operands.front().kind == operation::literal) {
      text = (node.kind == operation::negative ? "-" : "") + node.operands.front().value;
    } else { return false; }
    float number{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(number)) {
      throw std::invalid_argument{"Expression literal is outside float"};
    }
    node.kind = operation::literal;
    node.value = std::move(text);
    node.operands.clear();
    node.result_type = "float";
    node.resolved_type = type_name{std::string{"float"}, space};
    node.resolved_type->type = object_type::primitive;
    return true;
  }
  const bool integer = target == "char" || target == "int8" || target == "uint8" ||
      target == "int16" || target == "uint16" || target == "int32" || target == "uint32" ||
      target == "int64" || target == "uint64";
  if (!integer) { return false; }
  std::string text;
  if (node.kind == operation::literal && node.value.find_first_of(".eE") == std::string::npos) {
    text = node.value;
  } else if ((node.kind == operation::negative || node.kind == operation::positive) &&
      node.operands.size() == 1 && node.operands.front().kind == operation::literal &&
      node.operands.front().value.find_first_of(".eE") == std::string::npos) {
    text = (node.kind == operation::negative ? "-" : "") + node.operands.front().value;
  } else { return false; }
  const bool signed_value = target.starts_with("int");
  const unsigned width = target == "char" || target.ends_with("8") ? 8 :
      target.ends_with("16") ? 16 : target.ends_with("32") ? 32 : 64;
  bool valid{};
  if (signed_value) {
    std::int64_t number{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    valid = parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
        (width == 64 || (number >= -(std::int64_t{1} << (width - 1)) &&
                        number < (std::int64_t{1} << (width - 1))));
  } else {
    std::uint64_t number{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    valid = parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
        (width == 64 || number < (std::uint64_t{1} << width));
  }
  if (!valid) { throw std::invalid_argument{"Integral expression literal is outside " + target}; }
  node.kind = operation::integer_literal;
  node.value = std::move(text);
  node.operands.clear();
  node.result_type = target;
  node.resolved_type = type_name{std::string{target}, space};
  node.resolved_type->type = object_type::primitive;
  return true;
}

// Compare complete resolved storage types, including digest identity and extent.
bool matching_behavior_type(const type_name& left, const type_name& right) {
  if (left.get_full_name() != right.get_full_name() || left.digest != right.digest ||
      left.digest_extent != right.digest_extent || left.arguments.size() != right.arguments.size()) {
    return false;
  }
  for (std::size_t index{}; index < left.arguments.size(); ++index) {
    if (!matching_behavior_type(left.arguments[index], right.arguments[index])) { return false; }
  }
  return true;
}

// Bind scalar identities, lexical reduction items and readonly projections without numeric narrowing.
std::string validate_behavior_expression(behavior_expression& value, const class_node& owner,
    const function_declaration& method,
    const std::unordered_map<std::string, syntax_node*>& symbols = {},
    const std::map<std::string, behavior_expression>& locals = {},
    const std::vector<const function_node*>& functions = {}) {
  using operation = behavior_expression::operation;
  const auto typed = [&](std::string type) { value.result_type = type; return type; };
  const auto bind = [&](const type_name& type, member::modifier_type modifier = member::modifier_type::none,
                        std::uint64_t extent = 0) {
    value.resolved_type = type; value.collection_modifier = modifier; value.fixed_extent = extent;
    return typed(modifier == member::modifier_type::array || type.is_sequence() ? "collection" :
                 type.type == object_type::enum_type ? "enum:" + type.get_full_name() :
                 type.type == object_type::class_type || type.type == object_type::generic_definition ?
                     "record:" + type.get_full_name() : type.common_numeric ? type.get_full_name() : type.name);
  };
  const auto integer = [](std::string_view type) {
    return type == "char" || type == "int8" || type == "uint8" || type == "int16" ||
        type == "uint16" || type == "int32" || type == "uint32" || type == "int64" || type == "uint64";
  };
  // Category constraints must rule out records and other opaque values before generic arithmetic.
  const auto numeric_parameter = [&](std::string_view name) {
    const generic_parameter* parameter{};
    for (const auto& item : owner.generic_parameters) { if (item.name == name) { parameter = &item; } }
    for (const auto& item : method.generic_parameters) { if (item.name == name) { parameter = &item; } }
    if (!parameter || !parameter->constraint) { return false; }
    const auto numeric_constraint = [&](const auto& self, const generic_constraint& constraint) -> bool {
      using kind = generic_constraint::operation;
      if (constraint.kind == kind::integral || constraint.kind == kind::floating_point) { return true; }
      if (constraint.kind == kind::exact_type) {
        return constraint.type && (integer(constraint.type->name) ||
            constraint.type->name == "float" || constraint.type->name == "double");
      }
      if (constraint.kind == kind::logical_and) {
        return std::any_of(constraint.operands.begin(), constraint.operands.end(),
            [&](const auto& operand) { return self(self, operand); });
      }
      if (constraint.kind == kind::logical_or) {
        return std::all_of(constraint.operands.begin(), constraint.operands.end(),
            [&](const auto& operand) { return self(self, operand); });
      }
      return false;
    };
    return numeric_constraint(numeric_constraint, *parameter->constraint);
  };
  const auto numeric = [&](const std::string& type) {
    return integer(type) || type == "float" || type == "double" || numeric_parameter(type) ||
        type.starts_with("common_type<");
  };
  // The common numeric result policy requires a proof that each dependent input excludes bool.
  const auto parameter_excludes_bool = [&](std::string_view name) {
    const generic_parameter* parameter{};
    for (const auto& item : owner.generic_parameters) { if (item.name == name) { parameter = &item; } }
    for (const auto& item : method.generic_parameters) { if (item.name == name) { parameter = &item; } }
    if (!parameter || !parameter->constraint) { return false; }
    const auto accepts_bool = [&](const auto& self, const generic_constraint& constraint) -> std::optional<bool> {
      using kind = generic_constraint::operation;
      if (constraint.kind == kind::integral) { return true; }
      if (constraint.kind == kind::floating_point || constraint.kind == kind::stateless ||
          constraint.kind == kind::factory) { return false; }
      if (constraint.kind == kind::exact_type) {
        if (!constraint.type) { return std::nullopt; }
        const auto& type = *constraint.type;
        if (type.name == "bool") { return true; }
        if (integer(type.name) || type.name == "float" || type.name == "double" ||
            type.name == "string" || type.resolved_node) { return false; }
        return std::nullopt;
      }
      if (constraint.kind == kind::logical_not) {
        const auto operand = self(self, constraint.operands.at(0));
        return operand ? std::optional<bool>{!*operand} : std::nullopt;
      }
      const bool conjunction = constraint.kind == kind::logical_and;
      bool unknown{};
      for (const auto& operand : constraint.operands) {
        const auto accepted = self(self, operand);
        if (!accepted) { unknown = true; }
        else if (*accepted != conjunction) { return !conjunction; }
      }
      return unknown ? std::nullopt : std::optional<bool>{conjunction};
    };
    const auto accepted = accepts_bool(accepts_bool, *parameter->constraint);
    return accepted && !*accepted;
  };
  const auto scalar = [&](const std::string& type) {
    return numeric(type) || type == "bool" || type.starts_with("enum:") ||
        std::find(owner.type_parameters.begin(), owner.type_parameters.end(), type) != owner.type_parameters.end();
  };
  const auto literal = [&](behavior_expression& node, const std::string& target) {
    const auto integral_token = [&](const behavior_expression& candidate) {
      const auto& token = candidate.kind == operation::literal ? candidate.value : candidate.operands.front().value;
      return token.find_first_of(".eE") == std::string::npos;
    };
    if (value.numeric_contract_enabled &&
        (numeric_parameter(target) || target == "float" || target == "double") &&
        (node.kind == operation::literal ||
         ((node.kind == operation::negative || node.kind == operation::positive) &&
          node.operands.size() == 1 && node.operands.front().kind == operation::literal)) &&
        (numeric_parameter(target) || integral_token(node))) {
      // Preserve integral literal bits before binding to a floating or dependent representation.
      auto source = node;
      const auto& token = node.kind == operation::literal ? node.value : node.operands.front().value;
      if (token.find_first_of(".eE") == std::string::npos) {
        bind_behavior_literal(source, node.kind == operation::negative ? "int64" : "uint64", owner.parent_namespace);
      }
      node.kind = operation::checked_cast;
      node.value = target;
      node.operands = {std::move(source)};
      node.result_type = target;
      node.resolved_type = type_name{std::string{target}, owner.parent_namespace};
      if (target == "float" || target == "double") { node.resolved_type->type = object_type::primitive; }
      return true;
    }
    return bind_behavior_literal(node, target, owner.parent_namespace);
  };
  const auto validate = [&](behavior_expression& node, const auto& scope) {
    return validate_behavior_expression(node, owner, method, symbols, scope, functions);
  };
  if (value.kind == operation::call) {
    value.call_target = nullptr;
    value.call_base_path.clear();
    for (auto& argument : value.operands) { validate(argument, locals); }
    const class_node* receiver = &owner;
    const auto argument_begin = value.call_receiver ? std::size_t{1} : std::size_t{};
    if (value.call_receiver) {
      const auto& base = value.operands.front();
      if (!base.result_type.starts_with("record:") || !base.resolved_type ||
          !base.resolved_type->resolved_node ||
          (base.resolved_type->type != object_type::class_type &&
           base.resolved_type->type != object_type::generic_definition)) {
        throw std::invalid_argument{"Readonly call receiver requires a generated owning record"};
      }
      receiver = static_cast<const class_node*>(base.resolved_type->resolved_node);
      if (receiver->storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
          receiver->supports_managed()) {
        throw std::invalid_argument{"Readonly call receiver requires an ordinary owning record"};
      }
    }
    struct candidate {
      const syntax_node* target{};
      const function_declaration* declaration{};
      std::size_t index{};
      std::vector<std::size_t> base_path{};
      std::vector<behavior_expression> arguments{};
    };
    // Rebind the selected receiver's template arguments along its exact inherited method path.
    const auto receiver_type = [&](type_name type, const candidate& selected) {
      if (!value.call_receiver) { return type; }
      std::unordered_map<std::string, type_name> bindings;
      const auto substitute = [&](const auto& self, type_name item) -> type_name {
        if (!item.application && item.arguments.empty()) {
          if (const auto found = bindings.find(item.name); found != bindings.end()) { return found->second; }
        }
        for (auto& argument : item.arguments) { argument = self(self, std::move(argument)); }
        return item;
      };
      const auto bind_arguments = [&](const class_node& scope, const type_name& application) {
        bindings.clear();
        for (std::size_t index{}; index < scope.generic_parameters.size() && index < application.arguments.size(); ++index) {
          bindings.emplace(scope.generic_parameters[index].name, application.arguments[index]);
        }
      };
      const class_node* scope = receiver;
      auto application = *value.operands.front().resolved_type;
      bind_arguments(*scope, application);
      for (const auto index : selected.base_path) {
        const auto& base = scope->parents.at(index);
        application = base.type_expression.empty() ? type_name{std::string{base.name}, base.current_namespace} :
            substitute(substitute, base.type_expression.front());
        scope = base.parent_class;
        bind_arguments(*scope, application);
      }
      return substitute(substitute, std::move(type));
    };
    std::vector<candidate> candidates;
    const auto collect_members = [&](const auto& self, const class_node& scope,
        std::vector<std::size_t> path, std::set<std::string> hidden) -> void {
      for (std::size_t index{}; index < scope.functions.size(); ++index) {
        const auto& declaration = scope.functions[index];
        const candidate current{&scope, &declaration, index, path, {}};
        std::string signature = declaration.name + "(";
        for (const auto& parameter : declaration.parameters) {
          signature += receiver_type(parameter.type, current).get_full_name() + ",";
        }
        signature += ')';
        if (declaration.name == value.value && !hidden.contains(signature) &&
            (value.call_receiver ? declaration.access == access_type::public_access :
             (&scope == &owner || declaration.access != access_type::private_access))) {
          candidates.push_back(current);
        }
      }
      for (const auto& declaration : scope.functions) {
        const candidate current{&scope, &declaration, 0, path, {}};
        std::string signature = declaration.name + "(";
        for (const auto& parameter : declaration.parameters) {
          signature += receiver_type(parameter.type, current).get_full_name() + ",";
        }
        hidden.insert(signature + ')');
      }
      for (std::size_t index{}; index < scope.parents.size(); ++index) {
        if (!scope.parents[index].parent_class ||
            (value.call_receiver ? scope.parents[index].access != access_type::public_access :
             (&scope != &owner && scope.parents[index].access == access_type::private_access))) { continue; }
        auto next = path;
        next.push_back(index);
        self(self, *scope.parents[index].parent_class, std::move(next), hidden);
      }
    };
    if (value.value.find("::") == std::string::npos) { collect_members(collect_members, *receiver, {}, {}); }
    if (candidates.empty() && !value.call_receiver) {
      auto scope = owner.parent_namespace ? owner.parent_namespace->get_full_name() : std::string{};
      for (;;) {
        const auto name = scope.empty() ? value.value : scope + "::" + value.value;
        for (const auto* function : functions) {
          if (function->get_full_name() == name) {
            candidates.push_back({function, &function->function, 0, {}, {}});
          }
        }
        if (!candidates.empty() || scope.empty()) { break; }
        const auto separator = scope.rfind("::");
        scope = separator == std::string::npos ? std::string{} : scope.substr(0, separator);
      }
    }
    const auto exact = [&](const behavior_expression& argument, const type_name& parameter) {
      if (argument.resolved_type) {
        return argument.collection_modifier == member::modifier_type::none &&
            matching_behavior_type(*argument.resolved_type, parameter);
      }
      return argument.result_type == parameter.name;
    };
    std::vector<candidate> matches;
    const bool has_generic_candidate = std::any_of(candidates.begin(), candidates.end(),
        [](const auto& candidate) { return !candidate.declaration->generic_parameters.empty(); });
    // Resolve exact arguments before binding an untyped numeric literal to a parameter width.
    for (const bool bind_literals : {false, true}) {
      for (auto candidate : candidates) {
        if (!candidate.declaration->generic_parameters.empty()) { continue; }
        if (candidate.declaration->parameters.size() != value.operands.size() - argument_begin) { continue; }
        candidate.arguments = value.operands;
        bool matched = true;
        for (std::size_t index{argument_begin}; index < candidate.arguments.size(); ++index) {
          auto& argument = candidate.arguments[index];
          const auto parameter = receiver_type(candidate.declaration->parameters[index - argument_begin].type, candidate);
          if (exact(argument, parameter)) { continue; }
          try {
            if (!bind_literals || !literal(argument, parameter.name)) { matched = false; break; }
          } catch (const std::invalid_argument&) {
            matched = false;
            break;
          }
        }
        if (matched) { matches.push_back(std::move(candidate)); }
      }
      if (!matches.empty()) { break; }
    }
    if (matches.empty()) {
      if (has_generic_candidate) {
        throw std::invalid_argument{"Schema expression calls to independently generic functions are unsupported: " + value.value};
      }
      throw std::invalid_argument{"No matching readonly function: " + value.value};
    }
    if (matches.size() != 1) { throw std::invalid_argument{"Ambiguous readonly function call: " + value.value}; }
    const auto& selected = matches.front();
    if (selected.declaration->effect != function_declaration::effect_type::readonly ||
        selected.declaration->return_type.name == "void") {
      throw std::invalid_argument{"Expression calls require a nonvoid readonly function: " + value.value};
    }
    value.call_target = selected.target;
    value.call_method_index = selected.index;
    value.call_base_path = selected.base_path;
    value.operands = selected.arguments;
    return bind(receiver_type(selected.declaration->return_type, selected));
  }
  if (value.kind == operation::literal || value.kind == operation::pi) {
    return typed(value.result_type == "float" ? "float" : "double");
  }
  if (value.kind == operation::integer_literal) { return value.result_type; }
  if (value.kind == operation::boolean) { return typed("bool"); }
  if (value.kind == operation::symbol) {
    if (value.local_symbol) { return value.result_type; }
    if (const auto local = locals.find(value.value); local != locals.end()) {
      const auto numeric_contract = value.numeric_contract_enabled;
      value = local->second;
      value.numeric_contract_enabled = numeric_contract;
      return value.result_type;
    }
    const auto parameter = std::find_if(method.parameters.begin(), method.parameters.end(),
        [&](const auto& item) { return item.name == value.value; });
    if (parameter != method.parameters.end()) { return bind(parameter->type); }
    const auto field = std::find_if(owner.member_list.begin(), owner.member_list.end(),
        [&](const auto& item) { return item.name == value.value; });
    if (field == owner.member_list.end() || field->wire_only || (field->modifier != member::modifier_type::none && field->modifier != member::modifier_type::array)) {
      throw std::invalid_argument{"Unknown or unsupported expression symbol: " + value.value};
    }
    return bind(field->type_name_list.front(), field->modifier, field->fixed_extent);
  }
  if (value.kind == operation::enum_constant) {
    if (value.resolved_type) { return bind(*value.resolved_type); }
    const auto split = value.value.rfind("::");
    const auto type_spelling = value.value.substr(0, split), enumerator = value.value.substr(split + 2);
    const auto* declaration = find_declared_type(type_spelling, owner.parent_namespace, symbols);
    if (!declaration || declaration->type != object_type::enum_type) { throw std::invalid_argument{"Unknown expression enum: " + type_spelling}; }
    const auto& enumeration = static_cast<const enum_node&>(*declaration);
    if (std::find(enumeration.enum_name_list.begin(), enumeration.enum_name_list.end(), enumerator) == enumeration.enum_name_list.end()) {
      throw std::invalid_argument{"Unknown expression enumerator: " + value.value};
    }
    type_name type{std::string{type_spelling}, owner.parent_namespace}; type.type = object_type::enum_type; type.resolved_node = declaration;
    type.defined_namespace = declaration->parent_namespace; value.value = enumerator; return bind(type);
  }
  if (value.kind == operation::member_access) {
    validate(value.operands.at(0), locals);
    const auto& base = value.operands.front();
    if (base.result_type.starts_with("record:") && base.resolved_type && base.resolved_type->resolved_node) {
      const auto& record = static_cast<const class_node&>(*base.resolved_type->resolved_node);
      const member* selected{};
      const auto find = [&](const auto& self, const class_node& current, std::vector<std::size_t> path) -> void {
        for (const auto& field : current.member_list) {
          if (field.name == value.value && field.access == access_type::public_access && !field.wire_only) {
            if (selected) { throw std::invalid_argument{"Ambiguous inherited expression member: " + value.value}; }
            selected = &field; value.member_base_path = path;
          }
        }
        for (std::size_t index{}; index < current.parents.size(); ++index) {
          const auto& parent = current.parents[index];
          if (parent.access != access_type::public_access || !parent.parent_class) { continue; }
          auto nested = path; nested.push_back(index); self(self, *parent.parent_class, std::move(nested));
        }
      };
      find(find, record, {});
      if (selected && (selected->modifier == member::modifier_type::none || selected->modifier == member::modifier_type::array)) {
        auto application = *base.resolved_type;
        const class_node* current = &record;
        std::unordered_map<std::string, type_name> bindings;
        const auto substitute = [&](const auto& self, type_name type) -> type_name {
          if (!type.application) {
            if (const auto found = bindings.find(type.name); found != bindings.end()) { return found->second; }
          }
          for (auto& argument : type.arguments) { argument = self(self, argument); }
          return type;
        };
        const auto bind_arguments = [&]() {
          bindings.clear();
          for (std::size_t index{}; index < current->generic_parameters.size() && index < application.arguments.size(); ++index) {
            bindings.emplace(current->generic_parameters[index].name, application.arguments[index]);
          }
        };
        bind_arguments();
        for (const auto index : value.member_base_path) {
          const auto& parent = current->parents.at(index);
          if (!parent.type_expression.empty()) { application = substitute(substitute, parent.type_expression.front()); }
          current = parent.parent_class;
          bind_arguments();
        }
        return bind(substitute(substitute, selected->type_name_list.front()), selected->modifier, selected->fixed_extent);
      }
    }
    throw std::invalid_argument{"Member projection requires an accessible generated record field: " + value.value};
  }
  if (value.kind == operation::index_access || value.kind == operation::collection_size || value.kind == operation::all || value.kind == operation::any) {
    validate(value.operands.at(0), locals);
    const auto& container = value.operands.front();
    if (container.result_type != "collection" || !container.resolved_type) { throw std::invalid_argument{"Portable collection operation requires a sequence or fixed array"}; }
    const auto element = container.resolved_type->is_sequence() ? container.resolved_type->arguments.front() : *container.resolved_type;
    if (value.kind == operation::collection_size) {
      type_name size_type{std::string{"uint64"}, owner.parent_namespace}; size_type.type = object_type::primitive; return bind(size_type);
    }
    if (value.kind == operation::index_access) {
      auto index_type = validate(value.operands.at(1), locals);
      if (literal(value.operands.at(1), "uint64")) { index_type = "uint64"; }
      if (!integer(index_type)) { throw std::invalid_argument{"Collection indices require exact integral values"}; }
      return bind(element);
    }
    if (locals.contains(value.value) ||
        std::any_of(method.parameters.begin(), method.parameters.end(), [&](const auto& parameter) { return parameter.name == value.value; }) ||
        std::any_of(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) { return field.name == value.value; })) {
      throw std::invalid_argument{"Reduction binder cannot shadow an existing expression symbol: " + value.value};
    }
    const auto binder = value.value;
    behavior_expression local; local.kind = operation::symbol; local.local_symbol = true;
    local.numeric_contract_enabled = value.numeric_contract_enabled;
    local.value = "_srl_expression_item_" + std::to_string(value.source_offset);
    while (locals.contains(local.value) ||
        std::any_of(locals.begin(), locals.end(), [&](const auto& binding) { return binding.second.value == local.value; }) ||
        std::any_of(method.parameters.begin(), method.parameters.end(), [&](const auto& parameter) { return parameter.name == local.value; }) ||
        std::any_of(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) { return field.name == local.value; })) {
      local.value += '_';
    }
    local.resolved_type = element;
    local.result_type = element.type == object_type::enum_type ? "enum:" + element.get_full_name() :
        element.type == object_type::class_type ? "record:" + element.get_full_name() : element.is_sequence() ? "collection" : element.name;
    auto nested = locals; nested.emplace(binder, local);
    if (validate(value.operands.at(1), nested) != "bool") { throw std::invalid_argument{"Collection reductions require a boolean body"}; }
    value.value = local.value; return typed("bool");
  }
  if (value.kind == operation::checked_cast) {
    if (!value.numeric_contract_enabled || !value.resolved_type) {
      throw std::invalid_argument{"Checked numeric binding requires serializer version 1.11.0"};
    }
    const auto target = *value.resolved_type;
    if (target.common_numeric ? (target.arguments.size() != 2 ||
        !std::all_of(target.arguments.begin(), target.arguments.end(), [&](const auto& argument) {
          return !argument.application && numeric(argument.name) && argument.name != "bool" &&
              (!numeric_parameter(argument.name) || parameter_excludes_bool(argument.name));
        })) : (!target.arguments.empty() || !numeric(target.name))) {
      throw std::invalid_argument{"Checked numeric binding requires a numeric result representation"};
    }
    auto& source = value.operands.at(0);
    if (source.kind == operation::literal && source.value.find_first_of(".eE") == std::string::npos) {
      bind_behavior_literal(source, "uint64", owner.parent_namespace);
    } else if ((source.kind == operation::negative || source.kind == operation::positive) &&
        source.operands.size() == 1 && source.operands.front().kind == operation::literal &&
        source.operands.front().value.find_first_of(".eE") == std::string::npos) {
      bind_behavior_literal(source, source.kind == operation::negative ? "int64" : "uint64", owner.parent_namespace);
    }
    if (!numeric(validate(source, locals))) {
      throw std::invalid_argument{"Checked numeric binding requires a numeric source representation"};
    }
    return bind(target);
  }
  if (value.kind == operation::to_double) {
    const auto type = validate(value.operands.at(0), locals);
    if (type != "int32" && type != "uint32") {
      throw std::invalid_argument{"to_double currently requires int32 or uint32"};
    }
    value.value = type;
    return typed("double");
  }
  std::vector<std::string> types;
  for (auto& operand : value.operands) { types.push_back(validate(operand, locals)); }
  if (value.kind == operation::is_finite) {
    if (types.at(0) != "float" && types.at(0) != "double") { throw std::invalid_argument{"math.is_finite requires float or double"}; }
    return typed("bool");
  }
  if (types.size() == 2) {
    if (literal(value.operands[1], types[0])) { types[1] = types[0]; }
    if (literal(value.operands[0], types[1])) { types[0] = types[1]; }
  }
  if (value.kind == operation::conditional) {
    if (types.at(1).starts_with("record:") || types.at(2).starts_with("record:")) {
      if (!value.record_selection_enabled) {
        throw std::invalid_argument{"Record conditional selection requires serializer version 1.11.0"};
      }
      const auto& yes = value.operands.at(1);
      const auto& no = value.operands.at(2);
      if (types.at(0) != "bool" || !yes.resolved_type || !no.resolved_type ||
          yes.collection_modifier != member::modifier_type::none ||
          no.collection_modifier != member::modifier_type::none ||
          !matching_behavior_type(*yes.resolved_type, *no.resolved_type) ||
          !yes.resolved_type->resolved_node) {
        throw std::invalid_argument{"Record conditional selection requires exactly matching owning branches"};
      }
      const auto& record = static_cast<const class_node&>(*yes.resolved_type->resolved_node);
      if (record.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) || record.supports_managed()) {
        throw std::invalid_argument{"Record conditional selection requires ordinary owning values"};
      }
      return bind(*yes.resolved_type);
    }
    if (literal(value.operands[2], types[1])) { types[2] = types[1]; }
    if (literal(value.operands[1], types[2])) { types[1] = types[2]; }
    if (types.at(0) != "bool" || types.at(1) != types.at(2) || !scalar(types.at(1))) {
      throw std::invalid_argument{"Portable guard requires a boolean condition and matching scalar branches"};
    }
    value.resolved_type = value.operands.at(1).resolved_type;
    return typed(types.at(1));
  }
  if (value.kind == operation::logical_not || value.kind == operation::logical_and || value.kind == operation::logical_or) {
    if (std::any_of(types.begin(), types.end(), [](const auto& type) { return type != "bool"; })) {
      throw std::invalid_argument{"Portable logical operators require boolean operands"};
    }
    return typed("bool");
  }
  if (value.kind == operation::equal || value.kind == operation::not_equal) {
    if (types.at(0) != types.at(1) || !scalar(types.at(0))) {
      throw std::invalid_argument{"Portable equality requires matching scalar or enum operands"};
    }
    return typed("bool");
  }
  if (value.kind == operation::less || value.kind == operation::less_equal || value.kind == operation::greater || value.kind == operation::greater_equal) {
    if (types.at(0) != types.at(1) || !scalar(types.at(0)) || types.at(0) == "bool") {
      throw std::invalid_argument{"Portable ordering requires matching numeric or enum operands"};
    }
    return typed("bool");
  }
  const bool arithmetic = value.kind == operation::positive || value.kind == operation::negative ||
      value.kind == operation::add || value.kind == operation::subtract ||
      value.kind == operation::multiply || value.kind == operation::divide;
  if (arithmetic && value.numeric_contract_enabled) {
    if (!numeric(types.at(0)) || std::any_of(types.begin(), types.end(),
        [&](const auto& type) { return type != types.at(0); })) {
      throw std::invalid_argument{"Checked arithmetic requires matching numeric representations; use checked<T>(value) explicitly"};
    }
    value.resolved_type = value.operands.front().resolved_type.value_or(
        type_name{std::string{types.front()}, owner.parent_namespace});
    return typed(types.front());
  }
  if (std::any_of(types.begin(), types.end(), [](const auto& type) { return type != "double"; })) {
    throw std::invalid_argument{"Portable arithmetic/comparisons require explicit binary64 operands"};
  }
  return typed(value.kind == operation::less || value.kind == operation::less_equal || value.kind == operation::greater || value.kind == operation::greater_equal ? "bool" : "double");
}

// Match complete resolved signatures before selecting backend dispatch adapters.
std::string behavior_signature(const function_declaration& method) {
  std::string key = method.name + "(";
  for (const auto& parameter : method.parameters) { key += parameter.type.get_full_name() + ","; }
  return key + ")";
}

// Keep abstract behavior separate from data construction and diagnose inherited ambiguity early.
void validate_behavior_contracts(const rohit::type_check::schema_input_buffer auto& input,
                                 const std::vector<std::unique_ptr<syntax_node>>& nodes) {
  const auto inherited_methods = [&](const auto& self, const class_node& owner,
      std::map<std::string, std::vector<const function_declaration*>>& result) -> void {
    for (const auto& base : owner.parents) { self(self, *base.parent_class, result); }
    for (const auto& method : owner.functions) { result[behavior_signature(method)].push_back(&method); }
  };
  using exception_type_bindings = std::map<std::string, std::string>;
  // Retain dependent extents while composing the lexical arguments of each generic base path.
  const auto dimension_key = [&](const auto& self, const dimension_expression& value,
      const exception_type_bindings& bindings) -> std::string {
    if (value.kind == dimension_expression::operation::literal) { return std::to_string(value.value); }
    if (value.kind == dimension_expression::operation::parameter) {
      const auto found = bindings.find(value.name);
      return found == bindings.end() ? value.name : found->second;
    }
    return "(" + self(self, value.operands.at(0), bindings) +
        (value.kind == dimension_expression::operation::add ? "+" : "*") +
        self(self, value.operands.at(1), bindings) + ")";
  };
  // Compare nested type applications in the derived receiver's scope without mutating shared AST types.
  const auto exception_type_key = [&](const auto& self, const type_name& value,
      const exception_type_bindings& bindings) -> std::string {
    if (value.kind == generic_argument_kind::dimension) {
      return value.expression.empty() ? std::to_string(value.dimension) :
          dimension_key(dimension_key, value.expression.front(), bindings);
    }
    if (!value.application) {
      if (!value.resolved_node && value.type == object_type::unresolved) {
        const auto found = bindings.find(value.name);
        if (found != bindings.end()) { return found->second; }
      }
      return value.get_full_name();
    }
    auto name = value.name;
    if (value.resolved_node) {
      const auto* record = dynamic_cast<const class_node*>(value.resolved_node);
      name = record && !record->generic_name.empty() ? record->generic_name :
          value.resolved_node->get_full_name();
    } else if (value.defined_namespace) {
      name = get_full_name_for_namespace(value.defined_namespace) + "::" + name;
    }
    name += "<";
    for (std::size_t index{}; index < value.arguments.size(); ++index) {
      if (index) { name += ','; }
      name += self(self, value.arguments[index], bindings);
    }
    return name + ">";
  };
  // Match the authored overload after inherited class arguments have been rebound to its receiver.
  const auto exception_signature = [&](const function_declaration& method,
      const exception_type_bindings& bindings) {
    std::string key = method.name + "(";
    for (const auto& parameter : method.parameters) {
      key += exception_type_key(exception_type_key, parameter.type, bindings) + ",";
    }
    return key + ")";
  };
  // Compose renamed/defaulted/nested class arguments through transitive inherited exception contracts.
  const auto inherited_exception_methods = [&](const auto& self, const parent& occurrence,
      const exception_type_bindings& outer_bindings,
      std::map<std::string, std::vector<const function_declaration*>>& result) -> void {
    const auto& owner = *occurrence.parent_class;
    exception_type_bindings bindings;
    const auto* application = occurrence.type_expression.empty() ? nullptr :
        &occurrence.type_expression.front();
    for (std::size_t index{}; index < owner.generic_parameters.size(); ++index) {
      const auto& parameter = owner.generic_parameters[index];
      if (application && index < application->arguments.size()) {
        bindings.emplace(parameter.name,
            exception_type_key(exception_type_key, application->arguments[index], outer_bindings));
      } else if (!parameter.default_argument.empty()) {
        bindings.emplace(parameter.name,
            exception_type_key(exception_type_key, parameter.default_argument.front(), bindings));
      }
    }
    for (const auto& base : owner.parents) { self(self, base, bindings, result); }
    for (const auto& method : owner.functions) {
      if (method.cpp_noexcept && method.dispatch != function_declaration::dispatch_type::ordinary) {
        result[exception_signature(method, bindings)].push_back(&method);
      }
    }
  };
  // Preserve an explicit C++ exception promise without widening existing dependent generic validation.
  const auto require_cpp_exception_contract = [&](const function_declaration& method,
      const std::vector<const function_declaration*>& matches, const std::string& key) {
    for (const auto* base : matches) {
      if (base->dispatch != function_declaration::dispatch_type::ordinary &&
          base->cpp_noexcept && !method.cpp_noexcept) {
        throw exception::bad_member_spec{input, "Inherited noexcept(cpp) behavior requires an explicit nonthrowing qualifier: " + key};
      }
    }
  };
  for (const auto& node : nodes) {
    if (node->type == object_type::namespace_type) {
      validate_behavior_contracts(input, static_cast<const namespace_node&>(*node).statements);
    } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
      const auto& owner = static_cast<const class_node&>(*node);
      std::map<std::string, std::vector<const function_declaration*>> inherited;
      for (const auto& base : owner.parents) { inherited_methods(inherited_methods, *base.parent_class, inherited); }
      std::map<std::string, std::vector<const function_declaration*>> inherited_exceptions;
      for (const auto& base : owner.parents) {
        inherited_exception_methods(inherited_exception_methods, base, {}, inherited_exceptions);
      }
      for (const auto& method : owner.functions) {
        const auto key = exception_signature(method, {});
        const auto matches = inherited_exceptions.find(key);
        if (matches != inherited_exceptions.end()) {
          require_cpp_exception_contract(method, matches->second, key);
        }
      }
      if (node->type == object_type::generic_definition) {
        continue;
      }
      for (const auto& method : owner.functions) {
        if (owner.supports_managed() && method.effect == function_declaration::effect_type::edit &&
            method.dispatch != function_declaration::dispatch_type::ordinary) {
          throw exception::bad_member_spec{input, "Managed edit dispatch requires an ordinary transaction-bound editor method"};
        }
        const auto key = behavior_signature(method);
        const auto matches = inherited.find(key);
        if (method.dispatch == function_declaration::dispatch_type::override_method) {
          if (matches == inherited.end() || std::none_of(matches->second.begin(), matches->second.end(),
              [](const auto* base) { return base->dispatch != function_declaration::dispatch_type::ordinary; })) {
            throw exception::bad_member_spec{input, "Override has no inherited virtual behavior contract: " + key};
          }
          for (const auto* base : matches->second) {
            if (base->return_type.get_full_name() != method.return_type.get_full_name() || base->effect != method.effect) {
              throw exception::bad_member_spec{input, "Override return type/effect does not match its inherited contract: " + key};
            }
          }
        }
        if (matches != inherited.end()) {
          for (const auto* base : matches->second) {
            if (base->dispatch != function_declaration::dispatch_type::ordinary &&
                (base->return_type.get_full_name() != method.return_type.get_full_name() || base->effect != method.effect)) {
              throw exception::bad_member_spec{input, "Inherited virtual behavior return/effect collision: " + key};
            }
          }
        }
      }
      for (const auto& [key, matches] : inherited) {
        // One ancestor overridden along a single path is valid; unrelated base occurrences are ambiguous.
        std::size_t occurrences{};
        for (const auto& base : owner.parents) {
          std::map<std::string, std::vector<const function_declaration*>> path;
          inherited_methods(inherited_methods, *base.parent_class, path);
          if (path.contains(key)) { ++occurrences; }
        }
        static_cast<void>(matches);
        if (occurrences > 1 && std::none_of(owner.functions.begin(), owner.functions.end(), [&](const auto& method) {
            return behavior_signature(method) == key && method.dispatch == function_declaration::dispatch_type::override_method;
        })) {
          throw exception::bad_member_spec{input, "Ambiguous inherited behavior requires an explicit override: " + key};
        }
      }
    }
  }
}
