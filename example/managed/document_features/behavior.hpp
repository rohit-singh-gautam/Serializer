// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

// Include the generated document schema before this application-owned definition.
namespace managed_document_features {
// Keep editor-template definitions visible at the application instantiation site.
template <typename Access>
void document_metrics_editor<Access>::add_pages(std::uint32_t amount)
    requires (!Access::is_runtime_access) {
  access_.guard([&] { set_page_count(get_page_count() + amount); });
}
} // namespace managed_document_features
