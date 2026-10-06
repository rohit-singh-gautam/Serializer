#include "schema_policy.hpp"
#include "schema_version.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer {
namespace {
using calendar_date = std::chrono::year_month_day;
constexpr int first_calendar_year = 1;
constexpr int last_calendar_year = 9999;
constexpr std::size_t date_text_size = 10;
constexpr std::uint64_t months_per_year = 12;
constexpr std::uint64_t days_per_week = 7;

// Parse canonical ISO calendar dates without a timezone, locale, or permissive normalization.
calendar_date parse_date(std::string_view text) {
  if (text.size() != date_text_size || text[4] != '-' || text[7] != '-') {
    throw std::invalid_argument{"Release policy date must use YYYY-MM-DD"};
  }
  const auto number = [](std::string_view part) {
    unsigned value{};
    if (!std::all_of(part.begin(), part.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
      throw std::invalid_argument{"Release policy date requires decimal digits"};
    }
    const auto converted = std::from_chars(part.data(), part.data() + part.size(), value);
    if (converted.ec != std::errc{} || converted.ptr != part.data() + part.size()) {
      throw std::invalid_argument{"Invalid release policy date"};
    }
    return value;
  };
  const auto year = number(text.substr(0, 4));
  const auto result = calendar_date{std::chrono::year{static_cast<int>(year)},
                                    std::chrono::month{number(text.substr(5, 2))},
                                    std::chrono::day{number(text.substr(8, 2))}};
  if (year < first_calendar_year || year > last_calendar_year || !result.ok()) {
    throw std::invalid_argument{"Invalid release policy calendar date"};
  }
  return result;
}

// Require a quoted schema date; command-line dates are checked directly by parse_date.
calendar_date schema_date(std::string_view text) {
  if (text.size() != date_text_size + 2 || text.front() != '"' || text.back() != '"') {
    throw std::invalid_argument{"Schema release dates require a quoted YYYY-MM-DD literal"};
  }
  return parse_date(text.substr(1, date_text_size));
}

// Render UTC dates with fixed width so diagnostics can be reused as deterministic CLI input.
std::string date_text(calendar_date date) {
  if (!date.ok() || static_cast<int>(date.year()) < first_calendar_year ||
      static_cast<int>(date.year()) > last_calendar_year) {
    throw std::invalid_argument{"UTC reference date is outside years 0001 through 9999"};
  }
  const auto padded = [](unsigned value, std::size_t width) {
    auto text = std::to_string(value);
    return std::string(width - text.size(), '0') + text;
  };
  return padded(static_cast<unsigned>(static_cast<int>(date.year())), 4) + "-" +
         padded(static_cast<unsigned>(date.month()), 2) + "-" +
         padded(static_cast<unsigned>(date.day()), 2);
}

// Accept positive bounded decimal counts without overflow or leading-zero ambiguity.
std::uint32_t positive_count(std::string_view text) {
  std::uint32_t value{};
  const auto converted = std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || text.front() == '0' || converted.ec != std::errc{} ||
      converted.ptr != text.data() + text.size() || value == 0) {
    throw std::invalid_argument{"Release policy count must be a positive uint32 decimal integer"};
  }
  return value;
}

// Subtract calendar months, clamping month-end days and dates before the supported calendar.
std::chrono::sys_days subtract_months(calendar_date date, std::uint64_t count) {
  const auto available =
      static_cast<std::uint64_t>(static_cast<int>(date.year()) - first_calendar_year) *
          months_per_year +
      static_cast<unsigned>(date.month()) - 1;
  if (count > available) {
    return std::chrono::sys_days{std::chrono::year{first_calendar_year} / 1 / 1};
  }
  const auto remaining = available - count;
  const auto year =
      std::chrono::year{first_calendar_year + static_cast<int>(remaining / months_per_year)};
  const auto month = std::chrono::month{static_cast<unsigned>(remaining % months_per_year + 1)};
  const auto candidate = calendar_date{year, month, date.day()};
  return candidate.ok() ? std::chrono::sys_days{candidate}
                        : std::chrono::sys_days{year / month / std::chrono::last};
}

// Resolve an inclusive age cutoff once; day/week durations and calendar month/year ages differ.
std::chrono::sys_days age_cutoff(calendar_date date, std::string_view argument) {
  const auto separator = argument.find(' ');
  if (separator == std::string_view::npos) {
    throw std::invalid_argument{"max_age requires a positive count and a duration unit"};
  }
  const auto count = positive_count(argument.substr(0, separator));
  const auto unit = argument.substr(separator + 1);
  if (unit == "year" || unit == "years") {
    return subtract_months(date, static_cast<std::uint64_t>(count) * months_per_year);
  }
  if (unit == "month" || unit == "months") {
    return subtract_months(date, count);
  }
  const bool weeks = unit == "week" || unit == "weeks";
  if (!weeks && unit != "day" && unit != "days") {
    throw std::invalid_argument{"max_age units must be days, weeks, months, or years"};
  }
  const auto days = static_cast<std::uint64_t>(count) * (weeks ? days_per_week : 1);
  const auto earliest = std::chrono::sys_days{std::chrono::year{first_calendar_year} / 1 / 1};
  const auto today = std::chrono::sys_days{date};
  return days > static_cast<std::uint64_t>((today - earliest).count())
             ? earliest
             : today - std::chrono::days{static_cast<std::chrono::days::rep>(days)};
}

// Resolve one class against its retained release catalog; metadata never reaches a payload codec.
class policy_evaluator {
  const member& version_;
  const std::string& type_;
  calendar_date as_of_;
  std::vector<std::chrono::sys_days> dates_{};
  bool expired_current_{};

  // Find the earliest eligible catalog entry; keep the current version when no historical entry passes.
  std::string released_since(std::chrono::sys_days cutoff) {
    if (dates_.empty()) {
      throw std::invalid_argument{"Date policies require a releases catalog"};
    }
    if (dates_.back() < cutoff) {
      expired_current_ = true;
    }
    const auto found = std::lower_bound(dates_.begin(), dates_.end(), cutoff);
    return found == dates_.end()
               ? version_.default_value
               : version_.releases[static_cast<std::size_t>(found - dates_.begin())].version;
  }

  // Validate an explicit floor against known retained history and the current discriminator.
  std::string checked_compatibility(std::string_view text) const {
    const auto value = schema_version::parse(type_, text);
    if ((!version_.releases.empty() &&
         value < schema_version::parse(type_, version_.releases.front().version)) ||
        schema_version::parse(type_, version_.default_value) < value) {
      throw std::invalid_argument{"Policy compatibility is outside the retained release range"};
    }
    return schema_version::trim(text);
  }

public:
  // Validate strictly increasing release identities, nondecreasing dates, and a dated current version.
  policy_evaluator(const member& version, calendar_date as_of)
      : version_{version}, type_{version.type_name_list.front().name}, as_of_{as_of} {
    if (version_.releases.empty()) {
      return;
    }
    auto previous = schema_version::parse(type_, version_.releases.front().version);
    for (std::size_t index = 0; index < version_.releases.size(); ++index) {
      const auto& release = version_.releases[index];
      const auto value = schema_version::parse(type_, release.version);
      const auto date = std::chrono::sys_days{schema_date(release.date)};
      if (index != 0 && !(previous < value)) {
        throw std::invalid_argument{"Release versions must be unique and strictly increasing"};
      }
      if (!dates_.empty() && date < dates_.back()) {
        throw std::invalid_argument{"Release dates must follow version order"};
      }
      dates_.push_back(date);
      previous = value;
    }
    if (!(previous == schema_version::parse(type_, version_.default_value))) {
      throw std::invalid_argument{"The final dated release must equal the current version"};
    }
  }

  // Reduce nested acceptance expressions to minimum-version constants using numeric version order.
  std::string evaluate(const version_policy& policy) {
    if (policy.kind == version_policy_kind::any || policy.kind == version_policy_kind::all) {
      if (policy.children.empty()) {
        throw std::invalid_argument{"Policy groups must not be empty"};
      }
      auto result = evaluate(policy.children.front());
      for (std::size_t index = 1; index < policy.children.size(); ++index) {
        auto child = evaluate(policy.children[index]);
        const bool less =
            schema_version::parse(type_, child) < schema_version::parse(type_, result);
        if ((policy.kind == version_policy_kind::any && less) ||
            (policy.kind == version_policy_kind::all && !less)) {
          result = std::move(child);
        }
      }
      return result;
    }
    switch (policy.kind) {
    case version_policy_kind::max_age:
      return released_since(age_cutoff(as_of_, policy.argument));
    case version_policy_kind::released_since:
      return released_since(std::chrono::sys_days{schema_date(policy.argument)});
    case version_policy_kind::expires_on:
      if (dates_.empty()) {
        throw std::invalid_argument{"expires_on requires a releases catalog"};
      }
      if (std::chrono::sys_days{as_of_} >= std::chrono::sys_days{schema_date(policy.argument)}) {
        expired_current_ = true;
        return version_.default_value;
      }
      return version_.releases.front().version;
    case version_policy_kind::keep_last: {
      if (dates_.empty()) {
        throw std::invalid_argument{"keep_last requires a releases catalog"};
      }
      const auto count = positive_count(policy.argument);
      const auto index = count >= version_.releases.size() ? 0 : version_.releases.size() - count;
      return version_.releases[index].version;
    }
    case version_policy_kind::compatibility:
      return checked_compatibility(policy.argument);
    default:
      throw std::invalid_argument{"Unknown release policy"};
    }
  }

  // Apply the outside-tree override after validating every policy branch.
  std::string minimum() {
    auto result = version_.policy ? evaluate(*version_.policy) : version_.default_value;
    if (!version_.compatibility_version.empty()) {
      result = checked_compatibility(version_.compatibility_version);
    }
    return result;
  }

  // Report time conditions exceeded by the current release even when an acceptance override exists.
  bool expired_current() const {
    return expired_current_;
  }
};
} // namespace

namespace parser {
// Capture the process-independent UTC day without consulting any generated reader's clock.
std::string version_policy_reference_date() {
  return date_text(
      calendar_date{std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())});
}
} // namespace parser

namespace schema_policy {
// Validate an explicit date or pin the default once for the complete parser invocation.
parser::parse_options capture_options(const parser::parse_options& options) {
  auto result = options;
  if (result.version_policy_as_of.empty()) {
    result.version_policy_as_of = parser::version_policy_reference_date();
  }
  static_cast<void>(parse_date(result.version_policy_as_of));
  return result;
}

// Evaluate original declarations before generic lowering, so cloned contracts share the same floor.
void resolve(std::vector<std::unique_ptr<syntax_node>>& statements,
             const parser::parse_options& options) {
  const auto date = parse_date(options.version_policy_as_of);
  const auto visit = [&](const auto& self, auto& nodes) -> void {
    for (auto& node : nodes) {
      if (node->type == object_type::namespace_type) {
        self(self, static_cast<namespace_node&>(*node).statements);
      } else if (node->type == object_type::class_type) {
        auto& object = static_cast<class_node&>(*node);
        for (auto& field : object.member_list) {
          if (!field.version || (field.releases.empty() && !field.policy)) {
            continue;
          }
          try {
            policy_evaluator evaluator{field, date};
            field.resolved_compatibility_version = evaluator.minimum();
            if (options.information) {
              options.information(object.get_full_name() + ": version policy as of " +
                                  options.version_policy_as_of + ", minimum " +
                                  field.resolved_compatibility_version);
            }
            if (evaluator.expired_current()) {
              const auto message = object.get_full_name() + ": current version " +
                                   field.default_value + " exceeds a release time policy as of " +
                                   options.version_policy_as_of +
                                   "; the current version remains readable";
              if (options.version_policy_warnings_as_errors) {
                throw std::invalid_argument{"Release policy warning treated as error: " + message};
              }
              if (options.warning) {
                options.warning(message);
              }
            }
          } catch (const std::invalid_argument& error) {
            throw std::invalid_argument{
                (node->source_path.empty() ? object.get_full_name() : node->source_path) + ": " +
                error.what()};
          }
        }
      }
    }
  };
  visit(visit, statements);
}
} // namespace schema_policy
} // namespace rohit::serializer
