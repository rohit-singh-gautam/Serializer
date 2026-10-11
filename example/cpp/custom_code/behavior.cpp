// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.hpp"

#include <string>

// Format a display label outside the generated header so regeneration preserves this implementation.
std::string custom_code::inventory_item::summary() const {
  return description + " x" + std::to_string(quantity);
}
