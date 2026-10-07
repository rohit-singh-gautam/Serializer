#include "../src/command_line.hpp"
#include <array>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
namespace {
namespace cli = rohit::serializer::cli;
using cli::command_options;
using cli::command_type;
using cli::commandline_declaration;

// Preserve the compiler's existing string-map API while adding typed declarations.
TEST(command_line, preserves_existing_map_api) {
  constexpr std::array options{
      cli::commandline_option{'i', "input", "path", "Input", false, false},
      cli::commandline_option{'l', "language", "name", "Language", true, false},
      cli::commandline_option{'h', "help", "", "Help", false, false}};
  const char* argv[]{"tool", "-i", "a b", "--language=cpp", "-l", "java", "--help"};
  const auto parsed = cli::parse(static_cast<int>(std::size(argv)), argv, options);
  EXPECT_EQ(cli::first(parsed, "input"), "a b");
  EXPECT_EQ(parsed.at("language"), (std::vector<std::string>{"cpp", "java"}));
  EXPECT_EQ(cli::first(parsed, "help"), "true");
  const char* duplicate[]{"tool", "--input=a", "--input=b"};
  EXPECT_THROW(cli::parse(static_cast<int>(std::size(duplicate)), duplicate, options),
               std::invalid_argument);
}

// Parse synthetic argv in tests only; production declarations take process argv directly.
void parse(commandline_declaration& decl, std::initializer_list<const char*> args) {
  decl.parse(static_cast<int>(args.size()), args.begin());
}

// Reusable fixture exercises every scalar type, defaults, required values and repeated lists.
commandline_declaration make_declaration() {
  return {
      command_options{{'v', "verbose", "Enable detail", command_type::boolean},
                      {'l', "log", "path", "Log file", command_type::path,
                       std::filesystem::path("default.log")}},
      {{"build",
        command_options{
            {'i', "input", "path", "Input path", command_type::path, {}, true},
            {'n', "number", "int", "Signed value", command_type::integer, std::int64_t{7}},
            {'u', "unsigned", "uint", "Unsigned value", command_type::unsigned_integer},
            {'r', "ratio", "real", "Ratio", command_type::real, 1.5},
            {'s', "text", "text", "Text", command_type::string, std::string("default"), false,
             true},
            {0, "optional", "text", "Optional text", command_type::string},
            {0, "tag", "text", "Tags", command_type::strings, std::vector<std::string>{"default"}},
            {0, "include", "path", "Include paths", command_type::paths}},
        "Build an input"},
       {"other",
        command_options{
            {0, "required", "text", "Other required value", command_type::string, {}, true}},
        "Another command"}}};
}

// Common options are recognized on both sides of the command; values are converted just once.
TEST(command_line, parses_common_options_and_all_scalar_types) {
  auto decl = make_declaration();
  parse(decl, {"tool", "--verbose", "build", "-i", "a b.cpp", "-n", "-42", "--unsigned=99",
               "--ratio=2.75", "--text=hello", "--log=chosen.log"});
  EXPECT_EQ(decl.selected_command(), "build");
  EXPECT_TRUE(decl.common().get_bool("verbose"));
  EXPECT_EQ(decl.common().get_path("log"), std::filesystem::path("chosen.log"));
  const auto& options = decl["build"];
  EXPECT_EQ(options.get_path("input"), std::filesystem::path("a b.cpp"));
  EXPECT_EQ(options.get_int("number"), -42);
  EXPECT_EQ(options.get_uint64("unsigned"), 99u);
  EXPECT_DOUBLE_EQ(options.get_double("ratio"), 2.75);
  EXPECT_EQ(options.get_string("text"), "hello");
  EXPECT_FALSE(decl["other"].has_value("required"));
  EXPECT_THROW(options.get_int("text"), std::logic_error);
  EXPECT_THROW(options.get_string("optional"), std::logic_error);
  EXPECT_THROW(options.get_string("undeclared"), std::out_of_range);
  EXPECT_THROW(decl["undeclared"], std::out_of_range);
}

// Defaults, explicit empty values and CLI presence remain distinct; list occurrences keep order.
TEST(command_line, distinguishes_defaults_presence_and_repeated_values) {
  auto decl = make_declaration();
  parse(decl, {"tool", "build", "--input=a", "--text=", "--tag=first", "--tag=second",
               "--include=one", "--include=two"});
  const auto& options = decl["build"];
  EXPECT_TRUE(options.was_provided("text"));
  EXPECT_TRUE(options.has_value("text"));
  EXPECT_EQ(options.get_string("text"), "");
  EXPECT_FALSE(options.was_provided("number"));
  EXPECT_EQ(options.get_int("number"), 7);
  EXPECT_FALSE(options.has_value("unsigned"));
  EXPECT_FALSE(decl.common().get_bool("verbose"));
  EXPECT_EQ(options.get_strings("tag"), (std::vector<std::string>{"first", "second"}));
  EXPECT_EQ(options.get_paths("include"), (std::vector<std::filesystem::path>{"one", "two"}));
}

// Invalid numeric spelling, signedness and overflow fail before values reach a handler.
TEST(command_line, rejects_invalid_and_overflowing_numbers) {
  for (const auto* value : {"--number=9223372036854775808", "--number=-9223372036854775809",
                            "--unsigned=18446744073709551616", "--unsigned=-1", "--number=1x",
                            "--ratio=nan", "--ratio=inf", "--ratio=1e999", "--number="}) {
    auto decl = make_declaration();
    EXPECT_THROW(parse(decl, {"tool", "build", "--input=a", value}), std::invalid_argument)
        << value;
  }
  auto decl = make_declaration();
  parse(decl, {"tool", "build", "--input=a", "--number=-9223372036854775808",
               "--unsigned=18446744073709551615"});
  EXPECT_EQ(decl["build"].get_int("number"), std::numeric_limits<std::int64_t>::min());
  EXPECT_EQ(decl["build"].get_uint64("unsigned"), std::numeric_limits<std::uint64_t>::max());
}

// Invalid syntax and missing required values are rejected during parsing rather than dispatch.
TEST(command_line, rejects_invalid_options_and_commands) {
  for (const auto& args : std::vector<std::vector<const char*>>{
           {"tool"},
           {"tool", "missing"},
           {"tool", "build"},
           {"tool", "--input=a", "build"},
           {"tool", "build", "--input"},
           {"tool", "build", "--input=a", "--input=b"},
           {"tool", "build", "--input=a", "--unknown"},
           {"tool", "build", "--input=a", "--verbose=true"},
           {"tool", "build", "--input=a", "--verbose", "--verbose"},
           {"tool", "build", "--input=a", "stray"},
           {"tool", "build", "--input=a", "--", "stray"}}) {
    auto decl = make_declaration();
    EXPECT_THROW(decl.parse(static_cast<int>(args.size()), args.data()), std::invalid_argument);
  }
}

// Parsing uses a candidate state; failure preserves all previously published values and selection.
TEST(command_line, reparsing_is_transactional_and_resets_defaults) {
  auto decl = make_declaration();
  parse(decl, {"tool", "--verbose", "build", "--input=old", "--number=9", "--tag=old"});
  EXPECT_THROW(parse(decl, {"tool", "build", "--input=new", "--number=bad"}),
               std::invalid_argument);
  EXPECT_EQ(decl["build"].get_path("input"), std::filesystem::path("old"));
  EXPECT_EQ(decl["build"].get_int("number"), 9);
  EXPECT_TRUE(decl.common().get_bool("verbose"));
  parse(decl, {"tool", "build", "--input=new"});
  EXPECT_EQ(decl["build"].get_int("number"), 7);
  EXPECT_EQ(decl["build"].get_strings("tag"), (std::vector<std::string>{"default"}));
  EXPECT_FALSE(decl.common().get_bool("verbose"));
  parse(decl, {"tool", "other", "--required=yes"});
  EXPECT_EQ(decl.selected_command(), "other");
  EXPECT_FALSE(decl["build"].has_value("input"));
}

// Both global and command help bypass required values and describe the selected declaration.
TEST(command_line, generates_global_and_command_help) {
  auto decl = make_declaration();
  parse(decl, {"tool", "--help"});
  EXPECT_TRUE(decl.help_requested());
  std::ostringstream global;
  decl.write_help(global);
  EXPECT_NE(global.str().find("Common options:"), std::string::npos);
  EXPECT_NE(global.str().find("build"), std::string::npos);
  parse(decl, {"tool", "build", "-h"});
  std::ostringstream local;
  decl.write_help(local);
  EXPECT_NE(local.str().find("--input <path> [required]"), std::string::npos);
  EXPECT_NE(local.str().find("[default: 7]"), std::string::npos);
  EXPECT_NE(local.str().find("[repeatable]"), std::string::npos);
  EXPECT_EQ(cli::dispatch_command(decl), 0);
  parse(decl, {"tool", "build", "--input=a"});
  EXPECT_FALSE(decl.help_requested());
}

// Only explicitly declared positional inputs or command tails are retained after '--'.
TEST(command_line, parses_typed_positionals_and_unparsed_command_tail) {
  commandline_declaration decl{
      {},
      {{"objects", command_options{{}, {"objects", command_type::paths, 1}}, {}},
       {"words", command_options{{}, {"words", command_type::strings, 1}}, {}},
       {"run", command_options{{}, {}, {"compiler", 1}}, {}}}};
  parse(decl, {"tool", "objects", "first.obj", "--", "-second.obj"});
  EXPECT_EQ(decl["objects"].positional_paths(),
            (std::vector<std::filesystem::path>{"first.obj", "-second.obj"}));
  parse(decl, {"tool", "words", "one", "two"});
  EXPECT_EQ(decl["words"].positional_strings(), (std::vector<std::string>{"one", "two"}));
  parse(decl, {"tool", "run", "--", "compiler", "--help", "-DVALUE=a b", ""});
  EXPECT_FALSE(decl.help_requested());
  EXPECT_EQ(decl["run"].command_tail(),
            (std::vector<std::string>{"compiler", "--help", "-DVALUE=a b", ""}));
  EXPECT_THROW(parse(decl, {"tool", "objects"}), std::invalid_argument);
  EXPECT_THROW(parse(decl, {"tool", "run", "--"}), std::invalid_argument);
}

// Declaration errors are deterministic and cannot silently shadow common options or aliases.
TEST(command_line, rejects_invalid_declarations) {
  EXPECT_THROW((command_options{{0, "real", "value", "", command_type::real,
                                 std::numeric_limits<double>::infinity()}}),
               std::invalid_argument);
  EXPECT_THROW((command_options{{0, "value", "", "", command_type::integer, std::string("bad")}}),
               std::invalid_argument);
  EXPECT_THROW((command_options{{0, "same", "", "", command_type::string},
                                {0, "same", "", "", command_type::string}}),
               std::invalid_argument);
  EXPECT_THROW((command_options{{'x', "one", "", "", command_type::string},
                                {'x', "two", "", "", command_type::string}}),
               std::invalid_argument);
  EXPECT_THROW((command_options{{'h', "other", "", "", command_type::string}}),
               std::invalid_argument);
  EXPECT_THROW((command_options{{0, "help", "", "", command_type::string}}), std::invalid_argument);
  EXPECT_THROW((command_options{{}, {"input", command_type::integer, 1}}), std::invalid_argument);
  EXPECT_THROW((commandline_declaration{{}, {{"same", {}, {}}, {"same", {}, {}}}}),
               std::invalid_argument);
  EXPECT_THROW((commandline_declaration{
                   command_options{{'x', "common", "", "", command_type::boolean}},
                   {{"run", command_options{{'x', "local", "", "", command_type::boolean}}, {}}}}),
               std::invalid_argument);
  EXPECT_THROW((commandline_declaration{
                   command_options{{0, "common", "", "", command_type::boolean}},
                   {{"run", command_options{{0, "common", "", "", command_type::boolean}}, {}}}}),
               std::invalid_argument);
}

// Dispatch consumes typed values only and never invokes the handler as a side effect of parsing.
TEST(command_line, dispatches_only_successfully_parsed_parameters) {
  static int calls{};
  calls = 0;
  commandline_declaration decl{
      {},
      {{"build",
        command_options{{0, "count", "number", "Count", command_type::unsigned_integer, {}, true}},
        "Build", +[](const command_options& options) {
          ++calls;
          EXPECT_EQ(options.get_uint64("count"), 42u);
          return 17;
        }}}};
  EXPECT_THROW(cli::dispatch_command(decl), std::logic_error);
  EXPECT_THROW(parse(decl, {"tool", "build", "--count=bad"}), std::invalid_argument);
  EXPECT_THROW(cli::dispatch_command(decl), std::logic_error);
  std::string token = "--count=42";
  const char* argv[]{"tool", "build", token.c_str()};
  decl.parse(static_cast<int>(std::size(argv)), argv);
  token.assign("changed after parse");
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(cli::dispatch_command(decl), 17);
  EXPECT_EQ(calls, 1);
  parse(decl, {"tool", "build", "--help"});
  EXPECT_EQ(cli::dispatch_command(decl), 0);
  EXPECT_EQ(calls, 1);
}

// Options-only applications use the same declaration API without manufacturing a command name.
TEST(command_line, supports_options_only_and_owned_declaration_strings) {
  std::string name = "input";
  commandline_declaration original{
      command_options{{0, name, "text", "Input", command_type::string, {}, true}}};
  name.assign("changed");
  auto copy = original;
  auto moved = std::move(copy);
  parse(moved, {"tool", "--input=value"});
  EXPECT_EQ(moved.common().get_string("input"), "value");
  EXPECT_TRUE(moved.selected_command().empty());
  EXPECT_FALSE(original.common().has_value("input"));
}
#ifdef _WIN32
// Native Windows paths and UTF-8 string/tail values preserve BMP characters and surrogate pairs.
TEST(command_line, preserves_native_windows_unicode) {
  commandline_declaration decl{
      {},
      {{"read",
        command_options{{0, "path", "path", "Path", command_type::path, {}, true},
                        {0, "text", "text", "Text", command_type::string},
                        {0, "include", "path", "Includes", command_type::paths}},
        "Read"},
       {"run", command_options{{}, {}, {"compiler", 1}}, {}}}};
  const wchar_t* argv[]{L"tool", L"read", L"--path=\u76ee\u5f55/\U0001f680.cpp",
                        L"--text=\u76ee\u5f55", L"--include=\U0001f680"};
  decl.parse(static_cast<int>(std::size(argv)), argv);
  EXPECT_EQ(decl["read"].get_path("path"), std::filesystem::path(L"\u76ee\u5f55/\U0001f680.cpp"));
  EXPECT_EQ(decl["read"].get_string("text"), "\xe7\x9b\xae\xe5\xbd\x95");
  EXPECT_EQ(decl["read"].get_paths("include").at(0), std::filesystem::path(L"\U0001f680"));
  const wchar_t* tail[]{L"tool", L"run", L"--", L"compiler", L"/DNAME=\u5f55"};
  decl.parse(static_cast<int>(std::size(tail)), tail);
  EXPECT_EQ(decl["run"].command_tail().at(1), "/DNAME=\xe5\xbd\x95");
}
#endif
// Required common values are checked for each command, and defaults do not imply CLI presence.
TEST(command_line, validates_required_common_options_and_short_equals) {
  commandline_declaration decl{command_options{
      {'c', "config", "path", "Configuration", command_type::path, std::filesystem::path("default"), true}},
      {{"run", command_options{{'n', "number", "int", "Number", command_type::integer}}, {}}}};
  EXPECT_THROW(parse(decl, {"tool", "run"}), std::invalid_argument);
  parse(decl, {"tool", "-c=chosen", "run", "-n=-7"});
  EXPECT_EQ(decl.common().get_path("config"), std::filesystem::path("chosen"));
  EXPECT_EQ(decl["run"].get_int("number"), -7);
  parse(decl, {"tool", "run", "--help"});
  EXPECT_TRUE(decl.help_requested());
}
} // namespace
