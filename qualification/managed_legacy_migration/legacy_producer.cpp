// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "fixture.hpp"

#include <iostream>

namespace {
using namespace legacy_fixture;

// Produce complete old native containers without invoking any current migration implementation.
template <managed::history_mode Mode, managed::history_labels Labels>
storage save_history(const std::filesystem::path& directory, std::string_view name) {
  store<Mode, Labels> value{initial(), document_id};
  populate<Mode, Labels>(value);
  check_store<Mode, Labels>(value);
  write(directory / (std::string{name} + ".fixed"), value.save());
  return *value.read();
}

// Capture all ordinary binary codecs from the independently generated fixed-ID header.
void save_raw(const std::filesystem::path& directory, storage value, std::string_view name) {
  check_raw(value, name == "boundary");
  const auto prefix = std::string{"raw-"} + std::string{name} + '-';
  write(directory / (prefix + "binary_none.fixed"), encode<rohit::serializer::binary_none>(value));
  write(directory / (prefix + "binary_integer.fixed"), encode<rohit::serializer::binary_integer>(value));
  write(directory / (prefix + "binary_string.fixed"), encode<rohit::serializer::binary_string>(value));
}
} // namespace

// Produce old compiler/old runtime artifacts; qualification orchestration owns the output directory.
int main(int argc, char** argv) {
  try {
    if (argc != 2) { throw std::invalid_argument{"Expected an output directory"}; }
    const std::filesystem::path directory{argv[1]};
    save_history<managed::history_mode::disabled, managed::history_labels::disabled>(directory, "managed-disabled");
    save_history<managed::history_mode::linear, managed::history_labels::disabled>(directory, "managed-linear");
    auto value = save_history<managed::history_mode::linear, managed::history_labels::enabled>(directory, "managed-linear-labels");
    save_history<managed::history_mode::tree, managed::history_labels::disabled>(directory, "managed-tree");
    save_history<managed::history_mode::tree, managed::history_labels::enabled>(directory, "managed-tree-labels");
    save_raw(directory, value, "normal");
    value.persistent_id = 127U;
    traits::payload(value).entries[0].persistent_id = 128U;
    traits::payload(value).entries[1].persistent_id = std::numeric_limits<id_type>::max();
    save_raw(directory, value, "boundary");
    std::cout << "Old producer passed: " << sizeof(id_type) * 8 << " ID bits, five native histories, six raw artifacts\n";
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
