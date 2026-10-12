// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

// Parse bounded availability without runtime fields, arbitrary native tokens, or evaluation effects.
callable_guard parse_callable_guard(const rohit::type_check::schema_input_buffer auto& input,
    namespace_node* space) {
  if (active_language_version < language_version{1u, 17u, 0u}) {
    throw exception::bad_member_spec{input, "Callable availability requires serializer version 1.17.0"};
  }
  static_cast<void>(parse_identifier_impl(input));
  skip_whitespace_and_comment(input);
  check_and_increase(input, '(');
  using operation = callable_guard::operation;
  std::size_t nodes{}, depth{};
  const auto consume = [&](std::string_view token) {
    skip_whitespace_and_comment(input);
    if (input.remaining_buffer() < token.size() || std::string_view{
        reinterpret_cast<const char*>(input.curr()), token.size()} != token) { return false; }
    input.advance_unchecked(token.size());
    return true;
  };
  std::function<callable_guard()> disjunction, conjunction, atom;
  const auto operand = [&]() {
    skip_whitespace_and_comment(input);
    callable_guard result;
    if (!input.full() && *input >= '0' && *input <= '9') {
      result.kind = operation::integer;
      while (!input.full() && *input >= '0' && *input <= '9') {
        const auto digit = static_cast<std::uint64_t>(*input - '0');
        if (result.value > (::std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
          throw exception::bad_member_spec{input, "Availability integer overflows uint64"};
        }
        result.value = result.value * 10 + digit;
        ++input;
      }
    } else {
      result.kind = operation::symbol;
      result.name = parse_identifier_impl(input);
    }
    return result;
  };
  atom = [&]() -> callable_guard {
    if (++nodes > 128 || ++depth > 32) {
      throw exception::bad_member_spec{input, "Callable availability exceeds 128 nodes or 32 levels"};
    }
    callable_guard result;
    if (consume("!")) {
      result.kind = operation::logical_not;
      result.operands.push_back(atom());
    } else if (consume("(")) {
      result = disjunction();
      skip_whitespace_and_comment(input);
      check_and_increase(input, ')');
    } else if (next_keyword(input, "same_type")) {
      static_cast<void>(parse_identifier_impl(input));
      skip_whitespace_and_comment(input);
      check_and_increase(input, '(');
      skip_whitespace_and_comment(input);
      result.kind = operation::type_equal;
      result.left_type = type_name{parse_identifier_impl(input), space};
      skip_whitespace_and_comment(input);
      check_and_increase(input, ',');
      skip_whitespace_and_comment(input);
      result.right_type = type_name{parse_identifier_impl(input), space};
      skip_whitespace_and_comment(input);
      check_and_increase(input, ')');
    } else {
      result.operands.push_back(operand());
      if (consume("==")) { result.kind = operation::equal; }
      else if (consume("!=")) { result.kind = operation::not_equal; }
      else if (consume("<=")) { result.kind = operation::less_equal; }
      else if (consume(">=")) { result.kind = operation::greater_equal; }
      else if (consume("<")) { result.kind = operation::less; }
      else if (consume(">")) { result.kind = operation::greater; }
      else { throw exception::bad_member_spec{input, "Availability requires a dimension comparison or same_type"}; }
      result.operands.push_back(operand());
    }
    --depth;
    return result;
  };
  conjunction = [&]() {
    auto result = atom();
    while (consume("&&")) {
      if (++nodes > 128) { throw exception::bad_member_spec{input, "Callable availability exceeds 128 nodes"}; }
      callable_guard next; next.kind = operation::logical_and;
      next.operands.push_back(std::move(result)); next.operands.push_back(atom()); result = std::move(next);
    }
    return result;
  };
  disjunction = [&]() {
    auto result = conjunction();
    while (consume("||")) {
      if (++nodes > 128) { throw exception::bad_member_spec{input, "Callable availability exceeds 128 nodes"}; }
      callable_guard next; next.kind = operation::logical_or;
      next.operands.push_back(std::move(result)); next.operands.push_back(conjunction()); result = std::move(next);
    }
    return result;
  };
  auto result = disjunction();
  skip_whitespace_and_comment(input);
  check_and_increase(input, ')');
  skip_whitespace_and_comment(input);
  return result;
}
