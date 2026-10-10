// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// This implementation is included after the schema scanner helpers, inside parser.

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
  // Allocate one checked expression node with an original source location.
  behavior_expression make(behavior_expression::operation kind, std::string value = {}) {
    constexpr std::size_t maximum_expression_nodes = 1024;
    if (++nodes > maximum_expression_nodes) {
      throw exception::bad_member_spec{input, "Expression exceeds 1024 nodes"};
    }
    return {kind, std::move(value), {}, behavior_source_offset(input)};
  }
  // Parse constants, bound symbols, explicit conversions and parenthesized subexpressions.
  behavior_expression primary() {
    using operation = behavior_expression::operation;
    skip_whitespace_and_comment(input);
    if (++depth > 64) { throw exception::bad_member_spec{input, "Expression nesting exceeds 64"}; }
    behavior_expression result;
    if (*input == '(') {
      ++input;
      result = sum();
      skip_whitespace_and_comment(input);
      check_and_increase(input, ')');
    } else if (*input == '+' || *input == '-') {
      const auto sign = *input;
      ++input;
      result = make(sign == '+' ? operation::positive : operation::negative);
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
        if (parse_identifier_impl(input) != "pi") {
          throw exception::bad_member_spec{input, "Only math.pi is a portable constant"};
        }
        result = make(operation::pi);
      } else if (name == "to_double" && *input == '(') {
        ++input;
        result = make(operation::to_double);
        result.operands.push_back(primary());
        skip_whitespace_and_comment(input);
        check_and_increase(input, ')');
      } else {
        result = make(operation::symbol, std::move(name));
      }
    }
    --depth;
    return result;
  }
  // Multiplication binds before addition and subtraction; nodes retain exact grouping.
  behavior_expression product() {
    auto result = primary();
    skip_whitespace_and_comment(input);
    while (!input.full() && *input == '*') {
      ++input;
      auto next = make(behavior_expression::operation::multiply);
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
  skip_whitespace_and_comment(input);
  if (next_keyword(input, "expression")) {
    parse_identifier_impl(input);
    skip_whitespace_and_comment(input);
    check_and_increase(input, '(');
    const auto expression_begin = behavior_source_offset(input);
    result.expression = behavior_expression_parser{input}.sum();
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
  locate_behavior(result, source_begin, behavior_source_offset(input));
  return result;
}

// Type every pure node; integer values require an explicit exact int32/uint32 conversion.
std::string validate_behavior_expression(behavior_expression& value, const class_node& owner,
                                        const function_declaration& method) {
  using operation = behavior_expression::operation;
  if (value.kind == operation::literal || value.kind == operation::pi) { return "double"; }
  if (value.kind == operation::symbol) {
    const auto parameter = std::find_if(method.parameters.begin(), method.parameters.end(),
        [&](const auto& item) { return item.name == value.value; });
    if (parameter != method.parameters.end()) { return parameter->type.name; }
    const auto field = std::find_if(owner.member_list.begin(), owner.member_list.end(),
        [&](const auto& item) { return item.name == value.value; });
    if (field == owner.member_list.end() || field->modifier != member::modifier_type::none) {
      throw std::invalid_argument{"Unknown scalar expression symbol: " + value.value};
    }
    return field->type_name_list.front().name;
  }
  if (value.kind == operation::to_double) {
    const auto type = validate_behavior_expression(value.operands.at(0), owner, method);
    if (type != "int32" && type != "uint32") {
      throw std::invalid_argument{"to_double currently requires int32 or uint32"};
    }
    value.value = type;
    return "double";
  }
  for (auto& operand : value.operands) {
    if (validate_behavior_expression(operand, owner, method) != "double") {
      throw std::invalid_argument{"Portable arithmetic requires explicit binary64 values"};
    }
  }
  return "double";
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
  for (const auto& node : nodes) {
    if (node->type == object_type::namespace_type) {
      validate_behavior_contracts(input, static_cast<const namespace_node&>(*node).statements);
    } else if (node->type == object_type::class_type) {
      const auto& owner = static_cast<const class_node&>(*node);
      std::map<std::string, std::vector<const function_declaration*>> inherited;
      for (const auto& base : owner.parents) { inherited_methods(inherited_methods, *base.parent_class, inherited); }
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
