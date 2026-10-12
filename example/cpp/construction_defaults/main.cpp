// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <model.hpp>
#include <rohit/serializer.hpp>

#include <iostream>

// Verify neutral construction and customized coordinates through the generated keyed codec.
int main() {
  using vector = construction_example::vector3<double>;
  using frame = construction_example::frame<double>;
  constexpr frame neutral{};
  static_assert(neutral.normal.z == 1 && neutral.axes[0].x == 1 && neutral.axes[1].y == 1);
  const frame custom{vector{2.0, 3.0, 4.0}};
  rohit::full_stream_auto_alloc output;
  custom.serialize_out<rohit::serializer::binary_integer>(output);
  const auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
  const auto decoded = rohit::serializer::deserialize_exact<frame, rohit::serializer::binary_integer>(input);
  if (decoded.normal.x != 2 || decoded.normal.y != 3 || decoded.normal.z != 4) {
    return 1;
  }
  std::cout << "Default normal: (0, 0, 1); decoded custom normal: ("
            << decoded.normal.x << ", " << decoded.normal.y << ", " << decoded.normal.z << ")\n";
  return 0;
}
