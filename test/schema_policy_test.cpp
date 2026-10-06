#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include "../src/schema_version.hpp"

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace schema = rohit::serializer;
constexpr std::string_view release_catalog =
    "releases { 8 { \"2024-10-01\" }; 9 { \"2025-04-01\" }; 10 { \"2026-10-01\" }; } ";

// Pin policy evaluation independently of the test host's clock and timezone.
schema::parser::parse_options options(std::string date = "2026-10-06") {
  schema::parser::parse_options result{};
  result.version_policy_as_of = std::move(date);
  return result;
}

// Parse one owning model and retain the resolved compiler metadata for assertions.
auto parse(std::string_view text, const schema::parser::parse_options& settings = options()) {
  auto input = rohit::make_constant_stream(text.data(), text.size());
  return schema::parser::parse(input, false, settings);
}

// Build a policy fixture whose three historical layouts remain declared.
std::string model(std::string_view policy, std::string_view outside = {}) {
  return "class record stable_ids { public version { 10 } " + std::string{release_catalog} +
         "policy { " + std::string{policy} + " } " + std::string{outside} +
         "; public uint64 id (2); created(9) public bool enabled (3) { true }; "
         "obsolete(10) public string old_name (4); "
         "created(10) replaced(old_name) public string name (5); }";
}

// Return the compiler's folded minimum instead of inspecting language-dependent literal spellings.
std::string floor(std::string_view text,
                  const schema::parser::parse_options& settings = options()) {
  const auto nodes = parse(text, settings);
  return schema::schema_version::minimum(
      *static_cast<const schema::class_node&>(*nodes.front()).version_member());
}

// Each leaf resolves once, with inclusive cutoffs and release-entry counts rather than arithmetic IDs.
TEST(schema_policy, folds_supported_leaves) {
  const std::pair<std::string_view, std::string_view> cases[] = {
      {"max_age { 2 years };", "9"},
      {"max_age { 18 months };", "10"},
      {"max_age { 19 months };", "9"},
      {"max_age { 80 weeks };", "9"},
      {"max_age { 5 days };", "10"},
      {"max_age { 4294967295 years };", "8"},
      {"keep_last { 3 };", "8"},
      {"keep_last { 1 };", "10"},
      {"keep_last { 100 };", "8"},
      {"released_since { \"2024-10-01\" };", "8"},
      {"released_since { \"2025-04-01\" };", "9"},
      {"expires_on { \"2026-10-06\" };", "10"},
      {"expires_on { \"2026-10-07\" };", "8"},
      {"compatibility { 8 };", "8"}};
  for (const auto& [policy, expected] : cases) {
    SCOPED_TRACE(policy);
    EXPECT_EQ(floor(model(policy)), expected);
  }
}

// Nested any/all trees combine allow conditions; a compatibility leaf relaxes only its own group.
TEST(schema_policy, composes_nested_allow_conditions) {
  EXPECT_EQ(floor(model("any { max_age { 2 years }; keep_last { 3 }; };")), "8");
  EXPECT_EQ(floor(model("all { max_age { 2 years }; keep_last { 3 }; };")), "9");
  EXPECT_EQ(
      floor(model("all { keep_last { 2 }; any { max_age { 2 years }; compatibility { 8 }; }; };")),
      "9");
  EXPECT_EQ(
      floor(model("any { keep_last { 2 }; all { max_age { 2 years }; compatibility { 10 }; }; };")),
      "9");
  EXPECT_EQ(floor(model("any { compatibility { 8 }; compatibility { 9 }; };")), "8");
  EXPECT_EQ(floor(model("all { compatibility { 8 }; compatibility { 9 }; };")), "9");
  EXPECT_EQ(floor(model("all { max_age { 2 years }; keep_last { 1 }; };", "compatibility { 8 }")),
            "8");
  EXPECT_EQ(floor("class record { public version { 10 } policy { compatibility { 8 }; }; }"), "8");
}

// Policies supply a floor only when declared; a catalog alone keeps the original strict default.
TEST(schema_policy, retains_defaults_and_counts_gapped_releases) {
  EXPECT_EQ(floor("class record { public version { 10 } " + std::string{release_catalog} + "; }"),
            "10");
  EXPECT_EQ(floor("class record { public version { 10 } releases { "
                  "1 { \"2020-01-01\" }; 4 { \"2021-01-01\" }; 8 { \"2024-01-01\" }; "
                  "10 { \"2026-01-01\" }; } policy { keep_last { 3 }; }; }"),
            "4");
}

// Calendar years/months clamp February and month ends, while fixed day durations retain exact days.
TEST(schema_policy, clamps_calendar_boundaries) {
  constexpr std::string_view source =
      "class record { public version { 3 } releases { "
      "1 { \"2023-02-28\" }; 2 { \"2023-03-01\" }; 3 { \"2024-02-29\" }; } policy { ";
  EXPECT_EQ(floor(std::string{source} + "max_age { 1 year }; }; }", options("2024-02-29")), "1");
  EXPECT_EQ(floor(std::string{source} + "max_age { 365 days }; }; }", options("2024-02-29")), "2");
  EXPECT_EQ(floor(std::string{source} + "max_age { 1 month }; }; }", options("2023-03-31")), "1");
}

// Expired current releases remain readable, with deterministic warnings and optional promotion.
TEST(schema_policy, warns_without_expiring_current) {
  std::vector<std::string> warnings{};
  std::vector<std::string> information{};
  auto settings = options("2030-10-06");
  settings.warning = [&](std::string_view message) { warnings.emplace_back(message); };
  settings.information = [&](std::string_view message) { information.emplace_back(message); };
  EXPECT_EQ(floor(model("max_age { 2 years };"), settings), "10");
  ASSERT_EQ(warnings.size(), 1u);
  EXPECT_NE(warnings.front().find("current version remains readable"), std::string::npos);
  EXPECT_NE(information.front().find("2030-10-06, minimum 10"), std::string::npos);
  EXPECT_EQ(floor(model("max_age { 2 years };", "compatibility { 8 }"), settings), "8");
  settings.version_policy_warnings_as_errors = true;
  EXPECT_THROW(parse(model("max_age { 2 years };"), settings), std::invalid_argument);
}

// Every version type uses its declared comparison precision, including maximum uint64 and dotted order.
TEST(schema_policy, preserves_all_discriminator_types) {
  const std::array<std::array<std::string_view, 3>, 9> cases{
      {{{"uint8", "8", "10"}},
       {{"uint16", "300", "400"}},
       {{"uint32", "70000", "80000"}},
       {{"uint64", "18446744073709551614", "18446744073709551615"}},
       {{"float", "0.05", "0.1"}},
       {{"double", "1.5", "2.5"}},
       {{"version2", "\"1.2\"", "\"1.10\""}},
       {{"version3", "\"1.2.0\"", "\"1.10.0\""}},
       {{"version4", "\"1.2.0.0\"", "\"1.10.0.4\""}}}};
  for (const auto& item : cases) {
    const auto source = "class record { protected version " + std::string{item[0]} + " ver (6) { " +
                        std::string{item[2]} + " } releases { " + std::string{item[1]} +
                        " { \"2024-01-01\" }; " + std::string{item[2]} +
                        " { \"2026-01-01\" }; } policy { keep_last { 2 }; }; }";
    SCOPED_TRACE(source);
    EXPECT_EQ(floor(source), item[1]);
  }
}

// Folded policies produce byte-for-byte identical code to equivalent explicit compatibility in every backend.
TEST(schema_policy, adds_no_generated_release_processing) {
  const auto policy = parse(model("any { max_age { 2 years }; keep_last { 3 }; };"));
  auto literal_source = model("compatibility { 8 };");
  const auto begin = literal_source.find(release_catalog);
  literal_source.erase(begin, release_catalog.size());
  const auto expression = literal_source.find("policy {");
  const auto expression_end = literal_source.find(" } ", expression);
  literal_source.replace(expression, expression_end + 3 - expression, "compatibility { 8 } ");
  const auto literal = parse(literal_source);
  schema::writer::cpp_options cpp{};
  cpp.format = false;
  EXPECT_EQ(schema::writer::cpp::generate(policy, cpp),
            schema::writer::cpp::generate(literal, cpp));
  EXPECT_EQ(schema::writer::java::generate(policy, "Schema"),
            schema::writer::java::generate(literal, "Schema"));
  for (const auto language :
       {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    SCOPED_TRACE(language);
    EXPECT_EQ(schema::writer::portable::generate(policy, language, "Schema"),
              schema::writer::portable::generate(literal, language, "Schema"));
  }
}

// Invalid reference dates fail before file loading or stream parsing can publish a contract.
TEST(schema_policy, rejects_invalid_reference_dates) {
  for (const auto date :
       {"2026-2-01", "2026-02-29", "2026-13-01", "0000-01-01", "10000-01-01", "2026-01-01Z"}) {
    EXPECT_THROW(parse("class record {}", options(date)), std::invalid_argument);
  }
  EXPECT_EQ(schema::parser::version_policy_reference_date().size(), 10u);
}

// Malformed catalogs, leaves and every branch fail even when an outside compatibility override exists.
TEST(schema_policy, rejects_invalid_release_contracts) {
  for (const auto policy : {"keep_last { 0 };", "keep_last { 03 };", "keep_last { 4294967296 };",
                            "max_age { 0 years };", "max_age { 1 hours };", "max_age { 1 };",
                            "released_since { \"2025-02-29\" };", "expires_on { 2026-10-06 };",
                            "compatibility { 7 };", "compatibility { 11 };", "any {};", "all {};",
                            "unknown { 1 };", "keep_last { 1 }; keep_last { 2 };"}) {
    SCOPED_TRACE(policy);
    EXPECT_THROW(parse(model(policy, "compatibility { 8 }")), std::exception);
  }
  for (const auto catalog :
       {"releases {};", "releases { 9 { \"2025-01-01\" }; 8 { \"2026-01-01\" }; }",
        "releases { 10 { \"2026-01-01\" }; 10 { \"2026-02-01\" }; }",
        "releases { 8 { \"2026-01-01\" }; 10 { \"2025-01-01\" }; }",
        "releases { 9 { \"2025-01-01\" }; }", "releases { 10 { \"2026-02-30\" }; }"}) {
    EXPECT_THROW(parse("class record { public version { 10 } " + std::string{catalog} + "; }"),
                 std::exception);
  }
  EXPECT_THROW(parse("class record { public version { 10 } policy { keep_last { 3 }; }; }"),
               std::exception);
  EXPECT_THROW(parse("class record { public version { 10 } policy { max_age { 2 years }; }; }"),
               std::exception);
  EXPECT_THROW(parse(model("compatibility { 8 };") +
                     " class other { public uint8 value policy { compatibility { 1 }; }; }"),
               std::exception);
}

// Parser limits prevent recursive or oversized trees, and incomplete prefixes cannot read past EOF.
TEST(schema_policy, bounds_and_truncated_metadata) {
  std::string nested = "compatibility { 8 };";
  for (int index = 0; index < 32; ++index) {
    nested = "any { " + nested + " };";
  }
  EXPECT_THROW(parse(model(nested)), std::exception);
  std::string wide = "any { ";
  for (int index = 0; index < 4096; ++index) {
    wide += "compatibility { 8 }; ";
  }
  wide += "};";
  EXPECT_THROW(parse(model(wide)), std::exception);
  const auto source =
      model("all { max_age { 2 years }; any { keep_last { 3 }; compatibility { 8 }; }; };");
  for (std::size_t length = 1; length < source.size(); ++length) {
    SCOPED_TRACE(length);
    EXPECT_THROW(parse(std::string_view{source}.substr(0, length)), std::exception);
  }
}

// Original generic definitions are evaluated once and concrete aliases keep the resolved floor.
TEST(schema_policy, preserves_generic_policy_metadata) {
  const auto source =
      "class record<T> { public version { 10 } " + std::string{release_catalog} +
      "policy { keep_last { 3 }; }; public T value (2); } instantiate count = record<uint32>;";
  const auto nodes = parse(source);
  bool found{};
  for (const auto& node : nodes) {
    if (node->type == schema::object_type::class_type) {
      const auto* version = static_cast<const schema::class_node&>(*node).version_member();
      if (version) {
        EXPECT_EQ(schema::schema_version::minimum(*version), "8");
        found = true;
      }
    }
  }
  EXPECT_TRUE(found);
}
} // namespace
