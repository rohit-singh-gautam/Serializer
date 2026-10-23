// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/cache.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

// Crash without destructors at an optional cache record boundary; never writes authoritative data.
int main(int argc, char** argv) {
  if (argc != 4) {
    return 2;
  }
  try {
    const std::string_view action{argv[2]};
    const std::string_view point{argv[3]};
    rohit::cache::directory_options options;
    options.reserve_free_bytes = 0;
    options.fault_injector = [&](rohit::cache::io_event actual) {
      using event = rohit::cache::io_event;
      if ((point == "before_stage" && actual == event::before_stage) ||
          (point == "after_stage" && actual == event::after_stage) ||
          (point == "before_publish" && actual == event::before_publish) ||
          (point == "after_publish" && actual == event::after_publish) ||
          (point == "before_remove" && actual == event::before_remove)) {
        std::_Exit(86);
      }
    };
    const auto key = rohit::cache::make_key(
        1, 2, 0, {"crash.cache", 200},
        rohit::make_digest<rohit::digest_algorithm::sha256>("authoritative"), "v1");
    rohit::cache::directory_provider provider{argv[1], options};
    if (action == "read") {
      const auto value = provider.lookup(key);
      if (!value || value->value.size() != 1) {
        throw std::runtime_error{"Missing crash cache entry"};
      }
      std::cout << static_cast<unsigned>(value->value.front()) << '\n';
    } else if (action == "invalidate") {
      provider.invalidate(key);
    } else {
      provider.publish(key,
                       rohit::cache::bytes{static_cast<std::uint8_t>(action == "init" ? 1 : 2)});
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
