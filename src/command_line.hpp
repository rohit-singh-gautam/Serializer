// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from Chaturanga's commandline option descriptors and short/long lookup.
// See docs/command_line.md for upstream provenance and the adaptation scope.
#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rohit::serializer::cli {
struct commandline_option {
  char short_name{};
  std::string_view name{};
  std::string_view value_help{};
  std::string_view help{};
  bool repeatable{};
  bool allow_empty{};
};

// Compare ASCII descriptor spellings directly against narrow or wide argv, without conversion.
template <typename Char>
constexpr bool matches_ascii(std::basic_string_view<Char> value,
                             std::string_view expected) noexcept {
  if (value.size() != expected.size()) {
    return false;
  }
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] != static_cast<Char>(expected[index])) {
      return false;
    }
  }
  return true;
}

// Preserve narrow diagnostics and ASCII wide option names; do not narrow Unicode lossily.
template <typename Char> std::string diagnostic_name(std::basic_string_view<Char> value) {
  if constexpr (std::is_same_v<Char, char>) {
    return std::string{value};
  } else {
    std::string output;
    for (const auto character : value) {
      if (character < 0 || character > 0x7f) {
        return "<non-ASCII argument>";
      }
      output.push_back(static_cast<char>(character));
    }
    return output;
  }
}

using arguments = std::map<std::string, std::vector<std::string>, std::less<>>;

// Borrow the first option value; throw out_of_range for an absent key or empty value list.
// The reference remains valid until that option is modified or the arguments are destroyed.
inline const std::string& first(const arguments& values, std::string_view name) {
  const auto option = values.find(name);
  if (option == values.end()) {
    throw std::out_of_range{"Missing argument: " + std::string{name}};
  }
  return option->second.at(0);
}

// Parse into caller-owned indexed storage; callbacks borrow argv only during this call.
// The callback receives the descriptor index and value. Positional/tail handling is opt-in.
template <typename Char, typename Store, typename Positional, typename Tail>
void parse_into(int argc, const Char* const* argv, std::span<const commandline_option> options,
                Store&& store, Positional&& positional, Tail&& tail, bool allow_positionals = false,
                bool allow_tail = false) {
  for (int index = 1; index < argc; ++index) {
    const std::basic_string_view<Char> argument{argv[index]};
    if (allow_tail && matches_ascii(argument, "--")) {
      for (++index; index < argc; ++index) {
        tail(std::basic_string_view<Char>{argv[index]});
      }
      return;
    }
    if (allow_positionals && !argument.starts_with(static_cast<Char>('-'))) {
      positional(argument);
      continue;
    }
    const bool long_name = argument.size() >= 2 && argument[0] == static_cast<Char>('-') &&
                           argument[1] == static_cast<Char>('-');
    const auto equal = argument.find(static_cast<Char>('='));
    const auto name = argument.substr(0, equal);
    const auto option = std::find_if(
        options.begin(), options.end(), [name, long_name](const commandline_option& candidate) {
          return long_name ? matches_ascii(name.substr(2), candidate.name)
                           : name.size() == 2 && name.front() == static_cast<Char>('-') &&
                                 name.back() == static_cast<Char>(candidate.short_name);
        });
    if (option == options.end()) {
      throw std::invalid_argument{"Unknown argument: " + diagnostic_name(argument)};
    }
    std::basic_string_view<Char> value{};
    if (option->value_help.empty()) {
      if (equal != std::basic_string_view<Char>::npos) {
        throw std::invalid_argument{"Flag does not accept a value: " + diagnostic_name(name)};
      }
      static constexpr Char present[]{'t', 'r', 'u', 'e', 0};
      value = present;
    } else if (long_name && equal != std::basic_string_view<Char>::npos) {
      value = argument.substr(equal + 1);
    } else {
      if (equal != std::basic_string_view<Char>::npos || index + 1 == argc ||
          std::basic_string_view<Char>{argv[index + 1]}.starts_with(static_cast<Char>('-'))) {
        throw std::invalid_argument{"Missing value for argument: " + diagnostic_name(name)};
      }
      value = argv[++index];
    }
    if (value.empty() && !option->allow_empty) {
      throw std::invalid_argument{"Empty value for argument: " + diagnostic_name(name)};
    }
    store(static_cast<std::size_t>(option - options.begin()), value);
  }
}

// Preserve the owning map API for existing callers; indexed users call parse_into directly.
inline arguments parse(int argc, const char* const* argv,
                       std::span<const commandline_option> options) {
  arguments result{};
  parse_into(
      argc, argv, options,
      [&](std::size_t index, std::string_view value) {
        const auto& option = options[index];
        auto& values = result[std::string{option.name}];
        if (!option.repeatable && !values.empty()) {
          throw std::invalid_argument{"Repeated argument: --" + std::string{option.name}};
        }
        values.emplace_back(value);
      },
      [](std::string_view) {}, [](std::string_view) {});
  return result;
}

// Build help from the same descriptors used for parsing so option names cannot drift.
inline std::string usage(std::span<const commandline_option> options) {
  std::string result{"Usage: serializer --input <schema.serializer> [options]\nOptions:\n"};
  for (const auto& option : options) {
    result += "  ";
    if (option.short_name != '\0') {
      result += '-';
      result += option.short_name;
      result += ", ";
    }
    result += "--";
    result += option.name;
    if (!option.value_help.empty()) {
      result += " <";
      result += option.value_help;
      result += '>';
    }
    result += "\n      ";
    result += option.help;
    result += '\n';
  }
  return result;
}
// Values belong to the declaration, including the active alternative's lifetime.
enum class command_type { boolean, integer, unsigned_integer, real, string, path, strings, paths };
using option_value = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double,
                                  std::string, std::filesystem::path, std::vector<std::string>,
                                  std::vector<std::filesystem::path>>;

struct option_declaration {
  char short_name{};
  std::string name;
  std::string value_help;
  std::string help;
  command_type type{command_type::string};
  option_value default_value{};
  bool required{};
  bool allow_empty{};

  // Declare a typed value, optional default and occurrence/empty-value requirements.
  option_declaration(char alias, std::string option_name, std::string placeholder,
                     std::string description, command_type value_type, option_value initial = {},
                     bool is_required = false, bool permits_empty = false)
      : short_name(alias), name(std::move(option_name)), value_help(std::move(placeholder)),
        help(std::move(description)), type(value_type), default_value(std::move(initial)),
        required(is_required), allow_empty(permits_empty) {}

  // Flags omit the value placeholder; other types must use the full declaration form.
  option_declaration(char alias, std::string option_name, std::string description,
                     command_type value_type)
      : option_declaration(alias, std::move(option_name), "", std::move(description), value_type) {
    if (type != command_type::boolean) {
      throw std::invalid_argument("Only boolean declarations may omit the value placeholder");
    }
  }
};

// An empty name disables positional inputs; only strings and paths are valid positional types.
struct positional_declaration {
  std::string name;
  command_type type{command_type::paths};
  std::size_t minimum_count{};
};
// An empty name disables command tails; enabled tails preserve every token after '--'.
struct tail_declaration {
  std::string name;
  std::size_t minimum_count{};
};

namespace detail {
// Reject names that collide with parser syntax or cannot match ASCII option spellings.
inline void validate_name(std::string_view name) {
  if (name.empty() || name.front() == '-' || name == "help" ||
      !std::all_of(name.begin(), name.end(), [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
               ch == '-' || ch == '_';
      })) {
    throw std::invalid_argument("Invalid or reserved declaration name: " + std::string(name));
  }
}
// Preserve byte-oriented UTF-8 input; convert native Windows strings only when retaining them.
template <typename Char> std::string argument_text(std::basic_string_view<Char> value) {
  if constexpr (std::is_same_v<Char, char>) {
    return std::string(value);
  } else {
    const auto utf8 = std::filesystem::path(value).u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
  }
}
// Build native paths without interpreting narrow UTF-8 input in the Windows ANSI code page.
template <typename Char> std::filesystem::path argument_path(std::basic_string_view<Char> value) {
  if constexpr (std::is_same_v<Char, char>) {
    return std::filesystem::path(
        std::u8string_view(reinterpret_cast<const char8_t*>(value.data()), value.size()));
  } else {
    return std::filesystem::path(value);
  }
}
// Require a complete, representable decimal number; floating values must also be finite.
template <typename Number> Number parse_number(std::string_view text, std::string_view name) {
  if (text.empty()) {
    throw std::invalid_argument("Empty number for --" + std::string(name));
  }
  Number value{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    throw std::invalid_argument("Invalid or out-of-range number for --" + std::string(name));
  }
  if constexpr (std::is_floating_point_v<Number>) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("Non-finite number for --" + std::string(name));
    }
  }
  return value;
}
// Match explicit defaults to the declared type; monostate means no supplied default.
inline bool matches_type(const option_value& value, command_type type) {
  switch (type) {
  case command_type::boolean:
    return std::holds_alternative<bool>(value);
  case command_type::integer:
    return std::holds_alternative<std::int64_t>(value);
  case command_type::unsigned_integer:
    return std::holds_alternative<std::uint64_t>(value);
  case command_type::real:
    return std::holds_alternative<double>(value);
  case command_type::string:
    return std::holds_alternative<std::string>(value);
  case command_type::path:
    return std::holds_alternative<std::filesystem::path>(value);
  case command_type::strings:
    return std::holds_alternative<std::vector<std::string>>(value);
  case command_type::paths:
    return std::holds_alternative<std::vector<std::filesystem::path>>(value);
  }
  return false;
}
// List types are the only options that accept multiple occurrences.
inline bool is_repeated(command_type type) {
  return type == command_type::strings || type == command_type::paths;
}
// Give flags and lists natural empty states; other optional values remain absent.
inline option_value initial_value(const option_declaration& option) {
  if (!std::holds_alternative<std::monostate>(option.default_value)) {
    return option.default_value;
  }
  switch (option.type) {
  case command_type::boolean:
    return false;
  case command_type::strings:
    return std::vector<std::string>{};
  case command_type::paths:
    return std::vector<std::filesystem::path>{};
  default:
    return std::monostate{};
  }
}
} // namespace detail

class commandline_declaration;

// Own declarations and typed results. Returned references last until assignment, parsing or
// destruction. Concurrent const reads are safe; parsing requires exclusive access to the owning
// declaration.
class command_options {
public:
  // Copy initializer data so temporary strings and initializer lists never become dangling views.
  command_options(std::initializer_list<option_declaration> declarations = {},
                  positional_declaration positional = {}, tail_declaration tail = {})
      : options_{}, positional_(std::move(positional)), tail_(std::move(tail)),
        positional_paths_{}, positional_strings_{}, command_tail_{} {
    if ((!positional_.name.empty() && positional_.type != command_type::strings &&
         positional_.type != command_type::paths) ||
        (positional_.name.empty() && positional_.minimum_count != 0) ||
        (tail_.name.empty() && tail_.minimum_count != 0)) {
      throw std::invalid_argument("Invalid positional or command-tail declaration");
    }
    std::string aliases;
    for (const auto& option : declarations) {
      detail::validate_name(option.name);
      if (option.short_name != 0) {
        const auto ch = static_cast<unsigned char>(option.short_name);
        if (ch <= ' ' || ch >= 0x7f || ch == '-' || ch == '=' || ch == 'h' ||
            aliases.find(option.short_name) != std::string::npos) {
          throw std::invalid_argument("Invalid, reserved or duplicate short option");
        }
        aliases.push_back(option.short_name);
      }
      if (!std::holds_alternative<std::monostate>(option.default_value) &&
          !detail::matches_type(option.default_value, option.type)) {
        throw std::invalid_argument("Default type differs for --" + option.name);
      }
      if (const auto* real = std::get_if<double>(&option.default_value);
          real && !std::isfinite(*real)) {
        throw std::invalid_argument("Non-finite default for --" + option.name);
      }
      if (!options_.emplace(option.name, option_state{option, detail::initial_value(option)})
               .second) {
        throw std::invalid_argument("Duplicate option declaration: --" + option.name);
      }
    }
  }
  // Retrieve a signed 64-bit integer without parsing or coercing another stored type.
  std::int64_t get_int(std::string_view name) const { return get<std::int64_t>(name); }
  // Retrieve an unsigned 64-bit integer without narrowing.
  std::uint64_t get_uint64(std::string_view name) const { return get<std::uint64_t>(name); }
  // Retrieve a finite floating-point value parsed earlier.
  double get_double(std::string_view name) const { return get<double>(name); }
  // Retrieve a flag; omitted flags default to false unless explicitly configured otherwise.
  bool get_bool(std::string_view name) const { return get<bool>(name); }
  // Borrow an owned UTF-8 string without copying it.
  const std::string& get_string(std::string_view name) const { return get<std::string>(name); }
  // Borrow a native filesystem path without another encoding conversion.
  const std::filesystem::path& get_path(std::string_view name) const {
    return get<std::filesystem::path>(name);
  }
  // Borrow repeated string values in their command-line order.
  const std::vector<std::string>& get_strings(std::string_view name) const {
    return get<std::vector<std::string>>(name);
  }
  // Borrow repeated native paths in their command-line order.
  const std::vector<std::filesystem::path>& get_paths(std::string_view name) const {
    return get<std::vector<std::filesystem::path>>(name);
  }
  // Distinguish absent optional values from defaults and explicit empty strings.
  bool has_value(std::string_view name) const {
    return !std::holds_alternative<std::monostate>(find(name).value);
  }
  // Report explicit CLI presence independently of defaults, including false/empty defaults.
  bool was_provided(std::string_view name) const { return find(name).provided; }
  // Borrow typed positional paths; fail if this declaration does not accept them.
  const std::vector<std::filesystem::path>& positional_paths() const {
    if (positional_.name.empty() || positional_.type != command_type::paths) {
      throw std::logic_error("Positional paths are not declared");
    }
    return positional_paths_;
  }
  // Borrow positional strings; fail if this declaration does not accept them.
  const std::vector<std::string>& positional_strings() const {
    if (positional_.name.empty() || positional_.type != command_type::strings) {
      throw std::logic_error("Positional strings are not declared");
    }
    return positional_strings_;
  }
  // Borrow the unparsed compiler/process command tail in UTF-8.
  const std::vector<std::string>& command_tail() const { return command_tail_; }

private:
  friend class commandline_declaration;
  struct option_state {
    option_declaration declaration;
    option_value value;
    bool provided{};
  };
  std::map<std::string, option_state, std::less<>> options_;
  positional_declaration positional_;
  tail_declaration tail_;
  std::vector<std::filesystem::path> positional_paths_;
  std::vector<std::string> positional_strings_;
  std::vector<std::string> command_tail_;

  // Read-only lookup never creates a missing declaration.
  const option_state& find(std::string_view name) const {
    const auto found = options_.find(name);
    if (found == options_.end()) {
      throw std::out_of_range("Unknown option: " + std::string(name));
    }
    return found->second;
  }
  // Check absence and exact stored type before returning a reference.
  template <typename Value> const Value& get(std::string_view name) const {
    const auto& value = find(name).value;
    if (std::holds_alternative<std::monostate>(value)) {
      throw std::logic_error("Option has no value: --" + std::string(name));
    }
    const auto* result = std::get_if<Value>(&value);
    if (!result) {
      throw std::logic_error("Wrong getter type for --" + std::string(name));
    }
    return *result;
  }
  // Restore declared defaults before every candidate parse.
  void reset() {
    for (auto& [name, option] : options_) {
      option.value = detail::initial_value(option.declaration);
      option.provided = false;
    }
    positional_paths_.clear();
    positional_strings_.clear();
    command_tail_.clear();
  }
  // Find either a long spelling or a single-character alias in this option scope.
  template <typename Char> option_state* lookup(std::basic_string_view<Char> name, bool long_name) {
    for (auto& [key, option] : options_) {
      if (long_name ? matches_ascii(name, key)
                    : name.size() == 1 && option.declaration.short_name != 0 &&
                          name[0] == static_cast<Char>(option.declaration.short_name)) {
        return &option;
      }
    }
    return nullptr;
  }
  // Store one validated token; explicitly supplied lists replace defaults on their first
  // occurrence.
  template <typename Char>
  static void store(option_state& state, std::basic_string_view<Char> token) {
    const auto& option = state.declaration;
    if (state.provided && !detail::is_repeated(option.type)) {
      throw std::invalid_argument("Repeated argument: --" + option.name);
    }
    if (token.empty() && !option.allow_empty && option.type != command_type::boolean) {
      throw std::invalid_argument("Empty value for --" + option.name);
    }
    switch (option.type) {
    case command_type::boolean:
      state.value = true;
      break;
    case command_type::integer:
      state.value = detail::parse_number<std::int64_t>(detail::argument_text(token), option.name);
      break;
    case command_type::unsigned_integer:
      state.value = detail::parse_number<std::uint64_t>(detail::argument_text(token), option.name);
      break;
    case command_type::real:
      state.value = detail::parse_number<double>(detail::argument_text(token), option.name);
      break;
    case command_type::string:
      state.value = detail::argument_text(token);
      break;
    case command_type::path:
      state.value = detail::argument_path(token);
      break;
    case command_type::strings:
      if (!state.provided) {
        state.value = std::vector<std::string>{};
      }
      std::get<std::vector<std::string>>(state.value).push_back(detail::argument_text(token));
      break;
    case command_type::paths:
      if (!state.provided) {
        state.value = std::vector<std::filesystem::path>{};
      }
      std::get<std::vector<std::filesystem::path>>(state.value)
          .push_back(detail::argument_path(token));
      break;
    }
    state.provided = true;
  }
  // Append a positional token only when explicitly enabled for the active command.
  template <typename Char> void append_positional(std::basic_string_view<Char> value) {
    if (positional_.name.empty()) {
      throw std::invalid_argument("Unexpected positional argument: " + diagnostic_name(value));
    }
    if (value.empty()) {
      throw std::invalid_argument("Empty positional argument");
    }
    if (positional_.type == command_type::paths) {
      positional_paths_.push_back(detail::argument_path(value));
    } else {
      positional_strings_.push_back(detail::argument_text(value));
    }
  }
  // Enforce required occurrences and declared input counts before any handler runs.
  void validate_values() const {
    for (const auto& [name, option] : options_) {
      if (option.declaration.required && !option.provided) {
        throw std::invalid_argument("Missing required option: --" + name);
      }
    }
    if (positional_paths_.size() + positional_strings_.size() < positional_.minimum_count) {
      throw std::invalid_argument("Missing positional inputs: " + positional_.name);
    }
    if (command_tail_.size() < tail_.minimum_count) {
      throw std::invalid_argument("Missing command after '--': " + tail_.name);
    }
  }
  // Print option descriptions from the same definitions used for conversion and validation.
  void write_help(std::ostream& output) const {
    for (const auto& [name, state] : options_) {
      const auto& option = state.declaration;
      output << "  ";
      if (option.short_name) {
        output << '-' << option.short_name << ", ";
      }
      output << "--" << name;
      if (option.type != command_type::boolean) {
        output << " <" << (option.value_help.empty() ? "value" : option.value_help) << '>';
      }
      if (option.required) {
        output << " [required]";
      }
      if (detail::is_repeated(option.type)) {
        output << " [repeatable]";
      }
      if (!std::holds_alternative<std::monostate>(option.default_value)) {
        output << " [default: ";
        std::visit(
            [&](const auto& value) {
              using Value = std::decay_t<decltype(value)>;
              if constexpr (std::is_same_v<Value, std::filesystem::path>) {
                const auto utf8 = value.u8string();
                output << std::string_view(reinterpret_cast<const char*>(utf8.data()), utf8.size());
              } else if constexpr (std::is_same_v<Value, bool>) {
                output << (value ? "true" : "false");
              } else if constexpr (std::is_same_v<Value, std::vector<std::string>> ||
                                   std::is_same_v<Value, std::vector<std::filesystem::path>>) {
                output << value.size() << " values";
              } else if constexpr (!std::is_same_v<Value, std::monostate>) {
                output << value;
              }
            },
            option.default_value);
        output << ']';
      }
      output << "\n      " << option.help << '\n';
    }
    if (!positional_.name.empty()) {
      output << "  <" << positional_.name << ">... [minimum: " << positional_.minimum_count
             << "]\n";
    }
    if (!tail_.name.empty()) {
      output << "  -- <" << tail_.name << ">... [minimum: " << tail_.minimum_count << "]\n";
    }
  }
};

struct command_entry {
  std::string name;
  command_options options;
  std::string help;
  int (*handler)(const command_options&){};
};

// One declaration owns common options, subcommands and their parsed values; no argv storage
// escapes.
class commandline_declaration {
public:
  // Reject duplicate commands and common/command option collisions when constructing the
  // declaration.
  commandline_declaration(command_options common = {},
                          std::initializer_list<command_entry> commands = {})
      : common_(std::move(common)), commands_(commands), selected_{} {
    if (!commands_.empty() && (!common_.positional_.name.empty() || !common_.tail_.name.empty())) {
      throw std::invalid_argument("Common positional inputs conflict with subcommand selection");
    }
    for (std::size_t index = 0; index < commands_.size(); ++index) {
      const auto& command = commands_[index];
      detail::validate_name(command.name);
      for (std::size_t previous = 0; previous < index; ++previous) {
        if (commands_[previous].name == command.name) {
          throw std::invalid_argument("Duplicate command: " + command.name);
        }
      }
      for (const auto& [name, option] : command.options.options_) {
        for (const auto& [common_name, common_option] : common_.options_) {
          if (name == common_name ||
              (option.declaration.short_name != 0 &&
               option.declaration.short_name == common_option.declaration.short_name)) {
            throw std::invalid_argument("Common option collides with command: " + command.name);
          }
        }
      }
    }
  }
  // Parse main/wmain argv directly. Failed parsing preserves the previous successful state.
  // Successful reparsing resets all defaults and invalidates previously borrowed value references.
  template <typename Char> void parse(int argc, const Char* const* argv) {
    auto candidate = *this;
    candidate.common_.reset();
    for (auto& command : candidate.commands_) {
      command.options.reset();
    }
    candidate.selected_.reset();
    candidate.help_ = false;
    candidate.parsed_ = false;
    candidate.program_ =
        argc > 0 ? detail::argument_text(std::basic_string_view<Char>(argv[0])) : "tool";
    candidate.read(argc, argv);
    candidate.parsed_ = true;
    *this = std::move(candidate);
  }
  // Borrow declared command options; unknown names fail without creating a new command.
  const command_options& operator[](std::string_view name) const {
    const auto found = std::find_if(commands_.begin(), commands_.end(),
                                    [&](const auto& command) { return command.name == name; });
    if (found == commands_.end()) {
      throw std::out_of_range("Unknown command: " + std::string(name));
    }
    return found->options;
  }
  // Borrow common values, including options supplied before or after the selected command.
  const command_options& common() const noexcept { return common_; }
  // Return the selected command name, or an empty view for an options-only declaration/global help.
  std::string_view selected_command() const noexcept {
    return selected_ ? std::string_view(commands_[*selected_].name) : std::string_view{};
  }
  // Report global or command help requested by the last successful parse.
  bool help_requested() const noexcept { return help_; }
  // Render global or selected-command help without invoking handlers or requiring missing values.
  void write_help(std::ostream& output) const {
    output << "Usage: " << program_;
    if (selected_) {
      output << ' ' << selected_command();
    } else if (!commands_.empty()) {
      output << " <command>";
    }
    output << " [options]\n";
    if (!common_.options_.empty()) {
      output << "Common options:\n";
    }
    common_.write_help(output);
    if (selected_) {
      const auto& command = commands_[*selected_];
      output << command.help << "\nOptions:\n";
      command.options.write_help(output);
    } else if (!commands_.empty()) {
      output << "Commands:\n";
      for (const auto& command : commands_) {
        output << "  " << command.name << "  " << command.help << '\n';
      }
    }
    output << "  -h, --help\n      Show help\n";
    if (!output) {
      throw std::runtime_error("Cannot write command help");
    }
  }
  // Invoke only the handler selected by a successful parse, passing its typed parameters directly.
  int dispatch() const {
    if (!parsed_) {
      throw std::logic_error("Parse before dispatching a command");
    }
    if (help_) {
      return 0;
    }
    if (!selected_ || !commands_[*selected_].handler) {
      throw std::logic_error("Selected command has no handler");
    }
    const auto& command = commands_[*selected_];
    return command.handler(command.options);
  }

private:
  command_options common_;
  std::vector<command_entry> commands_;
  std::optional<std::size_t> selected_;
  std::string program_{"tool"};
  bool help_{};
  bool parsed_{};

  // Interpret tokens against common and selected-command declarations, retaining only typed values.
  template <typename Char> void read(int argc, const Char* const* argv) {
    for (int index = 1; index < argc; ++index) {
      const std::basic_string_view<Char> token(argv[index]);
      auto& active = selected_ ? commands_[*selected_].options : common_;
      if (matches_ascii(token, "--help") || matches_ascii(token, "-h")) {
        help_ = true;
        continue;
      }
      if (matches_ascii(token, "--")) {
        if (!selected_ && !commands_.empty()) {
          throw std::invalid_argument("Missing command before '--'");
        }
        if (active.tail_.name.empty() && active.positional_.name.empty()) {
          throw std::invalid_argument("Command does not accept arguments after '--'");
        }
        for (++index; index < argc; ++index) {
          const std::basic_string_view<Char> value(argv[index]);
          if (!active.tail_.name.empty()) {
            active.command_tail_.push_back(detail::argument_text(value));
          } else {
            active.append_positional(value);
          }
        }
        break;
      }
      if (token.starts_with(static_cast<Char>('-'))) {
        const bool long_name = token.size() >= 2 && token[1] == static_cast<Char>('-');
        const auto equal = token.find(static_cast<Char>('='));
        const auto spelling = token.substr(0, equal);
        const auto name = spelling.substr(long_name ? 2 : 1);
        auto* option = common_.lookup(name, long_name);
        if (!option && selected_) {
          option = active.lookup(name, long_name);
        }
        if (!option) {
          throw std::invalid_argument("Unknown argument: " + diagnostic_name(token));
        }
        std::basic_string_view<Char> value;
        if (option->declaration.type == command_type::boolean) {
          if (equal != token.npos) {
            throw std::invalid_argument("Flag does not accept a value: " +
                                        diagnostic_name(spelling));
          }
        } else if (equal != token.npos) {
          value = token.substr(equal + 1);
        } else {
          if (index + 1 == argc) {
            throw std::invalid_argument("Missing value: " + diagnostic_name(spelling));
          }
          value = argv[++index];
          const auto type = option->declaration.type;
          if (value.starts_with(static_cast<Char>('-')) && type != command_type::integer &&
              type != command_type::unsigned_integer && type != command_type::real) {
            throw std::invalid_argument("Missing value: " + diagnostic_name(spelling));
          }
        }
        command_options::store(*option, value);
      } else if (!selected_ && !commands_.empty()) {
        for (std::size_t candidate = 0; candidate < commands_.size(); ++candidate) {
          if (matches_ascii(token, commands_[candidate].name)) {
            selected_ = candidate;
            break;
          }
        }
        if (!selected_) {
          throw std::invalid_argument("Unknown command: " + diagnostic_name(token));
        }
      } else {
        active.append_positional(token);
      }
    }
    if (!help_) {
      if (!commands_.empty() && !selected_) {
        throw std::invalid_argument("Missing command; use --help");
      }
      common_.validate_values();
      if (selected_) {
        commands_[*selected_].options.validate_values();
      }
    }
  }
};

// Dispatch contains no argument parsing; the declaration already owns the selected typed values.
inline int dispatch_command(const commandline_declaration& declaration) {
  return declaration.dispatch();
}

} // namespace rohit::serializer::cli
