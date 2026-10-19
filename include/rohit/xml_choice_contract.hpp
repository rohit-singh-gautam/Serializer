// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>

namespace rohit::serializer::xml {
// Each branch retains its own occurrence range inside one repetition of a choice.
struct choice_branch_contract {
  name qualified_name{};
  bool wildcard{};
  std::string_view namespace_rule{};
  std::string_view target_namespace{};
  std::size_t minimum{1U};
  std::optional<std::size_t> maximum{1U};
};

// Validate ordered runs without constructing a second collection or a per-node dispatch table.
// Branches borrow generated static metadata; a validator belongs to one synchronous operation.
class repeated_choice_validator {
  std::span<const choice_branch_contract> branches_;
  std::size_t minimum_{};
  std::optional<std::size_t> maximum_{};
  operation_context* context_{};
  std::size_t least_groups_{}, most_groups_{}, run_length_{}, run_branch_{};
  bool empty_branch_{};

  // Add bounded group counts; their maxima are bounded by the number of admitted child nodes.
  static void add(std::size_t& count, std::size_t amount) {
    if (amount > std::numeric_limits<std::size_t>::max() - count) {
      throw std::length_error{"XML choice occurrence count overflows"};
    }
    count += amount;
  }

  // Partition a maximal same-branch run into feasible per-repetition occurrence intervals.
  void flush() {
    if (run_length_ == 0U) { return; }
    const auto& branch = branches_[run_branch_];
    if (branch.maximum == 0U) { throw std::invalid_argument{"XML choice branch forbids content"}; }
    const auto least = branch.maximum ?
        run_length_ / *branch.maximum + static_cast<std::size_t>(run_length_ % *branch.maximum != 0U) : 1U;
    const auto most = run_length_ / std::max(std::size_t{1U}, branch.minimum);
    if (least > most) { throw std::invalid_argument{"XML choice branch occurrence interval failed"}; }
    add(least_groups_, least);
    add(most_groups_, most);
    run_length_ = 0U;
  }

public:
  // Retain exact group and branch bounds, including choices whose branches can match empty content.
  repeated_choice_validator(std::span<const choice_branch_contract> branches,
      std::size_t minimum, std::optional<std::size_t> maximum,
      operation_context* context = nullptr)
      : branches_{branches}, minimum_{minimum}, maximum_{maximum}, context_{context} {
    if (branches.empty() || (maximum && minimum > *maximum)) {
      throw std::invalid_argument{"Invalid XML choice group contract"};
    }
    for (const auto& branch : branches) {
      if (context_) { context_->consume(operation_resource::work_units, 1U); }
      if (branch.maximum && branch.minimum > *branch.maximum) {
        throw std::invalid_argument{"Invalid XML choice branch contract"};
      }
      empty_branch_ = empty_branch_ || branch.minimum == 0U;
    }
  }

  repeated_choice_validator(const repeated_choice_validator&) = delete;
  repeated_choice_validator& operator=(const repeated_choice_validator&) = delete;

  // Add one run known to select this branch; zero-size runs do not create an artificial repetition.
  void consume(std::size_t branch, std::size_t count = 1U) {
    if (branch >= branches_.size()) { throw std::invalid_argument{"Unknown XML choice branch"}; }
    if (count == 0U) { return; }
    if (context_) { context_->consume(operation_resource::work_units, count); }
    if (run_length_ != 0U && run_branch_ != branch) { flush(); }
    run_branch_ = branch;
    add(run_length_, count);
  }

  // Dispatch one existing node by expanded identity; unknown children cannot satisfy the group.
  void consume(const element& child) {
    for (std::size_t index{}; index < branches_.size(); ++index) {
      const auto& branch = branches_[index];
      if (context_) {
        context_->consume(operation_resource::work_units, 1U);
        context_->consume(operation_resource::work_units, branch.namespace_rule.size());
        context_->consume(operation_resource::work_units, child.qualified_name.namespace_uri.size());
      }
      if ((!branch.wildcard && child.qualified_name == branch.qualified_name) ||
          (branch.wildcard && matches_namespace_rule(child.qualified_name.namespace_uri,
              branch.namespace_rule, branch.target_namespace))) {
        consume(index);
        return;
      }
    }
    throw std::invalid_argument{"XML child matches no repeated choice branch"};
  }

  // Accept exactly when an allowed repetition count can represent all ordered child runs.
  void finish() {
    flush();
    const auto selected = std::max(minimum_, least_groups_);
    if ((maximum_ && selected > *maximum_) || (!empty_branch_ && selected > most_groups_)) {
      throw std::invalid_argument{"XML repeated choice occurrence failed"};
    }
  }
};

// Validate the exact input interval consumed by a generated repeated field and its wildcard slots.
inline void validate_repeated_choice(std::span<const element> children,
    std::span<const choice_branch_contract> branches, std::size_t minimum,
    std::optional<std::size_t> maximum, operation_context* context = nullptr) {
  repeated_choice_validator validator{branches, minimum, maximum, context};
  for (const auto& child : children) { validator.consume(child); }
  validator.finish();
}

// Validate borrowed known values and positioned opaque slots before output copies any extension tree.
inline void validate_repeated_choice_output(std::size_t begin, std::size_t end,
    std::size_t known_branch, std::size_t field, const extension_store* extensions,
    std::span<const choice_branch_contract> branches, std::size_t minimum,
    std::optional<std::size_t> maximum, operation_context* context = nullptr) {
  if (end < begin) { throw std::invalid_argument{"Invalid XML choice output interval"}; }
  repeated_choice_validator validator{branches, minimum, maximum, context};
  auto position = begin;
  if (extensions) {
    for (const auto& slot : extensions->children) {
      if (context) { context->consume(operation_resource::work_units, 1U); }
      if (slot.before_field != field) { continue; }
      if (slot.before_child < position || slot.before_child > end) {
        throw std::invalid_argument{"XML choice extension position is outside its group"};
      }
      validator.consume(known_branch, slot.before_child - position);
      validator.consume(slot.value);
      position = slot.before_child;
    }
  }
  validator.consume(known_branch, end - position);
  validator.finish();
}
} // namespace rohit::serializer::xml
